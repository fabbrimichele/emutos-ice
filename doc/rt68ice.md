# EmuTOS porting to rt68ice
rt68ice is an FPGA single board computer based on 68000

# Completed

- Keyboard-triggered reset is implemented in `master`: wait for Ctrl, Alt,
  right Shift and Delete to be released before restarting. Generic boot-option
  handling remains unchanged. On 2026-11-24, the user confirmed it works and
  also confirmed the restored standard welcome behavior: no welcome delay on
  warm restart, three seconds on cold restart. No FPGA rebuild is needed.
  Staged release, multi-keyboard chords and explicit boot-option/configuration
  checks are optional additional validation, not remaining implementation work.

# TODO

- Next feature: DS3231 support in coordination with the RT68ICE I²C interface.
  Update RTC read/write routines and expose temperature for diagnostics;
  alarms and the module's EEPROM remain deferred. See the
  [RT68ICE roadmap](https://github.com/fabbrimichele/rt68ice/blob/main/doc/TODOs.md)
  for MIDI, CPU-mode, FreeMiNT, Doom and optional SDRAM-video work.

The active device map is documented in
[the RT68ICE port README](../readme-rt68ice.md#relocated-device-map).
Devices start above 32 MiB. The FPGA RAM expansion passed standalone board
tests; the new EmuTOS image loads at `$01F80000` and configures 31.5 MiB
contiguous ST-RAM, reserving the top 512 KiB. The updated monitor accepts its
high load address. Boot and 31.5 MiB ST-RAM were confirmed after a power cycle.
YAARTTT completed six clean passes over `$00026AE8–$01F80000` by 55:55,
with pass 7 continuing. One CoreMark run validated at 15.180265 iterations/sec,
consistent with the previous performance baseline. Further peripheral,
file-I/O, flash-boot and reset checks remain useful. The keyboard-reset fix and
restored welcome defaults are now implemented in `master` as recorded above.
