#include "activity_macos.h"
#include "platform_macos.h"

#include "../config.h"
#include "../platform_util.h"
#include "../schedule.h"
#include "../install_kind.h"
#include "../smart_mode.h"
#include "../tuya_client.h"
#include "../update_apply.h"
#include "../update_checker.h"
#include "../version.h"

#import <AppKit/AppKit.h>

#include <atomic>
#include <memory>
#include <string>

@interface DuskPlugAppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate>
@property(nonatomic, strong) NSStatusItem* statusItem;
@property(nonatomic, strong) NSMenu* menu;
@property(nonatomic, strong) NSMenuItem* lockOffItem;
@property(nonatomic, strong) NSMenuItem* applyUpdateItem;
@property(nonatomic, strong) NSTimer* pollTimer;
@property(nonatomic, strong) NSTimer* smartTimer;
@property(nonatomic, strong) NSTimer* lockTimer;
@end

namespace {

struct AppState {
    AppConfig config;
    std::unique_ptr<TuyaClient> client;
    SmartModeController smart;
    ILocationService* locationService = nullptr;
    MacActivityTracker* activityTracker = nullptr;
    std::string configPath;
    std::atomic<bool> busy{false};
    bool hasKnownState = false;
    bool knownOn = false;
    InstallKind installKind = InstallKind::MacAppBundle;
    UpdateInfo pendingUpdate;
};

AppState g_app;

void ShowMessage(const std::string& text);

void UpdateApplyMenuItem(DuskPlugAppDelegate* delegate) {
    if (!delegate.applyUpdateItem) {
        return;
    }
    const bool enabled = g_app.pendingUpdate.available && SupportsInAppUpdate(g_app.installKind);
    delegate.applyUpdateItem.enabled = enabled;
    if (enabled) {
        delegate.applyUpdateItem.title = [NSString stringWithFormat:@"Update to v%s...", g_app.pendingUpdate.version.c_str()];
    }
}

void HandleUpdateCheckResult(DuskPlugAppDelegate* delegate, const UpdateInfo& info, bool showNoUpdateMessage) {
    g_app.pendingUpdate = info;
    UpdateApplyMenuItem(delegate);

    if (!info.error.empty()) {
        if (showNoUpdateMessage) {
            ShowMessage(info.error);
        }
        return;
    }
    if (info.manifestMissing) {
        if (showNoUpdateMessage) {
            ShowMessage(
                "No update manifest is published yet for this release.\n\n"
                "See https://github.com/DuskPlug/DuskPlug/releases for downloads.");
        }
        return;
    }
    if (!info.available) {
        if (showNoUpdateMessage) {
            ShowMessage(std::string("You have the latest version (v") + DUSKPLUG_VERSION + ").");
        }
        return;
    }
    if (!SupportsInAppUpdate(g_app.installKind)) {
        ShowMessage("DuskPlug " + info.version + " is available.\n\n" + PackageManagerUpdateHint(g_app.installKind));
        return;
    }
    ShowMessage("DuskPlug " + info.version + " is available. Choose Update from the menu bar.");
}

void MaybeBackgroundUpdateCheck(DuskPlugAppDelegate* delegate) {
    if (!ShouldCheckForUpdatesNow()) {
        return;
    }
    HandleUpdateCheckResult(delegate, CheckForUpdates(g_app.installKind), false);
}

void ShowMessage(const std::string& text) {
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"DuskPlug";
    alert.informativeText = @(text.c_str());
    [alert runModal];
}

void UpdateTrayDisplay(bool on) {
    g_app.hasKnownState = true;
    g_app.knownOn = on;
    g_app.smart.SetKnownPlugState(on);
}

void RunPlugAction(bool toggle, bool setOn, bool statusOnly) {
    if (g_app.busy || !g_app.client) {
        return;
    }
    if (!statusOnly && g_app.smart.IsAutomationEnabled()) {
        g_app.smart.Disable();
    }

    g_app.busy = true;
    std::string error;
    bool on = false;
    bool ok = false;
    if (statusOnly) {
        ok = g_app.client->GetSwitchState(on, error);
    } else if (toggle) {
        ok = g_app.client->Toggle(error, on);
    } else {
        ok = g_app.client->SetSwitch(setOn, error);
        on = setOn;
    }
    if (ok) {
        UpdateTrayDisplay(on);
    }
    g_app.busy = false;
}

void StartAutomationTimers(DuskPlugAppDelegate* delegate) {
    delegate.smartTimer = [NSTimer scheduledTimerWithTimeInterval:10 repeats:YES block:^(__unused NSTimer* timer) {
        g_app.smart.Evaluate();
    }];
    delegate.lockTimer = [NSTimer scheduledTimerWithTimeInterval:1 repeats:YES block:^(__unused NSTimer* timer) {
        g_app.smart.OnLockTimerTick();
        if (g_app.activityTracker && g_app.activityTracker->IsPrepareForSleep()) {
            g_app.smart.OnPowerSuspend();
        }
    }];
}

}  // namespace

