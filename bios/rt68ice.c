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
static void rt68ice_usb_joystick_int(UBYTE joynum, UWORD status, UWORD gamepad);
static void process_usb_modifier(UBYTE, UBYTE, UBYTE);

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

#define MODE_320X240_4BP   0x00
#define MODE_640X240_2BP   0x01
#define MODE_640X480_1BP   0x02
#define MODE_320X240_8BP   0x03
#define MODE_640X240_4BP   0x04
#define MODE_640X480_2BP   0x05

/* USB */
// Global interrupt registers
#define USB_IRQ_STATUS  *(volatile UWORD*)(0x00f18000)   // Read-only pending bits: bit 0 Host 1, bit 1 Host 2, bit 2 Host 3, bit 3 Host 4. Does not acknowledge a host.
#define USB_IRQ_ENABLE  *(volatile UWORD*)(0x00f18002)   // Read/write mask bits: bit 0 Host 1, bit 1 Host 2, bit 2 Host 3, bit 3 Host 4. Reset value $0000 disables USB CPU interrupts.

// USB Host 1 (word offsets 8-23; registers 18-23 reserved)
#define USB1_STATUS     *(volatile UWORD*)(0x00f18010)   // Read acknowledges Host 1. Bit 7: conErr (1=Error). Bits 1-0: type (0=None, 1=KB, 2=Mouse, 3=Pad).
#define USB1_MOUSE_BTN  *(volatile UWORD*)(0x00f18012)   // Bits 2-0: middle, right, left buttons.
#define USB1_MOUSE_DX   *(volatile UWORD*)(0x00f18014)   // Signed 16-bit X accumulator.
#define USB1_MOUSE_DY   *(volatile UWORD*)(0x00f18016)   // Signed 16-bit Y accumulator.
#define USB1_GAMEPAD    *(volatile UWORD*)(0x00f18018)   // Bits 9-0: L, R, U, D, A, B, X, Y, Select, Start.
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
#define USB2_GAMEPAD    *(volatile UWORD*)(0x00f18038)   // Bits 9-0: L, R, U, D, A, B, X, Y, Select, Start.
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
#define USB3_GAMEPAD    *(volatile UWORD*)(0x00f18058)   // Bits 9-0: L, R, U, D, A, B, X, Y, Select, Start.
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
#define USB4_GAMEPAD    *(volatile UWORD*)(0x00f18078)   // Bits 9-0: L, R, U, D, A, B, X, Y, Select, Start.
#define USB4_KEY_MODS   *(volatile UWORD*)(0x00f1807a)   // USB HID modifier bitmap: bits 7-0 are RGUI, RALT, RSHIFT, RCTRL, LGUI, LALT, LSHIFT, LCTRL.
#define USB4_KEY1       *(volatile UWORD*)(0x00f1807c)   // First USB HID boot-keyboard usage ID; zero means no key.
#define USB4_KEY2       *(volatile UWORD*)(0x00f1807e)   // Second USB HID boot-keyboard usage ID; zero means no key.
#define USB4_KEY3       *(volatile UWORD*)(0x00f18080)   // Third USB HID boot-keyboard usage ID; zero means no key.
#define USB4_KEY4       *(volatile UWORD*)(0x00f18082)   // Fourth USB HID boot-keyboard usage ID; zero means no key.

#define USB_DEV_TYPE        0x0003
#define USB_DEV_TYPE_KEY    0x1
#define USB_DEV_TYPE_MOUSE  0x2
#define USB_DEV_TYPE_PAD    0x3
#define USB_DEV_ERROR       0x0080

/* UsbDevice.scala concatenates L, R, U, D, A, B, X, Y, Select, Start. */
#define USB_PAD_LEFT        0x0200
#define USB_PAD_RIGHT       0x0100
#define USB_PAD_UP          0x0080
#define USB_PAD_DOWN        0x0040
#define USB_PAD_FIRE        0x003c  /* A, B, X or Y */


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
static volatile ULONG * const pword_vga_palette = (volatile ULONG *)VIDEO_PLTE;
const UBYTE *rt68ice_screenbase;

/* The ST XBIOS palette uses three bits per component: 0x0RGB. */
static const UWORD rt68ice_default_st_palette[16] = {
    0x0fff, 0x0f00, 0x00f0, 0x0ff0,
    0x000f, 0x0f0f, 0x00ff, 0x0555,
    0x0333, 0x0f33, 0x03f3, 0x0ff3,
    0x033f, 0x0f3f, 0x03ff, 0x0000
};
static ULONG rt68ice_palette[256];

