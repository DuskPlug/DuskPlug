#include "activity_linux.h"

#include "../platform_util.h"

#include <functional>
#include <systemd/sd-bus.h>

void LinuxActivityTracker::SetLockOffSeconds(int seconds) {
    lockOffSeconds_ = seconds > 0 ? seconds : 30;
}

void LinuxActivityTracker::SetLockOffCallback(std::function<void()> onLockOffDue) {
    onLockOffDue_ = std::move(onLockOffDue);
}

void LinuxActivityTracker::SetLockActivityCallback(std::function<void()> onLockActivity) {
    onLockActivity_ = std::move(onLockActivity);
}

bool LinuxActivityTracker::Start() { return true; }
void LinuxActivityTracker::Stop() {}

bool LinuxActivityTracker::IsSessionLocked() const { return sessionLocked_; }
bool LinuxActivityTracker::IsLockOffDue() const { return lockOffDue_; }
bool LinuxActivityTracker::HasMouseMovedSinceUnlock() const { return mouseMovedSinceUnlock_; }
bool LinuxActivityTracker::UnlockRequiresMouse() const { return unlockRequiresMouse_; }

void LinuxActivityTracker::ClearMouseMovedFlag() { mouseMovedSinceUnlock_ = false; }
void LinuxActivityTracker::ClearUnlockRequiresMouse() { unlockRequiresMouse_ = false; }

void LinuxActivityTracker::OnSessionLock() {
    sessionLocked_ = true;
    lockOffDue_ = false;
    lockedAtMs_ = MonotonicTimeMs();
}

void LinuxActivityTracker::OnSessionUnlock() {
    sessionLocked_ = false;
    lockOffDue_ = false;
    mouseMovedSinceUnlock_ = false;
    unlockedAtMs_ = MonotonicTimeMs();
}

SessionTransition LinuxActivityTracker::HandleLockTimerTick() {
    SessionTransition transition = SessionTransition::None;
    const bool locked = QueryLockedHint();
    if (locked && !sessionLocked_) {
        OnSessionLock();
        transition = SessionTransition::Locked;
    } else if (!locked && sessionLocked_) {
        unlockRequiresMouse_ = lockOffDue_;
        OnSessionUnlock();
        transition = SessionTransition::Unlocked;
    }

    if (sessionLocked_) {
        const uint64_t elapsed = MonotonicTimeMs() - lockedAtMs_;
        const uint64_t threshold = static_cast<uint64_t>(lockOffSeconds_) * 1000ULL;
        if (elapsed >= threshold && !lockOffDue_) {
            lockOffDue_ = true;
            if (onLockOffDue_) {
                onLockOffDue_();
            }
        }
    } else if (unlockedAtMs_ != 0 && MonotonicTimeMs() - unlockedAtMs_ > 500) {
        mouseMovedSinceUnlock_ = true;
    }

    return transition;
}

void LinuxActivityTracker::PollInputActivity() {}

void LinuxActivityTracker::SetPrepareForSleep(bool value) {
    prepareForSleep_ = value;
}

bool LinuxActivityTracker::IsPrepareForSleep() const {
    return prepareForSleep_;
}

bool LinuxActivityTracker::QueryLockedHint() {
    sd_bus* bus = nullptr;
    if (sd_bus_open_system(&bus) < 0) {
        return sessionLocked_;
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    const int rc = sd_bus_call_method(
        bus,
        "org.freedesktop.login1",
        "/org/freedesktop/login1/session/self",
        "org.freedesktop.DBus.Properties",
        "Get",
        &error,
        &reply,
        "ss",
        "org.freedesktop.login1.Session",
        "LockedHint");
    sd_bus_flush_close_unref(bus);

    if (rc < 0 || !reply) {
        sd_bus_error_free(&error);
        return sessionLocked_;
    }

    bool locked = false;
    sd_bus_message_read(reply, "v", "b", &locked);
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    return locked;
}

LinuxActivityTracker* CreateLinuxActivityTracker() {
    return new LinuxActivityTracker();
}
