#include "smart_mode.h"

#include "json_util.h"
#include "platform_util.h"
#include "schedule.h"
#include "solar.h"

namespace {

bool LocationIsStale(uint64_t resolvedAtMs) {
    if (resolvedAtMs == 0) {
        return true;
    }
    return MonotonicTimeMs() - resolvedAtMs > 24ULL * 60 * 60 * 1000;
}

}  // namespace

void SmartModeController::Initialize(
    const AppConfig& config,
    TuyaClient* client,
    ILocationService* locationService,
    IActivityTracker* activityTracker,
    SmartModeCallbacks callbacks) {
    config_ = config;
    client_ = client;
    locationService_ = locationService;
    activity_ = activityTracker;
    callbacks_ = std::move(callbacks);

    if (activity_) {
        activity_->SetLockOffSeconds(config_.lockOffSeconds);
        activity_->SetLockOffCallback([this]() { OnLockOffDue(); });
        activity_->SetLockActivityCallback([this]() { Evaluate(); });
    }
}

void SmartModeController::Shutdown() {
    if (mode_ != ControlMode::Manual && activity_) {
        activity_->Stop();
    }
}

void SmartModeController::LoadPersistedState() {
    const std::string json = ReadTextFile(GetStatePath());
    if (json.empty()) {
        return;
    }

    bool lockOffEnabled = true;
    if (JsonGetBool(json, "LockOffEnabled", lockOffEnabled)) {
        lockOffEnabled_ = lockOffEnabled;
    }

    bool scheduleMode = false;
    if (JsonGetBool(json, "ScheduleMode", scheduleMode) && scheduleMode) {
        std::string error;
        if (!EnableSchedule(error)) {
            if (callbacks_.showSetupMessage && !setupErrorShown_) {
                callbacks_.showSetupMessage(error);
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
        if (callbacks_.showSetupMessage && !setupErrorShown_) {
            callbacks_.showSetupMessage(
                "Smart Mode is saved but needs location first. Open Settings.");
            setupErrorShown_ = true;
        }
        return;
    }

    std::string error;
    if (!Enable(error, false)) {
        if (callbacks_.showSetupMessage && !setupErrorShown_) {
            callbacks_.showSetupMessage(error);
            setupErrorShown_ = true;
        }
        SavePersistedState();
    }
}

void SmartModeController::SavePersistedState() const {
    uint64_t lastUpdateCheckMs = 0;
    const std::string existing = ReadTextFile(GetStatePath());
    if (!existing.empty()) {
        if (const auto value = JsonGetNumber(existing, "LastUpdateCheckMs")) {
            lastUpdateCheckMs = static_cast<uint64_t>(*value);
        }
    }

    std::string json = std::string(R"({"SmartMode":)")
        + (mode_ == ControlMode::Smart ? "true" : "false")
        + R"(,"ScheduleMode":)"
        + (mode_ == ControlMode::Schedule ? "true" : "false")
        + R"(,"LockOffEnabled":)"
        + (lockOffEnabled_ ? "true" : "false");
    if (lastUpdateCheckMs > 0) {
        json += R"(,"LastUpdateCheckMs":)" + std::to_string(lastUpdateCheckMs);
    }
    json += "}";
    WriteTextFile(GetStatePath(), json);
}

void SmartModeController::SetLockOffEnabled(bool enabled) {
    if (lockOffEnabled_ == enabled) {
        return;
    }
    lockOffEnabled_ = enabled;
    needsMouseAfterUnlock_ = false;
    SavePersistedState();
    if (mode_ != ControlMode::Manual) {
        hasApplied_ = false;
        Evaluate();
    }
}

void SmartModeController::ToggleLockOffEnabled() {
    SetLockOffEnabled(!lockOffEnabled_);
}

bool SmartModeController::EnsureLocation(std::string& error, bool promptForLocation) {
    if (!locationService_) {
        error = "Location service unavailable";
        return false;
    }

    if (hasLocation_ && !LocationIsStale(locationResolvedAtMs_)) {
        return true;
    }

    GeoLocation resolved{};
    if (promptForLocation) {
        const LocationPromptResult result = locationService_->ResolveWithPrompt(config_, resolved, error);
        if (result == LocationPromptResult::Success) {
            location_ = resolved;
            hasLocation_ = true;
            locationResolvedAtMs_ = MonotonicTimeMs();
            return true;
        }
        hasLocation_ = false;
        return false;
    }

    if (!locationService_->TryResolve(config_, resolved, error)) {
        hasLocation_ = false;
        return false;
    }

    location_ = resolved;
    hasLocation_ = true;
    locationResolvedAtMs_ = MonotonicTimeMs();
    return true;
}

bool SmartModeController::StartActivityTracking(std::string& error) {
    if (!activity_) {
        error = "Activity tracker unavailable";
        return false;
    }

    needsMouseAfterUnlock_ = false;
    activity_->ClearMouseMovedFlag();
    if (!activity_->Start()) {
        error = "Could not start activity tracking";
        return false;
    }
    return true;
}

bool SmartModeController::Enable(std::string& error, bool promptForLocation) {
    if (!EnsureLocation(error, promptForLocation)) {
        return false;
    }

    if (mode_ == ControlMode::Schedule && activity_) {
        activity_->Stop();
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

bool SmartModeController::EnableSchedule(std::string& error) {
    int onMinutes = 0;
    int offMinutes = 0;
    if (!ParseTimeHHMM(config_.scheduleOnTime, onMinutes)
        || !ParseTimeHHMM(config_.scheduleOffTime, offMinutes)) {
        error = "Schedule times are missing or invalid. Open Settings.";
        return false;
    }

    if (mode_ == ControlMode::Smart && activity_) {
        activity_->Stop();
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

void SmartModeController::Disable() {
    if (mode_ != ControlMode::Manual && activity_) {
        activity_->Stop();
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
    if (activity_) {
        activity_->SetLockOffSeconds(config_.lockOffSeconds);
    }
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

    if (lockOffEnabled_) {
        if (activity_ && activity_->IsSessionLocked() && activity_->IsLockOffDue()) {
            return false;
        }

        if (activity_ && activity_->IsSessionLocked() && !activity_->IsLockOffDue()) {
            return ShouldBeOnForSchedule(onMinutes, offMinutes, GetLocalMinutesNow());
        }

        if (needsMouseAfterUnlock_ && activity_ && !activity_->HasMouseMovedSinceUnlock()) {
            return false;
        }
    }

    return ShouldBeOnForSchedule(onMinutes, offMinutes, GetLocalMinutesNow());
}

bool SmartModeController::ShouldBeOn() const {
    if (mode_ == ControlMode::Schedule) {
        return ShouldBeOnSchedule();
    }

    if (!hasLocation_) {
        return false;
    }

    if (lockOffEnabled_) {
        if (activity_ && activity_->IsSessionLocked() && activity_->IsLockOffDue()) {
            return false;
        }

        if (activity_ && activity_->IsSessionLocked() && !activity_->IsLockOffDue()) {
            return IsDark(location_.latitude, location_.longitude, config_);
        }

        if (needsMouseAfterUnlock_ && activity_ && !activity_->HasMouseMovedSinceUnlock()) {
            return false;
        }
    }

    return IsDark(location_.latitude, location_.longitude, config_);
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
        std::string error;
        EnsureLocation(error, false);
    }

    ApplyDesiredState(ShouldBeOn());
}

void SmartModeController::OnSessionUnlock() {
    if (mode_ == ControlMode::Manual) {
        return;
    }

    needsMouseAfterUnlock_ = true;
    if (activity_) {
        activity_->ClearMouseMovedFlag();
    }
}

void SmartModeController::OnSessionLock() {
    if (mode_ == ControlMode::Manual) {
        return;
    }
    Evaluate();
}

void SmartModeController::OnSessionChangeEvent(bool isUnlock) {
    if (mode_ == ControlMode::Manual || !activity_) {
        return;
    }

    if (isUnlock) {
        const bool requireMouse = lockOffEnabled_ && activity_->IsLockOffDue();
        activity_->OnSessionUnlock();
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

    activity_->OnSessionLock();
    OnSessionLock();
}

void SmartModeController::OnLockTimerTick() {
    if (mode_ == ControlMode::Manual || !activity_) {
        return;
    }

    const SessionTransition transition = activity_->HandleLockTimerTick();

    if (transition == SessionTransition::Locked) {
        Evaluate();
    } else if (transition == SessionTransition::Unlocked) {
        if (lockOffEnabled_ && activity_->UnlockRequiresMouse()) {
            OnSessionUnlock();
        } else {
            activity_->ClearUnlockRequiresMouse();
        }
        if (powerOffHold_) {
            OnPowerResume();
        } else {
            Evaluate();
        }
    }

    if (lockOffEnabled_ && needsMouseAfterUnlock_ && activity_->HasMouseMovedSinceUnlock()) {
        needsMouseAfterUnlock_ = false;
        Evaluate();
    }
}

void SmartModeController::OnLockOffDue() {
    if (mode_ == ControlMode::Manual || powerOffHold_ || !lockOffEnabled_) {
        return;
    }

    ApplyDesiredState(false);
}

void SmartModeController::OnPowerSuspend() {
    if (mode_ == ControlMode::Manual || !client_ || !lockOffEnabled_) {
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
