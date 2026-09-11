@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Uninstall-Startup.ps1"
set EXITCODE=%ERRORLEVEL%
if %EXITCODE% NEQ 0 pause
exit /b %EXITCODE%
