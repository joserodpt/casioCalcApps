#!/usr/bin/env python3
"""Set one language's add-in name in a g3a header.

    scripts/g3a-name.py <file.g3a> <lang> <name>

The g3a header has 8 name slots of 24 bytes from offset 0x6B, one per OS
language; fxgxa fills all of them with the same name. <lang> is one of en,
es, de, fr, pt, zh. The checksums are left stale: run `fxgxa -r` after.
"""

import sys

SLOTS = {"en": 0, "es": 1, "de": 2, "fr": 3, "pt": 4, "zh": 5}
BASE, SIZE = 0x6B, 24

path, lang, name = sys.argv[1:4]
raw = name.encode("ascii")
if len(raw) >= SIZE:
    sys.exit(f"g3a-name: '{name}' is longer than {SIZE - 1} characters")

with open(path, "r+b") as f:
    f.seek(BASE + SLOTS[lang] * SIZE)
    f.write(raw.ljust(SIZE, b"\0"))
