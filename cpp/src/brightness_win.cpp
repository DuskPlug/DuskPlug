#include "brightness.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <Wbemidl.h>
#include <powrprof.h>

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "PowrProf.lib")

#include <algorithm>
#include <string>
#include <vector>

namespace {

struct WideBstr {
    BSTR value = nullptr;
    explicit WideBstr(const wchar_t* text) : value(SysAllocString(text)) {}
    ~WideBstr() {
        if (value) {
            SysFreeString(value);
        }
    }
    operator BSTR() const { return value; }
};

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

class ComScope {
public:
    ComScope() {
        hr_ = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        owns_ = hr_ == S_OK;
    }

    ~ComScope() {
        if (owns_) {
            CoUninitialize();
        }
    }

    bool Ok() const {
        return hr_ == S_OK || hr_ == S_FALSE;
    }

private:
    HRESULT hr_ = CO_E_NOTINITIALIZED;
    bool owns_ = false;
};

class DdcBrightnessBackend {
public:
    bool AnyControllable() const {
        EnsureMonitors();
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

    int GetAveragePercent() const {
        EnsureMonitors();
        int total = 0;
        int count = 0;
        for (const auto& monitor : monitors_) {
            DWORD minValue = 0;
            DWORD currentValue = 0;
            DWORD maxValue = 0;
            if (!api_.getBrightness(monitor.handle, &minValue, &currentValue, &maxValue)) {
                continue;
            }
            if (maxValue <= minValue) {
                continue;
            }
            total += static_cast<int>((currentValue - minValue) * 100.0 / (maxValue - minValue) + 0.5);
            ++count;
        }
        return count > 0 ? total / count : -1;
    }

    void Reset() {
        ReleaseMonitors();
    }

private:
    void EnsureMonitors() const {
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

    void ReleaseMonitors() const {
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

    mutable Dxva2Api api_;
    mutable std::vector<PhysicalMonitor> monitors_;
};

class WmiComBrightnessBackend {
public:
    ~WmiComBrightnessBackend() {
        Disconnect();
    }

    bool AnyControllable() const {
        return EnsureReady();
    }

    void Capture(std::vector<DWORD>& outValues) {
        if (!EnsureReady()) {
            return;
        }

        IEnumWbemClassObject* enumerator = nullptr;
        const HRESULT hr = services_->ExecQuery(
            WideBstr(L"WQL"),
            WideBstr(L"SELECT CurrentBrightness FROM WmiMonitorBrightness"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &enumerator);
        if (FAILED(hr) || !enumerator) {
            return;
        }

        IWbemClassObject* object = nullptr;
        ULONG returned = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &object, &returned) == S_OK && object) {
            VARIANT value{};
            VariantInit(&value);
            if (SUCCEEDED(object->Get(L"CurrentBrightness", 0, &value, nullptr, nullptr))
                && value.vt == VT_I4) {
                outValues.push_back(static_cast<DWORD>(value.lVal));
            }
            VariantClear(&value);
            object->Release();
            object = nullptr;
        }
        enumerator->Release();
    }

    void Restore(const std::vector<DWORD>& values) {
        for (const DWORD value : values) {
            SetSinglePercent(static_cast<int>(value));
        }
    }

    bool SetPercent(int percent) {
        return SetSinglePercent(percent);
    }

    int GetAveragePercent() const {
        if (!EnsureReady()) {
            return -1;
        }

        IEnumWbemClassObject* enumerator = nullptr;
        const HRESULT hr = services_->ExecQuery(
            WideBstr(L"WQL"),
            WideBstr(L"SELECT CurrentBrightness FROM WmiMonitorBrightness"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &enumerator);
        if (FAILED(hr) || !enumerator) {
            return -1;
        }

        int total = 0;
        int count = 0;
        IWbemClassObject* object = nullptr;
        ULONG returned = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &object, &returned) == S_OK && object) {
            VARIANT value{};
            VariantInit(&value);
            if (SUCCEEDED(object->Get(L"CurrentBrightness", 0, &value, nullptr, nullptr))
                && value.vt == VT_I4) {
                total += value.lVal;
                ++count;
            }
            VariantClear(&value);
            object->Release();
            object = nullptr;
        }
        enumerator->Release();
        return count > 0 ? total / count : -1;
    }

private:
    bool EnsureReady() const {
        if (ready_) {
            return true;
        }
        if (attempted_) {
            return false;
        }
        attempted_ = true;

        if (!comScope_.Ok()) {
            return false;
        }

        if (!locator_) {
            IWbemLocator* newLocator = nullptr;
            if (FAILED(CoCreateInstance(
                    CLSID_WbemLocator,
                    nullptr,
                    CLSCTX_INPROC_SERVER,
                    IID_IWbemLocator,
                    reinterpret_cast<void**>(&newLocator)))) {
                return false;
            }
            locator_ = newLocator;
        }

        if (!services_) {
            if (FAILED(locator_->ConnectServer(
                    WideBstr(L"ROOT\\WMI"),
                    nullptr,
                    nullptr,
                    nullptr,
                    0,
                    nullptr,
                    nullptr,
                    const_cast<IWbemServices**>(&services_)))) {
                return false;
            }

            CoSetProxyBlanket(
                services_,
                RPC_C_AUTHN_WINNT,
                RPC_C_AUTHZ_NONE,
                nullptr,
                RPC_C_AUTHN_LEVEL_CALL,
                RPC_C_IMP_LEVEL_IMPERSONATE,
                nullptr,
                EOAC_NONE);
        }

        IEnumWbemClassObject* enumerator = nullptr;
        const HRESULT hr = services_->ExecQuery(
            WideBstr(L"WQL"),
            WideBstr(L"SELECT Active FROM WmiMonitorBrightnessMethods WHERE Active=TRUE"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &enumerator);
        if (FAILED(hr) || !enumerator) {
            return false;
        }

        IWbemClassObject* object = nullptr;
        ULONG returned = 0;
        const HRESULT nextHr = enumerator->Next(WBEM_INFINITE, 1, &object, &returned);
        if (object) {
            object->Release();
        }
        enumerator->Release();
        ready_ = nextHr == S_OK && returned > 0;
        return ready_;
    }

    bool SetSinglePercent(int percent) const {
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }
        if (!EnsureReady() || !services_) {
            return false;
        }

        IWbemClassObject* classObject = nullptr;
        if (FAILED(services_->GetObject(
                WideBstr(L"WmiMonitorBrightnessMethods"),
                0,
                nullptr,
                &classObject,
                nullptr))
            || !classObject) {
            return false;
        }

        IWbemClassObject* methodSignature = nullptr;
        if (FAILED(classObject->GetMethod(L"WmiSetBrightness", 0, &methodSignature, nullptr))
            || !methodSignature) {
            classObject->Release();
            return false;
        }

        IEnumWbemClassObject* enumerator = nullptr;
        const HRESULT enumHr = services_->ExecQuery(
            WideBstr(L"WQL"),
            WideBstr(L"SELECT * FROM WmiMonitorBrightnessMethods WHERE Active=TRUE"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            nullptr,
            &enumerator);
        if (FAILED(enumHr) || !enumerator) {
            methodSignature->Release();
            classObject->Release();
            return false;
        }

        bool anySet = false;
        IWbemClassObject* instance = nullptr;
        ULONG returned = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &instance, &returned) == S_OK && instance) {
            IWbemClassObject* params = nullptr;
            if (SUCCEEDED(methodSignature->SpawnInstance(0, &params)) && params) {
                VARIANT timeout{};
                VariantInit(&timeout);
                timeout.vt = VT_I4;
                timeout.lVal = 1;
                params->Put(L"Timeout", 0, &timeout, 0);
                VariantClear(&timeout);

                VARIANT brightness{};
                VariantInit(&brightness);
                brightness.vt = VT_UI1;
                brightness.bVal = static_cast<UCHAR>(percent);
                params->Put(L"Brightness", 0, &brightness, 0);
                VariantClear(&brightness);

                VARIANT path{};
                VariantInit(&path);
                if (SUCCEEDED(instance->Get(L"__PATH", 0, &path, nullptr, nullptr)) && path.vt == VT_BSTR) {
                    if (SUCCEEDED(services_->ExecMethod(path.bstrVal, WideBstr(L"WmiSetBrightness"), 0, nullptr, params, nullptr, nullptr))) {
                        anySet = true;
                    }
                }
                VariantClear(&path);
                params->Release();
            }
            instance->Release();
            instance = nullptr;
        }

        enumerator->Release();
        methodSignature->Release();
        classObject->Release();
        return anySet;
    }

