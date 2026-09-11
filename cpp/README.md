# DuskPlug (C++)

Non-coders: start with the [root README](../README.md) and double-click `Start-DuskPlug.cmd`.

Native Windows tray app — no PowerShell, no console window.

## Build

Requires MinGW g++ (installed automatically via WinLibs if needed):

```cmd
cd cpp
build.cmd
```

Or install manually: `winget install BrechtSanders.WinLibs.POSIX.UCRT`

Produces `DuskPlug.exe` in the project root.

## Run

Double-click **DuskPlug.exe**, or reinstall startup:

```powershell
.\Install-Startup.ps1
```

Startup prefers `DuskPlug.exe` when present; otherwise it falls back to `Launch-Tray.vbs`.

## Behaviour

### Manual mode

- Left-click toggles the plug
- Right-click menu: Turn On / Off / Smart Mode / Schedule Mode / Settings / Refresh / Restart / Exit
- Lightbulb icon shows on/off state
- Polls every 30 seconds

### Smart Mode

Enable from the tray menu (checkmark when active). Smart Mode:

- Turns the plug **on at dusk** and **off at dawn**, based on your location and the date (season-adjusting sunrise/sunset)
- Uses **Windows Location Services** first; falls back to latitude/longitude from **Settings**
- Turns **off** if the screen is locked for more than 30 seconds (configurable via `LockOffSeconds`)
- Before **sleep or hibernate**, arms the plug's countdown (same 30 seconds) so the light turns off after the PC is already off; if the plug has no countdown, it turns off immediately
- Turns **back on** when you move the mouse after unlocking (if it's dark)
- Shows a blue-ring **smart icon** in the tray
- **Turn On**, **Turn Off**, or **left-click toggle** exit Smart Mode until you enable it again
- Preference is saved to `%APPDATA%\SMART\state.json` and restored on restart

### Schedule Mode

- Enable from the tray menu; uses the daily ON/OFF times from **Settings**
- No location required
- Lock-screen auto-off applies the same way as Smart Mode
- Sleep/hibernate auto-off applies the same way as Smart Mode

### Settings

All configuration is edited in **Settings...** from the tray menu (stored in `%APPDATA%\SMART\config.json`):

| Section | Fields |
|---------|--------|
| Plug connection | Access ID, Access Secret, Device ID, data center |
| Smart Mode | Latitude/longitude, Detect Location, sunset/sunrise offsets |
| Daily schedule | ON and OFF times |
| Advanced | Switch code, lock-off seconds |

Windows Location must be enabled in **Settings → Privacy & security → Location** for Detect Location to work.

## Files

- `src/main.cpp` — Win32 tray UI
- `src/smart_mode.cpp` — Smart Mode orchestration
- `src/solar.cpp` — Sunrise/sunset calculation
- `src/location_win.cpp` — Windows Location + config fallback
- `src/activity_win.cpp` — Lock detection and mouse hook
- `src/tuya_client.cpp` — Tuya Cloud API
- `src/http_win.cpp` — WinHTTP
- `src/crypto.cpp` — HMAC-SHA256 signing

Uses `%APPDATA%\SMART\config.json` and `assets\light-*.ico`.
