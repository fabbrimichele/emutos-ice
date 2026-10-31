# EmuTOS Porting to rt68ice

## Building and loading EmuTOS

Build the RT68ICE image with:

```shell
make rt68ice
```

This generates both `emutos.img` (the raw payload) and `emutos-rt68ice.img`
(the payload with the monitor header). Use the **headered** image for both
serial loading and flash programming.

### Serial loading

With the autoboot monitor, press **ESC** in the serial terminal during the
approximately 10-second startup window to enter the monitor. Then load it with:

```shell
python3 ~/rt68ice/tools/serial_load.py --port /dev/ttyACM0 --baud 57600 emutos-rt68ice.img
```

Close any serial terminal before loading. The script sends the image unchanged
and then sends `run 00D80000`; the monitor performs the CRC check. To load without
automatically sending `run`, add `--no-run`, reconnect the terminal, and check
for `Done.` before entering `run 00D80000`.

### Flash programming and booting

The current RT68ICE `main` branch provides a two-port SPI controller:
port 0 remains the SD card and port 1 accesses the onboard 32 MiB W25Q256JV
flash through the ECP5 `USRMCLK` primitive. Its updated monitor supports
`loadflash` and `boot`, and automatically boots after an approximately
10-second window during which ESC on the serial terminal cancels autoboot.

After building EmuTOS here, close the serial terminal and run these commands
from the sibling **rt68ice repository**, with its updated bitstream built:

```shell
make prog-emutos
make prog
make serial-open
```

`prog-emutos` writes `../emutos-ice/emutos-rt68ice.img` at flash offset
`0x00100000` (1 MiB) and verifies the write. It does not rebuild EmuTOS. To use
another headered image path, set `EMUTOS_IMAGE`, for example:

```shell
make prog-emutos EMUTOS_IMAGE=/path/to/emutos-rt68ice.img
```

The write replaces the application image and affected erase sectors, preserving
the configuration region below 1 MiB. Do not write the EmuTOS image at offset
zero, use a bulk erase, or pass it to the ordinary FPGA `prog-flash` target.
The FPGA bitstream must fit entirely below the application offset.
`make prog` restores the updated FPGA design into SRAM after flash programming;
it does not modify flash and is not persistent across power cycles. To persist
the updated FPGA design, the separate `make prog-flash` target programs the
**FPGA bitstream**, not EmuTOS.

With the autoboot monitor, reset prints the title followed by:

```text
Press ESC on serial to start monitor (10 seconds)...
```

Press **ESC** (ASCII 27, `0x1b`) on the **serial terminal** during this window
to cancel autoboot and enter the monitor. The ESC character is consumed, leaving
the command prompt ready for input. Other received bytes are discarded without
restarting the countdown, so incidental serial input does not cancel autoboot.
If no ESC is received, the monitor automatically runs the same load-and-validate
path as `boot`. USB keyboard input does not
cancel the countdown. The delay uses a software polling loop calibrated for
25 MHz, so its duration is approximate. The older monitor requires manual boot.

At the monitor prompt:

```text
loadflash
```

This reads the fixed flash offset `0x00100000`, validates the image and copies
its payload to the address from its header. It should print `Done.` and
`Loaded at 00D80000`, then stay in the monitor. Enter `run 00D80000` to execute
the loaded image, or use:

```text
boot
```

`boot` loads and validates the flash image again, then jumps to its load
address only if successful. Neither command takes an address argument or
writes flash. At the conservative SPI clock, loading can take tens of seconds.
Missing/corrupt images, invalid ranges, busy flash or SPI timeouts return to
the monitor without booting. A failed load may leave untrusted data in SDRAM;
do not manually execute it.

If automatic boot fails, the monitor reports the error and presents its command
prompt without restarting the countdown. The 10-second window is the time to
cancel autoboot; flash loading and validation take additional time. The RGB LED
stays off during normal monitor and boot operation and turns red on image load
or boot errors.

### Image format and validation

The image is a big-endian monitor transfer image, not a raw `emutos.img`:

| Offset | Size | Field |
| --- | ---: | --- |
| `0x00` | 4 bytes | ASCII magic `RT68` |
| `0x04` | 4 bytes | Load address (`0x00d80000`) |
| `0x08` | 4 bytes | Payload length |
| `0x0c` | 4 bytes | CRC-32/ISO-HDLC of the payload |
| `0x10` | variable | Raw EmuTOS payload |

