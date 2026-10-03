//#ifdef MACHINE_RT68ICE

/******************************************************************************/
/* Interrupt 6: USB (Keyboard, mouse and gamepads                             */
/******************************************************************************/
        .globl  _rt68ice_usb_int
_rt68ice_usb_int:
        movem.l d0-d1/a0-a1,-(sp)       // C code does not preserve these registers
        // HW interrupt cleared in C after identifying the triggering device (mouse, keyboard or gamepad)
        jbsr    _rt68ice_usb_int_c        // Delegate to C implementation
        movem.l (sp)+,d0-d1/a0-a1
        rte

/******************************************************************************/
/* Interrupt 5: 200 Hz timer                                                  */
/******************************************************************************/
// Interrupt handler
        .globl  _rt68ice_timer_int
_rt68ice_timer_int:
        // AES timer callbacks switch to a small private tick stack beside
        // the mouse stack. A level-6 USB interrupt here can overwrite its
        // own saved registers when a mouse callback reuses that mouse stack.
        // Defer USB until int_timerc returns with RTE, which restores the
        // interrupted SR (including its original interrupt mask).
        move.w  #0x2600,sr
        // TODO: use TIMER_STATUS
        tst.w   0x00f1c002              // Clear FPGA timer IRQ (if a read/test clears it without clobbering registers)
        move.l  _vector_5ms,-(sp)   
        rts                             // Tail-call jump to int_timerc

/******************************************************************************/
/* Interrupt 4: VBL                                                           */
/******************************************************************************/
        .globl  _rt68ice_vbl_int
_rt68ice_vbl_int:
        movem.l d0-d1/a0-a1,-(sp)       // C code does not preserve these registers
        // TODO: use VIDEO_IRQ_STATUS
        tst.w   0x00f0c002              // Read status to acknowledge the VBL
        jbsr    _int_vbl                // Call the standard VBL handler
        movem.l (sp)+,d0-d1/a0-a1
        rte

/******************************************************************************/
/* Interrupt 3: RS232                                                         */
/******************************************************************************/
        .globl  _rt68ice_rs232_int
_rt68ice_rs232_int:
        movem.l d0-d1/a0-a1,-(sp)       // C code does not preserve these registers
        jbsr    _rt68ice_rs232_int_c    // Delegate to C implementation
                                        // Interrupt cleared in C implementation
        movem.l (sp)+,d0-d1/a0-a1
        rte

//#endif /* MACHINE_RT68ICE */
