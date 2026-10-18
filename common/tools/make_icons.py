#!/usr/bin/env python3
# Renders apps/<app>/assets-cg/icon-uns.png and icon-sel.png: a rounded
# panel in the app's colour with a white pictogram, like the Base-N icon.
# The selected icon has the cyan gradient the OS menu uses behind it.
#   common/tools/make_icons.py              all apps listed in ICONS
#   common/tools/make_icons.py electronics  only these apps
# Apps not listed (base-n has a hand-drawn icon) are left alone.
import math
import os
import sys
from PIL import Image, ImageDraw, ImageFont

W, H = 92, 64
S = 4                                  # drawn at 4x, then downsampled
PANEL = (5, 5, 86, 43)                 # same box as the Base-N panel
WHITE = (255, 255, 255)
ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def font(px):
    try:
        return ImageFont.truetype(FONT, px * S)
    except OSError:
        return ImageFont.load_default()


def box(x1, y1, x2, y2):
    """Icon coordinates -> canvas coordinates."""
    return [x1 * S, y1 * S, x2 * S, y2 * S]


def text(d, cx, cy, s, px, fill=WHITE):
    d.text((cx * S, cy * S), s, font=font(px), fill=fill, anchor="mm")


# --- Pictograms: draw in icon coordinates inside the panel (5..86 x 5..43)

def draw_electronics(d, c):
    # Resistor with coloured bands
    d.rectangle(box(9, 23, 82, 25), fill=WHITE)
    d.rounded_rectangle(box(22, 14, 69, 34), radius=7 * S, fill=(242, 222, 178))
    d.rectangle(box(31, 17, 60, 31), fill=(242, 222, 178))
    for x, col in ((29, (250, 210, 0)), (36, (130, 40, 180)),
                   (43, (220, 20, 20)), (58, (205, 160, 40))):
        d.rectangle(box(x, 14.5, x + 3.5, 33.5), fill=col)


def draw_hello(d, c):
    # Speech bubble saying "Hi"
    d.rounded_rectangle(box(24, 9, 67, 33), radius=7 * S, fill=WHITE)
    d.polygon([(32 * S, 32 * S), (30 * S, 40 * S), (41 * S, 32 * S)],
              fill=WHITE)
    text(d, 45.5, 21, "Hi!", 15, c)


def draw_draw_syscalls(d, c):
    # Square, circle and triangle outlines
    w = 2 * S
    d.rectangle(box(12, 14, 30, 32), outline=WHITE, width=w)
    d.ellipse(box(36, 14, 54, 32), outline=WHITE, width=w)
    d.polygon([(69 * S, 13 * S), (59 * S, 32 * S), (79 * S, 32 * S)],
              outline=WHITE, width=w)


def draw_key_modifiers(d, c):
    # SHIFT and ALPHA keys in their keyboard colours
    for x, label, col in ((14, "SHIFT", (230, 180, 0)),
                          (49, "ALPHA", (220, 30, 30))):
        d.rounded_rectangle(box(x, 13, x + 28, 35), radius=4 * S, fill=WHITE)
        d.rounded_rectangle(box(x + 2, 15, x + 26, 33), radius=3 * S, fill=col)
        text(d, x + 14, 24, label, 6)


def gear(d, cx, cy, r, teeth, fill, hole, hole_fill):
    pts = []
    for i in range(teeth * 4):
        a = 2 * math.pi * i / (teeth * 4)
        rr = r if (i % 4) in (0, 1) else r * 0.78
        pts.append(((cx + rr * math.cos(a)) * S, (cy + rr * math.sin(a)) * S))
    d.polygon(pts, fill=fill)
    d.ellipse(box(cx - hole, cy - hole, cx + hole, cy + hole), fill=hole_fill)


def draw_os_syscalls(d, c):
    # Gear with "OS" in the middle
    gear(d, 45.5, 24, 16, 10, WHITE, 9, c)
    text(d, 45.5, 24.5, "OS", 8)