    void Disconnect() const {
        if (services_) {
            services_->Release();
            services_ = nullptr;
        }
        if (locator_) {
            locator_->Release();
            locator_ = nullptr;
        }
        ready_ = false;
        attempted_ = false;
    }

    mutable ComScope comScope_;
    mutable IWbemLocator* locator_ = nullptr;
    mutable IWbemServices* services_ = nullptr;
    mutable bool attempted_ = false;
    mutable bool ready_ = false;
};

class PowerSchemeBrightnessBackend {
public:
    bool AnyControllable() const {
        GUID* scheme = nullptr;
        return PowerGetActiveScheme(nullptr, &scheme) == ERROR_SUCCESS;
    }

    bool SetPercent(int percent) {
        if (percent < 0) {
            percent = 0;
        } else if (percent > 100) {
            percent = 100;
        }

        GUID* scheme = nullptr;
        if (PowerGetActiveScheme(nullptr, &scheme) != ERROR_SUCCESS || !scheme) {
            return false;
        }

        SYSTEM_POWER_STATUS status{};
        GetSystemPowerStatus(&status);
        const bool onBattery = status.ACLineStatus == 0;

        DWORD result = ERROR_GEN_FAILURE;
        if (onBattery) {
            result = PowerWriteDCValueIndex(
                nullptr,
                scheme,
                &GUID_VIDEO_SUBGROUP,
                &GUID_DEVICE_POWER_POLICY_VIDEO_BRIGHTNESS,
                static_cast<DWORD>(percent));
        } else {
            result = PowerWriteACValueIndex(
                nullptr,
                scheme,
                &GUID_VIDEO_SUBGROUP,
                &GUID_DEVICE_POWER_POLICY_VIDEO_BRIGHTNESS,
                static_cast<DWORD>(percent));
        }

        if (result == ERROR_SUCCESS) {
            result = PowerSetActiveScheme(nullptr, scheme);
        }
        LocalFree(scheme);
        return result == ERROR_SUCCESS;
    }

