"""Generate high-quality README screenshots for DuskPlug."""
from __future__ import annotations

import ctypes
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
    """Draw a Windows-style menu checkmark (not a Unicode glyph)."""
    draw.line(
        [(x + 2, y + size // 2), (x + size // 2 - 1, y + size - 3), (x + size - 2, y + 3)],
        fill=(20, 20, 20),
        width=2,
        joint="curve",
    )


def render_tray_strip() -> Image.Image:
    """Taskbar-style strip with crisp 32px tray icons."""
    icon_size = 32
    pad = 16
    labels = [
        ("Manual ON", render_icon(icon_size, on=True)),
        ("Smart ON", render_icon(icon_size, on=True, smart=True)),
        ("OFF", render_icon(icon_size, on=False)),
    ]
    label_font = _font(15)
    w = len(labels) * (icon_size + 56) + pad * 2
    h = 120
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    # Windows 11 dark taskbar strip
    strip_h = 48
    draw.rounded_rectangle((0, 0, w - 1, strip_h), radius=8, fill=(32, 32, 32, 255))

    x = pad + 8
    cy = strip_h // 2
    for _label, icon in labels:
        img.paste(icon, (x, cy - icon_size // 2), icon)
        x += icon_size + 56

    # Labels below strip
    x = pad + 8
    for label, _icon in labels:
        bbox = draw.textbbox((0, 0), label, font=label_font)
        tw = bbox[2] - bbox[0]
        draw.text((x + (icon_size - tw) // 2, strip_h + 14), label, fill=(60, 60, 60), font=label_font)
        x += icon_size + 56

    return img


def render_context_menu() -> Image.Image:
    """Windows-style context menu with bitmap checkmark on Smart Mode."""
    menu_font = _font(14)
    check_col = 28
    text_x = check_col + 8
    line_h = 32
    pad = 4
    w = 240

    items: list[tuple[str, bool] | None] = [
        ("Turn On", False),
        ("Turn Off", False),
        ("Smart Mode", True),
        ("Schedule Mode", False),
        ("Settings...", False),
        ("Refresh Status", False),
        None,
        ("Restart", False),
        ("Exit", False),
    ]
    lines = [i for i in items if i is not None]
    h = pad * 2 + len(lines) * line_h + 6

    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle((0, 0, w - 1, h - 1), radius=8, fill=(252, 252, 252, 255), outline=(200, 200, 200))

    y = pad
    for item in items:
        if item is None:
            draw.line([(12, y + 2), (w - 12, y + 2)], fill=(220, 220, 220), width=1)
            y += 6
            continue
        label, checked = item
        if checked:
            _draw_menu_check(draw, 8, y + 9, 13)
        draw.text((text_x, y + 6), label, fill=(25, 25, 25), font=menu_font)
        y += line_h

    return img


def find_window(class_name: str | None = None, title: str | None = None) -> int:
    if class_name:
        hwnd = user32.FindWindowW(class_name, None)
        if hwnd:
            return hwnd
    if title:
        return user32.FindWindowW(None, title)
    return 0


def wait_window(class_name: str | None = None, title: str | None = None, timeout: float = 10) -> int:
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


def capture_settings_dialog() -> None:
    main_hwnd = find_window("DuskPlugWindow")
    if not main_hwnd:
        import subprocess
        subprocess.Popen([str(ROOT / "DuskPlug.exe")], cwd=str(ROOT))
        main_hwnd = wait_window("DuskPlugWindow", timeout=15)
    if not main_hwnd:
        return

    user32.PostMessageW(main_hwnd, WM_COMMAND, CMD_SETTINGS, 0)
    time.sleep(0.8)
    settings = wait_window(title="DuskPlug Settings", timeout=8)
    if settings:
        user32.SetForegroundWindow(settings)
        time.sleep(0.3)
        save_window(settings, OUT / "settings-dialog.png")
        user32.PostMessageW(settings, WM_CLOSE, 0, 0)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    render_tray_strip().save(OUT / "tray-modes.png")
    render_context_menu().save(OUT / "tray-menu.png")

    # Individual high-res icons for reuse
    render_icon(128, on=True).save(OUT / "tray-manual-on.png")
    render_icon(128, on=True, smart=True).save(OUT / "tray-smart-on.png")
    render_icon(128, on=False).save(OUT / "tray-off.png")

    capture_settings_dialog()
    print(f"Screenshots saved to {OUT}")


if __name__ == "__main__":
    main()
