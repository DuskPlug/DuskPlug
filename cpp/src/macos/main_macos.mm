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
#include <cstdio>
#include <memory>
#include <string>

@interface DuskPlugAppDelegate : NSObject <NSApplicationDelegate, NSMenuDelegate>
@property(nonatomic, strong) NSStatusItem* statusItem;
@property(nonatomic, strong) NSMenu* menu;
@property(nonatomic, strong) NSMenuItem* turnOnItem;
@property(nonatomic, strong) NSMenuItem* turnOffItem;
@property(nonatomic, strong) NSMenuItem* lockOffItem;
@property(nonatomic, strong) NSMenuItem* screenBrightnessItem;
@property(nonatomic, strong) NSMenuItem* applyUpdateItem;
@property(nonatomic, strong) NSTimer* pollTimer;
@property(nonatomic, strong) NSTimer* smartTimer;
@property(nonatomic, strong) NSTimer* lockTimer;
- (void)rebuildMenu;
@end

namespace {

struct AppState {
    AppConfig config;
    std::unique_ptr<TuyaClient> client;
    SmartModeController smart;
    ILocationService* locationService = nullptr;
    MacActivityTracker* activityTracker = nullptr;
    IBrightnessController* brightnessController = nullptr;
    std::string configPath;
    std::atomic<bool> busy{false};
    bool hasKnownState = false;
    bool knownOn = false;
    InstallKind installKind = InstallKind::MacAppBundle;
    UpdateInfo pendingUpdate;
};

AppState g_app;
DuskPlugAppDelegate* g_delegate = nullptr;

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

void UpdateTrayDisplay(bool anyOn, size_t onCount, size_t totalCount) {
    g_app.hasKnownState = totalCount > 0;
    g_app.knownOn = anyOn;

    if (!g_delegate || !g_delegate.statusItem) {
        return;
    }

    const bool automated = g_app.smart.HasAutomatedDevices();
    char title[128] = "DuskPlug";

    if (totalCount <= 1) {
        if (automated) {
            snprintf(title, sizeof(title), anyOn ? "AUTO — ON" : "AUTO — OFF");
        } else {
            snprintf(title, sizeof(title), anyOn ? "ON" : "OFF");
        }
    } else {
        snprintf(title, sizeof(title), "%zu of %zu on", onCount, totalCount);
    }

    g_delegate.statusItem.button.title = @(title);
}

void RunPlugAction(bool toggle, bool setOn, bool statusOnly, int deviceIndex = -1) {
    if (g_app.busy || !g_app.client) {
        return;
    }

    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (enabledDevices.empty()) {
        return;
    }

    g_app.busy = true;
    std::string error;
    size_t successCount = 0;

    const auto operateDevice = [&](const DeviceConfig& device) {
        if (statusOnly) {
            DeviceState state{};
            if (g_app.client->GetDeviceState(device, state, error)) {
                g_app.smart.SetKnownDeviceState(device.id, state.switchOn);
                ++successCount;
            }
            return;
        }

        if (toggle) {
            bool newState = false;
            if (g_app.smart.ToggleDevice(device, error, newState)) {
                ++successCount;
            }
            return;
        }

        if (g_app.smart.SetDeviceSwitch(device, setOn, error)) {
            ++successCount;
        }
    };

    if (deviceIndex >= 0 && static_cast<size_t>(deviceIndex) < enabledDevices.size()) {
        operateDevice(*enabledDevices[static_cast<size_t>(deviceIndex)]);
    } else {
        for (const auto* device : enabledDevices) {
            operateDevice(*device);
        }
    }

    if (successCount > 0) {
        const size_t onCount = g_app.smart.GetKnownOnCount();
        UpdateTrayDisplay(onCount > 0, onCount, enabledDevices.size());
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

- (void)rebuildMenu {
    [self.menu removeAllItems];
    self.turnOnItem = nil;
    self.turnOffItem = nil;

    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (enabledDevices.size() <= 1) {
        self.turnOnItem = [self.menu addItemWithTitle:@"Turn On" action:@selector(onTurnOn:) keyEquivalent:@""];
        self.turnOffItem = [self.menu addItemWithTitle:@"Turn Off" action:@selector(onTurnOff:) keyEquivalent:@""];
    } else {
        [self.menu addItemWithTitle:@"Turn all on" action:@selector(onTurnOn:) keyEquivalent:@""];
        [self.menu addItemWithTitle:@"Turn all off" action:@selector(onTurnOff:) keyEquivalent:@""];
        [self.menu addItem:[NSMenuItem separatorItem]];
        for (size_t i = 0; i < enabledDevices.size(); ++i) {
            const std::string label = "Toggle " + enabledDevices[i]->name;
            NSMenuItem* item = [self.menu addItemWithTitle:@(label.c_str())
                                                    action:@selector(onToggleDevice:)
                                             keyEquivalent:@""];
            item.tag = static_cast<NSInteger>(i);
        }
    }

    [self.menu addItem:[NSMenuItem separatorItem]];
    self.lockOffItem = [[NSMenuItem alloc] initWithTitle:@"Off when locked or sleeping"
                                                  action:@selector(onToggleLockOff:)
                                           keyEquivalent:@""];
    self.lockOffItem.state = NSControlStateValueOn;
    [self.menu addItem:self.lockOffItem];
    self.screenBrightnessItem = [[NSMenuItem alloc] initWithTitle:@"Adjust screen brightness"
                                                           action:@selector(onToggleScreenBrightness:)
                                                    keyEquivalent:@""];
    self.screenBrightnessItem.state = NSControlStateValueOff;
    [self.menu addItem:self.screenBrightnessItem];
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
    UpdateApplyMenuItem(self);
}

- (void)applicationDidFinishLaunching:(__unused NSNotification*)notification {
    g_delegate = self;
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
    self.statusItem.menu = self.menu;
    [self rebuildMenu];

    g_app.locationService = CreateMacLocationService();
    g_app.activityTracker = CreateMacActivityTracker();
    g_app.brightnessController = CreateMacBrightnessController();

    if (IsConfigComplete(g_app.config)) {
        g_app.client = std::make_unique<TuyaClient>(g_app.config);
        SmartModeCallbacks callbacks{};
        callbacks.updateTray = [](bool anyOn, size_t onCount, size_t totalCount) {
            UpdateTrayDisplay(anyOn, onCount, totalCount);
        };
        callbacks.showSetupMessage = ShowMessage;
        callbacks.isBusy = []() { return g_app.busy.load(); };
        g_app.smart.Initialize(
            g_app.config,
            g_app.client.get(),
            g_app.locationService,
            g_app.activityTracker,
            g_app.brightnessController,
            callbacks);
        g_app.smart.LoadPersistedState();
        if (g_app.smart.HasAutomatedDevices()) {
            StartAutomationTimers(self);
            g_app.smart.Evaluate();
        } else {
            if (g_app.smart.IsScreenBrightnessEnabled()) {
                g_app.smart.Evaluate();
            }
            RunPlugAction(false, false, true);
        }
        MaybeBackgroundUpdateCheck(self);
    } else {
        ShowMessage("DuskPlug needs your Tuya plug connection details.\nOpen Settings from the menu bar.");
    }

    self.pollTimer = [NSTimer scheduledTimerWithTimeInterval:30 repeats:YES block:^(__unused NSTimer* timer) {
        if (g_app.smart.IsScreenBrightnessEnabled()) {
            g_app.smart.Evaluate();
        }
        if (!g_app.smart.HasAutomatedDevices()) {
            RunPlugAction(false, false, true);
        }
    }];
}

- (void)onTurnOn:(__unused id)sender { RunPlugAction(false, true, false); }
- (void)onTurnOff:(__unused id)sender { RunPlugAction(false, false, false); }
- (void)onRefresh:(__unused id)sender { RunPlugAction(false, false, true); }

- (void)onToggleDevice:(NSMenuItem*)sender {
    RunPlugAction(true, false, false, static_cast<int>(sender.tag));
}

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
    if (menu != self.menu) {
        return;
    }
    const bool automated = g_app.smart.HasAutomatedDevices();
    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (self.turnOnItem && self.turnOffItem && enabledDevices.size() <= 1) {
        self.turnOnItem.state = (!automated && g_app.hasKnownState && g_app.knownOn) ? NSControlStateValueOn
                                                                                     : NSControlStateValueOff;
        self.turnOffItem.state = (!automated && g_app.hasKnownState && !g_app.knownOn) ? NSControlStateValueOn
                                                                                       : NSControlStateValueOff;
    }
    if (self.lockOffItem) {
        self.lockOffItem.enabled = automated;
        self.lockOffItem.state = g_app.smart.IsLockOffEnabled() ? NSControlStateValueOn : NSControlStateValueOff;
    }
    if (self.screenBrightnessItem) {
        self.screenBrightnessItem.enabled = YES;
        self.screenBrightnessItem.state = g_app.smart.IsScreenBrightnessEnabled() ? NSControlStateValueOn
                                                                                  : NSControlStateValueOff;
    }
}

- (void)onToggleLockOff:(__unused id)sender {
    if (!g_app.smart.HasAutomatedDevices()) {
        return;
    }
    g_app.smart.ToggleLockOffEnabled();
    self.lockOffItem.state = g_app.smart.IsLockOffEnabled() ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)onToggleScreenBrightness:(__unused id)sender {
    g_app.smart.ToggleScreenBrightnessEnabled();
    self.screenBrightnessItem.state = g_app.smart.IsScreenBrightnessEnabled() ? NSControlStateValueOn
                                                                              : NSControlStateValueOff;
}

- (void)onSettings:(__unused id)sender {
    if (ShowMacSettingsDialog(g_app.configPath, g_app.config)) {
        g_app.client = std::make_unique<TuyaClient>(g_app.config);
        g_app.smart.UpdateConfig(g_app.config);
        g_app.smart.UpdateClient(g_app.client.get());
        if (g_app.smart.HasAutomatedDevices() || g_app.smart.IsScreenBrightnessEnabled()) {
            StartAutomationTimers(self);
            g_app.smart.Evaluate();
        }
        [self rebuildMenu];
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
    delete g_app.brightnessController;
    return 0;
}
