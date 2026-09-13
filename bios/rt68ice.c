/*
 * rt68ice.c - rt68ice specific functions
 *
 * Copyright (C) 2013-2025 The EmuTOS development team
 *
 * Authors:
 *  MF   Michele Fabbri
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */

#include "emutos.h"
#include "rt68ice.h"
#include "vectors.h"
#include "tosvars.h"
#include "ikbd.h"
#include "serport.h"
#include "biosext.h" // For rt68ice_vgetmode

#ifdef MACHINE_RT68ICE

/* Custom registers */
#define LED      *(volatile UBYTE*)(0x00f00000) // LED-mapped register base address
#define LEDS     *(volatile UWORD*)(0x00f14000) // LED array mapped register base address

/* Serial registers */
#define UART_RBR *(volatile UBYTE*)(0x00f04000) // Receive Buffer Register(RBR) / Transmitter Holding Register(THR) / Divisor Latch (LSB)
#define UART_IER *(volatile UBYTE*)(0x00f04002) // Interrupt enable register / Divisor Latch (MSB)
#define UART_IIR *(volatile UBYTE*)(0x00f04004) // Interrupt Identification Register
#define UART_LCR *(volatile UBYTE*)(0x00f04006) // Line control register
#define UART_MCR *(volatile UBYTE*)(0x00f04008) // MODEM control register
#define UART_LSR *(volatile UBYTE*)(0x00f0400a) // Line status register
#define UART_MSR *(volatile UBYTE*)(0x00f0400c) // MODEM status register

#define UART_IER_INT_RHR        0x01 // Enable interrupt on receive holding register
#define UART_LSR_THE            0x20 // Transmission Holding Empty

/* Programmable timer (68000 autovector interrupt level 5) */
#define TIMER_CONTROL       *(volatile UWORD*)(0x00f1c000) // bit 0 enable, bit 1 auto-reload, bit 2 IRQ enable, bit 3 reload command
#define TIMER_STATUS        *(volatile UWORD*)(0x00f1c002) // bit 0 IRQ pending (read to clear), bit 1 running
#define TIMER_DIVIDER_HI    *(volatile UWORD*)(0x00f1c004) // divider bits 31-16
#define TIMER_DIVIDER_LO    *(volatile UWORD*)(0x00f1c006) // divider bits 15-0
#define TIMER_RELOAD_HI     *(volatile UWORD*)(0x00f1c008) // reload value bits 31-16
#define TIMER_RELOAD_LO     *(volatile UWORD*)(0x00f1c00a) // reload value bits 15-0
#define TIMER_VALUE_HI      *(volatile UWORD*)(0x00f1c00c) // current value bits 31-16; latches VALUE_LO
#define TIMER_VALUE_LO      *(volatile UWORD*)(0x00f1c00e) // latched current value bits 15-0

#define TIMER_ENABLE        0x0001
#define TIMER_AUTO_RELOAD   0x0002
#define TIMER_IRQ_ENABLE    0x0004
#define TIMER_RELOAD        0x0008
#define TIMER_IRQ_PENDING   0x0001

/* Screen */
#define VIDEO_CTRL          *(volatile UWORD*)(0x00f0c000) // Resolution control
#define VIDEO_IRQ_STATUS    *(volatile UWORD*)(0x00f0c002) // Bit 0 VBL pending; read to acknowledge
#define VIDEO_IRQ_ENABLE    *(volatile UWORD*)(0x00f0c004) // Bit 0 VBL interrupt enable
#define VIDEO_PLTE          *(volatile UWORD*)(0x00f08000) // Video Palette Registers

#define VIDEO_IRQ_VBL       0001

#define MODE_320X240_8BP   0x00  // 320x240 8 bitplanes
#define MODE_640X240_4BP   0x01  // 640x240 4 bitplanes
#define MODE_640X480_2BP   0x02  // 640x480 2 bitplanes


/* Initialize Native Features */
extern void rt68ice_init(void) 
{
    // Debug
    LEDS = 0x1;
}


/******************************************************************************/
/* Screen                                                                     */
/******************************************************************************/
static UBYTE current_screen_mode;
//UWORD* pword_vga_palette = (UWORD *)VIDEO_PLTE;
const UBYTE *rt68f_screenbase;

/* 
 * Initialize graphic palette and video mode 
 */
