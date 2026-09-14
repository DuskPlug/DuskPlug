#include "platform_macos.h"

#import <CoreLocation/CoreLocation.h>
#import <Foundation/Foundation.h>

#include <string>

@interface DuskPlugLocationDelegate : NSObject <CLLocationManagerDelegate>
@property(nonatomic) CLLocationManager* manager;
@property(nonatomic, assign) bool finished;
@property(nonatomic, assign) bool success;
@property(nonatomic, assign) double latitude;
@property(nonatomic, assign) double longitude;
@property(nonatomic, copy) NSString* errorText;
@end

@implementation DuskPlugLocationDelegate
- (void)locationManager:(CLLocationManager*)manager didUpdateLocations:(NSArray<CLLocation*>*)locations {
    CLLocation* location = locations.lastObject;
    self.latitude = location.coordinate.latitude;
    self.longitude = location.coordinate.longitude;
    self.success = true;
    self.finished = true;
    CFRunLoopStop(CFRunLoopGetCurrent());
}

- (void)locationManager:(CLLocationManager*)manager didFailWithError:(NSError*)error {
    self.errorText = error.localizedDescription;
    self.success = false;
    self.finished = true;
    CFRunLoopStop(CFRunLoopGetCurrent());
}
@end

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

bool RequestCoreLocation(GeoLocation& out, std::string& error) {
    @autoreleasepool {
        DuskPlugLocationDelegate* delegate = [DuskPlugLocationDelegate new];
        delegate.manager = [CLLocationManager new];
        delegate.manager.delegate = delegate;
        [delegate.manager requestWhenInUseAuthorization];
        [delegate.manager requestLocation];

        CFRunLoopRun();
        if (!delegate.success) {
            error = delegate.errorText ? delegate.errorText.UTF8String : "Location request failed";
            return false;
        }
        out.latitude = delegate.latitude;
        out.longitude = delegate.longitude;
        out.fromOs = true;
        return true;
    }
}

}  // namespace

class MacLocationService : public ILocationService {
public:
    bool TryResolve(const AppConfig& config, GeoLocation& out, std::string& error) override {
        if (RequestCoreLocation(out, error)) {
            return true;
        }
        if (ApplyConfigFallback(config, out)) {
            error.clear();
            return true;
        }
        return false;
    }

    LocationPromptResult ResolveWithPrompt(const AppConfig& config, GeoLocation& out, std::string& error) override {
        if (RequestCoreLocation(out, error)) {
            return LocationPromptResult::Success;
        }
        if (ApplyConfigFallback(config, out)) {
            error.clear();
            return LocationPromptResult::Success;
        }
        OpenSettings();
        return LocationPromptResult::OpenedSettings;
    }

    void OpenSettings() override {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_LocationServices"]];
    }
};

ILocationService* CreateMacLocationService() {
    return new MacLocationService();
}
