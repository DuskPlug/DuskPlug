#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$Exe = Join-Path $Root 'DuskPlug.exe'
$OutDir = Join-Path $PSScriptRoot 'bin'
$Msi = Join-Path $OutDir 'DuskPlug.msi'

if (-not (Test-Path $Exe)) {
    Write-Host 'DuskPlug.exe not found. Building...' -ForegroundColor Yellow
    & (Join-Path $Root 'Build-DuskPlug.cmd')
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $Exe)) {
        throw 'Could not build DuskPlug.exe'
    }
}

$dotnet = Get-Command dotnet -ErrorAction SilentlyContinue
if (-not $dotnet) {
    throw 'dotnet SDK is required to build the MSI (winget install Microsoft.DotNet.SDK.8)'
}

$versionFile = Join-Path $Root 'VERSION'
if (-not (Test-Path -LiteralPath $versionFile)) {
    throw "Missing VERSION file at $versionFile"
}
$version = (Get-Content -LiteralPath $versionFile -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION must be semver major.minor.patch, got: $version"
}

Write-Host "Building DuskPlug.msi ($version)..." -ForegroundColor Cyan
& $dotnet.Source build (Join-Path $PSScriptRoot 'DuskPlug.wixproj') -c Release "-p:ProductVersion=$version"
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if (-not (Test-Path $Msi)) {
    $found = Get-ChildItem -Recurse $PSScriptRoot -Filter DuskPlug.msi | Select-Object -First 1
    if ($found) {
        $Msi = $found.FullName
    }
}

if (-not (Test-Path $Msi)) {
    throw 'MSI build finished but DuskPlug.msi was not found'
}

Copy-Item -Force $Msi (Join-Path $Root 'DuskPlug.msi')
Write-Host "Built $(Join-Path $Root 'DuskPlug.msi')" -ForegroundColor Green