The updated monitor shares header parsing and CRC-32/ISO-HDLC validation
between serial and flash loading. It rejects invalid magic, a zero payload
length, an odd load address, or a payload outside application SDRAM
(`0x00010000` through `0x00dfffff`). Flash lengths must also fit the flash
capacity. It calculates CRC while copying the payload and reports a CRC error
if the calculated value differs from the header.
`tools/make_rt68ice_image.py` creates this header as part of `make rt68ice`.

## Current hardware support

- Video modes: 320x240 with 4 or 8 bitplanes, 640x240 with 2 or 4
  bitplanes, and 640x480 with 1 or 2 bitplanes.  The 4/2/1-bitplane modes
  are selected through the standard ST Low/Medium/High resolution indices.
  The 8-bitplane mode has full EmuTOS VDI support for 256 palette entries;
  the FPGA receives 24-bit RGB palette values.  The lower-depth modes retain
  ST-compatible palette handling (including white background/black foreground
  in 640x480 monochrome mode).
- USB keyboard on port 1 and mouse on port 2.
- USB gamepads on ports 3 (Atari joystick 1) and 4 (Atari joystick 0).
  Directions map to the Atari joystick direction bits; A, B, X and Y each
  act as fire. Select and Start are ignored. The driver sends standard IKBD
  joystick packets through `joyvec` when the state changes, and supports
  event reporting (`0x14`), interrogation mode (`0x15`), interrogation
  (`0x16`), and disabling joystick events (`0x1a`). Joystick monitoring and
  keycode modes are not implemented. Both joysticks can coexist with the mouse.
- 14 MiB SDRAM.  EmuTOS is loaded at `0x00d80000`, reserving its upper
  512 KiB image and leaving 13.5 MiB available to the system; video memory
  begins at `0x00e00000`.
- Serial console at 57600 baud.
- SD card storage using SPI mode (one data bit).
- Onboard SPI flash image loading and EmuTOS boot via the ROM monitor, including
  automatic boot with an approximately 10-second serial cancellation window
  (with the current RT68ICE `main` FPGA design and monitor; press ESC to cancel).
- 200 Hz system timer.
- DS1302 real-time clock used by the BIOS date/time calls. RTC years 2000–2099
  map to the corresponding GEMDOS dates; set the clock with `Tsetdate`/`Tsettime`
  or the monitor utility described in the RT68ICE DS1302 documentation.

## Testing joystick input

The generic TOS joystick event diagnostic is maintained in the sibling
`emutos-apps` repository, under `joytest`. Build it on the host:

```sh
make -C ../emutos-apps/joytest
```

Copy `../emutos-apps/joytest/joytest.prg` to the SD card and run it under
EmuTOS. Alternatively, start a ZMODEM receiver on RT68ICE and run
`make -C ../emutos-apps/joytest send-setting-port` on the host.
Connect a gamepad
to USB port 3 for joystick 1, or port 4 for joystick 0. The program displays
both joystick states whenever they change. Test each direction and A/B/X/Y:
up is `01`, down `02`, left `04`, right `08`, and fire `80`; combinations
OR these bits together. Releasing all controls should return the state to
`00`. Start and Select should have no effect. If two pads are connected,
verify that moving one does not clear the other pad's held controls.
Press any keyboard key to exit and restore the previous joystick callback.
This tests the USB interrupt and IKBD event callback path; it does not test
interrogation mode.

## Remaining work

- Extend `rt68ice_ikbd_writeb()` to support mouse and keyboard IKBD commands.
  It currently handles joystick reporting commands; USB mouse and keyboard
  input is delivered directly by the interrupt handlers, without implementing
  the corresponding IKBD commands.
- Improve the video driver and add standard ST-compatible modes: 640x400 with
  1 bitplane, 640x200 with 2 bitplanes, and 320x200 with 4 bitplanes.
- Replace the 68000 with a 68020 and revise the memory map to support 32 MiB.
- Use FPGA RAM as an SDRAM cache.
- Add four-bit SD-card mode.
- PSG-compatible audio.
- Boot and recovery behaviour for absent or corrupt SD media.
- SDRAM memory tests and SD-card read/write stress tests.

## Suggested implementation order

1. Add the standard ST video modes.
2. Add the SDRAM cache, keeping I/O regions uncached and defining reset and
   cache-coherency behaviour.
3. Add four-bit SD-card mode.
4. Move to a 68020 and extend the memory map for 32 MiB.
