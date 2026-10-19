ASCII and UTF-8 reference for the Casio FX CG 50 (works with the CG10 and 20 too), with a black and white version for the fx-9860G II. Made with fxSDK and gint.

## Download

| Calculator | File |
|---|---|
| fx-CG50, fx-CG10, fx-CG20 (colour) | [ASCII.g3a](../../dist/g3a/ASCII.g3a) |
| fx-9860G II, fx-9750G II, Graph 35+E II (black and white) | [ASCII.g1a](../../dist/g1a/ASCII.g1a) |

Copy the file to the root of the calculator's storage over USB (USB Flash mode, F1), then eject it; the app appears in the MENU.

Three views, switched with F1–F3:

- **LIST** — every code 0–127 with its decimal, hex, octal and binary value,
  the character (or the mnemonic for control codes, in red) and a short name.
- **GRID** — the classic 16×8 ASCII grid; the selected cell is detailed below.
- **UTF-8** — type a Unicode code point and see its UTF-8, UTF-16 and UTF-32
  encodings, HTML entity, the Unicode block, and a bit-by-bit diagram of how
  the code point's bits are packed into the UTF-8 bytes (markers in grey).

| Key | Action |
|---|---|
| ▲ ▼ ◀ ▶ | Move the selection |
| 0–9 | List/grid: jump to a decimal code. UTF-8: type a hex code point |
| X,θ,T / log / ln / sin / cos / tan | UTF-8: hex digits A–F |
| EXE | Open the selected character in the UTF-8 view |
| ▲ / ▼ (UTF-8) | Code point +1 / −1 |
| DEL / AC | UTF-8: delete a hex digit / clear |
| OPTN | Info screen |
| EXIT | Quit |

## fx-9860G (128×64)

`ASCII.g1a` is the black-and-white version, with the same three views and
keys, laid out for the 21×5-character screen (greys instead of colours):

- **F-keys** are abbreviated: F1 **LST**, F2 **GRD**, F3 **UTF**.
- **LIST** shows 4 rows with decimal, hex, binary and the character; the line
  below gives the selected code's octal (`0o101`) and its name / C escape,
  shortened to what fits. ◀ ▶ page by 4 rows.
- **GRID** shows 4 of the 8 rows at a time and scrolls with the selection;
  control codes are grey cells. The line below gives the character, decimal,
  hex, octal and C escape (no long name).
- **UTF-8** has two pages, flipped with **F6** (BIT / ENC) or ◀ ▶:
  the encodings (UTF-8, UTF-16, UTF-32, HTML entities, decimal on the top
  line), and the bits page with the Unicode block (some names abbreviated)
  and the UTF-8 bytes in binary, two per line, markers in light grey.
- The UTF-16 "surrogate pair" note is left out; the About screen has no QR
  code.

Screenshots: `screenshots/fx-*.png` (from `screenshots/fx-demo.script`).

© José Rodrigues 2026