struct rt68ice_video_mode {
    UBYTE hardware_mode;
    UWORD videl_mode;
    UWORD width;
    UWORD height;
    UBYTE planes;
};

/* Hardware modes 0-2 are selected by the standard ST XBIOS resolution
 * indices.  The remaining modes are available through their VIDEL-style
 * description, while the FPGA register itself remains hardware-specific. */
static const struct rt68ice_video_mode rt68ice_video_modes[] = {
    { MODE_320X240_4BP, VIDEL_VGA | VIDEL_VERTICAL | VIDEL_4BPP, 320, 240, 4 },
    { MODE_640X240_2BP, VIDEL_VGA | VIDEL_VERTICAL | VIDEL_80COL | VIDEL_2BPP, 640, 240, 2 },
    { MODE_640X480_1BP, VIDEL_VGA | VIDEL_80COL | VIDEL_1BPP, 640, 480, 1 },
    { MODE_320X240_8BP, VIDEL_VGA | VIDEL_VERTICAL | VIDEL_8BPP, 320, 240, 8 },
    { MODE_640X240_4BP, VIDEL_VGA | VIDEL_VERTICAL | VIDEL_80COL | VIDEL_4BPP, 640, 240, 4 },
    { MODE_640X480_2BP, VIDEL_VGA | VIDEL_80COL | VIDEL_2BPP, 640, 480, 2 }
};

static const struct rt68ice_video_mode *rt68ice_video_mode_from_hardware(UBYTE hardware_mode)
{
    UWORD i;

    for (i = 0; i < ARRAY_SIZE(rt68ice_video_modes); i++) {
        if (rt68ice_video_modes[i].hardware_mode == hardware_mode)
            return &rt68ice_video_modes[i];
    }

    /* FPGA modes 6 and 7 fall back to 640x480 with one bitplane. */
    return &rt68ice_video_modes[MODE_640X480_1BP];
}

static const struct rt68ice_video_mode *rt68ice_video_mode_from_videl(UWORD videl_mode)
{
    UWORD i;

    videl_mode &= VIDEL_VALID;
    for (i = 0; i < ARRAY_SIZE(rt68ice_video_modes); i++) {
        if (rt68ice_video_modes[i].videl_mode == videl_mode)
            return &rt68ice_video_modes[i];
    }

    return NULL;
}

static ULONG rt68ice_palette_to_rgb(UWORD color)
{
    ULONG red = (color >> 8) & 0x07;
    ULONG green = (color >> 4) & 0x07;
    ULONG blue = color & 0x07;

    /* Expand the ST's 3-bit components to FPGA 0x00RRGGBB. */
    return ((red * 255UL / 7UL) << 16)
         | ((green * 255UL / 7UL) << 8)
         | (blue * 255UL / 7UL);
}

static UWORD rt68ice_rgb_to_palette(ULONG color)
{
    UWORD red = ((color >> 16) & 0xff) * 7UL / 255UL;
    UWORD green = ((color >> 8) & 0xff) * 7UL / 255UL;
    UWORD blue = (color & 0xff) * 7UL / 255UL;

    return (red << 8) | (green << 4) | blue;
}

static void rt68ice_write_palette(WORD color_num)
{
    pword_vga_palette[color_num] = rt68ice_palette[color_num];
}

static void rt68ice_write_st_palette(WORD color_num, UWORD color)
{
    rt68ice_palette[color_num] = rt68ice_palette_to_rgb(color);
    rt68ice_write_palette(color_num);
}

void rt68ice_setpalette(const UWORD *palette)
{
    WORD i;

    for (i = 0; i < ARRAY_SIZE(rt68ice_default_st_palette); i++) {
        rt68ice_write_st_palette(i, palette[i] & 0x0777);
    }
}

WORD rt68ice_setcolor(WORD color_num, WORD color)
{
    WORD old_color;

    if (rt68ice_video_mode_from_hardware(current_screen_mode)->planes == 8)
        color_num &= 0x00ff;
    else
        color_num &= 0x000f;

    old_color = rt68ice_rgb_to_palette(rt68ice_palette[color_num]);
    if (color >= 0) {
        rt68ice_write_st_palette(color_num, color & 0x0777);
    }

    return old_color;
}

void rt68ice_set_vdi_color(WORD color_num, WORD red, WORD green, WORD blue)
{
    ULONG color;

    color_num &= 0x00ff;
    color = ((ULONG)red * 255UL / 1000UL) << 16;
    color |= ((ULONG)green * 255UL / 1000UL) << 8;
    color |= (ULONG)blue * 255UL / 1000UL;
    rt68ice_palette[color_num] = color;
    rt68ice_write_palette(color_num);
}

