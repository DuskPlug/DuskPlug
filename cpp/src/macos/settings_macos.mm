#include "platform_macos.h"

#include "../config.h"
#include "../platform_util.h"
#include "../settings_page.h"

#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>
#import <WebKit/WebKit.h>

#include <string>

@interface DuskPlugSettingsHost : NSObject <WKScriptMessageHandler, NSWindowDelegate>
@property(nonatomic, assign) AppConfig* config;
@property(nonatomic, assign) const std::string* configPath;
@property(nonatomic, assign) BOOL saved;
@property(nonatomic, assign) WKWebView* webView;
@property(nonatomic, assign) NSWindow* window;
- (void)evalScript:(const std::string&)script;
- (void)handleMessage:(const std::string&)message;
@end

@implementation DuskPlugSettingsHost

- (void)evalScript:(const std::string&)script {
    if (!self.webView || script.empty()) {
        return;
    }
    NSString* js = [NSString stringWithUTF8String:script.c_str()];
    [self.webView evaluateJavaScript:js completionHandler:nil];
}

- (void)handleMessage:(const std::string&)message {
    if (!self.config || !self.configPath) {
        return;
    }
    const SettingsWebResult result = HandleSettingsWebMessage(message, *self.configPath, *self.config);
    switch (result.kind) {
    case SettingsWebResult::Kind::Saved:
        self.saved = YES;
        [NSApp stopModalWithCode:NSModalResponseOK];
        [self.window close];
        break;
    case SettingsWebResult::Kind::Cancel:
        [NSApp stopModalWithCode:NSModalResponseCancel];
        [self.window close];
        break;
    case SettingsWebResult::Kind::DetectLocation: {
        double latitude = 0.0;
        double longitude = 0.0;
        std::string error;
        if (RequestMacLocation(latitude, longitude, error)) {
            [self evalScript:JsCallSetLocation(latitude, longitude)];
        } else {
            [self evalScript:JsCallShowError(
                                 error.empty() ? "Location unavailable. Allow Location Services or paste coordinates."
                                               : error,
                                 true)];
        }
        break;
    }
    case SettingsWebResult::Kind::OpenLocationSettings:
        OpenMacLocationSettings();
        break;
    case SettingsWebResult::Kind::SetLocation:
        [self evalScript:JsCallSetLocation(result.latitude, result.longitude)];
        break;
    case SettingsWebResult::Kind::RunScript:
        [self evalScript:result.script];
        break;
    case SettingsWebResult::Kind::None:
    default:
        break;
    }
}

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
    (void)userContentController;
    std::string body;
    if ([message.body isKindOfClass:[NSString class]]) {
        body = [message.body UTF8String] ?: "";
    } else if (message.body) {
        NSError* error = nil;
        NSData* data = [NSJSONSerialization dataWithJSONObject:message.body options:0 error:&error];
        if (data) {
            NSString* text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
            body = text.UTF8String ?: "";
        }
    }
    if (!body.empty()) {
        [self handleMessage:body];
    }
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    (void)sender;
    [NSApp stopModalWithCode:NSModalResponseCancel];
    return YES;
}

@end

bool ShowMacSettingsDialog(const std::string& configPath, AppConfig& config) {
    @autoreleasepool {
        const std::string html = LoadSettingsHtml();
        if (html.empty()) {
            NSAlert* alert = [[NSAlert alloc] init];
            alert.messageText = @"DuskPlug Settings";
            alert.informativeText = @"Could not find settings.html in the app bundle.";
            [alert runModal];
            return false;
        }

        const std::string page = InjectSettingsBoot(html, BuildSettingsBootJson(config, "macos"));

        WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
        DuskPlugSettingsHost* host = [[DuskPlugSettingsHost alloc] init];
        host.config = &config;
        host.configPath = &configPath;
        [configuration.userContentController addScriptMessageHandler:host name:@"duskplug"];

        NSRect frame = NSMakeRect(0, 0, 820, 920);
        WKWebView* webView = [[WKWebView alloc] initWithFrame:frame configuration:configuration];
        host.webView = webView;
        NSString* htmlString = [NSString stringWithUTF8String:page.c_str()];
        [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"https://duskplug.local/"]];

        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                                    | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        window.title = @"DuskPlug Settings";
        window.contentView = webView;
        window.delegate = host;
        window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
        host.window = window;
        [window center];
        [window makeKeyAndOrderFront:nil];

        const NSModalResponse response = [NSApp runModalForWindow:window];
        [configuration.userContentController removeScriptMessageHandlerForName:@"duskplug"];
        return host.saved || response == NSModalResponseOK;
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
