# Tuya Cloud API helpers for Smart Home device control.

$Script:EmptyBodySha256 = 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'

function Get-SmartAppDataDir {
    $dir = Join-Path $env:APPDATA 'SMART'
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    return $dir
}

function Get-SmartConfigPath {
    param(
        [string]$LegacyPath = (Join-Path (Split-Path $PSScriptRoot -Parent) 'config.json')
    )

    $appDataConfig = Join-Path (Get-SmartAppDataDir) 'config.json'

    if (Test-Path $appDataConfig) {
        return $appDataConfig
    }

    if ($LegacyPath -and (Test-Path $LegacyPath)) {
        Copy-Item -Path $LegacyPath -Destination $appDataConfig -Force
        return $appDataConfig
    }

    return $appDataConfig
}

function Get-TuyaConfig {
    param(
        [string]$ConfigPath = (Get-SmartConfigPath)
    )

    if (-not (Test-Path $ConfigPath)) {
        throw "Config not found at '$ConfigPath'. Copy config.example.json there and fill in your credentials, or run Setup.ps1."
    }

    $config = Get-Content -Path $ConfigPath -Raw | ConvertFrom-Json

    foreach ($key in @('ClientId', 'ClientSecret', 'DeviceId', 'BaseUrl')) {
        if ([string]::IsNullOrWhiteSpace($config.$key)) {
            throw "Config is missing required value: $key"
        }
    }

    if ([string]::IsNullOrWhiteSpace($config.SwitchCode)) {
        $config | Add-Member -NotePropertyName SwitchCode -NotePropertyValue 'switch_1' -Force
    }

    return $config
}

function Get-TuyaDataCenterUrl {
    param([int]$Choice)

    switch ($Choice) {
        1 { return 'https://openapi.tuyaeu.com' }
        2 { return 'https://openapi-weaz.tuyaeu.com' }
        3 { return 'https://openapi.tuyaus.com' }
        4 { return 'https://openapi-ueaz.tuyaus.com' }
        5 { return 'https://openapi-sg.iotbing.com' }
        6 { return 'https://openapi.tuyain.com' }
        default { return $null }
    }
}

function Test-TimeHHMM {
    param([string]$Value)

    return $Value -match '^\d{1,2}:\d{2}$' -and ([int]($Value.Split(':')[0])) -le 23 -and ([int]($Value.Split(':')[1])) -le 59
}

function Save-SmartConfig {
    param(
        [Parameter(Mandatory)]
        [hashtable]$Values,

        [string]$ConfigPath = (Get-SmartConfigPath)
    )

    $examplePath = Join-Path (Split-Path $PSScriptRoot -Parent) 'config.example.json'
    if (Test-Path $examplePath) {
        $config = Get-Content -Path $examplePath -Raw | ConvertFrom-Json
    } else {
        $config = [ordered]@{}
    }

    foreach ($entry in $Values.GetEnumerator()) {
        $config | Add-Member -NotePropertyName $entry.Key -NotePropertyValue $entry.Value -Force
    }

    if ([string]::IsNullOrWhiteSpace($config.SwitchCode)) {
        $config | Add-Member -NotePropertyName SwitchCode -NotePropertyValue 'switch_1' -Force
    }

    $dir = Split-Path $ConfigPath -Parent
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    ($config | ConvertTo-Json -Depth 5) | Set-Content -Path $ConfigPath -Encoding UTF8
    return $ConfigPath
}

function Get-TuyaSha256 {
    param([string]$Text)

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
    $hash = [System.Security.Cryptography.SHA256]::Create().ComputeHash($bytes)
    return ([BitConverter]::ToString($hash) -replace '-', '').ToLowerInvariant()
}

function Get-TuyaSign {
    param(
        [string]$ClientId,
        [string]$ClientSecret,
        [string]$Timestamp,
        [string]$Nonce,
        [string]$StringToSign,
        [string]$AccessToken = ''
    )

    $message = if ($AccessToken) {
        "$ClientId$AccessToken$Timestamp$Nonce$StringToSign"
    } else {
        "$ClientId$Timestamp$Nonce$StringToSign"
    }

    $keyBytes = [System.Text.Encoding]::UTF8.GetBytes($ClientSecret)
    $messageBytes = [System.Text.Encoding]::UTF8.GetBytes($message)
    $hmac = [System.Security.Cryptography.HMACSHA256]::new($keyBytes)
    $hash = $hmac.ComputeHash($messageBytes)
    return ([BitConverter]::ToString($hash) -replace '-', '').ToUpperInvariant()
}

