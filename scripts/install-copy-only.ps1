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

Set-Content -LiteralPath $Log -Value 'Starting file-based install'
Get-Process DuskPlug -ErrorAction SilentlyContinue | Stop-Process -Force
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
Start-Process -FilePath (Join-Path $InstallDir 'DuskPlug.exe')
