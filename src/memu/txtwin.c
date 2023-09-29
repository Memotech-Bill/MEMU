/* txtwin.c - A generic text window, based upon the Monitor window */

#include <string.h>
#include "win.h"
#include "kbd.h"
#include "common.h"
#include "diag.h"
#ifdef __Pico__
#include "display_pico.h"
#else
#include "monprom.h"
#endif

#define	ATTR_ULINE      0x01
#define	ATTR_BRIGHT     0x04
#define	ATTR_REVERSE    0x10
#define	ATTR_BACKGROUND 0x20
#define	ATTR_BLINK      0x40
#define	ATTR_GRAPHICS   0x80

/* These are the values that AZMON.MAC initially sets them to. */
static uint8_t reg_init[NREG80C] =
	{
	15*8-1,	    /*  0 - Horizontal total */
	TW_ROWS,	/*  1 - Horizontal displayed */
	82,		    /*  2 - Horizontal sync position */
	9,		    /*  3 - Horizontal and vertical sync widths */
	30,		    /*  4 - Vertical total */
	3,		    /*  5 - Vertical total adjust */
	TW_COLS,	/*  6 - Vertical displayed */
	27,		    /*  7 - Vertical sync position */
	0,		    /*  8 - Interlace and skew */
	9,		    /*  9 - Maximum raster address */
	0x60,	    /* 10 - Cursor start raster with blink enable (0x40) and blink speed (0x20) */
	9,		    /* 11 - Cursor end raster */
	0,		    /* 12 - Display start address (high) */
	0,		    /* 13 - Display start address (low) */
	0,		    /* 14 - Cursor address (high) */
	0,		    /* 15 - Cursor address (low) */
	};

#define	TBUF_NCLR   8
#define	TBUF_NMONO  4

static COL tbuf_clr[TBUF_NCLR] =
	{	/* normal colours */
		{ 0x00,0x00,0x00 }, /* black */
		{ 0xff,0x00,0x00 }, /* red */
		{ 0x00,0xff,0x00 }, /* green */
		{ 0xff,0xff,0x00 }, /* yellow */
		{ 0x00,0x00,0xff }, /* blue */ 
		{ 0xff,0x00,0xff }, /* magenta */
		{ 0x00,0xff,0xff }, /* cyan */
		{ 0xff,0xff,0xff }, /* white */
	};

static COL tbuf_mono[TBUF_NMONO] =
	{
		{ 0x00,0x00,0x00 }, /* black */
		{ 0x00,0x40,0x00 }, /* pale green */
		{ 0x00,0xc0,0x00 }, /* normal green */
		{ 0x00,0xff,0x00 }, /* bright green */
	};

static BOOLEAN  tw_blink = FALSE;

TXTBUF *tbuf_create (BOOLEAN bMono)
    {
    TXTBUF *tbuf = (TXTBUF *) malloc (sizeof (TXTBUF));
    // printf ("tbuf_create: tbuf = %p\n", tbuf);
    if ( tbuf == NULL ) fatal ("Unable to allocate text buffer");
    for (int i = 0; i < NRAM80C; ++i)
        {
        tbuf->ram[i].ch = ' ';
        tbuf->ram[i].at = 0x02;
        }
    memcpy (tbuf->reg, reg_init, NREG80C);
    tbuf->bMono = bMono;
    tbuf->bChanged = FALSE;
    tbuf->wk = -1;
    return tbuf;
    }

WIN *twin_create (int width_scale, int height_scale, const char *title, const char *display, const char *geometry,
    void (*keypress)(WIN *, int), void (*keyrelease)(WIN *, int), BOOLEAN bMono)
    {
    WIN *win = win_create (TW_COLS * GLYPH_WIDTH, TW_ROWS * GLYPH_HEIGHT, width_scale, height_scale,
        title, display, geometry, bMono ? tbuf_mono : tbuf_clr, bMono ? TBUF_NMONO : TBUF_NCLR,
        keypress, keyrelease);
    win->tbuf = tbuf_create (bMono);
    win_show (win);
    return win;
    }

