#Requires -Version 5.1
<#
.SYNOPSIS
    Run the same unit-test paths as GitHub Actions before pushing.

    Windows CI uses test.ps1; Linux and macOS CI use CMake duskplug_tests.
    Running both locally catches drift between those two build paths.
#>
param(
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$CppRoot = $PSScriptRoot
$BuildDir = Join-Path $CppRoot 'build-ci-check'

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

Write-Host '=== Windows unit tests (CI: test-windows) ===' -ForegroundColor Cyan
& (Join-Path $CppRoot 'test.ps1')
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host ''
Write-Host '=== CMake unit tests (CI: test-linux, test-macos) ===' -ForegroundColor Cyan
& cmake -S $CppRoot -B $BuildDir -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& cmake --build $BuildDir --target duskplug_tests
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$testExe = Join-Path $BuildDir 'duskplug_tests.exe'
if (-not (Test-Path -LiteralPath $testExe)) {
    $testExe = Join-Path $BuildDir 'duskplug_tests'
}
& $testExe
exit $LASTEXITCODE
