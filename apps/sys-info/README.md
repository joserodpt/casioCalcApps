System information for the Casio FX CG 50 (works with the CG10 and 20 too). Made with fxSDK and gint.

## Download

| Calculator | File |
|---|---|
| fx-CG50, fx-CG10, fx-CG20 (colour) | [SysInfo.g3a](../../dist/g3a/SysInfo.g3a) |

Copy the file to the root of the calculator's storage over USB (USB Flash mode, F1), then eject it; the app appears in the MENU. This app needs the colour screen, so there is no black and white version.

Shows everything the calculator tells an add-in about itself on one
scrolling screen. **It only reads: nothing on the calculator is changed.**

| Section | What is shown | Where it comes from |
|---|---|---|
| Calculator | Model, emulator check, product ID (hex, and as text if it is printable) | gint's hardware table; 8 bytes of ROM at `0x8001FFD0` |
| Operating system | OS version, OS header, OS language | ROM at `0x80020020` / `0x80020000`; `GlibGetOSVersionInfo`, `LocalizeMessage1` |
| Processor | MPU, CPU version / product registers, CPU / bus / peripheral clocks, speed level (F1–F5) | gint (`HWMPU`, `HWCPUVR`, `HWCPUPR`, CPG) |
| Memory | RAM, add-in RAM, flash ROM size, free storage memory, filesystem, stack pointer | gint (`HWRAM`, `HWURAM`, `HWROM`, `HWFS`); `Bfile_GetMediaFree_OS`, `GetStackPtr` |
| Power | Battery voltage and type, backlight duration, auto power off | `GetMainBatteryVoltage`, `GetBatteryType`, `GetBacklightDuration`, `GetAutoPowerOffTime` |
| Display & keyboard | Screen size and controller, keyboard scan method | gint |
| Clock | Date and time | the RTC |
| This add-in | Version and build date | |

A size gint couldn't detect shows as `?`. In the emuCasioCalc emulator the
OS area is blank, so the OS version and header come only from the syscall
and the product ID is the emulator's (random, or `--device-id`).

| Key | Action |
|---|---|
| ▲ ▼ | Scroll a line |
| ◀ ▶ | Scroll a page |
| F1 | Read everything again (battery, clock and storage change) |
| OPTN or F6 | Info screen |
| EXIT | Quit |

© José Rodrigues 2026
