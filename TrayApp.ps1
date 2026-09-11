#Requires -Version 5.1

Add-Type -AssemblyName System.Windows.Forms, System.Drawing

$Root = $PSScriptRoot
. (Join-Path $Root 'lib\TuyaApi.ps1')
$Script:ConfigPath = Get-SmartConfigPath
$Script:IconOnPath = Join-Path $Root 'assets\light-on.ico'
$Script:IconOffPath = Join-Path $Root 'assets\light-off.ico'
$Script:LogPath = Join-Path $env:TEMP 'DuskPlug-tray.log'
$Script:Busy = $false
$Script:HasKnownState = $false
$Script:KnownOn = $false

$mutexName = 'Global\DuskPlug-SingleInstance'
$mutex = New-Object System.Threading.Mutex($false, $mutexName)
if (-not $mutex.WaitOne(0, $false)) {
    exit 0
}

function Write-TrayLog {
    param([string]$Message)
    $line = '{0} {1}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message
    Add-Content -Path $Script:LogPath -Value $line -ErrorAction SilentlyContinue
}

function Set-TrayPlugState {
    param([bool]$On)

    $Script:HasKnownState = $true
    $Script:KnownOn = $On

    $path = if ($On) { $Script:IconOnPath } else { $Script:IconOffPath }
    $newIcon = [System.Drawing.Icon]::new($path)
    $oldIcon = $Script:NotifyIcon.Icon
    $Script:NotifyIcon.Icon = $newIcon
    if ($oldIcon) {
        $oldIcon.Dispose()
    }

    if ($On) {
        $Script:NotifyIcon.Text = 'Plug: ON (click to toggle)'
    } else {
        $Script:NotifyIcon.Text = 'Plug: OFF (click to toggle)'
    }
}

function Invoke-PlugAction {
    param(
        [ValidateSet('Toggle', 'On', 'Off', 'Status')]
        [string]$Action
    )

    if ($Script:Busy) {
        return
    }

    $Script:Busy = $true
    if ($Action -ne 'Status') {
        $Script:NotifyIcon.Text = 'Plug: working...'
    }

    try {
        $state = Invoke-TuyaPlugAction -Action $Action -ConfigPath $Script:ConfigPath
        Set-TrayPlugState -On ($state -eq 'on')
    } catch {
        Write-TrayLog "$Action failed: $($_.Exception.Message)"
        if ($Script:HasKnownState) {
            Set-TrayPlugState -On $Script:KnownOn
        } else {
            $Script:NotifyIcon.Text = 'Plug: status unknown'
        }
    } finally {
        $Script:Busy = $false
    }
}

function Stop-TrayApp {
    $Script:PollTimer.Stop()
    $Script:NotifyIcon.Visible = $false
    if ($Script:NotifyIcon.Icon) {
        $Script:NotifyIcon.Icon.Dispose()
    }
    $Script:NotifyIcon.Dispose()
    $Script:HostForm.Close()
    if ($mutex) {
        try { $mutex.ReleaseMutex() } catch { }
        $mutex.Dispose()
    }
}

function Restart-TrayApp {
    $exe = Join-Path $Root 'DuskPlug.exe'
    if ($mutex) {
        try { $mutex.ReleaseMutex() } catch { }
        $mutex.Dispose()
        $mutex = $null
    }

    if (Test-Path $exe) {
        Start-Process -FilePath $exe -WorkingDirectory $Root | Out-Null
    } else {
        $wscript = Join-Path $env:SystemRoot 'System32\wscript.exe'
        $launcher = Join-Path $Root 'Launch-Tray.vbs'
        Start-Process -FilePath $wscript -ArgumentList "`"$launcher`"" -WorkingDirectory $Root | Out-Null
    }

    Stop-TrayApp
}

try {
    if (-not (Test-Path $Script:IconOnPath) -or -not (Test-Path $Script:IconOffPath)) {
        & (Join-Path $Root 'scripts\New-TrayIcons.ps1')
    }

    $Script:HostForm = New-Object System.Windows.Forms.Form
    $Script:HostForm.ShowInTaskbar = $false
    $Script:HostForm.FormBorderStyle = [System.Windows.Forms.FormBorderStyle]::None
    $Script:HostForm.Size = New-Object System.Drawing.Size 1, 1
    $Script:HostForm.Opacity = 0
    $Script:HostForm.StartPosition = [System.Windows.Forms.FormStartPosition]::Manual
    $Script:HostForm.Location = New-Object System.Drawing.Point (-100, -100)

    $Script:NotifyIcon = New-Object System.Windows.Forms.NotifyIcon
    $Script:NotifyIcon.Icon = [System.Drawing.Icon]::new($Script:IconOffPath)
    $Script:NotifyIcon.Text = 'Plug: starting...'
    $Script:NotifyIcon.Visible = $true

    $menu = New-Object System.Windows.Forms.ContextMenuStrip
    $null = $menu.Items.Add('Turn On', $null, { Invoke-PlugAction -Action On })
    $null = $menu.Items.Add('Turn Off', $null, { Invoke-PlugAction -Action Off })
    $null = $menu.Items.Add('Refresh Status', $null, { Invoke-PlugAction -Action Status })
    $null = $menu.Items.Add('-')
    $null = $menu.Items.Add('Restart', $null, { Restart-TrayApp })
    $null = $menu.Items.Add('Exit', $null, { Stop-TrayApp })

    $Script:NotifyIcon.ContextMenuStrip = $menu

    $Script:NotifyIcon.Add_MouseClick({
        param($sender, $e)
        if ($e.Button -ne [System.Windows.Forms.MouseButtons]::Left) {
            return
        }
        Invoke-PlugAction -Action Toggle
    })

    $Script:PollTimer = New-Object System.Windows.Forms.Timer
    $Script:PollTimer.Interval = 30000
    $Script:PollTimer.Add_Tick({ Invoke-PlugAction -Action Status })
    $Script:PollTimer.Start()

    $Script:HostForm.Add_Load({
        $Script:HostForm.Hide()
        Invoke-PlugAction -Action Status
    })

    [void]$Script:HostForm.Show()
    [System.Windows.Forms.Application]::Run($Script:HostForm)
}
catch {
    Write-TrayLog "Fatal: $($_.Exception.Message)"
    [System.Windows.Forms.MessageBox]::Show(
        "DuskPlug tray failed to start:`n$($_.Exception.Message)`n`nSee $Script:LogPath",
        'DuskPlug',
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Error
    ) | Out-Null
    exit 1
}
finally {
    if ($mutex) {
        try { $mutex.ReleaseMutex() } catch { }
        $mutex.Dispose()
    }
}
