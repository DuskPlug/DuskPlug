#include "smart_mode.h"

#include "json_util.h"
#include "schedule.h"
#include "solar.h"

#include <fstream>
#include <shlobj.h>
#include <sstream>

namespace {

std::wstring GetStatePath() {
    wchar_t appData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData))) {
        return L"state.json";
    }
    std::wstring path(appData);
    path += L"\\SMART";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"\\state.json";
    return path;
}

std::string ReadTextFileUtf8(const std::wstring& path) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return {};
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 65536) {
        CloseHandle(file);
        return {};
    }

    std::string contents(static_cast<size_t>(size.QuadPart), '\0');
    DWORD read = 0;
    const BOOL ok = ReadFile(file, contents.data(), static_cast<DWORD>(contents.size()), &read, nullptr);
    CloseHandle(file);
    if (!ok || read == 0) {
        return {};
    }
    contents.resize(read);
    return contents;
}

bool WriteTextFileUtf8(const std::wstring& path, const std::string& contents) {
    HANDLE file = CreateFileW(
        path.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(
        file,
        contents.data(),
        static_cast<DWORD>(contents.size()),
        &written,
        nullptr);
    CloseHandle(file);
    return ok && written == contents.size();
}

bool LocationIsStale(ULONGLONG resolvedAtMs) {
    if (resolvedAtMs == 0) {
        return true;
    }
    return GetTickCount64() - resolvedAtMs > 24ULL * 60 * 60 * 1000;
}

}  // namespace

void SmartModeController::Initialize(
    HWND hwnd,
    const AppConfig& config,
    const std::wstring& appDir,
    TuyaClient* client,
    SmartModeCallbacks callbacks) {
    hwnd_ = hwnd;
    config_ = config;
    appDir_ = appDir;
    client_ = client;
    callbacks_ = std::move(callbacks);

    activity_.SetLockOffSeconds(config_.lockOffSeconds);
    activity_.SetLockOffCallback([this]() { OnLockOffDue(); });
    activity_.SetLockActivityCallback([this]() { Evaluate(); });
}

void SmartModeController::Shutdown(HWND hwnd) {
    if (mode_ != ControlMode::Manual) {
        activity_.Stop(hwnd);
    }
}

void SmartModeController::LoadPersistedState() {
    const std::wstring path = GetStatePath();
    const std::string json = ReadTextFileUtf8(path);
    if (json.empty()) {
        return;
    }

    bool scheduleMode = false;
    if (JsonGetBool(json, "ScheduleMode", scheduleMode) && scheduleMode) {
        std::wstring error;
        if (!EnableSchedule(error)) {
            if (callbacks_.showSetupBalloon && !setupErrorShown_) {
                callbacks_.showSetupBalloon(error.c_str());
                setupErrorShown_ = true;
            }
            SavePersistedState();
        }
        return;
    }

    bool smartMode = false;
    if (!JsonGetBool(json, "SmartMode", smartMode) || !smartMode) {
        return;
    }

    if (!config_.hasLatitude || !config_.hasLongitude) {
        if (callbacks_.showSetupBalloon && !setupErrorShown_) {
            callbacks_.showSetupBalloon(
                L"Smart Mode is saved but needs location first. Open Settings.");
            setupErrorShown_ = true;
        }
        return;
    }

    std::wstring error;
    if (!Enable(error, false)) {
        if (callbacks_.showSetupBalloon && !setupErrorShown_) {
            callbacks_.showSetupBalloon(error.c_str());
            setupErrorShown_ = true;
        }
        SavePersistedState();
    }
}

void SmartModeController::SavePersistedState() const {
    const std::string json = std::string(R"({"SmartMode":)")
        + (mode_ == ControlMode::Smart ? "true" : "false")
        + R"(,"ScheduleMode":)"
        + (mode_ == ControlMode::Schedule ? "true" : "false")
        + "}";
    WriteTextFileUtf8(GetStatePath(), json);
}

