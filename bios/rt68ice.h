#ifndef RT68ICE_H
#define RT68ICE_H
#ifdef MACHINE_RT68ICE

// General
extern void rt68ice_init(void);

// Serial
void rt68ice_rs232_init(void);
void rt68ice_rs232_int(void);
void rt68ice_rs232_int_c(void);
BOOL rt68ice_rs232_can_write(void);
void rt68ice_rs232_write_byte(UBYTE);
void kprintf_outc_rt68ice_rs232(int);

#endif /* MACHINE_RT68ICE */
#endif /* RT68ICE_H */