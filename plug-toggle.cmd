@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Plug-Toggle.ps1"
exit /b %ERRORLEVEL%
