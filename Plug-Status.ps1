#Requires -Version 5.1

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\TuyaApi.ps1')

$state = Invoke-TuyaPlugAction -Action Status
Write-Host "Plug is $state."
