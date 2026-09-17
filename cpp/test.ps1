#Requires -Version 5.1
param(
    [switch]$Clean,
    [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'build-common.ps1')

$CppRoot = $PSScriptRoot
$Root = Split-Path $CppRoot -Parent
$Src = Join-Path $CppRoot 'src'
$Tests = Join-Path $CppRoot 'tests'
$Out = Join-Path $CppRoot 'DuskPlugTests.exe'
$ObjDir = Join-Path $CppRoot 'obj\test'
$JobCount = Get-DuskPlugJobCount -Requested $Jobs

if ($Clean -and (Test-Path -LiteralPath $ObjDir)) {
    Remove-Item -LiteralPath $ObjDir -Recurse -Force
}

$Gpp = Get-DuskPlugGpp

$flags = @('-std=c++17', '-O0', '-g', '-Wall', "-I$Src", "-I$Tests")
& (Join-Path $Root 'scripts\generate-version-h.ps1')

# Keep this list in sync with duskplug_tests in cpp/CMakeLists.txt (Linux/macOS CI).
$sources = @(
    (Join-Path $Tests 'test_main.cpp'),
    (Join-Path $Tests 'schedule_test.cpp'),
    (Join-Path $Tests 'json_test.cpp'),
    (Join-Path $Tests 'crypto_test.cpp'),
    (Join-Path $Tests 'solar_test.cpp'),
    (Join-Path $Tests 'config_test.cpp'),
    (Join-Path $Tests 'coords_test.cpp'),
    (Join-Path $Tests 'semver_test.cpp'),
    (Join-Path $Tests 'update_checker_test.cpp'),
    (Join-Path $Tests 'settings_page_test.cpp'),
    (Join-Path $Tests 'tray_menu_test.cpp'),
    (Join-Path $Tests 'brightness_display_test.cpp'),
    (Join-Path $Src 'schedule.cpp'),
    (Join-Path $Src 'json_util.cpp'),
    (Join-Path $Src 'crypto.cpp'),
    (Join-Path $Src 'solar.cpp'),
    (Join-Path $Src 'config.cpp'),
    (Join-Path $Src 'coords.cpp'),
    (Join-Path $Src 'settings_page.cpp'),
    (Join-Path $Src 'tray_menu.cpp'),
    (Join-Path $Src 'platform_util.cpp'),
    (Join-Path $Src 'semver.cpp'),
    (Join-Path $Src 'tuya_client.cpp'),
    (Join-Path $Src 'http_win.cpp')
)

New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$stampPath = Join-Path $ObjDir 'flags.txt'
$stampText = Get-FlagStampText -Compiler $Gpp -Flags $flags
$flagsChanged = -not (Test-FlagStamp -StampPath $stampPath -StampText $stampText)

Write-Host "Building DuskPlug tests with $Gpp ($JobCount jobs)..." -ForegroundColor Cyan

$compileJobs = @()
$objects = @()
foreach ($src in $sources) {
    $obj = Join-Path $ObjDir (Get-DuskPlugObjName $src)
    $dep = Join-Path $ObjDir (Get-DuskPlugDepName $src)
    $objects += $obj
    if ($flagsChanged -or (Test-CppObjectOutdated -ObjectPath $obj -SourcePath $src -DepPath $dep)) {
        $compileJobs += @{
            Name = [IO.Path]::GetFileName($src)
            Args = $flags + @('-c', $src, '-o', $obj, '-MMD', '-MF', $dep)
        }
    }
}

if ($compileJobs.Count -gt 0) {
    Write-Host "Compiling $($compileJobs.Count) file(s)..." -ForegroundColor Cyan
    Invoke-ParallelNative -FilePath $Gpp -Jobs @($compileJobs) -MaxJobs $JobCount
} else {
    Write-Host 'Objects up to date.' -ForegroundColor DarkGray
}

$needLink = $flagsChanged -or (Test-AnyInputNewerThan -OutputPath $Out -Inputs $objects)
if ($needLink) {
    Write-Host 'Linking tests...' -ForegroundColor Cyan
    & $Gpp $objects '-o' $Out '-ladvapi32' '-lole32' '-lshell32' '-lcrypt32' '-lwinhttp'
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host 'Link up to date.' -ForegroundColor DarkGray
}

Write-FlagStamp -StampPath $stampPath -StampText $stampText

$Root = Split-Path $CppRoot -Parent
. (Join-Path $Root 'scripts\verify-duskplug-binary.ps1')

Push-Location $CppRoot
try {
    & $Out
    $unitFailures = $LASTEXITCODE
    if ($unitFailures -ne 0) {
        exit $unitFailures
    }

    $exePath = Join-Path $Root 'DuskPlug.exe'
    if (Test-Path -LiteralPath $exePath) {
        $compatFailures = Run-DuskPlugBinaryCompatTests -RootDir $Root -Gpp $Gpp -RunSelfTest
        if ($compatFailures -gt 0) {
            Write-Host "$compatFailures binary compat test(s) failed." -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host 'Skipping binary compat tests (DuskPlug.exe not built yet).' -ForegroundColor DarkGray
    }

    exit 0
}
finally {
    Pop-Location
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $CppRoot 'duskplug_config_test.json')
}
