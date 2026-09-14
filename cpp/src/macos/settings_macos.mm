#include "platform_macos.h"

#include "../config.h"
#include "../coords.h"
#include "../platform_util.h"

#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>

#include <cstring>
#include <string>

static NSTextField* MakeField(NSString* value) {
    NSTextField* field = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 280, 24)];
    field.stringValue = value ?: @"";
    return field;
}

bool ShowMacSettingsDialog(const std::string& configPath, AppConfig& config) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        alert.messageText = @"DuskPlug Settings";
        alert.informativeText = @"Enter your Tuya Cloud credentials and optional location.";
        [alert addButtonWithTitle:@"Save"];
        [alert addButtonWithTitle:@"Cancel"];

        NSView* container = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 420, 248)];
        NSTextField* clientId = MakeField(@(config.clientId.c_str()));
        NSTextField* clientSecret = MakeField(@(config.clientSecret.c_str()));
        NSTextField* deviceId = MakeField(@(config.deviceId.c_str()));
        NSTextField* latitude = MakeField(@(std::to_string(config.latitude).c_str()));
        NSTextField* longitude = MakeField(@(std::to_string(config.longitude).c_str()));
        NSTextField* pasteCoords = MakeField(@"");
        pasteCoords.placeholderString = @"51.4809, -3.2092";
        NSTextField* scheduleOn = MakeField(@(config.scheduleOnTime.c_str()));
        NSTextField* scheduleOff = MakeField(@(config.scheduleOffTime.c_str()));

        NSArray<NSTextField*>* fields = @[clientId, clientSecret, deviceId, latitude, longitude, pasteCoords, scheduleOn, scheduleOff];
        NSArray<NSString*>* labels = @[@"Access ID", @"Access Secret", @"Device ID", @"Latitude", @"Longitude", @"Paste lat, lon", @"Schedule ON", @"Schedule OFF"];
        for (NSUInteger i = 0; i < labels.count; ++i) {
            NSTextField* label = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 218 - i * 28, 120, 22)];
            label.stringValue = labels[i];
            label.editable = NO;
            label.bezeled = NO;
            label.drawsBackground = NO;
            label.selectable = NO;
            fields[i].frame = NSMakeRect(130, 218 - i * 28, 280, 22);
            [container addSubview:label];
            [container addSubview:fields[i]];
        }

        id pasteObserver = [[NSNotificationCenter defaultCenter]
            addObserverForName:NSControlTextDidChangeNotification
                        object:pasteCoords
                         queue:nil
                    usingBlock:^(NSNotification*) {
                        double lat = 0.0;
                        double lon = 0.0;
                        if (ParseLatLonPair(pasteCoords.stringValue.UTF8String ?: "", lat, lon)) {
                            latitude.stringValue = [NSString stringWithFormat:@"%.6f", lat];
                            longitude.stringValue = [NSString stringWithFormat:@"%.6f", lon];
                        }
                    }];

        alert.accessoryView = container;
        const NSModalResponse response = [alert runModal];
        [[NSNotificationCenter defaultCenter] removeObserver:pasteObserver];
        if (response != NSAlertFirstButtonReturn) {
            return false;
        }

        config.clientId = clientId.stringValue.UTF8String;
        config.clientSecret = clientSecret.stringValue.UTF8String;
        config.deviceId = deviceId.stringValue.UTF8String;
        config.baseUrl = config.baseUrl.empty() ? "https://openapi.tuyaeu.com" : config.baseUrl;

        double lat = 0.0;
        double lon = 0.0;
        const char* pasted = pasteCoords.stringValue.UTF8String;
        const char* latText = latitude.stringValue.UTF8String;
        if ((pasted && pasted[0] && ParseLatLonPair(pasted, lat, lon))
            || (latText && std::strchr(latText, ',') && ParseLatLonPair(latText, lat, lon))) {
            config.latitude = lat;
            config.longitude = lon;
        } else {
            config.latitude = atof(latText ? latText : "0");
            config.longitude = atof(longitude.stringValue.UTF8String);
        }
        config.hasLatitude = true;
        config.hasLongitude = true;
        config.scheduleOnTime = scheduleOn.stringValue.UTF8String;
        config.scheduleOffTime = scheduleOff.stringValue.UTF8String;
        config.hasScheduleTimes = true;
        return SaveAppConfig(configPath, config);
    }
}

bool InstallMacAutostart() {
    if (@available(macOS 13.0, *)) {
        NSError* error = nil;
        return [[SMAppService mainAppService] registerAndReturnError:&error];
    }
    return false;
}

bool RemoveMacAutostart() {
    if (@available(macOS 13.0, *)) {
        NSError* error = nil;
        return [[SMAppService mainAppService] unregisterAndReturnError:&error];
    }
    return false;
}
