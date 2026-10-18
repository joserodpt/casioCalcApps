#!/usr/bin/env python3
# Renders assets-cg/font_big.png: a 10x14 monospace font (charset "print",
# 16 glyphs per row) for the large value display. Re-run after changing
# FONT or SIZE; the grid matches assets-cg/fxconv-metadata.txt.
import os
from PIL import Image, ImageDraw, ImageFont

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
SIZE = 16
CELL_W, CELL_H = 10, 14
COLS = 16

out = os.path.join(os.path.dirname(__file__), "..", "assets-cg", "font_big.png")
font = ImageFont.truetype(FONT, SIZE)
chars = [chr(c) for c in range(0x20, 0x7f)]
rows = (len(chars) + COLS - 1) // COLS

img = Image.new("RGB", (COLS * CELL_W, rows * CELL_H), "white")
for i, ch in enumerate(chars):
    # Draw each glyph in its own cell so descenders can't bleed into others
    cell = Image.new("RGB", (CELL_W, CELL_H), "white")
    draw = ImageDraw.Draw(cell)
    draw.fontmode = "1"  # no antialiasing: fxconv keeps only black pixels
    # Shift up so digits/capitals (bbox rows 3..15) sit at the top of the cell
    draw.text((0, -2), ch, font=font, fill="black")
    img.paste(cell, ((i % COLS) * CELL_W, (i // COLS) * CELL_H))
img.save(out)
print("wrote", os.path.normpath(out))
