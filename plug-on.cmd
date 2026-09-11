@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Plug-On.ps1"
exit /b %ERRORLEVEL%