#ifndef __Pico__
void twin_draw_glyph (WIN *win, int iAddr)
    {
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return;
    byte fg;
    byte bg;
    byte ul;
    byte *s;
    byte ch = tbuf->ram[iAddr].ch;
    byte attr = tbuf->ram[iAddr].at;
    iAddr = (iAddr - ((tbuf->reg[12] << 8) | tbuf->reg[13])) & 0x7FF;
    int iRow = iAddr / TW_COLS;
    int iCol = iAddr % TW_COLS;
    if ( iRow >= TW_ROWS ) return;
    iAddr = ( TW_COLS * GLYPH_HEIGHT * iRow + iCol ) * GLYPH_WIDTH;
    byte *d = &win->data[iAddr];
	if ( ((attr & ATTR_BLINK) != 0) && tw_blink )
		s = mon_blank_prom;
	else if ( attr & ATTR_GRAPHICS )
		s = mon_graphic_prom[ch];
	else
		s = mon_alpha_prom[ch];
    if ( tbuf->bMono )
        {
        fg = 2;
        bg = 0;
        ul = 2;
        if ( attr & ATTR_BRIGHT )
            fg = 3;
        if ( attr & ATTR_BACKGROUND )
            {
            bg = 1;
            ul = 3;
            }
        if ( attr & ATTR_REVERSE )
            {
            fg = 0;
            bg = ul;
            ul = 0;
            }
        }
    else
        {
        fg = attr & 0x07;
        bg = ( attr >> 3 ) & 0x07;
        }
    for (int j = 0; j < GLYPH_HEIGHT; ++j)
        {
        byte pix = *s;
        if ((tbuf->bMono) && (attr & ATTR_ULINE) && (j == GLYPH_HEIGHT - 1))
            {
            pix ^= 0xFF;
            fg = ul;
            }
        for (int i = 0; i < GLYPH_WIDTH; ++i)
            {
            *d = (pix & 0x80) ? fg : bg;
            ++d;
            pix <<= 1;
            }
        d += (TW_COLS - 1) * GLYPH_WIDTH;
        ++s;
        }
    tbuf->bChanged = TRUE;
    }

void twin_draw_csr (WIN *win, int iAddr)
    {
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return;
    iAddr = (iAddr - ((tbuf->reg[12] << 8) | tbuf->reg[13])) & 0x7FF;
    int iRow = iAddr / TW_COLS;
    int iCol = iAddr % TW_COLS;
    iAddr = ( TW_COLS * GLYPH_HEIGHT * iRow + iCol ) * GLYPH_WIDTH;
    byte *d = &win->data[iAddr];
    byte fg = ( tbuf->bMono ) ? 2 : 7;
    for (int j = 0; j < GLYPH_HEIGHT; ++j)
        {
        if (( j >= (tbuf->reg[10] & 0x1F) ) && ( j <= (tbuf->reg[11] & 0x1F) ))
            {
            for (int i = 0; i < GLYPH_WIDTH; ++i)
                {
                *d = fg;
                ++d;
                }
            d += (TW_COLS - 1) * GLYPH_WIDTH;
            }
        else
            {
            d += TW_COLS * GLYPH_WIDTH;
            }
        }
    tbuf->bChanged = TRUE;
    }
#endif

void twin_csr_style (WIN *win, int iFst, int iLst)
    {
    TXTBUF *tbuf = win->tbuf;
    tbuf->reg[10] = iFst;
    tbuf->reg[11] = iLst;
    }

void twin_print (WIN *win, int iRow, int iCol, uint8_t attr, const char *psTxt, int nCh)
    {
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return;
    int i;
    int iAddr = TW_COLS * iRow + iCol;
    if ( nCh < 0 ) nCh = (int) strlen (psTxt);
    else if ( nCh == 0 ) nCh = TW_COLS - iCol;
    for ( i = 0; i < nCh; ++i )
        {
        char ch = *psTxt;
        if ( ch == '\0' ) ch = ' ';
        tbuf->ram[iAddr].ch = ch;
        tbuf->ram[iAddr].at = attr;
#ifndef __Pico__
        twin_draw_glyph (win, iAddr);
#endif
        if ( *psTxt ) ++psTxt;
        ++iAddr;
        }
    tbuf->bChanged = TRUE;
    }

//  Dispays a cursor (inverted video)
void twin_csr (WIN *win, int iRow, int iCol)
    {
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return;
    int iAddr = TW_COLS * iRow + iCol;
    tbuf->reg[14] = iAddr >> 8;
    tbuf->reg[15] = iAddr & 0xFF;
#ifndef __Pico__
    twin_draw_csr (win, iAddr);
#endif
    }

//  Clears rows of text on the configuration screen - for ( row = first; row < last; ++row )
void twin_clear_rows (WIN *win, int iFirst, int iLast)
    {
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return;
    for (int i = TW_COLS * iFirst; i < TW_COLS * iLast; ++i)
        {
        tbuf->ram[i].ch = ' ';
        tbuf->ram[i].at = 0x02;
#ifndef __Pico__
        twin_draw_glyph (win, i);
#endif
        }
    tbuf->bChanged = TRUE;
    }

void twin_set_blink (BOOLEAN blink)
    {
    tw_blink = blink;
    }

static int kpad_map[] = {'4', '6', '8', '2', '9', '3', '7', '1', '+', '-', '*', '/', '\r', '5'};

