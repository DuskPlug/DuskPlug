#pragma once

#include "activity_win.h"
#include "config.h"
#include "location_win.h"
#include "tuya_client.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>
#include <string>

enum class ControlMode {
    Manual,
    Smart,
    Schedule,
};

struct SmartModeCallbacks {
    std::function<void(bool on)> updateTray;
    std::function<void(const wchar_t* text)> showSetupBalloon;
    std::function<bool()> isBusy;
};

class SmartModeController {
public:
    void Initialize(
        HWND hwnd,
        const AppConfig& config,
        const std::wstring& appDir,
        TuyaClient* client,
        SmartModeCallbacks callbacks);

    void Shutdown(HWND hwnd);

    ControlMode GetMode() const { return mode_; }
    bool IsEnabled() const { return mode_ == ControlMode::Smart; }
    bool IsScheduleEnabled() const { return mode_ == ControlMode::Schedule; }
    bool IsAutomationEnabled() const { return mode_ != ControlMode::Manual; }

    bool Enable(std::wstring& error, bool promptForLocation = true);
    bool EnableSchedule(std::wstring& error);
    void Disable(HWND hwnd);

    void LoadPersistedState();
    void SavePersistedState() const;

    void Evaluate();
    void OnLockOffDue();
    void OnSessionUnlock();
    void OnSessionChange(WPARAM event);
    void OnLockTimerTick();
    void OnPowerSuspend();
    void OnPowerResume();

    void SetKnownPlugState(bool on);
    void UpdateConfig(const AppConfig& config);
    void UpdateClient(TuyaClient* client);

    bool GetKnownPlugState() const { return knownPlugOn_; }
    bool HasKnownPlugState() const { return hasKnownPlugState_; }

private:
    bool EnsureLocation(std::wstring& error, bool promptForLocation);
    bool ShouldBeOn() const;
    bool ShouldBeOnSchedule() const;
    void ApplyDesiredState(bool desiredOn);
    bool StartActivityTracking(std::wstring& error);

    HWND hwnd_ = nullptr;
    AppConfig config_{};
    std::wstring appDir_;
    TuyaClient* client_ = nullptr;
    SmartModeCallbacks callbacks_{};
    ActivityTracker activity_{};
    ControlMode mode_ = ControlMode::Manual;
    GeoLocation location_{};
    bool hasLocation_ = false;
    ULONGLONG locationResolvedAtMs_ = 0;
    bool hasKnownPlugState_ = false;
    bool knownPlugOn_ = false;
    bool lastAppliedOn_ = false;
    bool hasApplied_ = false;
    bool needsMouseAfterUnlock_ = false;
    bool setupErrorShown_ = false;
    bool powerOffHold_ = false;
};