@implementation DuskPlugAppDelegate

- (void)applicationDidFinishLaunching:(__unused NSNotification*)notification {
    g_app.installKind = DetectInstallKind();
    g_app.configPath = ResolveConfigPath(GetExeDirectory() + "/config.json");
    EnsureConfigFile(g_app.configPath);

    std::string error;
    if (!LoadConfig(g_app.configPath, g_app.config, error, false)) {
        ShowMessage(error);
        [NSApp terminate:nil];
        return;
    }

    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    self.statusItem.button.title = @"DuskPlug";
    self.menu = [[NSMenu alloc] init];
    self.menu.delegate = self;
    [self.menu addItemWithTitle:@"Turn On" action:@selector(onTurnOn:) keyEquivalent:@""];
    [self.menu addItemWithTitle:@"Turn Off" action:@selector(onTurnOff:) keyEquivalent:@""];
    [self.menu addItemWithTitle:@"Smart Mode" action:@selector(onSmart:) keyEquivalent:@""];
    [self.menu addItemWithTitle:@"Schedule Mode" action:@selector(onSchedule:) keyEquivalent:@""];
    self.lockOffItem = [[NSMenuItem alloc] initWithTitle:@"Off when locked or sleeping"
                                                  action:@selector(onToggleLockOff:)
                                           keyEquivalent:@""];
    self.lockOffItem.state = NSControlStateValueOn;
    [self.menu addItem:self.lockOffItem];
    [self.menu addItem:[NSMenuItem separatorItem]];
    [self.menu addItemWithTitle:@"Settings..." action:@selector(onSettings:) keyEquivalent:@""];
    [self.menu addItemWithTitle:@"Refresh Status" action:@selector(onRefresh:) keyEquivalent:@""];
    [self.menu addItemWithTitle:@"Check for updates..." action:@selector(onCheckUpdates:) keyEquivalent:@""];
    self.applyUpdateItem = [[NSMenuItem alloc] initWithTitle:@"Update to latest..."
                                                      action:@selector(onApplyUpdate:)
                                               keyEquivalent:@""];
    self.applyUpdateItem.enabled = NO;
    [self.menu addItem:self.applyUpdateItem];
    [self.menu addItem:[NSMenuItem separatorItem]];
    [self.menu addItemWithTitle:@"Exit" action:@selector(onQuit:) keyEquivalent:@"q"];
    self.statusItem.menu = self.menu;

    g_app.locationService = CreateMacLocationService();
    g_app.activityTracker = CreateMacActivityTracker();

    if (IsConfigComplete(g_app.config)) {
        g_app.client = std::make_unique<TuyaClient>(g_app.config);
        SmartModeCallbacks callbacks{};
        callbacks.updateTray = UpdateTrayDisplay;
        callbacks.showSetupMessage = ShowMessage;
        callbacks.isBusy = []() { return g_app.busy.load(); };
        g_app.smart.Initialize(
            g_app.config,
            g_app.client.get(),
            g_app.locationService,
            g_app.activityTracker,
            callbacks);
        g_app.smart.LoadPersistedState();
        if (g_app.smart.IsAutomationEnabled()) {
            StartAutomationTimers(self);
            g_app.smart.Evaluate();
        } else {
            RunPlugAction(false, false, true);
        }
        MaybeBackgroundUpdateCheck(self);
    } else {
        ShowMessage("DuskPlug needs your Tuya plug connection details.\nOpen Settings from the menu bar.");
    }

    self.pollTimer = [NSTimer scheduledTimerWithTimeInterval:30 repeats:YES block:^(__unused NSTimer* timer) {
        if (!g_app.smart.IsAutomationEnabled()) {
            RunPlugAction(false, false, true);
        }
    }];
}

