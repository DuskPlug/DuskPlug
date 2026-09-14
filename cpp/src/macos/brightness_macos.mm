#include "../brightness.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/graphics/IOGraphicsLib.h>

#include <vector>

namespace {

struct MacDisplaySnapshot {
    io_service_t service = 0;
    float brightness = 1.0f;
};

class MacBrightnessController : public IBrightnessController {
public:
    ~MacBrightnessController() override {
        ReleaseServices();
    }

    bool AnyControllable() const override {
        return !services_.empty();
    }

    void Capture() override {
        if (captured_) {
            return;
        }

        ReleaseServices();
        DiscoverServices();
        snapshots_.clear();
        snapshots_.reserve(services_.size());

        for (const io_service_t service : services_) {
            float brightness = 1.0f;
            if (IODisplayGetFloatParameter(service, kNilOptions, CFSTR(kIODisplayBrightnessKey), &brightness)
                != kIOReturnSuccess) {
                continue;
            }

            MacDisplaySnapshot snapshot{};
            snapshot.service = service;
            snapshot.brightness = brightness;
            snapshots_.push_back(snapshot);
        }

        captured_ = true;
        lastPercent_ = -1;
    }

    void Restore() override {
        if (!captured_) {
            return;
        }

        for (const auto& snapshot : snapshots_) {
            IODisplaySetFloatParameter(
                snapshot.service,
                kNilOptions,
                CFSTR(kIODisplayBrightnessKey),
                snapshot.brightness);
        }

        captured_ = false;
        lastPercent_ = -1;
        snapshots_.clear();
        ReleaseServices();
    }

    bool SetPercent(int percent) override {
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }

        if (lastPercent_ == percent) {
            return AnyControllable();
        }

        if (services_.empty()) {
            DiscoverServices();
        }

        const float target = static_cast<float>(percent) / 100.0f;
        bool anySet = false;
        for (const io_service_t service : services_) {
            if (IODisplaySetFloatParameter(service, kNilOptions, CFSTR(kIODisplayBrightnessKey), target)
                == kIOReturnSuccess) {
                anySet = true;
            }
        }

        if (anySet) {
            lastPercent_ = percent;
        }
        return anySet;
    }

private:
    void DiscoverServices() {
        io_iterator_t iterator = 0;
        if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IODisplayConnect"), &iterator)
            != kIOReturnSuccess) {
            return;
        }

        io_service_t service = 0;
        while ((service = IOIteratorNext(iterator)) != 0) {
            services_.push_back(service);
        }
        IOObjectRelease(iterator);
    }

    void ReleaseServices() {
        for (const io_service_t service : services_) {
            IOObjectRelease(service);
        }
        services_.clear();
    }

    std::vector<io_service_t> services_;
    std::vector<MacDisplaySnapshot> snapshots_;
    bool captured_ = false;
    int lastPercent_ = -1;
};

}  // namespace

IBrightnessController* CreateMacBrightnessController() {
    return new MacBrightnessController();
}
