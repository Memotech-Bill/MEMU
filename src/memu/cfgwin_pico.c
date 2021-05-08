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

void cfg_refresh (void)
    {
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
