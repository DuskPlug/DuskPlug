"""Capture real Settings tab screenshots for README / GitHub Pages."""
from __future__ import annotations

import base64
import json
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
OUT = ROOT / "docs" / "screenshots"
SOURCE = OUT / "source"
SETTINGS = ASSETS / "settings.html"
BACKUP = ASSETS / "settings.html.capture-backup"
MARK_TOKEN = "/*__DUSKPLUG_MARK__*/"
TAB_TOKEN = "/*__DUSKPLUG_INITIAL_TAB__*/null"

SETTINGS_TABS = (
    ("connection", "settings-connection.png"),
    ("devices", "settings-devices.png"),
    ("location", "settings-location.png"),
    ("screen", "settings-screen.png"),
    ("general", "settings-general.png"),
)


def inject_brand_mark(html: str) -> str:
    pos = html.find(MARK_TOKEN)
    if pos == -1:
        raise SystemExit(f"Token {MARK_TOKEN} not found in settings.html")
    png = ASSETS / "brand-mark.png"
    if not png.is_file():
        raise SystemExit(f"Missing {png}")
    data_uri = "data:image/png;base64," + base64.b64encode(png.read_bytes()).decode("ascii")
    return html[:pos] + data_uri + html[pos + len(MARK_TOKEN) :]


def inject_initial_tab(html: str, tab_id: str) -> str:
    return html.replace(TAB_TOKEN, json.dumps(tab_id))


def prepare_settings_html(original_html: str, tab_id: str) -> str:
    html = inject_brand_mark(original_html)
    html = inject_initial_tab(html, tab_id)
    return html


def kill_duskplug() -> None:
    subprocess.run(
        ["taskkill", "/IM", "DuskPlug.exe", "/F"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    time.sleep(0.5)


def main() -> int:
    if not SETTINGS.is_file():
        raise SystemExit(f"Missing {SETTINGS}")

    capture_script = ROOT / "scripts" / "capture_readme_screenshots.py"
    if not capture_script.is_file():
        raise SystemExit(f"Missing {capture_script}")

    OUT.mkdir(parents=True, exist_ok=True)
    SOURCE.mkdir(parents=True, exist_ok=True)

    shutil.copy2(SETTINGS, BACKUP)
    original_html = BACKUP.read_text(encoding="utf-8")
    kill_duskplug()

    try:
        for tab_id, filename in SETTINGS_TABS:
            prepared = prepare_settings_html(original_html, tab_id)
            SETTINGS.write_text(prepared, encoding="utf-8", newline="\n")

            kill_duskplug()
            sys.path.insert(0, str(ROOT / "scripts"))
            import capture_readme_screenshots as cap

            cap.OUT = OUT
            if not cap.capture_settings_dialog(OUT / filename):
                raise SystemExit(f"Failed to capture tab {tab_id}")

            shutil.copy2(OUT / filename, SOURCE / filename)
            print(f"Saved {OUT / filename}")

        shutil.copy2(OUT / "settings-connection.png", OUT / "settings-dialog.png")
        shutil.copy2(OUT / "settings-connection.png", SOURCE / "settings-dialog.png")
        print(f"Saved {OUT / 'settings-dialog.png'}")
    finally:
        shutil.move(BACKUP, SETTINGS)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
