<#
.SYNOPSIS
    Builds RoboDrive and runs its automation tests without opening the editor.

.EXAMPLE
    ./Tools/RunTests.ps1
    ./Tools/RunTests.ps1 -Filter RoboDrive.RoboSim.Frames -SkipBuild
#>
param(
    [string]$Filter = "RoboDrive",
    [string]$EngineDir = "",
    [switch]$SkipBuild
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project     = Join-Path $ProjectRoot "RoboDrive.uproject"
# Find the engine the way Unreal does, unless one was passed explicitly.
. (Join-Path $PSScriptRoot "EngineLocation.ps1")
if (-not $EngineDir) {
    $EngineDir = Find-UnrealEngine -ProjectFile $Project
}
Write-Host "Using engine at $EngineDir"
$LogFile     = Join-Path $ProjectRoot "Saved\Logs\RunTests.log"

# --- 1. Build ------------------------------------------------------------------
if (-not $SkipBuild) {
    Write-Host "Building RoboDriveEditor..." -ForegroundColor Cyan
    & "$EngineDir\Engine\Build\BatchFiles\Build.bat" RoboDriveEditor Win64 Development -Project="$Project" -WaitMutex
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed." -ForegroundColor Red
        exit 1
    }
}

# --- 2. Run the tests headless ---------------------------------------------------
# Remove the previous run's log first. Otherwise an editor that fails to start
# leaves the old log in place, and its results would be read as this run's.
if (Test-Path $LogFile) {
    Remove-Item $LogFile -Force
}

Write-Host "Running tests matching '$Filter'..." -ForegroundColor Cyan
& "$EngineDir\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project" `
    -ExecCmds="Automation RunTests $Filter; Quit" `
    -unattended -nopause -nosplash -NullRHI -NoSound `
    -ABSLOG="$LogFile" | Out-Null
$EditorExitCode = $LASTEXITCODE

if (-not (Test-Path $LogFile)) {
    Write-Host "The editor wrote no log (exit code $EditorExitCode). It probably failed to start." -ForegroundColor Red
    exit 1
}

# The automation system logs this line only once every queued test has run.
# Without it, the editor stopped partway, for example by crashing, and the
# passes logged so far must not be mistaken for a complete run.
if (-not (Select-String -Path $LogFile -Pattern "TEST COMPLETE\. EXIT CODE:" -Quiet)) {
    Write-Host "The test run did not finish (editor exit code $EditorExitCode). See $LogFile" -ForegroundColor Red
    exit 1
}

# --- 3. Read the results from the log ----------------------------------------------
# Same "Test Completed. Result={...}" lines you saw in the Output Log.
$Passed = @(Select-String -Path $LogFile -Pattern "Test Completed\. Result=\{Success\}").Count
$Failed = @(Select-String -Path $LogFile -Pattern "Test Completed\. Result=\{Fail\}").Count

Write-Host ""
Write-Host "Passed: $Passed   Failed: $Failed"

# Zero tests is a failure, not a pass: it almost always means a typo in the filter.
if ($Passed + $Failed -eq 0) {
    Write-Host "No tests ran. Check the filter '$Filter'." -ForegroundColor Red
    exit 1
}

if ($Failed -gt 0) {
    Select-String -Path $LogFile -Pattern "LogAutomationController: Error" |
        ForEach-Object { Write-Host $_.Line -ForegroundColor Red }
    exit 1
}

Write-Host "All tests passed." -ForegroundColor Green
exit 0