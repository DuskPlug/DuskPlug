#include "activity_win.h"

#include <wtsapi32.h>

#pragma comment(lib, "wtsapi32.lib")

namespace {

DWORD CurrentSessionId() {
    DWORD sessionId = 0;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId)) {
        return sessionId;
    }
    return WTSGetActiveConsoleSessionId();
}

bool QuerySessionLockedViaWts(bool& locked) {
    const DWORD sessionId = CurrentSessionId();
    if (sessionId == 0xFFFFFFFF) {
        return false;
    }

    LPWSTR buffer = nullptr;
    DWORD bytes = 0;
    if (!WTSQuerySessionInformationW(
            WTS_CURRENT_SERVER_HANDLE,
            sessionId,
            WTSSessionInfoEx,
            &buffer,
            &bytes) || !buffer) {
        return false;
    }

    const auto* info = reinterpret_cast<const WTSINFOEXW*>(buffer);
    if (info->Level != 1) {
        WTSFreeMemory(buffer);
        return false;
    }

    const LONG flags = info->Data.WTSInfoExLevel1.SessionFlags;
    WTSFreeMemory(buffer);

    if (flags == WTS_SESSIONSTATE_LOCK) {
        locked = true;
        return true;
    }
    if (flags == WTS_SESSIONSTATE_UNLOCK) {
        locked = false;
        return true;
    }

    return false;
}

bool IsLockedViaDesktopSwitch() {
    HDESK desk = OpenInputDesktop(0, FALSE, DESKTOP_SWITCHDESKTOP);
    if (!desk) {
        desk = OpenDesktopW(L"Default", 0, FALSE, DESKTOP_SWITCHDESKTOP);
        if (!desk) {
            return true;
        }
    }

    const BOOL canSwitch = SwitchDesktop(desk);
    CloseDesktop(desk);
    return canSwitch == FALSE;
}

}  // namespace

void ActivityTracker::SetLockOffSeconds(int seconds) {
    lockOffSeconds_ = seconds > 0 ? seconds : 30;
}

void ActivityTracker::SetLockOffCallback(LockOffCallback onLockOffDue) {
    onLockOffDue_ = std::move(onLockOffDue);
}

void ActivityTracker::SetLockActivityCallback(LockOffCallback onLockActivity) {
    onLockActivity_ = std::move(onLockActivity);
}

DWORD ActivityTracker::CurrentInputTick() {
    LASTINPUTINFO info{};
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info)) {
        return 0;
    }
    return info.dwTime;
}

DWORD ActivityTracker::CurrentIdleMs() {
    const DWORD lastInputTick = CurrentInputTick();
    if (lastInputTick == 0) {
        return 0;
    }
    return GetTickCount() - lastInputTick;
}

bool ActivityTracker::IsWorkstationLocked() {
    bool locked = false;
    if (QuerySessionLockedViaWts(locked)) {
        return locked;
    }

    HDESK desk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (!desk) {
        return IsLockedViaDesktopSwitch();
    }

    wchar_t name[256] = {};
    DWORD size = 0;
    const bool ok = GetUserObjectInformationW(desk, UOI_NAME, name, sizeof(name), &size) != FALSE;
    CloseDesktop(desk);
    if (!ok) {
        return IsLockedViaDesktopSwitch();
    }

    if (_wcsicmp(name, L"Winlogon") == 0 || _wcsicmp(name, L"Screen-saver") == 0) {
        return true;
    }

    return IsLockedViaDesktopSwitch();
}

SessionTransition ActivityTracker::PollSessionLockState() {
    if (trackingStartedMs_ != 0) {
        const ULONGLONG sinceStart = GetTickCount64() - trackingStartedMs_;
        if (sinceStart < 3000) {
            return SessionTransition::None;
        }
    }

    unlockRequiresMouse_ = false;
    const bool locked = IsWorkstationLocked();
    if (locked && !sessionLocked_) {
        HandleSessionChange(WTS_SESSION_LOCK);
        return SessionTransition::Locked;
    }
    if (!locked && sessionLocked_) {
        unlockRequiresMouse_ = lockOffDue_;
        HandleSessionChange(WTS_SESSION_UNLOCK);
        return SessionTransition::Unlocked;
    }
    return SessionTransition::None;
}

bool ActivityTracker::Start(HWND hwnd) {
    Stop(hwnd);
    hwnd_ = hwnd;
    trackingStartedMs_ = GetTickCount64();
    unlockRequiresMouse_ = false;

    if (!WTSRegisterSessionNotification(hwnd_, NOTIFY_FOR_THIS_SESSION)) {
        hwnd_ = nullptr;
        return false;
    }

    return true;
}

void ActivityTracker::Stop(HWND) {
    if (hwnd_) {
        WTSUnRegisterSessionNotification(hwnd_);
    }

    hwnd_ = nullptr;
    sessionLocked_ = false;
    lockOffDue_ = false;
    mouseMovedSinceUnlock_ = false;
    inputBaselineTick_ = 0;
    trackingStartedMs_ = 0;
    unlockRequiresMouse_ = false;
}

void ActivityTracker::ClearMouseMovedFlag() {
    mouseMovedSinceUnlock_ = false;
}

void ActivityTracker::HandleSessionChange(WPARAM event) {
    if (event == WTS_SESSION_LOCK) {
        sessionLocked_ = true;
        lockOffDue_ = false;
        inputBaselineTick_ = 0;
        return;
    }

    if (event == WTS_SESSION_UNLOCK) {
        sessionLocked_ = false;
        lockOffDue_ = false;
        mouseMovedSinceUnlock_ = false;
        inputBaselineTick_ = CurrentInputTick();
    }
}

void ActivityTracker::PollInputActivity() {
    if (sessionLocked_ || mouseMovedSinceUnlock_ || inputBaselineTick_ == 0) {
        return;
    }

    const DWORD tick = CurrentInputTick();
    if (tick != 0 && tick > inputBaselineTick_) {
        mouseMovedSinceUnlock_ = true;
    }
}

SessionTransition ActivityTracker::HandleLockTimerTick() {
    const SessionTransition transition = PollSessionLockState();
    PollInputActivity();

    if (!sessionLocked_) {
        return transition;
    }

    const DWORD idleMs = CurrentIdleMs();
    const DWORD lockOffMs = static_cast<DWORD>(lockOffSeconds_) * 1000U;
    const bool shouldBeOff = idleMs >= lockOffMs;

    if (shouldBeOff && !lockOffDue_) {
        lockOffDue_ = true;
        if (onLockOffDue_) {
            onLockOffDue_();
        }
    } else if (!shouldBeOff && lockOffDue_) {
        lockOffDue_ = false;
        if (onLockActivity_) {
            onLockActivity_();
        }
    }

    return transition;
}
