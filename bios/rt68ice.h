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
void rt68ice_init_system_timer(void);
void rt68ice_timer_int(void);
void rt68ice_timer_int_c(void);
void rt68ice_call_5ms(void);

#endif /* MACHINE_RT68ICE */
#endif /* RT68ICE_H */