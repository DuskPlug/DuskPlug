#Requires -Version 5.1

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
. (Join-Path $Root 'lib\TuyaApi.ps1')

function Show-LinkGuide {
    Write-Host ''
    Write-Host '=== Link your Tuya app to your cloud project (one-time) ===' -ForegroundColor Cyan
    Write-Host ''
    Write-Host 'Portal: https://iot.tuya.com  (same login as platform.tuya.com)'
    Write-Host 'Docs:   https://developer.tuya.com/en/docs/developer/apply-cloud-api-key?id=Kff30z8sv62ah'
    Write-Host ''
    Write-Host 'A) Check your phone app region first'
    Write-Host '   Tuya / Smart Life / Status app -> Me -> Setting -> Account and Security -> Region'
    Write-Host '   Your cloud project data center MUST serve this region.'
    Write-Host '   UK: try Central Europe first; if All Devices is empty, try Western Europe.'
    Write-Host ''
    Write-Host 'B) Create or open a cloud project'
    Write-Host '   Left sidebar: Cloud -> Cloud Project -> Project Management'
    Write-Host '   (If you see Cloud -> Development, that is the same My Cloud Projects list.)'
    Write-Host '   If shown, click Upgrade IoT Core Plan and take the free trial.'
    Write-Host '   Create Cloud Project (or Open Project on an existing one).'
    Write-Host '     - Development Method: Smart Home (not Custom)'
    Write-Host '     - Data Center: pick the region that matches your phone app'
    Write-Host '   On Authorize API Services, enable at least:'
    Write-Host '     IoT Core, Authorization Token Management, Smart Home Basic Service,'
    Write-Host '     Device Status Notification'
    Write-Host '   Click Authorize. Skip Create asset / original account (Custom only).'
    Write-Host ''
    Write-Host 'C) Copy API credentials'
    Write-Host '   Open Project -> Overview -> Authorization Key'
    Write-Host '     Access ID / Client ID     -> Setup will ask for this'
    Write-Host '     Access Secret / Client Secret -> Setup will ask for this'
    Write-Host '   Pick your data center in Setup (must match the project Overview page).'
    Write-Host ''
    Write-Host 'D) Link your phone app account'
    Write-Host '   Open Project -> Devices -> Link Tuya App Account -> Add App Account'
    Write-Host '   Choose Tuya App Account Authorization if asked.'
    Write-Host '   Scan the QR code from the same app where your plug already works.'
    Write-Host '   Confirm on the phone. Leave Automatic Link selected.'
    Write-Host ''
    Write-Host 'E) Copy your device Device ID(s)'
    Write-Host '   Project -> Devices tab -> All Devices'
    Write-Host '   Find each plug or bulb and copy its Device ID (about 20 characters).'
    Write-Host ''
}

function Test-InteractiveHost {
    return [Environment]::UserInteractive -and $Host.Name -ne 'ServerRemoteHost'
}

function Test-ConfigReady {
    param($Config)

    foreach ($key in @('ClientId', 'ClientSecret', 'BaseUrl')) {
        $value = $Config.$key
        if ([string]::IsNullOrWhiteSpace($value) -or $value -match 'your_') {
            return $false
        }
    }

    $devices = Get-SmartConfigDevices -Config $Config
    foreach ($device in $devices) {
        if (-not [string]::IsNullOrWhiteSpace($device.Id) -and $device.Id -notmatch 'your_') {
            return $true
        }
    }

    return $false
}

function Initialize-Config {
    $configPath = Get-SmartConfigPath
    $examplePath = Join-Path $Root 'config.example.json'
    $legacyPath = Join-Path $Root 'config.json'

    if (Test-Path $configPath) {
        return $configPath
    }

    if (Test-Path $legacyPath) {
        Copy-Item -Path $legacyPath -Destination $configPath -Force
        Write-Host "Migrated config to $configPath" -ForegroundColor Green
        return $configPath
    }

    if (-not (Test-Path $examplePath)) {
        throw 'config.example.json is missing.'
    }

    Copy-Item -Path $examplePath -Destination $configPath
    Write-Host "Created $configPath" -ForegroundColor Green
    return $configPath
}

function Read-RequiredValue {
    param(
        [Parameter(Mandatory)][string]$Prompt
    )

    while ($true) {
        $value = Read-Host $Prompt
        if (-not [string]::IsNullOrWhiteSpace($value)) {
            return $value.Trim()
        }
        Write-Host 'This field is required.' -ForegroundColor Yellow
    }
}

