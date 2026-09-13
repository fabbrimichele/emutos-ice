//#ifdef MACHINE_RT68ICE

/******************************************************************************/
/* Interrupt 5: 200 Hz timer                                                  */
/******************************************************************************/
// Interrupt handler
        .globl  _rt68f_timer_int
_rt68f_timer_int:
        // TODO: use TIMER_STATUS
        tst.w   0x00f1c002              // Clear FPGA timer IRQ (if a read/test clears it without clobbering registers)
        move.l  _vector_5ms,-(sp)   
        rts                             // Tail-call jump to int_timerc

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
