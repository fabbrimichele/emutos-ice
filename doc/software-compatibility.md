# EmuTOS software compatibility

Track programs tested with EmuTOS, including the tasks that worked and any
limitations. Results apply to the recorded program version, hardware and
configuration.
A program opening successfully is only a launch check.

## How to use this list

Test one or two programs per session. Add a row for each version tested and
record the actions you tried. Add detailed notes below when a failure or
workaround needs explaining. Reuse the same configuration ID until the FPGA,
EmuTOS build or another relevant part of the setup changes. Record screen mode
and colour depth per result; changing only the screen mode does not require a
new configuration ID.

Suggested checks, where applicable:

- Launch the program and exercise its menus or command input.
- Open a representative file, or create a small document/image.
- Edit it; check keyboard, mouse and screen updates.
- Save, close, reopen and check the contents.
- Exit and confirm the desktop or command shell still works.

Mark features you did not exercise as untested; record skipped checks as N/A.
Use copies of sample documents so each session is easy to repeat.

## Result labels

| Status | Meaning |
| --- | --- |
| Works | All tasks listed in the row succeeded; other features remain untested |
| Partial | Some tasks work, but a failure or workaround limits use |
| Fails | The intended basic workflow cannot be completed |
| Not tested | Candidate only; no result yet |

## Configurations

Copy this block when the setup changes. Source references are useful when
known; they are not proof of which binaries were loaded on the board.

### C01

