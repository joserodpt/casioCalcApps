DC circuit simulator for the Casio FX CG 50 (works with the CG10 and 20 too). Made with fxSDK and gint.

Build a circuit on a 12 × 5 grid from wires, batteries, resistors, lamps and
switches, and read the voltage (V), current (A, mA, µA) and power (W, mW)
of every part. The circuit is solved again after every change. Moving dots
show the direction and size of the current, and lamps light up by how much
of their rated power they get.

Parts sit on the links between two grid points. Move the cursor over a link
to place or read a part, or over a point to read its voltage (measured from
the battery's − terminal).

| Key | Action |
|---|---|
| Arrows | Move the cursor (links and points) |
| F1 | Wire. On a point: draw wires with the arrows, EXE or EXIT to stop |
| F2 | Resistor (100 Ω) |
| F3 | Battery (9 V); again on a battery to swap + and − |
| F4 | Lamp (6 V, 3 W) |
| F5 | Switch; again to open or close it |
| F6 or DEL | Delete the part |
| EXE | Edit the part's values (or open/close a switch) |
| VARS | Load an example: series, parallel, voltage divider |
| AC twice | Clear the whole circuit |

In the edit screen, F1–F6 add an SI prefix (p n µ m k M), so `4.7` then `k`
is 4.7 kΩ. A lamp is a resistor of V²/P ohms: at its rated voltage it takes
its rated power and shines fully, and it shows OVERLOAD above 120%.

The solver uses nodal analysis. Points joined by wires or closed switches
become one node, each battery adds an equation for its current, and the
current in each wire is worked out from how it splits between the parts.
A battery whose terminals are wired together is a short circuit, and
batteries in parallel at different voltages have no solution; both are
shown instead of the readings.

The circuit is saved to `\\fls0\Circuit.cir` on exit and reopened next time.

OPTN opens the info screen and EXIT quits.

© José Rodrigues 2026
