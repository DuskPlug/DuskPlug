#include "platform_linux.h"

#include "../config.h"
#include "../platform_util.h"
#include "../settings_page.h"

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

struct SettingsHost {
    GtkWidget* dialog = nullptr;
    GtkWidget* webview = nullptr;
    std::string configPath;
    AppConfig* config = nullptr;
    bool saved = false;
};

GtkWidget* gSettingsDialog = nullptr;

void ClearSettingsDialogPointer(GtkWidget*, gpointer) {
    gSettingsDialog = nullptr;
}

void EvalScript(SettingsHost* host, const std::string& script) {
    if (!host || !host->webview || script.empty()) {
        return;
    }
#if WEBKIT_CHECK_VERSION(2, 40, 0)
    webkit_web_view_evaluate_javascript(
        WEBKIT_WEB_VIEW(host->webview),
        script.c_str(),
        static_cast<gssize>(script.size()),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
#else
    webkit_web_view_run_javascript(WEBKIT_WEB_VIEW(host->webview), script.c_str(), nullptr, nullptr, nullptr);
#endif
}

void ApplyResult(SettingsHost* host, const SettingsWebResult& result) {
    if (!host || !host->dialog) {
        return;
    }
    switch (result.kind) {
    case SettingsWebResult::Kind::Saved:
        host->saved = true;
        gtk_dialog_response(GTK_DIALOG(host->dialog), GTK_RESPONSE_OK);
        break;
    case SettingsWebResult::Kind::Cancel:
        gtk_dialog_response(GTK_DIALOG(host->dialog), GTK_RESPONSE_CANCEL);
        break;
    case SettingsWebResult::Kind::DetectLocation: {
        double latitude = 0.0;
        double longitude = 0.0;
        std::string error;
        if (RequestLinuxLocation(latitude, longitude, error)) {
            EvalScript(host, JsCallSetLocation(latitude, longitude));
        } else {
            EvalScript(
                host,
                JsCallShowError(
                    error.empty() ? "Location unavailable. Enable GeoClue or paste coordinates." : error,
                    true));
        }
        break;
    }
    case SettingsWebResult::Kind::OpenLocationSettings:
        OpenLinuxLocationSettings();
        break;
    case SettingsWebResult::Kind::SetLocation:
        EvalScript(host, JsCallSetLocation(result.latitude, result.longitude));
        break;
    case SettingsWebResult::Kind::RunScript:
        EvalScript(host, result.script);
        break;
    case SettingsWebResult::Kind::None:
    default:
        break;
    }
}

#if WEBKIT_CHECK_VERSION(2, 40, 0)
void OnScriptMessage(WebKitUserContentManager*, JSCValue* value, gpointer userData) {
    auto* host = static_cast<SettingsHost*>(userData);
    if (!host || !host->config || !value || !jsc_value_is_string(value)) {
        return;
    }
    char* text = jsc_value_to_string(value);
    if (!text) {
        return;
    }
    const std::string message = text;
    g_free(text);
    ApplyResult(host, HandleSettingsWebMessage(message, host->configPath, *host->config));
}
#else
void OnScriptMessage(WebKitUserContentManager*, WebKitJavascriptResult* jsResult, gpointer userData) {
    auto* host = static_cast<SettingsHost*>(userData);
    if (!host || !host->config || !jsResult) {
        return;
    }

    std::string message;
#if WEBKIT_CHECK_VERSION(2, 22, 0)
    JSCValue* value = webkit_javascript_result_get_js_value(jsResult);
    if (value && jsc_value_is_string(value)) {
        char* text = jsc_value_to_string(value);
        if (text) {
            message = text;
            g_free(text);
        }
    }
#endif
    if (message.empty()) {
        return;
    }
    ApplyResult(host, HandleSettingsWebMessage(message, host->configPath, *host->config));
}
#endif

gboolean OnContextMenu(
    WebKitWebView*,
    GtkWidget*,
    WebKitHitTestResult*,
    gboolean,
    gpointer) {
    return TRUE;
}

}  // namespace

bool ShowLinuxSettingsDialog(const std::string& configPath, AppConfig& config) {
    if (gSettingsDialog && GTK_IS_WIDGET(gSettingsDialog)) {
        gtk_window_present(GTK_WINDOW(gSettingsDialog));
        return false;
    }

    const std::string html = LoadSettingsHtml();
    if (html.empty()) {
        GtkWidget* error = gtk_message_dialog_new(
            nullptr,
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "Could not find assets/settings.html next to duskplug.");
        gtk_dialog_run(GTK_DIALOG(error));
        gtk_widget_destroy(error);
        return false;
    }

    const std::string page = PrepareSettingsHtml(html, BuildSettingsBootJson(config, "linux"));

    SettingsHost host{};
    host.configPath = configPath;
    host.config = &config;

    host.dialog = gtk_dialog_new();
    gSettingsDialog = host.dialog;
    g_signal_connect(host.dialog, "destroy", G_CALLBACK(ClearSettingsDialogPointer), nullptr);
    gtk_window_set_title(GTK_WINDOW(host.dialog), "DuskPlug Settings");
    gtk_window_set_default_size(GTK_WINDOW(host.dialog), 820, 920);
    gtk_window_set_modal(GTK_WINDOW(host.dialog), TRUE);

    WebKitUserContentManager* manager = webkit_user_content_manager_new();
    webkit_user_content_manager_register_script_message_handler(manager, "duskplug");
    g_signal_connect(manager, "script-message-received::duskplug", G_CALLBACK(OnScriptMessage), &host);

    host.webview = webkit_web_view_new_with_user_content_manager(manager);
    WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(host.webview));
    webkit_settings_set_enable_developer_extras(settings, FALSE);
    g_signal_connect(host.webview, "context-menu", G_CALLBACK(OnContextMenu), nullptr);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(host.dialog));
    gtk_box_set_spacing(GTK_BOX(content), 0);
    gtk_container_add(GTK_CONTAINER(content), host.webview);
    gtk_widget_set_hexpand(host.webview, TRUE);
    gtk_widget_set_vexpand(host.webview, TRUE);

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(host.webview), page.c_str(), "https://duskplug.local/");
    gtk_widget_show_all(host.dialog);
    gtk_dialog_run(GTK_DIALOG(host.dialog));
    gtk_widget_destroy(host.dialog);
    g_object_unref(manager);
    return host.saved;
}

bool InstallLinuxAutostart(const std::string& execPath) {
    const char* home = getenv("HOME");
    if (!home) {
        return false;
    }
    const std::string autostartDir = std::string(home) + "/.config/autostart";
    EnsureDirectoryExists(autostartDir);
    const std::string desktopPath = autostartDir + "/duskplug.desktop";
    const std::string contents =
        "[Desktop Entry]\nType=Application\nName=DuskPlug\nExec=" + execPath + "\nX-GNOME-Autostart-enabled=true\n";
    return WriteTextFile(desktopPath, contents);
}

bool RemoveLinuxAutostart() {
    const std::string desktopPath = GetAppDataDir() + "/../autostart/duskplug.desktop";
#ifdef _WIN32
    return false;
#else
    return remove(desktopPath.c_str()) == 0;
#endif
}
