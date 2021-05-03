/*

mon.h - 80 column card and its monitor

*/

#ifndef MON_H
#define	MON_H

#include "win.h"

#define	MONEMU_WIN           0x01
#define	MONEMU_WIN_MONO      0x02
#define	MONEMU_TH            0x04
#define	MONEMU_CONSOLE       0x08
#define	MONEMU_CONSOLE_NOKEY 0x10
#define	MONEMU_IGNORE_INIT   0x20

extern void mon_out30(byte value);
extern void mon_out31(byte value);
extern void mon_out32(byte value);
extern void mon_out33(byte value);
extern void mon_out38(byte value);
extern void mon_out39(byte value);
extern byte mon_in30(void);
extern byte mon_in32(void);
extern byte mon_in33(void);
extern byte mon_in38(void);
extern byte mon_in39(void);

extern void mon_refresh(void);
extern void mon_refresh_blink(void);
extern void mon_refresh_vdeb (void);
extern void mon_write(char c);

extern void mon_kbd_win_keypress(int wk);
extern void mon_kbd_win_keyrelease(int wk);

extern int mon_kbd_read(void);
extern int mon_kbd_read_non_wait(void);
extern BOOLEAN mon_kbd_status(void);

extern void mon_init(int emu, int width_scale, int height_scale);
extern void mon_show (void);
extern void mon_term(void);

extern void mon_set_title (const char *title);
extern const char * mon_get_title (void);
extern void mon_set_display (const char *display);
extern const char * mon_get_display (void);
extern WIN * mon_getwin (void);

#endif
