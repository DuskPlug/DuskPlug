@echo off
set "EXE=%~dp0DuskPlug.exe"
if exist "%EXE%" (
    start "" "%EXE%"
    exit /b 0
)

echo DuskPlug.exe was not found in this folder.
echo.
echo Download the latest release ZIP from GitHub, or run Setup.cmd first.
echo To build from source, run Build-DuskPlug.cmd.
pause
exit /b 1
