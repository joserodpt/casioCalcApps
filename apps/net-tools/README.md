Network and internet tools for the Casio FX CG 50 (works with the CG10 and 20 too), with a black and white version for the fx-9860G II. Made with fxSDK and gint.

## Download

| Calculator | File |
|---|---|
| fx-CG50, fx-CG10, fx-CG20 (colour) | [NetTools.g3a](../../dist/g3a/NetTools.g3a) |
| fx-9860G II, fx-9750G II, Graph 35+E II (black and white) | [NetTools.g1a](../../dist/g1a/NetTools.g1a) |

Copy the file to the root of the calculator's storage over USB (USB Flash mode, F1), then eject it; the app appears in the MENU.

## IP subnet calculator

Type an IPv4 address and a /CIDR prefix to get the netmask (dotted and hex),
wildcard, network, broadcast, host range and number of usable hosts, plus the
address class and type (private, loopback, link-local, CGNAT, multicast...).
A 32-bit grid shows the address and mask bits, with network bits filled and
a red line at the prefix boundary. /31 (RFC 3021) and /32 are handled.

| Key | Action |
|---|---|
| 0–9 | Type into the selected field; it moves on when full |
| `.`, EXE, ▶ / ◀ | Next / previous field |
| ÷ | Jump to the prefix |
| ▲ / ▼ | Prefix +1 / −1 |
| DEL | Delete a digit (back to the previous field when empty) |
| AC/ON | Clear the address (the prefix is kept) |
| F1 / F2 | Previous / next subnet of the same size |
| F3–F6 | Prefix /8, /16, /24, /30 |
| OPTN | Info screen |
| EXIT | Quit |

## fx-9860G (128x64)

`NetTools.g1a` is the same calculator for the black-and-white fx-9860G. The
address is typed the same way, on one line under the title (the selected
field is shaded grey, then inverted once you type into it). The results don't
all fit at once, so they are split into four pages; F6 (`PG1`–`PG4`) cycles
through them:

| Page | Rows |
|---|---|
| 1 | Network, netmask, broadcast, usable hosts |
| 2 | First and last host (`Host` for /32), total addresses, wildcard |
| 3 | Netmask in hex, address class, address type |
| 4 | The address and mask in binary, two octets per line, network bits shaded |

What differs from the colour version:

- F1 / F2 are `PRV` / `NXT`, F3–F5 are /16, /24, /30 and F6 changes page.
  There is no /8 key: press ÷ then 8 (or use ▲ / ▼).
- Labels are shortened (`Net`, `Mask`, `Bcast`, `Wild`...), the network is
  shown without its /prefix (it is on the input line), and the host range is
  split into a First and a Last row.
- The binary grid has no red boundary line or octet values under it; the
  shading shows where the network bits end.
- The menu is in English only (no OS language detection on the fx-9860G).

© José Rodrigues 2026
