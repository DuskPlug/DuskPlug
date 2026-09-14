#pragma once

#include "activity_tracker.h"
#include "config.h"
#include "location_service.h"
#include "tuya_client.h"

#include <functional>
#include <string>

enum class ControlMode {
    Manual,
    Smart,
    Schedule,
};

struct SmartModeCallbacks {
    std::function<void(bool on)> updateTray;
    std::function<void(const std::string& text)> showSetupMessage;
    std::function<bool()> isBusy;
};

class SmartModeController {
public:
    void Initialize(
        const AppConfig& config,
        TuyaClient* client,
        ILocationService* locationService,
        IActivityTracker* activityTracker,
        SmartModeCallbacks callbacks);

    void Shutdown();

    ControlMode GetMode() const { return mode_; }
    bool IsEnabled() const { return mode_ == ControlMode::Smart; }
    bool IsScheduleEnabled() const { return mode_ == ControlMode::Schedule; }
    bool IsAutomationEnabled() const { return mode_ != ControlMode::Manual; }

    bool Enable(std::string& error, bool promptForLocation = true);
    bool EnableSchedule(std::string& error);
    void Disable();

    void LoadPersistedState();
    void SavePersistedState() const;

    void Evaluate();
    void OnLockOffDue();
    void OnSessionUnlock();
    void OnSessionLock();
    void OnSessionChangeEvent(bool isUnlock);
    void OnLockTimerTick();
    void OnPowerSuspend();
    void OnPowerResume();

    void SetKnownPlugState(bool on);
    void UpdateConfig(const AppConfig& config);
    void UpdateClient(TuyaClient* client);

    bool GetKnownPlugState() const { return knownPlugOn_; }
    bool HasKnownPlugState() const { return hasKnownPlugState_; }

    bool IsLockOffEnabled() const { return lockOffEnabled_; }
    void SetLockOffEnabled(bool enabled);
    void ToggleLockOffEnabled();

private:
    bool EnsureLocation(std::string& error, bool promptForLocation);
    bool ShouldBeOn() const;
    bool ShouldBeOnSchedule() const;
    void ApplyDesiredState(bool desiredOn);
    bool StartActivityTracking(std::string& error);

    AppConfig config_{};
    TuyaClient* client_ = nullptr;
    ILocationService* locationService_ = nullptr;
    IActivityTracker* activity_ = nullptr;
    SmartModeCallbacks callbacks_{};
    ControlMode mode_ = ControlMode::Manual;
    GeoLocation location_{};
    bool hasLocation_ = false;
    uint64_t locationResolvedAtMs_ = 0;
    bool hasKnownPlugState_ = false;
    bool knownPlugOn_ = false;
    bool lastAppliedOn_ = false;
    bool hasApplied_ = false;
    bool needsMouseAfterUnlock_ = false;
    bool setupErrorShown_ = false;
    bool powerOffHold_ = false;
    bool lockOffEnabled_ = true;
};
