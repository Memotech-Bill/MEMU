/*

kbd.h - Keyboard

*/

#ifndef KDB_H
#define	KDB_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define KBDEMU_REMAP   0x01
#define KBDEMU_ADDON   0x02
#define KBDEMU_COUNTRY 0x0c

typedef void (*KeyHandler)(int);
extern void kbd_set_handlers (KeyHandler press, KeyHandler release);

extern void kbd_win_keypress(int k);
extern void kbd_win_keyrelease(int k);

extern BOOLEAN kbd_find_grid(const char **p, int *row, int *bitpos, BOOLEAN *shifted);

#ifdef HAVE_JOY
extern void kbd_grid_press(int row, int bitpos);
extern void kbd_grid_release(int row, int bitpos);
#endif

extern void kbd_out5(byte val);
extern byte kbd_in5(void);
extern byte kbd_in6(void);

extern void kbd_apply_remap(void);
extern void kbd_apply_unmap(void);

extern void kbd_add_events(const char *p);
extern void kbd_add_events_file(const char *fn);
extern void kbd_add_events_done(void);
extern void kbd_periodic(void);

extern void kbd_init(int emu);
extern int kbd_get_emu (void);
extern void kbd_term(void);

extern BOOLEAN kbd_diag;

#endif
