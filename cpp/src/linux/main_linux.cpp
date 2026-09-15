#include "platform_linux.h"

#include "../config.h"
#include "../platform_util.h"
#include "../schedule.h"
#include "../install_kind.h"
#include "../smart_mode.h"
#include "../tuya_client.h"
#include "../update_apply.h"
#include "../update_checker.h"
#include "../version.h"

#include <libayatana-appindicator/app-indicator.h>

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>

namespace {

struct AppState {
    AppIndicator* indicator = nullptr;
    AppConfig config;
    std::unique_ptr<TuyaClient> client;
    SmartModeController smart;
    ILocationService* locationService = nullptr;
    LinuxActivityTracker* activityTracker = nullptr;
    IBrightnessController* brightnessController = nullptr;
    std::string configPath;
    std::string appDir;
    std::atomic<bool> busy{false};
    bool hasKnownState = false;
    bool knownOn = false;
    guint pollTimer = 0;
    guint smartTimer = 0;
    guint lockTimer = 0;
    InstallKind installKind = InstallKind::LinuxTarball;
    UpdateInfo pendingUpdate;
};

AppState g_app;
GtkWidget* g_menu = nullptr;
GtkWidget* g_applyUpdateItem = nullptr;
GtkWidget* g_lockOffItem = nullptr;
GtkWidget* g_screenBrightnessItem = nullptr;
GtkWidget* g_smartModeItem = nullptr;
GtkWidget* g_scheduleModeItem = nullptr;

void UpdateTrayDisplay(bool anyOn, size_t onCount, size_t totalCount);
void RunPlugAction(bool toggle, bool setOn, bool statusOnly, int deviceIndex = -1);
void StartAutomationTimers();
void RebuildMenu();

void ShowMessage(const std::string& text) {
    GtkWidget* dialog = gtk_message_dialog_new(
        nullptr,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_OK,
        "%s",
        text.c_str());
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void UpdateTrayDisplay(bool anyOn, size_t onCount, size_t totalCount) {
    const bool automated = g_app.smart.HasAutomatedDevices();
    g_app.hasKnownState = totalCount > 0;
    g_app.knownOn = anyOn;

    const char* iconName = "light-off";
    char label[128] = "DuskPlug";

    if (automated) {
        iconName = anyOn ? "light-smart-on" : "light-smart-off";
    } else {
        iconName = anyOn ? "light-on" : "light-off";
    }

    if (totalCount <= 1) {
        if (automated) {
            snprintf(label, sizeof(label), anyOn ? "DuskPlug: AUTO — ON" : "DuskPlug: AUTO — OFF");
        } else {
            snprintf(label, sizeof(label), anyOn ? "DuskPlug: ON" : "DuskPlug: OFF");
        }
    } else {
        snprintf(label, sizeof(label), "DuskPlug: %zu of %zu on", onCount, totalCount);
    }

    app_indicator_set_icon_full(g_app.indicator, iconName, label);
    app_indicator_set_status(g_app.indicator, APP_INDICATOR_STATUS_ACTIVE);
}

void RunPlugAction(bool toggle, bool setOn, bool statusOnly, int deviceIndex) {
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

gboolean OnPollTimer(gpointer) {
    if (g_app.smart.IsScreenBrightnessEnabled()) {
        g_app.smart.Evaluate();
    }
    if (!g_app.smart.HasAutomatedDevices()) {
        RunPlugAction(false, false, true);
    }
    return G_SOURCE_CONTINUE;
}

gboolean OnSmartTimer(gpointer) {
    g_app.smart.Evaluate();
    return G_SOURCE_CONTINUE;
}

gboolean OnLockTimer(gpointer) {
    g_app.smart.OnLockTimerTick();
    if (g_app.activityTracker && g_app.activityTracker->IsPrepareForSleep()) {
        g_app.smart.OnPowerSuspend();
    }
    return G_SOURCE_CONTINUE;
}

void StartAutomationTimers() {
    if (!g_app.smartTimer) {
        g_app.smartTimer = g_timeout_add_seconds(10, OnSmartTimer, nullptr);
    }
    if (!g_app.lockTimer) {
        g_app.lockTimer = g_timeout_add_seconds(1, OnLockTimer, nullptr);
    }
}

void ApplySettingsReload() {
    std::string error;
    if (!LoadConfig(g_app.configPath, g_app.config, error, false)) {
        ShowMessage(error);
        return;
    }
    g_app.client = std::make_unique<TuyaClient>(g_app.config);
    g_app.smart.UpdateConfig(g_app.config);
    g_app.smart.UpdateClient(g_app.client.get());
    if (g_app.smart.HasAutomatedDevices() || g_app.smart.IsScreenBrightnessEnabled()) {
        StartAutomationTimers();
        g_app.smart.Evaluate();
    }
    RebuildMenu();
}

void OnSettings(GtkMenuItem*, gpointer) {
    if (ShowLinuxSettingsDialog(g_app.configPath, g_app.config)) {
        ApplySettingsReload();
        ShowMessage("Settings saved.");
    }
}

bool AnyEnabledDeviceUsesMode(DeviceAutomationMode mode) {
    for (const auto& device : g_app.config.devices) {
        if (device.enabled && device.automation.mode == mode) {
            return true;
        }
    }
    return false;
}

void PersistConfigToDisk() {
    SaveAppConfig(g_app.configPath, g_app.config);
}

void SetEnabledDevicesAutomationMode(DeviceAutomationMode mode) {
    bool changed = false;
    for (auto& device : g_app.config.devices) {
        if (!device.enabled) {
            continue;
        }
        if (device.automation.mode != mode) {
            device.automation.mode = mode;
            changed = true;
        }
    }
    if (!changed) {
        return;
    }

    SyncLegacyFieldsFromDevices(g_app.config);
    PersistConfigToDisk();
    g_app.smart.UpdateConfig(g_app.config);
    if (g_app.smart.HasAutomatedDevices()) {
        StartAutomationTimers();
        g_app.smart.Evaluate();
    }
    RebuildMenu();
}

void SetDeviceAutomationMode(size_t deviceIndex, DeviceAutomationMode mode) {
    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (deviceIndex >= enabledDevices.size()) {
        return;
    }

    const std::string& deviceId = enabledDevices[deviceIndex]->id;
    bool changed = false;
    for (auto& device : g_app.config.devices) {
        if (device.id == deviceId && device.enabled) {
            if (device.automation.mode != mode) {
                device.automation.mode = mode;
                changed = true;
            }
            break;
        }
    }
    if (!changed) {
        return;
    }

    SyncLegacyFieldsFromDevices(g_app.config);
    PersistConfigToDisk();
    g_app.smart.UpdateConfig(g_app.config);
    if (g_app.smart.HasAutomatedDevices()) {
        StartAutomationTimers();
        g_app.smart.Evaluate();
    }
    RebuildMenu();
}

void OnTurnOn(GtkMenuItem*, gpointer) {
    RunPlugAction(false, true, false);
}

void OnTurnOff(GtkMenuItem*, gpointer) {
    RunPlugAction(false, false, false);
}

void OnToggleSmartMode(GtkCheckMenuItem* item, gpointer) {
    if (gtk_check_menu_item_get_active(item)) {
        SetEnabledDevicesAutomationMode(DeviceAutomationMode::Smart);
    } else {
        SetEnabledDevicesAutomationMode(DeviceAutomationMode::Manual);
    }
}

void OnToggleScheduleMode(GtkCheckMenuItem* item, gpointer) {
    if (gtk_check_menu_item_get_active(item)) {
        SetEnabledDevicesAutomationMode(DeviceAutomationMode::Schedule);
    } else {
        SetEnabledDevicesAutomationMode(DeviceAutomationMode::Manual);
    }
}

void OnDeviceTurnOn(GtkMenuItem*, gpointer userData) {
    const int deviceIndex = GPOINTER_TO_INT(userData);
    SetDeviceAutomationMode(static_cast<size_t>(deviceIndex), DeviceAutomationMode::Manual);
    RunPlugAction(false, true, false, deviceIndex);
}

void OnDeviceTurnOff(GtkMenuItem*, gpointer userData) {
    const int deviceIndex = GPOINTER_TO_INT(userData);
    SetDeviceAutomationMode(static_cast<size_t>(deviceIndex), DeviceAutomationMode::Manual);
    RunPlugAction(false, false, false, deviceIndex);
}

void OnDeviceModeManual(GtkMenuItem*, gpointer userData) {
    SetDeviceAutomationMode(static_cast<size_t>(GPOINTER_TO_INT(userData)), DeviceAutomationMode::Manual);
}

void OnDeviceModeSmart(GtkMenuItem*, gpointer userData) {
    SetDeviceAutomationMode(static_cast<size_t>(GPOINTER_TO_INT(userData)), DeviceAutomationMode::Smart);
}

void OnDeviceModeSchedule(GtkMenuItem*, gpointer userData) {
    SetDeviceAutomationMode(static_cast<size_t>(GPOINTER_TO_INT(userData)), DeviceAutomationMode::Schedule);
}

void OnRefresh(GtkMenuItem*, gpointer) {
    RunPlugAction(false, false, true);
}

void UpdateApplyMenuItem() {
    if (!g_applyUpdateItem) {
        return;
    }
    const bool enabled = g_app.pendingUpdate.available && SupportsInAppUpdate(g_app.installKind);
    gtk_widget_set_sensitive(g_applyUpdateItem, enabled);
    if (enabled) {
        const std::string label = "Update to v" + g_app.pendingUpdate.version + "...";
        gtk_menu_item_set_label(GTK_MENU_ITEM(g_applyUpdateItem), label.c_str());
    }
}

void HandleUpdateCheckResult(const UpdateInfo& info, bool showNoUpdateMessage) {
    g_app.pendingUpdate = info;
    UpdateApplyMenuItem();

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
    ShowMessage("DuskPlug " + info.version + " is available. Choose Update from the tray menu.");
}

void OnCheckUpdates(GtkMenuItem*, gpointer) {
    HandleUpdateCheckResult(CheckForUpdates(g_app.installKind), true);
}

void OnApplyUpdate(GtkMenuItem*, gpointer) {
    if (!g_app.pendingUpdate.available || !SupportsInAppUpdate(g_app.installKind)) {
        return;
    }
    const std::string prompt = "Download and install DuskPlug " + g_app.pendingUpdate.version + "?";
    GtkWidget* dialog = gtk_message_dialog_new(
        nullptr,
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "%s",
        prompt.c_str());
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (response != GTK_RESPONSE_YES) {
        return;
    }

    const ApplyUpdateResult result = ApplyUpdate(g_app.pendingUpdate, g_app.installKind);
    if (!result.success) {
        ShowMessage(result.error);
        return;
    }
    if (result.restartScheduled) {
        g_app.smart.SavePersistedState();
        gtk_main_quit();
    }
}

void MaybeBackgroundUpdateCheck() {
    if (!ShouldCheckForUpdatesNow()) {
        return;
    }
    HandleUpdateCheckResult(CheckForUpdates(g_app.installKind), false);
}

void OnToggleLockOff(GtkCheckMenuItem* item, gpointer) {
    if (!g_app.smart.HasAutomatedDevices()) {
        return;
    }
    g_app.smart.SetLockOffEnabled(gtk_check_menu_item_get_active(item) != FALSE);
}

void OnToggleScreenBrightness(GtkCheckMenuItem* item, gpointer) {
    g_app.smart.SetScreenBrightnessEnabled(gtk_check_menu_item_get_active(item) != FALSE);
}

void OnMenuShow(GtkWidget*, gpointer) {
    const bool automated = g_app.smart.HasAutomatedDevices();
    const bool smartActive = AnyEnabledDeviceUsesMode(DeviceAutomationMode::Smart);
    const bool scheduleActive = AnyEnabledDeviceUsesMode(DeviceAutomationMode::Schedule);
    if (g_smartModeItem) {
        g_signal_handlers_block_by_func(g_smartModeItem, reinterpret_cast<gpointer>(OnToggleSmartMode), nullptr);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(g_smartModeItem), smartActive);
        g_signal_handlers_unblock_by_func(g_smartModeItem, reinterpret_cast<gpointer>(OnToggleSmartMode), nullptr);
    }
    if (g_scheduleModeItem) {
        g_signal_handlers_block_by_func(
            g_scheduleModeItem,
            reinterpret_cast<gpointer>(OnToggleScheduleMode),
            nullptr);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(g_scheduleModeItem), scheduleActive);
        g_signal_handlers_unblock_by_func(
            g_scheduleModeItem,
            reinterpret_cast<gpointer>(OnToggleScheduleMode),
            nullptr);
    }
    if (g_lockOffItem) {
        gtk_widget_set_sensitive(g_lockOffItem, automated);
        g_signal_handlers_block_by_func(g_lockOffItem, reinterpret_cast<gpointer>(OnToggleLockOff), nullptr);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(g_lockOffItem), g_app.smart.IsLockOffEnabled());
        g_signal_handlers_unblock_by_func(g_lockOffItem, reinterpret_cast<gpointer>(OnToggleLockOff), nullptr);
    }
    if (g_screenBrightnessItem) {
        gtk_widget_set_sensitive(g_screenBrightnessItem, TRUE);
        g_signal_handlers_block_by_func(
            g_screenBrightnessItem,
            reinterpret_cast<gpointer>(OnToggleScreenBrightness),
            nullptr);
        gtk_check_menu_item_set_active(
            GTK_CHECK_MENU_ITEM(g_screenBrightnessItem),
            g_app.smart.IsScreenBrightnessEnabled());
        g_signal_handlers_unblock_by_func(
            g_screenBrightnessItem,
            reinterpret_cast<gpointer>(OnToggleScreenBrightness),
            nullptr);
    }
}

