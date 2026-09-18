#include "smart_mode.h"

#include "json_util.h"
#include "platform_util.h"
#include "schedule.h"
#include "solar.h"

#include <chrono>
#include <cmath>
#include <ctime>

namespace {

struct LocalNow {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
};

LocalNow GetLocalNow() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {
        local.tm_year + 1900,
        local.tm_mon + 1,
        local.tm_mday,
        local.tm_hour,
        local.tm_min,
    };
}

bool LocationIsStale(uint64_t resolvedAtMs) {
    if (resolvedAtMs == 0) {
        return true;
    }
    return MonotonicTimeMs() - resolvedAtMs > 24ULL * 60 * 60 * 1000;
}

bool DeviceUsesSmart(const DeviceConfig& device) {
    return device.enabled && device.automation.mode == DeviceAutomationMode::Smart;
}

bool DeviceUsesSchedule(const DeviceConfig& device) {
    return device.enabled && device.automation.mode == DeviceAutomationMode::Schedule;
}

bool DeviceUsesTimed(const DeviceConfig& device) {
    return device.enabled && device.automation.mode == DeviceAutomationMode::Timed;
}

bool DeviceIsAutomated(const DeviceConfig& device) {
    return DeviceUsesSmart(device) || DeviceUsesSchedule(device);
}

}  // namespace

void SmartModeController::Initialize(
    const AppConfig& config,
    TuyaClient* client,
    ILocationService* locationService,
    IActivityTracker* activityTracker,
    IBrightnessController* brightnessController,
    SmartModeCallbacks callbacks) {
    config_ = config;
    client_ = client;
    locationService_ = locationService;
    activity_ = activityTracker;
    brightness_ = brightnessController;
    callbacks_ = std::move(callbacks);

    if (activity_) {
        activity_->SetLockOffSeconds(config_.lockOffSeconds);
        activity_->SetLockOffCallback([this]() { OnLockOffDue(); });
        activity_->SetLockActivityCallback([this]() { Evaluate(); });
    }
}

void SmartModeController::Shutdown() {
    ReleaseBrightness();
    if (activityStarted_ && activity_) {
        activity_->Stop();
        activityStarted_ = false;
    }
}

bool SmartModeController::HasAutomatedDevices() const {
    for (const auto& device : config_.devices) {
        if (DeviceIsAutomated(device)) {
            return true;
        }
    }
    return false;
}

