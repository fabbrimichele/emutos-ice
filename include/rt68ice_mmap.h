/*
 * RT68ICE device addresses, shared by C, assembly and target configuration.
 * Matches the FPGA device block relocated above 32 MiB. Register offsets and
 * widths are unchanged; this does not expand the SDRAM window.
 */
#ifndef RT68ICE_MMAP_H
#define RT68ICE_MMAP_H

#define RT68ICE_FRAMEBUFFER_BASE     0x02000000
#define RT68ICE_LED_BASE             0x02100000
#define RT68ICE_UART_BASE            0x02104000
#define RT68ICE_VIDEO_PALETTE_BASE   0x02108000
#define RT68ICE_VIDEO_CTRL_BASE      0x0210c000
#define RT68ICE_LEDS_BASE            0x02114000
#define RT68ICE_USB_BASE             0x02118000
#define RT68ICE_TIMER_BASE           0x0211c000
#define RT68ICE_SPI_BASE             0x02120000
#define RT68ICE_PSG_BASE             0x02124000
#define RT68ICE_RTC_BASE             0x02128000

#define RT68ICE_TIMER_STATUS_ADDR    (RT68ICE_TIMER_BASE + 0x02)
#define RT68ICE_VIDEO_IRQ_STATUS_ADDR (RT68ICE_VIDEO_CTRL_BASE + 0x02)

#endif /* RT68ICE_MMAP_H */
