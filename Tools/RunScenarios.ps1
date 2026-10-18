<#
.SYNOPSIS
    Launches the simulation headless, runs every scenario against it, and
    shuts it down.

.DESCRIPTION
    The simulation runs as a game (no editor UI, no window) on a port separate
    from the editor's, so CI can run while the editor is open. Exits with the
    harness's exit code: 0 if every scenario behaved as expected, 1 otherwise.

.EXAMPLE
    ./Tools/RunScenarios.ps1
#>
param(
    [string]$EngineDir = "",
    [int]$Port = 8766,
    [int]$StartupTimeoutSeconds = 180
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project     = Join-Path $ProjectRoot "RoboDrive.uproject"
# Find the engine the way Unreal does, unless one was passed explicitly.
. (Join-Path $PSScriptRoot "EngineLocation.ps1")
if (-not $EngineDir) {
    $EngineDir = Find-UnrealEngine -ProjectFile $Project
}
Write-Host "Using engine at $EngineDir"
$Harness     = Join-Path $ProjectRoot "Tools\Analysis\run_scenarios.py"
$LogFile     = Join-Path $ProjectRoot "Saved\Logs\RunScenarios.log"

# Assume failure until the harness says otherwise, so every early exit below
# reports red rather than accidentally green.
$ExitCode = 1

Write-Host "Starting simulation headless on port $Port..." -ForegroundColor Cyan

# -game runs the project as a game rather than opening the editor.
# -nullrhi skips rendering entirely: no window and no GPU work.
# -RoboApiPort keeps this off the editor's port, so both can run at once.
$Simulation = Start-Process -FilePath "$EngineDir\Engine\Binaries\Win64\UnrealEditor.exe" `
    -ArgumentList @(
        "`"$Project`"",
        "/Game/Maps/TestCourse",
        "-game", "-nullrhi", "-nosound", "-unattended", "-nosplash",
        "-RoboApiPort=$Port",
        "-ABSLOG=`"$LogFile`""
    ) `
    -PassThru

try {
    # Loading the engine takes a while. Wait until the API accepts a
    # connection, rather than sleeping for a guessed number of seconds.
    $Deadline = (Get-Date).AddSeconds($StartupTimeoutSeconds)
    $Ready = $false

    while ((Get-Date) -lt $Deadline -and -not $Simulation.HasExited) {
        try {
            $Probe = [System.Net.Sockets.TcpClient]::new("127.0.0.1", $Port)
            $Probe.Close()
            $Ready = $true
            break
        }
        catch {
            Start-Sleep -Seconds 2
        }
    }

    if (-not $Ready) {
        Write-Host "Simulation was not listening within $StartupTimeoutSeconds s. See $LogFile" -ForegroundColor Red
        exit 1
    }

    Write-Host "Simulation ready. Running scenarios..." -ForegroundColor Cyan

    # Run from the project root, so the harness finds Scenarios/ by default.
    Push-Location $ProjectRoot
    try {
        python $Harness --port $Port
        $ExitCode = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}
finally {
    # Always shut the simulation down, whether the scenarios passed, failed,
    # or the script itself errored. A leftover headless process would hold
    # the port and break the next run.
    if (-not $Simulation.HasExited) {
        Stop-Process -Id $Simulation.Id -Force
    }
}

exit $ExitCode