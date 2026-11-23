# EmuTOS porting to rt68ice
rt68ice is an FPGA single board computer based on 68000

# TODO

- Complete keyboard-reset edge-case checks on the board. The RT68ICE keyboard reset
  path now polls hardware USB reports until Ctrl, Alt, right Shift and Delete
  are released across all keyboard ports, before starting either warm or cold
  restart. Generic EmuTOS boot-option handling is unchanged. Check both reset
  shortcuts, keys released in stages, and a chord split across keyboards.
  Verify no welcome delay on warm restart and a three-second welcome countdown
  on cold restart, plus C: selection, accessories and saved desktop
  configuration; also verify deliberately pressing Ctrl/Alt during startup.
  The image builds locally. On 2026-11-24, the user confirmed the restored
  standard welcome behavior works on the board; the additional checks above
  have not all been reported.

The active device map is documented in
[the RT68ICE port README](../readme-rt68ice.md#relocated-device-map).
Devices start above 32 MiB. The FPGA RAM expansion passed standalone board
tests; the new EmuTOS image loads at `$01F80000` and configures 31.5 MiB
contiguous ST-RAM, reserving the top 512 KiB. The updated monitor accepts its
high load address. Boot and 31.5 MiB ST-RAM were confirmed after a power cycle.
YAARTTT completed six clean passes over `$00026AE8–$01F80000` by 55:55,
with pass 7 continuing. One CoreMark run validated at 15.180265 iterations/sec,
consistent with the previous performance baseline. Further peripheral,
file-I/O, flash-boot and reset checks remain useful; the keyboard-reset boot
modifier issue above is deferred to a separate branch.
