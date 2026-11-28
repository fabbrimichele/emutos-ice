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

- DS3231 BIOS clock driver implemented on `ds3231-rtc-driver`, replacing
  DS1302 accesses with the matching RT68ICE I²C interface. It converts BCD
  dates, checks OSF, and enables battery timekeeping when setting the date.
  `make rt68ice` succeeds. A temporary TG68K simulation of the compiled
  driver passed 13 cases covering reads, 12-hour conversion, set/readback,
  leap day, invalid dates/BCD, OSF/century rejection and NACK/timeout fallback.
  On 2026-11-28, the welcome screen showed the RTC time
  `2026/11/28 17:22:40`; the user subsequently confirmed the hardware checks
  worked after being asked to test clock setting, reset and power-cycle
  retention. Standalone RTC tests also confirmed battery retention.

# TODO

- An EmuTOS temperature API, RTC alarms and the module's EEPROM remain
  deferred. Standalone temperature diagnostics are available in RT68ICE.
  See the [RT68ICE roadmap](https://github.com/fabbrimichele/rt68ice/blob/main/doc/TODOs.md)
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
