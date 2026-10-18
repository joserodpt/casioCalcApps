This is a BaseN converter for the Casio FX CG 50 (works with the CG10 and 20 too). Made with fxSDK and gint.

Programmer-calculator layout: the number you type is shown large, with live
DEC / HEX / OCT / BIN conversions and a 64-bit grid of its bits below.

| Key | Action |
|---|---|
| F1–F4, ▲/▼ | Input base (DEC, HEX, OCT, BIN); the value is kept |
| 0–9, X,θ,T / log / ln / sin / cos / tan | Digits (A–F are the six keys of the second row) |
| DEL | Delete the last digit |
| AC/ON | Clear |
| (−) | Negate (two's complement) |
| F5 | Word size: 8 / 16 / 32 / 64 bits |
| F6 | Signed: decimal shows the two's complement value |
| OPTN | Info screen (author, QR code to the GitHub repo) |
| EXIT | Quit |

The large font and the info screen's QR code are shared by every app; see
`common/` in the repository README.

© José Rodrigues 2024