bool SmartModeController::EnsureLocation(std::wstring& error, bool promptForLocation) {
    if (hasLocation_ && !LocationIsStale(locationResolvedAtMs_)) {
        return true;
    }

    GeoLocation resolved{};
    if (promptForLocation) {
        const LocationResolveResult result = ResolveLocationWithPrompt(hwnd_, config_, appDir_, resolved, error);
        if (result == LocationResolveResult::Success) {
            location_ = resolved;
            hasLocation_ = true;
            locationResolvedAtMs_ = GetTickCount64();
            return true;
        }
        hasLocation_ = false;
        return false;
    }

    if (!ResolveLocation(hwnd_, config_, appDir_, resolved, error)) {
        hasLocation_ = false;
        return false;
    }

    location_ = resolved;
    hasLocation_ = true;
    locationResolvedAtMs_ = GetTickCount64();
    return true;
}

bool SmartModeController::StartActivityTracking(std::wstring& error) {
    needsMouseAfterUnlock_ = false;
    activity_.ClearMouseMovedFlag();
    if (!activity_.Start(hwnd_)) {
        error = L"Could not start activity tracking";
        return false;
    }
    return true;
}

bool SmartModeController::Enable(std::wstring& error, bool promptForLocation) {
    if (!EnsureLocation(error, promptForLocation)) {
        return false;
    }

    if (mode_ == ControlMode::Schedule) {
        activity_.Stop(hwnd_);
    }

    mode_ = ControlMode::Smart;
    if (!StartActivityTracking(error)) {
        mode_ = ControlMode::Manual;
        return false;
    }

    SavePersistedState();
    Evaluate();
    return true;
}

bool SmartModeController::EnableSchedule(std::wstring& error) {
    int onMinutes = 0;
    int offMinutes = 0;
    if (!ParseTimeHHMM(config_.scheduleOnTime, onMinutes)
        || !ParseTimeHHMM(config_.scheduleOffTime, offMinutes)) {
        error = L"Schedule times are missing or invalid. Open Settings.";
        return false;
    }

    if (mode_ == ControlMode::Smart) {
        activity_.Stop(hwnd_);
    }

    mode_ = ControlMode::Schedule;
    if (!StartActivityTracking(error)) {
        mode_ = ControlMode::Manual;
        return false;
    }

    SavePersistedState();
    Evaluate();
    return true;
}

void SmartModeController::Disable(HWND hwnd) {
    if (mode_ != ControlMode::Manual) {
        activity_.Stop(hwnd);
    }
    mode_ = ControlMode::Manual;
    needsMouseAfterUnlock_ = false;
    powerOffHold_ = false;
    SavePersistedState();
}

void SmartModeController::SetKnownPlugState(bool on) {
    hasKnownPlugState_ = true;
    knownPlugOn_ = on;
}

void SmartModeController::UpdateConfig(const AppConfig& config) {
    config_ = config;
    hasLocation_ = false;
    locationResolvedAtMs_ = 0;
    activity_.SetLockOffSeconds(config_.lockOffSeconds);
}

void SmartModeController::UpdateClient(TuyaClient* client) {
    client_ = client;
}

bool SmartModeController::ShouldBeOnSchedule() const {
    int onMinutes = 0;
    int offMinutes = 0;
    if (!ParseTimeHHMM(config_.scheduleOnTime, onMinutes)
        || !ParseTimeHHMM(config_.scheduleOffTime, offMinutes)) {
        return false;
    }

    if (activity_.IsSessionLocked() && activity_.IsLockOffDue()) {
        return false;
    }

    if (!ShouldBeOnForSchedule(onMinutes, offMinutes, GetLocalMinutesNow())) {
        return false;
    }

    if (activity_.IsSessionLocked() && !activity_.IsLockOffDue()) {
        return true;
    }

    if (needsMouseAfterUnlock_ && !activity_.HasMouseMovedSinceUnlock()) {
        return false;
    }

    return true;
}

bool SmartModeController::ShouldBeOn() const {
    if (mode_ == ControlMode::Schedule) {
        return ShouldBeOnSchedule();
    }

    if (!hasLocation_) {
        return false;
    }

    if (activity_.IsSessionLocked() && activity_.IsLockOffDue()) {
        return false;
    }

    if (!IsDark(location_.latitude, location_.longitude, config_)) {
        return false;
    }

    if (activity_.IsSessionLocked() && !activity_.IsLockOffDue()) {
        return true;
    }

    if (needsMouseAfterUnlock_ && !activity_.HasMouseMovedSinceUnlock()) {
        return false;
    }

    return true;
}

