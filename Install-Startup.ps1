#Requires -Version 5.1

$Root = $PSScriptRoot
$startupDir = [Environment]::GetFolderPath('Startup')
$shortcutPath = Join-Path $startupDir 'DuskPlug.lnk'
$exe = Join-Path $Root 'DuskPlug.exe'
$wscript = Join-Path $env:SystemRoot 'System32\wscript.exe'
$launcher = Join-Path $Root 'Launch-Tray.vbs'

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)

if (Test-Path $exe) {
    $shortcut.TargetPath = $exe
    $shortcut.Arguments = ''
    $message = 'Startup now launches DuskPlug.exe (native, no console).'
} else {
    Write-Host 'DuskPlug.exe not found — falling back to Launch-Tray.vbs' -ForegroundColor Yellow
    Write-Host 'Run Setup.cmd or download the release ZIP, or build with Build-DuskPlug.cmd'
    $shortcut.TargetPath = $wscript
    $shortcut.Arguments = "`"$launcher`""
    $message = 'Startup uses Launch-Tray.vbs until DuskPlug.exe is built.'
}

$shortcut.WorkingDirectory = $Root
$shortcut.WindowStyle = 7
$shortcut.Description = 'DuskPlug tray control'
$shortcut.Save()

Write-Host "Startup shortcut created:" -ForegroundColor Green
Write-Host $shortcutPath
Write-Host ''
Write-Host $message