void OnAutostart(GtkMenuItem*, gpointer) {
    const std::string execPath = GetExeDirectory() + "/duskplug";
    if (InstallLinuxAutostart(execPath)) {
        ShowMessage("DuskPlug will start at login.");
    }
}

void OnQuit(GtkMenuItem*, gpointer) {
    g_app.smart.SavePersistedState();
    gtk_main_quit();
}

void OnActivate(AppIndicator*, const char*, gpointer) {
    RunPlugAction(true, false, false);
}

void RebuildMenu() {
    if (g_menu) {
        gtk_widget_destroy(g_menu);
        g_menu = nullptr;
        g_lockOffItem = nullptr;
        g_screenBrightnessItem = nullptr;
        g_smartModeItem = nullptr;
        g_scheduleModeItem = nullptr;
        g_applyUpdateItem = nullptr;
    }

    g_menu = gtk_menu_new();
    auto add = [&](const char* label, GCallback handler, gpointer userData = nullptr) {
        GtkWidget* item = gtk_menu_item_new_with_label(label);
        g_signal_connect(item, "activate", handler, userData);
        gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), item);
        gtk_widget_show(item);
        return item;
    };

    const auto enabledDevices = GetEnabledDevices(g_app.config);
    if (enabledDevices.size() <= 1) {
        add("Turn On", G_CALLBACK(OnTurnOn));
        add("Turn Off", G_CALLBACK(OnTurnOff));
    } else {
        add("Turn all on", G_CALLBACK(OnTurnOn));
        add("Turn all off", G_CALLBACK(OnTurnOff));
    }

    g_smartModeItem = gtk_check_menu_item_new_with_label("Smart Mode");
    g_signal_connect(g_smartModeItem, "toggled", G_CALLBACK(OnToggleSmartMode), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), g_smartModeItem);
    gtk_widget_show(g_smartModeItem);

    if (enabledDevices.size() > 1) {
        gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
        for (size_t i = 0; i < enabledDevices.size(); ++i) {
            GtkWidget* deviceMenu = gtk_menu_new();
            GtkWidget* deviceItem = gtk_menu_item_new_with_label(enabledDevices[i]->name.c_str());
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(deviceItem), deviceMenu);
            auto addDevice = [&](const char* label, GCallback handler) {
                GtkWidget* item = gtk_menu_item_new_with_label(label);
                g_signal_connect(item, "activate", handler, GINT_TO_POINTER(static_cast<int>(i)));
                gtk_menu_shell_append(GTK_MENU_SHELL(deviceMenu), item);
                gtk_widget_show(item);
            };
            addDevice("Turn On", G_CALLBACK(OnDeviceTurnOn));
            addDevice("Turn Off", G_CALLBACK(OnDeviceTurnOff));
            gtk_menu_shell_append(GTK_MENU_SHELL(deviceMenu), gtk_separator_menu_item_new());
            addDevice("Manual", G_CALLBACK(OnDeviceModeManual));
            addDevice("Smart", G_CALLBACK(OnDeviceModeSmart));
            addDevice("Schedule", G_CALLBACK(OnDeviceModeSchedule));
            gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), deviceItem);
            gtk_widget_show(deviceItem);
        }
    }

    g_scheduleModeItem = gtk_check_menu_item_new_with_label("Schedule Mode");
    g_signal_connect(g_scheduleModeItem, "toggled", G_CALLBACK(OnToggleScheduleMode), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), g_scheduleModeItem);
    gtk_widget_show(g_scheduleModeItem);
    g_lockOffItem = gtk_check_menu_item_new_with_label("Off when locked or sleeping");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(g_lockOffItem), TRUE);
    g_signal_connect(g_lockOffItem, "toggled", G_CALLBACK(OnToggleLockOff), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), g_lockOffItem);
    gtk_widget_show(g_lockOffItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
    g_screenBrightnessItem = gtk_check_menu_item_new_with_label("Adjust screen brightness");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(g_screenBrightnessItem), FALSE);
    g_signal_connect(g_screenBrightnessItem, "toggled", G_CALLBACK(OnToggleScreenBrightness), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), g_screenBrightnessItem);
    gtk_widget_show(g_screenBrightnessItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
    g_signal_connect(g_menu, "show", G_CALLBACK(OnMenuShow), nullptr);
    add("Settings...", G_CALLBACK(OnSettings));
    add("Refresh Status", G_CALLBACK(OnRefresh));
    add("Check for updates...", G_CALLBACK(OnCheckUpdates));
    g_applyUpdateItem = gtk_menu_item_new_with_label("Update to latest...");
    gtk_widget_set_sensitive(g_applyUpdateItem, FALSE);
    g_signal_connect(g_applyUpdateItem, "activate", G_CALLBACK(OnApplyUpdate), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), g_applyUpdateItem);
    gtk_widget_show(g_applyUpdateItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(g_menu), gtk_separator_menu_item_new());
    add("Install Autostart", G_CALLBACK(OnAutostart));
    add("Exit", G_CALLBACK(OnQuit));
    gtk_widget_show_all(g_menu);

    app_indicator_set_menu(g_app.indicator, GTK_MENU(g_menu));
    UpdateApplyMenuItem();
}

}  // namespace

