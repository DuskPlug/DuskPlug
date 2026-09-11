#include "schedule_dialog.h"
#include "resource.h"
#include "schedule.h"

#include <commctrl.h>

namespace {

struct ScheduleDialogState {
    int onMinutes = 0;
    int offMinutes = 0;
    bool cancelled = true;
};

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

INT_PTR CALLBACK ScheduleDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<ScheduleDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_INITDIALOG: {
        state = reinterpret_cast<ScheduleDialogState*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        INITCOMMONCONTROLSEX icc{};
        icc.dwSize = sizeof(icc);
        icc.dwICC = ICC_DATE_CLASSES;
        InitCommonControlsEx(&icc);

        SYSTEMTIME onTime{};
        SYSTEMTIME offTime{};
        MinutesToSystemTime(state->onMinutes, onTime);
        MinutesToSystemTime(state->offMinutes, offTime);

        SendDlgItemMessageW(hwnd, IDC_ON_TIME, DTM_SETFORMAT, 0, reinterpret_cast<LPARAM>(L"HH:mm"));
        SendDlgItemMessageW(hwnd, IDC_OFF_TIME, DTM_SETFORMAT, 0, reinterpret_cast<LPARAM>(L"HH:mm"));
        SendDlgItemMessageW(hwnd, IDC_ON_TIME, DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&onTime));
        SendDlgItemMessageW(hwnd, IDC_OFF_TIME, DTM_SETSYSTEMTIME, GDT_VALID, reinterpret_cast<LPARAM>(&offTime));
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            if (!state) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }

            SYSTEMTIME onTime{};
            SYSTEMTIME offTime{};
            if (SendDlgItemMessageW(hwnd, IDC_ON_TIME, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&onTime)) != GDT_VALID) {
                MessageBoxW(hwnd, L"Please choose a valid ON time.", L"DuskPlug — Schedule", MB_ICONWARNING | MB_OK);
                return TRUE;
            }
            if (SendDlgItemMessageW(hwnd, IDC_OFF_TIME, DTM_GETSYSTEMTIME, 0, reinterpret_cast<LPARAM>(&offTime)) != GDT_VALID) {
                MessageBoxW(hwnd, L"Please choose a valid OFF time.", L"DuskPlug — Schedule", MB_ICONWARNING | MB_OK);
                return TRUE;
            }

            const int onMinutes = SystemTimeToMinutes(onTime);
            const int offMinutes = SystemTimeToMinutes(offTime);
            if (onMinutes == offMinutes) {
                MessageBoxW(
                    hwnd,
                    L"ON and OFF times cannot be the same.\n\n"
                    L"Pick two different times, or use overnight spans like 22:00 to 06:00.",
                    L"DuskPlug — Schedule",
                    MB_ICONWARNING | MB_OK);
                return TRUE;
            }

            state->onMinutes = onMinutes;
            state->offMinutes = offMinutes;
            state->cancelled = false;
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

bool ShowScheduleDialog(HWND owner, int& onMinutes, int& offMinutes) {
    ScheduleDialogState state{};
    state.onMinutes = onMinutes;
    state.offMinutes = offMinutes;

    const INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_SCHEDULE),
        owner,
        ScheduleDialogProc,
        reinterpret_cast<LPARAM>(&state));

    if (result != IDOK || state.cancelled) {
        return false;
    }

    onMinutes = state.onMinutes;
    offMinutes = state.offMinutes;
    return true;
}
