#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

function Wait-AsyncResult {
    param(
        [Parameter(Mandatory = $true)]
        $AsyncOperation,
        [int]$TimeoutSeconds = 10
    )

    $completed = $false
    $result = $null
    $handlerError = $null

    $handler = [System.EventHandler] {
        param($sender, $eventArgs)
        $script:completed = $true
        try {
            $script:result = $sender.GetResults()
        } catch {
            $script:handlerError = $_
        }
    }

    $AsyncOperation.add_Completed($handler)
    $wait = [System.Threading.WaitHandle]::WaitOne(
        $AsyncOperation.AsyncWaitHandle,
        [TimeSpan]::FromSeconds($TimeoutSeconds))

    if (-not $wait) {
        throw 'Location request timed out'
    }
    if ($handlerError) {
        throw $handlerError
    }
    return $result
}

try {
    [Windows.Devices.Geolocation.Geolocator, Windows.System.Device, ContentType = WindowsRuntime] | Out-Null
    [Windows.Devices.Geolocation.GeolocationAccessStatus, Windows.System.Device, ContentType = WindowsRuntime] | Out-Null

    $access = Wait-AsyncResult -AsyncOperation ([Windows.Devices.Geolocation.Geolocator]::RequestAccessAsync()) -TimeoutSeconds 15
    if ($access -ne [Windows.Devices.Geolocation.GeolocationAccessStatus]::Allowed) {
        [Console]::Error.WriteLine('Location access denied')
        exit 2
    }

    $geolocator = [Windows.Devices.Geolocation.Geolocator]::new()
    $geolocator.DesiredAccuracy = [Windows.Devices.Geolocation.PositionAccuracy]::Default
    $position = Wait-AsyncResult -AsyncOperation ($geolocator.GetGeopositionAsync()) -TimeoutSeconds 10
    $lat = $position.Coordinate.Latitude
    $lon = $position.Coordinate.Longitude
    Write-Output ("{0},{1}" -f $lat, $lon)
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
