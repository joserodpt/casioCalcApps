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
    # A character tile and its codes, like one row of the table
    d.rounded_rectangle(box(14, 9, 42, 39), radius=4 * S, fill=WHITE)
    text(d, 28, 24.5, "A", 22, c)
    text(d, 63, 17, "65", 13)
    text(d, 63, 31.5, "0x41", 11)


def draw_timer(d, c):
    # Stopwatch: crown and side button, ticks, two hands, inside the panel
    w = 2 * S
    cx, cy, r = 46, 27, 12
    d.rounded_rectangle(box(42, 8, 50, 11), radius=S, fill=WHITE)
    d.line(box(46, 11, 46, 15), fill=WHITE, width=w)
    d.line(box(55, 15, 58, 12), fill=WHITE, width=w)
    d.ellipse(box(cx - r, cy - r, cx + r, cy + r), outline=WHITE, width=w)
    for i in range(12):
        a = math.radians(i * 30)
        r1 = r - (4 if i % 3 == 0 else 2.5)
        d.line(box(cx + r1 * math.sin(a), cy - r1 * math.cos(a),
                   cx + (r - 1.5) * math.sin(a), cy - (r - 1.5) * math.cos(a)),
               fill=WHITE, width=S)
    d.line(box(cx, cy, cx, cy - 8), fill=WHITE, width=w)
    d.line(box(cx, cy, cx + 6, cy + 3), fill=WHITE, width=w)
    d.ellipse(box(cx - 1.5, cy - 1.5, cx + 1.5, cy + 1.5), fill=WHITE)


def draw_logic(d, c):
    # A 2x2 Karnaugh map (three 1s, one 0) next to an AND gate
    s, gx, gy = 13, 13, 11
    for cx, cy, on in ((0, 0, False), (1, 0, True), (0, 1, True),
                       (1, 1, True)):
        x1, y1 = gx + cx * (s + 2), gy + cy * (s + 2)
        if on:
            d.rounded_rectangle(box(x1, y1, x1 + s, y1 + s), radius=2 * S,
                                fill=WHITE)
            text(d, x1 + s / 2, y1 + s / 2, "1", 9, c)
        else:
            d.rounded_rectangle(box(x1, y1, x1 + s, y1 + s), radius=2 * S,
                                outline=WHITE, width=S)
            text(d, x1 + s / 2, y1 + s / 2, "0", 9)
    # AND gate: flat back, round front, two inputs, one output
    w = 2 * S
    x0, y0, y1 = 56, 15, 33
    d.line(box(48, 19.5, x0, 19.5), fill=WHITE, width=w)
    d.line(box(48, 28.5, x0, 28.5), fill=WHITE, width=w)
    d.chord(box(x0 + 1, y0, x0 + 19, y1), 270, 90, fill=WHITE)
    d.rectangle(box(x0, y0, x0 + 10, y1), fill=WHITE)
    d.line(box(x0 + 19, 24, 82, 24), fill=WHITE, width=w)


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
    # An atom: nucleus, three orbits and their electrons
    cx, cy, rx, ry = 46, 24, 17, 6
    w = int(1.6 * S)
    for ang, e in ((0, 0), (60, 140), (-60, 260)):
        a = math.radians(ang)
        def at(t):
            ex, ey = rx * math.cos(t), ry * math.sin(t)
            return (cx + ex * math.cos(a) - ey * math.sin(a),
                    cy + ex * math.sin(a) + ey * math.cos(a))
        pts = [tuple(v * S for v in at(math.radians(i)))
               for i in range(0, 361, 3)]
        d.line(pts, fill=WHITE, width=w, joint="curve")
        x, y = at(math.radians(e))
        d.ellipse(box(x - 2.2, y - 2.2, x + 2.2, y + 2.2), fill=WHITE)
    d.ellipse(box(cx - 4, cy - 4, cx + 4, cy + 4), fill=WHITE)


