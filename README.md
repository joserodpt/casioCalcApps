<div align="center">

# casioCalcApps

**Useful add-ins for the Casio fx-CG50 / fx-CG10 / fx-CG20 (Prizm) graphing calculators,
with black and white versions of most of them for the fx-9860G family**

DC and AC circuit simulators, toolkits for electronics, chemistry, physics,
logic, networking and informatics, number converters, a timer, a tone generator for the
link port and a firmware dumper, all with colour interfaces and in English,
Portuguese and French (SysInfo is English only).

![Calculator](https://img.shields.io/badge/calculator-fx--CG50%20%7C%20CG10%20%7C%20CG20%20%7C%20fx--9860G-1f6feb)
![Language](https://img.shields.io/badge/language-C-555555)
![Built with](https://img.shields.io/badge/built%20with-fxSDK%20%2B%20gint-8a2be2)
![Apps](https://img.shields.io/badge/apps-12-2ea44f)

<img src="apps/electronics/screenshots/demo.gif" width="32%" alt="Electronics demo">
<img src="apps/base-n/screenshots/demo.gif" width="32%" alt="Base-N demo">
<img src="apps/circuit/screenshots/demo.gif" width="32%" alt="Circuit demo">

</div>

## Apps

| | App | What it does | fx-CG (colour) | fx-9860G (B&W) |
|:---:|---|---|:---:|:---:|
| <img src="apps/electronics/assets-cg/icon-sel.png" width="46"> | [**Electronics**](apps/electronics) | Resistor colour and SMD codes, series/parallel, voltage divider, RC/RL/LC filters, 555 timer, LED resistor | [Electronics.g3a](dist/g3a/Electronics.g3a) | [Electronics.g1a](dist/g1a/Electronics.g1a) |
| <img src="apps/circuit/assets-cg/icon-sel.png" width="46"> | [**Circuit**](apps/circuit) | DC circuit simulator: wires, batteries, resistors, lamps, switches, diodes, LEDs, meters, potentiometers and fuses, with V, A/mA and W for each part | [Circuit.g3a](dist/g3a/Circuit.g3a) | – |
| <img src="apps/ac-circuit/assets-cg/icon-sel.png" width="46"> | [**AC Circuit**](apps/ac-circuit) | AC circuit simulator: R, C, L, sources, lamps, switches and meters, with V and I phasors, power factor, waveforms and frequency response | [ACCircuit.g3a](dist/g3a/ACCircuit.g3a) | – |
| <img src="apps/chemistry/assets-cg/icon-sel.png" width="46"> | [**Chemistry**](apps/chemistry) | Periodic table, molar mass from a formula, moles, concentration, ideal gas law, unit converter | [Chemistry.g3a](dist/g3a/Chemistry.g3a) | [Chemistry.g1a](dist/g1a/Chemistry.g1a) |
| <img src="apps/physics/assets-cg/icon-sel.png" width="46"> | [**Physics**](apps/physics) | Kinematics (SUVAT), dynamics, energy and power, waves and optics, physical constants, unit converter | [Physics.g3a](dist/g3a/Physics.g3a) | [Physics.g1a](dist/g1a/Physics.g1a) |
| <img src="apps/logic/assets-cg/icon-sel.png" width="46"> | [**Logic**](apps/logic) | Boolean expression → truth table, coloured Karnaugh map and simplified sum of products | [Logic.g3a](dist/g3a/Logic.g3a) | – |
| <img src="apps/informatics/assets-cg/icon-sel.png" width="46"> | [**Informatics**](apps/informatics) | Data size and rate conversions, transfer time, raw media sizes, IEEE 754 floats bit by bit, Unix time, CPU performance and Amdahl's law, integer and float limits | [Informatics.g3a](dist/g3a/Informatics.g3a) | – |
| <img src="apps/base-n/assets-cg/icon-sel.png" width="46"> | [**Base-N**](apps/base-n) | Programmer's converter: DEC / HEX / OCT / BIN live, 8–64-bit, signed or unsigned, bit grid | [Base-N.g3a](dist/g3a/Base-N.g3a) | [Base-N.g1a](dist/g1a/Base-N.g1a) |
| <img src="apps/net-tools/assets-cg/icon-sel.png" width="46"> | [**Net Tools**](apps/net-tools) | IPv4 subnet calculator: netmask, network, broadcast, host range, address class, bit grid | [NetTools.g3a](dist/g3a/NetTools.g3a) | [NetTools.g1a](dist/g1a/NetTools.g1a) |
| <img src="apps/ascii-table/assets-cg/icon-sel.png" width="46"> | [**ASCII**](apps/ascii-table) | ASCII table as a list or a 16×8 grid, and a UTF-8 / UTF-16 encoder for any code point | [ASCII.g3a](dist/g3a/ASCII.g3a) | [ASCII.g1a](dist/g1a/ASCII.g1a) |
| <img src="apps/timer/assets-cg/icon-sel.png" width="46"> | [**Timer**](apps/timer) | Stopwatch with laps and a Pomodoro timer, driven by the real-time clock | [Timer.g3a](dist/g3a/Timer.g3a) | [Timer.g1a](dist/g1a/Timer.g1a) |
| <img src="apps/tone-generator/assets-cg/icon-sel.png" width="46"> | [**Tone**](apps/tone-generator) | Tone generator on the 3-pin link port: square, sine, triangle and sawtooth from 20 Hz to 20 kHz, a piano, frequency sweeps and 17 melodies | [Tone.g3a](dist/g3a/Tone.g3a) | [Tone.g1a](dist/g1a/Tone.g1a) |
| <img src="apps/firmware-dump/assets-cg/icon-sel.png" width="46"> | [**FirmDump**](apps/firmware-dump) | Dumps the OS, the full ROM or any memory region to storage or over USB, read-only | [FirmDump.g3a](dist/g3a/FirmDump.g3a) | – |
| <img src="apps/sys-info/assets-cg/icon-sel.png" width="46"> | [**SysInfo**](apps/sys-info) | Everything the calculator reports: model, product ID, OS version and language, CPU and clocks, memory, battery and power settings, clock, read-only | [SysInfo.g3a](dist/g3a/SysInfo.g3a) | – |
| <img src="apps/activation/assets-cg/icon-sel.png" width="46"> | [**Activation**](apps/activation) | Product-activation demo: a request code from the calculator's product ID, unlocked by an activation code from a PC key generator | [Activation.g3a](dist/g3a/Activation.g3a) | – |

Every app has its own README with the full key reference. Press **OPTN** in any
app for its About screen.

### Black and white versions

Electronics, Chemistry, Physics, Base-N, Net Tools, ASCII, Timer and Tone also come
as `.g1a` add-ins for the fx-9860G II, fx-9750G II and Graph 35+E II (128×64
screen, four shades of grey). They have their own layouts with the same
tools; what had to be shortened is listed in each app's README. They run in
English. Circuit, AC Circuit, Logic, Informatics, FirmDump, SysInfo and Activation
need the colour screen.

| | | |
|:---:|:---:|:---:|
| <img src="apps/electronics/screenshots/fx-colorcode.png" width="256"><br>Electronics (colour code) | <img src="apps/electronics/screenshots/fx-divider.png" width="256"><br>Electronics (divider) | <img src="apps/chemistry/screenshots/fx-periodic-fe.png" width="256"><br>Chemistry (periodic table) |
| <img src="apps/chemistry/screenshots/fx-molar.png" width="256"><br>Chemistry (molar mass) | <img src="apps/physics/screenshots/fx-kinematics.png" width="256"><br>Physics (kinematics) | <img src="apps/physics/screenshots/fx-constants.png" width="256"><br>Physics (constants) |
| <img src="apps/base-n/screenshots/fx-decimal.png" width="256"><br>Base-N | <img src="apps/net-tools/screenshots/fx-main.png" width="256"><br>Net Tools | <img src="apps/ascii-table/screenshots/fx-utf8-emoji.png" width="256"><br>ASCII (UTF-8) |
| <img src="apps/ascii-table/screenshots/fx-list.png" width="256"><br>ASCII table | <img src="apps/timer/screenshots/fx-laps.png" width="256"><br>Timer (stopwatch) | <img src="apps/timer/screenshots/fx-pomodoro-running.png" width="256"><br>Timer (Pomodoro) |

## Screenshots

Click an app to see it in action.

<details>
<summary><b>Electronics</b>: resistor codes, divider, 555 timer, LED resistor</summary>
<br>

<img src="apps/electronics/screenshots/demo.gif" width="600" alt="Electronics demo">

| | |
|:---:|:---:|
| <img src="apps/electronics/screenshots/menu.png" width="396"><br>Tool menu | <img src="apps/electronics/screenshots/colorcode.png" width="396"><br>4.7 kΩ in colour bands |
| <img src="apps/electronics/screenshots/colorcode-68k.png" width="396"><br>From a value to its bands | <img src="apps/electronics/screenshots/divider.png" width="396"><br>Voltage divider |
| <img src="apps/electronics/screenshots/timer555.png" width="396"><br>555 timer, astable | <img src="apps/electronics/screenshots/led.png" width="396"><br>LED series resistor |

</details>

<details>
<summary><b>Circuit</b>: DC circuit simulator with live V, A and W</summary>
<br>

<img src="apps/circuit/screenshots/demo.gif" width="600" alt="Circuit demo">

| | |
|:---:|:---:|
| <img src="apps/circuit/screenshots/series.png" width="396"><br>Series circuit with current flow | <img src="apps/circuit/screenshots/parts-menu.png" width="396"><br>Parts menu |
| <img src="apps/circuit/screenshots/led.png" width="396"><br>LED with its resistor and an ammeter | <img src="apps/circuit/screenshots/dimmer.png" width="396"><br>Dimmer: potentiometer, fuse, voltmeter |
| <img src="apps/circuit/screenshots/fuse-blown.png" width="396"><br>Short across the lamp: the fuse blows | <img src="apps/circuit/screenshots/lamp-overload.png" width="396"><br>Smaller resistor: lamp overloaded |
| <img src="apps/circuit/screenshots/divider.png" width="396"><br>Voltage divider, point voltage | <img src="apps/circuit/screenshots/drawn.png" width="396"><br>A circuit drawn with the wire tool |

</details>

<details>
<summary><b>AC Circuit</b>: phasors, power factor, frequency response</summary>
<br>

<img src="apps/ac-circuit/screenshots/demo.gif" width="600" alt="AC Circuit demo">

| | |
|:---:|:---:|
| <img src="apps/ac-circuit/screenshots/lowpass.png" width="396"><br>RC low-pass, the source's readings | <img src="apps/ac-circuit/screenshots/phasors.png" width="396"><br>Phasors of the capacitor's voltage |
| <img src="apps/ac-circuit/screenshots/waveforms.png" width="396"><br>Waveforms over two periods | <img src="apps/ac-circuit/screenshots/bode.png" width="396"><br>Frequency response of the low-pass |
| <img src="apps/ac-circuit/screenshots/rlc-resonance.png" width="396"><br>Series RLC at resonance: the lamp at full power | <img src="apps/ac-circuit/screenshots/pf-corrected.png" width="396"><br>RL load with its power factor corrected |
| <img src="apps/ac-circuit/screenshots/tank-bode.png" width="396"><br>LC tank peaking at 5.03 kHz | <img src="apps/ac-circuit/screenshots/edit-capacitor.png" width="396"><br>Editing a capacitor, with its reactance |

</details>

<details>
<summary><b>Chemistry</b>: periodic table, molar mass, ideal gas law</summary>
<br>

<img src="apps/chemistry/screenshots/demo.gif" width="600" alt="Chemistry demo">

| | |
|:---:|:---:|
| <img src="apps/chemistry/screenshots/main.png" width="396"><br>Tool menu | <img src="apps/chemistry/screenshots/molar.png" width="396"><br>Molar mass of CO₂ |
| <img src="apps/chemistry/screenshots/periodic.png" width="396"><br>Periodic table | <img src="apps/chemistry/screenshots/gas.png" width="396"><br>Ideal gas law |
| <img src="apps/chemistry/screenshots/units.png" width="396"><br>Unit converter | |

</details>

<details>
<summary><b>Physics</b>: kinematics, energy, waves, constants</summary>
<br>

<img src="apps/physics/screenshots/demo.gif" width="600" alt="Physics demo">

| | |
|:---:|:---:|
| <img src="apps/physics/screenshots/main.png" width="396"><br>Tool menu | <img src="apps/physics/screenshots/kinematics.png" width="396"><br>Kinematics (SUVAT) |
| <img src="apps/physics/screenshots/energy.png" width="396"><br>Energy and power | <img src="apps/physics/screenshots/waves.png" width="396"><br>Waves and Snell's law |
| <img src="apps/physics/screenshots/constants.png" width="396"><br>Physical constants | <img src="apps/physics/screenshots/units.png" width="396"><br>Unit converter |

</details>

<details>
<summary><b>Logic</b>: truth tables and Karnaugh maps</summary>
<br>

<img src="apps/logic/screenshots/demo.gif" width="600" alt="Logic demo">

| | |
|:---:|:---:|
| <img src="apps/logic/screenshots/expression.png" width="396"><br>Expression and its minimal form | <img src="apps/logic/screenshots/truth-table.png" width="396"><br>Truth table |
| <img src="apps/logic/screenshots/kmap.png" width="396"><br>Karnaugh map with groups | <img src="apps/logic/screenshots/kmap-edited.png" width="396"><br>After editing a minterm |

</details>

<details>
<summary><b>Base-N</b>: DEC / HEX / OCT / BIN converter</summary>
<br>

<img src="apps/base-n/screenshots/demo.gif" width="600" alt="Base-N demo">

| | |
|:---:|:---:|
| <img src="apps/base-n/screenshots/decimal.png" width="396"><br>Decimal input, 64-bit | <img src="apps/base-n/screenshots/hex-16bit.png" width="396"><br>Hex input, 16-bit |
| <img src="apps/base-n/screenshots/signed.png" width="396"><br>Signed (two's complement) | <img src="apps/base-n/screenshots/negative.png" width="396"><br>Negative numbers |

</details>

<details>
<summary><b>Net Tools</b>: IPv4 subnet calculator</summary>
<br>

<img src="apps/net-tools/screenshots/demo.gif" width="600" alt="Net Tools demo">

| | |
|:---:|:---:|
| <img src="apps/net-tools/screenshots/main.png" width="396"><br>192.168.1.10/24 | <img src="apps/net-tools/screenshots/custom.png" width="396"><br>10.20.30.40/22 |
| <img src="apps/net-tools/screenshots/next.png" width="396"><br>Stepping to the next subnet | <img src="apps/net-tools/screenshots/slash31.png" width="396"><br>/31 point-to-point (RFC 3021) |

</details>

<details>
<summary><b>ASCII</b>: ASCII table and UTF-8 encoder</summary>
<br>

<img src="apps/ascii-table/screenshots/demo.gif" width="600" alt="ASCII demo">

| | |
|:---:|:---:|
| <img src="apps/ascii-table/screenshots/list.png" width="396"><br>List view | <img src="apps/ascii-table/screenshots/grid.png" width="396"><br>16×8 grid |
| <img src="apps/ascii-table/screenshots/utf8-euro.png" width="396"><br>UTF-8 of € (3 bytes) | <img src="apps/ascii-table/screenshots/utf8-emoji.png" width="396"><br>UTF-8 of 😀 (4 bytes) |

</details>

<details>
<summary><b>Timer</b>: stopwatch and Pomodoro</summary>
<br>

<img src="apps/timer/screenshots/demo.gif" width="600" alt="Timer demo">

| | |
|:---:|:---:|
| <img src="apps/timer/screenshots/laps.png" width="396"><br>Stopwatch with laps | <img src="apps/timer/screenshots/pomodoro.png" width="396"><br>Pomodoro |
| <img src="apps/timer/screenshots/pomodoro-running.png" width="396"><br>Focus running | <img src="apps/timer/screenshots/break.png" width="396"><br>Short break |

</details>

<details>
<summary><b>Tone</b>: tone generator, piano, sweeps and melodies on the link port</summary>
<br>

<img src="apps/tone-generator/screenshots/demo.gif" width="600" alt="Tone demo">

| | |
|:---:|:---:|
| <img src="apps/tone-generator/screenshots/square.png" width="396"><br>440 Hz square wave | <img src="apps/tone-generator/screenshots/sine.png" width="396"><br>Sine, a semitone at a time |
| <img src="apps/tone-generator/screenshots/piano.png" width="396"><br>Piano on the number and F keys | <img src="apps/tone-generator/screenshots/sweep.png" width="396"><br>Logarithmic sweep |
| <img src="apps/tone-generator/screenshots/melodies.png" width="396"><br>Seventeen melodies | <img src="apps/tone-generator/screenshots/melody.png" width="396"><br>Playing the Portuguese anthem |
| <img src="apps/tone-generator/screenshots/output.png" width="396"><br>Output method and wiring | <img src="apps/tone-generator/screenshots/fx-sine.png" width="256"><br>fx-9860G version |

</details>

<details>
<summary><b>FirmDump</b>: firmware and memory dumper</summary>
<br>

<img src="apps/firmware-dump/screenshots/demo.gif" width="600" alt="FirmDump demo">

| | |
|:---:|:---:|
| <img src="apps/firmware-dump/screenshots/main.png" width="396"><br>Regions and live preview | <img src="apps/firmware-dump/screenshots/custom.png" width="396"><br>Custom region |
| <img src="apps/firmware-dump/screenshots/dump.png" width="396"><br>Dump finished | <img src="apps/firmware-dump/screenshots/about.png" width="396"><br>About screen |

</details>

> The screenshots come from the [emuCasioCalc](https://github.com/joserodpt/emuCasioCalc)
> emulator, which has no Casio OS, so FirmDump only shows blank memory there
> and Tone makes no sound (it has no serial port to play on).

## Installing on the calculator

1. Download the files you want from the table above: `.g3a` for an fx-CG
   (from [`dist/g3a/`](dist/g3a)), `.g1a` for an fx-9860G (from [`dist/g1a/`](dist/g1a)).
2. Connect the calculator to your computer with its USB cable and choose
   **USB Flash** (F1) on the calculator.
3. Copy the files to the root of the calculator's drive, then eject it. (On
   older fx-9860G models without USB Flash, send `.g1a` files with FA-124.)
4. The apps appear in the calculator's **MENU**.

## Building from source

You need the [fxSDK](https://gitea.planet-casio.com/Lephenixnoir/fxsdk) with
[gint](https://gitea.planet-casio.com/Lephenixnoir/gint) installed (for example
with [GiteaPC](https://gitea.planet-casio.com/Lephenixnoir/GiteaPC)).

```sh
make                            # build all apps -> dist/g3a/*.g3a, dist/g1a/*.g1a
make app APP=base-n             # build one app
make send APP=base-n            # build and send to a connected calculator
make new APP=my-app NAME=MyApp  # create apps/my-app from the template
make clean
```

In VS Code, `Ctrl+Shift+B` builds everything; the other tasks are under *Run Task*.

### Running on a computer

The apps are tested on [emuCasioCalc](https://github.com/joserodpt/emuCasioCalc),
an fx-CG50 add-in emulator that runs `.g3a` files on a PC.

```sh
./run.sh                 # pick cg or fx, then an app; build it and run it
./run.sh cg base-n       # build apps/base-n and run the colour .g3a
./run.sh fx base-n       # ... or its black and white .g1a
scripts/screenshots.sh   # regenerate every app's demo GIF and screenshots
```

Set `EMUCASIO` if your emuCasioCalc checkout is not in `~/VSCode/emuCasioCalc`.
Each app's screenshots come from the key steps in `apps/<app>/screenshots/demo.script`
(keys, `wait 1s`, `shot name.png`), run headless with a fixed clock, so the
results are the same every time.

## Project layout

```
casioCalcApps/
├── apps/                one folder per add-in
│   └── base-n/
│       ├── src/         app source code
│       ├── assets-cg/   icons and images for fx-CG
│       ├── screenshots/ demo.script, demo.gif and screenshots
│       └── CMakeLists.txt
├── common/              code and assets shared by the apps
├── template/            skeleton used by `make new` (also apps/hello)
├── scripts/             build.sh, screenshots.sh, new-app.sh...
└── dist/                compiled add-ins: g3a/ (fx-CG), g1a/ (fx-9860G)
```

<details>
<summary><b>Developer notes</b>: shared modules, info screen, icons</summary>

### Black and white (.g1a) versions

An app gets a `.g1a` when its `CMakeLists.txt` says `casio_app(<name> FX ...)`;
`scripts/build.sh` then builds both. Its 128×64 layout goes in
`#if GINT_HW_FX` blocks next to the colour one (see `apps/hello` and the
template), and it calls `screen_init()` at start-up for the greys. The shared
title bar, F-keys and About screen already have both layouts. Icons are drawn
by `common/tools/make_icons_fx.py`, and `screenshots/fx-demo.script` makes
the `fx-*.png` screenshots.

### Adding code to an app

List new `.c` files under `SOURCES` in the app's `CMakeLists.txt`, and images
under `ASSETS_cg` (with an entry in `assets-cg/fxconv-metadata.txt`).
Shared code goes in `common/src` + `common/include` (and `COMMON_SOURCES` in
`common/common.cmake`). It is built as a static library, so an app only links
the modules it uses.

### Shared modules

| Header | What it gives |
| --- | --- |
| `info.h` | title bar with the info hint, About screen (`info_show`, `info_app`) |
| `color.h` | colour names on both families (greys on the fx-9860G), `screen_init()` for the grey engine |
| `i18n.h` | `lang_init()` and `L("English", "Portugues", "Francais")` |
| `menu.h` | numbered tool list that toolkit apps open on (`menu_run`) |
| `form.h` | screen of number fields with live results (`form_run`) |
| `numfield.h` | one number input box (digits, `.`, EXP, sign) |
| `units.h` | unit tables, `unit_convert()` and the converter screen |
| `ui.h` | text with Ω µ ± symbols, `fmt_si()` (4.7 kΩ, 250 mA), number fields with SI-prefix F-keys, forms |
| `fmt.h` | `fmt_g()` / `fmt_fixed()`: doubles without printf's `%f` |
| `usb_screen.h` | `usb_screen_init()`: sends each frame over USB, for emuCasioCalc's File > Show calculator's screen or `fxlink -i`; never makes the app wait. Every app but firmware-dump calls it after `lang_init()` |
| `keys.h` | `key_digit()` / `key_hex()` (A-F on X,theta,T log ln sin cos tan) |
| `utils.h` | F-key buttons, scrollbar |

### Info screen

Every app shows `OPTN for info` in its title bar and opens a shared About screen
(name, version, author, build date, QR code to this repository):

```c
#include "info.h"

info_title("My App", C_BLUE);                      // title bar with the hint
if(key == INFO_KEY) info_show("My App", "v1.0", C_BLUE);
```

`make new` apps already do this. The big font (`font_big`) is available too.
The assets are generated by `common/tools/make_font.py` and
`common/tools/make_qr.py` (needs `pip install qrcode`).

### Icons

`common/tools/make_icons.py` draws each app's `icon-uns.png` / `icon-sel.png`
(a rounded panel in the app's colour with a white pictogram). Add an entry to
`ICONS` for a new app, then run it with the app's folder name.

</details>

## License

The code is released under the [MIT License](LICENSE).

The big font (`common/assets-cg/font_big.png` and `common/assets-fx/font_big.png`)
is rendered from **DejaVu Sans Mono Bold**, part of the
[DejaVu fonts](https://dejavu-fonts.github.io/). DejaVu fonts are based on
Bitstream Vera: Copyright © 2003 Bitstream, Inc. All Rights Reserved; Bitstream
Vera is a trademark of Bitstream, Inc. DejaVu changes are in the public domain.
See the [DejaVu fonts license](https://dejavu-fonts.github.io/License.html).

---

<div align="center">
Made by <a href="https://github.com/joserodpt">joserodpt</a> with
<a href="https://gitea.planet-casio.com/Lephenixnoir/gint">gint</a> and the
<a href="https://gitea.planet-casio.com/Lephenixnoir/fxsdk">fxSDK</a>.
</div>