- Date recorded: 2026-11-07
- Machine/board: RT68ICE on iCESugar Pro, Lattice ECP5 LFE5U-25F.
- FPGA source: [`220f7af`](https://github.com/fabbrimichele/rt68ice/commit/220f7af6f6ebac0008b99abb5059f805802cf79e),
  `main` after merging PR #40 (16-bit SPI register interface).
- EmuTOS source: [`3289c304`](https://github.com/fabbrimichele/emutos-ice/commit/3289c304e734517b383cbd9ae2c24669d8d3c85c),
  `master` after merging PR #17 (SPI CPU word-access driver); RT68ICE build.
- CPU and clock: TG68K in 68000 mode, 25 MHz system clock.
- Memory: 32 MiB physical SDRAM, 16-bit bus, 50 MHz SDRAM clock;
  14 MiB CPU window with EmuTOS loaded at `$00D80000`.
- Storage: SD over 12.5 MHz SPI, 16-bit payload transfers through CPU word
  data accesses. SD card model/capacity and filesystem details not recorded.
- Boot: user rebuilt both merged source trees, programmed the FPGA and
  EmuTOS image into flash, then reset the board. EmuTOS image is stored at
  flash offset `$00100000` and loaded by the ROM monitor.
- Launch environment: the reported FILETEST checks ran from the EmuTOS
  command shell. Record application launch arguments/environment in detailed
  results when relevant.
- Relevant setup: accessories, AUTO programs, additional drivers and exact
  input-device models not recorded. Screen mode belongs to each result below.
- Baseline checks: three 1 MiB mixed-size integrity passes succeeded;
  the retained 4 MiB `SPEEDW.DAT` passed full verification after the merged
  builds were flashed and the board reset. A further three 4 MiB speed passes
  on the merged builds all verified, averaging 252.6 KiB/s write and
  295.5 KiB/s read.

The source revisions above are the merged checkouts inspected for this entry,
consistent with the user's reported rebuild. Hashes of the programmed
bitstream and EmuTOS image were not captured.

Suggested source tag: `compat-c01` in both repositories, pointing to the
respective commits above. Tags have not been created. Keep the compatibility
list up to date independently; it can gain results without changing the
tested firmware configuration.

## Compatibility list

One row per program version, configuration and screen mode tested. Include
resolution and colour depth (for example, `640x480 mono`) in the screen-mode
column. Reuse C01 across modes; retain older results when retesting on a
different configuration.

| Program | Version | Config | Screen mode | Date tested | Status | Tested actions / limitations |
| --- | --- | --- | --- | --- | --- | --- |
| GEM Calculator — Axel T. Schreiner (`CALC.PRG`) | Not stated; local `CALC_2` copy | C01 | 640x480 / 1 bpp (monochrome) | 2026-11-07 | Works | User reports working; specific calculations, input methods and exit behaviour not recorded. |
| GEM Calculator — Axel T. Schreiner (`CALC.PRG`) | Not stated; local `CALC_2` copy | C01 | 640x400 / 1 bpp (monochrome) | 2026-11-07 | Works | User reports working; specific calculations, input methods and exit behaviour not recorded. |
| GEM Calculator — Axel T. Schreiner (`CALC.PRG`) | Not stated; local `CALC_2` copy | C01 | 640x480 / 2 bpp (4 colours) | 2026-11-07 | Works | User reports working; green desktop visible. Specific calculations, input methods and exit behaviour not recorded. |
| GEM Calculator — Axel T. Schreiner (`CALC.PRG`) | Not stated; local `CALC_2` copy | C01 | 320x240 / 8 bpp (256 colours) | 2026-11-07 | Works | Initial black title bar resolved after disabling XControl colour-module loading and rebooting. Specific calculations, input methods and exit behaviour not recorded. |
| Everest | 3.6Eß (23.1.1999) | C01 | 640x480 / 1 bpp (monochrome) | 2026-11-07 | Works | Created, saved and reopened a file; search, keyboard Help key, block selection and copy tested successfully. Startup reports minimum resolution 640x200. |
| Everest | 3.6Eß (23.1.1999) | C01 | 640x480 / 2 bpp (4 colours) | 2026-11-07 | Works | Created, saved and reopened a file; search, keyboard Help key, block selection and copy tested successfully. Startup reports minimum resolution 640x200. |
| Everest | 3.6Eß (23.1.1999) | C01 | 640x240 / 4 bpp (16 colours) | 2026-11-07 | Works | Created, saved and reopened a file; search, keyboard Help key, block selection and copy tested successfully. Startup reports minimum resolution 640x200. |
| ... | ... | C01 | ... | YYYY-MM-DD | Not tested | ... |

## Session notes

Copy this short block for each session. Detailed failure notes are optional.

### YYYY-MM-DD — session 1

- Configuration: C01
- Screen modes tested:
- Programs tested:
- Main findings:
- Next session: one or two programs, or a specific retest

## Program identification notes

### GEM Calculator (`CALC.PRG`)

The local `CALC_2` folder contains `CALC.PRG`, `CALC.RSC` and `C_SOURCE`.
Its source identifies the author as Axel T. Schreiner (copyright 1985).
`C_SOURCE/READ.ME` records a modulo-operation fix dated 24 May 1986; this is a
source-history date, not a confirmed application version. The executable hash
of the copy run on the board was not recorded.

### Everest

Version reported by the user: `3.6Eß (23.1.1999)`. At startup, the program
reports a minimum supported resolution of `640x200`; this message is not a
record of a separate test at that resolution. The reported mode list included
`640x480 / 1 bpp` twice and is recorded once.

## Observed issues

### Resolved configuration issue — black title bar in low-resolution modes

- Configuration: C01 source builds; control-panel colour settings were changed
  during the investigation, as noted in the affected result above.
- CALC and the desktop C: window showed a completely black title bar with an
  invisible right-hand button at `320x240 / 8 bpp`. The button still worked.
  The title-bar issue was also observed at `320x200 / 4 bpp`.
- The user reported the problem fixed after the suggested reset test:
  disabling XControl colour-module loading and rebooting. The instructions
  were to rename `WCOLORS.CPX` to `WCOLORS.OFF`, and also `COLOR.CPX` to
  `COLOR.OFF` if needed. Which module(s) were disabled was not specified.
- This resolves the reported symptom through control-panel configuration;
  no EmuTOS rendering or FPGA change was required. The exact saved setting
  responsible has not been identified.
- Restoring fresh CPX files from their original distribution is an option for
  re-enabling the modules without their previously saved settings.
- No separate complete CALC workflow test is recorded for `320x200 / 4 bpp`.

## Detailed result template

Copy for a result that needs more detail; remove unused fields.

### Program name — version — configuration C01

- Date:
- Screen mode and colour depth:
- Executable filename and launch command/arguments:
- Download/source URL, if known:
- Result: Works / Partial / Fails
- Tasks completed successfully:
- Features not tested:
- Failure: exact action, expected behaviour and observed behaviour
- Reproduction steps:
  1. ...
  2. ...
- Error/panic output: paste in a fenced text block below
- Workaround, if tested:
- Repeatability: every attempt / intermittent / tried once
- Retest result, configuration and screen mode, if applicable:

```text
Paste relevant output here, or remove this block.
```

A failure alone does not identify the cause. Keep observations separate from
suspected CPU, EmuTOS, video, input or unsupported Atari-hardware issues.