def draw_file_syscalls(d, c):
    # Folder with a document
    d.rounded_rectangle(box(20, 12, 40, 20), radius=2 * S, fill=WHITE)
    d.rounded_rectangle(box(20, 16, 71, 36), radius=3 * S, fill=WHITE)
    d.rectangle(box(46, 9, 64, 30), fill=c)
    d.rectangle(box(48, 11, 62, 28), fill=WHITE)
    for y in (15, 19, 23):
        d.rectangle(box(51, y, 59, y + 1), fill=c)


def draw_status_bar(d, c):
    # Status bar strip with a battery and signal-style bars
    d.rounded_rectangle(box(11, 10, 80, 19), radius=2 * S, fill=WHITE)
    d.rectangle(box(15, 12, 29, 17), fill=c)
    d.rectangle(box(16, 13, 22, 16), fill=WHITE)
    d.rectangle(box(29, 13.5, 30, 15.5), fill=c)
    for i, h in enumerate((2, 3, 4)):
        x = 66 + 4 * i
        d.rectangle(box(x, 17 - h, x + 2, 17), fill=c)
    for y in (24, 30):
        d.rounded_rectangle(box(11, y, 80 - (y - 24) * 3, y + 3),
                            radius=1 * S, fill=WHITE)


def draw_net_tools(d, c):
    # Small globe above "127.0.0.1"
    w = int(1.5 * S)
    d.ellipse(box(37, 7, 54, 24), outline=WHITE, width=w)
    d.ellipse(box(42, 7, 49, 24), outline=WHITE, width=w)
    d.line(box(45.5, 7, 45.5, 24), fill=WHITE, width=w)
    for y, dx in ((11.5, 6.5), (15.5, 8.5), (19.5, 6.5)):
        d.line(box(45.5 - dx, y, 45.5 + dx, y), fill=WHITE, width=w)
    text(d, 45.5, 33.5, "127.0.0.1", 12)


# app folder -> (panel colour, pictogram)
ICONS = {
    "electronics":   ((168, 76, 26), draw_electronics),
    "hello":         ((46, 150, 70), draw_hello),
    "draw-syscalls": ((140, 50, 180), draw_draw_syscalls),
    "key-modifiers": ((70, 76, 90), draw_key_modifiers),
    "os-syscalls":   ((30, 130, 140), draw_os_syscalls),
    "file-syscalls": ((215, 120, 20), draw_file_syscalls),
    "status-bar":    ((40, 70, 160), draw_status_bar),
    "net-tools":     ((24, 112, 160), draw_net_tools),
}


def gradient():
    """Cyan background of a selected icon, lighter at the top left."""
    img = Image.new("RGB", (W, H))
    for y in range(H):
        for x in range(W):
            t = (x / W + y / H) / 2
            img.putpixel((x, y), (int(150 - 90 * t), int(226 - 32 * t), 255))
    return img


def render(color, draw):
    big = Image.new("RGBA", (W * S, H * S), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    d.rounded_rectangle(box(*PANEL), radius=5 * S, fill=color)
    draw(d, color)
    small = big.resize((W, H), Image.LANCZOS)

    uns = Image.new("RGB", (W, H), WHITE)
    uns.paste(small, (0, 0), small)
    sel = gradient()
    sel.paste(small, (0, 0), small)
    return uns, sel


def main(apps):
    for app in apps:
        if app not in ICONS:
            print(f"no icon defined for {app}, skipped")
            continue
        out = os.path.join(ROOT, "apps", app, "assets-cg")
        uns, sel = render(*ICONS[app])
        uns.save(os.path.join(out, "icon-uns.png"))
        sel.save(os.path.join(out, "icon-sel.png"))
        print(f"wrote apps/{app}/assets-cg/icon-{{uns,sel}}.png")


if __name__ == "__main__":
    main(sys.argv[1:] or list(ICONS))
