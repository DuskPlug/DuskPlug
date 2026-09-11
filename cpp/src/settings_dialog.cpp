#include "settings_dialog.h"
#include "location_win.h"
#include "resource.h"
#include "schedule.h"

#include <commctrl.h>

#include <cstdlib>
#include <string>

namespace {

struct SettingsDialogState {
    std::wstring configPath;
    AppConfig* config = nullptr;
};

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (size <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(), size, nullptr, nullptr);
    return utf8;
}

std::wstring GetDlgItemTextWide(HWND dlg, int controlId) {
    const HWND control = GetDlgItem(dlg, controlId);
    if (!control) {
        return {};
    }

    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length) + 1, L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(length);
    return text;
}

void SetDlgItemTextUtf8(HWND dlg, int controlId, const std::string& text) {
    SetDlgItemTextW(dlg, controlId, Utf8ToWide(text).c_str());
}

std::string GetDlgItemTextUtf8(HWND dlg, int controlId) {
    return WideToUtf8(GetDlgItemTextWide(dlg, controlId));
}

void MinutesToSystemTime(int minutes, SYSTEMTIME& st) {
    GetLocalTime(&st);
    st.wHour = static_cast<WORD>(minutes / 60);
    st.wMinute = static_cast<WORD>(minutes % 60);
    st.wSecond = 0;
    st.wMilliseconds = 0;
}

int SystemTimeToMinutes(const SYSTEMTIME& st) {
    return static_cast<int>(st.wHour) * 60 + static_cast<int>(st.wMinute);
}

bool ParseSignedInt(const std::wstring& text, int& out) {
    if (text.empty()) {
        return false;
    }

    wchar_t* end = nullptr;
    const long value = wcstol(text.c_str(), &end, 10);
    if (end == text.c_str() || (end && *end != L'\0')) {
        return false;
    }

    out = static_cast<int>(value);
    return true;
}

bool ParseDoubleValue(const std::wstring& text, double& out) {
    if (text.empty()) {
        return false;
    }

    wchar_t* end = nullptr;
    out = wcstod(text.c_str(), &end);
    return end != text.c_str() && (end == nullptr || *end == L'\0');
}

void PopulateDataCenters(HWND combo) {
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Central Europe (UK / most EU)"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Western Europe"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Western America"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Eastern America"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Singapore"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"India"));
}

void LoadSettingsIntoDialog(HWND dlg, const AppConfig& config) {
    SetDlgItemTextUtf8(dlg, IDC_SET_CLIENT_ID, config.clientId);
    SetDlgItemTextUtf8(dlg, IDC_SET_CLIENT_SECRET, config.clientSecret);
    SetDlgItemTextUtf8(dlg, IDC_SET_DEVICE_ID, config.deviceId);
    SetDlgItemTextUtf8(dlg, IDC_SET_SWITCH_CODE, config.switchCode);

    const HWND dataCenter = GetDlgItem(dlg, IDC_SET_DATA_CENTER);
    if (dataCenter) {
        SendMessageW(dataCenter, CB_SETCURSEL, BaseUrlToDataCenterIndex(config.baseUrl), 0);
    }

    wchar_t buffer[64];
    swprintf_s(buffer, L"%.4f", config.latitude);
    SetDlgItemTextW(dlg, IDC_SET_LATITUDE, buffer);
    swprintf_s(buffer, L"%.4f", config.longitude);
    SetDlgItemTextW(dlg, IDC_SET_LONGITUDE, buffer);
    swprintf_s(buffer, L"%d", config.darkOffsetMinutes);
    SetDlgItemTextW(dlg, IDC_SET_DARK_OFFSET, buffer);
    swprintf_s(buffer, L"%d", config.lightOffsetMinutes);
    SetDlgItemTextW(dlg, IDC_SET_LIGHT_OFFSET, buffer);
    swprintf_s(buffer, L"%d", config.lockOffSeconds);
    SetDlgItemTextW(dlg, IDC_SET_LOCK_OFF, buffer);

    int onMinutes = 18 * 60;
    int offMinutes = 23 * 60;
    ParseTimeHHMM(config.scheduleOnTime, onMinutes);
    ParseTimeHHMM(config.scheduleOffTime, offMinutes);

    SYSTEMTIME onTime{};
    SYSTEMTIME offTime{};
    MinutesToSystemTime(onMinutes, onTime);
    MinutesToSystemTime(offMinutes, offTime);
    SendDlgItemMessageW(dlg, IDC_SET_ON_TIME, DTM_SETFORMAT, 0, reinterpret_cast<LPARAM>(L"HH:mm"));
    SendDlgItemMessageW(dlg, IDC_SET_OFF_TIME, DTM_SETFORMAT, 0, reinterpret_cast<LPARAM>(L"HH:mm"));
    SendDlgItemMessageW(dlg, IDC_SET_ON_TIME, DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&onTime));
    SendDlgItemMessageW(dlg, IDC_SET_OFF_TIME, DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&offTime));
}