def draw_circuit(d, c):
    # A loop: battery on the left, resistor on top, glowing lamp on the right
    w = 2 * S
    x1, y1, x2, y2 = 20, 12, 72, 37
    d.line(box(x1, y1, x2, y1), fill=WHITE, width=w)
    d.line(box(x1, y2, x2, y2), fill=WHITE, width=w)
    d.line(box(x1, y1, x1, y2), fill=WHITE, width=w)
    d.line(box(x2, y1, x2, y2), fill=WHITE, width=w)
    # battery: gap in the left wire, long and short plates
    d.rectangle(box(x1 - 3, 21, x1 + 3, 28), fill=c)
    d.rectangle(box(x1 - 7, 22, x1 + 7, 23), fill=WHITE)
    d.rectangle(box(x1 - 4, 26, x1 + 4, 28), fill=WHITE)
    # resistor on the top wire
    d.rectangle(box(38, y1 - 4, 54, y1 + 4), fill=(242, 222, 178),
                outline=WHITE, width=S)
    # lamp on the right wire, with a glow
    lx, ly = x2, 24.5
    d.ellipse(box(lx - 9, ly - 9, lx + 9, ly + 9), fill=(250, 220, 90))
    d.ellipse(box(lx - 6, ly - 6, lx + 6, ly + 6), fill=(255, 250, 200),
              outline=WHITE, width=S)
    d.line(box(lx - 3.5, ly - 3.5, lx + 3.5, ly + 3.5), fill=c, width=S)
    d.line(box(lx - 3.5, ly + 3.5, lx + 3.5, ly - 3.5), fill=c, width=S)


def draw_tone(d, c):
    # A speaker giving out sound waves
    d.rectangle(box(20, 18, 28, 30), fill=WHITE)
    d.polygon([(28 * S, 18 * S), (40 * S, 9 * S), (40 * S, 39 * S),
               (28 * S, 30 * S)], fill=WHITE)
    w = int(2.2 * S)
    for r in (8, 15, 22):
        d.arc(box(36 - r, 24 - r, 36 + r, 24 + r), -42, 42, fill=WHITE,
              width=w)


def draw_ac_circuit(d, c):
    # An AC source on the left feeding a coil and a capacitor
    w = 2 * S
    x1, y1, x2, y2 = 18, 11, 74, 37
    d.line(box(x1, y1, x2, y1), fill=WHITE, width=w)
    d.line(box(x1, y2, x2, y2), fill=WHITE, width=w)
    d.line(box(x1, y1, x1, y2), fill=WHITE, width=w)
    d.line(box(x2, y1, x2, y2), fill=WHITE, width=w)
    # source: a circle with a sine wave
    cy = 24
    d.ellipse(box(x1 - 8, cy - 8, x1 + 8, cy + 8), fill=c, outline=WHITE,
              width=S)
    pts = [((x1 - 5 + 10 * t / 40) * S,
            (cy - 3.2 * math.sin(2 * math.pi * t / 40)) * S)
           for t in range(41)]
    d.line(pts, fill=WHITE, width=int(1.4 * S), joint="curve")
    # coil on the top wire
    d.rectangle(box(36, y1 - 3, 56, y1 + 3), fill=c)
    for k in range(4):
        cx = 38.5 + 5 * k
        d.arc(box(cx - 2.5, y1 - 4, cx + 2.5, y1 + 4), 180, 360, fill=WHITE,
              width=int(1.6 * S))
    # capacitor on the right wire
    d.rectangle(box(x2 - 4, 21, x2 + 4, 27), fill=c)
    d.rectangle(box(x2 - 7, 20.5, x2 + 7, 22), fill=WHITE)
    d.rectangle(box(x2 - 7, 26, x2 + 7, 27.5), fill=WHITE)


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
    "circuit":       ((16, 130, 98), draw_circuit),
    "tone-generator": ((160, 32, 160), draw_tone),
    "ac-circuit":    ((160, 24, 64), draw_ac_circuit),
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
