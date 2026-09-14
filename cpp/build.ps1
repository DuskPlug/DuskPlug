#Requires -Version 5.1
param(
    [switch]$Release,
    [switch]$Clean,
    [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'build-common.ps1')

$Root = Split-Path $PSScriptRoot -Parent
$Src = Join-Path $Root 'cpp\src'
$Out = Join-Path $Root 'DuskPlug.exe'
$ConfigName = if ($Release) { 'release' } else { 'debug' }
$ObjDir = Join-Path $Root "cpp\obj\$ConfigName"
$JobCount = Get-DuskPlugJobCount -Requested $Jobs

if ($Clean -and (Test-Path -LiteralPath (Join-Path $Root 'cpp\obj'))) {
    Remove-Item -LiteralPath (Join-Path $Root 'cpp\obj') -Recurse -Force
}

& (Join-Path $Root 'scripts\generate-version-h.ps1')

function Install-WebView2Sdk {
    $version = '1.0.2849.39'
    $dest = Join-Path $Root 'cpp\third_party\webview2'
    $includeDir = Join-Path $dest 'include'
    $header = Join-Path $includeDir 'WebView2.h'
    $dll = Join-Path $dest 'x64\WebView2Loader.dll'
    if (-not ((Test-Path $header) -and (Test-Path $dll))) {
        Write-Host "Downloading WebView2 SDK $version..." -ForegroundColor Cyan
        $nupkg = Join-Path $env:TEMP "Microsoft.Web.WebView2.$version.zip"
        Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/$version" -OutFile $nupkg
        $extract = Join-Path $dest 'package'
        if (Test-Path $extract) { Remove-Item -Recurse -Force $extract }
        Expand-Archive -LiteralPath $nupkg -DestinationPath $extract -Force
        New-Item -ItemType Directory -Force -Path $includeDir, (Join-Path $dest 'x64') | Out-Null
        Copy-Item -Force (Join-Path $extract 'build\native\include\*') $includeDir
        Copy-Item -Force (Join-Path $extract 'build\native\x64\WebView2Loader.dll') $dll
    }
    Copy-IfChanged -From (Join-Path $dest 'mingw\WebView2EnvironmentOptions.h') -To (Join-Path $includeDir 'WebView2EnvironmentOptions.h')
    Copy-IfChanged -From $dll -To (Join-Path $Root 'WebView2Loader.dll')
    return $includeDir
}

$WebView2Include = Install-WebView2Sdk

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

$Gpp = Get-DuskPlugGpp
$BinDir = Split-Path $Gpp -Parent
$Windres = Join-Path $BinDir 'windres.exe'
if (-not (Test-Path $Windres)) {
    throw "windres not found next to g++ at $BinDir"
}

$ObjCopy = Join-Path $BinDir 'objcopy.exe'
if (-not (Test-Path $ObjCopy)) {
    throw "objcopy not found next to g++ at $BinDir"
}

$AppIcon = Join-Path $Root 'assets\app.ico'
if (-not (Test-Path $AppIcon)) {
    Write-Host 'Generating icons...' -ForegroundColor Yellow
    & (Join-Path $Root 'scripts\New-TrayIcons.ps1')
}

New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$optFlags = if ($Release) { @('-O2') } else { @('-O0', '-g') }
$commonFlags = @(
    '-std=c++17'
) + $optFlags + @(
    '-municode', '-mwindows', '-fms-extensions',
    '-static-libgcc', '-static-libstdc++',
    "-I$Src", "-I$WebView2Include"
)
$geoFlags = @(
    '-std=c++17'
) + $optFlags + @(
    '-municode', '-mwindows',
    '-D_AMD64_', '-fpermissive',
    '-static-libgcc', '-static-libstdc++',
    "-I$Src", "-I$winRtInc", "-I$sharedInc", "-I$umInc"
)
$linkFlags = @(
    '-std=c++17', '-municode', '-mwindows', '-fms-extensions',
    '-static-libgcc', '-static-libstdc++'
)
if (-not $Release) { $linkFlags += '-g' }

$libs = @(
    '-lwinhttp', '-ladvapi32', '-lshell32', '-lcomctl32', '-lole32', '-loleaut32', '-luuid',
    '-luser32', '-lgdi32', '-lwtsapi32', '-lruntimeobject', '-lwindowsapp', '-ldwmapi'
)

$sources = @(
    'main.cpp', 'config.cpp', 'crypto.cpp', 'platform_util.cpp', 'http_win.cpp', 'tuya_client.cpp', 'json_util.cpp',
    'solar.cpp', 'schedule.cpp', 'coords.cpp', 'settings_page.cpp', 'settings_dialog.cpp', 'location_win.cpp', 'location_cli.cpp', 'activity_win.cpp',
    'location_service_win.cpp', 'brightness_win.cpp', 'smart_mode.cpp', 'semver.cpp', 'install_kind.cpp', 'update_checker.cpp',
    'update_apply_win.cpp'
) | ForEach-Object { Join-Path $Src $_ }

$geoSrc = Join-Path $Src 'location_geolocator.cpp'
$ResFile = Join-Path $Src 'app.rc'
$ResObj = Join-Path $ObjDir 'app_res.o'
$Manifest = Join-Path $Src 'app.manifest'
$ResourceH = Join-Path $Src 'resource.h'

$stampPath = Join-Path $ObjDir 'flags.txt'
$stampText = Get-FlagStampText -Compiler $Gpp -Flags ($commonFlags + @('---') + $geoFlags + @('---') + $linkFlags)
$flagsChanged = -not (Test-FlagStamp -StampPath $stampPath -StampText $stampText)

Write-Host "Building with $Gpp ($ConfigName, $JobCount jobs)" -ForegroundColor Cyan

$compileJobs = @()
$objects = @()
foreach ($src in $sources) {
    $obj = Join-Path $ObjDir (Get-DuskPlugObjName $src)
    $dep = Join-Path $ObjDir (Get-DuskPlugDepName $src)
    $objects += $obj
    if ($flagsChanged -or (Test-CppObjectOutdated -ObjectPath $obj -SourcePath $src -DepPath $dep)) {
        $compileJobs += @{
            Name = [IO.Path]::GetFileName($src)
            Args = $commonFlags + @('-c', $src, '-o', $obj, '-MMD', '-MP', '-MF', $dep)
        }
    }
}

$geoObj = Join-Path $ObjDir (Get-DuskPlugObjName $geoSrc)
$geoDep = Join-Path $ObjDir (Get-DuskPlugDepName $geoSrc)
$objects += $geoObj
if ($flagsChanged -or (Test-CppObjectOutdated -ObjectPath $geoObj -SourcePath $geoSrc -DepPath $geoDep)) {
    $compileJobs += @{
        Name = [IO.Path]::GetFileName($geoSrc)
        Args = $geoFlags + @('-c', $geoSrc, '-o', $geoObj, '-MMD', '-MP', '-MF', $geoDep)
    }
}

if ($compileJobs.Count -gt 0) {
    Write-Host "Compiling $($compileJobs.Count) file(s)..." -ForegroundColor Cyan
    Invoke-ParallelNative -FilePath $Gpp -Jobs $compileJobs -MaxJobs $JobCount
} else {
    Write-Host 'Objects up to date.' -ForegroundColor DarkGray
}

$resInputs = @($ResFile, $ResourceH, $Manifest, $AppIcon)
if (Test-AnyInputNewerThan -OutputPath $ResObj -Inputs $resInputs) {
    Write-Host 'Compiling resources...' -ForegroundColor Cyan
    & $Windres $ResFile -O coff -o $ResObj
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$objects += $ResObj
$needLink = $flagsChanged -or (Test-AnyInputNewerThan -OutputPath $Out -Inputs $objects)
if ($needLink) {
    Write-Host 'Linking DuskPlug...' -ForegroundColor Cyan
    & $Gpp @linkFlags $objects '-o' $Out "-L$libDir" @libs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # MinGW 16 can emit a .retplne section at 0x200000000, which Windows rejects
    # as "not a valid application for this OS platform."
    $stripped = "$Out.stripped.exe"
    & $ObjCopy --remove-section=.retplne $Out $stripped
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Move-Item -Force $stripped $Out
} else {
    Write-Host 'Link up to date.' -ForegroundColor DarkGray
}

Write-FlagStamp -StampPath $stampPath -StampText $stampText

Remove-Item -Force -ErrorAction SilentlyContinue @(
    (Join-Path $Src 'location_geolocator.o'),
    (Join-Path $Src 'app_res.o'),
    (Join-Path $Src 'main_test.o')
)

Write-Host "Built $Out ($ConfigName)" -ForegroundColor Green
