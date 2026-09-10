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

/* Serial Feature Bit Flags */
#define UART_IER_INT_RHR        0x01 // Enable interrupt on receive holding register
#define UART_LSR_THE            0x20 // Transmission Holding Empty


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
void rt68f_rs232_init(void) 
{
    // Main settings inhereted from boot loader

    VEC_LEVEL3 = rt68f_rs232_int; // Set interrupt handler
    UART_IER = UART_IER_INT_RHR;  // Enable interrupt on receive holding register

    // Debug
    LEDS = 0x2;
}

void rt68f_rs232_int_c(void) 
{
    // Debug
    LEDS = 0x3;

    if (UART_IIR != 4) // Check received rata ready and clean interrupt
        return;        // If not Received Data Ready, return

    push_serial_iorec(UART_RBR); // Read and push serial input byte    
}


BOOL rt68f_rs232_can_write(void)
{
    // Debug
    LEDS = 0x4;

    /*
    TODO: it stopped here, perhaps the serial is not configured properly?
          or it's accessing the wrong addresses?
          Check if the access is byte or word oriented and compare it with the RT68F    
    
    UART_IER_INT_RHR        0x01; // Enable interrupt on receive holding register
    UART_LSR_THE            0x20; // Transmission Holding Empty

    Below a working asm code:
        put_chr:
        movem.l d1,-(sp)
    .wait:
        move.b  UART_LSR,d1
        btst    #5,d1   		; write buffer empty?
        beq     .wait    		; eq 0, not ready, check again
        move.b  d0,UART_RBR		; write d0 to serial
        movem.l (sp)+,d1
        rts						; return
    */


    // Check if space is available in the FIFO
    return UART_LSR & UART_LSR_THE; // Transmission Holding Empty
}

void rt68f_rs232_write_byte(UBYTE b)
{
    // Debug
    LEDS = 0x5;

    while (!rt68f_rs232_can_write()); // Wait
    
    // Send the byte
    UART_RBR = (UWORD)b;
}

void kprintf_outc_rt68f_rs232(int c)
{
    // Debug
    LEDS = 0x6;

    // Raw terminals usually require CRLF 
    if ( c == '\n')
        rt68f_rs232_write_byte('\r');

    rt68f_rs232_write_byte((char)c);
}

#endif /* MACHINE_RT68ICE */