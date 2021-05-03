/* cfgwin_pico.c - Pico versions of the config window low level routines */

#include <string.h>
#include "display_pico.h"
#include "types.h"
#include "kbd.h"
#include "diag.h"
#include "config.h"

static EightyColumn ec_cfg;
static DisplayState dstate;
static uint8_t  styatr[STY_COUNT] = { 0x06, 0x03, 0x04, 0x02 };
static int  cfg_wk   =  -1;

static int mods = 0;

//  Key press event handler - record key pressed.
void cfg_keypress(int wk)
    {
//    int mods = win_mod_keys ();
    if ( wk == WK_Shift_L )         mods |= MKY_LSHIFT;
    else if ( wk == WK_Shift_R )    mods |= MKY_RSHIFT;
    else if ( wk == WK_Caps_Lock )  mods ^= MKY_CAPSLK;
    if ( mods & ( MKY_LSHIFT | MKY_RSHIFT ) ) cfg_wk = win_shifted_wk (wk);
    else if ( ( mods & MKY_CAPSLK ) && ( wk >= 'a' ) && ( wk <= 'z' ) ) cfg_wk = wk & 0x5F;
    else cfg_wk =  wk;
    }

//  Key release event handler - does nothing.
void cfg_keyrelease(int wk)
    {
    if ( wk == WK_Shift_L )         mods &= ~ MKY_LSHIFT;
    else if ( wk == WK_Shift_R )    mods &= ~ MKY_RSHIFT;
    }

//  Display text on config screen - Uses the monitor ROM font
void cfg_print (int iRow, int iCol, int iSty, const char *psTxt, int nCh)
    {
    int iAddr = EC_COLS * iRow + iCol;
    uint8_t attr = styatr[iSty];
    if ( nCh == 0 ) nCh = EC_COLS;
    for ( int i = 0; i < nCh; ++i )
        {
        if ( *psTxt == '\0' ) break;
        ec_cfg.ram[iAddr].ch = *psTxt;
        ec_cfg.ram[iAddr].at = attr;
        ++psTxt;
        ++iAddr;
        }
    }

//  Dispays a cursor (inverted video)
void cfg_csr (int iRow, int iCol, int iSty)
    {
    int iAddr = EC_COLS * iRow + iCol;
    ec_cfg.reg[10] = 0x40;
    ec_cfg.reg[11] = 9;
    ec_cfg.reg[14] = iAddr >> 8;
    ec_cfg.reg[15] = iAddr & 0xFF;
    }

//  Clears rows of text on the configuration screen - for ( row = first; row < last; ++row )
void cfg_clear_rows (int iFirst, int iLast)
    {
    memset (&ec_cfg.ram[EC_COLS * iFirst], 0, (iLast - iFirst)*EC_COLS*sizeof (ec_cfg.ram[0]));
    }

//  Wait for a key press
#ifdef   ALT_KEYIN
extern int ALT_KEYIN (void);
#endif

int cfg_key (void)
    {
    // diag_message (DIAG_INIT, "Entered cfg_key");
    cfg_wk   =  -1;
    while ( cfg_wk < 0 )
        {
        // diag_message (DIAG_INIT, "Call kbd_periodic");
        kbd_periodic ();
#ifdef   ALT_KEYIN
        if (cfg_wk >= 0 )  break;
        cfg_wk =  ALT_KEYIN ();
#endif
        }
    // diag_message (DIAG_INIT, "Return cfg_wk = %d", cfg_wk);
    return   cfg_wk;
    }

//  Test for entering config mode
BOOLEAN test_cfg_key (int wk)
    {
    // diag_message (DIAG_INIT, "Test key 0x%02X for Config key", wk);
    switch (wk)
        {
//        case WK_Scroll_Lock:
        case WK_Sys_Req:
        case WK_PC_Windows_L:
        case WK_PC_Windows_R:
        case WK_PC_Menu:
        case WK_F11:
            // diag_message (DIAG_INIT, "Config Key found");
            config ();
            // diag_message (DIAG_INIT, "Config exit");
            return TRUE;
        }
    return FALSE;
    }

void cfg_wininit (void)
    {
    diag_message (DIAG_INIT, "Install Config key handlers");
    kbd_set_handlers (cfg_keypress, cfg_keyrelease);
    display_save (&dstate);
    memset (&ec_cfg, 0, sizeof (ec_cfg));
    ec_cfg.reg[10] = 0x20;                     // Hide cursor
    diag_message (DIAG_INIT, "Display Config window");
    display_80column (&ec_cfg);
    }

//  Exit config screen.
void config_term (void)
    {
    diag_message (DIAG_INIT, "Config Terminate - Remove keyboard handlers and restore screen");
    kbd_set_handlers (NULL, NULL);
    display_load (&dstate);
    }

void cfg_set_display (void)
    {
    display_save (&dstate);
    }
