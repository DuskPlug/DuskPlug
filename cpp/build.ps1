#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

$Root = Split-Path $PSScriptRoot -Parent
$Src = Join-Path $Root 'cpp\src'
$Out = Join-Path $Root 'DuskPlug.exe'

& (Join-Path $Root 'scripts\generate-version-h.ps1')

$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10"
$sdkVersion = Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory |
    Sort-Object Name -Descending |
    Select-Object -First 1
if (-not $sdkVersion) {
    throw 'Windows 10 SDK not found'
}

$sdkName = $sdkVersion.Name
$winRtInc = Join-Path $sdkRoot "Include\$sdkName\winrt"
$sharedInc = Join-Path $sdkRoot "Include\$sdkName\shared"
$umInc = Join-Path $sdkRoot "Include\$sdkName\um"
$libDir = Join-Path $sdkRoot "Lib\$sdkName\um\x64"

$Gpp = $null
$cmd = Get-Command g++ -ErrorAction SilentlyContinue
if ($cmd) { $Gpp = $cmd.Source }
if (-not $Gpp) {
    $fallback = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"
    if (Test-Path $fallback) { $Gpp = $fallback }
}

if (-not $Gpp) {
    throw 'g++ not found. Install WinLibs: winget install BrechtSanders.WinLibs.POSIX.UCRT'
}

$BinDir = Split-Path $Gpp -Parent
$Windres = Join-Path $BinDir 'windres.exe'
if (-not (Test-Path $Windres)) {
    throw "windres not found next to g++ at $BinDir"
}

$AppIcon = Join-Path $Root 'assets\app.ico'
if (-not (Test-Path $AppIcon)) {
    Write-Host 'Generating icons...' -ForegroundColor Yellow
    & (Join-Path $Root 'scripts\New-TrayIcons.ps1')
}

$ResFile = Join-Path $Src 'app.rc'
$ResObj = Join-Path $Src 'app_res.o'
& $Windres $ResFile -O coff -o $ResObj
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$commonFlags = @(
    '-std=c++17', '-O2', '-municode', '-mwindows',
    '-static-libgcc', '-static-libstdc++',
    "-I$Src"
)
$geoFlags = @(
    '-std=c++17', '-O2', '-municode', '-mwindows',
    '-D_AMD64_', '-fpermissive',
    '-static-libgcc', '-static-libstdc++',
    "-I$Src", "-I$winRtInc", "-I$sharedInc", "-I$umInc"
)
$libs = @(
    '-lwinhttp', '-ladvapi32', '-lshell32', '-lcomctl32', '-lole32', '-loleaut32',
    '-luser32', '-lgdi32', '-lwtsapi32', '-lruntimeobject', '-lwindowsapp'
)

$sources = @(
    'main.cpp', 'config.cpp', 'crypto.cpp', 'platform_util.cpp', 'http_win.cpp', 'tuya_client.cpp', 'json_util.cpp',
    'solar.cpp', 'schedule.cpp', 'coords.cpp', 'settings_dialog.cpp', 'location_win.cpp', 'location_cli.cpp', 'activity_win.cpp',
    'location_service_win.cpp', 'smart_mode.cpp', 'semver.cpp', 'install_kind.cpp', 'update_checker.cpp',
    'update_apply_win.cpp'
) | ForEach-Object { Join-Path $Src $_ }

$geoSrc = Join-Path $Src 'location_geolocator.cpp'
$geoObj = Join-Path $Src 'location_geolocator.o'

Write-Host "Building with $Gpp" -ForegroundColor Cyan
Write-Host "Compiling WinRT geolocator module..." -ForegroundColor Cyan
& $Gpp @geoFlags '-c' $geoSrc '-o' $geoObj
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Linking DuskPlug..." -ForegroundColor Cyan
& $Gpp @commonFlags $sources $geoObj $ResObj '-o' $Out "-L$libDir" @libs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Remove-Item -Force -ErrorAction SilentlyContinue $geoObj, (Join-Path $Src 'main_test.o')
Write-Host "Built $Out" -ForegroundColor Green