- (void)onTurnOn:(__unused id)sender { RunPlugAction(false, true, false); }
- (void)onTurnOff:(__unused id)sender { RunPlugAction(false, false, false); }
- (void)onRefresh:(__unused id)sender { RunPlugAction(false, false, true); }

- (void)onCheckUpdates:(__unused id)sender {
    HandleUpdateCheckResult(self, CheckForUpdates(g_app.installKind), true);
}

- (void)onApplyUpdate:(__unused id)sender {
    if (!g_app.pendingUpdate.available || !SupportsInAppUpdate(g_app.installKind)) {
        return;
    }
    NSAlert* alert = [[NSAlert alloc] init];
    alert.messageText = @"DuskPlug — Updates";
    alert.informativeText = [NSString stringWithFormat:@"Download and install DuskPlug %s?", g_app.pendingUpdate.version.c_str()];
    [alert addButtonWithTitle:@"Install"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) {
        return;
    }

    const ApplyUpdateResult result = ApplyUpdate(g_app.pendingUpdate, g_app.installKind);
    if (!result.success) {
        ShowMessage(result.error);
        return;
    }
    if (result.restartScheduled) {
        g_app.smart.SavePersistedState();
        [NSApp terminate:nil];
    }
}

- (void)menuWillOpen:(NSMenu*)menu {
    if (menu != self.menu || !self.lockOffItem) {
        return;
    }
    const bool automation = g_app.smart.IsAutomationEnabled();
    self.lockOffItem.enabled = automation;
    self.lockOffItem.state = g_app.smart.IsLockOffEnabled() ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)onToggleLockOff:(__unused id)sender {
    if (!g_app.smart.IsAutomationEnabled()) {
        return;
    }
    g_app.smart.ToggleLockOffEnabled();
    self.lockOffItem.state = g_app.smart.IsLockOffEnabled() ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)onSmart:(__unused id)sender {
    if (g_app.smart.IsEnabled()) {
        g_app.smart.Disable();
        return;
    }
    std::string error;
    if (!g_app.smart.Enable(error, true)) {
        if (!error.empty()) {
            ShowMessage(error);
        }
        return;
    }
    StartAutomationTimers(self);
    g_app.smart.Evaluate();
}

- (void)onSchedule:(__unused id)sender {
    if (g_app.smart.IsScheduleEnabled()) {
        g_app.smart.Disable();
        return;
    }
    std::string error;
    if (!g_app.smart.EnableSchedule(error)) {
        if (!error.empty()) {
            ShowMessage(error);
        }
        return;
    }
    StartAutomationTimers(self);
    g_app.smart.Evaluate();
}

- (void)onSettings:(__unused id)sender {
    if (ShowMacSettingsDialog(g_app.configPath, g_app.config)) {
        g_app.client = std::make_unique<TuyaClient>(g_app.config);
        g_app.smart.UpdateConfig(g_app.config);
        g_app.smart.UpdateClient(g_app.client.get());
        ShowMessage("Settings saved.");
    }
}

- (void)onQuit:(__unused id)sender {
    g_app.smart.SavePersistedState();
    [NSApp terminate:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(__unused NSApplication*)sender {
    return NO;
}

@end

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        DuskPlugAppDelegate* delegate = [DuskPlugAppDelegate new];
        app.delegate = delegate;
        [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
        [app run];
    }
    delete g_app.locationService;
    delete g_app.activityTracker;
    return 0;
}