    int GetPercent() const {
        GUID* scheme = nullptr;
        if (PowerGetActiveScheme(nullptr, &scheme) != ERROR_SUCCESS || !scheme) {
            return -1;
        }

        SYSTEM_POWER_STATUS status{};
        GetSystemPowerStatus(&status);
        const bool onBattery = status.ACLineStatus == 0;

        DWORD value = 0;
        DWORD result = ERROR_GEN_FAILURE;
        if (onBattery) {
            result = PowerReadDCValueIndex(
                nullptr,
                scheme,
                &GUID_VIDEO_SUBGROUP,
                &GUID_DEVICE_POWER_POLICY_VIDEO_BRIGHTNESS,
                &value);
        } else {
            result = PowerReadACValueIndex(
                nullptr,
                scheme,
                &GUID_VIDEO_SUBGROUP,
                &GUID_DEVICE_POWER_POLICY_VIDEO_BRIGHTNESS,
                &value);
        }
        LocalFree(scheme);
        return result == ERROR_SUCCESS ? static_cast<int>(value) : -1;
    }
};

class WinBrightnessController : public IBrightnessController {
public:
    bool AnyControllable() const override {
        return ddc_.AnyControllable() || wmi_.AnyControllable() || power_.AnyControllable();
    }

    void Capture() override {
        if (captured_) {
            return;
        }

        ddcCapturedValues_.clear();
        wmiCapturedValues_.clear();
        powerCapturedPercent_ = -1;
        ddc_.Capture(ddcCapturedValues_);
        wmi_.Capture(wmiCapturedValues_);
        powerCapturedPercent_ = power_.GetPercent();
        captured_ = true;
        lastPercent_ = -1;
    }

    void Restore() override {
        if (!captured_) {
            return;
        }

        ddc_.Restore(ddcCapturedValues_);
        wmi_.Restore(wmiCapturedValues_);
        if (powerCapturedPercent_ >= 0) {
            power_.SetPercent(powerCapturedPercent_);
        }
        captured_ = false;
        lastPercent_ = -1;
        ddcCapturedValues_.clear();
        wmiCapturedValues_.clear();
        powerCapturedPercent_ = -1;
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
        const bool powerSet = power_.SetPercent(percent);
        if (ddcSet || wmiSet || powerSet) {
            lastPercent_ = percent;
        }
        return ddcSet || wmiSet || powerSet;
    }

    int GetCurrentPercent() const override {
        const int ddc = ddc_.GetAveragePercent();
        if (ddc >= 0) {
            return ddc;
        }
        const int wmi = wmi_.GetAveragePercent();
        if (wmi >= 0) {
            return wmi;
        }
        return power_.GetPercent();
    }

private:
    DdcBrightnessBackend ddc_;
    WmiComBrightnessBackend wmi_;
    PowerSchemeBrightnessBackend power_;
    std::vector<DWORD> ddcCapturedValues_;
    std::vector<DWORD> wmiCapturedValues_;
    int powerCapturedPercent_ = -1;
    bool captured_ = false;
    int lastPercent_ = -1;
};

}  // namespace

IBrightnessController* CreateWinBrightnessController() {
    return new WinBrightnessController();
}
