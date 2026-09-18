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
#include "asm.h"
#include "bios.h"   /* kbdvecs */

#ifdef MACHINE_RT68ICE

static void rt68ice_usb_mouse_int(void);
static void rt68ice_usb_key_int(void);
static void process_usb_modifiers(UBYTE);

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
#define VIDEO_PLTE          (void *)(0x00f08000)           // Video Palette address

#define VIDEO_IRQ_VBL       0001

#define MODE_320X240_8BP   0x00  // 320x240 8 bitplanes
#define MODE_640X240_4BP   0x01  // 640x240 4 bitplanes
#define MODE_640X480_2BP   0x02  // 640x480 2 bitplanes

/* USB */
// Global interrupt registers
#define USB_IRQ_STATUS  *(volatile UWORD*)(0x00f18000)   // Read-only pending bits: bit 0 Host 1, bit 1 Host 2, bit 2 Host 3, bit 3 Host 4. Does not acknowledge a host.
#define USB_IRQ_ENABLE  *(volatile UWORD*)(0x00f18002)   // Read/write mask bits: bit 0 Host 1, bit 1 Host 2, bit 2 Host 3, bit 3 Host 4. Reset value $0000 disables USB CPU interrupts.

// USB Host 1 (word offsets 8-23; registers 18-23 reserved)
#define USB1_STATUS     *(volatile UWORD*)(0x00f18010)   // Read acknowledges Host 1. Bit 7: conErr (1=Error). Bits 1-0: type (0=None, 1=KB, 2=Mouse, 3=Pad).
#define USB1_MOUSE_BTN  *(volatile UWORD*)(0x00f18012)   // Bits 2-0: middle, right, left buttons.
#define USB1_MOUSE_DX   *(volatile UWORD*)(0x00f18014)   // Signed 16-bit X accumulator.
#define USB1_MOUSE_DY   *(volatile UWORD*)(0x00f18016)   // Signed 16-bit Y accumulator.
#define USB1_GAMEPAD    *(volatile UWORD*)(0x00f18018)   // Bits 9-0: U, D, L, R, A, B, X, Y, Start, Select.
#define USB1_KEY_MODS   *(volatile UWORD*)(0x00f1801a)   // USB HID modifier bitmap: bits 7-0 are RGUI, RALT, RSHIFT, RCTRL, LGUI, LALT, LSHIFT, LCTRL.
#define USB1_KEY1       *(volatile UWORD*)(0x00f1801c)   // First USB HID boot-keyboard usage ID; zero means no key.
#define USB1_KEY2       *(volatile UWORD*)(0x00f1801e)   // Second USB HID boot-keyboard usage ID; zero means no key.
#define USB1_KEY3       *(volatile UWORD*)(0x00f18020)   // Third USB HID boot-keyboard usage ID; zero means no key.
#define USB1_KEY4       *(volatile UWORD*)(0x00f18022)   // Fourth USB HID boot-keyboard usage ID; zero means no key.

// USB Host 2 (word offsets 24-39; registers 34-39 reserved)
#define USB2_STATUS     *(volatile UWORD*)(0x00f18030)   // Read acknowledges Host 2. Bit 7: conErr (1=Error). Bits 1-0: type (0=None, 1=KB, 2=Mouse, 3=Pad).
#define USB2_MOUSE_BTN  *(volatile UWORD*)(0x00f18032)   // Bits 2-0: middle, right, left buttons.
#define USB2_MOUSE_DX   *(volatile UWORD*)(0x00f18034)   // Signed 16-bit X accumulator.
#define USB2_MOUSE_DY   *(volatile UWORD*)(0x00f18036)   // Signed 16-bit Y accumulator.
#define USB2_GAMEPAD    *(volatile UWORD*)(0x00f18038)   // Bits 9-0: U, D, L, R, A, B, X, Y, Start, Select.
#define USB2_KEY_MODS   *(volatile UWORD*)(0x00f1803a)   // USB HID modifier bitmap: bits 7-0 are RGUI, RALT, RSHIFT, RCTRL, LGUI, LALT, LSHIFT, LCTRL.
#define USB2_KEY1       *(volatile UWORD*)(0x00f1803c)   // First USB HID boot-keyboard usage ID; zero means no key.
#define USB2_KEY2       *(volatile UWORD*)(0x00f1803e)   // Second USB HID boot-keyboard usage ID; zero means no key.
#define USB2_KEY3       *(volatile UWORD*)(0x00f18040)   // Third USB HID boot-keyboard usage ID; zero means no key.
#define USB2_KEY4       *(volatile UWORD*)(0x00f18042)   // Fourth USB HID boot-keyboard usage ID; zero means no key.

