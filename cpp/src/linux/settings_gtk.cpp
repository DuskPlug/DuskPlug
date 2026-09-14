#include "platform_linux.h"

#include "../config.h"
#include "../coords.h"
#include "../platform_util.h"

#include <gtk/gtk.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

GtkWidget* AddLabeledEntry(GtkWidget* grid, int row, const char* label, const char* value) {
    GtkWidget* lbl = gtk_label_new(label);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(entry), value ? value : "");
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry, 1, row, 1, 1);
    return entry;
}

const char* DataCenterLabel(int index) {
    switch (index) {
    case 1: return "Western Europe";
    case 2: return "US East";
    case 3: return "US West";
    case 4: return "Singapore";
    case 5: return "India";
    default: return "Central Europe";
    }
}

void SetCoordEntry(GtkWidget* entry, double value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", value);
    gtk_entry_set_text(GTK_ENTRY(entry), buf);
}

gboolean ApplyPastedCoords(GtkWidget* paste, GtkWidget* latitude, GtkWidget* longitude) {
    double lat = 0.0;
    double lon = 0.0;
    if (!ParseLatLonPair(gtk_entry_get_text(GTK_ENTRY(paste)), lat, lon)) {
        return FALSE;
    }
    SetCoordEntry(latitude, lat);
    SetCoordEntry(longitude, lon);
    return TRUE;
}

void OnPasteCoordsChanged(GtkEditable*, gpointer user_data) {
    auto** fields = static_cast<GtkWidget**>(user_data);
    ApplyPastedCoords(fields[0], fields[1], fields[2]);
}

}  // namespace

bool ShowLinuxSettingsDialog(const std::string& configPath, AppConfig& config) {
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "DuskPlug Settings",
        nullptr,
        GTK_DIALOG_MODAL,
        "_Cancel",
        GTK_RESPONSE_CANCEL,
        "_Save",
        GTK_RESPONSE_OK,
        nullptr);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 520, 460);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget* clientId = AddLabeledEntry(grid, 0, "Access ID", config.clientId.c_str());
    GtkWidget* clientSecret = AddLabeledEntry(grid, 1, "Access Secret", config.clientSecret.c_str());
    GtkWidget* deviceId = AddLabeledEntry(grid, 2, "Device ID", config.deviceId.c_str());

    GtkWidget* dataCenter = gtk_combo_box_text_new();
    for (int i = 0; i < 6; ++i) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dataCenter), DataCenterLabel(i));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(dataCenter), BaseUrlToDataCenterIndex(config.baseUrl));
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Data center"), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), dataCenter, 1, 3, 1, 1);

    char latBuf[32];
    char lonBuf[32];
    snprintf(latBuf, sizeof(latBuf), "%.6f", config.latitude);
    snprintf(lonBuf, sizeof(lonBuf), "%.6f", config.longitude);
    GtkWidget* latitude = AddLabeledEntry(grid, 4, "Latitude", latBuf);
    GtkWidget* longitude = AddLabeledEntry(grid, 5, "Longitude", lonBuf);
    GtkWidget* pasteCoords = AddLabeledEntry(grid, 6, "Paste from Google Maps", "");
    gtk_entry_set_placeholder_text(GTK_ENTRY(pasteCoords), "51.4809, -3.2092");
    GtkWidget* locationFields[] = {pasteCoords, latitude, longitude};
    g_signal_connect(pasteCoords, "changed", G_CALLBACK(OnPasteCoordsChanged), locationFields);
    GtkWidget* scheduleOn = AddLabeledEntry(grid, 7, "Schedule ON", config.scheduleOnTime.c_str());
    GtkWidget* scheduleOff = AddLabeledEntry(grid, 8, "Schedule OFF", config.scheduleOffTime.c_str());
    GtkWidget* lockOff = AddLabeledEntry(grid, 9, "Lock-off seconds", std::to_string(config.lockOffSeconds).c_str());

    gtk_widget_show_all(dialog);
    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response != GTK_RESPONSE_OK) {
        gtk_widget_destroy(dialog);
        return false;
    }

    config.clientId = gtk_entry_get_text(GTK_ENTRY(clientId));
    config.clientSecret = gtk_entry_get_text(GTK_ENTRY(clientSecret));
    config.deviceId = gtk_entry_get_text(GTK_ENTRY(deviceId));
    config.baseUrl = DataCenterIndexToBaseUrl(gtk_combo_box_get_active(GTK_COMBO_BOX(dataCenter)));

    double latitudeValue = 0.0;
    double longitudeValue = 0.0;
    const char* pasted = gtk_entry_get_text(GTK_ENTRY(pasteCoords));
    const char* latText = gtk_entry_get_text(GTK_ENTRY(latitude));
    if ((pasted && pasted[0] && ParseLatLonPair(pasted, latitudeValue, longitudeValue))
        || (latText && std::strchr(latText, ',') && ParseLatLonPair(latText, latitudeValue, longitudeValue))) {
        config.latitude = latitudeValue;
        config.longitude = longitudeValue;
    } else {
        config.latitude = std::atof(latText ? latText : "0");
        config.longitude = std::atof(gtk_entry_get_text(GTK_ENTRY(longitude)));
    }
    config.hasLatitude = true;
    config.hasLongitude = true;
    config.scheduleOnTime = gtk_entry_get_text(GTK_ENTRY(scheduleOn));
    config.scheduleOffTime = gtk_entry_get_text(GTK_ENTRY(scheduleOff));
    config.lockOffSeconds = std::atoi(gtk_entry_get_text(GTK_ENTRY(lockOff)));
    config.hasScheduleTimes = true;

    const bool saved = SaveAppConfig(configPath, config);
    gtk_widget_destroy(dialog);
    return saved;
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
