#Requires -Version 5.1

param(
    [string]$DeviceId
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\TuyaApi.ps1')

$params = @{ Action = 'On' }
if ($DeviceId) {
    $params.DeviceId = $DeviceId
}

$state = Invoke-TuyaPlugAction @params
Write-Host "Device turned ON ($state)."
