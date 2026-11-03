<#
.SYNOPSIS
    Sets up ROS 2 in the current PowerShell session.

.DESCRIPTION
    Dot-source it, so the environment it sets stays in your session:

        . .\Tools\RosEnv.ps1

    It activates the pixi environment ROS 2 runs in, sources the ROS 2
    install, then sources this repository's ROS packages if they have been
    built (cd Ros2; colcon build).

    ROS 2 is looked for in C:\dev\lyrical, where the Windows install guide
    puts it, unless -RosDir or the ROS2_DIR environment variable says
    otherwise.
#>

param(
    [string]$RosDir = $(if ($env:ROS2_DIR) { $env:ROS2_DIR } else { "C:\dev\lyrical" })
)

if (-not (Test-Path (Join-Path $RosDir "local_setup.ps1"))) {
    Write-Host "No ROS 2 install at $RosDir. See docs/ros2.md." -ForegroundColor Red
    return
}

# Pixi adds itself to PATH for new terminals; one opened before it was
# installed will not have it yet.
$Pixi = (Get-Command pixi -ErrorAction SilentlyContinue).Source
if (-not $Pixi) {
    $Pixi = Join-Path $env:LOCALAPPDATA "pixi\bin\pixi.exe"
}

# The environment refers to the current directory (Qt finds its plugins
# relative to it), so it must be activated from the ROS 2 folder.
Push-Location $RosDir
try {
    Invoke-Expression ((& $Pixi shell-hook --shell powershell 2>$null) -join "`n")
}
finally {
    Pop-Location
}

# Tell Qt where its plugins are. The environment names the folder with
# cmd.exe's %cd%, which PowerShell leaves as literal text, so RViz fails to
# start with "Could not find the Qt platform plugin". And RViz is not in Qt's
# own folder, where Qt's qt6.conf would find them, so without this it also
# cannot draw its SVG icons.
$QtPlugins = Join-Path $RosDir ".pixi\envs\default\Library\lib\qt6\plugins"
if (Test-Path $QtPlugins) {
    $env:QT_PLUGIN_PATH = $QtPlugins
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $QtPlugins "platforms"
}

. (Join-Path $RosDir "local_setup.ps1")

$Workspace = Join-Path (Split-Path -Parent $PSScriptRoot) "Ros2\install\local_setup.ps1"
if (Test-Path $Workspace) {
    . $Workspace
    Write-Host "ROS 2 and the RoboDrive packages are ready." -ForegroundColor Green
}
else {
    Write-Host "ROS 2 is ready. Build the RoboDrive packages with: cd Ros2; colcon build" -ForegroundColor Yellow
}
