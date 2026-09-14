#include "activity_macos.h"

#include "../platform_util.h"

#import <AppKit/AppKit.h>

MacActivityTracker::MacActivityTracker() {
    screenLockObserver_ = [[NSDistributedNotificationCenter defaultCenter]
        addObserverForName:@"com.apple.screenIsLocked"
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(__unused NSNotification* note) {
                    OnSessionLock();
                }];
    screenUnlockObserver_ = [[NSDistributedNotificationCenter defaultCenter]
        addObserverForName:@"com.apple.screenIsUnlocked"
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(__unused NSNotification* note) {
                    unlockRequiresMouse_ = lockOffDue_;
                    OnSessionUnlock();
                }];
    sleepObserver_ = [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserverForName:NSWorkspaceWillSleepNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(__unused NSNotification* note) {
                    prepareForSleep_ = true;
                }];
    wakeObserver_ = [[[NSWorkspace sharedWorkspace] notificationCenter]
        addObserverForName:NSWorkspaceDidWakeNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(__unused NSNotification* note) {
                    prepareForSleep_ = false;
                }];
}

MacActivityTracker::~MacActivityTracker() {
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:(__bridge id)screenLockObserver_];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:(__bridge id)screenUnlockObserver_];
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:(__bridge id)sleepObserver_];
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:(__bridge id)wakeObserver_];
}

void MacActivityTracker::SetLockOffSeconds(int seconds) { lockOffSeconds_ = seconds > 0 ? seconds : 30; }
void MacActivityTracker::SetLockOffCallback(std::function<void()> cb) { onLockOffDue_ = std::move(cb); }
void MacActivityTracker::SetLockActivityCallback(std::function<void()> cb) { onLockActivity_ = std::move(cb); }
bool MacActivityTracker::Start() { return true; }
void MacActivityTracker::Stop() {}

bool MacActivityTracker::IsSessionLocked() const { return sessionLocked_; }
bool MacActivityTracker::IsLockOffDue() const { return lockOffDue_; }
bool MacActivityTracker::HasMouseMovedSinceUnlock() const { return mouseMovedSinceUnlock_; }
bool MacActivityTracker::UnlockRequiresMouse() const { return unlockRequiresMouse_; }
void MacActivityTracker::ClearMouseMovedFlag() { mouseMovedSinceUnlock_ = false; }
void MacActivityTracker::ClearUnlockRequiresMouse() { unlockRequiresMouse_ = false; }

void MacActivityTracker::OnSessionLock() {
    sessionLocked_ = true;
    lockOffDue_ = false;
    lockedAtMs_ = MonotonicTimeMs();
}

void MacActivityTracker::OnSessionUnlock() {
    sessionLocked_ = false;
    lockOffDue_ = false;
    mouseMovedSinceUnlock_ = false;
    unlockedAtMs_ = MonotonicTimeMs();
}

SessionTransition MacActivityTracker::HandleLockTimerTick() {
    if (sessionLocked_) {
        const uint64_t elapsed = MonotonicTimeMs() - lockedAtMs_;
        if (elapsed >= static_cast<uint64_t>(lockOffSeconds_) * 1000ULL && !lockOffDue_) {
            lockOffDue_ = true;
            if (onLockOffDue_) {
                onLockOffDue_();
            }
        }
    } else if (unlockedAtMs_ != 0 && MonotonicTimeMs() - unlockedAtMs_ > 500) {
        mouseMovedSinceUnlock_ = true;
    }
    return SessionTransition::None;
}

void MacActivityTracker::PollInputActivity() {}
bool MacActivityTracker::IsPrepareForSleep() const { return prepareForSleep_; }

MacActivityTracker* CreateMacActivityTracker() {
    return new MacActivityTracker();
}