int main(int argc, char** argv) {
    gtk_init(&argc, &argv);

    g_app.appDir = GetExeDirectory();
    g_app.installKind = DetectInstallKind();
    g_app.configPath = ResolveConfigPath(g_app.appDir + "/config.json");
    EnsureConfigFile(g_app.configPath);

    std::string configError;
    if (!LoadConfig(g_app.configPath, g_app.config, configError, false)) {
        ShowMessage(configError);
        return 1;
    }

    g_app.indicator = app_indicator_new("duskplug", "light-off", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(g_app.indicator, APP_INDICATOR_STATUS_ACTIVE);
    g_signal_connect(g_app.indicator, "activate", G_CALLBACK(OnActivate), nullptr);

    g_app.locationService = CreateLinuxLocationService();
    g_app.activityTracker = CreateLinuxActivityTracker();
    g_app.brightnessController = CreateLinuxBrightnessController();

    RebuildMenu();

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
            StartAutomationTimers();
            g_app.smart.Evaluate();
        } else {
            if (g_app.smart.IsScreenBrightnessEnabled()) {
                g_app.smart.Evaluate();
            }
            RunPlugAction(false, false, true);
        }
        MaybeBackgroundUpdateCheck();
    } else {
        ShowMessage("DuskPlug needs your Tuya plug connection details.\nOpen Settings from the tray menu.");
    }

    g_app.pollTimer = g_timeout_add_seconds(30, OnPollTimer, nullptr);
    gtk_main();

    if (g_app.pollTimer) {
        g_source_remove(g_app.pollTimer);
    }
    if (g_app.smartTimer) {
        g_source_remove(g_app.smartTimer);
    }
    if (g_app.lockTimer) {
        g_source_remove(g_app.lockTimer);
    }
    g_app.smart.Shutdown();
    delete g_app.locationService;
    delete g_app.activityTracker;
    delete g_app.brightnessController;
    return 0;
}
