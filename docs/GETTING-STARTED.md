# Getting started with DuskPlug

This guide walks through creating a Tuya cloud project, linking the phone app that already controls your plug, and entering those details in DuskPlug.

## Before you begin

- Windows 10 or 11
- A Tuya-compatible smart plug that already works in the **Smart Life**, **Tuya**, or **Status** phone app
- About 10 minutes for a one-time cloud setup (free)

DuskPlug never stores your credentials in the GitHub repo. They are saved only on your PC in `%APPDATA%\SMART\config.json`.

## 1. Download DuskPlug

**Installer (recommended):** download **DuskPlug.msi** from [GitHub Releases](https://github.com/DuskPlug/DuskPlug/releases) and run it. Windows may ask for administrator approval. That puts DuskPlug in Program Files, adds a Start menu shortcut, and starts it when you log in.

**Portable ZIP:** download **DuskPlug-Windows.zip** and unzip it anywhere (for example `Documents\DuskPlug`). Double-click **`Start-DuskPlug.cmd`**.

If you cloned the source repo instead, run `Build-DuskPlug.cmd` once to create `DuskPlug.exe`. `Build-Msi.cmd` builds the installer.

## 2. Create a Tuya developer account

1. Open the [Tuya Developer Platform](https://iot.tuya.com) (`iot.tuya.com`; `platform.tuya.com` signs you into the same place).
2. You land on **Tuya Smart Developer Center**. Sign in, or click **Sign Up**.
3. This is a **developer** account. It can use the same email as the phone app, but it is not the same as the consumer app login until you link them in step 6.
4. If Tuya asks you to complete a developer profile (country, account type), choose **individual** / personal use.

## 3. Check your phone app region

The cloud project **must** use a data center that serves the phone app that already controls the plug.

In **Smart Life**, **Tuya**, or **Status**:

**Me → Setting → Account and Security → Region**

(Tuya’s English UI uses **Setting**, not Settings.)

Remember that region (for example United Kingdom). You pick a matching **Data Center** when you create the project.

| Phone app region | Data center in Tuya and DuskPlug |
|------------------|-----------------------------------|
| United Kingdom / Europe (existing accounts) | **Central Europe** |
| United Kingdom / Europe (if Central Europe shows no devices) | **Western Europe** |
| United States / Canada | **Western America** |
| Latin America, Japan, Korea, New Zealand | **Eastern America** |
| Singapore, SE Asia, Hong Kong, Taiwan | **Singapore** |
| India | **India** |

Start with **Central Europe** for a typical UK **Smart Life** / **Tuya** / **Status** account. Tuya added **Western Europe** in late 2025; if **All Devices** is empty after linking, create or edit the project to **Western Europe** (or the other way around) and link again.

Official mapping: [OEM app accounts and data centers](https://developer.tuya.com/en/docs/iot/oem-app-data-center-distributed?id=Kafi0ku9l07qb).

## 4. Create a cloud project

1. In the left sidebar choose **Cloud → Cloud Project → Project Management**.  
   If you instead see **Cloud → Development**, that is the same **My Cloud Projects** list — use that.
2. If the page shows **Upgrade IoT Core Plan**, click it and subscribe to the **free trial**. You cannot create a useful project without IoT Core.
3. Click **Create Cloud Project**.
4. In the dialog, set:
   - **Project Name** — anything, e.g. `DuskPlug`
   - **Description** — optional
   - **Industry** — any value (Smart Home is fine)
   - **Development Method** — **Smart Home** (not **Custom**)
   - **Data Center** — from the table above
5. Click **Create**.
6. The **Authorize API Services** wizard appears. Keep the Smart Home defaults and make sure these are ticked:
   - **IoT Core**
   - **Authorization** / **Authorization Token Management**
   - **Smart Home Basic Service**
   - **Device Status Notification** (add it if it is not already selected)
   - **Industry Basic Service** (add it if listed)
7. Click **Authorize**.

Skip any **Create asset** / **original account** step — that is only for **Custom** projects.

If APIs are missing later:

- Open the project → **Service API** tab → **Go to Authorize**, or
- **Cloud → Cloud Project → Cloud Services**, subscribe, then authorize this project.

Tuya’s current API-key walkthrough: [Request Tuya Cloud API Key](https://developer.tuya.com/en/docs/developer/apply-cloud-api-key?id=Kff30z8sv62ah).

## 5. Copy Access ID and Access Secret

1. On **Project Management**, click **Open Project** in the **Operation** column (or click the project name).
2. Stay on the **Overview** tab.
3. Under **Authorization Key**, copy:
   - **Access ID / Client ID**
   - **Access Secret / Client Secret**

Treat the secret like a password. Do not commit it to Git or paste it into a public issue.

Confirm the **Data Center** on Overview matches what you will pick in DuskPlug.

## 6. Link your phone app (the step most people miss)

Creating a cloud project does **not** automatically see your plug. You must authorize the consumer app account:

1. In the project, open the **Devices** tab.
2. Open **Link Tuya App Account** (sometimes **Link App Account**).
3. Click **Add App Account**, then **Tuya App Account Authorization** if it asks which type.
4. A QR code appears.
5. On your phone, open the **same** **Smart Life**, **Tuya**, or **Status** app that already controls the plug.
6. Scan the QR code (in-app scan: often **Me → Setting** and a scan icon, or the scan control on the home screen).
7. Tap **Confirm** on the phone.
8. If the portal asks how to link devices, leave **Automatic Link** selected.
9. Open the **All Devices** tab. Your plug should be listed.

If **All Devices** is empty:

- You scanned with a different Tuya account than the one that owns the plug
- The project **Data Center** does not serve that app region — try the other Europe data center (see step 3)
- The QR code expired — **Add App Account** again and scan a new code

## 7. Copy the Device ID

1. Still under **Devices → All Devices**.
2. Find your smart plug.
3. Copy **Device ID**. It is a long alphanumeric id (about 20 characters).

That is the id DuskPlug will send on/off commands to.

## 8. Enter the configuration in DuskPlug

Double-click **`Start-DuskPlug.cmd`**.

On first run, **DuskPlug Settings** opens automatically. Later you can open it from the tray icon: right-click → **Settings...**

### Plug connection (required)

| Settings field | Paste / choose |
|----------------|----------------|
| **Access ID** | Access ID from project Overview → Authorization Key |
| **Access Secret** | Access Secret from the same place |
| **Device ID** | Device ID from All Devices |
| **Data center** | Same **Data Center** as the cloud project Overview (UK: usually **Central Europe**, or **Western Europe** if that is what the project uses) |

Click **Save**.

Alternatively, double-click **`Setup.cmd`** for a terminal wizard that asks for the same values and then **tests** the connection (token, device functions, optional live toggle).

### Smart Mode location (needed for dusk/dawn)

In Settings, under **Smart Mode location**:

1. Click **Detect Location**, or type latitude and longitude yourself.
2. Windows may prompt for location access. Allow it, and in Windows **Settings → Privacy & security → Location** turn on location services and **Let desktop apps access your location**.
3. Leave **After sunset** / **Before sunrise** at `0` unless you want the light to come on a few minutes after sunset or stay on a few minutes after sunrise.

### Daily schedule (needed for Schedule Mode)

Set **Turn plug ON at** and **Turn plug OFF at**. Overnight spans such as 22:00 → 06:00 are allowed. The two times cannot be the same.

### Advanced

Leave **Switch code** as `switch_1` unless Setup or the Tuya device functions list shows a different switch code. **Lock-off seconds** (default 30) is how long the screen can stay locked, and how long after sleep/hibernate, before the plug turns off.

## 9. Use the tray

You should see a lightbulb icon near the clock.

- **Left-click** toggles the plug (this leaves Smart/Schedule Mode and goes back to manual).
- **Right-click** opens the menu:
  - **Turn On** / **Turn Off** — manual control
  - **Smart Mode** — on at dusk, off at dawn, using your location
  - **Schedule Mode** — follows the daily ON/OFF times from Settings
  - **Settings...** — change Tuya details, location, or schedule
  - **Refresh Status** — re-read the plug from the cloud

A checkmark on **Smart Mode** or **Schedule Mode** means that mode is active.

## 10. Optional: start with Windows

Double-click **`Install-Startup.cmd`** so DuskPlug runs when you log in. **`Uninstall-Startup.cmd`** removes that.

## What success looks like

- Settings saves without an error (or Setup prints `Setup successful!`)
- Left-clicking the tray icon turns the physical plug on and off
- You did not have to edit any JSON by hand

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Sign invalid / token request failed | Access ID or Access Secret is wrong, or DuskPlug **Data center** does not match the project Overview |
| Device list empty in the Tuya portal | **Devices → Link Tuya App Account → Add App Account**, scan again; if still empty, the data center is probably wrong |
| IoT Core / trial expired | **Cloud → Cloud Project → Project Management → Upgrade IoT Core Plan** and extend the trial |
| Switch code not found | Run **`Setup.cmd`** and use the suggested switch code in Settings → Advanced |
| Smart Mode needs location | Windows **Settings → Privacy & security → Location**: services On, desktop apps On, then **Detect Location** in DuskPlug |
| Plug does not change | Confirm Device ID is the plug (not another device), and that the phone app can still control it |
| No tray icon | Start **DuskPlug** from the Start menu, or run **`Start-DuskPlug.cmd`** from the ZIP |

Official Tuya walkthroughs: [Request Tuya Cloud API Key](https://developer.tuya.com/en/docs/developer/apply-cloud-api-key?id=Kff30z8sv62ah) and [Smart Home project wizard](https://developer.tuya.com/en/docs/iot/Platform_Configuration_smarthome?id=Kamcgamwoevrx).