void rt68ice_get_vdi_color(WORD color_num, WORD *red, WORD *green, WORD *blue)
{
    ULONG color = rt68ice_palette[color_num & 0x00ff];

    *red = ((color >> 16) & 0xff) * 1000UL / 255UL;
    *green = ((color >> 8) & 0xff) * 1000UL / 255UL;
    *blue = (color & 0xff) * 1000UL / 255UL;
}

static void rt68ice_set_default_palette(void)
{
    WORD i;

    for (i = 0; i < ARRAY_SIZE(rt68ice_default_st_palette); i++)
        rt68ice_write_st_palette(i, rt68ice_default_st_palette[i]);
}

/* 
 * Initialize graphic palette and video mode 
 */
void rt68ice_screen_init(void)
{
    rt68ice_set_default_palette();

    /* Set VBL interrupt routine */
    VEC_LEVEL4 = rt68ice_vbl_int;

    VIDEO_IRQ_ENABLE = 0;               // Disable VGA interrupts during setup
    (void)VIDEO_IRQ_STATUS;             // Read to clear pending IRQ
    VIDEO_IRQ_ENABLE = VIDEO_IRQ_VBL;   // Disable VGA interrupts during setup

    /* Set screen mode and enable vblank interrupt */
    rt68ice_set_screen_mode(MODE_640X480_1BP);
    sshiftmod = ST_HIGH;
}

ULONG rt68ice_vram_size(void)
{
    return 75UL * 1024UL;
}

/*
 * returns the palette (number of colour choices) for the current hardware
 */
WORD rt68ice_get_palette(void)
{
    return 1 << rt68ice_video_mode_from_hardware(current_screen_mode)->planes;
}

WORD rt68ice_vgetmode(void)
{
    return rt68ice_video_mode_from_hardware(current_screen_mode)->videl_mode;
}

void rt68ice_get_current_mode_info(UWORD *planes, UWORD *hz_rez, UWORD *vt_rez)
{
    const struct rt68ice_video_mode *mode;

    mode = rt68ice_video_mode_from_hardware(current_screen_mode);
    *hz_rez = mode->width;
    *vt_rez = mode->height;
    *planes = mode->planes;
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
    if (screen_mode > MODE_640X480_2BP)
        screen_mode = MODE_640X480_1BP;

    VIDEO_CTRL = screen_mode;
    VIDEO_IRQ_ENABLE = VIDEO_IRQ_VBL;
    current_screen_mode = screen_mode;

    /* ST high resolution is monochrome: white paper, black ink. */
    if (screen_mode == MODE_640X480_1BP) {
        rt68ice_write_st_palette(0, 0x0fff);
        rt68ice_write_st_palette(1, 0x0000);
    }
}

WORD rt68ice_check_moderez(WORD moderez)
{
    const struct rt68ice_video_mode *mode;
    WORD rez;

    if (moderez < 0) {
        rez = moderez & 0x00ff;
        if (rez < ST_LOW || rez > ST_HIGH)
            return 0;
        mode = &rt68ice_video_modes[rez];
    } else {
        mode = rt68ice_video_mode_from_videl((UWORD)moderez);
        if (!mode)
            return 0;
    }

    return (mode->hardware_mode == current_screen_mode) ? 0 : moderez;
}

/* Used by Setscreen().  The FPGA mode numbers are independent of the
 * Atari XBIOS resolution indices. */
