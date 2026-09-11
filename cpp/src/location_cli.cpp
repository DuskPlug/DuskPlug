#include "config.h"
#include "location_geolocator.h"

#include <cstdio>
#include <iostream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int RunGetLocationMode() {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* stdoutFile = nullptr;
        FILE* stderrFile = nullptr;
        freopen_s(&stdoutFile, "CONOUT$", "w", stdout);
        freopen_s(&stderrFile, "CONOUT$", "w", stderr);
    }

    double lat = 0.0;
    double lon = 0.0;
    if (!TryWinRtGeolocator(nullptr, lat, lon, true)) {
        std::wcerr << L"Location request failed" << std::endl;
        return 1;
    }

    std::wcout << lat << L"," << lon << std::endl;
    return 0;
}
