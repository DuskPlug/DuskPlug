#include "settings_dialog.h"

#include "location_win.h"
#include "platform_util.h"
#include "settings_page.h"

#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>

#include <WebView2.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

using CreateCoreWebView2EnvironmentWithOptionsFn = HRESULT(STDMETHODCALLTYPE*)(
    PCWSTR browserExecutableFolder,
    PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions* environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* environmentCreatedHandler);

namespace {

constexpr wchar_t kSettingsClass[] = L"DuskPlugSettingsHtml";

HWND g_settingsHwnd = nullptr;

class ComHandlerBase {
public:
    ComHandlerBase() = default;
    virtual ~ComHandlerBase() = default;

    ULONG AddRefImpl() { return static_cast<ULONG>(InterlockedIncrement(&ref_)); }
    ULONG ReleaseImpl() {
        const ULONG ref = static_cast<ULONG>(InterlockedDecrement(&ref_));
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

private:
    volatile LONG ref_ = 1;
};

HRESULT QuerySelf(REFIID riid, REFIID interfaceId, void** ppv, IUnknown* self) {
    if (!ppv) {
        return E_POINTER;
    }
    if (riid == IID_IUnknown || riid == interfaceId) {
        *ppv = self;
        self->AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

struct SettingsHost {
    HWND hwnd = nullptr;
    std::wstring configPath;
    AppConfig* config = nullptr;
    std::wstring html;
    ICoreWebView2Controller* controller = nullptr;
    ICoreWebView2* webview = nullptr;
    bool saved = false;
    bool ready = false;
    bool alive = true;

    void Eval(const std::string& script);
    void HandleMessage(const std::string& message);
    void Resize();
    void Close();
};

class ScriptDoneHandler : public ICoreWebView2ExecuteScriptCompletedHandler, public ComHandlerBase {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        return QuerySelf(
            riid,
            IID_ICoreWebView2ExecuteScriptCompletedHandler,
            ppv,
            static_cast<ICoreWebView2ExecuteScriptCompletedHandler*>(this));
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AddRefImpl(); }
    ULONG STDMETHODCALLTYPE Release() override { return ReleaseImpl(); }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT, LPCWSTR) override { return S_OK; }
};

class MessageHandler : public ICoreWebView2WebMessageReceivedEventHandler, public ComHandlerBase {
public:
    explicit MessageHandler(SettingsHost* host) : host_(host) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        return QuerySelf(
            riid,
            IID_ICoreWebView2WebMessageReceivedEventHandler,
            ppv,
            static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this));
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AddRefImpl(); }
    ULONG STDMETHODCALLTYPE Release() override { return ReleaseImpl(); }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        if (!host_ || !args) {
            return S_OK;
        }
        LPWSTR message = nullptr;
        if (FAILED(args->TryGetWebMessageAsString(&message)) || !message) {
            return S_OK;
        }
        const std::string utf8 = WideToUtf8(message);
        CoTaskMemFree(message);
        host_->HandleMessage(utf8);
        return S_OK;
    }

private:
    SettingsHost* host_ = nullptr;
};

class ControllerReadyHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, public ComHandlerBase {
public:
    explicit ControllerReadyHandler(SettingsHost* host) : host_(host) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        return QuerySelf(
            riid,
            IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
            ppv,
            static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this));
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AddRefImpl(); }
    ULONG STDMETHODCALLTYPE Release() override { return ReleaseImpl(); }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) override;

private:
    SettingsHost* host_ = nullptr;
};

class EnvironmentReadyHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, public ComHandlerBase {
public:
    explicit EnvironmentReadyHandler(SettingsHost* host) : host_(host) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        return QuerySelf(
            riid,
            IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
            ppv,
            static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this));
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return AddRefImpl(); }
    ULONG STDMETHODCALLTYPE Release() override { return ReleaseImpl(); }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT errorCode, ICoreWebView2Environment* env) override {
        if (!host_ || FAILED(errorCode) || !env || !host_->hwnd) {
            if (host_ && host_->hwnd) {
                MessageBoxW(
                    host_->hwnd,
                    L"Could not start the settings page (WebView2 environment failed).",
                    L"DuskPlug — Settings",
                    MB_ICONERROR | MB_OK);
                DestroyWindow(host_->hwnd);
            }
            return S_OK;
        }
        env->CreateCoreWebView2Controller(host_->hwnd, new ControllerReadyHandler(host_));
        return S_OK;
    }

