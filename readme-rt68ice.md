# EmuTOS Porting to rt68ice

## Validation results

See the [RT68ICE validation report](https://github.com/fabbrimichele/rt68ice/blob/main/doc/validation.md)
for on-board CoreMark and YAARTTT results, test configuration and coverage,
and the unresolved Dhrystone timing and GEMBench compatibility issues.

## Software compatibility

Record application test results in the
[EmuTOS software compatibility list](doc/software-compatibility.md).

## Building and loading EmuTOS

Build the RT68ICE image with:

```shell
make rt68ice
```

This generates both `emutos.img` (the raw payload) and `emutos-rt68ice.img`
(the payload with the monitor header). Use the **headered** image for both
serial loading and flash programming.

### Serial loading

Set **SW1 OFF and SW0 OFF**, then press the reset button to enter the monitor
immediately. Alternatively, with SW1 ON, press **ESC** in the serial terminal
during the approximately 10-second startup window. See
[Reset button and boot switches](#reset-button-and-boot-switches) below.
Then load it with:

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
`loadflash` and `boot`, with startup behavior selected by the DIP switches:
enter the monitor, boot immediately, or boot after an approximately 10-second
window during which ESC on the serial terminal cancels autoboot.

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

### Reset button and boot switches

The current FPGA design connects an active-low reset button to the system
reset logic. Pressing it resets the system; reset remains asserted while the
button is held and for approximately 20 ms after its synchronized release.
The monitor restarts and reads the boot switches once per reset. Set the
switches before pressing reset to apply a new selection.

| SW1 | SW0 | Startup behavior |
| --- | --- | --- |
| OFF | OFF | Enter the monitor immediately. |
| OFF | ON | Load, validate and boot the flash image immediately (normally EmuTOS). |
| ON | Either | Wait approximately 10 seconds, then boot; serial ESC cancels to the monitor. |

**SW1 overrides SW0. SW2 is reserved and currently ignored.** ON corresponds
to a logical set bit: the FPGA synchronizes and inverts the physical switch
inputs. The read-only DIP-switch device occupies `$F2C000`–`$F2FFFF`; a word
read at `$F2C000` returns SW0, SW1 and SW2 in bits 0, 1 and 2, with the other
bits zero. Writes have no effect. This is an RT68ICE-specific device, separate
from the Atari DIP-switch interface.

These options require the matching FPGA bitstream and monitor; rebuilding
EmuTOS alone does not add them. Further details are in the RT68ICE
[flash and monitor documentation](https://github.com/fabbrimichele/rt68ice/blob/main/doc/spi_flash.md#monitor-image-loading).

With SW1 ON, reset prints the title followed by:

```text
Boot: EmuTOS in 10 seconds; ESC on serial enters monitor
```

Press **ESC** (ASCII 27, `0x1b`) on the **serial terminal** during this window
to cancel autoboot and enter the monitor. The ESC character is consumed, leaving
the command prompt ready for input. Other received bytes are discarded without
restarting the countdown, so incidental serial input does not cancel autoboot.
If no ESC is received, the monitor automatically runs the same load-and-validate
path as `boot`. USB keyboard input does not
cancel the countdown. The delay uses a software polling loop calibrated for
25 MHz, so its duration is approximate. Immediate boot (SW1 OFF, SW0 ON)
does not offer this cancellation window; select both switches OFF and reset
to recover the monitor prompt.

### Manual flash loading and booting

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
writes flash. Flash loading and validation take additional time after the
selected startup delay; the current monitor uses a 1.5625 MHz SPI clock.
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

- Nine FPGA video modes, selectable from the desktop resolution dialog:

  | Hardware ID | Resolution | Bitplanes | Colours |
  | --- | --- | --- | --- |
  | 0 | 320x200 | 4 | 16 |
  | 1 | 640x200 | 2 | 4 |
  | 2 | 640x400 | 1 | 2 |
  | 3 | 320x240 | 4 | 16 |
  | 4 | 640x240 | 2 | 4 |
  | 5 | 640x480 | 1 | 2 |
  | 6 | 320x240 | 8 | 256 |
  | 7 | 640x240 | 4 | 16 |
  | 8 | 640x480 | 2 | 4 |

  Standard ST Low/Medium/High XBIOS resolution indices select hardware modes
  0/1/2. The boot default is ST High (640x400 monochrome). VIDEL-style mode
  descriptions distinguish the ST modes using `VIDEL_COMPAT`; the six extended
  modes retain their existing descriptions. All modes use the framebuffer at
  `0x00e00000`; the FPGA supplies the borders for the ST modes. The largest
  modes still require 75 KiB of video RAM. The 8-bitplane mode has full EmuTOS
  VDI support for 256 palette entries; the FPGA receives 24-bit RGB palette
  values. Lower-depth modes retain ST-compatible palette handling, with white
  background/black foreground in both monochrome modes.
- USB keyboards and mice on any of the four ports. Multiple keyboards share
  keyboard input, with held keys and modifiers tracked per port; multiple mice
  share the pointer and combine their held buttons.
- USB gamepads on any port: the first two detected gamepads in physical port
  order map to Atari joystick 1, then joystick 0. With gamepads on ports 3
  and 4, this preserves the previous mapping. Additional gamepads are ignored.
  Directions map to the Atari joystick direction bits; A, B, X and Y each
  act as fire. Select and Start are ignored. The driver sends standard IKBD
  joystick packets through `joyvec` when the state changes, and supports
  event reporting (`0x14`), interrogation mode (`0x15`), interrogation
  (`0x16`), and disabling joystick events (`0x1a`). Joystick monitoring and
  keycode modes are not implemented. Both joysticks can coexist with the mouse.
- 14 MiB SDRAM.  EmuTOS is loaded at `0x00d80000`, reserving its upper
  512 KiB image and leaving 13.5 MiB available to the system; video memory
  begins at `0x00e00000`.
  YAARTTT 0.2.3 tested approximately 13.35 MiB of free SDRAM: ten complete
  passes and pass 11 in progress, with zero reported errors. See the
  [memory validation results](https://github.com/fabbrimichele/rt68ice/blob/main/doc/validation.md#yaarttt-memory-integrity).
  Dedicated byte-lane preservation and mixed byte/word/longword checks would
  extend this coverage; they are optional follow-up tests. Reserved OS memory,
  low-memory BRAM and the framebuffer were outside the YAARTTT test range.
- Serial console at 57600 baud.
- SD card storage using SPI mode (one data bit).
- Onboard SPI flash image loading and EmuTOS boot via the ROM monitor, including
  DIP-selected monitor, immediate boot and delayed boot modes. Delayed boot
  offers an approximately 10-second serial ESC cancellation window.
- Hardware reset button and three read-only DIP switches, with boot settings
  applied on reset (requires the matching FPGA design and monitor).
- 200 Hz system timer.
- DS1302 real-time clock used by the BIOS date/time calls. RTC years 2000–2099
  map to the corresponding GEMDOS dates; set the clock with `Tsetdate`/`Tsettime`
  or the monitor utility described in the RT68ICE DS1302 documentation.
- PSG-compatible audio.

## Testing joystick input

The generic TOS joystick event diagnostic is maintained in the sibling
`emutos-apps` repository, under `joytest`. Build it on the host:

```sh
make -C ../emutos-apps/joytest
```

Copy `../emutos-apps/joytest/joytest.prg` to the SD card and run it under
EmuTOS. Alternatively, start a ZMODEM receiver on RT68ICE and run
`make -C ../emutos-apps/joytest send-setting-port` on the host.
Connect one gamepad to any USB port for joystick 1. With two gamepads, the
lower-numbered port supplies joystick 1 and the higher-numbered port supplies
joystick 0. Connect devices before reset for these tests; disconnect cleanup
and hot swapping are not covered by this change. The program displays
both joystick states whenever they change. Test each direction and A/B/X/Y:
up is `01`, down `02`, left `04`, right `08`, and fire `80`; combinations
OR these bits together. Releasing all controls should return the state to
`00`. Start and Select should have no effect. If two pads are connected,
verify that moving one does not clear the other pad's held controls.
Press any keyboard key to exit and restore the previous joystick callback.
This tests the USB interrupt and IKBD event callback path; it does not test
interrogation mode.

### Testing USB port routing

Run the host-side driver regression tests with:

```sh
python3 tools/test_rt68ice_usb.py
```

These exercise the production handlers with simulated registers, including
all keyboard/mouse port arrangements, shared keys/modifiers and mouse buttons,
gamepad assignment, and joystick interrogation. They require a host C compiler
with AddressSanitizer and UndefinedBehaviorSanitizer support.

After loading the updated EmuTOS image, check typing, modifiers, mouse movement
and clicks with the keyboard and mouse connected to different port pairs,
resetting between arrangements. Also test two keyboards holding the same key
and two mice holding the same button: releasing one must leave the other
device's press active. Physical board validation of this routing change is
pending. It uses the existing FPGA USB register interface and requires no new
bitstream.

## SD SPI payload transfers

SD-card SPI hardware support and the EmuTOS driver are implemented and have
passed the file-integrity and throughput tests recorded below. Further
performance improvements are optional. Verification after a reboot also passed,
as reported by the user on 2026-11-09.

The RT68ICE driver uses 16-bit SPI transfers for payload byte pairs, with
one CPU word access to the data register per pair. Aligned RAM buffers also
use word accesses; unaligned buffers use byte packing/unpacking to avoid
68000 address errors. Each 512-byte payload uses 256 START/completion
sequences. Commands, tokens, response polling and CRC bytes remain in 8-bit
SPI mode. The driver preserves the clock divisor and restores 8-bit mode
before returning, including for odd-length and discarded payloads. Other
targets retain their existing byte-transfer loops.

This build requires the FPGA's new SPI register interface, including low-byte
data reads. It is incompatible with the earlier split Data High/Data Low map:

| CPU address | Driver access |
| --- | --- |
| `$F20000` | Word data writes and payload reads |
| `$F20001` | Byte reads for 8-bit SPI responses |
| `$F20003` | Byte command/status |
| `$F20005` | Byte configuration |

Build with `make rt68ice`. On the matching FPGA, three 1 MiB mixed-size
integrity passes and three 4 MiB speed passes completed with all payloads
verified. Aggregate throughput reached **245.0 KiB/s write and 293.0 KiB/s
read**. Compared with the previous driver using separate data-byte registers
(206.3/225.5 KiB/s), this improves writes by **18.7%** and reads by **29.9%**.
Full results are in the
[validation report](https://github.com/fabbrimichele/rt68ice/blob/main/doc/validation.md#cpu-word-access-spi-interface).
The user also confirmed successful file verification after a reboot on
2026-11-09, validating persistence for the tested configuration. The test
filename and command output were not recorded with that confirmation.

After loading this EmuTOS image on the matching FPGA, use new filenames for
integrity and speed checks (the test refuses existing files):

```text
FILETEST run C:\CHECKW.DAT 1 3
FILETEST speed C:\SPEEDW.DAT 4 3
```

Compare against 206.3 KiB/s write and 225.5 KiB/s read using the same card and
benchmark binary to measure the additional benefit of CPU word data accesses.
After a successful complete run, reboot normally and run
`FILETEST verify C:\SPEEDW.DAT` to check persistence.

## Remaining work

- Check boot and recovery behaviour for absent or corrupt SD media. SD SPI
  hardware, driver implementation, file-integrity tests, sequential-throughput
  measurements and verification after a reboot are complete for the tested
  configuration.
- Extend `rt68ice_ikbd_writeb()` to support mouse and keyboard IKBD commands.
  It currently handles joystick reporting commands; USB mouse and keyboard
  input is delivered directly by the interrupt handlers, without implementing
  the corresponding IKBD commands.
- Replace the 68000 with a 68020 and revise the memory map to support 32 MiB.
- Use FPGA RAM as an SDRAM cache.

## Optional SD performance improvements

If further SD performance work is needed, use **245.0 KiB/s write and
293.0 KiB/s read** as the current baseline. Profile filesystem and
sector-transfer time before choosing a change. Possible improvements include
continuous SPI transfers with an FPGA FIFO or sector buffer; native four-bit
SD mode is a larger future extension.

Any such change should preserve shared SD/flash controller selection and be
checked with the same mixed-size integrity, sequential-throughput and
normal-reboot readback tests, plus monitor flash boot. Record results in the
[validation report](https://github.com/fabbrimichele/rt68ice/blob/main/doc/validation.md#cpu-word-access-spi-interface).
