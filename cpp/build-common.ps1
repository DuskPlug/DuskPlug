# Shared helpers for incremental Windows builds. Dot-sourced by build.ps1 and test.ps1.

function Get-DuskPlugGpp {
    $cmd = Get-Command g++ -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $fallback = "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\g++.exe"
    if (Test-Path -LiteralPath $fallback) { return $fallback }
    throw 'g++ not found. Install WinLibs: winget install BrechtSanders.WinLibs.POSIX.UCRT'
}

function Get-DuskPlugJobCount {
    param([int]$Requested = 0)
    if ($Requested -gt 0) { return $Requested }
    $n = [Environment]::ProcessorCount
    if ($n -lt 1) { return 1 }
    return $n
}

function ConvertTo-NativeArg {
    param([string]$Value)
    if ($Value -notmatch '[\s"]') { return $Value }
    '"' + ($Value -replace '"', '\"') + '"'
}

function Get-DuskPlugObjName {
    param([string]$SourcePath)
    [IO.Path]::GetFileNameWithoutExtension($SourcePath) + '.o'
}

function Get-DuskPlugDepName {
    param([string]$SourcePath)
    [IO.Path]::GetFileNameWithoutExtension($SourcePath) + '.d'
}

function Copy-IfChanged {
    param(
        [Parameter(Mandatory)][string]$From,
        [Parameter(Mandatory)][string]$To
    )
    if (Test-Path -LiteralPath $To) {
        $fromItem = Get-Item -LiteralPath $From
        $toItem = Get-Item -LiteralPath $To
        if ($fromItem.Length -eq $toItem.Length -and $fromItem.LastWriteTimeUtc -eq $toItem.LastWriteTimeUtc) {
            return
        }
    }
    $dir = Split-Path -Parent $To
    if ($dir -and -not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }
    Copy-Item -LiteralPath $From -Destination $To -Force
}

function Get-FlagStampText {
    param(
        [Parameter(Mandatory)][string]$Compiler,
        [Parameter(Mandatory)][string[]]$Flags
    )
    (@($Compiler) + $Flags) -join "`n"
}

function Test-FlagStamp {
    param(
        [Parameter(Mandatory)][string]$StampPath,
        [Parameter(Mandatory)][string]$StampText
    )
    if (-not (Test-Path -LiteralPath $StampPath)) { return $false }
    $existing = Get-Content -LiteralPath $StampPath -Raw -ErrorAction SilentlyContinue
    if ($null -eq $existing) { return $false }
    return ($existing.TrimEnd() -eq $StampText.TrimEnd())
}

function Write-FlagStamp {
    param(
        [Parameter(Mandatory)][string]$StampPath,
        [Parameter(Mandatory)][string]$StampText
    )
    Set-Content -LiteralPath $StampPath -Value $StampText -NoNewline -Encoding ascii
}

function Test-CppObjectOutdated {
    param(
        [Parameter(Mandatory)][string]$ObjectPath,
        [Parameter(Mandatory)][string]$SourcePath,
        [Parameter(Mandatory)][string]$DepPath
    )
    if (-not (Test-Path -LiteralPath $ObjectPath)) { return $true }
    if (-not (Test-Path -LiteralPath $SourcePath)) { return $true }
    $objTime = (Get-Item -LiteralPath $ObjectPath).LastWriteTimeUtc
    if ((Get-Item -LiteralPath $SourcePath).LastWriteTimeUtc -gt $objTime) { return $true }
    if (-not (Test-Path -LiteralPath $DepPath)) { return $true }

    $text = Get-Content -LiteralPath $DepPath -Raw
    if ([string]::IsNullOrWhiteSpace($text)) { return $true }
    $text = [regex]::Replace($text, '\\\r?\n', ' ')
    $deps = [regex]::Replace($text, '^[^:]*:\s*', '')
    foreach ($token in ($deps -split '\s+')) {
        if ([string]::IsNullOrWhiteSpace($token)) { continue }
        $depPath = $token -replace '\\ ', ' '
        if (-not (Test-Path -LiteralPath $depPath)) { return $true }
        if ((Get-Item -LiteralPath $depPath).LastWriteTimeUtc -gt $objTime) { return $true }
    }
    return $false
}

function Test-AnyInputNewerThan {
    param(
        [Parameter(Mandatory)][string]$OutputPath,
        [Parameter(Mandatory)][string[]]$Inputs
    )
    if (-not (Test-Path -LiteralPath $OutputPath)) { return $true }
    $outTime = (Get-Item -LiteralPath $OutputPath).LastWriteTimeUtc
    foreach ($inputPath in $Inputs) {
        if (-not (Test-Path -LiteralPath $inputPath)) { return $true }
        if ((Get-Item -LiteralPath $inputPath).LastWriteTimeUtc -gt $outTime) { return $true }
    }
    return $false
}

function Invoke-ParallelNative {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][object[]]$Jobs,
        [int]$MaxJobs = 1
    )
    if ($Jobs.Count -eq 0) { return }
    if ($MaxJobs -lt 1) { $MaxJobs = 1 }

    $pending = New-Object System.Collections.Queue
    foreach ($job in $Jobs) { $pending.Enqueue($job) }
    $running = New-Object System.Collections.Generic.List[object]
    $failures = New-Object System.Collections.Generic.List[string]

    while ($pending.Count -gt 0 -or $running.Count -gt 0) {
        for ($i = $running.Count - 1; $i -ge 0; $i--) {
            $r = $running[$i]
            if (-not $r.Process.HasExited) { continue }
            if ($r.Process.ExitCode -ne 0) {
                [void]$failures.Add("$($r.Name) (exit $($r.Process.ExitCode))")
            }
            $r.Process.Dispose()
            $running.RemoveAt($i)
        }

        while ($pending.Count -gt 0 -and $running.Count -lt $MaxJobs -and $failures.Count -eq 0) {
            $job = $pending.Dequeue()
            Write-Host "  $($job.Name)" -ForegroundColor DarkGray
            $psi = New-Object System.Diagnostics.ProcessStartInfo
            $psi.FileName = $FilePath
            $psi.Arguments = ($job.Args | ForEach-Object { ConvertTo-NativeArg $_ }) -join ' '
            $psi.UseShellExecute = $false
            $proc = New-Object System.Diagnostics.Process
            $proc.StartInfo = $psi
            [void]$proc.Start()
            [void]$running.Add(@{ Process = $proc; Name = $job.Name })
        }

        if ($failures.Count -gt 0) {
            while ($pending.Count -gt 0) { [void]$pending.Dequeue() }
        }

        if ($running.Count -gt 0) {
            Start-Sleep -Milliseconds 40
        }
    }

    if ($failures.Count -gt 0) {
        throw "Compile failed: $($failures -join ', ')"
    }
}