// USB Host 3 (word offsets 40-55; registers 50-55 reserved) 
#define USB3_STATUS     *(volatile UWORD*)(0x00f18050)   // Read acknowledges Host 3. Bit 7: conErr (1=Error). Bits 1-0: type (0=None, 1=KB, 2=Mouse, 3=Pad).
#define USB3_MOUSE_BTN  *(volatile UWORD*)(0x00f18052)   // Bits 2-0: middle, right, left buttons.
#define USB3_MOUSE_DX   *(volatile UWORD*)(0x00f18054)   // Signed 16-bit X accumulator.
#define USB3_MOUSE_DY   *(volatile UWORD*)(0x00f18056)   // Signed 16-bit Y accumulator.
#define USB3_GAMEPAD    *(volatile UWORD*)(0x00f18058)   // Bits 9-0: U, D, L, R, A, B, X, Y, Start, Select.
#define USB3_KEY_MODS   *(volatile UWORD*)(0x00f1805a)   // USB HID modifier bitmap: bits 7-0 are RGUI, RALT, RSHIFT, RCTRL, LGUI, LALT, LSHIFT, LCTRL.
#define USB3_KEY1       *(volatile UWORD*)(0x00f1805c)   // First USB HID boot-keyboard usage ID; zero means no key.
#define USB3_KEY2       *(volatile UWORD*)(0x00f1805e)   // Second USB HID boot-keyboard usage ID; zero means no key.
#define USB3_KEY3       *(volatile UWORD*)(0x00f18060)   // Third USB HID boot-keyboard usage ID; zero means no key.
#define USB3_KEY4       *(volatile UWORD*)(0x00f18062)   // Fourth USB HID boot-keyboard usage ID; zero means no key.

// USB Host 4 (word offsets 56-71; registers 66-71 reserved)
#define USB4_STATUS     *(volatile UWORD*)(0x00f18070)   // Read acknowledges Host 4. Bit 7: conErr (1=Error). Bits 1-0: type (0=None, 1=KB, 2=Mouse, 3=Pad).
#define USB4_MOUSE_BTN  *(volatile UWORD*)(0x00f18072)   // Bits 2-0: middle, right, left buttons.
#define USB4_MOUSE_DX   *(volatile UWORD*)(0x00f18074)   // Signed 16-bit X accumulator.
#define USB4_MOUSE_DY   *(volatile UWORD*)(0x00f18076)   // Signed 16-bit Y accumulator.
#define USB4_GAMEPAD    *(volatile UWORD*)(0x00f18078)   // Bits 9-0: U, D, L, R, A, B, X, Y, Start, Select.
#define USB4_KEY_MODS   *(volatile UWORD*)(0x00f1807a)   // USB HID modifier bitmap: bits 7-0 are RGUI, RALT, RSHIFT, RCTRL, LGUI, LALT, LSHIFT, LCTRL.
#define USB4_KEY1       *(volatile UWORD*)(0x00f1807c)   // First USB HID boot-keyboard usage ID; zero means no key.
#define USB4_KEY2       *(volatile UWORD*)(0x00f1807e)   // Second USB HID boot-keyboard usage ID; zero means no key.
#define USB4_KEY3       *(volatile UWORD*)(0x00f18080)   // Third USB HID boot-keyboard usage ID; zero means no key.
#define USB4_KEY4       *(volatile UWORD*)(0x00f18082)   // Fourth USB HID boot-keyboard usage ID; zero means no key.

#define USB_DEV_TYPE        0x0003
#define USB_DEV_TYPE_KEY    0x1
#define USB_DEV_TYPE_MOUSE  0x2


/* Initialize Native Features */
extern void rt68ice_init(void) 
{
    // Debug
    //LEDS = 0x1;
}


/******************************************************************************/
/* Screen                                                                     */
/******************************************************************************/
static UBYTE current_screen_mode;
ULONG* pword_vga_palette = (ULONG *)VIDEO_PLTE;
const UBYTE *rt68ice_screenbase;

/* 
 * Initialize graphic palette and video mode 
 */
