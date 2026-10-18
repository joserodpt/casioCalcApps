This is an electronics toolkit for the Casio FX CG 50 (works with the CG10 and 20 too). Made with fxSDK and gint.

Pick a tool from the menu with ▲/▼ and EXE, or its number key:

| # | Tool | What it does |
|---|---|---|
| 1 | Colour code | Draws the resistor in real colours from 3 to 6 bands: value, tolerance, range, tempco, nearest E12/E24/E96. Also works in reverse, from a value to its bands |
| 2 | SMD codes | Decodes 3-digit (`472`), 4-digit (`4702`), `R` (`4R7`) and EIA-96 (`01C`) codes, and gives every code for a value |
| 3 | Series / parallel | Up to 8 resistors; series and parallel totals with the nearest E24 value |
| 4 | Voltage divider | Vout, current, power in each resistor; R2 for a target Vout, with E12/E24 values |
| 5 | RC / RL filters | RC and RL cutoff and time constant, LC resonance, series RLC Q |
| 6 | 555 timer | Astable frequency, period, high/low times, duty cycle; monostable pulse width |
| 7 | LED resistor | Series resistor for one or more LEDs, its power and rating, E12/E24 values rounded up |

## Colour code

| Key | Action |
|---|---|
| ◀ / ▶ | Select a band |
| ▲ / ▼ | Change its colour |
| 0–9 | Set a digit or multiplier band directly |
| F1 | Number of bands: 3 / 4 / 5 / 6 |
| F2 or EXE | Type a value and get its bands |
| F4 / F5 / F6 | Snap to the nearest E12 / E24 / E96 value |

## Forms (tools 2–7)

Results update as you type.

| Key | Action |
|---|---|
| ▲ / ▼, EXE | Move between fields (◀ / ▶ change columns in Series / parallel) |
| 0–9, `.`, ×10^x, (−) | Type a number (`1E-6`) |
| F1–F6 | SI prefix: p n µ m k M; `4k7` means 4.7k |
| ◀ / ▶ | Change a choice field (555 mode, LED colour, EIA-96 letter) |
| `.` | In an SMD code, types `R` |
| DEL / AC | Delete a character / clear the field |

OPTN opens the info screen and EXIT goes back (to the menu, then out of the add-in).

© José Rodrigues 2026
