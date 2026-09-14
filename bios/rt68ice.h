#ifndef RT68ICE_H
#define RT68ICE_H
#ifdef MACHINE_RT68ICE

// General
extern void rt68ice_init(void);

// Screen
void rt68f_screen_init(void);
ULONG rt68f_vram_size(void);
WORD rt68f_get_palette(void);
void rt68f_get_current_mode_info(UWORD *planes, UWORD *hz_rez, UWORD *vt_rez);
void rt68f_setrez(WORD rez, WORD videlmode);
WORD rt68f_check_moderez(WORD moderez);
void rt68f_set_screen_mode(UBYTE screen_mode);
void rt68f_setphys(const UBYTE *addr);
const UBYTE *rt68f_physbase(void);
void rt68f_vbl_int(void);

// Serial
void rt68ice_rs232_init(void);
void rt68ice_rs232_int(void);
void rt68ice_rs232_int_c(void);
BOOL rt68ice_rs232_can_write(void);
void rt68ice_rs232_write_byte(UBYTE);
void kprintf_outc_rt68ice_rs232(int);

// Timer
void rt68ice_init_system_timer(void);
void rt68ice_timer_int(void);
void rt68ice_timer_int_c(void);
void rt68ice_call_5ms(void);

// USB
void rt68f_usb_init(void);
void rt68f_usb_int(void);
void rt68f_usb_int_c(void);

#endif /* MACHINE_RT68ICE */
#endif /* RT68ICE_H */