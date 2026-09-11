#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

$CppRoot = $PSScriptRoot
$Src = Join-Path $CppRoot 'src'
$Tests = Join-Path $CppRoot 'tests'
$Out = Join-Path $CppRoot 'DuskPlugTests.exe'

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

$flags = @('-std=c++17', '-O0', '-Wall', "-I$Src", "-I$Tests")
$sources = @(
    (Join-Path $Tests 'test_main.cpp'),
    (Join-Path $Tests 'schedule_test.cpp'),
    (Join-Path $Tests 'json_test.cpp'),
    (Join-Path $Tests 'crypto_test.cpp'),
    (Join-Path $Tests 'solar_test.cpp'),
    (Join-Path $Tests 'config_test.cpp'),
    (Join-Path $Src 'schedule.cpp'),
    (Join-Path $Src 'json_util.cpp'),
    (Join-Path $Src 'crypto.cpp'),
    (Join-Path $Src 'solar.cpp'),
    (Join-Path $Src 'config.cpp')
)

Write-Host "Building DuskPlug tests..." -ForegroundColor Cyan
& $Gpp @flags $sources '-o' $Out '-ladvapi32' '-lole32' '-lshell32'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Push-Location $CppRoot
try {
    & $Out
    exit $LASTEXITCODE
}
finally {
    Pop-Location
    Remove-Item -Force -ErrorAction SilentlyContinue $Out, (Join-Path $CppRoot 'smarttray_config_test.json')
}
