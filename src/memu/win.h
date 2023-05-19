/*

win.h - Platform independant interface to Windowing code

*/

#ifndef WIN_H
#define WIN_H

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define MKY_LSHIFT   0x01
#define MKY_RSHIFT   0x02
#define MKY_LCTRL    0x04
#define MKY_RCTRL    0x08
#define MKY_LALT     0x10
#define MKY_RALT     0x20
#define MKY_CAPSLK   0x40

typedef struct
	{
	byte r, g, b;
	} COL;

#define	N_COLS_MAX 64

typedef struct
	{
	int width, height;
	int width_scale, height_scale;
	int n_cols;
	byte *data;
	void (*keypress)(int);
	void (*keyrelease)(int);
	/* Private window data here */
	} WIN;

#ifdef __cplusplus
extern "C"
    {
#endif

extern WIN *win_create(
	int width, int height,
	int width_scale, int height_scale,
	const char *title,
	const char *display,
	const char *geometry,
	COL *col, int n_cols,
	void (*keypress)(int),
	void (*keyrelease)(int)
	);

extern void win_term (void);    
extern void win_delete (WIN *win);
extern void win_refresh (WIN *win);
extern int win_shifted_wk (int wk);
extern void win_show (WIN *win);
extern BOOLEAN win_active (WIN *win);
extern WIN * win_current (void);
extern int win_mod_keys (void);
extern void win_kbd_leds (BOOLEAN bCaps, BOOLEAN bNum, BOOLEAN bScroll);
#ifdef WIN32
extern void win_leds_state (BOOLEAN *bCaps, BOOLEAN *bNum, BOOLEAN *bScroll);
#endif
extern void win_max_size (const char *display, int *pWth, int *pHgt);

/* Handle any pending events for any of our windows.
   A no-op on Windows, where we use separate threads. */
extern void win_handle_events();

#ifdef __cplusplus
    }
#endif

#endif
