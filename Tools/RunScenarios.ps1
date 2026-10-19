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
.EXAMPLE
    ./Tools/RunScenarios.ps1 -Script Tools\Analysis\fog_sweep.py -ScriptArgs "--runs 1"
.EXAMPLE
    ./Tools/RunScenarios.ps1 -RealTime

#>
param(
    [string]$EngineDir = "",
    [int]$Port = 8766,
    [int]$StartupTimeoutSeconds = 180,
    # The Python script to run against the simulation, relative to the project root.
    [string]$Script = "Tools\Analysis\run_scenarios.py",
    # Extra arguments for that script, as one string, e.g. "--runs 1".
    [string]$ScriptArgs = "",
    # Pace the simulation by the wall clock, as the editor does, instead of
    # running it as fast as the machine allows.
    [switch]$RealTime
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project     = Join-Path $ProjectRoot "RoboDrive.uproject"
# Find the engine the way Unreal does, unless one was passed explicitly.
. (Join-Path $PSScriptRoot "EngineLocation.ps1")
if (-not $EngineDir) {
    $EngineDir = Find-UnrealEngine -ProjectFile $Project
}
Write-Host "Using engine at $EngineDir"
$Harness     = Join-Path $ProjectRoot $Script
$LogFile     = Join-Path $ProjectRoot "Saved\Logs\RunScenarios.log"

# Assume failure until the harness says otherwise, so every early exit below
# reports red rather than accidentally green.
$ExitCode = 1

Write-Host "Starting simulation headless on port $Port..." -ForegroundColor Cyan

# -game runs the project as a game rather than opening the editor.
# -nullrhi skips rendering entirely: no window and no GPU work.
# -RoboApiPort keeps this off the editor's port, so both can run at once.
$SimulationArgs = @(
    "`"$Project`"",
    "/Game/Maps/TestCourse",
    "-game", "-nullrhi", "-nosound", "-unattended", "-nosplash",
    "-RoboApiPort=$Port",
    "-ABSLOG=`"$LogFile`""
)

# Fixed time step: every frame advances exactly 1/120 s of simulation, and the
# engine stops waiting for the wall clock between frames. Each step does the
# same physics as in real time; only the waiting is removed. 120 matches both
# the simulation clock's step and the physics substep, so each frame is
# exactly one of each.
if (-not $RealTime) {
    $SimulationArgs += "-UseFixedTimeStep", "-FPS=120"
}

$Simulation = Start-Process -FilePath "$EngineDir\Engine\Binaries\Win64\UnrealEditor.exe" `
    -ArgumentList $SimulationArgs `
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

    Write-Host "Simulation ready. Running $Script..." -ForegroundColor Cyan

    # Run from the project root, so the harness finds Scenarios/ by default.
    Push-Location $ProjectRoot
    try {
        # Split the extra arguments into separate words, and pass them on with
        # splatting (@), which hands each array element over as its own argument.
        $ExtraArgs = $ScriptArgs.Split(" ", [System.StringSplitOptions]::RemoveEmptyEntries)
        python $Harness --port $Port @ExtraArgs
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