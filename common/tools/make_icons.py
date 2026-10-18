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





def draw_net_tools(d, c):
    # Small globe above "127.0.0.1"
    w = int(1.5 * S)
    d.ellipse(box(37, 7, 54, 24), outline=WHITE, width=w)
    d.ellipse(box(42, 7, 49, 24), outline=WHITE, width=w)
    d.line(box(45.5, 7, 45.5, 24), fill=WHITE, width=w)
    for y, dx in ((11.5, 6.5), (15.5, 8.5), (19.5, 6.5)):
        d.line(box(45.5 - dx, y, 45.5 + dx, y), fill=WHITE, width=w)
    text(d, 45.5, 33.5, "127.0.0.1", 12)


def draw_ascii_table(d, c):
    # "Aa" with a code below, suggesting a character table
    text(d, 32, 20, "Aa", 15)
    d.rounded_rectangle(box(50, 24, 80, 38), radius=3 * S, fill=WHITE)
    text(d, 65, 31.5, "41", 9, c)


def draw_timer(d, c):
    # Stopwatch: a circle with a top button and two hands
    w = 2 * S
    d.rectangle(box(42, 6, 50, 10), fill=WHITE)
    d.line(box(46, 10, 46, 14), fill=WHITE, width=w)
    d.ellipse(box(30, 13, 62, 41), outline=WHITE, width=w)
    d.line(box(46, 27, 46, 18), fill=WHITE, width=w)
    d.line(box(46, 27, 54, 31), fill=WHITE, width=w)


def draw_logic(d, c):
    # Mini Karnaugh map: 2x2 grid, three cells filled, 1px gaps between
    cells = [(0, 0, False), (1, 0, True), (0, 1, True), (1, 1, True)]
    gx, gy, s, gap = 33, 9, 12, 1
    d.rounded_rectangle(box(gx - 1, gy - 1, gx + 2 * s + 1, gy + 2 * s + 1),
                        radius=2 * S, fill=WHITE)
    for cx, cy, on in cells:
        x1 = gx + cx * s + gap
        y1 = gy + cy * s + gap
        d.rectangle(box(x1, y1, x1 + s - gap, y1 + s - gap),
                    fill=WHITE if on else c)
    text(d, 46, 37, "AB+C", 9)


def draw_firmware_dump(d, c):
    # Memory chip with a down arrow (dumping out)
    w = 2 * S
    d.rounded_rectangle(box(30, 10, 62, 32), radius=2 * S, outline=WHITE,
                        width=w)
    for i in range(3):
        y = 15 + i * 6
        d.line(box(24, y, 30, y), fill=WHITE, width=S)
        d.line(box(62, y, 68, y), fill=WHITE, width=S)
    text(d, 46, 21, "ROM", 8)
    d.line(box(46, 33, 46, 41), fill=WHITE, width=w)
    d.polygon([(41 * S, 38 * S), (51 * S, 38 * S), (46 * S, 43 * S)],
              fill=WHITE)


def draw_converter(d, c):
    # A conical flask with liquid, plus a conversion double-arrow
    w = 2 * S
    # flask: neck + body (triangle)
    d.line(box(30, 9, 30, 17), fill=WHITE, width=w)
    d.line(box(38, 9, 38, 17), fill=WHITE, width=w)
    d.line(box(28, 9, 40, 9), fill=WHITE, width=w)
    d.polygon([(30*S,17*S),(38*S,17*S),(46*S,36*S),(22*S,36*S)],
              outline=WHITE, width=w)
    d.polygon([(26*S,27*S),(42*S,27*S),(46*S,36*S),(22*S,36*S)], fill=WHITE)
    # conversion arrows on the right
    d.line(box(52, 16, 66, 16), fill=WHITE, width=w)
    d.polygon([(66*S,13*S),(66*S,19*S),(71*S,16*S)], fill=WHITE)
    d.line(box(52, 26, 66, 26), fill=WHITE, width=w)
    d.polygon([(52*S,23*S),(52*S,29*S),(47*S,26*S)], fill=WHITE)
    text(d, 61, 37, "u<>u", 8)


def draw_physics(d, c):
    # An atom: nucleus with two elliptical orbits
    import math as _m
    cx, cy = 46, 24
    d.ellipse(box(cx-3, cy-3, cx+3, cy+3), fill=WHITE)   # nucleus
    # two tilted orbits drawn as point rings
    for ang in (35, -35):
        pts = []
        for i in range(0, 361, 12):
            t = _m.radians(i)
            ex, ey = 17*_m.cos(t), 7*_m.sin(t)
            a = _m.radians(ang)
            rx = ex*_m.cos(a) - ey*_m.sin(a)
            ry = ex*_m.sin(a) + ey*_m.cos(a)
            pts.append(((cx+rx)*S, (cy+ry)*S))
        d.line(pts, fill=WHITE, width=S)
    text(d, 46, 37, "F=ma", 8)


# app folder -> (panel colour, pictogram)
ICONS = {
    "electronics":   ((168, 76, 26), draw_electronics),
    "hello":         ((46, 150, 70), draw_hello),
    "net-tools":     ((24, 112, 160), draw_net_tools),
    "ascii-table":   ((34, 100, 130), draw_ascii_table),
    "timer":         ((30, 80, 170), draw_timer),
    "logic":         ((112, 50, 160), draw_logic),
    "firmware-dump": ((60, 62, 74), draw_firmware_dump),
    "chemistry":     ((18, 92, 120), draw_converter),
    "physics":       ((150, 70, 14), draw_physics),
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
