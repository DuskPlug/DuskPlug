#Requires -Version 5.1
#Requires -RunAsAdministrator
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$InstallDir = 'C:\Program Files\DuskPlug'
$Log = Join-Path $Root 'install-last.log'

function Write-Log([string]$Message) {
    $line = "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $Message"
    Add-Content -LiteralPath $Log -Value $line
    Write-Host $line
}

function Start-DuskPlugForInteractiveUser([string]$ExePath) {
    $isElevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)

    if (-not $isElevated) {
        Start-Process -FilePath $ExePath
        Write-Log "Started $ExePath"
        return
    }

    # Elevated installs must not launch the tray app in the admin session.
    $taskName = 'DuskPlugPostInstallLaunch'
    try {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
        $action = New-ScheduledTaskAction -Execute $ExePath
        $trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddSeconds(1)
        $principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType Interactive
        Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null
        Start-ScheduledTask -TaskName $taskName
        Start-Sleep -Milliseconds 1500
        Write-Log "Started $ExePath for interactive user $env:USERNAME"
    } catch {
        Write-Log "Could not start via scheduled task: $($_.Exception.Message)"
        throw
    } finally {
        Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
    }
}

try {
    Set-Content -LiteralPath $Log -Value 'Starting file-based install'
    for ($attempt = 0; $attempt -lt 5; $attempt++) {
        Get-Process DuskPlug -ErrorAction SilentlyContinue | Stop-Process -Force
        Start-Sleep -Milliseconds 500
        if (-not (Get-Process DuskPlug -ErrorAction SilentlyContinue)) {
            break
        }
    }
    if (Get-Process DuskPlug -ErrorAction SilentlyContinue) {
        throw 'Could not stop DuskPlug. Exit it from the tray menu and run install again.'
    }
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $InstallDir 'assets') | Out-Null

    Copy-Item -Force (Join-Path $Root 'DuskPlug.exe') (Join-Path $InstallDir 'DuskPlug.exe')
    Copy-Item -Force (Join-Path $Root 'WebView2Loader.dll') (Join-Path $InstallDir 'WebView2Loader.dll')
    Copy-Item -Force (Join-Path $Root 'libwinpthread-1.dll') (Join-Path $InstallDir 'libwinpthread-1.dll')
    Copy-Item -Force (Join-Path $Root 'assets\*') (Join-Path $InstallDir 'assets\')
    if (Test-Path (Join-Path $Root 'LICENSE')) {
        Copy-Item -Force (Join-Path $Root 'LICENSE') (Join-Path $InstallDir 'LICENSE')
    }

    $info = Get-Item (Join-Path $InstallDir 'DuskPlug.exe')
    Write-Log "Installed $($info.FullName) ($($info.LastWriteTime), $($info.Length) bytes)"
    Start-DuskPlugForInteractiveUser (Join-Path $InstallDir 'DuskPlug.exe')
} catch {
    Write-Log "ERROR: $($_.Exception.Message)"
    throw
}
