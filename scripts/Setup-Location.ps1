#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
. (Join-Path $Root 'lib\TuyaApi.ps1')
$configPath = Get-SmartConfigPath

if (-not (Test-Path $configPath)) {
    [System.Windows.Forms.MessageBox]::Show(
        "DuskPlug config not found at $configPath`n`nRun Setup.ps1 or copy config.example.json there.",
        'DuskPlug',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error
    ) | Out-Null
    exit 1
}

Add-Type -AssemblyName System.Windows.Forms

function Wait-AsyncResult {
    param($AsyncOperation, [int]$TimeoutSeconds = 15)
    $result = $null
    $handlerError = $null
    $handler = [System.EventHandler] {
        param($sender, $eventArgs)
        try { $script:result = $sender.GetResults() } catch { $script:handlerError = $_ }
    }
    $AsyncOperation.add_Completed($handler)
    $wait = [System.Threading.WaitHandle]::WaitOne(
        $AsyncOperation.AsyncWaitHandle,
        [TimeSpan]::FromSeconds($TimeoutSeconds))
    if (-not $wait) { throw 'Location request timed out' }
    if ($handlerError) { throw $handlerError }
    return $result
}

function Get-WindowsCoordinates {
    [Windows.Devices.Geolocation.Geolocator, Windows.System.Device, ContentType = WindowsRuntime] | Out-Null
    [Windows.Devices.Geolocation.GeolocationAccessStatus, Windows.System.Device, ContentType = WindowsRuntime] | Out-Null
    $access = Wait-AsyncResult ([Windows.Devices.Geolocation.Geolocator]::RequestAccessAsync())
    if ($access -ne [Windows.Devices.Geolocation.GeolocationAccessStatus]::Allowed) {
        return $null
    }
    $geo = [Windows.Devices.Geolocation.Geolocator]::new()
    $pos = Wait-AsyncResult ($geo.GetGeopositionAsync())
    return [pscustomobject]@{
        Latitude  = [Math]::Round($pos.Coordinate.Latitude, 4)
        Longitude = [Math]::Round($pos.Coordinate.Longitude, 4)
        Source    = 'Windows Location'
    }
}

function Get-ApproxCoordinates {
    $resp = Invoke-RestMethod -Uri 'https://ipapi.co/json/' -TimeoutSec 10
    if ($null -eq $resp.latitude -or $null -eq $resp.longitude) {
        return $null
    }
    return [pscustomobject]@{
        Latitude  = [Math]::Round([double]$resp.latitude, 4)
        Longitude = [Math]::Round([double]$resp.longitude, 4)
        Source    = "Approximate location ($($resp.city), $($resp.country_name))"
    }
}

function Update-ConfigCoordinates {
    param([double]$Latitude, [double]$Longitude)

    $raw = Get-Content -Path $configPath -Raw -Encoding UTF8
    if ($raw.StartsWith([char]0xFEFF)) {
        $raw = $raw.Substring(1)
    }

    $config = $raw | ConvertFrom-Json
    $config | Add-Member -NotePropertyName Latitude -NotePropertyValue $Latitude -Force
    $config | Add-Member -NotePropertyName Longitude -NotePropertyValue $Longitude -Force

    $json = ($config | ConvertTo-Json -Depth 5)
    [System.IO.File]::WriteAllText($configPath, $json, [System.Text.UTF8Encoding]::new($false))
}

$coords = $null
try {
    $coords = Get-WindowsCoordinates
} catch {
    $coords = $null
}

if (-not $coords) {
    try {
        $coords = Get-ApproxCoordinates
    } catch {
        $coords = $null
    }
}

if (-not $coords) {
    [System.Windows.Forms.MessageBox]::Show(
        @"
Could not detect your location automatically.

DuskPlug will NOT appear in the Windows location app list — that is normal.

You can either:
1. Settings → Privacy → Location → turn on "Let desktop apps access your location", then run Set Location again
2. Add Latitude and Longitude manually to %APPDATA%\SMART\config.json
"@,
        'DuskPlug — Set Location',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Warning
    ) | Out-Null
    exit 1
}

Update-ConfigCoordinates -Latitude $coords.Latitude -Longitude $coords.Longitude

[System.Windows.Forms.MessageBox]::Show(
    "Saved location to $configPath`n`nLatitude:  $($coords.Latitude)`nLongitude: $($coords.Longitude)`nSource:      $($coords.Source)`n`nYou can now enable Smart Mode.",
    'DuskPlug — Set Location',
    [System.Windows.Forms.MessageBoxButtons]::OK,
    [System.Windows.Forms.MessageBoxIcon]::Information
) | Out-Null

exit 0
