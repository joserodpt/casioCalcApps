#!/usr/bin/env python3
# Renders apps/<app>/assets-fx/icon.png: the 30x19 one-bit menu icon of the
# fx-9860G (.g1a) version, drawn pixel by pixel with a 3x5 font.
#   common/tools/make_icons_fx.py           all apps listed in ICONS
#   common/tools/make_icons_fx.py timer     only these apps
import math
import os
import sys
from PIL import Image

W, H = 30, 19
ROOT = os.path.join(os.path.dirname(__file__), "..", "..")

# 3x5 glyphs, rows top to bottom
FONT = {
    "0": ["###", "#.#", "#.#", "#.#", "###"],
    "1": [".#.", "##.", ".#.", ".#.", "###"],
    "2": ["##.", "..#", ".#.", "#..", "###"],
    "4": ["#.#", "#.#", "###", "..#", "..#"],
    "5": ["###", "#..", "##.", "..#", "##."],
    "6": [".##", "#..", "###", "#.#", "###"],
    "7": ["###", "..#", ".#.", ".#.", ".#."],
    "F": ["###", "#..", "##.", "#..", "#.."],
    "H": ["#.#", "#.#", "###", "#.#", "#.#"],
    "I": ["###", ".#.", ".#.", ".#.", "###"],
    "P": ["##.", "#.#", "##.", "#..", "#.."],
    "x": ["...", "#.#", ".#.", "#.#", "..."],
    "!": [".#.", ".#.", ".#.", "...", ".#."],
    "/": ["..#", "..#", ".#.", "#..", "#.."],
    ".": ["...", "...", "...", "...", ".#."],
}


class Icon:
    def __init__(self):
        self.px = [[0] * W for _ in range(H)]

    def set(self, x, y, v=1):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = v

    def rect(self, x1, y1, x2, y2, v=1, fill=False):
        for y in range(y1, y2 + 1):
            for x in range(x1, x2 + 1):
                if fill or x in (x1, x2) or y in (y1, y2):
                    self.set(x, y, v)

    def text(self, x, y, s, v=1):
        for ch in s:
            for r, row in enumerate(FONT[ch]):
                for c, p in enumerate(row):
                    if p == "#":
                        self.set(x + c, y + r, v)
            x += 4

    def circle(self, cx, cy, r):
        for k in range(360):
            a = math.radians(k)
            self.set(round(cx + r * math.cos(a)), round(cy + r * math.sin(a)))

    def save(self, path):
        img = Image.new("RGB", (W, H), "white")
        for y in range(H):
            for x in range(W):
                if self.px[y][x]:
                    img.putpixel((x, y), (0, 0, 0))
        img.save(path)


def text_width(s):
    return 4 * len(s) - 1


# --- Pictograms

