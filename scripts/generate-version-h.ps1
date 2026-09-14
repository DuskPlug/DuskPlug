#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$VersionFile = Join-Path $Root 'VERSION'
$OutFile = Join-Path $Root 'cpp\src\version.h'

if (-not (Test-Path $VersionFile)) {
    throw "Missing VERSION file at $VersionFile"
}

$version = (Get-Content $VersionFile -Raw).Trim()
if ($version -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
    throw "VERSION must be semver major.minor.patch, got: $version"
}

$major = $Matches[1]
$minor = $Matches[2]
$patch = $Matches[3]

$content = @"
#pragma once

#define DUSKPLUG_VERSION "$version"
#define DUSKPLUG_VERSION_MAJOR $major
#define DUSKPLUG_VERSION_MINOR $minor
#define DUSKPLUG_VERSION_PATCH $patch

"@

$existing = ''
if (Test-Path -LiteralPath $OutFile) {
    $existing = Get-Content -LiteralPath $OutFile -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
}
if ($existing -eq $content) {
    return
}

Set-Content -Path $OutFile -Value $content -NoNewline -Encoding utf8
Write-Host "Wrote $OutFile ($version)"