function Read-DataCenterChoice {
    Write-Host ''
    Write-Host 'Select your Tuya data center:'
    Write-Host '  1) Central Europe (UK / most EU)'
    Write-Host '  2) Western Europe'
    Write-Host '  3) Western America'
    Write-Host '  4) Eastern America'
    Write-Host '  5) Singapore'
    Write-Host '  6) India'
    $choice = 0
    while ($choice -lt 1 -or $choice -gt 6) {
        $input = Read-Host 'Enter 1-6'
        if (-not [int]::TryParse($input, [ref]$choice) -or $choice -lt 1 -or $choice -gt 6) {
            $choice = 0
            Write-Host 'Please enter a number from 1 to 6.' -ForegroundColor Yellow
        }
    }
    return Get-TuyaDataCenterUrl -Choice $choice
}

function New-SetupDeviceEntry {
    param(
        [Parameter(Mandatory)][string]$DeviceId,
        [string]$Name,
        $Discovered
    )

    $displayName = if ([string]::IsNullOrWhiteSpace($Name)) { $DeviceId } else { $Name.Trim() }
    return [ordered]@{
        Id           = $DeviceId.Trim()
        Name         = $displayName
        Type         = $Discovered.Type
        Enabled      = $true
        Capabilities = $Discovered.Capabilities
        Automation   = [ordered]@{
            mode                = 'manual'
            scheduleOnTime      = '18:00'
            scheduleOffTime     = '23:00'
            darkOffsetMinutes   = 0
            lightOffsetMinutes  = 0
            nightBrightness     = 20
            dayBrightness       = 80
            useBrightness       = ($Discovered.Type -eq 'bulb')
        }
    }
}

function Invoke-SetupWizard {
    param([string]$ConfigPath)

    Write-Host ''
    Write-Host '=== DuskPlug setup ===' -ForegroundColor Cyan
    Write-Host 'This connects your Tuya plugs and bulbs to Windows.'
    Write-Host ''
    Show-LinkGuide

    Write-Host ''
    Write-Host '=== Enter your credentials ===' -ForegroundColor Cyan
    $clientId = Read-RequiredValue 'Access ID / Client ID'
    $clientSecret = Read-RequiredValue 'Access Secret / Client Secret'
    $baseUrl = Read-DataCenterChoice

    $tempConfig = [pscustomobject]@{
        ClientId     = $clientId
        ClientSecret = $clientSecret
        BaseUrl      = $baseUrl
        DeviceId     = 'pending'
        SwitchCode   = 'switch_1'
    }

    Write-Host ''
    Write-Host '=== Add devices ===' -ForegroundColor Cyan
    $devices = @()
    do {
        $deviceId = Read-RequiredValue 'Device ID (20 characters)'
        $deviceName = Read-Host 'Friendly name (optional)'
        Write-Host "Discovering functions for $deviceId..." -ForegroundColor Cyan

        $token = Get-TuyaAccessToken -Config $tempConfig
        $tempConfig | Add-Member -NotePropertyName DeviceId -NotePropertyValue $deviceId -Force
        $functions = Get-TuyaDeviceFunctions -Config $tempConfig -AccessToken $token -DeviceId $deviceId
        $discovered = Get-TuyaDiscoveredCapabilities -Functions $functions
        Write-Host "Detected: $($discovered.Type) (switch $($discovered.SwitchCode))" -ForegroundColor Green
        if ($discovered.Capabilities.brightness) {
            Write-Host "Brightness DP: $($discovered.Capabilities.brightness)" -ForegroundColor Green
        }

        $devices += New-SetupDeviceEntry -DeviceId $deviceId -Name $deviceName -Discovered $discovered
        $another = Read-Host 'Add another device? [y/N]'
    } while ($another -match '^[Yy]')

    $primary = $devices[0]
    $values = @{
        ClientId     = $clientId
        ClientSecret = $clientSecret
        BaseUrl      = $baseUrl
        Devices      = $devices
        DeviceId     = $primary.Id
        SwitchCode   = $primary.Capabilities.switch
    }

    Save-SmartConfig -Values $values -ConfigPath $ConfigPath | Out-Null
    Write-Host ''
    Write-Host "Saved $($devices.Count) device(s) to $ConfigPath" -ForegroundColor Green
}