bool CollectSettingsFromDialog(HWND dlg, AppConfig& config) {
    config.clientId = GetDlgItemTextUtf8(dlg, IDC_SET_CLIENT_ID);
    config.clientSecret = GetDlgItemTextUtf8(dlg, IDC_SET_CLIENT_SECRET);
    config.deviceId = GetDlgItemTextUtf8(dlg, IDC_SET_DEVICE_ID);
    config.switchCode = GetDlgItemTextUtf8(dlg, IDC_SET_SWITCH_CODE);
    if (config.switchCode.empty()) {
        config.switchCode = "switch_1";
    }

    if (config.clientId.empty() || config.clientSecret.empty() || config.deviceId.empty()) {
        MessageBoxW(
            dlg,
            L"Access ID, Access Secret, and Device ID are required.",
            L"DuskPlug — Settings",
            MB_ICONWARNING | MB_OK);
        return false;
    }

    const HWND dataCenter = GetDlgItem(dlg, IDC_SET_DATA_CENTER);
    const int centerIndex = static_cast<int>(SendMessageW(dataCenter, CB_GETCURSEL, 0, 0));
    config.baseUrl = DataCenterIndexToBaseUrl(centerIndex < 0 ? 0 : centerIndex);

    double latitude = 0.0;
    double longitude = 0.0;
    if (!ParseDoubleValue(GetDlgItemTextWide(dlg, IDC_SET_LATITUDE), latitude)
        || !ParseDoubleValue(GetDlgItemTextWide(dlg, IDC_SET_LONGITUDE), longitude)) {
        MessageBoxW(
            dlg,
            L"Enter valid latitude and longitude values, or use Detect Location.",
            L"DuskPlug — Settings",
            MB_ICONWARNING | MB_OK);
        return false;
    }

    config.latitude = latitude;
    config.longitude = longitude;
    config.hasLatitude = true;
    config.hasLongitude = true;

    if (!ParseSignedInt(GetDlgItemTextWide(dlg, IDC_SET_DARK_OFFSET), config.darkOffsetMinutes)
        || !ParseSignedInt(GetDlgItemTextWide(dlg, IDC_SET_LIGHT_OFFSET), config.lightOffsetMinutes)
        || !ParseSignedInt(GetDlgItemTextWide(dlg, IDC_SET_LOCK_OFF), config.lockOffSeconds)) {
        MessageBoxW(
            dlg,
            L"Offset and lock-off values must be whole numbers.",
            L"DuskPlug — Settings",
            MB_ICONWARNING | MB_OK);
        return false;
    }

    if (config.lockOffSeconds < 0) {
        MessageBoxW(dlg, L"Lock-off seconds cannot be negative.", L"DuskPlug — Settings", MB_ICONWARNING | MB_OK);
        return false;
    }

    SYSTEMTIME onTime{};
    SYSTEMTIME offTime{};
    if (SendDlgItemMessageW(dlg, IDC_SET_ON_TIME, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&onTime)) != GDT_VALID
        || SendDlgItemMessageW(dlg, IDC_SET_OFF_TIME, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&offTime)) != GDT_VALID) {
        MessageBoxW(dlg, L"Please choose valid ON and OFF times.", L"DuskPlug — Settings", MB_ICONWARNING | MB_OK);
        return false;
    }

    const int onMinutes = SystemTimeToMinutes(onTime);
    const int offMinutes = SystemTimeToMinutes(offTime);
    if (onMinutes == offMinutes) {
        MessageBoxW(
            dlg,
            L"ON and OFF times cannot be the same.\n\n"
            L"Pick two different times, or use overnight spans like 22:00 to 06:00.",
            L"DuskPlug — Settings",
            MB_ICONWARNING | MB_OK);
        return false;
    }

    if (!FormatTimeHHMM(onMinutes, config.scheduleOnTime) || !FormatTimeHHMM(offMinutes, config.scheduleOffTime)) {
        MessageBoxW(dlg, L"Could not read the schedule times.", L"DuskPlug — Settings", MB_ICONERROR | MB_OK);
        return false;
    }
    config.hasScheduleTimes = true;
    return true;
}

INT_PTR CALLBACK SettingsDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<SettingsDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_INITDIALOG: {
        state = reinterpret_cast<SettingsDialogState*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        INITCOMMONCONTROLSEX icc{};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_DATE_CLASSES;
        InitCommonControlsEx(&icc);

        const HWND dataCenter = GetDlgItem(hwnd, IDC_SET_DATA_CENTER);
        if (dataCenter) {
            PopulateDataCenters(dataCenter);
        }

        if (state && state->config) {
            LoadSettingsIntoDialog(hwnd, *state->config);
        }
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SET_DETECT_LOCATION: {
            double latitude = 0.0;
            double longitude = 0.0;
            if (!RequestWindowsLocation(hwnd, latitude, longitude)) {
                const int choice = MessageBoxW(
                    hwnd,
                    L"Windows did not provide a location.\n\n"
                    L"Check Settings → Privacy → Location:\n"
                    L"• Location services = On\n"
                    L"• Let desktop apps access your location = On\n\n"
                    L"Open Location settings now?",
                    L"DuskPlug — Settings",
                    MB_YESNO | MB_ICONWARNING);
                if (choice == IDYES) {
                    OpenWindowsLocationSettings();
                }
                return TRUE;
            }

            wchar_t buffer[64];
            swprintf_s(buffer, L"%.4f", latitude);
            SetDlgItemTextW(hwnd, IDC_SET_LATITUDE, buffer);
            swprintf_s(buffer, L"%.4f", longitude);
            SetDlgItemTextW(hwnd, IDC_SET_LONGITUDE, buffer);
            return TRUE;
        }
        case IDOK: {
            if (!state || !state->config) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }

            AppConfig updated = *state->config;
            if (!CollectSettingsFromDialog(hwnd, updated)) {
                return TRUE;
            }

            if (!SaveAppConfig(state->configPath, updated)) {
                MessageBoxW(
                    hwnd,
                    L"Could not save your settings.",
                    L"DuskPlug — Settings",
                    MB_ICONERROR | MB_OK);
                return TRUE;
            }

            *state->config = updated;
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}

}  // namespace

bool ShowSettingsDialog(HWND owner, const std::wstring& configPath, AppConfig& config) {
    SettingsDialogState state{};
    state.configPath = configPath;
    state.config = &config;

    const INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_SETTINGS),
        owner,
        SettingsDialogProc,
        reinterpret_cast<LPARAM>(&state));

    return result == IDOK;
}
