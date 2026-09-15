# Why I built a dusk-to-dawn tray app for my desk lamp

*Draft for Dev.to or a personal blog. ~800 words.*

---

My desk lamp is on a Tuya smart plug. Smart Life on my phone works fine, but I wanted the light to follow the day: on at sunset, off at sunrise, without opening an app or running Home Assistant on a Raspberry Pi I do not own.

So I built **DuskPlug** — a Windows system tray app that controls one plug through my own Tuya Cloud project.

## The problem with "just use a schedule"

Fixed schedules drift from the seasons. Sunset in London in June is nothing like sunset in December. Smart Life has scenes and automations, but I spend most of my day at a PC. I wanted something that lives in the tray, starts with Windows, and respects astronomical dusk and dawn.

## Why a tray app and not a script

A PowerShell script could call the Tuya API on a timer. I tried that. It works until:

- You want a visible on/off state in the tray
- You want Smart Mode, Schedule Mode, and manual control in one place
- You want lock-off behaviour (turn the plug off after the PC is locked)
- You want sleep/hibernate to still turn the light off reliably

The last point is subtle. When Windows hibernates, your PC is gone — you cannot run a 30-second timer on the PC. DuskPlug sends a **countdown command** to the plug itself (`countdown_1` paired with `switch_1`). The plug turns off on its own schedule even after the machine sleeps.

## Tuya setup is the hard part

The code is straightforward HMAC-SHA256 signing against the OpenAPI. The friction is the developer portal:

1. Create a Smart Home cloud project in the right **data center**
2. Authorize IoT Core and Smart Home Basic Service
3. **Link your phone app account** (easy to miss)
4. Copy Device ID and credentials into the app

I wrote a getting-started guide that matches the 2026 portal UI because I lost an evening to "sign invalid" and empty device lists before linking the app account.

## What DuskPlug does today

- **Manual mode** — left-click the tray icon to toggle
- **Smart Mode** — sunset on, sunrise off from latitude/longitude
- **Schedule Mode** — fixed daily times, including overnight windows
- **Settings dialog** — no hand-editing JSON unless you want to
- **MSI installer** — Program Files, Start menu, run at login
- **winget and Scoop** — `winget install DuskPlug.DuskPlug`

Everything is MIT licensed: https://github.com/DuskPlug/DuskPlug

## If you try it

Start at https://duskplug.github.io/DuskPlug/ — winget is the quickest install on Windows 10/11. First run opens Settings. If Smart Mode says it needs location, allow desktop apps in Windows Location privacy settings, then click **Detect Location**.

If the Tuya portal shows no devices after linking, double-check the data center — UK/EU accounts sometimes need Western Europe instead of Central Europe.

## What's next

Code signing through SignPath Foundation is pending. winget listing is in review; Scoop has an official project bucket. If you have a Tuya plug and a Windows desktop, I would love to hear whether the setup guide is clear enough.

---

**Suggested tags:** `windows`, `iot`, `opensource`, `smart-home`, `tuya`

**Link:** https://duskplug.github.io/DuskPlug/