void rt68ice_setrez(WORD rez, WORD videlmode)
{
    const struct rt68ice_video_mode *mode;

    if (rez >= ST_LOW && rez <= ST_HIGH) {
        mode = &rt68ice_video_modes[rez];
        sshiftmod = rez;
    } else if (rez == FALCON_REZ) {
        mode = rt68ice_video_mode_from_videl((UWORD)videlmode);
        if (!mode)
            return;
        sshiftmod = FALCON_REZ;
    } else {
        return;
    }

    rt68ice_set_screen_mode(mode->hardware_mode);
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

    #if CONF_SERIAL_CONSOLE && !CONF_SERIAL_CONSOLE_POLLING_MODE
        /* Serial-console input must enter the IKBD queue used by Bconin(2). */
        push_ascii_ikbdiorec(c);
    #else
        push_serial_iorec(c);
    #endif
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
static UBYTE usb_last_key_mods;
static UBYTE usb_last_keys[4];
static UBYTE usb_joy_state[2];
static UBYTE usb_joy_reported[2];
static BOOL usb_joy_events_disabled;
static UBYTE usb_ikbd_command;
static UBYTE usb_ikbd_remaining;
static UWORD usb_ikbd_load;
//static BOOL  usb_keyb_is_ext;

void rt68ice_usb_init(void)
{
    
    usb_mouse_buf_index = 0;        /* Reset mouse buffer index */
    usb_keyb_is_break = FALSE;      /* Reset key */
    usb_last_key_mods = 0;
    usb_last_keys[0] = usb_last_keys[1] = 0;
    usb_last_keys[2] = usb_last_keys[3] = 0;
    usb_joy_state[0] = usb_joy_state[1] = 0;
    usb_joy_reported[0] = usb_joy_reported[1] = 0;
    usb_joy_events_disabled = FALSE;
    usb_ikbd_command = usb_ikbd_remaining = 0;
    usb_ikbd_load = 0;

    USB_IRQ_ENABLE = 0;             /* important on warm reset */
    (void)USB1_STATUS;
    (void)USB2_STATUS;              /* discard/ack any pending Host 2 report */
    (void)USB3_STATUS;
    (void)USB4_STATUS;

    /* Safe until init_acia_vecs() runs, without it mousevec is BSS and 
    therefore zero. call_mousevec() loads that zero callback and executes 
    jsr (a1) (bios/aciavecs.S:540), jumping into address 0. */
    kbdvecs.mousevec = just_rts;
    kbdvecs.joyvec = just_rts;
    
    VEC_LEVEL6 = rt68ice_usb_int;   /* Set interrupt handlers */
    USB_IRQ_ENABLE = 0x000f;        /* Enable all four USB hosts */
}

/********************************************************************/
/* Requires                                                         */
/* - Keyboard on USB port 1                                         */
/* - Mouse on USB port 2                                            */
/* - Joystick 1 on USB port 3, joystick 0 on USB port 4               */
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

    if (irq_status & 0x0004)
    {
        UWORD status = USB3_STATUS;     /* acknowledge host 3 */

        rt68ice_usb_joystick_int(1, status, USB3_GAMEPAD);
    }

    if (irq_status & 0x0008)
    {
        UWORD status = USB4_STATUS;     /* acknowledge host 4 */

        rt68ice_usb_joystick_int(0, status, USB4_GAMEPAD);
    }
}

static UBYTE rt68ice_usb_joystick_state(UWORD status, UWORD gamepad)
{
    if ((status & (USB_DEV_ERROR | USB_DEV_TYPE)) != USB_DEV_TYPE_PAD)
        return 0;

    return ((gamepad & USB_PAD_UP)    ? 0x01 : 0)
         | ((gamepad & USB_PAD_DOWN)  ? 0x02 : 0)
         | ((gamepad & USB_PAD_LEFT)  ? 0x04 : 0)
         | ((gamepad & USB_PAD_RIGHT) ? 0x08 : 0)
         | ((gamepad & USB_PAD_FIRE)  ? 0x80 : 0);
}

static void rt68ice_usb_joystick_int(UBYTE joynum, UWORD status, UWORD gamepad)
{
    UBYTE packet[3];
    UBYTE state = rt68ice_usb_joystick_state(status, gamepad);

    usb_joy_state[joynum] = state;
    if (usb_joy_events_disabled || state == usb_joy_reported[joynum])
        return;

    /* Event callbacks receive the header and both joystick states. */
    packet[0] = 0xfe + joynum;
    packet[1] = usb_joy_state[0];
    packet[2] = usb_joy_state[1];
    usb_joy_reported[joynum] = state;
    call_joyvec(packet);
}

