<#
.SYNOPSIS
    Builds RoboDrive and runs its automation tests without opening the editor.

.EXAMPLE
    ./Tools/RunTests.ps1
    ./Tools/RunTests.ps1 -Filter RoboDrive.RoboSim.Frames -SkipBuild
#>
param(
    [string]$Filter = "RoboDrive",
    [string]$EngineDir = "C:\Program Files\Epic Games\UE_5.8",
    [switch]$SkipBuild
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project     = Join-Path $ProjectRoot "RoboDrive.uproject"
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
Write-Host "Running tests matching '$Filter'..." -ForegroundColor Cyan
& "$EngineDir\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project" `
    -ExecCmds="Automation RunTests $Filter; Quit" `
    -unattended -nopause -nosplash -NullRHI -NoSound `
    -ABSLOG="$LogFile" | Out-Null

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