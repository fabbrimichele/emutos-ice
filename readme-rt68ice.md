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

Load it through the RT68ICE monitor with:

```shell
python3 ~/rt68ice/tools/serial_load.py --port /dev/ttyACM0 --baud 57600 emutos-rt68ice.img
```

Close any serial terminal before loading. The script sends the image unchanged
and then sends `run 00D80000`; the monitor performs the CRC check. To load without
automatically sending `run`, add `--no-run`, reconnect the terminal, and check
for `Done.` before entering `run 00D80000`.

### Flash programming and booting

The RT68ICE `spi_flash_port` branch provides a two-port SPI controller:
port 0 remains the SD card and port 1 accesses the onboard 32 MiB W25Q256JV
flash through the ECP5 `USRMCLK` primitive. Its updated monitor supports
`loadflash` and `boot`.

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

Autoboot is **not enabled**: resets enter the monitor, and booting remains an
explicit command while EmuTOS is under active development.

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
- USB keyboard and mouse.
- 14 MiB SDRAM.  EmuTOS is loaded at `0x00d80000`, reserving its upper
  512 KiB image and leaving 13.5 MiB available to the system; video memory
  begins at `0x00e00000`.
- Serial console at 57600 baud.
- SD card storage using SPI mode (one data bit).
- Onboard SPI flash image loading and manual EmuTOS boot via the ROM monitor
  (with the updated RT68ICE FPGA design and monitor).
- 200 Hz system timer.

## Remaining work

- Optional monitor autoboot countdown with cancellation and recovery fallback
  (deferred during active EmuTOS development).
- Improve the video driver and add standard ST-compatible modes: 640x400 with
  1 bitplane, 640x200 with 2 bitplanes, and 320x200 with 4 bitplanes.
- Gamepad/joystick driver for EmuTOS.
- Replace the 68000 with a 68020 and revise the memory map to support 32 MiB.
- Use FPGA RAM as an SDRAM cache.
- Add four-bit SD-card mode.
- PSG-compatible audio.
- RTC / persistent clock.
- Boot and recovery behaviour for absent or corrupt SD media.
- SDRAM memory tests and SD-card read/write stress tests.

## Suggested implementation order

1. Add the standard ST video modes.
2. Add gamepad/joystick support.
3. Add the SDRAM cache, keeping I/O regions uncached and defining reset and
   cache-coherency behaviour.
4. Add four-bit SD-card mode.
5. Move to a 68020 and extend the memory map for 32 MiB.
