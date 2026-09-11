#include "location_win.h"

#include "location_geolocator.h"

#include <shellapi.h>

#include <string>

namespace {

bool ApplyConfigFallback(const AppConfig& config, GeoLocation& out) {
    if (!config.hasLatitude || !config.hasLongitude) {
        return false;
    }

    out.latitude = config.latitude;
    out.longitude = config.longitude;
    out.fromWindows = false;
    return true;
}

LocationPromptChoice PromptForLocationAccess(bool hasConfigFallback) {
    const wchar_t* message =
        L"DuskPlug needs a one-time location fix from Windows.\n\n"
        L"You should see a location permission prompt.\n"
        L"After success, DuskPlug will appear under\n"
        L"\"Let desktop apps access your location\".\n\n"
        L"Choose Yes to request location now.\n"
        L"Choose No to open Location settings.\n"
        L"Choose Cancel to keep Smart Mode off.";

    const int choice = MessageBoxW(
        nullptr,
        message,
        L"DuskPlug — Allow Location",
        MB_YESNOCANCEL | MB_ICONINFORMATION);

    if (choice == IDYES) {
        return LocationPromptChoice::RunSetupLocation;
    }
    if (choice == IDNO) {
        return hasConfigFallback ? LocationPromptChoice::UseConfigFallback : LocationPromptChoice::OpenSettings;
    }
    return LocationPromptChoice::Cancelled;
}

bool ResolveLocationInternal(
    HWND hwnd,
    const AppConfig& config,
    const std::wstring&,
    GeoLocation& out) {
    if (ApplyConfigFallback(config, out)) {
        return true;
    }

    double lat = 0.0;
    double lon = 0.0;
    const bool requestAccess = !config.hasLatitude || !config.hasLongitude;
    if (TryWinRtGeolocator(hwnd, lat, lon, requestAccess)) {
        out.latitude = lat;
        out.longitude = lon;
        out.fromWindows = true;
        return true;
    }

    return false;
}

}  // namespace

void OpenWindowsLocationSettings() {
    ShellExecuteW(nullptr, L"open", L"ms-settings:privacy-location", nullptr, nullptr, SW_SHOW);
}

void RunSetupLocationScript(const std::wstring&) {
    // Legacy entry point retained for callers; in-process setup happens in main.cpp.
}

bool ResolveLocation(
    HWND hwnd,
    const AppConfig& config,
    const std::wstring& appDir,
    GeoLocation& out,
    std::wstring& error) {
    if (ResolveLocationInternal(hwnd, config, appDir, out)) {
        return true;
    }

    error = L"Allow DuskPlug to access your location when prompted, "
            L"or set location in Settings.";
    return false;
}

LocationResolveResult ResolveLocationWithPrompt(
    HWND hwnd,
    const AppConfig& config,
    const std::wstring& appDir,
    GeoLocation& out,
    std::wstring& error) {
    if (ResolveLocationInternal(hwnd, config, appDir, out)) {
        return LocationResolveResult::Success;
    }

    const bool hasConfigFallback = config.hasLatitude && config.hasLongitude;
    const LocationPromptChoice choice = PromptForLocationAccess(hasConfigFallback);

    if (choice == LocationPromptChoice::RunSetupLocation) {
        error = L"Open Settings and use Detect Location to allow access.";
        return LocationResolveResult::OpenedSettings;
    }

    if (choice == LocationPromptChoice::OpenSettings) {
        OpenWindowsLocationSettings();
        error = L"Allow desktop apps to access your location, then use Detect Location in Settings.";
        return LocationResolveResult::OpenedSettings;
    }

    if (choice == LocationPromptChoice::Cancelled) {
        error = L"Smart Mode needs location access.";
        return LocationResolveResult::Cancelled;
    }

    if (ApplyConfigFallback(config, out)) {
        return LocationResolveResult::Success;
    }

    error = L"Open Settings and set your location, or use Detect Location.";
    return LocationResolveResult::Failed;
}

bool RequestWindowsLocation(HWND hwnd, double& latitude, double& longitude) {
    return TryWinRtGeolocator(hwnd, latitude, longitude, true);
}