function Offer-PostSetupActions {
    $exe = Join-Path $Root 'DuskPlug.exe'
    if (-not (Test-Path $exe)) {
        Write-Host ''
        Write-Host 'DuskPlug.exe not found in this folder.' -ForegroundColor Yellow
        Write-Host 'Download the latest release ZIP, or run Build-DuskPlug.cmd to build from source.'
        return
    }

    if (-not (Test-InteractiveHost)) {
        return
    }

    $start = Read-Host 'Start DuskPlug now? [Y/n]'
    if ($start -notmatch '^[Nn]') {
        Start-Process -FilePath $exe -WorkingDirectory $Root | Out-Null
    }

    $startup = Read-Host 'Add DuskPlug to Windows startup? [y/N]'
    if ($startup -match '^[Yy]') {
        & (Join-Path $Root 'Install-Startup.ps1')
    }
}

$configPath = Initialize-Config

$existing = Get-Content -Path $configPath -Raw | ConvertFrom-Json
if (-not (Test-ConfigReady -Config $existing)) {
    if (Test-InteractiveHost) {
        Invoke-SetupWizard -ConfigPath $configPath
    } else {
        Write-Host 'DuskPlug config still has placeholder values.' -ForegroundColor Yellow
        Write-Host "Run Setup.cmd on your PC to enter credentials interactively."
        Show-LinkGuide
        exit 0
    }
}

Write-Host ''
Write-Host '=== Testing Tuya connection ===' -ForegroundColor Cyan

try {
    $config = Get-TuyaConfig -ConfigPath $configPath
    $allDevices = Get-SmartConfigDevices -Config $config
    Write-Host "Using $($allDevices.Count) device(s) via $($config.BaseUrl)"

    $token = Get-TuyaAccessToken -Config $config
    Write-Host 'Access token: OK' -ForegroundColor Green

    foreach ($device in $allDevices) {
        Write-Host ''
        Write-Host "=== $($device.Name) ($($device.Id)) ===" -ForegroundColor Cyan
        $functions = Get-TuyaDeviceFunctions -Config $config -AccessToken $token -DeviceId $device.Id
        Write-Host 'Supported device functions:'
        $functions | Format-Table code, type, values -AutoSize

        $switchCode = Get-TuyaDeviceSwitchCode -Device $device
        $switchFunctions = $functions | Where-Object { $_.type -eq 'Boolean' -or $_.code -like 'switch*' }
        if ($switchFunctions -and ($switchFunctions.code -notcontains $switchCode)) {
            $suggested = ($switchFunctions | Select-Object -First 1).code
            Write-Host "Note: configured switch '$switchCode' was not found." -ForegroundColor Yellow
            Write-Host "Consider using '$suggested' in DuskPlug Settings instead." -ForegroundColor Yellow
        }

        $status = Get-TuyaDeviceStatus -Config $config -AccessToken $token -DeviceId $device.Id
        Write-Host 'Current device status:'
        $status | Format-Table code, value -AutoSize

        $current = Get-TuyaSwitchState -Config $config -AccessToken $token -DeviceId $device.Id
        Write-Host "Switch '$switchCode' is currently: $(if ($current) { 'ON' } else { 'OFF' })" -ForegroundColor Green
    }

    $test = 'N'
    if (Test-InteractiveHost) {
        $test = Read-Host 'Run a live toggle test on the first device? (turn OFF then back ON) [y/N]'
    }
    if ($test -match '^[Yy]') {
        $primary = Resolve-TuyaDevice -Config $config
        Write-Host "Turning $($primary.Name) OFF..."
        Set-TuyaDeviceSwitch -Config $config -AccessToken $token -On $false -DeviceId $primary.Id | Out-Null
        Start-Sleep -Seconds 2

        Write-Host "Turning $($primary.Name) ON..."
        Set-TuyaDeviceSwitch -Config $config -AccessToken $token -On $true -DeviceId $primary.Id | Out-Null
        Start-Sleep -Seconds 1

        $after = Get-TuyaSwitchState -Config $config -AccessToken $token -DeviceId $primary.Id
        Write-Host "Test complete. Device is now: $(if ($after) { 'ON' } else { 'OFF' })" -ForegroundColor Green
    }

    Write-Host ''
    Write-Host 'Setup successful!' -ForegroundColor Green
    Write-Host 'Double-click Start-DuskPlug.cmd anytime to run DuskPlug.'
    Offer-PostSetupActions
}
catch {
    Write-Host ''
    Write-Host "Setup failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host ''
    Write-Host 'Common fixes:'
    Write-Host '  1004 sign invalid       -> check Client ID / Secret and data center choice'
    Write-Host '  1106 permission deny    -> enable IoT Core on your cloud project'
    Write-Host '  empty device list/error -> Devices -> Link Tuya App Account -> Add App Account'
    Write-Host '  data center mismatch  -> pick the data center that matches your phone app'
    exit 1
}
