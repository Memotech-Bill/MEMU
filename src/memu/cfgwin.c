/* cfgwin.c - Standard versions of the config window low level routines */

#include <string.h>
#include "types.h"
#include "win.h"
#include "kbd.h"
#include "diag.h"
#include "config.h"
#include "monprom.h"
#include "memu.h"

#define CLR_BACKGROUND      0
#define CLR_HIGHBACK        1
#define CLR_NORMAL          2
#define CLR_HIGHLIGHT       3
#define CLR_DISABLED        4
#define CLR_HELP            5
#define CLR_COUNT           6

static WIN *cfg_win  =  NULL;
static COL cfg_clr[CLR_COUNT] = {
    {   0,   0,   0 },   /* Backgound */
    {  64,  64,  64 },   /* Highlighted background */
    {   0, 255, 255 },   /* Normal text */
    { 255, 255,   0 },   /* Highlighted text */
    {   0, 127, 127 },   /* Disabled text */
    {   0, 255,   0 }    /* Help text */
    };

static struct st_style
    {
    int fg;
    int bg;
    } style[] = {
        { CLR_NORMAL   , CLR_BACKGROUND },
        { CLR_HIGHLIGHT, CLR_HIGHBACK },
        { CLR_DISABLED , CLR_BACKGROUND },
        { CLR_HELP     , CLR_BACKGROUND }
    };

//  Display text on config screen - Uses the monitor ROM font
void cfg_print (int iRow, int iCol, int iSty, const char *psTxt, int nCh)
    {
    BOOLEAN  bEOT;
    int  iFG =  style[iSty].fg;
    int  iBG =  style[iSty].bg;
    byte *pby   =  cfg_win->data + GLYPH_HEIGHT * cfg_win->width * iRow + GLYPH_WIDTH * iCol;
    int  ch;
    byte by;
    int  iScan, iPix, iCh;
    if ( nCh <= 0 )   nCh   =  strlen (psTxt);

    for ( iScan = 0; iScan < GLYPH_HEIGHT; ++iScan )
        {
        bEOT  =  FALSE;
        for ( iCh = 0; iCh < nCh; ++iCh )
            {
            if ( ! bEOT )
                {
                ch =  psTxt[iCh];
                if ( ch == '\0' )
                    {
                    bEOT  =  TRUE;
                    ch =  ' ';
                    }
                else if ( ch < 0 )   ch =  '?';
                }
            else  ch =  ' ';
            by =  mon_alpha_prom[ch][iScan];
            for ( iPix = 0; iPix < GLYPH_WIDTH; ++iPix )
                {
                *pby  =  ( by & 0x80 ) ? iFG : iBG;
                ++pby;
                by = by << 1;
                }
            }
        pby   += cfg_win->width - GLYPH_WIDTH * nCh;
        }
    }

//  Dispays a cursor (inverted video)
void cfg_csr (int iRow, int iCol, int iSty)
    {
    int  iFG =  style[iSty].fg;
    int  iBG =  style[iSty].bg;
    int  iPix;
    byte *pby   =  cfg_win->data + ( GLYPH_HEIGHT * ( iRow + 1 ) - 1 ) * cfg_win->width + GLYPH_WIDTH * iCol;
    for ( iPix = 0; iPix < GLYPH_WIDTH; ++iPix )
        {
        if ( *pby == iFG )   *pby  =  iBG;
        else                 *pby  =  iFG;
        ++pby;
        }
    }

//  Clears rows of text on the configuration screen - for ( row = first; row < last; ++row )
void cfg_clear_rows (int iFirst, int iLast)
    {
    memset (cfg_win->data + GLYPH_HEIGHT * cfg_win->width * iFirst,
        CLR_BACKGROUND,
        GLYPH_HEIGHT * cfg_win->width * ( iLast - iFirst ));
    }

void cfg_refresh (void)
    {
    win_refresh (cfg_win);
    }

void cfg_wininit (void)
    {
    cfg_win  =  win_create (GLYPH_WIDTH * WINCFG_WTH, GLYPH_HEIGHT * WINCFG_HGT,
        cfg.mon_width_scale, cfg.mon_height_scale,
        "MEMU Configuration",
        NULL, NULL,
        cfg_clr, CLR_COUNT,
        cfg_keypress, cfg_keyrelease);
    }

//  Exit config screen.
void config_term (void)
    {
    if ( cfg_win != NULL )
        {
        win_delete (cfg_win);
        cfg_win  =  NULL;
        }
    }

void cfg_set_display (void)
    {
    }
