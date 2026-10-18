Firmware / memory dumper for the Casio FX CG 50 (works with the CG10 and 20 too). Made with fxSDK and gint.

Reads a region of the calculator's memory and saves it to storage as
`\\fls0\DUMP\<name>NN.bin`, split into 1 MB parts, for analysis on a PC. The
flash ROM is memory-mapped at `0x80000000` (P1, cached) on the SH7305; the
Casio OS lives in the first few MB. **It only reads memory and writes new
files — nothing on the calculator is modified.**

Pick a region with ▲/▼ and press F1 to dump it:

| Region | Range |
|---|---|
| OS firmware | `0x80000000` + 4 MB (the OS area) |
| Full ROM | `0x80000000` + the ROM size reported by the hardware |
| Custom | any base and size you enter in hex |

A live hex preview of the selected base is shown (unmapped addresses read as
`--`). Reading an unmapped custom range triggers gint's safe error screen
rather than dumping garbage.

| Key | Action |
|---|---|
| ▲ ▼ | Select a region |
| F1 | Dump the selected region (EXIT aborts while it runs) |
| F4 / F5 (Custom) | Edit base / size; type hex digits (A–F on the letter row) |
| DEL / AC | Edit a digit / reset the custom region |
| OPTN or F6 | Info screen |
| EXIT | Quit |

Custom parts are named by their start address (`DUMP\80400000.bin`,
`DUMP\80500000.bin`, ...), so a large area can be dumped one window at a
time: dump 4 MB, copy the files off, delete them, move the base on. The full
ROM is larger than the free storage memory, and the OS may go past 4 MB:
use the preview to see where the data turns into `FF FF ...`.

Copy the files off with the USB cable or by reading the storage memory. To
reassemble on a PC: `cat OS00.bin OS01.bin ... > firmware.bin` (custom parts
sort by address: `cat 8*.bin > firmware.bin`).

© José Rodrigues 2026
