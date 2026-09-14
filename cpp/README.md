# DuskPlug (C++)

Native tray/menu-bar apps for Windows, Linux, and macOS — no PowerShell runtime required on any platform.

## Build

### Windows

Requires MinGW g++ (installed automatically via WinLibs if needed):

```cmd
cd cpp
build.cmd
```

Produces `DuskPlug.exe` in the project root.

### Linux

Dependencies (Debian/Ubuntu):

```bash
sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev \
  libgtk-3-dev libayatana-appindicator3-dev libgeoclue-2-dev libsystemd-dev
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
| `src/main.cpp` + Win32 UI | Windows tray app (unchanged UX) |
| `src/linux/` | GTK3 + Ayatana AppIndicator |
| `src/macos/` | Cocoa menu bar app |

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
