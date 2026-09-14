@echo off
setlocal EnableExtensions

cd /d "%~dp0.."
set "ROOT=%CD%"
set "SRC=%ROOT%\cpp\src"
set "OUT=%ROOT%\DuskPlug.exe"

set "VS=C:\Program Files\Microsoft Visual Studio\2022\Community"
set "SDK=10.0.26100.0"
set "KIT=C:\Program Files (x86)\Windows Kits\10"
set "VCVARS=%VS%\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
  echo vcvars64.bat not found at "%VCVARS%"
  exit /b 1
)

call "%VCVARS%" >nul
if errorlevel 1 exit /b 1

set "RES=%SRC%\app_res.res"
"%KIT%\bin\%SDK%\x64\rc.exe" /nologo /i "%SRC%" /fo "%RES%" "%SRC%\app.rc"
if errorlevel 1 exit /b 1

cl /nologo /EHsc /std:c++17 /O2 /DUNICODE /D_UNICODE ^
  /I"%SRC%" ^
  /I"%KIT%\Include\%SDK%\cppwinrt" ^
  "%SRC%\main.cpp" ^
  "%SRC%\config.cpp" ^
  "%SRC%\crypto.cpp" ^
  "%SRC%\http_win.cpp" ^
  "%SRC%\tuya_client.cpp" ^
  "%SRC%\json_util.cpp" ^
  "%SRC%\solar.cpp" ^
  "%SRC%\location_win.cpp" ^
  "%SRC%\location_geolocator.cpp" ^
  "%SRC%\location_cli.cpp" ^
  "%SRC%\activity_win.cpp" ^
  "%SRC%\smart_mode.cpp" ^
  /Fe:"%OUT%" ^
  /link /SUBSYSTEM:WINDOWS "%RES%" user32.lib gdi32.lib winhttp.lib bcrypt.lib shell32.lib comctl32.lib ole32.lib oleaut32.lib advapi32.lib wtsapi32.lib runtimeobject.lib windowsapp.lib
if errorlevel 1 exit /b 1

del /q "%ROOT%\*.obj" "%RES%" 2>nul
echo Built "%OUT%"
