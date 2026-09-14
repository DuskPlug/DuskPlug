#Requires -Version 5.1

param(
    [string]$DeviceId
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\TuyaApi.ps1')

$params = @{ Action = 'Toggle' }
if ($DeviceId) {
    $params.DeviceId = $DeviceId
}

$state = Invoke-TuyaPlugAction @params
Write-Host "Device toggled to $state."
