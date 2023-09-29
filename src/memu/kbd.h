/*

kbd.h - Keyboard

*/

#ifndef KDB_H
#define	KDB_H

#include "win.h"

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	WK_BackSpace     '\b'
#define	WK_Tab           '\t'
#define	WK_Linefeed      '\n'
#define	WK_Return        '\r'
#define	WK_Escape          27
#define	WK_Left         0x100
#define	WK_Right        0x101
#define	WK_Up           0x102
#define	WK_Down         0x103
#define	WK_Page_Up      0x104
#define	WK_Page_Down    0x105
#define	WK_Home         0x106
#define	WK_End          0x107
#define	WK_Insert       0x108
#define	WK_Delete       0x109
#define	WK_Pause        0x10a
#define	WK_Scroll_Lock  0x10b
#define	WK_Sys_Req      0x10c
#define	WK_Shift_L      0x10d
#define	WK_Shift_R      0x10e
#define	WK_Control_L    0x10f
#define	WK_Control_R    0x110
#define	WK_Caps_Lock    0x111
#define	WK_Shift_Lock   0x112
#define	WK_Num_Lock     0x113
#define WK_F1           0x120
#define WK_F2           0x121
#define WK_F3           0x122
#define WK_F4           0x123
#define WK_F5           0x124
#define WK_F6           0x125
#define WK_F7           0x126
#define WK_F8           0x127
#define WK_F9           0x128
#define WK_F10          0x129
#define WK_F11          0x12a
#define WK_F12          0x12b
#define	WK_KP_Left      0x130
#define	WK_KP_Right     0x131
#define	WK_KP_Up        0x132
#define	WK_KP_Down      0x133
#define	WK_KP_Page_Up   0x134
#define	WK_KP_Page_Down 0x135
#define	WK_KP_Home      0x136
#define	WK_KP_End       0x137
#define	WK_KP_Add       0x138
#define	WK_KP_Subtract  0x139
#define	WK_KP_Multiply  0x13a
#define	WK_KP_Divide    0x13b
#define	WK_KP_Enter     0x13c
#define	WK_KP_Middle    0x13d
#define	WK_PC_Windows_L 0x13e
#define	WK_PC_Windows_R 0x13f
#define	WK_PC_Alt_L     0x140
#define	WK_PC_Alt_R     0x141
#define	WK_PC_Menu      0x142
#define	WK_Mac_Cmd_L    0x143
#define	WK_Mac_Cmd_R    0x144
#define	WK_Mac_Alt      0x145
#define	WK_Menu         0x150

#define MKY_LSHIFT      0x001
#define MKY_RSHIFT      0x002
#define MKY_LCTRL       0x004
#define MKY_RCTRL       0x008
#define MKY_LALT        0x010
#define MKY_RALT        0x020
#define MKY_NUMLK       0x040
#define MKY_CAPSLK      0x080
#define MKY_SCRLLK      0x100
#define MKY_SHFTLK      0x200

#define KBDEMU_REMAP    0x01
#define KBDEMU_ADDON    0x02
#define KBDEMU_COUNTRY  0x0c

typedef void (*KeyHandler)(int);
extern void kbd_set_handlers (KeyHandler press, KeyHandler release);

extern void kbd_mod_keypress(int k);
extern void kbd_mod_keyrelease(int k);
extern void kbd_win_keypress(WIN *win, int k);
extern void kbd_win_keyrelease(WIN *win, int k);
extern int kbd_shifted_wk(int wk);

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
extern void kbd_chk_leds (int *mods);

extern char * ListModifiers (void);

extern BOOLEAN kbd_diag;
extern int kbd_mods;

#endif
