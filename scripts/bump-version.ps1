#Requires -Version 5.1
<#
.SYNOPSIS
    Bump the repo version (semver) and sync generated headers.

.PARAMETER Part
    Which semver segment to increment: patch (default), minor, or major.
#>
param(
    [ValidateSet('major', 'minor', 'patch')]
    [string]$Part = 'patch'
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$VersionFile = Join-Path $Root 'VERSION'
$InfoPlist = Join-Path $Root 'cpp\macos\Info.plist'

if (-not (Test-Path -LiteralPath $VersionFile)) {
    throw "Missing VERSION file at $VersionFile"
}

$current = (Get-Content -LiteralPath $VersionFile -Raw).Trim()
if ($current -notmatch '^(\d+)\.(\d+)\.(\d+)$') {
    throw "VERSION must be semver major.minor.patch, got: $current"
}

$major = [int]$Matches[1]
$minor = [int]$Matches[2]
$patch = [int]$Matches[3]

switch ($Part) {
    'major' {
        $major++
        $minor = 0
        $patch = 0
    }
    'minor' {
        $minor++
        $patch = 0
    }
    'patch' {
        $patch++
    }
}

$newVersion = "$major.$minor.$patch"
[System.IO.File]::WriteAllText($VersionFile, $newVersion, (New-Object System.Text.UTF8Encoding $false))
Write-Host "VERSION: $current -> $newVersion" -ForegroundColor Cyan

if (Test-Path -LiteralPath $InfoPlist) {
    $plist = Get-Content -LiteralPath $InfoPlist -Raw
    $plist = [regex]::Replace(
        $plist,
        '(<key>CFBundleShortVersionString</key>\s*<string>)[^<]+(</string>)',
        "`${1}$newVersion`${2}")
    $plist = [regex]::Replace(
        $plist,
        '(<key>CFBundleVersion</key>\s*<string>)[^<]+(</string>)',
        "`${1}$newVersion`${2}")
    Set-Content -LiteralPath $InfoPlist -Value $plist -NoNewline -Encoding utf8
    Write-Host "Updated $InfoPlist" -ForegroundColor DarkGray
}

& (Join-Path $PSScriptRoot 'generate-version-h.ps1')
