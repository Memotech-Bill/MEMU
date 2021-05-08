/*      vdeb.c  -  A VDEB style debugger which does not disturb screen memory or require interrupts */

#include "memu.h"
#include "diag.h"
#include "common.h"
#include "dis.h"
#include "mem.h"
#include "monprom.h"
#include "win.h"
#include "vid.h"
#include "mon.h"
#include "ui.h"
#include "vga.h"
#include "kbd.h"
#include "Z80.h"
#include "vdeb.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#define WINVDEB_WTH          80
#define WINVDEB_HGT          24

#define CLR_BACKGROUND      0
#define CLR_HIGHBACK        1
#define CLR_NORMAL          2
#define CLR_HIGHLIGHT       3
#define CLR_COUNT           4

#define STY_NORMAL          ( 16 * CLR_NORMAL    + CLR_BACKGROUND )
#define STY_HIGHLIGHT       ( 16 * CLR_HIGHLIGHT + CLR_HIGHBACK )

#define COL_L           0
#define COL_R           50
#define COL_P           70
#define ROW_LIST        0
#define ROW_REG         0
#define ROW_INS         7
#define ROW_PR          9
#define ROW_DATA        11

static WIN *vdeb_win  =  NULL;
static int  vdeb_wk   =  -1;
static COL vdeb_clr[CLR_COUNT] = {
    {   0,   0,   0 },   /* Backgound */
    {  64,  64,  64 },   /* Highlighted background */
    {   0, 255, 255 },   /* Normal text */
    { 255, 255,   0 },   /* Highlighted text */
    };

static enum { vm_dis, vm_stp, vm_trc, vm_run } vmode = vm_dis;  // Execution mode
static BOOLEAN bFollow = TRUE;  // Listing follows PC
static BOOLEAN bASCII = FALSE;  // Display bytes as ASCII
static word ltop = 0;           // Top of listing
static word dtop = 0;           // Top of data
static word daddr = 0;          // Current data address
static word taddr = 0;          // Stack address to halt trace
static int  ireg = 0;           // Current register pair (0 = None)
static int  mods = 0;           // Keyboard shift status
#ifndef __circle__
static unsigned int nprofl = 0;     // Number of profiled steps
static unsigned int *profl = NULL;  //  Profile of instruction execution
#endif

static BOOLEAN bDiag = FALSE;

// Break point definition
typedef struct st_brk
    {
    struct st_brk   *pbrkNext;
    struct st_brk   *pbrkPrev;
    word            addr;
    byte            iob;
    enum { bkAll, bkCount, bkAF, bkHL, bkBC, bkDE, bkIX, bkIY, bkAF1, bkHL1, bkBC1, bkDE1,
           bkA, bkH, bkL, bkB, bkC, bkD, bkE, bkA1, bkH1, bkL1, bkB1, bkC1, bkD1, bkE1 } bk;
    word            cond;
    BOOLEAN         bTemp;
    } BREAK;

// Linked list of break points
BREAK   *pbrkFst = NULL;
BREAK   *pbrkLst = NULL;

// Watch address definitions

#define NWPT_INI    5           // Initial watch point allocation
BOOLEAN bWPt = FALSE;           // Watch point data initialised
struct st_wpt
    {
    int nwpt;                   // Number of active watch points
    int mwpt;                   // Size of watch point list
    int *pwpt;                  // Pointer to list of watch addresses
    };

struct st_wpt   wpt_hi;         // Watch points in high memory
struct st_wpt   wpt_pg[16];     // Watch points in paged memory

typedef int (*PVALID)(int);     // Edit validator function prototype

//  Key press event handler - record key pressed.
static void vdeb_keypress(int wk)
    {
//    int mods = win_mod_keys ();
    if ( wk == WK_Shift_L )         mods |= MKY_LSHIFT;
    else if ( wk == WK_Shift_R )    mods |= MKY_RSHIFT;
    else if ( wk == WK_Caps_Lock )  mods ^= MKY_CAPSLK;
    else if ( wk == WK_F10 )        kbd_diag = TRUE;
    else if ( kbd_diag ) diag_control (wk);
    else if ( mods & ( MKY_LSHIFT | MKY_RSHIFT ) ) vdeb_wk = win_shifted_wk (wk);
    else if ( ( mods & MKY_CAPSLK ) && ( wk >= 'a' ) && ( wk <= 'z' ) ) vdeb_wk = wk & 0x5F;
    else vdeb_wk =  wk;
    }

//  Key release event handler - does nothing.
static void vdeb_keyrelease(int wk)
    {
    if ( wk == WK_Shift_L )         mods &= ~ MKY_LSHIFT;
    else if ( wk == WK_Shift_R )    mods &= ~ MKY_RSHIFT;
    else if ( wk == WK_F10 )        kbd_diag = FALSE;
    }

//  Display text on config screen - Uses the monitor ROM font
static void vdeb_print (int iRow, int iCol, int iSty, const char *psTxt, int nCh)
    {
    BOOLEAN  bEOT;
    int  iFG =  iSty >> 4;
    int  iBG =  iSty & 0x0f;
    byte *pby   =  vdeb_win->data + GLYPH_HEIGHT * vdeb_win->width * iRow + GLYPH_WIDTH * iCol;
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
        pby   += vdeb_win->width - GLYPH_WIDTH * nCh;
        }
    }

