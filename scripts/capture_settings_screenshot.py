"""Capture a real Settings window screenshot for README / GitHub Pages."""
from __future__ import annotations

import base64
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
OUT = ROOT / "docs" / "screenshots"
SOURCE = OUT / "source" / "settings-dialog.png"
SETTINGS = ASSETS / "settings.html"
BACKUP = ASSETS / "settings.html.capture-backup"
TOKEN = "/*__DUSKPLUG_MARK__*/"
BOOT_TOKEN = "/*__DUSKPLUG_BOOT__*/null"


def inject_brand_mark(html: str) -> str:
    pos = html.find(TOKEN)
    if pos == -1:
        raise SystemExit(f"Token {TOKEN} not found in settings.html")
    png = ASSETS / "brand-mark.png"
    if not png.is_file():
        raise SystemExit(f"Missing {png}")
    data_uri = "data:image/png;base64," + base64.b64encode(png.read_bytes()).decode("ascii")
    return html[:pos] + data_uri + html[pos + len(TOKEN) :]


def main() -> None:
    if not SETTINGS.is_file():
        raise SystemExit(f"Missing {SETTINGS}")

    html = SETTINGS.read_text(encoding="utf-8")
    prepared = inject_brand_mark(html)

    shutil.copy2(SETTINGS, BACKUP)
    try:
        SETTINGS.write_text(prepared, encoding="utf-8", newline="\n")
        result = subprocess.run(
            [sys.executable, str(ROOT / "scripts" / "capture_readme_screenshots.py")],
            cwd=str(ROOT),
            check=False,
        )
        if result.returncode != 0:
            raise SystemExit(result.returncode)
    finally:
        shutil.move(BACKUP, SETTINGS)

    if not (OUT / "settings-dialog.png").is_file():
        raise SystemExit("Capture did not produce settings-dialog.png")

    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(OUT / "settings-dialog.png", SOURCE)
    print(f"Saved {OUT / 'settings-dialog.png'}")
    print(f"Saved {SOURCE}")


if __name__ == "__main__":
    main()
