//#ifdef MACHINE_RT68ICE

/******************************************************************************/
/* Interrupt 6: Keyboard & Mouse                                              */
/******************************************************************************/
        .globl  _rt68f_usb_int
_rt68f_usb_int:
        movem.l d0-d1/a0-a1,-(sp)       // C code does not preserve these registers
        // HW interrupt cleared in C after identifying the triggering device (mouse, keyboard or gamepad)
        jbsr    _rt68f_usb_int_c        // Delegate to C implementation
        movem.l (sp)+,d0-d1/a0-a1
        rte

/******************************************************************************/
/* Interrupt 5: 200 Hz timer                                                  */
/******************************************************************************/
// Interrupt handler
        .globl  _rt68ice_timer_int
_rt68ice_timer_int:
        // TODO: use TIMER_STATUS
        tst.w   0x00f1c002              // Clear FPGA timer IRQ (if a read/test clears it without clobbering registers)
        move.l  _vector_5ms,-(sp)   
        rts                             // Tail-call jump to int_timerc

/******************************************************************************/
/* Interrupt 4: VBL                                                           */
/******************************************************************************/
        .globl  _rt68f_vbl_int
_rt68f_vbl_int:
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