/* Consume whole IKBD commands so parameter bytes cannot change joystick mode. */
void rt68ice_ikbd_writeb(UBYTE b)
{
    UBYTE packet[3];
    WORD old_sr;

    if (usb_ikbd_load)
    {
        usb_ikbd_load--;
        return;
    }

    if (usb_ikbd_remaining)
    {
        usb_ikbd_remaining--;
        if (usb_ikbd_remaining)
            return;
        if (usb_ikbd_command == 0x20) /* MEMORY LOAD: skip payload */
            usb_ikbd_load = b;
        if (usb_ikbd_command != 0x80 || b != 0x01)
            return;
    }
    else
    {
        usb_ikbd_command = b;
        switch (b)
        {
        case 0x80: case 0x07: case 0x17:
            usb_ikbd_remaining = 1;
            return;
        case 0x0a: case 0x0b: case 0x0c: case 0x21: case 0x22:
            usb_ikbd_remaining = 2;
            return;
        case 0x20:
            usb_ikbd_remaining = 3;
            return;
        case 0x09:
            usb_ikbd_remaining = 4;
            return;
        case 0x0e:
            usb_ikbd_remaining = 5;
            return;
        case 0x19: case 0x1b:
            usb_ikbd_remaining = 6;
            return;
        }
    }

    /* A USB interrupt must not modify a packet while joyvec consumes it. */
    old_sr = set_sr(0x2700);
    switch (usb_ikbd_command)
    {
    case 0x80: /* RESET */
        usb_joy_reported[0] = usb_joy_reported[1] = 0;
        usb_joy_events_disabled = FALSE;
        break;
    case 0x14: /* SET JOYSTICK EVENT REPORTING */
        usb_joy_events_disabled = FALSE;
        break;
    case 0x15: /* SET JOYSTICK INTERROGATION MODE */
    case 0x1a: /* DISABLE JOYSTICKS */
        usb_joy_events_disabled = TRUE;
        break;
    case 0x16: /* JOYSTICK INTERROGATE */
        /* Read current hardware state even if no report IRQ was received. */
        usb_joy_state[1] = rt68ice_usb_joystick_state(USB3_STATUS, USB3_GAMEPAD);
        usb_joy_state[0] = rt68ice_usb_joystick_state(USB4_STATUS, USB4_GAMEPAD);
        packet[0] = 0xfd;
        packet[1] = usb_joy_state[0];
        packet[2] = usb_joy_state[1];
        /* Interrogation callbacks receive only the two state bytes. */
        call_joyvec(packet + 1);
        break;
    }
    set_sr(old_sr);
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

#define IDKB_BREAK              0x80

static BOOL rt68ice_usb_key_present(const UBYTE *keys, UBYTE usage)
{
    UBYTE i;

    for (i = 0; i < 4; i++)
        if (keys[i] == usage)
            return TRUE;

    return FALSE;
}

static void rt68ice_usb_send_key(UBYTE usage, BOOL released)
{
    UBYTE scancode = usb_to_idkb_map[usage];

    if (scancode != 0) {
        if (released)
            scancode |= IDKB_BREAK;
        call_ikbdraw(scancode);
    }
}

static void process_usb_modifier(UBYTE current, UBYTE mask, UBYTE scancode)
{
    BOOL was_down = (usb_last_key_mods & mask) != 0;
    BOOL is_down = (current & mask) != 0;

    if (was_down != is_down)
        call_ikbdraw(is_down ? scancode : (scancode | IDKB_BREAK));
}

static void rt68ice_usb_key_int(void)
{
    UBYTE current_mods = (UBYTE)USB1_KEY_MODS;
    UBYTE current_keys[4];
    UBYTE i;

    current_keys[0] = (UBYTE)USB1_KEY1;
    current_keys[1] = (UBYTE)USB1_KEY2;
    current_keys[2] = (UBYTE)USB1_KEY3;
    current_keys[3] = (UBYTE)USB1_KEY4;

    /* HID 0x01 means ErrorRollOver; ignore the incomplete report. */
    for (i = 0; i < 4; i++)
        if (current_keys[i] == 0x01)
            return;

    /* Release ordinary keys which disappeared from the new report. */
    for (i = 0; i < 4; i++)
        if (usb_last_keys[i] != 0 && !rt68ice_usb_key_present(current_keys, usb_last_keys[i]))
            rt68ice_usb_send_key(usb_last_keys[i], TRUE);

    process_usb_modifier(current_mods, (1 << 1), 0x2a); /* left Shift */
    process_usb_modifier(current_mods, (1 << 5), 0x36); /* right Shift */
    process_usb_modifier(current_mods, (1 << 0) | (1 << 4), 0x1d); /* Ctrl */
    process_usb_modifier(current_mods, (1 << 2) | (1 << 6), 0x38); /* Alt */

    /* Press ordinary keys which appeared in the new report. */
    for (i = 0; i < 4; i++)
        if (current_keys[i] != 0 && !rt68ice_usb_key_present(usb_last_keys, current_keys[i]))
            rt68ice_usb_send_key(current_keys[i], FALSE);

    for (i = 0; i < 4; i++)
        usb_last_keys[i] = current_keys[i];
    
    usb_last_key_mods = current_mods;
}


#endif /* MACHINE_RT68ICE */
