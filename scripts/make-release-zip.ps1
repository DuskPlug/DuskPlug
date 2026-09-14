$ErrorActionPreference = 'Stop'
if (Test-Path release-staging) { Remove-Item -Recurse -Force release-staging }
$staging = New-Item -ItemType Directory -Path release-staging -Force
Copy-Item DuskPlug.exe, WebView2Loader.dll, config.example.json, README.md, Setup.ps1, Setup.cmd, Start-DuskPlug.cmd, Install-Startup.ps1, Install-Startup.cmd, Uninstall-Startup.ps1, Uninstall-Startup.cmd, Build-DuskPlug.cmd -Destination $staging
Copy-Item -Recurse assets, lib, docs -Destination $staging
if (Test-Path DuskPlug-Windows.zip) { Remove-Item DuskPlug-Windows.zip }
Compress-Archive -Path "$staging\*" -DestinationPath DuskPlug-Windows.zip
Remove-Item -Recurse -Force $staging
