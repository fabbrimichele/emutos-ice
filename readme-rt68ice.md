# EmuTOS Porting to rt68ice

## How to load an EmuTOS to RT68ICE
```shell
python3 ~/rt68ice/tools/serial_load.py --port /dev/ttyACM0 --baud 57600 emutos-rt68ice.img
```

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
- 200 Hz system timer.

## Remaining work

- Boot from serial flash ROM: FPGA SPI-ROM device and a boot-loader driver.
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
2. Add the SPI-ROM boot path.
3. Add gamepad/joystick support.
4. Add the SDRAM cache, keeping I/O regions uncached and defining reset and
   cache-coherency behaviour.
5. Add four-bit SD-card mode.
6. Move to a 68020 and extend the memory map for 32 MiB.
