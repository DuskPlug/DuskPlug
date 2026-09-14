#pragma once

#include "../activity_tracker.h"

class MacActivityTracker : public IActivityTracker {
public:
    MacActivityTracker();
    ~MacActivityTracker() override;

    void SetLockOffSeconds(int seconds) override;
    void SetLockOffCallback(std::function<void()> onLockOffDue) override;
    void SetLockActivityCallback(std::function<void()> onLockActivity) override;
    bool Start() override;
    void Stop() override;
    bool IsSessionLocked() const override;
    bool IsLockOffDue() const override;
    bool HasMouseMovedSinceUnlock() const override;
    bool UnlockRequiresMouse() const override;
    void ClearMouseMovedFlag() override;
    void ClearUnlockRequiresMouse() override;
    void OnSessionLock() override;
    void OnSessionUnlock() override;
    SessionTransition HandleLockTimerTick() override;
    void PollInputActivity() override;

    bool IsPrepareForSleep() const;

private:
    void* screenLockObserver_ = nullptr;
    void* screenUnlockObserver_ = nullptr;
    void* sleepObserver_ = nullptr;
    void* wakeObserver_ = nullptr;
    std::function<void()> onLockOffDue_;
    std::function<void()> onLockActivity_;
    int lockOffSeconds_ = 30;
    bool sessionLocked_ = false;
    bool lockOffDue_ = false;
    bool mouseMovedSinceUnlock_ = false;
    bool unlockRequiresMouse_ = false;
    bool prepareForSleep_ = false;
    uint64_t lockedAtMs_ = 0;
    uint64_t unlockedAtMs_ = 0;
};

MacActivityTracker* CreateMacActivityTracker();
