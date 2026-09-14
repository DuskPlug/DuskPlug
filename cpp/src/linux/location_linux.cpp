#include "platform_linux.h"

#include "../location_service.h"

#include <geoclue/simple.h>
#include <gio/gio.h>

namespace {

bool ApplyConfigFallback(const AppConfig& config, GeoLocation& out) {
    if (!config.hasLatitude || !config.hasLongitude) {
        return false;
    }
    out.latitude = config.latitude;
    out.longitude = config.longitude;
    out.fromOs = false;
    return true;
}

bool TryGeoclue(GeoLocation& out, std::string& error) {
    GError* gerror = nullptr;
    GeoclueSimple* simple = geoclue_simple_new_sync(
        "duskplug",
        GEOCLUE_ACCURACY_CITY,
        nullptr,
        &gerror);
    if (!simple) {
        error = gerror ? gerror->message : "GeoClue unavailable";
        if (gerror) {
            g_error_free(gerror);
        }
        return false;
    }

    GeoclueLocation* location = geoclue_simple_get_location(simple);
    out.latitude = geoclue_location_get_latitude(location);
    out.longitude = geoclue_location_get_longitude(location);
    out.fromOs = true;
    g_object_unref(simple);
    return true;
}

}  // namespace

class LinuxLocationService : public ILocationService {
public:
    bool TryResolve(const AppConfig& config, GeoLocation& out, std::string& error) override {
        if (TryGeoclue(out, error)) {
            return true;
        }
        if (ApplyConfigFallback(config, out)) {
            error.clear();
            return true;
        }
        if (error.empty()) {
            error = "Location unavailable. Set latitude and longitude in Settings.";
        }
        return false;
    }

    LocationPromptResult ResolveWithPrompt(const AppConfig& config, GeoLocation& out, std::string& error) override {
        if (TryGeoclue(out, error)) {
            return LocationPromptResult::Success;
        }
        if (ApplyConfigFallback(config, out)) {
            error.clear();
            return LocationPromptResult::Success;
        }
        OpenSettings();
        error = "Enable GeoClue location services or enter coordinates in Settings.";
        return LocationPromptResult::OpenedSettings;
    }

    void OpenSettings() override {
        g_spawn_command_line_async(
            "xdg-open 'https://wiki.gnome.org/Projects/GeoClue'",
            nullptr);
    }
};

ILocationService* CreateLinuxLocationService() {
    return new LinuxLocationService();
}
