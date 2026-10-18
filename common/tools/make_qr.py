#!/usr/bin/env python3
# Renders common/assets-cg/qr_repo.png: a QR code for the GitHub repository,
# shown on every app's info screen. Needs the "qrcode" package (pip install qrcode).
# No quiet zone is drawn: the screen around it is white.
import os
import qrcode
from PIL import Image

URL = "https://github.com/joserodpt/casioPrizmApps"
SCALE = 4  # pixels per module

out = os.path.join(os.path.dirname(__file__), "..", "assets-cg", "qr_repo.png")
qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, border=0)
qr.add_data(URL)
qr.make(fit=True)
matrix = qr.get_matrix()

n = len(matrix)
img = Image.new("RGB", (n * SCALE, n * SCALE), "white")
for y, row in enumerate(matrix):
    for x, dark in enumerate(row):
        if dark:
            img.paste((0, 0, 0), (x * SCALE, y * SCALE,
                (x + 1) * SCALE, (y + 1) * SCALE))
img.save(out)
print(f"wrote {os.path.normpath(out)} ({n}x{n} modules, {img.width}px)")
