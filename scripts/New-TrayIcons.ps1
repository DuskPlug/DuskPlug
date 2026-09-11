#Requires -Version 5.1
# Regenerate DuskPlug tray and application icons.
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
python (Join-Path $PSScriptRoot 'generate_icons.py')
if ($LASTEXITCODE -ne 0) {
    throw 'Icon generation failed'
}
Write-Host "Icons updated in $(Join-Path $Root 'assets')"
