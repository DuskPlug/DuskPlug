#pragma once

#include "activity_tracker.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <functional>

class ActivityTracker : public IActivityTracker {
public:
    void SetLockOffSeconds(int seconds) override;
    void SetLockOffCallback(std::function<void()> onLockOffDue) override;
    void SetLockActivityCallback(std::function<void()> onLockActivity) override;

    bool Start() override;
    void Stop() override;
    void SetWindow(HWND hwnd) { hwnd_ = hwnd; }

    bool IsSessionLocked() const override { return sessionLocked_; }
    bool IsLockOffDue() const override { return lockOffDue_; }
    bool HasMouseMovedSinceUnlock() const override { return mouseMovedSinceUnlock_; }
    bool UnlockRequiresMouse() const override { return unlockRequiresMouse_; }

    void ClearMouseMovedFlag() override;
    void ClearUnlockRequiresMouse() override { unlockRequiresMouse_ = false; }
    void OnSessionLock() override;
    void OnSessionUnlock() override;
    SessionTransition HandleLockTimerTick() override;
    void PollInputActivity() override;

    void HandleSessionChange(WPARAM event);

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
    std::function<void()> onLockOffDue_;
    std::function<void()> onLockActivity_;
};
