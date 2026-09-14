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
GtkWidget* g_applyUpdateItem = nullptr;

void UpdateTrayDisplay(bool on);
void RunPlugAction(bool toggle, bool setOn, bool statusOnly);
void ExitAutomationMode();
void StartAutomationTimers();

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

void UpdateTrayDisplay(bool on) {
    g_app.hasKnownState = true;
    g_app.knownOn = on;
    g_app.smart.SetKnownPlugState(on);

    const char* iconName = "light-off";
    const char* label = "DuskPlug: OFF";
    if (g_app.smart.IsScheduleEnabled() || g_app.smart.IsEnabled()) {
        iconName = on ? "light-smart-on" : "light-smart-off";
        label = on ? "DuskPlug: AUTO — ON" : "DuskPlug: AUTO — OFF";
    } else {
        iconName = on ? "light-on" : "light-off";
        label = on ? "DuskPlug: ON" : "DuskPlug: OFF";
    }

    app_indicator_set_icon_full(g_app.indicator, iconName, label);
    app_indicator_set_status(g_app.indicator, APP_INDICATOR_STATUS_ACTIVE);
}

void RunPlugAction(bool toggle, bool setOn, bool statusOnly) {
    if (g_app.busy || !g_app.client) {
        return;
    }
    if (!statusOnly) {
        ExitAutomationMode();
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

void ExitAutomationMode() {
    if (!g_app.smart.IsAutomationEnabled()) {
        return;
    }
    if (g_app.smartTimer) {
        g_source_remove(g_app.smartTimer);
        g_app.smartTimer = 0;
    }
    if (g_app.lockTimer) {
        g_source_remove(g_app.lockTimer);
        g_app.lockTimer = 0;
    }
    g_app.smart.Disable();
}

gboolean OnPollTimer(gpointer) {
    if (!g_app.smart.IsAutomationEnabled()) {
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
    if (g_app.smart.IsAutomationEnabled()) {
        g_app.smart.Evaluate();
    }
}

void OnSettings(GtkMenuItem*, gpointer) {
    if (ShowLinuxSettingsDialog(g_app.configPath, g_app.config)) {
        ApplySettingsReload();
        ShowMessage("Settings saved.");
    }
}

void OnToggleSmart(GtkMenuItem*, gpointer) {
    if (g_app.smart.IsEnabled()) {
        ExitAutomationMode();
        if (g_app.hasKnownState) {
            UpdateTrayDisplay(g_app.knownOn);
        }
        return;
    }
    std::string error;
    if (!g_app.smart.Enable(error, true)) {
        if (!error.empty()) {
            ShowMessage(error);
        }
        return;
    }
    StartAutomationTimers();
    g_app.smart.Evaluate();
}

void OnToggleSchedule(GtkMenuItem*, gpointer) {
    if (g_app.smart.IsScheduleEnabled()) {
        ExitAutomationMode();
        if (g_app.hasKnownState) {
            UpdateTrayDisplay(g_app.knownOn);
        }
        return;
    }
    std::string error;
    if (!g_app.smart.EnableSchedule(error)) {
        if (!error.empty()) {
            ShowMessage(error);
        }
        return;
    }
    StartAutomationTimers();
    g_app.smart.Evaluate();
}

void OnTurnOn(GtkMenuItem*, gpointer) {
    RunPlugAction(false, true, false);
}

void OnTurnOff(GtkMenuItem*, gpointer) {
    RunPlugAction(false, false, false);
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

GtkWidget* g_lockOffItem = nullptr;

void OnToggleLockOff(GtkCheckMenuItem* item, gpointer) {
    if (!g_app.smart.IsAutomationEnabled()) {
        return;
    }
    g_app.smart.SetLockOffEnabled(gtk_check_menu_item_get_active(item) != FALSE);
}

void OnMenuShow(GtkWidget*, gpointer) {
    if (!g_lockOffItem) {
        return;
    }
    const bool automation = g_app.smart.IsAutomationEnabled();
    gtk_widget_set_sensitive(g_lockOffItem, automation);
    g_signal_handlers_block_by_func(g_lockOffItem, reinterpret_cast<gpointer>(OnToggleLockOff), nullptr);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(g_lockOffItem), g_app.smart.IsLockOffEnabled());
    g_signal_handlers_unblock_by_func(g_lockOffItem, reinterpret_cast<gpointer>(OnToggleLockOff), nullptr);
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

GtkWidget* BuildMenu() {
    GtkWidget* menu = gtk_menu_new();
    auto add = [&](const char* label, GCallback handler) {
        GtkWidget* item = gtk_menu_item_new_with_label(label);
        g_signal_connect(item, "activate", handler, nullptr);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        gtk_widget_show(item);
    };

    add("Turn On", G_CALLBACK(OnTurnOn));
    add("Turn Off", G_CALLBACK(OnTurnOff));
    add("Smart Mode", G_CALLBACK(OnToggleSmart));
    add("Schedule Mode", G_CALLBACK(OnToggleSchedule));
    g_lockOffItem = gtk_check_menu_item_new_with_label("Off when locked or sleeping");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(g_lockOffItem), TRUE);
    g_signal_connect(g_lockOffItem, "toggled", G_CALLBACK(OnToggleLockOff), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), g_lockOffItem);
    gtk_widget_show(g_lockOffItem);
    g_signal_connect(menu, "show", G_CALLBACK(OnMenuShow), nullptr);
    add("Settings...", G_CALLBACK(OnSettings));
    add("Refresh Status", G_CALLBACK(OnRefresh));
    add("Check for updates...", G_CALLBACK(OnCheckUpdates));
    g_applyUpdateItem = gtk_menu_item_new_with_label("Update to latest...");
    gtk_widget_set_sensitive(g_applyUpdateItem, FALSE);
    g_signal_connect(g_applyUpdateItem, "activate", G_CALLBACK(OnApplyUpdate), nullptr);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), g_applyUpdateItem);
    gtk_widget_show(g_applyUpdateItem);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    add("Install Autostart", G_CALLBACK(OnAutostart));
    add("Exit", G_CALLBACK(OnQuit));
    gtk_widget_show_all(menu);
    return menu;
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
    app_indicator_set_menu(g_app.indicator, GTK_MENU(BuildMenu()));
    app_indicator_set_status(g_app.indicator, APP_INDICATOR_STATUS_ACTIVE);
    g_signal_connect(g_app.indicator, "activate", G_CALLBACK(OnActivate), nullptr);

    g_app.locationService = CreateLinuxLocationService();
    g_app.activityTracker = CreateLinuxActivityTracker();

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
            StartAutomationTimers();
            g_app.smart.Evaluate();
        } else {
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
    ExitAutomationMode();
    g_app.smart.Shutdown();
    delete g_app.locationService;
    delete g_app.activityTracker;
    return 0;
}