void rt68ice_screen_init(void)
{
    // Set palette colors:
    pword_vga_palette[0] = 0x00FFFFFF; // color 0 xxRRGGBB (white)
    pword_vga_palette[1] = 0x00FF0000; // color 1 xxRRGGBB (red)
    pword_vga_palette[2] = 0x0000FF00; // color 2 xxRRGGBB (green)
    pword_vga_palette[3] = 0x00000000; // color 3 xxRRGGBB (black)

    /* Set VBL interrupt routine */
    VEC_LEVEL4 = rt68ice_vbl_int;

    VIDEO_IRQ_ENABLE = 0;               // Disable VGA interrupts during setup
    (void)VIDEO_IRQ_STATUS;             // Read to clear pending IRQ
    VIDEO_IRQ_ENABLE = VIDEO_IRQ_VBL;   // Disable VGA interrupts during setup

    /* Set screen mode and enable vblank interrupt */
    rt68ice_set_screen_mode(MODE_640X480_2BP);
}

ULONG rt68ice_vram_size(void)
{
    return 70800UL;
}

/*
 * returns the palette (number of colour choices) for the current hardware
 */
WORD rt68ice_get_palette(void)
{
    return 2;
}

WORD  rt68ice_vgetmode(void)
{
    return current_screen_mode;
}

void rt68ice_get_current_mode_info(UWORD *planes, UWORD *hz_rez, UWORD *vt_rez)
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

void rt68ice_setphys(const UBYTE *addr)
{
    rt68ice_screenbase = addr;
}

const UBYTE *rt68ice_physbase(void)
{
    return rt68ice_screenbase;
}

void rt68ice_set_screen_mode(UBYTE screen_mode) 
{
    VIDEO_CTRL = screen_mode;
    VIDEO_IRQ_ENABLE = VIDEO_IRQ_VBL;
    current_screen_mode = screen_mode;
}

WORD rt68ice_check_moderez(WORD moderez)
{
    return (moderez == current_screen_mode)?0:moderez;
}

