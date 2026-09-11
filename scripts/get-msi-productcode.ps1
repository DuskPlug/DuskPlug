$db = New-Object -ComObject WindowsInstaller.Installer
$database = $db.OpenDatabase((Resolve-Path "$PSScriptRoot\..\DuskPlug.msi"), 0)
$view = $database.OpenView("SELECT Value FROM Property WHERE Property='ProductCode'")
$view.Execute() | Out-Null
$record = $view.Fetch()
Write-Output $record.StringData(1)
