#Requires -Version 5.1
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$NotesUrl,
    [Parameter(Mandatory = $true)][string]$OutFile,
    [string]$WindowsMsi,
    [string]$WindowsZip,
    [string]$LinuxTarball,
    [string]$MacosZip
)

$ErrorActionPreference = 'Stop'

function Get-Sha256Hex([string]$Path) {
    if (-not (Test-Path $Path)) {
        throw "Missing file for latest.json: $Path"
    }
    return (Get-FileHash -Path $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Asset-Entry([string]$Path, [string]$Url) {
    return @{
        url    = $Url
        sha256 = Get-Sha256Hex $Path
    }
}

$base = "https://github.com/DuskPlug/DuskPlug/releases/download/v$Version"
$assets = @{}

if ($WindowsMsi) {
    $assets.windows_msi = Asset-Entry $WindowsMsi "$base/DuskPlug.msi"
}
if ($WindowsZip) {
    $assets.windows_zip = Asset-Entry $WindowsZip "$base/DuskPlug-Windows.zip"
}
if ($LinuxTarball) {
    $assets.linux_tarball = Asset-Entry $LinuxTarball "$base/DuskPlug-Linux-x64.tar.gz"
}
if ($MacosZip) {
    $assets.macos_zip = Asset-Entry $MacosZip "$base/DuskPlug-macOS.zip"
}

$manifest = [ordered]@{
    version      = $Version
    published_at = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    notes_url    = $NotesUrl
    assets       = $assets
}

$json = $manifest | ConvertTo-Json -Depth 5
Set-Content -Path $OutFile -Value $json -Encoding utf8
Write-Host "Wrote $OutFile"
