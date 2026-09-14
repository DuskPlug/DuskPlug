#Requires -Version 5.1
# Local/dev reinstall: one UAC prompt. Finds every leftover DuskPlug MSI,
# uninstalls them, installs the newly built package, and copies the current
# build into Program Files if Windows Installer left a stale binary.
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$Msi = Join-Path $Root 'DuskPlug.msi'
$SourceExe = Join-Path $Root 'DuskPlug.exe'
$SourceLoader = Join-Path $Root 'WebView2Loader.dll'
$SourcePthread = Join-Path $Root 'libwinpthread-1.dll'
$SourceAssets = Join-Path $Root 'assets'
$InstallDir = 'C:\Program Files\DuskPlug'
$InstallExe = Join-Path $InstallDir 'DuskPlug.exe'

if (-not (Test-Path -LiteralPath $Msi)) {
    throw "MSI not found: $Msi. Run Build-Msi.cmd first."
}
if (-not (Test-Path -LiteralPath $SourceExe)) {
    throw "DuskPlug.exe not found: $SourceExe. Run Build-DuskPlug.cmd -Release first."
}

Get-Process DuskPlug -ErrorAction SilentlyContinue | Stop-Process -Force

$work = Join-Path $env:TEMP 'duskplug-install-work.ps1'
$log = Join-Path $env:TEMP 'duskplug-install.log'

@'
param(
    [Parameter(Mandatory = $true)][string]$Msi,
    [Parameter(Mandatory = $true)][string]$SourceExe,
    [Parameter(Mandatory = $true)][string]$SourceLoader,
    [Parameter(Mandatory = $true)][string]$SourcePthread,
    [Parameter(Mandatory = $true)][string]$SourceAssets,
    [Parameter(Mandatory = $true)][string]$InstallDir,
    [Parameter(Mandatory = $true)][string]$InstallExe,
    [Parameter(Mandatory = $true)][string]$Log
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
function Write-Log([string]$Message) {
    Add-Content -LiteralPath $Log -Value $Message
}
try {
    Set-Content -LiteralPath $Log -Value 'Starting DuskPlug local install'
    Get-Process DuskPlug -ErrorAction SilentlyContinue | Stop-Process -Force

    $uninstallKeys = @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*'
    )
    $codes = @(Get-ItemProperty $uninstallKeys -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -eq 'DuskPlug' } |
        ForEach-Object {
            if ($_.PSChildName -match '^(\{[0-9A-Fa-f-]{36}\})$') { $Matches[1] }
            elseif ($_.UninstallString -match '(\{[0-9A-Fa-f-]{36}\})') { $Matches[1] }
        } | Select-Object -Unique)

    foreach ($code in $codes) {
        Write-Log "Uninstalling $code"
        $u = Start-Process -FilePath 'msiexec.exe' -ArgumentList @('/x', $code, '/qn', '/norestart') -Wait -PassThru
        if ($u.ExitCode -notin 0, 3010, 1605) {
            throw "Uninstall $code exited with code $($u.ExitCode)"
        }
    }

    Write-Log "Installing $Msi"
    $p = Start-Process -FilePath 'msiexec.exe' -ArgumentList @('/i', $Msi, '/qn', '/norestart') -Wait -PassThru
    if ($p.ExitCode -notin 0, 3010) {
        throw "msiexec install exited with code $($p.ExitCode)"
    }

    if (-not (Test-Path -LiteralPath $InstallExe)) {
        throw "Install finished but $InstallExe was not found."
    }

    $installed = Get-Item -LiteralPath $InstallExe
    $source = Get-Item -LiteralPath $SourceExe
    if ($installed.Length -ne $source.Length -or $source.LastWriteTime -ne $installed.LastWriteTime) {
        Write-Log 'Copying the local build into Program Files'
        Get-Process DuskPlug -ErrorAction SilentlyContinue | Stop-Process -Force
        Copy-Item -Force $SourceExe $InstallExe
        if (Test-Path -LiteralPath $SourceLoader) {
            Copy-Item -Force $SourceLoader (Join-Path $InstallDir 'WebView2Loader.dll')
        }
        if (Test-Path -LiteralPath $SourcePthread) {
            Copy-Item -Force $SourcePthread (Join-Path $InstallDir 'libwinpthread-1.dll')
        }
        New-Item -ItemType Directory -Force -Path (Join-Path $InstallDir 'assets') | Out-Null
        Copy-Item -Force (Join-Path $SourceAssets '*') (Join-Path $InstallDir 'assets\')
    }

    Write-Log 'OK'
    exit 0
} catch {
    Write-Log $_.Exception.Message
    exit 1
}
'@ | Set-Content -LiteralPath $work -Encoding UTF8

try {
    Write-Host 'Installing DuskPlug (one elevation prompt)...' -ForegroundColor Cyan
    $elevated = Start-Process -FilePath 'powershell.exe' -ArgumentList @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-WindowStyle', 'Hidden',
        '-File', $work,
        '-Msi', $Msi,
        '-SourceExe', $SourceExe,
        '-SourceLoader', $SourceLoader,
        '-SourcePthread', $SourcePthread,
        '-SourceAssets', $SourceAssets,
        '-InstallDir', $InstallDir,
        '-InstallExe', $InstallExe,
        '-Log', $log
    ) -Verb RunAs -PassThru -Wait
    if (-not $elevated) {
        throw 'Elevation was cancelled.'
    }
    if ($elevated.ExitCode -ne 0) {
        $detail = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
        throw "Elevated install failed (exit $($elevated.ExitCode)). $detail"
    }
} finally {
    Remove-Item -LiteralPath $work -Force -ErrorAction SilentlyContinue
}

if (-not (Test-Path -LiteralPath $InstallExe)) {
    throw "Install finished but $InstallExe was not found."
}

$installedInfo = Get-Item -LiteralPath $InstallExe
Write-Host "Installed: $InstallExe ($($installedInfo.LastWriteTime), $($installedInfo.Length) bytes)" -ForegroundColor Green
Start-Process -FilePath $InstallExe
