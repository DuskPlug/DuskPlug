#Requires -Version 5.1
$ErrorActionPreference = 'Stop'

function Get-DuskPlugObjdump {
    param([string]$Gpp)
    $binDir = Split-Path $Gpp -Parent
    $objdump = Join-Path $binDir 'objdump.exe'
    if (-not (Test-Path -LiteralPath $objdump)) {
        throw "objdump.exe not found next to g++ at $binDir"
    }
    return $objdump
}

function Test-DuskPlugImportsWinPthread {
    param(
        [Parameter(Mandatory)][string]$ExePath,
        [Parameter(Mandatory)][string]$Objdump
    )
    $output = & $Objdump -p $ExePath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for $ExePath"
    }
    return [bool]($output | Select-String -SimpleMatch 'DLL Name: libwinpthread-1.dll')
}

function Test-DuskPlugDllExportsClockGettime64 {
    param(
        [Parameter(Mandatory)][string]$DllPath,
        [Parameter(Mandatory)][string]$Objdump
    )
    $output = & $Objdump -p $DllPath 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for $DllPath"
    }
    return [bool]($output | Select-String -SimpleMatch 'clock_gettime64')
}

function Copy-DuskPlugRuntimeDlls {
    param(
        [Parameter(Mandatory)][string]$Gpp,
        [Parameter(Mandatory)][string]$DestDir
    )
    $binDir = Split-Path $Gpp -Parent
    $pthread = Join-Path $binDir 'libwinpthread-1.dll'
    if (-not (Test-Path -LiteralPath $pthread)) {
        throw "Missing MinGW runtime DLL: $pthread"
    }
    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    Copy-Item -LiteralPath $pthread -Destination (Join-Path $DestDir 'libwinpthread-1.dll') -Force
}

function Invoke-DuskPlugBinaryCompatChecks {
    param(
        [Parameter(Mandatory)][string]$ExePath,
        [Parameter(Mandatory)][string]$RootDir,
        [Parameter(Mandatory)][string]$Gpp,
        [switch]$RunSelfTest
    )

    if (-not (Test-Path -LiteralPath $ExePath)) {
        throw "DuskPlug binary not found: $ExePath"
    }

    $objdump = Get-DuskPlugObjdump -Gpp $Gpp
    $importsWinPthread = Test-DuskPlugImportsWinPthread -ExePath $ExePath -Objdump $objdump
    if (-not $importsWinPthread) {
        return
    }

    $bundledDll = Join-Path $RootDir 'libwinpthread-1.dll'
    if (-not (Test-Path -LiteralPath $bundledDll)) {
        throw @(
            "DuskPlug.exe imports libwinpthread-1.dll but $bundledDll is missing.",
            'Copy the matching libwinpthread-1.dll from your MinGW bin folder next to DuskPlug.exe.',
            'An older libwinpthread-1.dll elsewhere on PATH causes clock_gettime64 entry point errors.'
        ) -join ' '
    }

    if (-not (Test-DuskPlugDllExportsClockGettime64 -DllPath $bundledDll -Objdump $objdump)) {
        throw "Bundled libwinpthread-1.dll does not export clock_gettime64: $bundledDll"
    }

    if ($RunSelfTest) {
        $proc = Start-Process -FilePath $ExePath -ArgumentList @('--self-test') -PassThru -Wait -WindowStyle Hidden
        if ($proc.ExitCode -ne 0) {
            throw "DuskPlug.exe --self-test exited with code $($proc.ExitCode)"
        }
    }
}

function Run-DuskPlugBinaryCompatTests {
    param(
        [Parameter(Mandatory)][string]$RootDir,
        [Parameter(Mandatory)][string]$Gpp,
        [switch]$RunSelfTest
    )

    $failures = 0
    $exePath = Join-Path $RootDir 'DuskPlug.exe'

    Write-Host 'binary compat tests'

    try {
        Invoke-DuskPlugBinaryCompatChecks -ExePath $exePath -RootDir $RootDir -Gpp $Gpp
        Write-Host '  OK  BundledWinPthreadWhenImported' -ForegroundColor Green
    } catch {
        Write-Host "  FAIL BundledWinPthreadWhenImported: $($_.Exception.Message)" -ForegroundColor Red
        $failures++
    }

    if ($RunSelfTest) {
        try {
            Invoke-DuskPlugBinaryCompatChecks -ExePath $exePath -RootDir $RootDir -Gpp $Gpp -RunSelfTest
            Write-Host '  OK  ProcessSelfTestLaunch' -ForegroundColor Green
        } catch {
            Write-Host "  FAIL ProcessSelfTestLaunch: $($_.Exception.Message)" -ForegroundColor Red
            $failures++
        }
    }

    return $failures
}
