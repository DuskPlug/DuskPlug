# DuskPlug (C++)

Native tray/menu-bar apps for Windows, Linux, and macOS — no PowerShell runtime required on any platform.

## Build

### Windows

Requires MinGW g++ (installed automatically via WinLibs if needed):

```cmd
cd cpp
build.cmd
```

Produces `DuskPlug.exe` in the project root. Rebuilds are incremental and parallel; default is debug (`-O0`). Pass `-Release` for `-O2`, or `-Clean` to discard cached objects.

### Linux

Dependencies (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev \
  libgtk-3-dev libayatana-appindicator3-dev libgeoclue-2-dev libsystemd-dev \
  libwebkit2gtk-4.1-dev
cmake -S cpp -B cpp/build && cmake --build cpp/build --target duskplug
```

### macOS

```bash
brew install cmake curl
cmake -S cpp -B cpp/build && cmake --build cpp/build --target DuskPlug
```

## Architecture

| Layer | Purpose |
|-------|---------|
| `src/config.cpp`, `tuya_client.cpp`, `smart_mode.cpp`, … | Portable UTF-8 core |
| `src/platform_util.cpp` | Paths, file I/O, time, random |
| `src/http_win.cpp` / `src/http_curl.cpp` | Platform HTTP |
| `src/main.cpp` + HTML settings | Windows tray app + WebView2 settings |
| `src/linux/` | GTK3 + Ayatana AppIndicator + WebKitGTK settings |
| `src/macos/` | Cocoa menu bar app + WKWebView settings |

## Behaviour

### Manual mode

- Left-click toggles the plug
- Tray/menu: Turn On / Off / Smart Mode / Schedule Mode / Settings / Refresh / Exit
- Polls every 30 seconds

### Smart Mode

- On at dusk, off at dawn from your location
- **Windows:** Windows Location Services
- **Linux:** GeoClue 2
- **macOS:** Core Location
- Lock-screen auto-off and sleep countdown-off on all platforms (toggle from tray menu: **Off when locked or sleeping**)
- Optional screen brightness (toggle from tray menu: **Adjust screen brightness**): sets night/day backlight levels on controllable displays while Smart or Schedule Mode is active; restores previous brightness when disabled or when leaving automation

### Schedule Mode

- Fixed daily ON/OFF times from Settings
- Lock-screen and sleep behaviour matches Smart Mode

## Config paths

| OS | Path |
|----|------|
| Windows | `%APPDATA%\SMART\config.json` |
| Linux | `~/.config/duskplug/config.json` |
| macOS | `~/Library/Application Support/DuskPlug/config.json` |

Uses `assets/light-*.ico` on Windows and icon names on Linux (install PNGs alongside the binary for custom icons).
