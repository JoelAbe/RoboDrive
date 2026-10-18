<#
.SYNOPSIS
    Finds the Unreal Engine installation a project uses.

.DESCRIPTION
    Dot-source this file, then call Find-UnrealEngine. It resolves the engine
    the same way Unreal does: read the version the .uproject asks for, then look
    it up in the registry where installs record themselves. Nothing here
    assumes a particular drive or folder, so it keeps working when the engine
    is reinstalled somewhere else and works on machines other than this one.
#>

function Find-UnrealEngine {
    param(
        [Parameter(Mandatory)]
        [string]$ProjectFile
    )

    # An explicit override wins. Useful when a runner runs as a different
    # Windows user, whose registry would not list this user's engines.
    if ($env:UE_ENGINE_DIR) {
        return $env:UE_ENGINE_DIR
    }

    $Association = (Get-Content -Raw $ProjectFile | ConvertFrom-Json).EngineAssociation

    # Per-user registrations: source builds, and launcher installs placed in a
    # user folder, which is where this machine's 5.8 lives.
    $UserBuilds = Get-ItemProperty -Path "HKCU:\Software\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue
    if ($UserBuilds -and $UserBuilds.$Association) {
        return $UserBuilds.$Association
    }

    # Machine-wide launcher installs.
    $LauncherInstall = Get-ItemProperty -Path "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association" -ErrorAction SilentlyContinue
    if ($LauncherInstall -and $LauncherInstall.InstalledDirectory) {
        return $LauncherInstall.InstalledDirectory
    }

    throw "Could not find Unreal Engine '$Association'. Install it, or set UE_ENGINE_DIR to its folder."
}