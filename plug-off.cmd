@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Plug-Off.ps1"
exit /b %ERRORLEVEL%
