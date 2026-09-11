# DuskPlug

[![CI](https://github.com/MrChriZ/DuskPlug/actions/workflows/ci.yml/badge.svg)](https://github.com/MrChriZ/DuskPlug/actions/workflows/ci.yml)

Control a Tuya smart plug from the Windows system tray — manual on/off, **Smart Mode** (on at dusk, off at dawn), and **Schedule Mode** (fixed daily times).

Free, open source, and MIT-licensed. You only need your own free Tuya cloud project — no DuskPlug account.

## Screenshots

**System tray** (real Windows 11 taskbar):

![DuskPlug in the system tray](docs/screenshots/tray-in-taskbar.png)

**Context menu**:

![Tray context menu with Smart Mode enabled](docs/screenshots/tray-menu.png)

**Settings**:

![Settings dialog](docs/screenshots/settings-dialog.png)

## Why DuskPlug?

- **Free and open source** — MIT license, no subscription
- **Your Tuya project only** — credentials stay in `%APPDATA%\SMART\config.json` on your PC
- **Smart Mode** — sunset on, sunrise off from your location
- **Schedule Mode** — fixed daily on/off times
- **Easy install** — MSI installer or portable ZIP

## Download

**Installer (recommended):** download **DuskPlug.msi** from [GitHub Releases](https://github.com/MrChriZ/DuskPlug/releases) and run it. That installs to Program Files, adds a Start menu shortcut, and starts DuskPlug when you log in. First run opens **Settings**.

**Portable ZIP:** download **DuskPlug-Windows.zip**, unzip it anywhere, and double-click **`Start-DuskPlug.cmd`**.

Code signing via [SignPath Foundation](https://signpath.org) (pending approval). Windows builds will be Authenticode-signed once approved.

If you cloned this repo instead, run **`Build-DuskPlug.cmd`**. **`Build-Msi.cmd`** builds the installer (needs the .NET SDK).

Full walkthrough (Tuya portal screens, linking the phone app, and every Settings field): **[Getting started](docs/GETTING-STARTED.md)**.

Privacy: [Privacy Policy](docs/PRIVACY.md)

## Set up a Tuya cloud project (one-time)

You need a free [Tuya Developer Platform](https://iot.tuya.com) project so Windows can talk to the same plug as your phone. The plug must already work in **Smart Life**, **Tuya**, or **Status**.

1. **Check the phone app region**  
   **Me → Setting → Account and Security → Region**.  
   The cloud project data center must serve that region. UK **Smart Life** accounts are usually **Central Europe**; if devices do not appear after linking, try **Western Europe** (Tuya added that data center in 2025).

2. **Create a developer account** at [iot.tuya.com](https://iot.tuya.com) (**Tuya Smart Developer Center**) and sign in.

3. **Create a cloud project**  
   Sidebar **Cloud → Cloud Project → Project Management** (or **Cloud → Development**).  
   Click **Upgrade IoT Core Plan** if shown (free trial), then **Create Cloud Project**.  
   - **Development Method:** **Smart Home** (not Custom)  
   - **Data Center:** match the phone app region  

4. **Authorize API Services** in the wizard. Keep the defaults and include **IoT Core**, **Authorization Token Management**, **Smart Home Basic Service**, and **Device Status Notification**. Click **Authorize**. Skip asset/user creation (that is for Custom projects).

5. **Copy credentials** — **Open Project** → **Overview** → **Authorization Key**:  
   - **Access ID / Client ID**  
   - **Access Secret / Client Secret**

6. **Link the phone app** (this is the step most people miss)  
   **Devices → Link Tuya App Account → Add App Account** → **Tuya App Account Authorization** → scan the QR code from the **same** phone app that already controls the plug → **Confirm**. Leave **Automatic Link** selected.  
   **All Devices** should then list your plug.

7. **Copy Device ID** from **Devices → All Devices** for that plug.

Do not put Access Secret or Device ID in git. DuskPlug stores them only in `%APPDATA%\SMART\config.json`.

## Enter the configuration in DuskPlug

Double-click **`Start-DuskPlug.cmd`**. On first run, **Settings** opens automatically. Later: tray icon → right-click → **Settings...**

**Plug connection** (required):

| Field | What to enter |
|-------|----------------|
| **Access ID** | Access ID from Tuya Overview |
| **Access Secret** | Access Secret from Tuya Overview |
| **Device ID** | Device ID from All Devices |
| **Data center** | Same Data Center as the project Overview |

Click **Save**.

**Smart Mode location:** click **Detect Location** (allow Windows location for desktop apps) or type latitude/longitude.

**Daily schedule:** set ON and OFF times if you will use Schedule Mode. Overnight spans (22:00 → 06:00) are fine.

Alternatively, **`Setup.cmd`** is a terminal wizard that fills the same values and tests the live connection to the plug.

### Tray

A lightbulb appears near the clock. Left-click toggles the plug. Right-click for **Smart Mode**, **Schedule Mode**, **Settings...**, and the rest.

| Mode | What it does |
|------|----------------|
| **Manual** | You control the plug (default) |
| **Smart Mode** | On at sunset, off at sunrise |
| **Schedule Mode** | Follows the daily ON/OFF times from Settings |

Optional: **`Install-Startup.cmd`** runs DuskPlug at Windows login.

### Troubleshooting

| Problem | Fix |
|---------|-----|
| Setup says sign invalid | Access ID / Secret wrong, or wrong data center — fix in **Settings** or re-run **`Setup.cmd`** |
| Device list empty in the Tuya portal | **Devices → Link Tuya App Account → Add App Account**, and check the data center |
| Smart Mode needs location | Windows **Settings → Privacy → Location** → allow desktop apps, then **Detect Location** in DuskPlug |
| No tray icon | Run **`Start-DuskPlug.cmd`** |

## Developers

### Build

```cmd
Build-DuskPlug.cmd
```

Or `cd cpp` and run `build.cmd`. Produces `DuskPlug.exe` in the project root.

### Tests

```cmd
cpp\test.cmd
```

### Project layout

| Path | Purpose |
|------|---------|
| `cpp/src/` | DuskPlug C++ source |
| `lib/TuyaApi.ps1` | Tuya Cloud API helpers |
| `Setup.ps1` / `Setup.cmd` | Interactive setup wizard |
| `Plug-*.ps1` | Optional CLI helpers (same cloud API as the tray) |
| `config.example.json` | Public template (placeholders only) |
| `installer/` | WiX source for **DuskPlug.msi** |
| `packaging/` | winget and Scoop manifest sources |

Config is loaded from `%APPDATA%\SMART\config.json`. See [`cpp/README.md`](cpp/README.md) for Smart Mode details.

Releases: [`docs/RELEASE.md`](docs/RELEASE.md)

## License

MIT — see [LICENSE](LICENSE).
