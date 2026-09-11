//#ifdef MACHINE_RT68ICE

/******************************************************************************/
/* Interrupt 3: RS232                                                          */
/******************************************************************************/
        .globl  _rt68ice_rs232_int
_rt68ice_rs232_int:
        movem.l d0-d1/a0-a1,-(sp)       // C code does not preserve these registers
        jbsr    _rt68ice_rs232_int_c      // Delegate to C implementation
                                        // Interrupt cleared in C implementation
        movem.l (sp)+,d0-d1/a0-a1
        rte

//#endif /* MACHINE_RT68ICE */