private:
    SettingsHost* host_ = nullptr;
};

void SettingsHost::Eval(const std::string& script) {
    if (!webview || script.empty()) {
        return;
    }
    webview->ExecuteScript(Utf8ToWide(script).c_str(), new ScriptDoneHandler());
}

void SettingsHost::HandleMessage(const std::string& message) {
    if (!config) {
        return;
    }
    const SettingsWebResult result = HandleSettingsWebMessage(message, WideToUtf8(configPath), *config);
    switch (result.kind) {
    case SettingsWebResult::Kind::Saved:
        saved = true;
        Close();
        break;
    case SettingsWebResult::Kind::Cancel:
        Close();
        break;
    case SettingsWebResult::Kind::DetectLocation: {
        double latitude = 0.0;
        double longitude = 0.0;
        if (RequestWindowsLocation(hwnd, latitude, longitude)) {
            Eval(JsCallSetLocation(latitude, longitude));
        } else {
            Eval(JsCallShowError(
                "Windows did not provide a location. Turn on Location services and allow desktop apps, then try again.",
                true));
        }
        break;
    }
    case SettingsWebResult::Kind::OpenLocationSettings:
        OpenWindowsLocationSettings();
        break;
    case SettingsWebResult::Kind::SetLocation:
        Eval(JsCallSetLocation(result.latitude, result.longitude));
        break;
    case SettingsWebResult::Kind::RunScript:
        Eval(result.script);
        break;
    case SettingsWebResult::Kind::None:
    default:
        break;
    }
}

void SettingsHost::Resize() {
    if (!controller || !hwnd) {
        return;
    }
    RECT bounds{};
    GetClientRect(hwnd, &bounds);
    controller->put_Bounds(bounds);
}

