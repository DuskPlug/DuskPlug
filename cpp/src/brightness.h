#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Slider/UI brightness: prefer the last value DuskPlug applied so automatic
// changes show immediately even if the hardware readout lags.
inline bool BrightnessDriftedFromTarget(int target, int hardware, int tolerance = 2) {
    if (hardware < 0) {
        return false;
    }
    return std::abs(hardware - target) > tolerance;
}

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

inline double SmoothStep(double progress) {
    const double t = std::clamp(progress, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

inline int InterpolateBrightnessPercent(int from, int to, double progress) {
    const double eased = SmoothStep(progress);
    return from + static_cast<int>(std::lround(static_cast<double>(to - from) * eased));
}

inline uint64_t BrightnessFadeDurationMs(int from, int to) {
    const int delta = std::abs(to - from);
    if (delta <= 1) {
        return 0;
    }
    const uint64_t duration = 250ULL + static_cast<uint64_t>(delta) * 12ULL;
    return duration > 1500ULL ? 1500ULL : duration;
}

// Maps stored glare weight (0..1, default 0.5 = linear) to a power-curve exponent.
// Lower weight = softer blend; higher = sharper (full day only when sun is square-on).
inline double WindowExposureCurveExponent(double glareWeight) {
    const double weight = std::clamp(glareWeight, 0.0, 1.0);
    return std::pow(2.0, (weight - 0.5) * 2.0);
}

inline double ApplyWindowExposureCurve(double intoWindow, double glareWeight) {
    const double t = std::clamp(intoWindow, 0.0, 1.0);
    if (t <= 0.0) {
        return 0.0;
    }
    if (t >= 1.0) {
        return 1.0;
    }
    return std::pow(t, WindowExposureCurveExponent(glareWeight));
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
