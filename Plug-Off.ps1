#Requires -Version 5.1

param(
    [string]$DeviceId
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib\TuyaApi.ps1')

$params = @{ Action = 'Off' }
if ($DeviceId) {
    $params.DeviceId = $DeviceId
}

$state = Invoke-TuyaPlugAction @params
Write-Host "Device turned OFF ($state)."