void SettingsHost::Close() {
    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

HRESULT ControllerReadyHandler::Invoke(HRESULT errorCode, ICoreWebView2Controller* controller) {
    if (!host_ || FAILED(errorCode) || !controller) {
        if (host_ && host_->hwnd) {
            MessageBoxW(
                host_->hwnd,
                L"Could not create the settings page view.",
                L"DuskPlug — Settings",
                MB_ICONERROR | MB_OK);
            DestroyWindow(host_->hwnd);
        }
        return S_OK;
    }

    host_->controller = controller;
    host_->controller->AddRef();
    host_->controller->get_CoreWebView2(&host_->webview);
    if (!host_->webview) {
        DestroyWindow(host_->hwnd);
        return S_OK;
    }

    ICoreWebView2Settings* settings = nullptr;
    if (SUCCEEDED(host_->webview->get_Settings(&settings)) && settings) {
        settings->put_AreDefaultContextMenusEnabled(FALSE);
        settings->put_AreDevToolsEnabled(FALSE);
        settings->put_IsStatusBarEnabled(FALSE);
        settings->Release();
    }

    ICoreWebView2Controller2* controller2 = nullptr;
    if (SUCCEEDED(host_->controller->QueryInterface(IID_ICoreWebView2Controller2, reinterpret_cast<void**>(&controller2)))
        && controller2) {
        COREWEBVIEW2_COLOR color{255, 12, 16, 24};
        controller2->put_DefaultBackgroundColor(color);
        controller2->Release();
    }

    EventRegistrationToken token{};
    auto* messageHandler = new MessageHandler(host_);
    host_->webview->add_WebMessageReceived(messageHandler, &token);
    messageHandler->Release();
    host_->Resize();
    host_->controller->put_IsVisible(TRUE);
    host_->webview->NavigateToString(host_->html.c_str());
    host_->ready = true;
    return S_OK;
}

void ApplyDarkTitleBar(HWND hwnd) {
    BOOL value = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}

void ShowWebView2InstallPrompt(HWND owner) {
    const int choice = MessageBoxW(
        owner,
        L"DuskPlug Settings needs the Microsoft Edge WebView2 Runtime.\n\n"
        L"Open the download page now?",
        L"DuskPlug — Settings",
        MB_YESNO | MB_ICONWARNING);
    if (choice == IDYES) {
        ShellExecuteW(owner, L"open", L"https://go.microsoft.com/fwlink/p/?LinkId=2124703", nullptr, nullptr, SW_SHOWNORMAL);
    }
}

HMODULE LoadWebView2Loader() {
    const std::wstring path = Utf8ToWide(GetExeDirectory()) + L"\\WebView2Loader.dll";
    HMODULE module = LoadLibraryW(path.c_str());
    if (module) {
        return module;
    }
    return LoadLibraryW(L"WebView2Loader.dll");
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* host = reinterpret_cast<SettingsHost*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_NCCREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        host = reinterpret_cast<SettingsHost*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
        if (host) {
            host->hwnd = hwnd;
        }
        break;
    }
    case WM_SIZE:
        if (host) {
            host->Resize();
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (host) {
            if (host->controller) {
                host->controller->Close();
                host->controller->Release();
                host->controller = nullptr;
            }
            if (host->webview) {
                host->webview->Release();
                host->webview = nullptr;
            }
            if (g_settingsHwnd == hwnd) {
                g_settingsHwnd = nullptr;
            }
            host->hwnd = nullptr;
            host->alive = false;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

bool IsSettingsDialogOpen() {
    return g_settingsHwnd != nullptr && IsWindow(g_settingsHwnd);
}

void FocusSettingsDialog() {
    if (!IsSettingsDialogOpen()) {
        return;
    }
    if (IsIconic(g_settingsHwnd)) {
        ShowWindow(g_settingsHwnd, SW_RESTORE);
    }
    ShowWindow(g_settingsHwnd, SW_SHOW);
    SetForegroundWindow(g_settingsHwnd);
}

bool ShowSettingsDialog(HWND owner, const std::wstring& configPath, AppConfig& config) {
    if (IsSettingsDialogOpen()) {
        FocusSettingsDialog();
        return false;
    }

    const std::string htmlUtf8 = LoadSettingsHtml();
    if (htmlUtf8.empty()) {
        MessageBoxW(
            owner,
            L"Could not find assets\\settings.html next to DuskPlug.exe.",
            L"DuskPlug — Settings",
            MB_ICONERROR | MB_OK);
        return false;
    }

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool ownsCom = comHr == S_OK;

    HMODULE loader = LoadWebView2Loader();
    if (!loader) {
        ShowWebView2InstallPrompt(owner);
        if (ownsCom) {
            CoUninitialize();
        }
        return false;
    }

    auto createEnv = reinterpret_cast<CreateCoreWebView2EnvironmentWithOptionsFn>(
        GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!createEnv) {
        ShowWebView2InstallPrompt(owner);
        FreeLibrary(loader);
        if (ownsCom) {
            CoUninitialize();
        }
        return false;
    }

    SettingsHost host{};
    host.configPath = configPath;
    host.config = &config;
    host.html = Utf8ToWide(InjectSettingsBoot(htmlUtf8, BuildSettingsBootJson(config, "windows")));

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SettingsWndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kSettingsClass;
    RegisterClassExW(&wc);

    const int width = 820;
    const int height = 940;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    HWND hwnd = CreateWindowExW(
        0,
        kSettingsClass,
        L"DuskPlug Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        x,
        y,
        width,
        height,
        owner,
        nullptr,
        instance,
        &host);
    if (!hwnd) {
        FreeLibrary(loader);
        if (ownsCom) {
            CoUninitialize();
        }
        return false;
    }

    g_settingsHwnd = hwnd;

    ApplyDarkTitleBar(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    const std::wstring userData = Utf8ToWide(GetAppDataDir() + "\\WebView2");
    EnsureDirectoryExists(WideToUtf8(userData));
    const HRESULT envHr = createEnv(nullptr, userData.c_str(), nullptr, new EnvironmentReadyHandler(&host));
    if (FAILED(envHr)) {
        ShowWebView2InstallPrompt(hwnd);
        DestroyWindow(hwnd);
        g_settingsHwnd = nullptr;
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        FreeLibrary(loader);
        if (ownsCom) {
            CoUninitialize();
        }
        return false;
    }

    MSG msg{};
    while (host.alive && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    FreeLibrary(loader);
    if (ownsCom) {
        CoUninitialize();
    }
    return host.saved;
}
