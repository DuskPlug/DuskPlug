#Requires -Version 5.1

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\TuyaApi.ps1')

$state = Invoke-TuyaPlugAction -Action On
Write-Host "Plug turned ON ($state)."
