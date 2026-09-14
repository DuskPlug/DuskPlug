#include "config.h"

#include "timed_dialog.h"
#include "resource.h"

#include <commctrl.h>
#include <cwchar>
#include <string>

namespace {

struct TimedDialogState {
    int minutes = 30;
    bool cancelled = true;
};

INT_PTR CALLBACK TimedDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* state = reinterpret_cast<TimedDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_INITDIALOG: {
        state = reinterpret_cast<TimedDialogState*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        wchar_t buffer[32];
        swprintf_s(buffer, L"%d", state->minutes);
        SetDlgItemTextW(hwnd, IDC_TIMED_MINUTES, buffer);
        SendDlgItemMessageW(hwnd, IDC_TIMED_MINUTES, EM_SETSEL, 0, -1);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            if (!state) {
                EndDialog(hwnd, IDCANCEL);
                return TRUE;
            }

            wchar_t buffer[32]{};
            GetDlgItemTextW(hwnd, IDC_TIMED_MINUTES, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
            const int minutes = ClampTimedDurationMinutes(_wtoi(buffer));
            if (minutes < 1) {
                MessageBoxW(
                    hwnd,
                    L"Enter a duration of at least 1 minute.",
                    L"DuskPlug — Timed Mode",
                    MB_ICONWARNING | MB_OK);
                return TRUE;
            }

            state->minutes = minutes;
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

bool PromptTimedMinutes(HWND owner, int& minutes) {
    TimedDialogState state{};
    state.minutes = ClampTimedDurationMinutes(minutes > 0 ? minutes : 30);

    const INT_PTR result = DialogBoxParamW(
        GetModuleHandleW(nullptr),
        MAKEINTRESOURCEW(IDD_TIMED),
        owner,
        TimedDialogProc,
        reinterpret_cast<LPARAM>(&state));

    if (result != IDOK || state.cancelled) {
        return false;
    }

    minutes = state.minutes;
    return true;
}
