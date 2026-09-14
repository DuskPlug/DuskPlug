#pragma once

#include <functional>

enum class SessionTransition {
    None,
    Locked,
    Unlocked,
};

class IActivityTracker {
public:
    virtual ~IActivityTracker() = default;

    virtual void SetLockOffSeconds(int seconds) = 0;
    virtual void SetLockOffCallback(std::function<void()> onLockOffDue) = 0;
    virtual void SetLockActivityCallback(std::function<void()> onLockActivity) = 0;

    virtual bool Start() = 0;
    virtual void Stop() = 0;

    virtual bool IsSessionLocked() const = 0;
    virtual bool IsLockOffDue() const = 0;
    virtual bool HasMouseMovedSinceUnlock() const = 0;
    virtual bool UnlockRequiresMouse() const = 0;

    virtual void ClearMouseMovedFlag() = 0;
    virtual void ClearUnlockRequiresMouse() = 0;

    virtual void OnSessionLock() = 0;
    virtual void OnSessionUnlock() = 0;
    virtual SessionTransition HandleLockTimerTick() = 0;
    virtual void PollInputActivity() = 0;
};
