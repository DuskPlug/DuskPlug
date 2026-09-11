#Requires -Version 5.1

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\TuyaApi.ps1')

$state = Invoke-TuyaPlugAction -Action Off
Write-Host "Plug turned OFF ($state)."
