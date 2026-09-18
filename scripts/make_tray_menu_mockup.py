"""Deprecated: tray-menu.png is a real Windows capture in docs/screenshots/source/."""
import sys

print("tray-menu.png is maintained manually from docs/screenshots/source/tray-menu.png", file=sys.stderr)
raise SystemExit(1)

# Legacy mockup code kept for reference only.
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent.parent / "docs" / "screenshots" / "tray-menu.png"

MENU = [
    "Turn On",
    "Turn Off",
    "Smart Mode",
    "Schedule Mode",
    "Timed Mode",
    "Off when locked or sleeping",
    None,
    "Screen brightness",
    None,
    "Settings...",
    "Check for updates...",
    None,
    "Exit",
]
CHECKED = {"Smart Mode"}


def main() -> None:
    w, pad = 260, 8
    line_h = 28
    lines = [m for m in MENU if m is not None]
    sep = 1 if None in MENU else 0
    h = pad * 2 + len(lines) * line_h + sep * 6

    img = Image.new("RGB", (w, h), (243, 243, 243))
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype("segoeui.ttf", 14)
    except OSError:
        font = ImageFont.load_default()

    y = pad
    for item in MENU:
        if item is None:
            draw.line([(pad, y + 2), (w - pad, y + 2)], fill=(200, 200, 200), width=1)
            y += 6
            continue
        prefix = "✓ " if item in CHECKED else "   "
        draw.text((pad + 4, y), prefix + item, fill=(20, 20, 20), font=font)
        y += line_h

    draw.rectangle([(0, 0), (w - 1, h - 1)], outline=(180, 180, 180))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print(f"Saved {OUT}")


if __name__ == "__main__":
    main()
