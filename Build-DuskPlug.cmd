@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0cpp\build.ps1" %*
set EXITCODE=%ERRORLEVEL%
if %EXITCODE% NEQ 0 pause
exit /b %EXITCODE%