void SmartModeController::ApplyDesiredState(bool desiredOn) {
    if (!client_ || (callbacks_.isBusy && callbacks_.isBusy())) {
        return;
    }

    if (hasApplied_ && lastAppliedOn_ == desiredOn) {
        if (callbacks_.updateTray) {
            callbacks_.updateTray(desiredOn);
        }
        return;
    }

    std::string error;
    if (!client_->SetSwitch(desiredOn, error)) {
        if (hasKnownPlugState_ && callbacks_.updateTray) {
            callbacks_.updateTray(knownPlugOn_);
        }
        return;
    }

    hasApplied_ = true;
    lastAppliedOn_ = desiredOn;
    hasKnownPlugState_ = true;
    knownPlugOn_ = desiredOn;
    if (callbacks_.updateTray) {
        callbacks_.updateTray(desiredOn);
    }
}

void SmartModeController::Evaluate() {
    if (mode_ == ControlMode::Manual || powerOffHold_) {
        return;
    }

    if (mode_ == ControlMode::Smart && LocationIsStale(locationResolvedAtMs_)) {
        std::wstring error;
        EnsureLocation(error, false);
    }

    ApplyDesiredState(ShouldBeOn());
}

void SmartModeController::OnSessionUnlock() {
    if (mode_ == ControlMode::Manual) {
        return;
    }

    needsMouseAfterUnlock_ = true;
    activity_.ClearMouseMovedFlag();
}

void SmartModeController::OnSessionChange(WPARAM event) {
    if (mode_ == ControlMode::Manual) {
        return;
    }

    if (event == WTS_SESSION_UNLOCK) {
        const bool requireMouse = activity_.IsLockOffDue();
        activity_.HandleSessionChange(event);
        if (requireMouse) {
            OnSessionUnlock();
        }
        if (powerOffHold_) {
            OnPowerResume();
        } else {
            Evaluate();
        }
        return;
    }

    activity_.HandleSessionChange(event);
    if (event == WTS_SESSION_LOCK) {
        Evaluate();
    }
}

void SmartModeController::OnLockTimerTick() {
    if (mode_ == ControlMode::Manual) {
        return;
    }

    const SessionTransition transition = activity_.HandleLockTimerTick();

    if (transition == SessionTransition::Locked) {
        Evaluate();
    } else if (transition == SessionTransition::Unlocked) {
        if (activity_.UnlockRequiresMouse()) {
            OnSessionUnlock();
        } else {
            activity_.ClearUnlockRequiresMouse();
        }
        if (powerOffHold_) {
            OnPowerResume();
        } else {
            Evaluate();
        }
    }

    if (needsMouseAfterUnlock_ && activity_.HasMouseMovedSinceUnlock()) {
        needsMouseAfterUnlock_ = false;
        Evaluate();
    }
}

void SmartModeController::OnLockOffDue() {
    if (mode_ == ControlMode::Manual || powerOffHold_) {
        return;
    }

    ApplyDesiredState(false);
}

void SmartModeController::OnPowerSuspend() {
    if (mode_ == ControlMode::Manual || !client_) {
        return;
    }

    powerOffHold_ = true;

    if (hasApplied_ && !lastAppliedOn_) {
        return;
    }

    constexpr unsigned long kSuspendTimeoutMs = 2500;
    const int delaySeconds = config_.lockOffSeconds > 0 ? config_.lockOffSeconds : 30;
    std::string error;
    if (client_->SetCountdownOff(delaySeconds, error, kSuspendTimeoutMs)) {
        hasApplied_ = true;
        lastAppliedOn_ = false;
        hasKnownPlugState_ = true;
        knownPlugOn_ = false;
        if (callbacks_.updateTray) {
            callbacks_.updateTray(false);
        }
        return;
    }

    if (client_->SetSwitch(false, error, kSuspendTimeoutMs)) {
        hasApplied_ = true;
        lastAppliedOn_ = false;
        hasKnownPlugState_ = true;
        knownPlugOn_ = false;
        if (callbacks_.updateTray) {
            callbacks_.updateTray(false);
        }
    }
}

void SmartModeController::OnPowerResume() {
    if (!powerOffHold_) {
        return;
    }

    powerOffHold_ = false;
    hasApplied_ = false;
    Evaluate();
}
