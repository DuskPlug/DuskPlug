# DuskPlug Privacy Policy

Last updated: September 2026

DuskPlug does not operate any servers and does not collect analytics or telemetry.

## Data stored on your computer

DuskPlug stores configuration locally on your computer:

- **Windows:** `%APPDATA%\SMART\config.json`
- **Linux:** `~/.config/duskplug/config.json`
- **macOS:** `~/Library/Application Support/DuskPlug/config.json`

That file includes:

- Tuya Cloud API credentials (Access ID, Access Secret, Device ID) that you enter
- Optional latitude and longitude for Smart Mode (dusk/dawn times)
- Schedule and preference settings (including optional screen brightness night/day levels)

This data never leaves your PC except when DuskPlug calls the Tuya Cloud API to control your plug, using your own Tuya developer project.

Screen brightness changes are applied locally through your operating system’s display APIs. DuskPlug does not send brightness data anywhere.

## Location

If you use **Detect Location**, DuskPlug requests your coordinates from the Windows location service. Coordinates are saved locally in your config file. DuskPlug does not send location data to the developer or any third party other than being stored locally on your device.

## Third parties

Plug control requests go directly to Tuya's API (for example `openapi.tuyaeu.com`, depending on the data center you select). Tuya's privacy policy applies to that service: https://www.tuya.com/legal/privacy-policy

## Contact

Issues and questions: https://github.com/DuskPlug/DuskPlug/issues
