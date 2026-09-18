"""Copy real tray captures from docs/screenshots/source/ into README exports."""
from __future__ import annotations

from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "docs" / "screenshots" / "source" / "tray-menu.png"
OUT = ROOT / "docs" / "screenshots"


def main() -> None:
    if not SOURCE.is_file():
        raise SystemExit(f"Missing source screenshot: {SOURCE}")

    img = Image.open(SOURCE).convert("RGBA")
    img.save(OUT / "tray-menu.png", format="PNG", optimize=True)
    print(f"Updated tray-menu.png from {SOURCE}")


if __name__ == "__main__":
    main()
