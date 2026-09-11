#Requires -Version 5.1

$startupDir = [Environment]::GetFolderPath('Startup')
$shortcutPath = Join-Path $startupDir 'DuskPlug.lnk'

if (Test-Path $shortcutPath) {
    Remove-Item $shortcutPath -Force
    Write-Host "Removed startup shortcut:" -ForegroundColor Green
    Write-Host $shortcutPath
} else {
    Write-Host 'No startup shortcut found.' -ForegroundColor Yellow
}
