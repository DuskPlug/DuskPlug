#pragma once

struct HWND__;
typedef HWND__* HWND;

bool TryWinRtGeolocator(HWND hwnd, double& latitude, double& longitude, bool requestAccess);