void rt68f_screen_init(void)
{
    // Set palette colors:
    // TODO: configure palette
    //pword_vga_palette[0] = 0x0FFF; // color 0 xRGB (white)
    //pword_vga_palette[1] = 0x0000; // color 1 xRGB (black)

    /* Set VBL interrupt routine */
    VEC_LEVEL4 = rt68f_vbl_int;

    VIDEO_IRQ_ENABLE = 0;               // Disable VGA interrupts during setup
    (void)VIDEO_IRQ_STATUS;             // Read to clear pending IRQ
    VIDEO_IRQ_ENABLE = VIDEO_IRQ_VBL;   // Disable VGA interrupts during setup


    /* Set screen mode and enable vblank interrupt */
    rt68f_set_screen_mode(MODE_640X480_2BP);
}

ULONG rt68f_vram_size(void)
{
    return 70800UL;
}

/*
 * returns the palette (number of colour choices) for the current hardware
 */
WORD rt68f_get_palette(void)
{
    return 2;
}

WORD  rt68f_vgetmode(void)
{
    return current_screen_mode;
}

void rt68f_get_current_mode_info(UWORD *planes, UWORD *hz_rez, UWORD *vt_rez)
{
    switch (current_screen_mode)
    {
        case MODE_320X240_8BP:
            *hz_rez = 320;
            *vt_rez = 240;
            *planes = 8;
            break;

        case MODE_640X240_4BP:
            *hz_rez = 640;
            *vt_rez = 240;
            *planes = 4;
            break;

        case MODE_640X480_2BP:
            *hz_rez = 640;
            *vt_rez = 480;
            *planes = 2;
            break;

        default:
            break;
    }    
}

void rt68f_setphys(const UBYTE *addr)
{
    rt68f_screenbase = addr;
}

const UBYTE *rt68f_physbase(void)
{
    return rt68f_screenbase;
}

void rt68f_set_screen_mode(UBYTE screen_mode) 
{
    VIDEO_CTRL = screen_mode;
    VIDEO_IRQ_ENABLE = VIDEO_IRQ_VBL;
    current_screen_mode = screen_mode;
}

WORD rt68f_check_moderez(WORD moderez)
{
    return (moderez == current_screen_mode)?0:moderez;
}

/*
    Used by setscreen function (screen.c).
    It uses ST resolutions screen modes (low and med) but for 
    640x400 and 640x480, this may confuse  some applications. 
    A better approach could be the amiga one with VIDEL.
*/
void rt68f_setrez(WORD rez, WORD videlmode)
{
    switch (rez)
    {
        case 0:
            rt68f_set_screen_mode(MODE_320X240_8BP);
            break;

        case 2:
            rt68f_set_screen_mode(MODE_640X240_4BP);
            break;

        case 1:
            rt68f_set_screen_mode(MODE_640X480_2BP);
            break;

        default:
            break;
    }
}

/******************************************************************************/
/* RS232                                                                      */
/******************************************************************************/
void rt68ice_rs232_init(void) 
{
    // Main settings inhereted from boot loader

    VEC_LEVEL3 = rt68ice_rs232_int; // Set interrupt handler
    UART_IER = UART_IER_INT_RHR;    // Enable interrupt on receive holding register
}

void rt68ice_rs232_int_c(void) 
{
    if (UART_IIR != 4) // Check received rata ready and clean interrupt
        return;        // If not Received Data Ready, return

    // Read and push serial input byte
    UBYTE c = UART_RBR;
    LEDS = c;
    push_serial_iorec(c);
}

BOOL rt68ice_rs232_can_write(void)
{
    // Check if space is available in the FIFO
    return UART_LSR & UART_LSR_THE; // Transmission Holding Empty
}

void rt68ice_rs232_write_byte(UBYTE b)
{
    while (!rt68ice_rs232_can_write()); // Wait
    
    // Send the byte
    UART_RBR = b;
}

void kprintf_outc_rt68ice_rs232(int c)
{
    // Raw terminals usually require CRLF 
    if ( c == '\n')
        rt68ice_rs232_write_byte('\r');

    rt68ice_rs232_write_byte((char)c);
}

/******************************************************************************/
/* Timer                                                                      */
/******************************************************************************/
void rt68ice_init_system_timer(void)
{
    // Install the level-5 interrupt handler
    VEC_LEVEL5 = rt68ice_timer_int;

    // Stop the timer and clear any pending interrupt
    TIMER_CONTROL = 0;
    (void)TIMER_STATUS; // Read to clear pending IRQ

    // Configure divider and reload values (200Hz tick at 25 MHz)
    TIMER_DIVIDER_HI = 0;
    TIMER_DIVIDER_LO = 124;
    TIMER_RELOAD_HI  = 0;
    TIMER_RELOAD_LO  = 1000;

    // Enable the timer with auto-reload and IRQ enabled
    TIMER_CONTROL = TIMER_ENABLE | TIMER_AUTO_RELOAD | TIMER_IRQ_ENABLE;
}

#endif /* MACHINE_RT68ICE */