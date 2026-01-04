/*

vid.h - Video chip and TV

*/

#ifndef VID_H
#define	VID_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	VIDEMU_WIN              0x01
#define	VIDEMU_WIN_HW_PALETTE   0x02
#define VIDEMU_WIN_MAX          0x80
#define	VID_MEMORY_SIZE         0x4000

extern void vid_reset(void);

extern void vid_setup_timing_check(unsigned t_2us, unsigned t_8us, unsigned t_blank);

extern void vid_out1(byte val, unsigned long long elapsed);
extern void vid_out2(byte val, unsigned long long elapsed);
extern byte vid_in1(unsigned long long elapsed);
extern byte vid_in2(unsigned long long elapsed);

extern void vid_refresh(unsigned long long elapsed);
extern void vid_refresh_vdeb(void);

extern BOOLEAN vid_int_pending(void);
extern void vid_set_int (void);
extern void vid_clear_int(void);

extern byte vid_vram_read(word addr);
extern void vid_vram_write(word addr, byte b);
extern byte vid_reg_read(int reg);
extern void vid_reg_write(int reg, byte b);
extern byte vid_status_read(void);

extern void vid_init(int emu, int width_scale, int height_scale);
extern void vid_term(void);
extern void vid_max_scale (int *pxscl, int *pyscl);

extern void vid_set_title (const char *title);
extern const char * vid_get_title (void);
extern void vid_set_display (const char *display);
extern const char * vid_get_display (void);

extern void vid_show (void);

#endif
