#pragma once

#include "activity_tracker.h"
#include "brightness.h"
#include "config.h"
#include "location_service.h"
#include "tuya_client.h"

#include <functional>
#include <map>
#include <string>

struct SmartModeCallbacks {
    std::function<void(bool anyOn, size_t onCount, size_t totalCount)> updateTray;
    std::function<void(const std::string& text)> showSetupMessage;
    std::function<bool()> isBusy;
};

struct DeviceRuntimeState {
    bool hasKnownState = false;
    bool knownOn = false;
    bool hasApplied = false;
    bool lastAppliedOn = false;
    int lastAppliedBrightness = -1;
};

class SmartModeController {
public:
    void Initialize(
        const AppConfig& config,
        TuyaClient* client,
        ILocationService* locationService,
        IActivityTracker* activityTracker,
        IBrightnessController* brightnessController,
        SmartModeCallbacks callbacks);

    void Shutdown();

    bool HasAutomatedDevices() const;
    bool IsLockOffEnabled() const { return lockOffEnabled_; }
    bool IsScreenBrightnessEnabled() const { return screenBrightnessEnabled_; }

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

    void SetKnownDeviceState(const std::string& deviceId, bool on);
    void UpdateConfig(const AppConfig& config);
    void UpdateClient(TuyaClient* client);

    bool GetKnownAggregateOn() const;
    bool HasKnownDeviceStates() const;
    size_t GetKnownOnCount() const;
    size_t GetEnabledDeviceCount() const;

    void SetLockOffEnabled(bool enabled);
    void ToggleLockOffEnabled();
    void SetScreenBrightnessEnabled(bool enabled);
    void ToggleScreenBrightnessEnabled();

    bool SetDeviceSwitch(const DeviceConfig& device, bool on, std::string& error);
    bool ToggleDevice(const DeviceConfig& device, std::string& error, bool& newState);

private:
    struct DesiredDeviceState {
        bool on = false;
        bool applyBrightness = false;
        int brightnessPercent = 100;
    };

    bool EnsureLocation(std::string& error, bool promptForLocation);
    bool ShouldBeOnForDevice(const DeviceConfig& device) const;
    bool IsNightForDevice(const DeviceConfig& device) const;
    bool ShouldUseNightBrightness() const;
    DesiredDeviceState ComputeDesiredState(const DeviceConfig& device) const;
    void ApplyDesiredState(const DeviceConfig& device, const DesiredDeviceState& desired);
    void ApplyBrightness();
    void ReleaseBrightness();
    bool StartActivityTracking(std::string& error);
    void UpdateTrayFromRuntimeState();
    DeviceRuntimeState& RuntimeFor(const std::string& deviceId);

    AppConfig config_{};
    TuyaClient* client_ = nullptr;
    ILocationService* locationService_ = nullptr;
    IActivityTracker* activity_ = nullptr;
    IBrightnessController* brightness_ = nullptr;
    SmartModeCallbacks callbacks_{};
    std::map<std::string, DeviceRuntimeState> runtimeByDeviceId_;
    GeoLocation location_{};
    bool hasLocation_ = false;
    uint64_t locationResolvedAtMs_ = 0;
    bool needsMouseAfterUnlock_ = false;
    bool setupErrorShown_ = false;
    bool powerOffHold_ = false;
    bool lockOffEnabled_ = true;
    bool screenBrightnessEnabled_ = false;
    bool brightnessCaptured_ = false;
    bool hasAppliedBrightness_ = false;
    int lastAppliedBrightnessPercent_ = -1;
    bool brightnessUnavailableNotified_ = false;
    bool activityStarted_ = false;
};
