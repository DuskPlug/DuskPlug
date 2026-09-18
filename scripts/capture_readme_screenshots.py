"""Generate high-quality README screenshots for DuskPlug."""
from __future__ import annotations

import ctypes
import subprocess
import sys
import time
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "screenshots"
sys.path.insert(0, str(ROOT / "scripts"))
from generate_icons import render_icon  # noqa: E402

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
WM_COMMAND = 0x0111
CMD_SETTINGS = 10009
WM_CLOSE = 0x0010
PW_RENDERFULLCONTENT = 2

VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()


class RECT(ctypes.Structure):
    _fields_ = [("left", ctypes.c_long), ("top", ctypes.c_long),
                ("right", ctypes.c_long), ("bottom", ctypes.c_long)]


def _font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    names = ["segoeuib.ttf", "segoeui.ttf"] if bold else ["segoeui.ttf", "arial.ttf"]
    for name in names:
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def _draw_menu_check(draw: ImageDraw.ImageDraw, x: int, y: int, size: int = 13) -> None:
    draw.line(
        [(x + 2, y + size // 2), (x + size // 2 - 1, y + size - 3), (x + size - 2, y + 3)],
        fill=(20, 20, 20),
        width=2,
        joint="curve",
    )


def _draw_submenu_arrow(draw: ImageDraw.ImageDraw, x: int, y: int) -> None:
    draw.polygon([(x, y), (x + 5, y + 4), (x, y + 8)], fill=(80, 80, 80))


def render_context_menu() -> Image.Image:
    """Windows-style context menu matching the current DuskPlug tray menu."""
    menu_font = _font(14)
    title_font = _font(13, bold=True)
    muted_font = _font(12)
    check_col = 28
    text_x = check_col + 8
    line_h = 32
    title_h = 36
    pad = 4
    w = 280

    items: list[tuple[str, bool, str] | None] = [
        ("__title__", False, ""),
        None,
        ("Turn On", False, ""),
        ("Turn Off", False, ""),
        ("Smart Mode", True, ""),
        None,
        ("Schedule Mode", False, ""),
        ("Timed Mode", False, "submenu"),
        ("Off when locked or sleeping", False, ""),
        None,
        ("Screen brightness", False, "submenu"),
        None,
        ("Settings...", False, ""),
        ("Check for updates...", False, ""),
        (f"v{VERSION}", False, "disabled"),
        None,
        ("Exit", False, ""),
    ]

    body_lines = [i for i in items if i is not None and i[0] != "__title__"]
    h = pad * 2 + title_h + 6 + len(body_lines) * line_h + sum(6 for i in items if i is None)

    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle((0, 0, w - 1, h - 1), radius=8, fill=(252, 252, 252, 255), outline=(200, 200, 200))

    y = pad
    icon = render_icon(16, on=True, smart=True)
    img.paste(icon, (12, y + 10), icon)
    draw.text((12 + 16 + 8, y + 10), "DuskPlug", fill=(25, 25, 25), font=title_font)
    y += title_h + 2

    for item in items:
        if item is None:
            draw.line([(12, y + 2), (w - 12, y + 2)], fill=(220, 220, 220), width=1)
            y += 6
            continue
        if item[0] == "__title__":
            continue

        label, checked, style = item
        if style == "disabled":
            draw.text((text_x, y + 6), label, fill=(140, 140, 140), font=muted_font)
        else:
            if checked:
                _draw_menu_check(draw, 8, y + 9, 13)
            draw.text((text_x, y + 6), label, fill=(25, 25, 25), font=menu_font)
            if style == "submenu":
                _draw_submenu_arrow(draw, w - 18, y + 12)
        y += line_h

    return img


def render_taskbar_snippet() -> Image.Image:
    """Windows 11-style taskbar crop with DuskPlug smart-on icon (normal + hover)."""
    icon_size = 16
    tray_icons = 4
    clock_w = 52
    pad_x = 10
    strip_h = 40
    gap = 6
    label_h = 22
    rows = 2
    w = pad_x * 2 + tray_icons * icon_size + (tray_icons - 1) * gap + clock_w + 24
    h = pad_x + rows * (strip_h + label_h + 8)

    img = Image.new("RGBA", (w, h), (255, 255, 255, 255))
    draw = ImageDraw.Draw(img)
    label_font = _font(13)

    smart_icon = render_icon(icon_size, on=True, smart=True)
    network = Image.new("RGBA", (icon_size, icon_size), (0, 0, 0, 0))
    nd = ImageDraw.Draw(network)
    nd.rectangle((2, 6, 14, 12), outline=(200, 200, 200), width=1)
    nd.line([(4, 12), (12, 12)], fill=(200, 200, 200), width=1)
    speaker = Image.new("RGBA", (icon_size, icon_size), (0, 0, 0, 0))
    sd = ImageDraw.Draw(speaker)
    sd.polygon([(4, 7), (7, 7), (11, 4), (11, 12), (7, 9), (4, 9)], fill=(220, 220, 220))

    states = [
        ("Smart Mode — plug on at dusk", False),
        ("Tray hover / selection", True),
    ]

    y = pad_x
    for label, hover in states:
        row_w = w - pad_x * 2
        draw.rounded_rectangle((pad_x, y, pad_x + row_w, y + strip_h), radius=6, fill=(32, 32, 32, 255))

        x = pad_x + 12
        cy = y + strip_h // 2
        if hover:
            draw.rounded_rectangle(
                (x - 4, cy - icon_size // 2 - 4, x + icon_size + 4, cy + icon_size // 2 + 4),
                radius=4,
                fill=(60, 60, 60, 255),
            )
        img.paste(smart_icon, (x, cy - icon_size // 2), smart_icon)
        x += icon_size + gap
        img.paste(network, (x, cy - icon_size // 2), network)
        x += icon_size + gap
        img.paste(speaker, (x, cy - icon_size // 2), speaker)
        x += icon_size + gap + 8
        draw.text((x + row_w - clock_w, cy - 7), "15:42", fill=(235, 235, 235), font=label_font)

        draw.text((pad_x, y + strip_h + 8), label, fill=(70, 70, 70), font=label_font)
        y += strip_h + label_h + 8

    return img


def find_window(class_name: str | None = None, title: str | None = None) -> int:
    if class_name:
        hwnd = user32.FindWindowW(class_name, None)
        if hwnd:
            return hwnd
    if title:
        return user32.FindWindowW(None, title)
    return 0


def wait_window(class_name: str | None = None, title: str | None = None, timeout: float = 15) -> int:
    deadline = time.time() + timeout
    while time.time() < deadline:
        hwnd = find_window(class_name, title)
        if hwnd:
            return hwnd
        time.sleep(0.2)
    return 0


def save_window(hwnd: int, out: Path) -> None:
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    if w <= 0 or h <= 0:
        raise RuntimeError(f"Invalid window size {w}x{h}")

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", ctypes.c_uint32), ("biWidth", ctypes.c_int32), ("biHeight", ctypes.c_int32),
            ("biPlanes", ctypes.c_uint16), ("biBitCount", ctypes.c_uint16),
            ("biCompression", ctypes.c_uint32), ("biSizeImage", ctypes.c_uint32),
            ("biXPelsPerMeter", ctypes.c_int32), ("biYPelsPerMeter", ctypes.c_int32),
            ("biClrUsed", ctypes.c_uint32), ("biClrImportant", ctypes.c_uint32),
        ]

    hdc_screen = user32.GetDC(0)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    gdi32.SelectObject(hdc_mem, hbmp)
    user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT)

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0

    buf = (ctypes.c_char * (w * h * 4))()
    gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), 0)
    Image.frombuffer("RGBA", (w, h), bytes(buf), "raw", "BGRA", 0, 1).save(out)

    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(0, hdc_screen)


def render_settings_mockup() -> Image.Image:
    """Styled mockup of the WebView2 settings page for README docs."""
    w, h = 760, 920
    bg_top = (20, 26, 36)
    bg_bottom = (12, 16, 24)
    card = (24, 31, 44)
    border = (255, 255, 255, 18)
    text = (238, 243, 248)
    muted = (147, 160, 180)
    accent = (231, 163, 90)
    input_bg = (15, 21, 32)

    img = Image.new("RGB", (w, h), bg_bottom)
    draw = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(h - 1, 1)
        color = tuple(int(bg_top[i] * (1 - t) + bg_bottom[i] * t) for i in range(3))
        draw.line([(0, y), (w, y)], fill=color)

    title_font = _font(18, bold=True)
    section_font = _font(13, bold=True)
    label_font = _font(12, bold=True)
    body_font = _font(13)
    small_font = _font(12)

    draw.rounded_rectangle((18, 18, 58, 58), radius=12, fill=(255, 215, 161))
    draw.text((74, 24), "DuskPlug Settings", fill=text, font=title_font)
    draw.text((74, 48), "Your Tuya credentials stay on this computer.", fill=muted, font=small_font)
    draw.rounded_rectangle((w - 88, 28, w - 24, 48), radius=12, outline=(60, 68, 82), width=1)
    draw.text((w - 78, 33), f"v{VERSION}", fill=muted, font=small_font)

    y = 84

    def card_block(top: int, title: str, hint: str, height: int) -> int:
        draw.rounded_rectangle((24, top, w - 24, top + height), radius=14, fill=card, outline=(40, 48, 62))
        draw.text((40, top + 16), title.upper(), fill=accent, font=section_font)
        draw.text((40, top + 36), hint, fill=muted, font=small_font)
        return top + height + 14

    def field(x: int, top: int, label: str, value: str, width: int) -> None:
        draw.text((x, top), label, fill=muted, font=label_font)
        draw.rounded_rectangle((x, top + 18, x + width, top + 54), radius=10, fill=input_bg, outline=(50, 58, 72))
        draw.text((x + 11, top + 30), value, fill=text, font=body_font)

    h1 = 170
    card_block(y, "Tuya connection", "From your Tuya cloud project Overview.", h1)
    field(40, y + 58, "Access ID", "your-access-id", 320)
    field(40, y + 112, "Access Secret", "••••••••••••••••", 320)
    field(420, y + 58, "Data center", "Central Europe (UK / most EU)", 300)
    y += h1 + 14

    h2 = 250
    card_block(y, "Device", "Smart Mode and Schedule Mode can also be toggled from the tray menu.", h2)
    field(40, y + 58, "Name", "Living room bulb", 300)
    field(380, y + 58, "Device ID", "bf12411369569ac3f6mshp", 340)
    field(40, y + 112, "Type", "Smart bulb (dimming)", 300)
    field(380, y + 112, "Automation mode", "Smart Mode (dusk/dawn)", 340)
    draw.text((40, y + 168), "Smart Mode is on — on at sunset, off at sunrise.", fill=muted, font=small_font)
    field(40, y + 192, "Turn ON at", "18:00", 180)
    field(240, y + 192, "Turn OFF at", "23:00", 180)
    y += h2 + 14

    h3 = 150
    card_block(y, "Home location", "Used for Smart Mode on devices.", h3)
    field(40, y + 58, "Latitude", "50.891800", 180)
    field(240, y + 58, "Longitude", "-1.396900", 180)
    draw.rounded_rectangle((440, y + 76, 560, y + 112), radius=10, fill=(38, 44, 56), outline=(70, 78, 92))
    draw.text((458, y + 88), "Detect Location", fill=text, font=body_font)
    y += h3 + 14

    h4 = 130
    card_block(y, "Screen brightness", "Local monitor backlight. Enable from the tray menu.", h4)
    draw.text((40, y + 62), "Night (%)", fill=muted, font=label_font)
    draw.rounded_rectangle((40, y + 82, 280, y + 92), radius=4, fill=(40, 48, 62))
    draw.rounded_rectangle((40, y + 82, 120, y + 92), radius=4, fill=accent)
    draw.text((290, y + 78), "20", fill=text, font=body_font)
    y += h4 + 14

    draw.rectangle((0, h - 64, w, h), fill=(8, 11, 16))
    draw.line([(0, h - 64), (w, h - 64)], fill=(40, 48, 62))
    draw.rounded_rectangle((w - 210, h - 48, w - 120, h - 18), radius=10, outline=(70, 78, 92))
    draw.text((w - 192, h - 40), "Cancel", fill=text, font=body_font)
    draw.rounded_rectangle((w - 108, h - 48, w - 28, h - 18), radius=10, fill=(212, 137, 58))
    draw.text((w - 82, h - 40), "Save", fill=(27, 18, 8), font=body_font)

    return img


def capture_settings_dialog(out: Path | None = None, *, close_after: bool = True) -> bool:
    """Capture the WebView2 settings window from a running DuskPlug instance."""
    target = out or (OUT / "settings-dialog.png")
    main_hwnd = find_window("DuskPlugWindow")
    if not main_hwnd:
        for exe in (ROOT / "DuskPlug.exe", Path(r"C:\Program Files\DuskPlug\DuskPlug.exe")):
            if exe.is_file():
                subprocess.Popen([str(exe)], cwd=str(exe.parent))
                break
        else:
            subprocess.Popen([str(ROOT / "DuskPlug.exe")], cwd=str(ROOT))
        main_hwnd = wait_window("DuskPlugWindow", timeout=20)
    if not main_hwnd:
        return False

    user32.PostMessageW(main_hwnd, WM_COMMAND, CMD_SETTINGS, 0)
    settings = wait_window("DuskPlugSettingsHtml", timeout=12)
    if not settings:
        settings = wait_window(title="DuskPlug Settings", timeout=5)
    if not settings:
        return False

    user32.SetForegroundWindow(settings)
    time.sleep(2.5)
    save_window(settings, target)
    if close_after:
        user32.PostMessageW(settings, WM_CLOSE, 0, 0)
    return True


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    # Tray screenshots are real Windows captures kept in docs/screenshots/source/.
    # Do not regenerate tray-menu.png or tray-in-taskbar.png from mockups.

    if capture_settings_dialog():
        print("Captured live settings window.")
    else:
        render_settings_mockup().save(OUT / "settings-dialog.png")
        print("Saved settings mockup (live WebView2 capture unavailable).")

    print(f"Screenshots saved to {OUT}")


if __name__ == "__main__":
    main()
