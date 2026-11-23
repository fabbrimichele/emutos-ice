#!/usr/bin/env python3
"""Exercise the production reset-release routines with simulated USB reports."""

from pathlib import Path
import os
import shlex
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / "bios/rt68ice.c").read_text()


def function(signature):
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


program = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char UBYTE;
typedef unsigned short UWORD;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define USB_PORT_COUNT 4
#define USB_DEV_TYPE 3
#define USB_DEV_TYPE_KEY 1
#define USB_DEV_ERROR 0x80
static struct { UWORD status, key_modifiers, keys[4]; } hosts[4];
static unsigned reads;
static int release_sequence;

static void advance_reports(void)
{
    assert(++reads < 1000); /* Detect a broken wait without hanging the test. */
    if (release_sequence) {
        if (reads >= 4) hosts[0].key_modifiers = 0;
        if (reads >= 8) hosts[1].key_modifiers = 0;
        if (reads >= 12) hosts[3].keys[3] = 0;
    }
}
#define USB_HOSTS (advance_reports(), hosts)
''' + function("static BOOL rt68ice_reset_keys_held(void)") + "\n" + function(
    "void rt68ice_wait_reset_keys_released(void)"
) + r'''
static void clear_reports(void)
{
    unsigned p;
    memset(hosts, 0, sizeof(hosts));
    reads = 0;
    release_sequence = 0;
    for (p = 0; p < 4; p++) hosts[p].status = USB_DEV_TYPE_KEY;
}

int main(void)
{
    unsigned p, bit, slot;
    clear_reports();
    assert(!rt68ice_reset_keys_held());
    for (p = 0; p < 4; p++) {
        for (bit = 0; bit < 8; bit++) {
            clear_reports();
            hosts[p].key_modifiers = 1u << bit;
            assert(rt68ice_reset_keys_held() == !!((1u << bit) & 0x75));
        }
        for (slot = 0; slot < 4; slot++) {
            clear_reports();
            hosts[p].keys[slot] = 0x4c; /* Delete in any report position. */
            assert(rt68ice_reset_keys_held());
            hosts[p].keys[slot] = 0x01; /* Rollover is not a release. */
            assert(rt68ice_reset_keys_held());
            hosts[p].keys[slot] = 0x04; /* Unrelated key must not block. */
            assert(!rt68ice_reset_keys_held());
        }
        clear_reports();
        hosts[p].key_modifiers = 0x75;
        hosts[p].keys[0] = 0x4c;
        hosts[p].status = 0; /* Unplugging must allow restart. */
        assert(!rt68ice_reset_keys_held());
        hosts[p].status = 2; /* Ignore stale data on non-keyboard ports. */
        assert(!rt68ice_reset_keys_held());
        hosts[p].status = USB_DEV_ERROR | USB_DEV_TYPE_KEY;
        assert(!rt68ice_reset_keys_held());
    }

    clear_reports();
    hosts[0].key_modifiers = 0x01; /* Ctrl */
    hosts[1].key_modifiers = 0x24; /* Alt and right Shift */
    hosts[3].keys[3] = 0x4c;      /* Delete */
    release_sequence = 1;
    rt68ice_wait_reset_keys_released();
    assert(reads >= 12); /* Must wait for Delete, not just the modifiers. */
    release_sequence = 0;
    assert(!rt68ice_reset_keys_held());
    /* A fresh Ctrl press remains visible; there is no persistent filter. */
    hosts[2].key_modifiers = 0x10;
    assert(rt68ice_reset_keys_held());
    puts("PASS: reset chord release, four ports, modifier/key positions, "
         "rollover, disconnect and staged release");
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="rt68ice-reset-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(program)
    subprocess.run(shlex.split(os.environ.get("HOST_CC", "cc")) + [
        "-std=c99", "-Wall", "-Wextra", "-Werror", "-O2",
        str(path / "test.c"), "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True, timeout=5)
