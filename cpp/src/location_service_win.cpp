#include "platform_win.h"

#include "activity_win.h"
#include "location_win.h"
#include "platform_util.h"

#ifdef _WIN32

namespace {

class WinLocationService : public ILocationService {
public:
    WinLocationService(HWND hwnd, std::wstring appDir) : hwnd_(hwnd), appDir_(std::move(appDir)) {}

    bool TryResolve(const AppConfig& config, GeoLocation& out, std::string& error) override {
        std::wstring wideError;
        if (!ResolveLocation(hwnd_, config, appDir_, out, wideError)) {
            error = WideToUtf8(wideError);
            return false;
        }
        return true;
    }

    LocationPromptResult ResolveWithPrompt(const AppConfig& config, GeoLocation& out, std::string& error) override {
        std::wstring wideError;
        switch (ResolveLocationWithPrompt(hwnd_, config, appDir_, out, wideError)) {
        case LocationResolveResult::Success:
            return LocationPromptResult::Success;
        case LocationResolveResult::OpenedSettings:
            return LocationPromptResult::OpenedSettings;
        case LocationResolveResult::Cancelled:
            return LocationPromptResult::Cancelled;
        case LocationResolveResult::Failed:
        default:
            error = WideToUtf8(wideError);
            return LocationPromptResult::Failed;
        }
    }

    void OpenSettings() override {
        OpenWindowsLocationSettings();
    }

private:
    HWND hwnd_ = nullptr;
    std::wstring appDir_;
};

class WinActivityTracker : public ActivityTracker {
public:
    explicit WinActivityTracker(HWND hwnd) {
        SetWindow(hwnd);
    }
};

}  // namespace

ILocationService* CreateWinLocationService(HWND hwnd, const std::wstring& appDir) {
    return new WinLocationService(hwnd, appDir);
}

IActivityTracker* CreateWinActivityTracker(HWND hwnd) {
    return new WinActivityTracker(hwnd);
}

#endif
