"""Create GitHub social preview image for DuskPlug."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "social-preview.png"
ICON = ROOT / "assets" / "app.ico"

W, H = 1280, 640
BG = (24, 28, 36)
ACCENT = (255, 180, 80)


def main() -> None:
    img = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(img)

    icon = Image.open(ICON).resize((128, 128), Image.Resampling.LANCZOS)
    img.paste(icon, (80, (H - 128) // 2), icon if icon.mode == "RGBA" else None)

    try:
        title_font = ImageFont.truetype("segoeui.ttf", 72)
        sub_font = ImageFont.truetype("segoeui.ttf", 32)
    except OSError:
        title_font = ImageFont.load_default()
        sub_font = ImageFont.load_default()

    draw.text((240, 220), "DuskPlug", fill=(255, 255, 255), font=title_font)
    draw.text(
        (240, 320),
        "Tuya smart plug control from the Windows tray",
        fill=ACCENT,
        font=sub_font,
    )

    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    github_dir = ROOT / ".github"
    github_dir.mkdir(exist_ok=True)
    img.save(github_dir / "social-preview.png")
    print(f"Saved {OUT}")


if __name__ == "__main__":
    main()
