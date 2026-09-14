Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*' -ErrorAction SilentlyContinue |
    Where-Object { $_.DisplayName -match 'DuskPlug|SmartTray' } |
    Select-Object DisplayName, DisplayVersion, UninstallString, InstallLocation |
    Format-List

Get-Item 'C:\Program Files\DuskPlug\DuskPlug.exe','C:\Users\Chris\SMART\DuskPlug.exe' -ErrorAction SilentlyContinue |
    Select-Object FullName, Length, LastWriteTime |
    Format-Table -AutoSize
