"""Capture README screenshots from running DuskPlug."""
from __future__ import annotations

import ctypes
import time
from pathlib import Path

from PIL import Image

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

WM_COMMAND = 0x0111
CMD_SETTINGS = 10009
WM_CLOSE = 0x0010
PW_RENDERFULLCONTENT = 2

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "screenshots"
ASSETS = ROOT / "assets"


class RECT(ctypes.Structure):
    _fields_ = [
        ("left", ctypes.c_long),
        ("top", ctypes.c_long),
        ("right", ctypes.c_long),
        ("bottom", ctypes.c_long),
    ]


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


def save_icon(ico: Path, out: Path, size: int = 64) -> None:
    img = Image.open(ico)
    img.resize((size, size), Image.Resampling.LANCZOS).save(out)


def save_window(hwnd: int, out: Path) -> None:
    rect = RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    if w <= 0 or h <= 0:
        raise RuntimeError(f"Invalid window size {w}x{h}")

    hdc_screen = user32.GetDC(0)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_screen, w, h)
    gdi32.SelectObject(hdc_mem, hbmp)
    user32.PrintWindow(hwnd, hdc_mem, PW_RENDERFULLCONTENT)

    bmp = Image.frombuffer(
        "RGB",
        (w, h),
        ctypes.create_string_buffer(w * h * 4),
        "raw",
        "BGRX",
        0,
        1,
    )
    # PrintWindow via PIL is awkward; use ctypes BITMAPINFO instead
    import struct

    class BITMAPINFOHEADER(ctypes.Structure):
        _fields_ = [
            ("biSize", ctypes.c_uint32),
            ("biWidth", ctypes.c_int32),
            ("biHeight", ctypes.c_int32),
            ("biPlanes", ctypes.c_uint16),
            ("biBitCount", ctypes.c_uint16),
            ("biCompression", ctypes.c_uint32),
            ("biSizeImage", ctypes.c_uint32),
            ("biXPelsPerMeter", ctypes.c_int32),
            ("biYPelsPerMeter", ctypes.c_int32),
            ("biClrUsed", ctypes.c_uint32),
            ("biClrImportant", ctypes.c_uint32),
        ]

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0

    buf = (ctypes.c_char * (w * h * 4))()
    gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), 0)
    img = Image.frombuffer("RGBA", (w, h), bytes(buf), "raw", "BGRA", 0, 1)
    img.save(out)

    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(0, hdc_screen)


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    save_icon(ASSETS / "light-on.ico", OUT / "tray-manual-on.png")
    save_icon(ASSETS / "light-smart-on.ico", OUT / "tray-smart-on.png")
    save_icon(ASSETS / "light-off.ico", OUT / "tray-off.png")

    main_hwnd = find_window("DuskPlugWindow")
    if not main_hwnd:
        import subprocess

        subprocess.Popen([str(ROOT / "DuskPlug.exe")], cwd=str(ROOT))
        main_hwnd = wait_window("DuskPlugWindow", timeout=15)

    if main_hwnd:
        user32.PostMessageW(main_hwnd, WM_COMMAND, CMD_SETTINGS, 0)
        time.sleep(0.8)
        settings = wait_window(title="DuskPlug Settings", timeout=8)
        if settings:
            user32.SetForegroundWindow(settings)
            time.sleep(0.3)
            save_window(settings, OUT / "settings-dialog.png")
            user32.PostMessageW(settings, WM_CLOSE, 0, 0)

    print(f"Screenshots saved to {OUT}")


if __name__ == "__main__":
    main()
