DC circuit simulator for the Casio FX CG 50 (works with the CG10 and 20 too). Made with fxSDK and gint.

## Download

| Calculator | File |
|---|---|
| fx-CG50, fx-CG10, fx-CG20 (colour) | [Circuit.g3a](../../dist/g3a/Circuit.g3a) |

Copy the file to the root of the calculator's storage over USB (USB Flash mode, F1), then eject it; the app appears in the MENU. This app needs the colour screen, so there is no black and white version.

Build a circuit on a 12 × 5 grid and read the voltage (V), current (A, mA,
µA) and power (W, mW) of every part. The circuit is solved again after every
change. Moving dots show the direction and size of the current, and lamps
and LEDs light up by how much current they get.

| Part | Behaviour | Default |
|---|---|---|
| Wire | No resistance | |
| Resistor | Ohm's law | 100 Ω |
| Battery | Ideal voltage source; long plate is + | 9 V |
| Lamp | Resistor of V²/P ohms; glows by power, OVERLOAD above 120% | 6 V, 3 W |
| Switch | Open or closed | closed |
| Diode | Conducts from the triangle to the bar above its forward voltage | 0.7 V |
| LED | A diode that lights: red, yellow, green, blue or white (about 2.0–3.0 V at 20 mA); OVERLOAD above 30 mA | red |
| Ammeter | No resistance; shows the current next to it | |
| Voltmeter | Infinite resistance; shows the voltage next to it | |
| Potentiometer | Variable resistor, turned with + and − in 5% steps | 1 kΩ at 50% |
| Fuse | Blows (opens) above its rating; EXE puts in a new one | 1 A |

Parts sit on the links between two grid points. Move the cursor over a link
to place or read a part, or over a point to read its voltage (measured from
the battery's − terminal).

| Key | Action |
|---|---|
| Arrows | Move the cursor (links and points) |
| F1 | Wire. On a point: draw wires with the arrows, EXE or EXIT to stop |
| F2 | Resistor (100 Ω) |
| F3 | Battery (9 V); again on a battery to swap + and − (diodes and LEDs: pick them again in F5) |
| F4 | Lamp (6 V, 3 W) |
| F5 | Parts menu: switch, diode, LED, ammeter, voltmeter, potentiometer, fuse |
| F6 or DEL | Delete the part |
| EXE | Edit the part's values (or open/close a switch) |
| + / − | Turn a potentiometer |
| VARS | Load an example: series, parallel, voltage divider, LED with an ammeter, dimmer with a fuse and a voltmeter |
| AC twice | Clear the whole circuit |

In the edit screen, F1–F6 add an SI prefix (p n µ m k M), so `4.7` then `k`
is 4.7 kΩ. A lamp is a resistor of V²/P ohms: at its rated voltage it takes
its rated power and shines fully, and it shows OVERLOAD above 120%.

The solver uses nodal analysis. Points joined by wires, ammeters or closed
switches become one node, each battery adds an equation for its current, and
the current in each wire is worked out from how it splits between the parts.
Diodes and LEDs are a forward voltage plus a small resistance when on, and
almost no conductance when off: they start off, and the circuit is solved
again with each one switched until the result agrees with all of them. After
solving, the fuse that is most over its rating blows and the circuit is
solved again, so in series only the weaker fuse goes.
A battery whose terminals are wired together is a short circuit, and
batteries in parallel at different voltages have no solution; both are
shown instead of the readings.

The circuit is saved to `\\fls0\Circuit.cir` on exit and reopened next time.

OPTN opens the info screen and EXIT quits.

© José Rodrigues 2026