DeviceRuntimeState& SmartModeController::RuntimeFor(const std::string& deviceId) {
    return runtimeByDeviceId_[deviceId];
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

    bool screenBrightnessEnabled = false;
    if (JsonGetBool(json, "ScreenBrightnessEnabled", screenBrightnessEnabled)) {
        screenBrightnessEnabled_ = screenBrightnessEnabled;
    }

    if (HasAutomatedDevices()) {
        std::string error;
        if (!StartActivityTracking(error) && callbacks_.showSetupMessage && !setupErrorShown_) {
            callbacks_.showSetupMessage(error);
            setupErrorShown_ = true;
        }
        Evaluate();
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

    std::string json = std::string(R"({"LockOffEnabled":)")
        + (lockOffEnabled_ ? "true" : "false")
        + R"(,"ScreenBrightnessEnabled":)"
        + (screenBrightnessEnabled_ ? "true" : "false");
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
    if (HasAutomatedDevices()) {
        for (auto& entry : runtimeByDeviceId_) {
            entry.second.hasApplied = false;
        }
        Evaluate();
    }
}

void SmartModeController::ToggleLockOffEnabled() {
    SetLockOffEnabled(!lockOffEnabled_);
}

void SmartModeController::SetScreenBrightnessEnabled(bool enabled) {
    if (screenBrightnessEnabled_ == enabled) {
        return;
    }

    screenBrightnessEnabled_ = enabled;
    brightnessUnavailableNotified_ = false;
    SavePersistedState();

    if (!enabled) {
        ReleaseBrightness();
        return;
    }

    hasAppliedBrightness_ = false;
    lastAppliedBrightnessPercent_ = -1;
    ApplyBrightness();
    Evaluate();
}

void SmartModeController::ToggleScreenBrightnessEnabled() {
    SetScreenBrightnessEnabled(!screenBrightnessEnabled_);
}

bool SmartModeController::IsScreenBrightnessAvailable() const {
    return brightness_ != nullptr && brightness_->AnyControllable();
}

int SmartModeController::GetScreenBrightnessPercent() const {
    const int hardware = ReadHardwareBrightnessPercent();
    if (screenBrightnessEnabled_) {
        const int target = ComputeScreenBrightnessTarget();
        if (BrightnessDriftedFromTarget(target, hardware)) {
            return hardware;
        }
        return target;
    }

    const int lastApplied = (hasAppliedBrightness_ && lastAppliedBrightnessPercent_ >= 0)
        ? lastAppliedBrightnessPercent_
        : -1;
    const int automaticTarget = 50;
    return ResolveDisplayedScreenBrightnessPercent(
        lastApplied,
        hardware,
        false,
        automaticTarget);
}

int SmartModeController::ReadHardwareBrightnessPercent() const {
    return brightness_ ? brightness_->GetCurrentPercent() : -1;
}

void SmartModeController::EnsureLocationForBrightness() {
    if (!screenBrightnessEnabled_) {
        return;
    }

    if (config_.hasLatitude && config_.hasLongitude) {
        return;
    }

    if (!locationService_) {
        return;
    }

    if (hasLocation_ && !LocationIsStale(locationResolvedAtMs_)) {
        return;
    }

    std::string error;
    EnsureLocation(error, false);
}

void SmartModeController::SetScreenBrightnessPercent(int percent) {
    if (!brightness_) {
        return;
    }

    brightnessUnavailableNotified_ = false;

    if (!brightnessCaptured_) {
        brightness_->Capture();
        brightnessCaptured_ = true;
    }

    BeginBrightnessTransition(ClampScreenBrightnessPercent(percent));
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

    if (activityStarted_) {
        return true;
    }

    needsMouseAfterUnlock_ = false;
    activity_->ClearMouseMovedFlag();
    if (!activity_->Start()) {
        error = "Could not start activity tracking";
        return false;
    }
    activityStarted_ = true;
    return true;
}

bool SmartModeController::ShouldBeOnForDevice(const DeviceConfig& device) const {
    if (DeviceUsesTimed(device)) {
        return IsTimedModeActive();
    }

    if (DeviceUsesSchedule(device)) {
        int onMinutes = 0;
        int offMinutes = 0;
        if (!ParseTimeHHMM(device.automation.scheduleOnTime, onMinutes)
            || !ParseTimeHHMM(device.automation.scheduleOffTime, offMinutes)) {
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

    if (!DeviceUsesSmart(device)) {
        return false;
    }

    double latitude = config_.latitude;
    double longitude = config_.longitude;
    if (hasLocation_) {
        latitude = location_.latitude;
        longitude = location_.longitude;
    } else if (!config_.hasLatitude || !config_.hasLongitude) {
        return false;
    }

    if (lockOffEnabled_) {
        if (activity_ && activity_->IsSessionLocked() && activity_->IsLockOffDue()) {
            return false;
        }

        if (activity_ && activity_->IsSessionLocked() && !activity_->IsLockOffDue()) {
            return IsDarkWithOffsets(
                latitude,
                longitude,
                device.automation.darkOffsetMinutes,
                device.automation.lightOffsetMinutes);
        }

        if (needsMouseAfterUnlock_ && activity_ && !activity_->HasMouseMovedSinceUnlock()) {
            return false;
        }
    }

    return IsDarkWithOffsets(
        latitude,
        longitude,
        device.automation.darkOffsetMinutes,
        device.automation.lightOffsetMinutes);
}

bool SmartModeController::IsNightForDevice(const DeviceConfig& device) const {
    if (DeviceUsesSchedule(device)) {
        int onMinutes = 0;
        int offMinutes = 0;
        if (ParseTimeHHMM(device.automation.scheduleOnTime, onMinutes)
            && ParseTimeHHMM(device.automation.scheduleOffTime, offMinutes)) {
            return ShouldBeOnForSchedule(onMinutes, offMinutes, GetLocalMinutesNow());
        }
        return false;
    }

    double latitude = config_.latitude;
    double longitude = config_.longitude;
    if (hasLocation_) {
        latitude = location_.latitude;
        longitude = location_.longitude;
    } else if (!config_.hasLatitude || !config_.hasLongitude) {
        return false;
    }

    return IsDarkWithOffsets(
        latitude,
        longitude,
        device.automation.darkOffsetMinutes,
        device.automation.lightOffsetMinutes);
}

SmartModeController::DesiredDeviceState SmartModeController::ComputeDesiredState(const DeviceConfig& device) const {
    DesiredDeviceState desired{};
    desired.on = ShouldBeOnForDevice(device);

    if (device.type == DeviceType::Bulb
        && device.automation.useBrightness
        && !device.capabilities.brightnessCode.empty()
        && desired.on) {
        desired.applyBrightness = true;
        desired.brightnessPercent = IsNightForDevice(device)
            ? device.automation.nightBrightness
            : device.automation.dayBrightness;
    }

    return desired;
}

bool SmartModeController::ShouldUseNightBrightness() const {
    for (const auto& device : config_.devices) {
        if (DeviceUsesSchedule(device)) {
            int onMinutes = 0;
            int offMinutes = 0;
            if (ParseTimeHHMM(device.automation.scheduleOnTime, onMinutes)
                && ParseTimeHHMM(device.automation.scheduleOffTime, offMinutes)) {
                if (ShouldBeOnForSchedule(onMinutes, offMinutes, GetLocalMinutesNow())) {
                    return true;
                }
            }
        }
    }

    if (config_.hasLatitude && config_.hasLongitude) {
        return IsDark(config_.latitude, config_.longitude, config_);
    }

    if (hasLocation_) {
        return IsDark(location_.latitude, location_.longitude, config_);
    }

    return false;
}

int SmartModeController::ComputeScreenBrightnessTarget() const {
    for (const auto& device : config_.devices) {
        if (DeviceUsesSchedule(device)) {
            int onMinutes = 0;
            int offMinutes = 0;
            if (ParseTimeHHMM(device.automation.scheduleOnTime, onMinutes)
                && ParseTimeHHMM(device.automation.scheduleOffTime, offMinutes)) {
                if (ShouldBeOnForSchedule(onMinutes, offMinutes, GetLocalMinutesNow())) {
                    return config_.screenBrightnessNight;
                }
            }
        }
    }

    double latitude = 0.0;
    double longitude = 0.0;
    bool hasCoords = false;
    if (config_.hasLatitude && config_.hasLongitude) {
        latitude = config_.latitude;
        longitude = config_.longitude;
        hasCoords = true;
    } else if (hasLocation_) {
        latitude = location_.latitude;
        longitude = location_.longitude;
        hasCoords = true;
    }

    if (hasCoords && config_.windowAzimuthDegrees >= 0) {
        const LocalNow now = GetLocalNow();
        const SunPosition sun = ComputeSunPosition(
            latitude,
            longitude,
            now.year,
            now.month,
            now.day,
            now.hour,
            now.minute,
            GetCurrentUtcOffsetMinutes());
        const double factor = WindowSunExposure(
            sun.azimuthDegrees,
            sun.elevationDegrees,
            static_cast<double>(config_.windowAzimuthDegrees),
            config_.windowGlareWeight);
        const int night = config_.screenBrightnessNight;
        const int day = config_.screenBrightnessDay;
        const int target = night + static_cast<int>(std::lround(static_cast<double>(day - night) * factor));
        return ClampScreenBrightnessPercent(target);
    }

    return ShouldUseNightBrightness() ? config_.screenBrightnessNight : config_.screenBrightnessDay;
}

void SmartModeController::NotifyScreenBrightnessChanged() {
    if (callbacks_.onScreenBrightnessChanged) {
        callbacks_.onScreenBrightnessChanged();
    }
}

int SmartModeController::CurrentAppliedBrightnessPercent() const {
    if (hasAppliedBrightness_ && lastAppliedBrightnessPercent_ >= 0) {
        return lastAppliedBrightnessPercent_;
    }
    if (brightness_) {
        const int hardware = brightness_->GetCurrentPercent();
        if (hardware >= 0) {
            return hardware;
        }
    }
    return -1;
}

void SmartModeController::StopBrightnessFade() {
    if (!brightnessFadeActive_) {
        return;
    }

    brightnessFadeActive_ = false;
    if (callbacks_.onBrightnessFadeFinished) {
        callbacks_.onBrightnessFadeFinished();
    }
}

void SmartModeController::ApplyBrightnessImmediate(int percent, bool notifyUi) {
    if (!brightness_) {
        return;
    }

    percent = ClampScreenBrightnessPercent(percent);
    if (hasAppliedBrightness_
        && lastAppliedBrightnessPercent_ >= 0
        && lastAppliedBrightnessPercent_ == percent) {
        const int hardware = ReadHardwareBrightnessPercent();
        if (!screenBrightnessEnabled_ || !BrightnessDriftedFromTarget(percent, hardware)) {
            return;
        }
    }

    if (brightness_->SetPercent(percent)) {
        hasAppliedBrightness_ = true;
        lastAppliedBrightnessPercent_ = percent;
        if (notifyUi) {
            NotifyScreenBrightnessChanged();
        }
        return;
    }

    if (!brightnessUnavailableNotified_ && callbacks_.showSetupMessage) {
        brightnessUnavailableNotified_ = true;
        callbacks_.showSetupMessage(
            "Could not change screen brightness. On laptops, check that no other app is "
            "controlling brightness. On external monitors, enable DDC/CI in the monitor menu.");
    }
}

void SmartModeController::BeginBrightnessTransition(int target) {
    if (!brightness_) {
        return;
    }

    target = ClampScreenBrightnessPercent(target);
    const int hardware = ReadHardwareBrightnessPercent();
    if (brightnessFadeActive_ && brightnessFadeTo_ == target
        && !BrightnessDriftedFromTarget(target, hardware)) {
        return;
    }
    if (brightnessFadeActive_ && brightnessFadeTo_ == target
        && BrightnessDriftedFromTarget(target, hardware)) {
        StopBrightnessFade();
    }

    int current = CurrentAppliedBrightnessPercent();
    if (screenBrightnessEnabled_ && hardware >= 0) {
        current = hardware;
    } else if (current < 0) {
        current = target;
    }

    const uint64_t durationMs = BrightnessFadeDurationMs(current, target);
    if (durationMs == 0) {
        StopBrightnessFade();
        ApplyBrightnessImmediate(target);
        return;
    }

    const bool wasFading = brightnessFadeActive_;
    brightnessFadeFrom_ = current;
    brightnessFadeTo_ = target;
    brightnessFadeStartMs_ = MonotonicTimeMs();
    brightnessFadeDurationMs_ = durationMs;
    brightnessFadeActive_ = true;

    if (!wasFading && callbacks_.onBrightnessFadeStarted) {
        callbacks_.onBrightnessFadeStarted();
    }

    OnBrightnessFadeTick();
}

void SmartModeController::OnBrightnessFadeTick() {
    if (!brightnessFadeActive_ || !brightness_) {
        return;
    }

    const uint64_t elapsed = MonotonicTimeMs() - brightnessFadeStartMs_;
    const double progress = brightnessFadeDurationMs_ == 0
        ? 1.0
        : static_cast<double>(elapsed) / static_cast<double>(brightnessFadeDurationMs_);
    const int percent = InterpolateBrightnessPercent(brightnessFadeFrom_, brightnessFadeTo_, progress);
    const bool notifyUi = screenBrightnessEnabled_ || progress >= 1.0;
    ApplyBrightnessImmediate(percent, notifyUi);

    if (progress >= 1.0) {
        ApplyBrightnessImmediate(brightnessFadeTo_, true);
        StopBrightnessFade();
    }
}

void SmartModeController::ReleaseBrightness() {
    StopBrightnessFade();
    if (brightnessCaptured_ && brightness_) {
        brightness_->Restore();
        brightnessCaptured_ = false;
        hasAppliedBrightness_ = false;
        lastAppliedBrightnessPercent_ = -1;
        NotifyScreenBrightnessChanged();
    }
}

void SmartModeController::ApplyBrightness() {
    if (!screenBrightnessEnabled_ || !brightness_) {
        return;
    }

    if (!brightness_->AnyControllable()) {
        if (!brightnessUnavailableNotified_ && callbacks_.showSetupMessage) {
            brightnessUnavailableNotified_ = true;
            callbacks_.showSetupMessage(
                "Screen brightness could not be adjusted on this system. "
                "Built-in laptop panels and DDC/CI-enabled external monitors are supported.");
        }
        return;
    }

    if (!brightnessCaptured_) {
        brightness_->Capture();
        brightnessCaptured_ = true;
    }

    EnsureLocationForBrightness();
    BeginBrightnessTransition(ComputeScreenBrightnessTarget());
}

void SmartModeController::ApplyDesiredState(const DeviceConfig& device, const DesiredDeviceState& desired) {
    if (!client_ || (callbacks_.isBusy && callbacks_.isBusy())) {
        return;
    }

    DeviceRuntimeState& runtime = RuntimeFor(device.id);
    const bool sameSwitch = runtime.hasApplied && runtime.lastAppliedOn == desired.on;
    const bool sameBrightness = !desired.applyBrightness
        || runtime.lastAppliedBrightness == desired.brightnessPercent;

    if (sameSwitch && sameBrightness) {
        UpdateTrayFromRuntimeState();
        return;
    }

    std::string error;
    if (desired.on) {
        if (!client_->SetSwitch(device, true, error)) {
            UpdateTrayFromRuntimeState();
            return;
        }
        if (desired.applyBrightness) {
            if (!client_->SetBrightness(device, desired.brightnessPercent, error)) {
                UpdateTrayFromRuntimeState();
                return;
            }
            runtime.lastAppliedBrightness = desired.brightnessPercent;
        }
    } else {
        if (!client_->SetSwitch(device, false, error)) {
            UpdateTrayFromRuntimeState();
            return;
        }
        runtime.lastAppliedBrightness = -1;
    }

    runtime.hasApplied = true;
    runtime.lastAppliedOn = desired.on;
    runtime.hasKnownState = true;
    runtime.knownOn = desired.on;
    UpdateTrayFromRuntimeState();
}

void SmartModeController::UpdateTrayFromRuntimeState() {
    if (!callbacks_.updateTray) {
        return;
    }

    size_t onCount = 0;
    size_t totalCount = 0;
    for (const auto& device : config_.devices) {
        if (!device.enabled || device.id.empty()) {
            continue;
        }
        ++totalCount;
        const auto it = runtimeByDeviceId_.find(device.id);
        if (it != runtimeByDeviceId_.end() && it->second.hasKnownState && it->second.knownOn) {
            ++onCount;
        }
    }

    callbacks_.updateTray(onCount > 0, onCount, totalCount);
}

void SmartModeController::Evaluate() {
    bool needsSmartLocation = false;
    for (const auto& device : config_.devices) {
        if (DeviceUsesSmart(device)) {
            needsSmartLocation = true;
            break;
        }
    }

    if (needsSmartLocation && LocationIsStale(locationResolvedAtMs_)) {
        std::string error;
        EnsureLocation(error, false);
    }

    EnsureLocationForBrightness();
    ApplyBrightness();

    if (!HasAutomatedDevices()) {
        UpdateTrayFromRuntimeState();
        return;
    }

    if (powerOffHold_) {
        return;
    }

    if (HasAutomatedDevices()) {
        std::string error;
        StartActivityTracking(error);
    }

    for (const auto& device : config_.devices) {
        if (!DeviceIsAutomated(device)) {
            continue;
        }
        ApplyDesiredState(device, ComputeDesiredState(device));
    }
}

void SmartModeController::SetKnownDeviceState(const std::string& deviceId, bool on) {
    DeviceRuntimeState& runtime = RuntimeFor(deviceId);
    runtime.hasKnownState = true;
    runtime.knownOn = on;
    UpdateTrayFromRuntimeState();
}

void SmartModeController::UpdateConfig(const AppConfig& config) {
    config_ = config;
    hasLocation_ = false;
    locationResolvedAtMs_ = 0;
    if (activity_) {
        activity_->SetLockOffSeconds(config_.lockOffSeconds);
    }
    if (screenBrightnessEnabled_) {
        hasAppliedBrightness_ = false;
        lastAppliedBrightnessPercent_ = -1;
    }
    for (auto& entry : runtimeByDeviceId_) {
        entry.second.hasApplied = false;
    }
    Evaluate();
}

void SmartModeController::UpdateClient(TuyaClient* client) {
    client_ = client;
}

bool SmartModeController::GetKnownAggregateOn() const {
    return GetKnownOnCount() > 0;
}

bool SmartModeController::HasKnownDeviceStates() const {
    for (const auto& entry : runtimeByDeviceId_) {
        if (entry.second.hasKnownState) {
            return true;
        }
    }
    return false;
}

size_t SmartModeController::GetKnownOnCount() const {
    size_t count = 0;
    for (const auto& entry : runtimeByDeviceId_) {
        if (entry.second.hasKnownState && entry.second.knownOn) {
            ++count;
        }
    }
    return count;
}

size_t SmartModeController::GetEnabledDeviceCount() const {
    return GetEnabledDevices(config_).size();
}

bool SmartModeController::HasDeviceKnownState(const std::string& deviceId) const {
    const auto it = runtimeByDeviceId_.find(deviceId);
    return it != runtimeByDeviceId_.end() && it->second.hasKnownState;
}

bool SmartModeController::GetDeviceKnownOn(const std::string& deviceId) const {
    const auto it = runtimeByDeviceId_.find(deviceId);
    if (it == runtimeByDeviceId_.end() || !it->second.hasKnownState) {
        return false;
    }
    return it->second.knownOn;
}

bool SmartModeController::SetDeviceSwitch(const DeviceConfig& device, bool on, std::string& error) {
    if (!client_) {
        error = "Tuya client unavailable";
        return false;
    }
    if (!client_->SetSwitch(device, on, error)) {
        return false;
    }
    DeviceRuntimeState& runtime = RuntimeFor(device.id);
    runtime.hasKnownState = true;
    runtime.knownOn = on;
    runtime.hasApplied = true;
    runtime.lastAppliedOn = on;
    UpdateTrayFromRuntimeState();
    return true;
}

void SmartModeController::StartTimedMode(int minutes) {
    activeTimedMinutes_ = ClampTimedDurationMinutes(minutes);
    timedExpiresAtMs_ = MonotonicTimeMs() + static_cast<uint64_t>(activeTimedMinutes_) * 60ULL * 1000ULL;

    for (const auto& device : config_.devices) {
        if (!device.enabled) {
            continue;
        }
        std::string error;
        SetDeviceSwitch(device, true, error);
    }
    UpdateTrayFromRuntimeState();
}

void SmartModeController::CancelTimedMode() {
    timedExpiresAtMs_ = 0;
    activeTimedMinutes_ = 0;
}

void SmartModeController::OnTimedModeTick() {
    if (!IsTimedModeExpired()) {
        return;
    }

    for (const auto& device : config_.devices) {
        if (!device.enabled) {
            continue;
        }
        std::string error;
        SetDeviceSwitch(device, false, error);
    }
    CancelTimedMode();
    UpdateTrayFromRuntimeState();

    if (callbacks_.onTimedModeExpired) {
        callbacks_.onTimedModeExpired();
    }
}

bool SmartModeController::IsTimedModeActive() const {
    return timedExpiresAtMs_ != 0 && MonotonicTimeMs() < timedExpiresAtMs_;
}

bool SmartModeController::IsTimedModeExpired() const {
    return timedExpiresAtMs_ != 0 && MonotonicTimeMs() >= timedExpiresAtMs_;
}

int SmartModeController::GetTimedDurationMinutes() const {
    return activeTimedMinutes_;
}

bool SmartModeController::ToggleDevice(const DeviceConfig& device, std::string& error, bool& newState) {
    if (!client_) {
        error = "Tuya client unavailable";
        return false;
    }
    if (!client_->Toggle(device, error, newState)) {
        return false;
    }
    DeviceRuntimeState& runtime = RuntimeFor(device.id);
    runtime.hasKnownState = true;
    runtime.knownOn = newState;
    runtime.hasApplied = true;
    runtime.lastAppliedOn = newState;
    UpdateTrayFromRuntimeState();
    return true;
}

void SmartModeController::OnSessionUnlock() {
    if (!HasAutomatedDevices()) {
        return;
    }

    needsMouseAfterUnlock_ = true;
    if (activity_) {
        activity_->ClearMouseMovedFlag();
    }
}

void SmartModeController::OnSessionLock() {
    if (!HasAutomatedDevices()) {
        return;
    }
    Evaluate();
}

void SmartModeController::OnSessionChangeEvent(bool isUnlock) {
    if (!HasAutomatedDevices() || !activity_) {
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
    if (!HasAutomatedDevices() || !activity_) {
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
    if (!HasAutomatedDevices() || powerOffHold_ || !lockOffEnabled_ || !client_) {
        return;
    }

    for (const auto& device : config_.devices) {
        if (!DeviceIsAutomated(device)) {
            continue;
        }
        DesiredDeviceState desired{};
        desired.on = false;
        ApplyDesiredState(device, desired);
    }
}

void SmartModeController::OnPowerSuspend() {
    if (!HasAutomatedDevices() || !client_ || !lockOffEnabled_) {
        return;
    }

    powerOffHold_ = true;
    constexpr unsigned long kSuspendTimeoutMs = 2500;
    const int delaySeconds = config_.lockOffSeconds > 0 ? config_.lockOffSeconds : 30;

    for (const auto& device : config_.devices) {
        if (!DeviceIsAutomated(device)) {
            continue;
        }

        DeviceRuntimeState& runtime = RuntimeFor(device.id);
        if (runtime.hasApplied && !runtime.lastAppliedOn) {
            continue;
        }

        std::string error;
        if (device.type == DeviceType::Plug) {
            if (client_->SetCountdownOff(device, delaySeconds, error, kSuspendTimeoutMs)
                || client_->SetSwitch(device, false, error, kSuspendTimeoutMs)) {
                runtime.hasApplied = true;
                runtime.lastAppliedOn = false;
                runtime.hasKnownState = true;
                runtime.knownOn = false;
            }
        } else if (client_->SetSwitch(device, false, error, kSuspendTimeoutMs)) {
            runtime.hasApplied = true;
            runtime.lastAppliedOn = false;
            runtime.hasKnownState = true;
            runtime.knownOn = false;
        }
    }

    UpdateTrayFromRuntimeState();
}

void SmartModeController::OnPowerResume() {
    if (!powerOffHold_) {
        return;
    }

    powerOffHold_ = false;
    for (auto& entry : runtimeByDeviceId_) {
        entry.second.hasApplied = false;
    }
    Evaluate();
}
