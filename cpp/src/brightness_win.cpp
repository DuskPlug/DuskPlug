#include "brightness.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <vector>

namespace {

struct PhysicalMonitor {
    HANDLE handle = nullptr;
};

using GetNumberOfPhysicalMonitorsFromHMONITORFn = BOOL(WINAPI*)(HMONITOR, LPDWORD);
using GetPhysicalMonitorsFromHMONITORFn = BOOL(WINAPI*)(HMONITOR, DWORD, void*);
using DestroyPhysicalMonitorsFn = BOOL(WINAPI*)(DWORD, void*);
using GetMonitorBrightnessFn = BOOL(WINAPI*)(HANDLE, LPDWORD, LPDWORD, LPDWORD);
using SetMonitorBrightnessFn = BOOL(WINAPI*)(HANDLE, DWORD);

struct Dxva2Api {
    HMODULE module = nullptr;
    GetNumberOfPhysicalMonitorsFromHMONITORFn getCount = nullptr;
    GetPhysicalMonitorsFromHMONITORFn getMonitors = nullptr;
    DestroyPhysicalMonitorsFn destroyMonitors = nullptr;
    GetMonitorBrightnessFn getBrightness = nullptr;
    SetMonitorBrightnessFn setBrightness = nullptr;

    bool Load() {
        if (module) {
            return getCount && getMonitors && destroyMonitors && getBrightness && setBrightness;
        }

        module = LoadLibraryW(L"dxva2.dll");
        if (!module) {
            return false;
        }

        getCount = reinterpret_cast<GetNumberOfPhysicalMonitorsFromHMONITORFn>(
            GetProcAddress(module, "GetNumberOfPhysicalMonitorsFromHMONITOR"));
        getMonitors = reinterpret_cast<GetPhysicalMonitorsFromHMONITORFn>(
            GetProcAddress(module, "GetPhysicalMonitorsFromHMONITOR"));
        destroyMonitors = reinterpret_cast<DestroyPhysicalMonitorsFn>(
            GetProcAddress(module, "DestroyPhysicalMonitors"));
        getBrightness = reinterpret_cast<GetMonitorBrightnessFn>(GetProcAddress(module, "GetMonitorBrightness"));
        setBrightness = reinterpret_cast<SetMonitorBrightnessFn>(GetProcAddress(module, "SetMonitorBrightness"));

        return getCount && getMonitors && destroyMonitors && getBrightness && setBrightness;
    }

    ~Dxva2Api() {
        if (module) {
            FreeLibrary(module);
        }
    }
};

struct WinPhysicalMonitor {
    HANDLE handle = nullptr;
    WCHAR description[128]{};
};

class WinBrightnessController : public IBrightnessController {
public:
    bool AnyControllable() const override {
        return !monitors_.empty();
    }

    void Capture() override {
        if (captured_) {
            return;
        }

        EnsureMonitors();
        capturedValues_.clear();
        capturedValues_.reserve(monitors_.size());

        for (const auto& monitor : monitors_) {
            DWORD minValue = 0;
            DWORD currentValue = 0;
            DWORD maxValue = 0;
            if (api_.getBrightness(monitor.handle, &minValue, &currentValue, &maxValue)) {
                capturedValues_.push_back(currentValue);
            } else {
                capturedValues_.push_back(0);
            }
        }

        captured_ = true;
        lastPercent_ = -1;
    }

    void Restore() override {
        if (!captured_) {
            return;
        }

        EnsureMonitors();
        for (size_t i = 0; i < monitors_.size() && i < capturedValues_.size(); ++i) {
            api_.setBrightness(monitors_[i].handle, capturedValues_[i]);
        }

        captured_ = false;
        lastPercent_ = -1;
        capturedValues_.clear();
        ReleaseMonitors();
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

        EnsureMonitors();
        bool anySet = false;
        for (const auto& monitor : monitors_) {
            DWORD minValue = 0;
            DWORD currentValue = 0;
            DWORD maxValue = 0;
            if (!api_.getBrightness(monitor.handle, &minValue, &currentValue, &maxValue)) {
                continue;
            }

            const DWORD target = minValue
                + static_cast<DWORD>((maxValue - minValue) * static_cast<double>(percent) / 100.0 + 0.5);
            if (api_.setBrightness(monitor.handle, target)) {
                anySet = true;
            }
        }

        if (anySet) {
            lastPercent_ = percent;
        }
        return anySet;
    }

private:
    void EnsureMonitors() {
        if (!api_.Load()) {
            return;
        }

        if (!monitors_.empty()) {
            return;
        }

        EnumDisplayMonitors(
            nullptr,
            nullptr,
            [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
                auto* self = reinterpret_cast<WinBrightnessController*>(data);
                DWORD count = 0;
                if (!self->api_.getCount(monitor, &count) || count == 0) {
                    return TRUE;
                }

                std::vector<WinPhysicalMonitor> physical(count);
                if (!self->api_.getMonitors(monitor, count, physical.data())) {
                    return TRUE;
                }

                for (DWORD i = 0; i < count; ++i) {
                    PhysicalMonitor entry{};
                    entry.handle = physical[i].handle;
                    self->monitors_.push_back(entry);
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(this));
    }

    void ReleaseMonitors() {
        if (monitors_.empty() || !api_.destroyMonitors) {
            monitors_.clear();
            return;
        }

        std::vector<WinPhysicalMonitor> physical;
        physical.reserve(monitors_.size());
        for (const auto& monitor : monitors_) {
            WinPhysicalMonitor entry{};
            entry.handle = monitor.handle;
            physical.push_back(entry);
        }

        api_.destroyMonitors(static_cast<DWORD>(physical.size()), physical.data());
        monitors_.clear();
    }

    ~WinBrightnessController() override {
        ReleaseMonitors();
    }

    Dxva2Api api_;
    std::vector<PhysicalMonitor> monitors_;
    std::vector<DWORD> capturedValues_;
    bool captured_ = false;
    int lastPercent_ = -1;
};

}  // namespace

IBrightnessController* CreateWinBrightnessController() {
    return new WinBrightnessController();
}
