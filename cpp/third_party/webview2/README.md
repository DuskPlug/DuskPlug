MinGW cannot compile Microsoft's `WebView2EnvironmentOptions.h` because it depends on WRL.

`build.ps1` and CMake download the WebView2 SDK and replace that header with the stub in `mingw/`.
The Evergreen WebView2 Runtime is still required at runtime (preinstalled on Windows 11 and most Windows 10 PCs with Edge).
