/*

win.h - Platform independant interface to Windowing code

*/

#ifndef WIN_H
#define WIN_H

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "types.h"
#include "monprom.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	N_COLS_MAX  256
#define   MAX_WINS  10

#define TW_ROWS     24
#define TW_COLS     80
#define NRAM80C     2048
#define NREG80C     16

typedef struct
	{
	byte r, g, b;
	} COL;

typedef struct st_txtbuf
    {
    struct st_monram
        {
        uint8_t ch;
        uint8_t at;
        } ram[NRAM80C];

    uint8_t reg[NREG80C];
    BOOLEAN bMono;
    BOOLEAN bChanged;
    int wk;
    } TXTBUF;

typedef int (*PVALID)(int);     // Edit validator function prototype

typedef struct st_win
	{
    int iWin;
	int width, height;
	int width_scale, height_scale;
	int n_cols;
	byte *data;
	void (*keypress)(struct st_win *, int);
	void (*keyrelease)(struct st_win *, int);
    TXTBUF *tbuf;
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
	void (*keypress)(WIN *, int),
	void (*keyrelease)(WIN *, int)
	);

extern WIN *twin_create(
	int width_scale, int height_scale,
	const char *title,
	const char *display,
	const char *geometry,
	void (*keypress)(WIN *, int),
	void (*keyrelease)(WIN *, int),
    BOOLEAN bMono
	);

extern WIN *win_alloc (size_t size_win, size_t size_data);
extern void win_free (WIN *win);
extern void win_delete (WIN *win);
extern void win_colour (WIN *win, int idx, COL *clr);
extern void win_refresh (WIN *win);
extern int win_shifted_wk (int wk);
extern void win_show (WIN *win);
extern void win_show_num (int n);
extern void win_next (void);
extern void win_prev (void);
extern BOOLEAN win_active (WIN *win);
extern WIN * win_current (void);
extern void win_term (void);
extern void win_max_size (const char *display, int *pWth, int *pHgt);

/* Handle any pending events for any of our windows.
   A no-op on Windows, where we use separate threads. */
extern void win_handle_events();

TXTBUF *tbuf_create (BOOLEAN bMono);
void twin_delete (WIN *win);
void twin_show (WIN *win);
void twin_refresh (WIN *win);
void twin_draw_glyph (WIN *win, int iAddr);
void twin_csr_style (WIN *win, int iFst, int iLst);
void twin_draw_csr (WIN *win, int iAddr);
void twin_print (WIN *win, int iRow, int iCol, uint8_t attr, const char *psTxt, int nCh);
void twin_csr (WIN *win, int iRow, int iCol);
void twin_clear_rows (WIN *win, int iFirst, int iLast);
void twin_set_blink (BOOLEAN blink);
void twin_keypress (WIN *win, int wk);
void twin_keyrelease (WIN *win, int wk);
BOOLEAN twin_kbd_stat (WIN *win);
int twin_kbd_test (WIN *win);
int twin_kbd_in (WIN *win);
int twin_edit (WIN *win, int iRow, int iCol, int nWth, int iSty, int nLen, char *psText, PVALID vld);

extern int n_wins;
extern WIN *wins[MAX_WINS];
extern WIN *active_win;

#ifdef __cplusplus
}
#endif

#endif