//  Dispays a cursor (inverted video)
static void vdeb_csr (int iRow, int iCol, int iSty)
    {
    int  iFG =  iSty >> 4;
    int  iBG =  iSty & 0x0f;
    int  iPix;
    byte *pby   =  vdeb_win->data + ( GLYPH_HEIGHT * ( iRow + 1 ) - 1 ) * vdeb_win->width + GLYPH_WIDTH * iCol;
    for ( iPix = 0; iPix < GLYPH_WIDTH; ++iPix )
        {
        if ( *pby == iFG )   *pby  =  iBG;
        else                 *pby  =  iFG;
        ++pby;
        }
    }

//  Wait for a key press
#ifdef   ALT_KEYIN
extern int ALT_KEYIN (void);
#endif

static int vdeb_key (void)
    {
    win_refresh (vdeb_win);
    vdeb_wk   =  -1;
    while ( vdeb_wk < 0 )
        {
        win_handle_events ();
#ifdef   ALT_KEYIN
        if (vdeb_wk >= 0 )  break;
        vdeb_wk =  ALT_KEYIN ();
#endif
        }
    return   vdeb_wk;
    }

//  Edit a line of text - exit on return or escape
static int vdeb_edit (int iRow, int iCol, int nWth, int iSty, int nLen, char *psText, PVALID vld)
    {
    int  nCh    =  strlen (psText);
    int  iCsr   =  0;
    int  iScl   =  0;
    int  wk;
    while (TRUE)
        {
        if ( ( iCsr - iScl ) >= nWth )   iScl  =  iCsr - nWth + 1;
        else if ( iCsr < iScl )          iScl  =  iCsr;
        vdeb_print (iRow, iCol, iSty, &psText[iScl], nWth);
        vdeb_csr (iRow, iCol + iCsr - iScl, iSty);
        wk =  vdeb_key ();
        switch (wk)
            {
            case  WK_Return:
            case  WK_Escape:
            {
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

int vld_hex (int wk)
    {
    if ( ( wk >= '0' ) && ( wk <= '9' ) )   return wk;
    if ( ( wk >= 'A' ) && ( wk <= 'F' ) )   return wk;
    if ( ( wk >= 'a' ) && ( wk <= 'f' ) )   return wk - 32;
    return -1;
    }

int vld_brk (int wk)
    {
    if ( wk == ':' ) return wk;
    return vld_hex (wk);
    }

int vld_cond (int wk)
    {
    if ( ( wk >='a' ) && ( wk <= 'z' ) ) wk -= 32;
    if ( strchr ("0123456789ABCDEFHLN'=", wk) != NULL ) return wk;
    return -1;
    }

BOOLEAN HexVal (const char *psHex, int *pval)
    {
    int val = 0;
    if ( *psHex == '\0' ) return FALSE;
    while ( *psHex )
        {
        char ch = *psHex;
        if ( ( ch >= '0' ) && ( ch <= '9' ) )   val = 16 * val + ch - '0';
        else if ( ( ch >= 'A' ) && ( ch <= 'F' ) )   val = 16 * val + 10 + ch - 'A';
        else if ( ( ch >= 'a' ) && ( ch <= 'f' ) )   val = 16 * val + 10 + ch - 'a';
        else return FALSE;
        ++psHex;
        }
    *pval = val;
    return TRUE;
    }

BOOLEAN GetHex (const char *psPrpt, int nHex, int *pval)
    {
    char sHex[5];
    int nPr = strlen (psPrpt);
    int wk;
    sHex[0] = '\0';
    vdeb_print (ROW_PR, COL_R, STY_NORMAL, psPrpt, nPr);
    wk = vdeb_edit (ROW_PR, COL_R + nPr, nHex + 1, STY_NORMAL, sizeof (sHex), sHex, vld_hex);
    vdeb_print (ROW_PR, COL_R, STY_NORMAL, "", nPr + nHex + 1);
    if ( wk == WK_Return ) return HexVal (sHex, pval);
    return FALSE;
    }

BOOLEAN GetBreakLoc (const char *psPrpt, int *paddr, int *piob)
    {
    char sHex[8];
    int nPr = strlen (psPrpt);
    int wk;
    sHex[0] = '\0';
    vdeb_print (ROW_PR, COL_R, STY_NORMAL, psPrpt, nPr);
    wk = vdeb_edit (ROW_PR, COL_R + nPr, 8, STY_NORMAL, sizeof (sHex), sHex, vld_brk);
    vdeb_print (ROW_PR, COL_R, STY_NORMAL, "", nPr + 8);
    if ( wk == WK_Return )
        {
        int addr;
        int iob;
        BOOLEAN bValid;
        char *psAddr = strchr (sHex, ':');
        if ( psAddr )
            {
            *psAddr = '\0';
            ++psAddr;
            bValid = HexVal (sHex, &iob) && HexVal (psAddr, &addr);
            }
        else
            {
            iob = mem_get_iobyte ();
            bValid = HexVal (sHex, &addr);
            }
        *paddr = (word) addr;
        *piob = (byte) iob;
        return bValid;
        }
    return FALSE;
    }

void AddBreak (BREAK *pbrk)
    {
    if ( pbrkFst == NULL )
        {
        pbrk->pbrkPrev = NULL;
        pbrk->pbrkNext = NULL;
        pbrkFst = pbrk;
        pbrkLst = pbrk;
        }
    else
        {
        pbrk->pbrkPrev = NULL;
        pbrk->pbrkNext = pbrkFst;
        pbrkFst->pbrkPrev = pbrk;
        pbrkFst = pbrk;
        }
    }

void DelBreak (BREAK *pbrk)
    {
    if ( pbrk != NULL )
        {
        if ( pbrk->pbrkPrev == NULL )   pbrkFst = pbrk->pbrkNext;
        else    pbrk->pbrkPrev->pbrkNext = pbrk->pbrkNext;
        if ( pbrk->pbrkNext == NULL )   pbrkLst = pbrk->pbrkPrev;
        else    pbrk->pbrkNext->pbrkPrev = pbrk->pbrkPrev;
        free (pbrk);
        }
    }

BOOLEAN MatchBreak (BREAK *pbrk, word addr, byte iob)
    {
    if ( pbrk->addr != addr ) return FALSE;
    if ( addr >= 0xC000 )
        {
        return TRUE;
        }
    else if ( ( addr >= 0x4000 ) || ( iob & 0x80 ) )
        {
        if ( ( pbrk->iob & 0x8F ) == ( iob & 0x8F ) ) return TRUE;
        }
    else if ( addr >= 0x2000 )
        {
        if ( ( pbrk->iob & 0xF0 ) == ( iob & 0xF0 ) ) return TRUE;
        }
    else
        {
        return TRUE;
        }
    return FALSE;
    }

BREAK *FindBreak (word addr, byte iob)
    {
    BREAK *pbrk = pbrkFst;
    while ( pbrk != NULL )
        {
        if ( MatchBreak (pbrk, addr, iob) ) return pbrk;
        pbrk = pbrk->pbrkNext;
        }
    return NULL;
    }

BREAK *GetBreak (word addr, byte iob)
    {
    BREAK *pbrk = FindBreak (addr, iob);
    if ( pbrk == NULL )
        {
        pbrk = (BREAK *) malloc (sizeof (BREAK));
        pbrk->pbrkPrev = NULL;
        pbrk->pbrkNext = NULL;
        pbrk->addr = addr;
        pbrk->iob  = iob;
        pbrk->bk   = bkCount;
        pbrk->cond = 1;
        pbrk->bTemp = FALSE;
        AddBreak (pbrk);
        if (bDiag) diag_message (DIAG_ALWAYS, "Created Break at addr = 0x%04X, iob = 0x%02X", pbrk->addr, pbrk->iob);
        }
    return pbrk;
    }

BOOLEAN TestBreak (Z80 *R)
    {
    BREAK *pbrk = pbrkFst;
    byte iob = mem_get_iobyte ();
    if (bDiag) diag_message (DIAG_ALWAYS, "TestBreak: addr = 0x%04X, iob = 0x%02X", R->PC.W, iob);
    while ( pbrk != NULL )
        {
        if (bDiag) diag_message (DIAG_ALWAYS, "Testing: addr = 0x%04X, iob = 0x%02X", pbrk->addr, pbrk->iob);
        if ( MatchBreak (pbrk, R->PC.W, iob) )
            {
            if (bDiag) diag_message (DIAG_ALWAYS, "... Match found");
            if ( pbrk->bk == bkCount )
                {
                if ( --pbrk->cond == 0 )
                    {
                    DelBreak (pbrk);
                    return TRUE;
                    }
                }
            if ( pbrk->bTemp )
                {
                pbrk->bTemp = FALSE;
                return TRUE;
                }
            switch (pbrk->bk)
                {
                case bkAll: return TRUE;
                case bkAF: if ( R->AF.W == pbrk->cond ) return TRUE; break;
                case bkHL: if ( R->HL.W == pbrk->cond ) return TRUE; break;
                case bkBC: if ( R->BC.W == pbrk->cond ) return TRUE; break;
                case bkDE: if ( R->DE.W == pbrk->cond ) return TRUE; break;
                case bkIX: if ( R->IX.W == pbrk->cond ) return TRUE; break;
                case bkIY: if ( R->IY.W == pbrk->cond ) return TRUE; break;
                case bkAF1: if ( R->AF1.W == pbrk->cond ) return TRUE; break;
                case bkHL1: if ( R->HL1.W == pbrk->cond ) return TRUE; break;
                case bkBC1: if ( R->BC1.W == pbrk->cond ) return TRUE; break;
                case bkDE1: if ( R->DE1.W == pbrk->cond ) return TRUE; break;
                case bkA: if ( R->AF.B.h == pbrk->cond ) return TRUE; break;
                case bkH: if ( R->HL.B.h == pbrk->cond ) return TRUE; break;
                case bkL: if ( R->HL.B.l == pbrk->cond ) return TRUE; break;
                case bkB: if ( R->BC.B.h == pbrk->cond ) return TRUE; break;
                case bkC: if ( R->BC.B.l == pbrk->cond ) return TRUE; break;
                case bkD: if ( R->DE.B.h == pbrk->cond ) return TRUE; break;
                case bkE: if ( R->DE.B.l == pbrk->cond ) return TRUE; break;
                case bkA1: if ( R->AF1.B.h == pbrk->cond ) return TRUE; break;
                case bkH1: if ( R->HL1.B.h == pbrk->cond ) return TRUE; break;
                case bkL1: if ( R->HL1.B.l == pbrk->cond ) return TRUE; break;
                case bkB1: if ( R->BC1.B.h == pbrk->cond ) return TRUE; break;
                case bkC1: if ( R->BC1.B.l == pbrk->cond ) return TRUE; break;
                case bkD1: if ( R->DE1.B.h == pbrk->cond ) return TRUE; break;
                case bkE1: if ( R->DE1.B.l == pbrk->cond ) return TRUE; break;
                default: break;
                }
            }
        pbrk = pbrk->pbrkNext;
        }
    return FALSE;
    }

BOOLEAN BreakCond (BREAK *pbrk, const char *psCond)
    {
    static const char *psTest[] = { "N=", "AF=", "HL=", "BC=", "DE=", "IX=", "IY=",
                                    "AF'=", "HL'=", "BC'=", "DE'=",
                                    "A=", "H=", "L=", "B=", "C=", "D=", "E=",
                                    "A'=", "H'=", "L'=", "B'=", "C'=", "D'=", "E'=" };
    int i;
    if ( *psCond == '\0' ) return TRUE;
    for ( i = 0; i < sizeof (psTest) / sizeof (psTest[0]); ++i )
        {
        int n = strlen (psTest[i]);
        if ( strncmp (psCond, psTest[i], n) == 0 )
            {
            int val;
            if ( HexVal (&psCond[n], &val) )
                {
                pbrk->bk = bkCount + i;
                pbrk->cond = (word) val;
                return TRUE;
                }
            return FALSE;
            }
        }
    return FALSE;
    }

BOOLEAN TestCall (Z80 *R)
    {
    BOOLEAN bExit = FALSE;
    byte f = R->AF.B.l;
    switch (RdZ80 (R->PC.W))
        {
        case 0xCD:  bExit = TRUE; break;
        case 0xC7:  bExit = TRUE; break;
        case 0xCF:  bExit = TRUE; break;
        case 0xD7:  bExit = TRUE; break;
        case 0xDF:  bExit = TRUE; break;
        case 0xE7:  bExit = TRUE; break;
        case 0xEF:  bExit = TRUE; break;
        case 0xF7:  bExit = TRUE; break;
        case 0xFF:  bExit = TRUE; break;
        case 0xC4:  if ( ( f & Z_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xCC:  if ( ( f & Z_FLAG ) != 0 ) bExit = TRUE; break;
        case 0xD4:  if ( ( f & C_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xDC:  if ( ( f & C_FLAG ) != 0 ) bExit = TRUE; break;
        case 0xE4:  if ( ( f & P_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xEC:  if ( ( f & P_FLAG ) != 0 ) bExit = TRUE; break;
        case 0xF4:  if ( ( f & S_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xFC:  if ( ( f & S_FLAG ) != 0 ) bExit = TRUE; break;
        default: break;
        }
    if ( bExit ) if (bDiag) diag_message (DIAG_ALWAYS, "AF = %04X Flags = %02X", R->AF.W, f);
    return bExit;
    }

BOOLEAN TestReturn (Z80 *R)
    {
    BOOLEAN bExit = FALSE;
    byte f = R->AF.B.l;
    if ( R->SP.W != taddr ) return FALSE;
    switch (RdZ80 (R->PC.W))
        {
        case 0xC9:  bExit = TRUE; break;
        case 0xC0:  if ( ( f & Z_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xC8:  if ( ( f & Z_FLAG ) != 0 ) bExit = TRUE; break;
        case 0xD0:  if ( ( f & C_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xD8:  if ( ( f & C_FLAG ) != 0 ) bExit = TRUE; break;
        case 0xE0:  if ( ( f & P_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xE8:  if ( ( f & P_FLAG ) != 0 ) bExit = TRUE; break;
        case 0xF0:  if ( ( f & S_FLAG ) == 0 ) bExit = TRUE; break;
        case 0xF8:  if ( ( f & S_FLAG ) != 0 ) bExit = TRUE; break;
        case 0xED:
            switch (RdZ80 (R->PC.W) + 1)
                {
                case 0x45:
                case 0x4D:
                    bExit = TRUE;
                default:
                    break;
                }
        default: break;
        }
    if ( bExit ) if (bDiag) diag_message (DIAG_ALWAYS, "AF = %04X Flags = %02X", R->AF.W, f);
    return bExit;
    }

void vdeb_init (void)
    {
    if ( ! bWPt )
        {
        memset (&wpt_hi, 0, sizeof (wpt_hi));
        memset (&wpt_pg, 0, sizeof (wpt_pg));
        bWPt = TRUE;
        }
    vdeb_win  =  win_create (GLYPH_WIDTH * WINVDEB_WTH, GLYPH_HEIGHT * WINVDEB_HGT,
        cfg.mon_width_scale, cfg.mon_height_scale,
        "Visual Debugger",
        NULL, NULL,
        vdeb_clr, CLR_COUNT,
        vdeb_keypress, vdeb_keyrelease);
    vmode = vm_stp;
    }

void vdeb_break (void)
    {
    kbd_diag = FALSE;   // kbd_win_keyrelease (WK_F10);
    if ( vmode == vm_dis ) vdeb_init ();
    vmode = vm_stp;
    }

void vdeb_term (void)
    {
    vmode = vm_dis;
    if ( vdeb_win != NULL ) win_delete (vdeb_win);
    }

void wpt_add (struct st_wpt *pwpt, word addr)
    {
    if ( pwpt->mwpt == 0 )
        {
        pwpt->pwpt = emalloc (NWPT_INI * sizeof (word));
        pwpt->mwpt = NWPT_INI;
        }
    else if ( pwpt->nwpt == pwpt->mwpt )
        {
        pwpt->mwpt *= 2;
        pwpt->pwpt = realloc (pwpt->pwpt, pwpt->mwpt * sizeof (word));
        if ( pwpt->pwpt == NULL ) fatal ("Insufficient memory for watch point");
        }
    int i = pwpt->nwpt;
    while ( --i >= 0 )
        {
        if ( addr > pwpt->pwpt[i] )
            {
            pwpt->pwpt[i+1] = addr;
            ++pwpt->nwpt;
            return;
            }
        else if ( addr == pwpt->pwpt[i] )
            {
            while ( ++i < pwpt->nwpt ) pwpt->pwpt[i] = pwpt->pwpt[i+1];
            return;
            }
        else
            {
            pwpt->pwpt[i+1] = pwpt->pwpt[i];
            }
        }
    pwpt->pwpt[0] = addr;
    ++pwpt->nwpt;
    }

void wpt_del (struct st_wpt *pwpt, word addr)
    {
    int i = 0;
    while (( i < pwpt->nwpt ) && ( pwpt->pwpt[i] != addr )) ++i;
    if ( i < pwpt->nwpt )
        {
        while ( ++i < pwpt->nwpt ) pwpt->pwpt[i-1] = pwpt->pwpt[i];
        --pwpt->nwpt;
        }
    }

BOOLEAN wpt_find (struct st_wpt *pwpt, word addr)
    {
    if ( pwpt->nwpt == 0 ) return FALSE;
    int i1 = 0;
    if ( pwpt->pwpt[i1] == addr ) return TRUE;
    else if ( pwpt->pwpt[i1] > addr ) return FALSE;
    if ( pwpt->nwpt == 1 ) return FALSE;
    int i2 = pwpt->nwpt - 1;
    if ( pwpt->pwpt[i2] == addr ) return TRUE;
    else if ( pwpt->pwpt[i2] < addr ) return FALSE;
    if ( pwpt->nwpt == 2 ) return FALSE;
    while ( i2 - i1 > 1 )
        {
        int i3 = ( i1 + i2 ) / 2;
        if ( pwpt->pwpt[i3] == addr ) return TRUE;
        else if ( pwpt->pwpt[i3] > addr ) i2 = i3;
        else i1 = i3;
        }
    return FALSE;
    }

void add_wpt (byte iob, int addr)
    {
    if ( addr >= 0xC000 ) wpt_add (&wpt_hi, addr);
    else wpt_add (&wpt_pg[iob & 0x0F], addr);
    }

void del_wpt (byte iob, int addr)
    {
    if ( addr >= 0xC000 ) wpt_del (&wpt_hi, addr);
    else wpt_del (&wpt_pg[iob & 0x0F], addr);
    }

BOOLEAN find_wpt (byte iob, int addr)
    {
    if ( addr >= 0xC000 ) return wpt_find (&wpt_hi, addr);
    else return wpt_find (&wpt_pg[iob & 0x0F], addr);
    }

void vdeb_mwrite (byte iob, word addr)
    {
    if ( vmode != vm_dis )
        {
        if ( find_wpt (iob, addr) ) vmode = vm_stp;
        }
    }

void vdeb_iobyte (byte iob)
    {
    if ( vmode != vm_dis )
        {
        mem_wrchk (wpt_hi.nwpt + wpt_pg[iob & 0x0F].nwpt > 0);
        }
    }

void vdeb_regs (Z80 *R)
    {
    static char sFlag[] = "CNV3H5ZS";
    char sReg[9];
    byte flags = R->AF.B.l;
    int n;
    for ( n = 0; n < 8; ++n )
        {
        vdeb_print (ROW_REG, COL_R + n, STY_NORMAL, ( flags & 1 ) ? &sFlag[n] : " ", 1);
        flags >>= 1;
        }
    sprintf (sReg, "%-3s %04X", "AF", R->AF.W);
    vdeb_print (ROW_REG + 1, COL_R, ( ireg == 1 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "HL", R->HL.W);
    vdeb_print (ROW_REG + 2, COL_R, ( ireg == 2 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "BC", R->BC.W);
    vdeb_print (ROW_REG + 3, COL_R, ( ireg == 3 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "DE", R->DE.W);
    vdeb_print (ROW_REG + 4, COL_R, ( ireg == 4 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "AF'", R->AF1.W);
    vdeb_print (ROW_REG + 1, COL_R + 10, ( ireg == 5 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "HL'", R->HL1.W);
    vdeb_print (ROW_REG + 2, COL_R + 10, ( ireg == 6 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "BC'", R->BC1.W);
    vdeb_print (ROW_REG + 3, COL_R + 10, ( ireg == 7 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "DE'", R->DE1.W);
    vdeb_print (ROW_REG + 4, COL_R + 10, ( ireg == 8 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "IX", R->IX.W);
    vdeb_print (ROW_REG + 1, COL_R + 20, ( ireg ==  9 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "IY", R->IY.W);
    vdeb_print (ROW_REG + 2, COL_R + 20, ( ireg == 10 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "SP", R->SP.W);
    vdeb_print (ROW_REG + 3, COL_R + 20, ( ireg == 11 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %04X", "PC", R->PC.W);
    vdeb_print (ROW_REG + 4, COL_R + 20, ( ireg == 12 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %02X", "I", R->I);
    vdeb_print (ROW_REG + 5, COL_R, ( ireg == 13 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    sprintf (sReg, "%-3s %02X", "IFF", R->IFF);
    vdeb_print (ROW_REG + 5, COL_R + 10, ( ireg == 14 ) ? STY_HIGHLIGHT : STY_NORMAL, sReg, 8);
    flags = R->IFF;
    sprintf (sReg, "%c%d%c%c%c", ( flags & IFF_IEN ) ? 'E' : ' ', ( flags & IFF_IMODE ) >> 1,
        ( flags & IFF_IENX ) ? 'X' : ' ', ( flags & IFF_IEN2 ) ? 'S' : ' ',
        ( flags & IFF_HALT ) ? 'H' : ' ');
    vdeb_print (ROW_REG + 5, COL_R + 20, STY_NORMAL, sReg, 5);
    }

void vdeb_ins (int iRow, int iCol, int nLen, int iSty, word *addr)
    {
    char sAsm[WINVDEB_WTH+1];
    if ( ! dis_instruction (addr, sAsm) )
        {
        sprintf (sAsm, "defb 0x%02X", RdZ80(*addr));
        ++(*addr);
        }
    sAsm[nLen] = '\0';
    vdeb_print (iRow, iCol, iSty, sAsm, nLen);
    }

void vdeb_dis (Z80 *R)
    {
    byte iob = mem_get_iobyte ();
    byte ios;
    char sAddr[9];
    word addr;
    int n;
    BOOLEAN bFound;
    int iSty;
    use_syms = FALSE;
    show_opcode = FALSE;
    dis_show_ill = FALSE;
    dis_mtx_exts = FALSE;
    while (TRUE)
        {
        addr = ltop;
        bFound = FALSE;
        for ( n = 0; n < WINVDEB_HGT; ++n )
            {
            if ( ( addr < 0x4000 ) && ( ( iob & 0x80 ) == 0 ) )
                {
                ios = iob & 0x70;
                if ( addr < 0x2000 ) iob = 0x00;
                }
            else
                {
                ios = iob & 0x8F;
                if ( addr >= 0xC000 ) iob = 0x00;
                }
            iSty = STY_NORMAL;
            if ( addr == R->PC.W )
                {
                bFound = TRUE;
                iSty = STY_HIGHLIGHT;
                }
            sprintf (sAddr, "%02X:%04X ", ios, addr);
            vdeb_print (n, COL_L, iSty, sAddr, 8);
            vdeb_ins (n, COL_L + 8, COL_R - COL_L - 9, iSty, &addr);
            }
        if ( bFound || ! bFollow ) return;
        ltop = R->PC.W;
        }
    }

void vdeb_data (void)
    {
    char sText[6];
    int i, n;
    word addr;
    if ( ( dtop > daddr ) || ( ( dtop + 8 * ( WINVDEB_HGT - ROW_DATA ) ) <= daddr ) )
        dtop = daddr & 0xFFF8;
    addr = dtop;
    for ( n = 0; n < WINVDEB_HGT - ROW_DATA; ++n )
        {
        sprintf (sText, "%04X", addr);
        vdeb_print (n + ROW_DATA, COL_R, STY_NORMAL, sText, 4);
        for ( i = 0; i < 8; ++i )
            {
            byte b = RdZ80 (addr);
            if ( bASCII && ( b > 0x20 ) && ( b < 0x7F ) )
                sprintf (sText, " %c ", b);
            else
                sprintf (sText, " %02X", b);
            vdeb_print (n + ROW_DATA, COL_R + 4 + 3 * i,
                ( addr == daddr ) ? STY_HIGHLIGHT : STY_NORMAL, sText, 3);
            ++addr;
            }
        }
    }

void vdeb_running (BOOLEAN bRun)
    {
    if ( bRun )
        {
        vdeb_print (ROW_INS, COL_R, STY_HIGHLIGHT, "Running", WINVDEB_WTH - COL_R);
        win_refresh (vdeb_win);
        }
    else
        {
        vdeb_print (ROW_INS, COL_R, STY_NORMAL, "", WINVDEB_WTH - COL_R);
        }
    }

#ifndef __circle__
void pcount (void)
    {
    char sCount[11];
    if ( profl != NULL )
        {
        sprintf (sCount, "%10u", nprofl);
        vdeb_print (ROW_PR, COL_P, STY_NORMAL, sCount, WINVDEB_WTH - COL_P);
        }
    else
        {
        vdeb_print (ROW_PR, COL_P, STY_NORMAL, "", WINVDEB_WTH - COL_P);
        }
    }

struct st_pro
    {
    word            abegin;
    word            aend;
    unsigned int    count;
    };

int pro_cmp (const void *pv1, const void *pv2)
    {
    const struct st_pro *pro1 = (const struct st_pro *) pv1;
    const struct st_pro *pro2 = (const struct st_pro *) pv2;
    // Sort to result in decreasing order of count
    return  pro2->count - pro1->count;
    }

void dump_profl (void)
    {
    int mpro = 100;
    int npro = -1;
    struct st_pro *pro = calloc (mpro, sizeof (struct st_pro));
    int last = 0;
    int addr;
    if ( pro == NULL ) return;
    // Collect address ranges with the same count
    for ( addr = 0; addr < 0x10000; ++addr )
        {
        unsigned int pc = profl[addr];
        if ( pc > 0 )
            {
            if ( pc != last )
                {
                if ( ++npro >= mpro )
                    {
                    mpro *= 2;
                    struct st_pro *pro2 = realloc (pro, mpro * sizeof (struct st_pro));
                    if ( pro2 == NULL )
                        {
                        free (pro);
                        return;
                        }
                    pro = pro2;
                    }
                pro[npro].abegin = (word) addr;
                pro[npro].count = pc;
                last = pc;
                }
            pro[npro].aend = (word) addr;
            }
        }
    // Sort the results
    ++npro;
    qsort (pro, npro, sizeof (struct st_pro), pro_cmp);
    // Save results to file
    time_t  t = time (NULL);
    char sFile[29];
    strftime (sFile, sizeof (sFile), "Profile_%Y%m%d_%H%M%S.txt", localtime (&t));
    FILE *f = fopen (sFile, "w");
    if ( f == NULL ) return;
    fprintf (f, "Address    \t     Count\n");
    int i;
    for ( i = 0; i < npro; ++i )
        fprintf (f, "%04X - %04X\t%10u\n", pro[i].abegin, pro[i].aend, pro[i].count);
    fclose (f);
    }
#endif

void vdeb (Z80 *R)
    {
    BREAK *pbrk;
    int addr;
    int iob;
    // Collect profile data
#ifndef __circle__
    if ( profl != NULL )
        {
        ++nprofl;
        ++profl[R->PC.W];
        }
#endif
    // Test for entering VDEB display
    switch (vmode)
        {
        case vm_dis:
            return;
        case vm_stp:
            break;
        case vm_trc:
            if ( TestReturn (R) ) vmode = vm_stp;
            // Deliberately run on to next case
        case vm_run:
            if ( ! TestBreak (R) ) return;
            break;
        }
    // Update windows
    vdeb_running (FALSE);
    vdeb_regs (R);
    vdeb_dis (R);
    vdeb_data ();
#ifndef __circle__
    pcount ();
#endif
    vid_refresh_vdeb ();
    mon_refresh_vdeb ();
    ui_refresh ();
#ifdef HAVE_VGA
    if ( cfg.bVGA ) vga_refresh ();
#endif
    word adnext = R->PC.W;
    vdeb_ins (ROW_INS, COL_R, WINVDEB_WTH - COL_R, STY_NORMAL, &adnext);
    // Process key presses
    while (TRUE)
        {
#ifdef __circle__
        vdeb_print (ROW_PR, COL_R, STY_NORMAL, "BCDGILNQRSTWX ?", COL_P - COL_R);
#else
        vdeb_print (ROW_PR, COL_R, STY_NORMAL, "BCDGILNPQRSTWX ?", COL_P - COL_R);
#endif
        int key = vdeb_key ();
        vdeb_print (ROW_PR, COL_R, STY_NORMAL, "", COL_P - COL_R);
        if ( ( key >= 'a' ) && ( key <= 'z' ) ) key -= 32;
        switch (key)
            {
            case 'G':
            {
            if ( GetHex ("GO> ", 4, &addr) )
                {
                R->PC.W = (word) addr;
                vmode = vm_run;
                }
            if ( GetBreakLoc ("TO> ", &addr, &iob) )
                {
                GetBreak ((word) addr, (byte) iob)->bTemp = TRUE;
                }
            if ( vmode != vm_trc ) vmode = vm_run;
            vdeb_running (TRUE);
            return;
            }
            case 'S':
            {
            vmode = vm_stp;
            return;
            }
            case 'N':
            {
            // bDiag = TRUE;
            if (bDiag) diag_message (DIAG_ALWAYS, "VDEB: Next");
            GetBreak ((word) adnext, mem_get_iobyte ())->bTemp = TRUE;
            if ( vmode != vm_trc ) vmode = vm_run;
            // diag_flags[DIAG_Z80_INSTRUCTIONS] = TRUE;
            vdeb_running (TRUE);
            return;
            }
            case 'T':
            {
            if (bDiag) diag_message (DIAG_ALWAYS, "VDEB: Trace");
            if ( TestCall (R) )
                {
                // CALL Opcodes
                taddr = R->SP.W - 2;
                vmode = vm_trc;
                vdeb_running (TRUE);
                }
            else
                {
                vmode = vm_stp;
                }
            // diag_flags[DIAG_Z80_INSTRUCTIONS] = TRUE;
            return;
            }
            case 'X':
            {
            if (bDiag) diag_message (DIAG_ALWAYS, "VDEB: Exit");
            taddr = R->SP.W;
            vmode = vm_trc;
            if ( TestReturn (R) ) vmode = vm_stp;
            vdeb_running (TRUE);
            // diag_flags[DIAG_Z80_INSTRUCTIONS] = TRUE;
            return;
            }
            case 'B':
            {
            if ( GetBreakLoc ("Break> ", &addr, &iob) )
                {
                char sCond[9];
                sCond[0] = '\0';
                pbrk = GetBreak ((word) addr, (byte) iob);
                pbrk->bk = bkAll;
                vdeb_print (ROW_PR, COL_R, STY_NORMAL, "Cond>        ", 0);
                while (TRUE)
                    {
                    if ( vdeb_edit (ROW_PR, COL_R + 6, 9, STY_NORMAL, sizeof (sCond), sCond, vld_cond)
                        == WK_Escape )
                        break;
                    if ( BreakCond (pbrk, sCond) ) break;
                    }
                vdeb_print (ROW_PR, COL_R, STY_NORMAL, "", 14);
                }
            break;
            }
            case 'W':
            {
            if ( GetBreakLoc ("Watch> ", &addr, &iob) )
                {
                add_wpt ((byte) iob, (word) addr);
                vdeb_iobyte (mem_get_iobyte ());
                }
            break;
            }
            case 'C':
            {
            if ( GetBreakLoc ("Clear Break> ", &addr, &iob) )
                {
                DelBreak (FindBreak ((word) addr, (byte) iob));
                del_wpt ((byte) iob, (word) addr);
                vdeb_iobyte (mem_get_iobyte ());
                }
            break;
            }
            case 'L':
            {
            if ( GetHex ("List> ", 4, &addr) )
                {
                ltop = (word) addr;
                bFollow = FALSE;
                }
            else
                {
                ltop = R->PC.W;
                bFollow = TRUE;
                }
            vdeb_dis (R);
            break;
            }
            case WK_Left:
            {
            --daddr;
            vdeb_data ();
            break;
            }
            case WK_Right:
            {
            ++daddr;
            vdeb_data ();
            break;
            }
            case WK_Up:
            {
            daddr -= 8;
            vdeb_data ();
            break;
            }
            case WK_Down:
            {
            daddr += 8;
            vdeb_data ();
            break;
            }
            case 'D':
            {
            if ( GetHex ("Display> ", 4, &addr) )
                {
                daddr = (word) addr;
                vdeb_data ();
                while (TRUE)
                    {
                    char sByte[9];
                    int  val;
                    sprintf (sByte, "%04X %02X ", daddr, RdZ80 (daddr));
                    if ( GetHex (sByte, 2, &val) )
                        {
                        mem_write_byte (daddr, (byte) val);
                        ++daddr;
                        vdeb_data ();
                        }
                    else
                        {
                        break;
                        }
                    }
                }
            break;
            }
            case 'I':
            {
            bASCII = ! bASCII;
            vdeb_data ();
            break;
            }
            case '.':
            {
            if ( ++ireg > 14 ) ireg = 0;
            vdeb_regs (R);
            break;
            }
            case 'R':
            {
            static char *psRegs[] = { "AF> ", "HL> ", "BC> ", "DE> ", "AF'> ", "HL'> ", "BC'> ", "DE'> ",
                                      "IX> ", "IY> ", "SP> ", "PC> ", "I> ", "IFF> " };
            if ( ireg != 0 )
                {
                if ( GetHex (psRegs[ireg-1], 4, &addr) )
                    {
                    switch (ireg)
                        {
                        case  1: R->AF.W  = (word) addr; break;
                        case  2: R->HL.W  = (word) addr; break;
                        case  3: R->BC.W  = (word) addr; break;
                        case  4: R->DE.W  = (word) addr; break;
                        case  5: R->AF1.W = (word) addr; break;
                        case  6: R->HL1.W = (word) addr; break;
                        case  7: R->BC1.W = (word) addr; break;
                        case  8: R->DE1.W = (word) addr; break;
                        case  9: R->IX.W  = (word) addr; break;
                        case 10: R->IY.W  = (word) addr; break;
                        case 11: R->SP.W  = (word) addr; break;
                        case 12: R->PC.W  = (word) addr; break;
                        case 13: R->I     = (byte) addr; break;
                        case 14: R->IFF   = (byte) addr; break;
                        default: break;
                        }
                    vdeb_regs (R);
                    }
                }
            break;
            }
#ifndef __circle__
            case 'P':
            {
            if ( profl == NULL )
                {
                profl = calloc (0x10000, sizeof (unsigned int));
                nprofl = 0;
                }
            else
                {
                dump_profl ();
                free (profl);
                profl = NULL;
                nprofl = 0;
                }
            pcount ();
            break;
            }
#endif
            case 'Q':
            {
            vdeb_print (ROW_PR, COL_R, STY_NORMAL, "Quit ? ", 0);
            key = vdeb_key ();
            if ( ( key == 'Y' ) || ( key == 'y' ) )
                {
                vdeb_term ();
                return;
                }
            }
            break;
            }
        }
    }
