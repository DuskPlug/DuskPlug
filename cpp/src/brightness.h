#pragma once

// Slider/UI brightness: prefer the last value DuskPlug applied so automatic
// changes show immediately even if the hardware readout lags.
inline int ResolveDisplayedScreenBrightnessPercent(
    int lastAppliedPercent,
    int hardwarePercent,
    bool automaticEnabled,
    int automaticTargetPercent) {
    if (lastAppliedPercent >= 0) {
        return lastAppliedPercent;
    }
    if (hardwarePercent >= 0) {
        return hardwarePercent;
    }
    if (automaticEnabled) {
        return automaticTargetPercent;
    }
    return 50;
}

class IBrightnessController {
public:
    virtual ~IBrightnessController() = default;

    virtual bool AnyControllable() const = 0;
    virtual void Capture() = 0;
    virtual void Restore() = 0;
    virtual bool SetPercent(int percent) = 0;
    virtual int GetCurrentPercent() const { return -1; }
};