/*
    Used by setscreen function (screen.c).
    It uses ST resolutions screen modes (low and med) but for 
    640x400 and 640x480, this may confuse  some applications. 
    A better approach could be the amiga one with VIDEL.
*/
void rt68ice_setrez(WORD rez, WORD videlmode)
{
    switch (rez)
    {
        case 0:
            rt68ice_set_screen_mode(MODE_320X240_8BP);
            break;

        case 2:
            rt68ice_set_screen_mode(MODE_640X240_4BP);
            break;

        case 1:
            rt68ice_set_screen_mode(MODE_640X480_2BP);
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

/******************************************************************************/
/* IKBD                                                                       */
/* Documentation: https://www.kernel.org/doc/Documentation/input/atarikbd.txt */
/******************************************************************************/
static UBYTE usb_mouse_buf_index;
static BOOL  usb_keyb_is_break;
//static BOOL  usb_keyb_is_ext;

void rt68ice_usb_init(void)
{
    
    usb_mouse_buf_index = 0;        /* Reset mouse buffer index */
    usb_keyb_is_break = FALSE;      /* Reset key */

    USB_IRQ_ENABLE = 0;             /* important on warm reset */
    (void)USB2_STATUS;              /* discard/ack any pending Host 2 report */

    /* Safe until init_acia_vecs() runs, without it mousevec is BSS and 
    therefore zero. call_mousevec() loads that zero callback and executes 
    jsr (a1) (bios/aciavecs.S:540), jumping into address 0. */
    kbdvecs.mousevec = just_rts;
    
    VEC_LEVEL6 = rt68ice_usb_int;   /* Set interrupt handlers */
    USB_IRQ_ENABLE = 0x0003;        /* Enable USB1 and USB2 interrupts */
}

/********************************************************************/
/* Requires                                                         */
/* - Keyboard on USB port 1                                         */
/* - Mouse on USB port 2                                            */
/* TODO: I could make it more generic and allow mouse on any port   */
/********************************************************************/
void rt68ice_usb_int_c(void)
{
    UWORD irq_status = USB_IRQ_STATUS;

    if (irq_status & 0x0001)
    {        
        UWORD status = USB1_STATUS;     /* acknowledge host 1 */
        
        if ((status & USB_DEV_TYPE) == USB_DEV_TYPE_KEY)
            rt68ice_usb_key_int();
    }
    
    if (irq_status & 0x0002)
    {
        UWORD status = USB2_STATUS;     /* acknowledge host 2 */

        if ((status & USB_DEV_TYPE) == USB_DEV_TYPE_MOUSE)
            rt68ice_usb_mouse_int();
    }

    // TODO: handle the other USB interrupts
    (void)USB3_STATUS;
    (void)USB4_STATUS;
}

static void rt68ice_usb_mouse_int(void) {
    LEDS = 0x2;

    UBYTE mouse_buttons = (UBYTE) USB2_MOUSE_BTN;
    SBYTE dx = (SBYTE) USB2_MOUSE_DX;
    SBYTE dy = (SBYTE) USB2_MOUSE_DY; 

    SBYTE packet[3];
    packet[0] = 0xf8; /* IKBD mouse packet header */

    if (mouse_buttons & 0x02)
        packet[0] |= 0x01; /* Right button */

    if (mouse_buttons & 0x01)
        packet[0] |= 0x02; /* Left button */

    packet[1] = dx;
    packet[2] = dy;

    /* Send mouse packet to IKBD handler */
    call_mousevec(packet);
}

/*
 * USB HID Keyboard/Keypad usage ID to Atari IKBD make code.
 *
 * A zero denotes an HID usage which has no Atari equivalent.  The eight HID
 * modifier usages (0xe0-0xe7) are deliberately not mapped here: they are
 * reported separately by USB1_KEY_MODS.
 *
 * Atari IKBD scan codes:
 * https://www.kernel.org/doc/Documentation/input/atarikbd.txt
 */
static const UBYTE usb_to_idkb_map[256] = {
    // 0     1     2     3     4     5     6     7     8     9     A     B     C     D     E     F
    0x00, 0x00, 0x00, 0x00, 0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, // 00: A-L
    0x32, 0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c, 0x02, 0x03, // 10: M-Z, 1-2
    0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x1c, 0x01, 0x0e, 0x0f, 0x39, 0x0c, 0x0d, 0x1a, // 20: 3-0, controls, [
    0x1b, 0x2b, 0x2b, 0x27, 0x28, 0x29, 0x33, 0x34, 0x35, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, // 30: ], punctuation, Caps, F1-F7
    0x41, 0x42, 0x43, 0x44, 0x00, 0x00, 0x00, 0x00, 0x52, 0x47, 0x00, 0x53, 0x00, 0x00, 0x4d, 0x4b, // 40: F8-F10, navigation
    0x50, 0x48, 0x00, 0x64, 0x65, 0x4a, 0x4e, 0x72, 0x6d, 0x6e, 0x6f, 0x6a, 0x6b, 0x6c, 0x67, 0x68, // 50: arrows, keypad / * - + Enter, 1-8
    0x69, 0x70, 0x71, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 60: keypad 9, 0, ., ISO key
    0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, // 70: Help, Undo
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 90
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // A0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // B0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // C0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // D0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // E0
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // F0
};

static const UBYTE modifier_scancodes[8] = {
    0x1d, /* bit 0: left Ctrl  */
    0x2a, /* bit 1: left Shift */
    0x38, /* bit 2: left Alt   */
    0x00, /* bit 3: left GUI   */
    0x1d, /* bit 4: right Ctrl */
    0x36, /* bit 5: right Shift*/
    0x38, /* bit 6: right Alt  */
    0x00  /* bit 7: right GUI  */
};
#define IDKB_BREAK              0x80
#define IDKB_CAPSLOCK           0x3a

static UBYTE last_key_mods = 0;
static UBYTE last_key1 = 0;
/*
static UBYTE last_key2 = 0;
static UBYTE last_key3 = 0;
static UBYTE last_key4 = 0;
*/
static void rt68ice_usb_key_int(void) {
    UBYTE curr_key_mods = (UBYTE)USB1_KEY_MODS;
    UBYTE curr_key1 = (UBYTE)USB1_KEY1;
    /*
    UBYTE curr_key2 = (UBYTE)USB1_KEY2;
    UBYTE curr_key3 = (UBYTE)USB1_KEY3;
    UBYTE curr_key4 = (UBYTE)USB1_KEY4;
    */


    if (curr_key1 != last_key1) {
        UBYTE idkb_code;
        if (curr_key1 != 0) 
            idkb_code = usb_to_idkb_map[curr_key1];              /* key pressed */
        else                
            idkb_code = usb_to_idkb_map[last_key1] | IDKB_BREAK; /* key released */

        call_ikbdraw(idkb_code);
        last_key1 = curr_key1;
    }

    /* Handle modifiers */
    process_usb_modifiers(curr_key_mods);
}

static void process_usb_modifiers(UBYTE current)
{
    UBYTE changed = current ^ last_key_mods;
    UBYTE bit;
    UBYTE scancode;

    for (bit = 0; bit < 8; bit++) {
        if (!(changed & (1 << bit)))
            continue;

        scancode = modifier_scancodes[bit];
        if (!scancode)
            continue;

        if (!(current & (1 << bit)))
            scancode |= 0x80;       /* key release */

        call_ikbdraw(scancode);
    }

    last_key_mods = current;
}


#endif /* MACHINE_RT68ICE */
