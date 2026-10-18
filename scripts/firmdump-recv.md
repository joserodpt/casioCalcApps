# Dumping the calculator's firmware

How to copy the fx-CG50's OS (or the whole flash ROM) to a PC with the
FirmDump add-in (`apps/firmware-dump`). There are two ways:

- **USB streaming (F2)**: the region goes straight to the PC through
  `firmdump-recv.py`. Nothing is written to the calculator, and the full ROM
  fits. Use this one.
- **Storage dump (F1)**: the region is written to `\\fls0\DUMP` as 1 MB
  files, which you then copy off. Use it if streaming doesn't work. Storage
  memory is about 16 MB, so dump large areas a few MB at a time.

FirmDump only reads memory. The only thing it changes on the calculator is
the `DUMP` folder that F1 creates.

The dump is Casio's code: keep it out of every repository and don't share
it.

## Before the first dump

1. **Build FirmDump** and copy it to the calculator:
   ```sh
   cd ~/VSCode/casioPrizmApps
   make app APP=firmware-dump          # -> dist/FirmDump.g3a
   ```
   Then put the calculator in USB Flash mode (plug in the cable, F1) and
   use the emulator's File > Copy add-in to calculator with
   `dist/FirmDump.g3a` open. A plain copy can fail on Linux with
   "Input/output error": the calculator only accepts 4 kB transfers.
2. **Allow USB access (Linux, once).** This is needed for streaming. Run it
   as root, then unplug and plug the calculator in again:
   ```sh
   echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="07cf", ATTRS{idProduct}=="6102", MODE="0660", GROUP="plugdev"' \
     | sudo tee /etc/udev/rules.d/50-casio-fxlink.rules
   sudo udevadm control --reload
   ```
   Your user must be in the `plugdev` group (`id` lists it). On macOS,
   install libusb instead: `brew install libusb`.
3. **Note the OS version**: System > Version on the calculator. Keep it
   with the dump, because addresses and resources change between versions.

## Method 1: USB streaming

1. On the PC, start the receiver and leave it running:
   ```sh
   ~/VSCode/casioPrizmApps/scripts/firmdump-recv.py -o ~/casio-dump
   ```
   It prints `waiting for the calculator ...`.
2. On the calculator, **with the cable unplugged**, open FirmDump. The top
   right should say "real hardware".
3. Select **Full ROM** with ▲/▼ and press **F2**. The screen says
   "Connect the USB cable".
4. Plug the cable in. If the OS asks for a connection mode, something else
   has taken USB: unplug, press EXIT and try again. FirmDump drives USB
   itself, so never choose USB Flash here.
5. The receiver prints `connected`, then `receiving ROM (... KB)` with
   the speed. The calculator shows a progress bar. When it ends, both sides
   say so, and the file is `~/casio-dump/ROM.bin`.
6. To send another region, select it and press F2 again; the link stays
   open. Press Ctrl+C on the PC when you're finished, then unplug.

Each region becomes its own file: `OS.bin`, `ROM.bin`, or the custom base
address (`80400000.bin`). A name already taken gets `-1`, `-2`, ... added.
A transfer that stops halfway is kept as `<name>.bin.partial`; delete it
and send the region again.

`fxlink -i` from the fxSDK can be used instead of the script, if you have
it. It saves each region as a `.bin` file whose name starts with
`firmdump-<name>`.

### If something goes wrong

| What you see | What to do |
|---|---|
| Receiver: "Permission denied opening the calculator" | Do step 2 of "Before the first dump", then plug in again |
| Receiver: "libusb-1.0 not found" | Install libusb (Linux: `libusb-1.0-0`; macOS: `brew install libusb`) |
| Receiver stays at "waiting" | Check that the cable carries data and that F2 was pressed before plugging in. `lsusb` should list `07cf:6102` |
| Calculator: "PC not reading: run firmdump-recv" | The receiver wasn't running or stopped for 10 s. Start it and press F2 again |
| Calculator: "USB open failed (n)" | Leave FirmDump, open it again, retry |
| EXIT during a transfer | The transfer stops, and the receiver keeps what arrived as `.partial` |

## Method 2: storage dump

1. Open FirmDump, select **OS firmware** and press **F1**. It writes
   `\\fls0\DUMP\OS00.bin` … `OS03.bin` (4 MB). EXIT cancels.
2. **Check whether the OS goes past 4 MB.** Select **Custom**, press **F4**
   and type `80400000`. For A–F, press the keys with those red letters
   (X,θ,T, log, ln, sin, cos, tan), without ALPHA.
   If the preview shows `FF FF FF …` there, the OS fits in the 4 MB you
   have. Otherwise, press **F5**, type `400000` and press **F1**. That
   writes the next 4 MB as `80400000.bin` … `80700000.bin`.
3. **Copy the files to the PC.** Plug in the cable and choose USB Flash
   (F1). On Linux, copy with 4 kB direct reads, because of the 4 kB
   transfer limit:
   ```sh
   mkdir -p ~/casio-dump
   for f in /media/$USER/*/DUMP/*.bin; do
     dd if="$f" of=~/casio-dump/"$(basename "$f")" bs=4k iflag=direct status=none
   done
   ls -l ~/casio-dump        # every part should be 1048576 bytes
   ```
4. Delete the `DUMP` folder on the calculator, then eject the drive before
   unplugging.
5. If the data went on past `80800000`, repeat steps 2–4 with the next base
   (`80800000`, `80C00000`, …). Clearing `DUMP` between windows keeps
   storage from filling up.
6. Join the parts in address order:
   ```sh
   cd ~/casio-dump
   cat OS00.bin OS01.bin OS02.bin OS03.bin 8*.bin > firmware.bin
   ```

## Checking the result

- **Size**: `ROM.bin` should be the ROM size FirmDump showed. The OS
  region should be a whole number of MB.
- **Not blank**: `xxd ROM.bin | head` should show code, not only `00` or
  `FF`. A file of zeros means it was dumped in the emulator, which has no
  OS.
- **Readable strings**: `strings -n 8 ROM.bin | head -50` should show
  Casio text (menu names, messages).
- **Same twice**: dump the OS region twice and compare
  (`cmp OS.bin OS-1.bin`). The OS area never changes, so the files must
  match. The full ROM includes storage memory, which can differ between
  runs.

Then write down where the files are and the OS version. Those two are
enough to start extracting the character table, the messages and the
F-key bitmaps for the emulator.
