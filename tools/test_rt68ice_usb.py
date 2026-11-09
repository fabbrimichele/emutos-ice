#!/usr/bin/env python3
"""Exercise the production RT68ICE USB handlers with host-side fake registers."""

from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parent.parent
source = (ROOT / "bios/rt68ice.c").read_text()
prototypes = source[source.index("static void rt68ice_usb_mouse_int"):
                    source.index("/* Custom registers */")]
registers = source[source.index("/* Each host occupies"):
                   source.index("/******************************************************************************/", source.index("#define USB_PAD_FIRE"))]
registers = registers.replace(
    "#define USB_HOSTS ((volatile struct rt68ice_usb_host *)0x00f18010)",
    "static volatile struct rt68ice_usb_host USB_HOSTS[USB_PORT_COUNT];")
handlers = source[source.index("static UBYTE usb_mouse_buf_index;"):
                  source.rindex("#endif /* MACHINE_RT68ICE */")]

HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UBYTE;
typedef int8_t SBYTE;
typedef uint16_t UWORD;
typedef int16_t WORD;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
static UWORD USB_IRQ_STATUS, USB_IRQ_ENABLE, LEDS;
static void (*VEC_LEVEL6)(void);
static struct { void (*mousevec)(void); void (*joyvec)(void); } kbdvecs;
static UBYTE key_events[128], mouse_packet[3], joy_packet[3];
static unsigned key_count, mouse_count, joy_count;
static void just_rts(void) {}
static void rt68ice_usb_int(void) {}
static WORD set_sr(WORD sr) { return sr; }
static void call_ikbdraw(UBYTE code) {
    assert(key_count < sizeof(key_events));
    key_events[key_count++] = code;
}
static void call_mousevec(const void *packet) {
    memcpy(mouse_packet, packet, 3); mouse_count++;
}
static void call_joyvec(const UBYTE *packet) {
    /* Interrogation has two data bytes; event packets have a header. */
    memcpy(joy_packet, packet, packet[0] >= 0xfe ? 3 : 2); joy_count++;
}
'''

TESTS = r'''
static void reset(void) {
    memset((void *)USB_HOSTS, 0, sizeof(USB_HOSTS));
    USB_IRQ_STATUS = 0;
    rt68ice_usb_init();
    key_count = mouse_count = joy_count = 0;
}
static void report(unsigned port, UWORD type) {
    USB_HOSTS[port].status = type;
    USB_IRQ_STATUS = 1 << port;
    rt68ice_usb_int_c();
    USB_IRQ_STATUS = 0;
}
int main(void) {
    unsigned port, other;
    assert(sizeof(struct rt68ice_usb_host) == 32);
    assert(offsetof(struct rt68ice_usb_host, mouse_buttons) == 2);
    assert(offsetof(struct rt68ice_usb_host, mouse_dx) == 4);
    assert(offsetof(struct rt68ice_usb_host, mouse_dy) == 6);
    assert(offsetof(struct rt68ice_usb_host, gamepad) == 8);
    assert(offsetof(struct rt68ice_usb_host, key_modifiers) == 10);
    assert(offsetof(struct rt68ice_usb_host, keys) == 12);
    for (port = 0; port < USB_PORT_COUNT; port++) {
        reset();
        USB_HOSTS[port].keys[0] = 4; /* A */
        report(port, USB_DEV_TYPE_KEY);
        assert(key_count == 1 && key_events[0] == 0x1e);
        USB_HOSTS[port].keys[0] = 0;
        report(port, USB_DEV_TYPE_KEY);
        assert(key_count == 2 && key_events[1] == 0x9e);
        USB_HOSTS[port].mouse_buttons = 1;
        USB_HOSTS[port].mouse_dx = (UWORD)-3;
        USB_HOSTS[port].mouse_dy = 7;
        report(port, USB_DEV_TYPE_MOUSE);
        assert(mouse_count == 1 && mouse_packet[0] == 0xfa);
        assert(mouse_packet[1] == (UBYTE)-3 && mouse_packet[2] == 7);
        reset();
        USB_HOSTS[port].gamepad = USB_PAD_UP | USB_PAD_FIRE;
        report(port, USB_DEV_TYPE_PAD);
        assert(joy_count == 1 && joy_packet[0] == 0xff);
        assert(joy_packet[1] == 0 && joy_packet[2] == 0x81);
    }
    /* Every keyboard/mouse arrangement, including swapped ports 1 and 2. */
    for (port = 0; port < USB_PORT_COUNT; port++) {
        for (other = 0; other < USB_PORT_COUNT; other++) {
            if (port == other) continue;
            reset();
            USB_HOSTS[port].status = USB_DEV_TYPE_KEY;
            USB_HOSTS[port].keys[0] = 4;
            USB_HOSTS[other].status = USB_DEV_TYPE_MOUSE;
            USB_HOSTS[other].mouse_dx = 1;
            USB_IRQ_STATUS = (1 << port) | (1 << other);
            rt68ice_usb_int_c();
            assert(key_count == 1 && mouse_count == 1);
        }
    }
    reset();
    USB_HOSTS[0].keys[0] = USB_HOSTS[3].keys[0] = 4;
    USB_HOSTS[0].key_modifiers = USB_HOSTS[3].key_modifiers = 2;
    report(0, USB_DEV_TYPE_KEY);
    report(3, USB_DEV_TYPE_KEY);
    assert(key_count == 2 && key_events[0] == 0x2a && key_events[1] == 0x1e);
    USB_HOSTS[0].keys[0] = USB_HOSTS[0].key_modifiers = 0;
    report(0, USB_DEV_TYPE_KEY);
    assert(key_count == 2); /* Other keyboard still holds A and Shift. */
    USB_HOSTS[3].keys[0] = 1; /* Rollover must preserve previous state. */
    USB_HOSTS[3].key_modifiers = 0;
    report(3, USB_DEV_TYPE_KEY);
    assert(key_count == 2);
    USB_HOSTS[3].keys[0] = 0;
    report(3, USB_DEV_TYPE_KEY);
    assert(key_count == 4 && key_events[2] == 0x9e && key_events[3] == 0xaa);
    reset();
    USB_HOSTS[0].mouse_buttons = USB_HOSTS[2].mouse_buttons = 1;
    report(0, USB_DEV_TYPE_MOUSE);
    report(2, USB_DEV_TYPE_MOUSE);
    USB_HOSTS[0].mouse_buttons = 0;
    report(0, USB_DEV_TYPE_MOUSE);
    assert(mouse_packet[0] == 0xfa);
    USB_HOSTS[2].mouse_buttons = 0;
    report(2, USB_DEV_TYPE_MOUSE);
    assert(mouse_packet[0] == 0xf8);
    /* Interrogating gamepads must consume pending keyboard reports as well. */
    reset();
    rt68ice_ikbd_writeb(0x15);
    USB_HOSTS[1].status = USB_DEV_TYPE_KEY;
    USB_HOSTS[1].keys[0] = 4;
    USB_HOSTS[3].status = USB_DEV_TYPE_PAD;
    USB_HOSTS[3].gamepad = USB_PAD_LEFT;
    USB_IRQ_STATUS = (1 << 1) | (1 << 3);
    rt68ice_ikbd_writeb(0x16);
    assert(key_count == 1 && key_events[0] == 0x1e);
    assert(joy_count == 1 && joy_packet[0] == 0 && joy_packet[1] == 4);
    /* All pairs map in port order, independently of report arrival order. */
    for (port = 0; port < USB_PORT_COUNT; port++) {
        for (other = port + 1; other < USB_PORT_COUNT; other++) {
            reset();
            USB_HOSTS[other].gamepad = USB_PAD_RIGHT;
            report(other, USB_DEV_TYPE_PAD);
            USB_HOSTS[port].gamepad = USB_PAD_UP;
            report(port, USB_DEV_TYPE_PAD);
            assert(usb_joy_state[1] == 1 && usb_joy_state[0] == 8);
            rt68ice_ikbd_writeb(0x15); /* Interrogation mode */
            joy_count = 0;
            USB_HOSTS[other].gamepad = USB_PAD_DOWN;
            report(other, USB_DEV_TYPE_PAD);
            assert(joy_count == 0);
            rt68ice_ikbd_writeb(0x16);
            assert(joy_count == 1 && joy_packet[0] == 2 && joy_packet[1] == 1);
            rt68ice_ikbd_writeb(0x14);
            report(other, USB_DEV_TYPE_PAD);
            assert(joy_count == 2 && joy_packet[0] == 0xfe);
        }
    }
    reset();
    for (port = 0; port < USB_PORT_COUNT; port++) {
        USB_HOSTS[port].gamepad = USB_PAD_UP;
        report(port, USB_DEV_TYPE_PAD);
    }
    assert(usb_joy_state[0] == 1 && usb_joy_state[1] == 1);
    USB_HOSTS[3].gamepad = USB_PAD_DOWN;
    joy_count = 0;
    report(3, USB_DEV_TYPE_PAD);
    assert(joy_count == 0); /* Only two Atari joysticks are available. */
    reset();
    USB_HOSTS[0].keys[0] = 4;
    report(0, USB_DEV_TYPE_KEY | USB_DEV_ERROR);
    USB_HOSTS[1].mouse_dx = 1;
    report(1, USB_DEV_TYPE_MOUSE | USB_DEV_ERROR);
    USB_HOSTS[2].gamepad = USB_PAD_UP;
    report(2, USB_DEV_TYPE_PAD | USB_DEV_ERROR);
    assert(key_count == 0 && mouse_count == 0 && joy_count == 0);
    puts("RT68ICE USB routing tests passed");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="rt68ice-usb-") as directory:
    test_c = Path(directory) / "usb_test.c"
    executable = Path(directory) / "usb_test"
    test_c.write_text(HARNESS + prototypes + registers + handlers + TESTS)
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-g", str(test_c),
                    "-o", str(executable)], check=True)
    # LeakSanitizer cannot run under the sandbox's ptrace supervision.
    subprocess.run([str(executable)], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