function Invoke-TuyaApi {
    param(
        [Parameter(Mandatory)]
        [string]$BaseUrl,

        [Parameter(Mandatory)]
        [string]$ClientId,

        [Parameter(Mandatory)]
        [string]$ClientSecret,

        [Parameter(Mandatory)]
        [ValidateSet('GET', 'POST')]
        [string]$Method,

        [Parameter(Mandatory)]
        [string]$Path,

        [string]$AccessToken = '',
        [string]$Body = ''
    )

    $timestamp = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds().ToString()
    $nonce = [guid]::NewGuid().ToString('N')
    $contentSha256 = if ($Body) { Get-TuyaSha256 -Text $Body } else { $Script:EmptyBodySha256 }
    $stringToSign = "$Method`n$contentSha256`n`n$Path"
    $sign = Get-TuyaSign -ClientId $ClientId -ClientSecret $ClientSecret -Timestamp $timestamp -Nonce $nonce -StringToSign $stringToSign -AccessToken $AccessToken

    $headers = @{
        client_id   = $ClientId
        sign        = $sign
        sign_method = 'HMAC-SHA256'
        t           = $timestamp
        nonce       = $nonce
        lang        = 'en'
    }

    if ($AccessToken) {
        $headers.access_token = $AccessToken
    }

    $uri = "$($BaseUrl.TrimEnd('/'))$Path"
    $params = @{
        Uri         = $uri
        Method      = $Method
        Headers     = $headers
        ContentType = 'application/json'
    }

    if ($Body) {
        $params.Body = $Body
    }

    $response = Invoke-RestMethod @params
    return $response
}

function Get-TuyaAccessToken {
    param(
        [Parameter(Mandatory)]
        $Config
    )

    $response = Invoke-TuyaApi -BaseUrl $Config.BaseUrl -ClientId $Config.ClientId -ClientSecret $Config.ClientSecret -Method GET -Path '/v1.0/token?grant_type=1'

    if (-not $response.success) {
        $code = $response.code
        $msg = $response.msg
        throw "Failed to get access token (code $code): $msg"
    }

    return $response.result.access_token
}

function Get-TuyaDeviceFunctions {
    param(
        [Parameter(Mandatory)]
        $Config,

        [Parameter(Mandatory)]
        [string]$AccessToken
    )

    $path = "/v1.0/devices/$($Config.DeviceId)/functions"
    $response = Invoke-TuyaApi -BaseUrl $Config.BaseUrl -ClientId $Config.ClientId -ClientSecret $Config.ClientSecret -Method GET -Path $path -AccessToken $AccessToken

    if (-not $response.success) {
        $code = $response.code
        $msg = $response.msg
        throw "Failed to get device functions (code $code): $msg"
    }

    return $response.result.functions
}

function Get-TuyaDeviceStatus {
    param(
        [Parameter(Mandatory)]
        $Config,

        [Parameter(Mandatory)]
        [string]$AccessToken
    )

    $path = "/v1.0/devices/$($Config.DeviceId)/status"
    $response = Invoke-TuyaApi -BaseUrl $Config.BaseUrl -ClientId $Config.ClientId -ClientSecret $Config.ClientSecret -Method GET -Path $path -AccessToken $AccessToken

    if (-not $response.success) {
        $code = $response.code
        $msg = $response.msg
        throw "Failed to get device status (code $code): $msg"
    }

    return $response.result
}

function Set-TuyaDeviceSwitch {
    param(
        [Parameter(Mandatory)]
        $Config,

        [Parameter(Mandatory)]
        [string]$AccessToken,

        [Parameter(Mandatory)]
        [bool]$On
    )

    $bodyObj = @{
        commands = @(
            @{
                code  = $Config.SwitchCode
                value = $On
            }
        )
    }
    $body = ($bodyObj | ConvertTo-Json -Compress -Depth 5)
    $path = "/v1.0/devices/$($Config.DeviceId)/commands"
    $response = Invoke-TuyaApi -BaseUrl $Config.BaseUrl -ClientId $Config.ClientId -ClientSecret $Config.ClientSecret -Method POST -Path $path -AccessToken $AccessToken -Body $body

    if (-not $response.success) {
        $code = $response.code
        $msg = $response.msg
        throw "Failed to send switch command (code $code): $msg"
    }

    return $response.result
}

function Get-TuyaSwitchState {
    param(
        [Parameter(Mandatory)]
        $Config,

        [Parameter(Mandatory)]
        [string]$AccessToken
    )

    $status = Get-TuyaDeviceStatus -Config $Config -AccessToken $AccessToken
    $switch = $status | Where-Object { $_.code -eq $Config.SwitchCode } | Select-Object -First 1

    if (-not $switch) {
        $available = ($status | ForEach-Object { $_.code }) -join ', '
        throw "Switch code '$($Config.SwitchCode)' not found in device status. Available codes: $available"
    }

    return [bool]$switch.value
}

function Invoke-TuyaPlugAction {
    param(
        [Parameter(Mandatory)]
        [ValidateSet('On', 'Off', 'Status', 'Toggle')]
        [string]$Action,

        [string]$ConfigPath
    )

    $params = @{}
    if ($ConfigPath) {
        $params.ConfigPath = $ConfigPath
    }

    $config = Get-TuyaConfig @params
    $token = Get-TuyaAccessToken -Config $config

    switch ($Action) {
        'On' {
            Set-TuyaDeviceSwitch -Config $config -AccessToken $token -On $true | Out-Null
            return 'on'
        }
        'Off' {
            Set-TuyaDeviceSwitch -Config $config -AccessToken $token -On $false | Out-Null
            return 'off'
        }
        'Status' {
            if (Get-TuyaSwitchState -Config $config -AccessToken $token) {
                return 'on'
            }
            return 'off'
        }
        'Toggle' {
            $current = Get-TuyaSwitchState -Config $config -AccessToken $token
            $next = -not $current
            Set-TuyaDeviceSwitch -Config $config -AccessToken $token -On $next | Out-Null
            if ($next) {
                return 'on'
            }
            return 'off'
        }
    }
}
