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
#define LED         *(volatile UWORD*)0x00F00000   // LED-mapped register base address
#define LEDS        *(volatile UWORD*)0x00F14000   // LED array mapped register base address

/* Custom registers */

/* Initialize Native Features */
extern void rt68ice_init(void) 
{
    // Debug
    LEDS = 0x1;
}

#endif /* MACHINE_RT68ICE */