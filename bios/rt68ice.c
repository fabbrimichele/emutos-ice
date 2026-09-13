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
#define TIMER_CONTROL       *(volatile UWORD*)(0x00f1c000)   // bit 0 enable, bit 1 auto-reload, bit 2 IRQ enable, bit 3 reload command
#define TIMER_STATUS        *(volatile UWORD*)(0x00f1c002)   // bit 0 IRQ pending (read to clear), bit 1 running
#define TIMER_DIVIDER_HI    *(volatile UWORD*)(0x00f1c004)   // divider bits 31-16
#define TIMER_DIVIDER_LO    *(volatile UWORD*)(0x00f1c006)   // divider bits 15-0
#define TIMER_RELOAD_HI     *(volatile UWORD*)(0x00f1c008)   // reload value bits 31-16
#define TIMER_RELOAD_LO     *(volatile UWORD*)(0x00f1c00a)   // reload value bits 15-0
#define TIMER_VALUE_HI      *(volatile UWORD*)(0x00f1c00c)   // current value bits 31-16; latches VALUE_LO
#define TIMER_VALUE_LO      *(volatile UWORD*)(0x00f1c00e)   // latched current value bits 15-0

#define TIMER_ENABLE        0x0001
#define TIMER_AUTO_RELOAD   0x0002
#define TIMER_IRQ_ENABLE    0x0004
#define TIMER_RELOAD        0x0008
#define TIMER_IRQ_PENDING   0x0001



/* Initialize Native Features */
extern void rt68ice_init(void) 
{
    // Debug
    LEDS = 0x1;
}


/******************************************************************************/
/* Screen                                                                     */
/******************************************************************************/
// TODO
/*
## Screen
Atari TT uses the same planar layout but for higher resolutions and number of colors.
For example 640x480 16 colors (4 planes).
I could also implement 1280x960 1 plan in the FPGA.
*/


/******************************************************************************/
/* RS232                                                                      */
/******************************************************************************/

// TODO: replace rt68ice with rt68ice everywhere


void rt68ice_rs232_init(void) 
{
    // Main settings inhereted from boot loader

    VEC_LEVEL3 = rt68ice_rs232_int; // Set interrupt handler
    UART_IER = UART_IER_INT_RHR;    // Enable interrupt on receive holding register

    // Debug
    LEDS = 0x2;
}

void rt68ice_rs232_int_c(void) 
{
    // Debug
    LEDS = 0x3;

    if (UART_IIR != 4) // Check received rata ready and clean interrupt
        return;        // If not Received Data Ready, return

    push_serial_iorec(UART_RBR); // Read and push serial input byte    
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
void rt68f_init_system_timer(void)
{
    // Debug
    LEDS = 0x4;

    // Install the level-5 interrupt handler
    VEC_LEVEL5 = rt68f_timer_int;

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