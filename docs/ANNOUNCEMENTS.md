# Community announcement drafts

Copy, edit, and publish on your schedule. Stagger posts by a few days.

---

## Show HN (Hacker News)

**Title:** DuskPlug – open-source Windows tray app for Tuya smart plugs (dusk/dawn scheduling)

**Body:**

I built DuskPlug because I wanted my desk lamp to turn on at sunset and off at sunrise without running Home Assistant or leaving a browser tab open.

DuskPlug is a small Win32/C++ system tray app that talks to your own Tuya Cloud project (free developer account). It supports:

- Manual on/off from the tray
- Smart Mode: on at sunset, off at sunrise (uses Windows location)
- Schedule Mode: fixed daily times
- Lock-off when the PC is locked
- Hibernate-aware: uses the plug's built-in countdown so the light still turns off after sleep

MIT licensed, MSI installer + portable ZIP:
https://github.com/DuskPlug/DuskPlug/releases/tag/v1.0.0

Getting started (Tuya portal walkthrough):
https://github.com/DuskPlug/DuskPlug/blob/master/docs/GETTING-STARTED.md

Happy to answer questions about the Tuya API setup or the sleep/countdown approach.

---

## Reddit — r/homeautomation

**Title:** [Tool] DuskPlug – free Windows tray app for Tuya plugs (sunset on / sunrise off)

**Body:**

If you use a Tuya smart plug for a desk lamp or similar and want dusk/dawn automation without Home Assistant, I released a small open-source tray app called **DuskPlug**.

What it does:
- Lives in the Windows system tray
- **Smart Mode**: plug on at sunset, off at sunrise (from your location)
- **Schedule Mode**: fixed daily on/off times
- MSI installer or portable ZIP

You need a free Tuya developer project linked to the same Smart Life / Tuya app account as your phone. Full setup guide is in the repo.

Release: https://github.com/DuskPlug/DuskPlug/releases/tag/v1.0.0  
Guide: https://github.com/DuskPlug/DuskPlug/blob/master/docs/GETTING-STARTED.md

Feedback welcome — especially if you hit Tuya data-center / linking issues (UK/EU users sometimes need Western Europe vs Central Europe).

---

## Reddit — r/Tuya (or Tuya developer forum)

**Title:** Open-source Windows tray client for Smart Home OpenAPI (dusk/dawn scheduling)

**Body:**

I published **DuskPlug**, an MIT-licensed Windows tray app that controls a Tuya smart plug via the Smart Home OpenAPI.

It uses the standard token + device commands flow (`switch_1`, optional `countdown_1` for delayed off on sleep). Setup requires linking your Smart Life app account in the developer portal — the step-by-step is here:

https://github.com/DuskPlug/DuskPlug/blob/master/docs/GETTING-STARTED.md

Repo: https://github.com/DuskPlug/DuskPlug

Useful if you want a lightweight native alternative to scripting against the API yourself.

---

## Tips before posting

- Post Show HN on a weekday morning US Eastern if possible.
- Do not cross-post identical text to multiple subreddits on the same day.
- Reply to comments promptly — setup questions are the most common.
