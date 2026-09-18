"""Generate DuskPlug tray and application icons."""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
ASSETS = ROOT / "assets"
SCREENSHOTS = ROOT / "docs" / "screenshots"

# DuskPlug palette
ON_TOP = (255, 196, 92)
ON_BOTTOM = (232, 128, 40)
ON_OUTLINE = (120, 62, 18)
OFF_TOP = (156, 163, 175)
OFF_BOTTOM = (75, 85, 99)
OFF_OUTLINE = (55, 65, 81)
SMART_RING = (99, 102, 241)
SMART_RING_DIM = (129, 140, 248)
FILAMENT_ON = (255, 244, 200)
FILAMENT_OFF = (107, 114, 128)


def _lerp(a: float, b: float, t: float) -> float:
    return a + (b - a) * t


def _lerp_color(c1: tuple[int, int, int], c2: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return (
        int(_lerp(c1[0], c2[0], t)),
        int(_lerp(c1[1], c2[1], t)),
        int(_lerp(c1[2], c2[2], t)),
    )


def _vertical_gradient(size: int, top: tuple[int, int, int], bottom: tuple[int, int, int]) -> Image.Image:
    grad = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = grad.load()
    for y in range(size):
        t = y / max(size - 1, 1)
        color = _lerp_color(top, bottom, t) + (255,)
        for x in range(size):
            px[x, y] = color
    return grad


def _mask_from_draw(size: int, draw_fn) -> Image.Image:
    mask = Image.new("L", (size, size), 0)
    draw_fn(ImageDraw.Draw(mask))
    return mask


def _draw_bulb_mask(draw: ImageDraw.ImageDraw, size: int) -> None:
    s = size
    cx = s * 0.5
    bulb_w = s * 0.50
    bulb_h = s * 0.50
    bulb_top = s * 0.12
    # Pear-shaped glass
    draw.ellipse(
        (cx - bulb_w / 2, bulb_top, cx + bulb_w / 2, bulb_top + bulb_h),
        fill=255,
    )
    neck_w = s * 0.22
    neck_h = s * 0.06
    neck_top = bulb_top + bulb_h - s * 0.08
    draw.rectangle(
        (cx - neck_w / 2, neck_top, cx + neck_w / 2, neck_top + neck_h),
        fill=255,
    )
    base_w = s * 0.28
    base_h = s * 0.09
    base_top = neck_top + neck_h - s * 0.01
    draw.rounded_rectangle(
        (cx - base_w / 2, base_top, cx + base_w / 2, base_top + base_h),
        radius=max(1, s // 24),
        fill=255,
    )
    pin_w = s * 0.16
    pin_h = s * 0.08
    pin_top = base_top + base_h - s * 0.01
    draw.rounded_rectangle(
        (cx - pin_w / 2, pin_top, cx + pin_w / 2, pin_top + pin_h),
        radius=max(1, s // 28),
        fill=255,
    )


def _draw_filament(draw: ImageDraw.ImageDraw, size: int, on: bool) -> None:
    s = size
    cx = s * 0.5
    cy = s * 0.36
    color = FILAMENT_ON if on else FILAMENT_OFF
    w = max(1, s // 18)
    draw.line((cx, cy - s * 0.10, cx, cy + s * 0.02), fill=color, width=w)
    draw.arc(
        (cx - s * 0.08, cy - s * 0.04, cx + s * 0.08, cy + s * 0.12),
        start=15,
        end=165,
        fill=color,
        width=w,
    )


def _draw_smart_ring(draw: ImageDraw.ImageDraw, size: int, on: bool) -> None:
    s = size
    cx = cy = s * 0.5
    radius = s * 0.44
    color = SMART_RING if on else SMART_RING_DIM
    w = max(1, s // 14)
    # Upper arc — dusk horizon
    draw.arc(
        (cx - radius, cy - radius, cx + radius, cy + radius),
        start=200,
        end=340,
        fill=color,
        width=w,
    )
    # Small crescent dot
    dot_r = max(1, s // 18)
    angle = math.radians(235)
    dx = math.cos(angle) * radius
    dy = math.sin(angle) * radius
    draw.ellipse(
        (cx + dx - dot_r, cy + dy - dot_r, cx + dx + dot_r, cy + dy + dot_r),
        fill=color,
    )


def render_icon(size: int, *, on: bool, smart: bool = False) -> Image.Image:
    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    if on:
        glow = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        gdraw = ImageDraw.Draw(glow)
        cx = size * 0.5
        glow_r = size * 0.34
        gdraw.ellipse(
            (cx - glow_r, size * 0.16, cx + glow_r, size * 0.16 + glow_r * 1.6),
            fill=(255, 170, 60, 90 if size >= 24 else 70),
        )
        glow = glow.filter(ImageFilter.GaussianBlur(radius=max(1, size // 10)))
        canvas = Image.alpha_composite(canvas, glow)

    mask = _mask_from_draw(size, lambda d: _draw_bulb_mask(d, size))
    top = ON_TOP if on else OFF_TOP
    bottom = ON_BOTTOM if on else OFF_BOTTOM
    gradient = _vertical_gradient(size, top, bottom)
    bulb = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    bulb.paste(gradient, (0, 0), mask)

    outline = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    odraw = ImageDraw.Draw(outline)
    outline_color = ON_OUTLINE if on else OFF_OUTLINE
    w = max(1, size // 16)
    cx = size * 0.5
    bulb_w = size * 0.50
    bulb_h = size * 0.50
    bulb_top = size * 0.12
    odraw.ellipse(
        (cx - bulb_w / 2, bulb_top, cx + bulb_w / 2, bulb_top + bulb_h),
        outline=outline_color,
        width=w,
    )
    neck_w = size * 0.22
    neck_h = size * 0.06
    neck_top = bulb_top + bulb_h - size * 0.08
    odraw.rectangle(
        (cx - neck_w / 2, neck_top, cx + neck_w / 2, neck_top + neck_h),
        outline=outline_color,
        width=w,
    )
    base_w = size * 0.28
    base_h = size * 0.09
    base_top = neck_top + neck_h - size * 0.01
    odraw.rounded_rectangle(
        (cx - base_w / 2, base_top, cx + base_w / 2, base_top + base_h),
        radius=max(1, size // 24),
        outline=outline_color,
        width=w,
    )
    pin_w = size * 0.16
    pin_h = size * 0.08
    pin_top = base_top + base_h - size * 0.01
    odraw.rounded_rectangle(
        (cx - pin_w / 2, pin_top, cx + pin_w / 2, pin_top + pin_h),
        radius=max(1, size // 28),
        outline=outline_color,
        width=w,
    )

    canvas = Image.alpha_composite(canvas, bulb)
    canvas = Image.alpha_composite(canvas, outline)

    if size >= 20:
        fdraw = ImageDraw.Draw(canvas)
        _draw_filament(fdraw, size, on)

    if smart:
        sdraw = ImageDraw.Draw(canvas)
        _draw_smart_ring(sdraw, size, on)

    if on and size >= 24:
        highlight = ImageDraw.Draw(canvas)
        hx = cx - bulb_w * 0.15
        hy = bulb_top + bulb_h * 0.18
        hr = size * 0.07
        highlight.ellipse((hx - hr, hy - hr, hx + hr, hy + hr), fill=(255, 255, 255, 110))

    return canvas


def _with_app_tile(icon: Image.Image, size: int) -> Image.Image:
    """Dusk gradient tile for Start menu / installer icon."""
    if size < 48:
        return icon
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    margin = size * 0.06
    radius = size * 0.18
    rect = (margin, margin, size - margin, size - margin)
    for y in range(int(margin), int(size - margin)):
        t = (y - margin) / max(size - 2 * margin, 1)
        color = _lerp_color((30, 27, 75), (67, 56, 120), t) + (255,)
        draw.line((margin, y, size - margin, y), fill=color)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(rect, radius=radius, fill=255)
    bg = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    bg.paste(tile, (0, 0), mask)

    scale = 0.62 if size >= 128 else 0.68
    inner = int(size * scale)
    scaled = icon.resize((inner, inner), Image.Resampling.LANCZOS)
    offset = (size - inner) // 2
    bg.alpha_composite(scaled, (offset, offset))
    return bg


def save_ico(path: Path, renderer, *, sizes: list[int]) -> None:
    """Write a multi-resolution ICO with PNG-encoded frames (Windows Vista+)."""
    import io
    import struct

    pngs: list[tuple[int, bytes]] = []
    for size in sorted(sizes):
        buf = io.BytesIO()
        renderer(size).save(buf, format="PNG")
        pngs.append((size, buf.getvalue()))

    count = len(pngs)
    header = struct.pack("<HHH", 0, 1, count)
    entries = bytearray()
    blob = bytearray()
    offset = 6 + 16 * count
    for size, png in pngs:
        w = size if size < 256 else 0
        h = size if size < 256 else 0
        entries.extend(struct.pack("<BBBBHHII", w, h, 0, 0, 1, 32, len(png), offset))
        offset += len(png)
        blob.extend(png)

    path.write_bytes(header + entries + blob)


def render_app_tile(size: int) -> Image.Image:
    return _with_app_tile(render_icon(size, on=True, smart=True), size)


def save_png(path: Path, image: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="PNG", optimize=True)


def main() -> None:
    ASSETS.mkdir(parents=True, exist_ok=True)
    SCREENSHOTS.mkdir(parents=True, exist_ok=True)
    tray_sizes = [16, 20, 24, 32, 48]
    app_sizes = [16, 24, 32, 48, 64, 128, 256]

    save_ico(ASSETS / "light-on.ico", lambda s: render_icon(s, on=True), sizes=tray_sizes)
    save_ico(ASSETS / "light-off.ico", lambda s: render_icon(s, on=False), sizes=tray_sizes)
    save_ico(ASSETS / "light-smart-on.ico", lambda s: render_icon(s, on=True, smart=True), sizes=tray_sizes)
    save_ico(ASSETS / "light-smart-off.ico", lambda s: render_icon(s, on=False, smart=True), sizes=tray_sizes)
    save_ico(ASSETS / "light-smart.ico", lambda s: render_icon(s, on=True, smart=True), sizes=tray_sizes)
    save_ico(ASSETS / "app.ico", render_app_tile, sizes=app_sizes)

    # Small tile for Settings / Timed mode headers (42 px @ 2x).
    save_png(ASSETS / "brand-mark.png", render_app_tile(84))

    # High-resolution PNG exports for docs, store listings, and Buy Me a Coffee.
    for size in (128, 256, 512, 1024):
        save_png(SCREENSHOTS / f"icon-app-{size}.png", render_app_tile(size))

    print(f"Generated icons in {ASSETS}")
    print(f"Generated PNG exports in {SCREENSHOTS}")


if __name__ == "__main__":
    main()
