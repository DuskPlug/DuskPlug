#include "brightness.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <string>
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

std::string RunCommandCapture(const std::string& command) {
    std::string output;
    FILE* pipe = _popen(command.c_str(), "r");
    if (!pipe) {
        return output;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }
    _pclose(pipe);
    return output;
}

bool RunCommandOk(const std::string& command) {
    return std::system(command.c_str()) == 0;
}

class DdcBrightnessBackend {
public:
    bool AnyControllable() const {
        return !monitors_.empty();
    }

    void Capture(std::vector<DWORD>& outValues) {
        EnsureMonitors();
        for (const auto& monitor : monitors_) {
            DWORD minValue = 0;
            DWORD currentValue = 0;
            DWORD maxValue = 0;
            if (api_.getBrightness(monitor.handle, &minValue, &currentValue, &maxValue)) {
                outValues.push_back(currentValue);
            }
        }
    }

    void Restore(const std::vector<DWORD>& values) {
        EnsureMonitors();
        for (size_t i = 0; i < monitors_.size() && i < values.size(); ++i) {
            api_.setBrightness(monitors_[i].handle, values[i]);
        }
    }

    bool SetPercent(int percent) {
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
        return anySet;
    }

    void Reset() {
        ReleaseMonitors();
    }

private:
    void EnsureMonitors() {
        if (!api_.Load() || !monitors_.empty()) {
            return;
        }

        EnumDisplayMonitors(
            nullptr,
            nullptr,
            [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
                auto* self = reinterpret_cast<DdcBrightnessBackend*>(data);
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

    Dxva2Api api_;
    std::vector<PhysicalMonitor> monitors_;
};

class WmiBrightnessBackend {
public:
    bool AnyControllable() const {
        return EnsureAvailable();
    }

    void Capture(std::vector<DWORD>& outValues) {
        if (!EnsureAvailable()) {
            return;
        }

        const std::string output = RunCommandCapture(
            "powershell -NoProfile -NonInteractive -Command "
            "\"$b = Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightness -ErrorAction SilentlyContinue; "
            "if (-not $b) { $b = Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightness -ErrorAction SilentlyContinue }; "
            "if ($b) { $b | Select-Object -ExpandProperty CurrentBrightness }\"");
        size_t start = 0;
        while (start < output.size()) {
            const size_t end = output.find_first_of("\r\n", start);
            const std::string line = output.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!line.empty()) {
                outValues.push_back(static_cast<DWORD>(std::atoi(line.c_str())));
            }
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
            if (start < output.size() && output[start] == '\n') {
                ++start;
            }
        }
    }

    void Restore(const std::vector<DWORD>& values) {
        if (!EnsureAvailable() || values.empty()) {
            return;
        }

        for (const DWORD value : values) {
            const std::string command =
                "powershell -NoProfile -NonInteractive -Command "
                "\"$m = Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightnessMethods -ErrorAction SilentlyContinue; "
                "if (-not $m) { $m = Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods -ErrorAction SilentlyContinue }; "
                "if ($m) { $m | ForEach-Object { $_.WmiSetBrightness(1, "
                + std::to_string(value) + ") } }\" >nul 2>&1";
            RunCommandOk(command);
        }
    }

    bool SetPercent(int percent) {
        if (!EnsureAvailable()) {
            return false;
        }

        const std::string command =
            "powershell -NoProfile -NonInteractive -Command "
            "\"$m = Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightnessMethods -ErrorAction SilentlyContinue; "
            "if (-not $m) { $m = Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods -ErrorAction SilentlyContinue }; "
            "if ($m) { $m | ForEach-Object { $_.WmiSetBrightness(1, "
            + std::to_string(percent) + ") } }\" >nul 2>&1";
        return RunCommandOk(command);
    }

private:
    bool EnsureAvailable() const {
        if (probed_) {
            return available_;
        }

        probed_ = true;
        const std::string output = RunCommandCapture(
            "powershell -NoProfile -NonInteractive -Command "
            "\"$m = Get-CimInstance -Namespace root/WMI -ClassName WmiMonitorBrightnessMethods -ErrorAction SilentlyContinue; "
            "if (-not $m) { $m = Get-WmiObject -Namespace root/WMI -Class WmiMonitorBrightnessMethods -ErrorAction SilentlyContinue }; "
            "if ($m) { @($m).Count } else { 0 }\"");
        available_ = std::atoi(output.c_str()) > 0;
        return available_;
    }

    mutable bool probed_ = false;
    mutable bool available_ = false;
};

class WinBrightnessController : public IBrightnessController {
public:
    bool AnyControllable() const override {
        return ddc_.AnyControllable() || wmi_.AnyControllable();
    }

    void Capture() override {
        if (captured_) {
            return;
        }

        ddcCapturedValues_.clear();
        wmiCapturedValues_.clear();
        ddc_.Capture(ddcCapturedValues_);
        wmi_.Capture(wmiCapturedValues_);
        captured_ = true;
        lastPercent_ = -1;
    }

    void Restore() override {
        if (!captured_) {
            return;
        }

        ddc_.Restore(ddcCapturedValues_);
        wmi_.Restore(wmiCapturedValues_);
        captured_ = false;
        lastPercent_ = -1;
        ddcCapturedValues_.clear();
        wmiCapturedValues_.clear();
        ddc_.Reset();
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

        const bool ddcSet = ddc_.SetPercent(percent);
        const bool wmiSet = wmi_.SetPercent(percent);
        if (ddcSet || wmiSet) {
            lastPercent_ = percent;
        }
        return ddcSet || wmiSet;
    }

private:
    DdcBrightnessBackend ddc_;
    WmiBrightnessBackend wmi_;
    std::vector<DWORD> ddcCapturedValues_;
    std::vector<DWORD> wmiCapturedValues_;
    bool captured_ = false;
    int lastPercent_ = -1;
};

}  // namespace

IBrightnessController* CreateWinBrightnessController() {
    return new WinBrightnessController();
}
