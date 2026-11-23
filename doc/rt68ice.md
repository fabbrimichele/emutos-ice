# EmuTOS porting to rt68ice
rt68ice is an FPGA single board computer based on 68000

# TODO

- Investigate keyboard software-restart boot modifiers in a separate branch.
  After Ctrl+Alt+Delete or Ctrl+Alt+Right Shift+Delete, startup can see Ctrl
  and Alt still held: the welcome countdown ends immediately, Alt selects
  A: instead of C:, and Ctrl skips accessories and the saved desktop
  configuration. Physical reset followed by selecting C: works. Check how
  the RT68ICE USB driver reports held keys across restart before changing
  generic EmuTOS boot logic; preserve deliberate Ctrl/Alt boot options.

The active device map is documented in
[the RT68ICE port README](../readme-rt68ice.md#relocated-device-map).
Devices start above 32 MiB. The FPGA RAM expansion passed standalone board
tests; the new EmuTOS image loads at `$01F80000` and configures 31.5 MiB
contiguous ST-RAM, reserving the top 512 KiB. The updated monitor accepts its
high load address. Boot and 31.5 MiB ST-RAM were confirmed after a power cycle.
YAARTTT reported
`$00026AE8–$01F80000`; completed-pass results, CoreMark, peripherals,
accessories/configuration and reset checks remain pending.
