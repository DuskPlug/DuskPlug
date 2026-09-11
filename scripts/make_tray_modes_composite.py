"""Composite tray icon states for README."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "screenshots" / "tray-modes.png"
ASSETS = ROOT / "assets"

items = [
    ("Manual ON", "light-on.ico"),
    ("Smart ON", "light-smart-on.ico"),
    ("OFF", "light-off.ico"),
]


def main() -> None:
    size = 64
    pad = 24
    label_h = 28
    w = len(items) * (size + pad) + pad
    h = size + label_h + pad * 2
    img = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype("segoeui.ttf", 16)
    except OSError:
        font = ImageFont.load_default()

    x = pad
    for label, ico in items:
        icon = Image.open(ASSETS / ico).resize((size, size), Image.Resampling.LANCZOS)
        img.paste(icon, (x, pad), icon if icon.mode == "RGBA" else None)
        bbox = draw.textbbox((0, 0), label, font=font)
        tw = bbox[2] - bbox[0]
        draw.text((x + (size - tw) // 2, pad + size + 6), label, fill=(40, 40, 40), font=font)
        x += size + pad

    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print(f"Saved {OUT}")


if __name__ == "__main__":
    main()
