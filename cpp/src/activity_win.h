#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>

enum class SessionTransition {
    None,
    Locked,
    Unlocked,
};

class ActivityTracker {
public:
    using LockOffCallback = std::function<void()>;

    void SetLockOffSeconds(int seconds);
    void SetLockOffCallback(LockOffCallback onLockOffDue);
    void SetLockActivityCallback(LockOffCallback onLockActivity);

    bool Start(HWND hwnd);
    void Stop(HWND hwnd);

    bool IsSessionLocked() const { return sessionLocked_; }
    bool IsLockOffDue() const { return lockOffDue_; }
    bool HasMouseMovedSinceUnlock() const { return mouseMovedSinceUnlock_; }
    bool UnlockRequiresMouse() const { return unlockRequiresMouse_; }

    void ClearMouseMovedFlag();
    void ClearUnlockRequiresMouse() { unlockRequiresMouse_ = false; }
    void HandleSessionChange(WPARAM event);
    SessionTransition HandleLockTimerTick();
    void PollInputActivity();

private:
    static bool IsWorkstationLocked();
    static DWORD CurrentInputTick();
    static DWORD CurrentIdleMs();

    SessionTransition PollSessionLockState();

    HWND hwnd_ = nullptr;
    int lockOffSeconds_ = 30;
    bool sessionLocked_ = false;
    bool lockOffDue_ = false;
    bool mouseMovedSinceUnlock_ = false;
    ULONGLONG trackingStartedMs_ = 0;
    DWORD inputBaselineTick_ = 0;
    bool unlockRequiresMouse_ = false;
    LockOffCallback onLockOffDue_;
    LockOffCallback onLockActivity_;
};