def icon_hello(i):
    # Speech bubble saying "HI!"
    i.rect(5, 1, 24, 12)
    for c in ((5, 1), (24, 1), (5, 12), (24, 12)):
        i.set(*c, 0)
    i.text(15 - text_width("HI!") // 2, 4, "HI!")
    # Tail: from the bubble's bottom edge down to a point at the left
    for k in range(5):
        i.set(8, 12 + k)
        i.set(12 - k, 12 + k)
    i.rect(9, 12, 11, 12, v=0)


def icon_base_n(i):
    # Binary above hex, in a frame
    i.rect(0, 0, 29, 18)
    i.text(15 - text_width("1010") // 2, 3, "1010")
    i.text(15 - text_width("0x2F") // 2, 11, "0x2F")


def icon_net_tools(i):
    # "IP" over a /24 prefix, with a small network of three linked nodes
    i.text(2, 2, "IP")
    i.text(2, 11, "/24")
    nodes = ((21, 3), (26, 12), (16, 14))
    for (x1, y1), (x2, y2) in ((nodes[0], nodes[1]), (nodes[1], nodes[2]),
                               (nodes[2], nodes[0])):
        n = max(abs(x2 - x1), abs(y2 - y1))
        for k in range(n + 1):
            i.set(round(x1 + (x2 - x1) * k / n), round(y1 + (y2 - y1) * k / n))
    for x, y in nodes:
        i.rect(x - 1, y - 1, x + 1, y + 1, fill=True)


def icon_timer(i):
    # Stopwatch: crown, dial, ticks and hands
    cx, cy, r = 15, 10, 7
    i.rect(13, 0, 17, 1, fill=True)
    i.set(15, 2)
    i.set(22, 3)
    i.set(23, 2)
    i.circle(cx, cy, r)
    for k in range(4):
        a = math.radians(90 * k)
        i.set(round(cx + (r - 2) * math.cos(a)), round(cy + (r - 2) * math.sin(a)))
    for k in range(5):
        i.set(cx, cy - k)
    for k in range(4):
        i.set(cx + k, cy + k // 2)


def icon_ascii(i):
    # A white "A" on a black tile, then its code 65 (decimal / hex)
    i.rect(1, 1, 13, 17, fill=True)
    a = [".....###.....", "....##.##....", "...##...##...", "...##...##...",
         "..##.....##..", "..#########..", "..##.....##..", ".##.......##.",
         ".##.......##."]
    for r, row in enumerate(a):
        for c, p in enumerate(row):
            if p == "#":
                i.set(1 + c, 4 + r, 0)
    i.text(17, 4, "65")
    i.text(17, 11, "41")


def line(i, x1, y1, x2, y2):
    n = max(abs(x2 - x1), abs(y2 - y1), 1)
    for k in range(n + 1):
        i.set(round(x1 + (x2 - x1) * k / n), round(y1 + (y2 - y1) * k / n))


def icon_physics(i):
    # An atom: nucleus and three orbits
    cx, cy = 15, 9
    for ang in (0, 60, -60):
        a = math.radians(ang)
        for k in range(0, 360, 4):
            t = math.radians(k)
            ex, ey = 9 * math.cos(t), 3 * math.sin(t)
            i.set(round(cx + ex * math.cos(a) - ey * math.sin(a)),
                  round(cy + ex * math.sin(a) + ey * math.cos(a)))
    i.rect(cx - 1, cy - 1, cx + 1, cy + 1, fill=True)


def icon_chemistry(i):
    # A conical flask, half full
    i.rect(12, 0, 18, 0)
    line(i, 13, 1, 13, 6)
    line(i, 17, 1, 17, 6)
    line(i, 13, 6, 6, 17)
    line(i, 17, 6, 24, 17)
    line(i, 6, 18, 24, 18)
    for y in range(11, 18):
        half = 2 + (y - 6) * 7 // 11
        i.rect(15 - half + 1, y, 15 + half - 1, y, fill=True)
    i.set(13, 14, 0)
    i.set(17, 13, 0)
    i.set(15, 16, 0)


def icon_electronics(i):
    # A resistor with three bands, on its leads
    line(i, 0, 9, 29, 9)
    i.rect(7, 5, 22, 13, v=0, fill=True)
    i.rect(7, 5, 22, 13)
    i.set(7, 5, 0); i.set(22, 5, 0); i.set(7, 13, 0); i.set(22, 13, 0)
    for x in (10, 13, 16):
        i.rect(x, 6, x + 1, 12, fill=True)
    i.rect(19, 6, 19, 12, fill=True)


def icon_tone(i):
    # A speaker and three sound waves
    i.rect(5, 7, 8, 11, fill=True)
    for k in range(5):
        i.rect(9 + k, 6 - k, 9 + k, 12 + k, fill=True)
    for r in (4, 8, 12):
        for k in range(-40, 41, 4):
            a = math.radians(k)
            i.set(round(14 + r * math.cos(a)), round(9 + r * math.sin(a)))


ICONS = {
    "tone-generator": icon_tone,
    "physics": icon_physics,
    "chemistry": icon_chemistry,
    "electronics": icon_electronics,
    "hello": icon_hello,
    "base-n": icon_base_n,
    "net-tools": icon_net_tools,
    "timer": icon_timer,
    "ascii-table": icon_ascii,
}


def main(apps):
    for app in apps:
        if app not in ICONS:
            print(f"no fx icon defined for {app}, skipped")
            continue
        icon = Icon()
        ICONS[app](icon)
        out = os.path.join(ROOT, "apps", app, "assets-fx")
        os.makedirs(out, exist_ok=True)
        icon.save(os.path.join(out, "icon.png"))
        print(f"wrote apps/{app}/assets-fx/icon.png")


if __name__ == "__main__":
    main(sys.argv[1:] or list(ICONS))
