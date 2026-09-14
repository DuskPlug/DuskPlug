#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

$copyScript = Join-Path $PSScriptRoot 'install-copy-only.ps1'
$elevated = Start-Process -FilePath 'powershell.exe' -ArgumentList @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', $copyScript
) -Verb RunAs -PassThru -Wait

if (-not $elevated) {
    throw 'Elevation was cancelled.'
}
if ($elevated.ExitCode -ne 0) {
    $log = Join-Path (Split-Path $PSScriptRoot -Parent) 'install-last.log'
    $detail = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
    throw "Install failed (exit $($elevated.ExitCode)). $detail"
}

$log = Join-Path (Split-Path $PSScriptRoot -Parent) 'install-last.log'
if (Test-Path -LiteralPath $log) {
    Get-Content -LiteralPath $log
}