void twin_keypress (WIN *win, int wk)
    {
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return;
    kbd_mod_keypress (wk);
    if (( wk >= WK_KP_Left ) && ( wk <= WK_KP_Middle ) && ( kbd_mods & (MKY_LSHIFT | MKY_RSHIFT | MKY_SHFTLK | MKY_NUMLK) ))
        tbuf->wk = kpad_map[wk - WK_KP_Left];
    else if ( kbd_mods & ( MKY_LSHIFT | MKY_RSHIFT | MKY_SHFTLK ) ) tbuf->wk = kbd_shifted_wk (wk);
    else if ( ( kbd_mods & MKY_CAPSLK ) && ( wk >= 'a' ) && ( wk <= 'z' ) ) tbuf->wk = wk & 0x5F;
    else tbuf->wk =  wk;
    if ( ( kbd_mods & ( MKY_LCTRL | MKY_RCTRL ) ) && ( tbuf->wk < 0x100 ) ) tbuf->wk &= 0x1F;
    }

void twin_keyrelease (WIN *win, int wk)
    {
    kbd_mod_keyrelease (wk);
    }

//  Wait for a key press
#ifdef   ALT_KEYIN
extern int ALT_KEYIN (void);
#endif

void twin_refresh (WIN *win)
    {
    if ( win->tbuf->bChanged ) win_refresh (win);
    win->tbuf->bChanged = FALSE;
    }

BOOLEAN twin_kbd_stat (WIN *win)
    {
    twin_refresh (win);
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return FALSE;
    win_handle_events ();
#ifdef   ALT_KEYIN
    if (tbuf->wk < 0 ) tbuf->wk =  ALT_KEYIN ();
#endif
    diag_message (DIAG_KBD_WIN_KEY, "kbd_stat: tbuf->wk = %d", tbuf->wk);
    return ( tbuf->wk > 0 ) ? TRUE : FALSE;
    }

int twin_kbd_test (WIN *win)
    {
    twin_refresh (win);
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return -1;
    win_handle_events ();
#ifdef   ALT_KEYIN
    if (tbuf->wk < 0 ) tbuf->wk =  ALT_KEYIN ();
#endif
    int wk = tbuf->wk;
    tbuf->wk = -1;
    diag_message (DIAG_KBD_WIN_KEY, "kbd_test = %d", wk);
    return wk;
    }

int twin_kbd_in (WIN *win)
    {
    twin_refresh (win);
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return -1;
    // diag_message (DIAG_INIT, "Entered kbd_key");
    while ( tbuf->wk < 0 )
        {
        // diag_message (DIAG_INIT, "Call kbd_periodic");
        win_handle_events ();
#ifdef   ALT_KEYIN
        if (tbuf->wk >= 0 )  break;
        tbuf->wk =  ALT_KEYIN ();
#endif
        }
    int wk = tbuf->wk;
    tbuf->wk = -1;
    diag_message (DIAG_KBD_WIN_KEY, "kbd_in = %d", wk);
    return   wk;
    }

//  Edit a line of text - exit on return or escape
int twin_edit (WIN *win, int iRow, int iCol, int nWth, int iSty, int nLen, char *psText, PVALID vld)
    {
    TXTBUF *tbuf = win->tbuf;
    if ( tbuf == NULL ) return 0;
    int  nCh    =  (int) strlen (psText);
    int  iCsr   =  0;
    int  iScl   =  0;
    int  wk;
    twin_csr (win, iRow, iCol);
    twin_csr_style (win, 8, 9);
    while (TRUE)
        {
        if ( ( iCsr - iScl ) >= nWth )   iScl  =  iCsr - nWth + 1;
        else if ( iCsr < iScl )          iScl  =  iCsr;
        twin_print (win, iRow, iCol, iSty, &psText[iScl], nWth);
        twin_csr (win, iRow, iCol + iCsr - iScl);
        wk =  twin_kbd_in (win);
        switch (wk)
            {
            case  WK_Return:
            case  WK_Escape:
            {
            twin_csr_style (win, 0x20, 9);
            return   wk;
            }
            case  WK_Left:
            {
            if ( iCsr > 0 )   --iCsr;
            break;
            }
            case  WK_Home:
            {
            iCsr  =  0;
            break;
            }
            case  WK_End:
            {
            iCsr  =  nCh;
            break;
            }
            case  WK_Right:
            {
            if ( iCsr < nCh ) ++iCsr;
            break;
            }
            case  WK_Delete:
            {
            if ( iCsr < nCh )
                {
                strcpy (&psText[iCsr], &psText[iCsr+1]);
                --nCh;
                }
            break;
            }
            case  WK_BackSpace:
            {
            if ( iCsr > 0 )
                {
                --iCsr;
                strcpy (&psText[iCsr], &psText[iCsr+1]);
                --nCh;
                }
            break;
            }
            default:
            {
            if ( vld != NULL )  wk = vld (wk);
            if ( ( nCh < nLen - 1 ) && ( wk >= ' ' ) && ( wk <= '~' ) )
                {
                memmove (&psText[iCsr+1], &psText[iCsr], nCh + 1 - iCsr);
                psText[iCsr]   =  wk;
                ++nCh;
                ++iCsr;
                }
            break;
            }
            }
        }
    }
