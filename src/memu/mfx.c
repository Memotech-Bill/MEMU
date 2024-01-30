/*  mfx.c - Emulation of the MFX hardware features */

#include "types.h"
#include "mfx.h"
#include "vdp.h"
#include "win.h"
#include "kbd.h"
#include "monprom.h"
#include "common.h"
#include "diag.h"
#include <stdio.h>

#define WIDTH   640
#define HEIGHT  480
#define VDP_XORG    ( WIDTH / 2 - VDP_WIDTH )
#define VDP_YORG    ( HEIGHT / 2 - VDP_HEIGHT )
#define TWIDTH      8
#define THEIGHT     GLYPH_HEIGHT
#define TROWS       ( HEIGHT / THEIGHT )
#define TCOLS       ( WIDTH / TWIDTH )
#define GWIDTH      ( 2 * TWIDTH )
#define GHEIGHT     ( 2 * GLYPH_HEIGHT )
#define GROWS       ( HEIGHT / GHEIGHT )
#define GCOLS       ( WIDTH / GWIDTH )

static WIN *mfx_win = NULL;

#define N_CLR_MFX  256
static COL mfx_pal[N_CLR_MFX] = {
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0x00, 0xFF, 0x00},
    {0x00, 0xFF, 0x00},
    {0x00, 0xFF, 0x00},
    {0x00, 0xFF, 0x00},
    {0x00, 0xFF, 0x00},
    {0x00, 0xFF, 0x00},
    {0x00, 0xFF, 0x00},
    {0x00, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0x00, 0x00, 0xFF},
    {0x00, 0x00, 0xFF},
    {0x00, 0x00, 0xFF},
    {0x00, 0x00, 0xFF},
    {0x00, 0x00, 0xFF},
    {0x00, 0x00, 0xFF},
    {0x00, 0x00, 0xFF},
    {0x00, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0x00},
    {0x55, 0x00, 0x00},
    {0x00, 0x55, 0x00},
    {0x55, 0x55, 0x00},
    {0x00, 0x00, 0x55},
    {0x55, 0x00, 0x55},
    {0x00, 0x55, 0x55},
    {0x55, 0x55, 0x55},
    {0xAA, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xAA, 0x55, 0x00},
    {0xFF, 0x55, 0x00},
    {0xAA, 0x00, 0x55},
    {0xFF, 0x00, 0x55},
    {0xAA, 0x55, 0x55},
    {0xFF, 0x55, 0x55},
    {0x00, 0xAA, 0x00},
    {0x55, 0xAA, 0x00},
    {0x00, 0xFF, 0x00},
    {0x55, 0xFF, 0x00},
    {0x00, 0xAA, 0x55},
    {0x55, 0xAA, 0x55},
    {0x00, 0xFF, 0x55},
    {0x55, 0xFF, 0x55},
    {0xAA, 0xAA, 0x00},
    {0xFF, 0xAA, 0x00},
    {0xAA, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0xAA, 0xAA, 0x55},
    {0xFF, 0xAA, 0x55},
    {0xAA, 0xFF, 0x55},
    {0xFF, 0xFF, 0x55},
    {0x00, 0x00, 0xAA},
    {0x55, 0x00, 0xAA},
    {0x00, 0x55, 0xAA},
    {0x55, 0x55, 0xAA},
    {0x00, 0x00, 0xFF},
    {0x55, 0x00, 0xFF},
    {0x00, 0x55, 0xFF},
    {0x55, 0x55, 0xFF},
    {0xAA, 0x00, 0xAA},
    {0xFF, 0x00, 0xAA},
    {0xAA, 0x55, 0xAA},
    {0xFF, 0x55, 0xAA},
    {0xAA, 0x00, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0xAA, 0x55, 0xFF},
    {0xFF, 0x55, 0xFF},
    {0x00, 0xAA, 0xAA},
    {0x55, 0xAA, 0xAA},
    {0x00, 0xFF, 0xAA},
    {0x55, 0xFF, 0xAA},
    {0x00, 0xAA, 0xFF},
    {0x55, 0xAA, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0x55, 0xFF, 0xFF},
    {0xAA, 0xAA, 0xAA},
    {0xFF, 0xAA, 0xAA},
    {0xAA, 0xFF, 0xAA},
    {0xFF, 0xFF, 0xAA},
    {0xAA, 0xAA, 0xFF},
    {0xFF, 0xAA, 0xFF},
    {0xAA, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0x00},
    {0x44, 0x00, 0x00},
    {0x00, 0x44, 0x00},
    {0x44, 0x44, 0x00},
    {0x00, 0x00, 0x44},
    {0x44, 0x00, 0x44},
    {0x00, 0x44, 0x44},
    {0x44, 0x44, 0x44},
    {0x88, 0x00, 0x00},
    {0xCC, 0x00, 0x00},
    {0x88, 0x44, 0x00},
    {0xCC, 0x44, 0x00},
    {0x88, 0x00, 0x44},
    {0xCC, 0x00, 0x44},
    {0x88, 0x44, 0x44},
    {0xCC, 0x44, 0x44},
    {0x00, 0x88, 0x00},
    {0x44, 0x88, 0x00},
    {0x00, 0xCC, 0x00},
    {0x44, 0xCC, 0x00},
    {0x00, 0x88, 0x44},
    {0x44, 0x88, 0x44},
    {0x00, 0xCC, 0x44},
    {0x44, 0xCC, 0x44},
    {0x88, 0x88, 0x00},
    {0xCC, 0x88, 0x00},
    {0x88, 0xCC, 0x00},
    {0xCC, 0xCC, 0x00},
    {0x88, 0x88, 0x44},
    {0xCC, 0x88, 0x44},
    {0x88, 0xCC, 0x44},
    {0xCC, 0xCC, 0x44},
    {0x00, 0x00, 0x88},
    {0x44, 0x00, 0x88},
    {0x00, 0x44, 0x88},
    {0x44, 0x44, 0x88},
    {0x00, 0x00, 0xCC},
    {0x44, 0x00, 0xCC},
    {0x00, 0x44, 0xCC},
    {0x44, 0x44, 0xCC},
    {0x88, 0x00, 0x88},
    {0xCC, 0x00, 0x88},
    {0x88, 0x44, 0x88},
    {0xCC, 0x44, 0x88},
    {0x88, 0x00, 0xCC},
    {0xCC, 0x00, 0xCC},
    {0x88, 0x44, 0xCC},
    {0xCC, 0x44, 0xCC},
    {0x00, 0x88, 0x88},
    {0x44, 0x88, 0x88},
    {0x00, 0xCC, 0x88},
    {0x44, 0xCC, 0x88},
    {0x00, 0x88, 0xCC},
    {0x44, 0x88, 0xCC},
    {0x00, 0xCC, 0xCC},
    {0x44, 0xCC, 0xCC},
    {0x88, 0x88, 0x88},
    {0xCC, 0x88, 0x88},
    {0x88, 0xCC, 0x88},
    {0xCC, 0xCC, 0x88},
    {0x88, 0x88, 0xCC},
    {0xCC, 0x88, 0xCC},
    {0x88, 0xCC, 0xCC},
    {0xCC, 0xCC, 0xCC},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x44, 0xBB, 0x33},
    {0x77, 0xCC, 0x66},
    {0x55, 0x44, 0xFF},
    {0xBB, 0x77, 0xFF},
    {0xBB, 0x66, 0x44},
    {0x55, 0xCC, 0xEE},
    {0xDD, 0x66, 0x44},
    {0xFF, 0x88, 0x66},
    {0xCC, 0xCC, 0x44},
    {0xDD, 0xDD, 0x77},
    {0x33, 0x99, 0x22},
    {0xBB, 0x66, 0xCC},
    {0xCC, 0xCC, 0xCC},
    {0xFF, 0xFF, 0xFF},
    {0x11, 0x11, 0x11},
    {0x00, 0x11, 0x11},
    {0x55, 0xAA, 0x22},
    {0x66, 0xDD, 0x77},
    {0x44, 0x55, 0xEE},
    {0x99, 0x66, 0xEE},
    {0xAA, 0x77, 0x55},
    {0x44, 0xDD, 0xFF},
    {0xCC, 0x77, 0x55},
    {0xEE, 0x99, 0x77},
    {0xDD, 0xDD, 0x55},
    {0xCC, 0xCC, 0x66},
    {0x22, 0x88, 0x33},
    {0xAA, 0x77, 0xDD},
    {0xDD, 0xDD, 0xDD},
    {0xFF, 0xFF, 0xFF},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x22, 0xCC, 0x22},
    {0x66, 0xCC, 0x66},
    {0x22, 0x22, 0xEE},
    {0x44, 0x66, 0xEE},
    {0xAA, 0x22, 0x22},
    {0x44, 0xCC, 0xEE},
    {0xEE, 0x22, 0x22},
    {0xEE, 0x66, 0x66},
    {0xCC, 0xCC, 0x22},
    {0xCC, 0xCC, 0x88},
    {0x22, 0x88, 0x22},
    {0xCC, 0x44, 0xAA},
    {0xAA, 0xAA, 0xAA},
    {0xEE, 0xEE, 0xEE},
    {0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00},
    {0x22, 0xCC, 0x22},
    {0x66, 0xCC, 0x66},
    {0x22, 0x22, 0xEE},
    {0x44, 0x66, 0xEE},
    {0xAA, 0x22, 0x22},
    {0x44, 0xCC, 0xEE},
    {0xEE, 0x22, 0x22},
    {0xEE, 0x66, 0x66},
    {0xCC, 0xCC, 0x22},
    {0xCC, 0xCC, 0x88},
    {0x22, 0x88, 0x22},
    {0xCC, 0x44, 0xAA},
    {0xAA, 0xAA, 0xAA},
    {0xEE, 0xEE, 0xEE}};

static byte *vram = NULL;
typedef byte FONT[0x100][THEIGHT];
static FONT *font = NULL;

static int mfx_emu = 0;
static byte page = 0;
static byte palidx = 0;
static byte raddr = 0;
static byte treg[32];
static byte chr = 0;
static byte atr1 = 0;
static byte atr2 = 0;
static BOOLEAN blink = FALSE;
static BOOLEAN changed = FALSE;
static word taddr = 0;
static int iSer = 0;
static const char sSer[] = "ME\01\03\00\02\01x";
static byte fontidx = 0;
static byte fontrow = 0;
static byte byFPGA = 0;
static word vaddr = 0;
static byte vincr = 1;
static word caddr = 0;
static word ccntr = 0;

static VDP mfxvdp;
static byte *vdppix = NULL;

void mfx_init (int emu)
    {
    mfx_emu = emu;
    diag_message (DIAG_MFX_CFG, "mfx_init (%d)", mfx_emu);
    if ( mfx_emu == 0 ) return;
    if ( mfx_emu >= MFXEMU_MAX )
        {
        int ix;
        int iy;
        win_max_size (NULL, &ix, &iy);
        ix /= 640;
        iy /= 480;
        if ( ( ix <= 0 ) || ( iy <= 1 ) )   mfx_emu = 1;
        else if ( ix < iy )                 mfx_emu = ix;
        else                                mfx_emu = iy;
        }
    if ( vram == NULL ) vram = (byte *) emalloc (0x8000);
    memset (vram, 0, 0x8000);
    if ( font == NULL ) font = (FONT *) emalloc (sizeof (FONT));
    memcpy (font, mon_alpha_prom, sizeof (FONT));
    if ( vdppix == NULL ) vdppix = (byte *) emalloc (VDP_WIDTH * VDP_HEIGHT);
    mfxvdp.ram = &vram[0x4000];
    mfxvdp.pix = vdppix;
    mfx_win = win_create(
        WIDTH, HEIGHT,
        mfx_emu, mfx_emu,
        "MFX Video",
        NULL, /* display */
        NULL, /* geometry */
        mfx_pal, N_CLR_MFX,
        kbd_win_keypress,
        kbd_win_keyrelease
        );
    }

static void mfx_tupdate (void)
    {
    word mask;
    if (treg[31] & 0x01) mask = 0x0FFF;
    else                 mask = 0x07FF;
    word maddr = (taddr & mask) << 2;
    maddr |= (treg[31] & 0x60) << 8;
    if ( taddr & 0x8000 )
        {
        if ( taddr & 0x4000 ) vram[maddr] = chr;
        if ( taddr & 0x2000 ) vram[maddr+1] = atr1;
        if ( taddr & 0x1000 ) vram[maddr+2] = atr2;
        diag_message (DIAG_MFX_TEXT, "Write at %d:%4d: chr = '%c' (0x%02X), atr1 = 0x%02X, atr2 = 0x%02X, Flags = 0x%X"
            , maddr >> 13, taddr & mask, ((chr >= 0x20) && ( chr < 0x7F)) ? chr : '.', chr, atr1, atr2, taddr >> 12
            );
        changed = TRUE;
        }
    else
        {
        chr  = vram[maddr];
        atr1 = vram[maddr+1];
        atr2 = vram[maddr+2];
        diag_message (DIAG_MFX_TEXT, "Read at %d:%4d: chr = '%c' (0x%02X), atr1 = 0x%02X, atr2 = 0x%02X, Flags = 0x%X"
            , maddr >> 13, taddr & mask, ((chr >= 0x20) && ( chr < 0x7F)) ? chr : '.', chr, atr1, atr2, taddr >> 12
            );
        }
    }

byte mfx_in (word port)
    {
    byte value;
    port &= 0xFF;
    switch (port)
        {
        case 0x28:
            value = ccntr & 0xFF;
            break;
        case 0x29:
            value = ccntr >> 8;
            break;
        case 0x2A:
            value = caddr & 0xFF;
            break;
        case 0x2B:
            value = caddr >> 8;
            break;
        case 0x2C:
            value = vaddr & 0xFF;
            break;
        case 0x2D:
            value = vaddr >> 8;
            break;
        case 0x2E:
            value = vram[vaddr];
            vaddr = (vaddr + vincr) & 0x7FFF;
            value = value;
            break;
        case 0x2F:
            value = vram[vaddr];
            break;
        case 0x30:
            value = taddr & 0xFF;
            break;
        case 0x31:
            value = taddr >> 8;
            break;
        case 0x32:
            value = chr;
            break;
        case 0x33:
            value = atr1;
            break;
        case 0x34:
            value = 0;
            break;
        case 0x35:
            value = sSer[iSer];
            ++iSer;
            iSer &= 0x07;
            value = value;
            break;
        case 0x36:
            diag_message (DIAG_MFX_FONT, "Read font index = 0x%02X", fontidx);
            value = fontidx;
            break;
        case 0x37:
            value = (*font)[fontidx][fontrow];
            diag_message (DIAG_MFX_FONT, "Read font character 0X%02X row %d = 0x%02X", fontidx, fontrow, value);
            if ( ++fontrow >= THEIGHT )
                {
                fontrow = 0;
                ++fontidx;
                }
            break;
        case 0x38:
            value = raddr;
            break;
        case 0x39:
            value = treg[raddr];
            break;
        case 0x3A:
            value = byFPGA;
            break;
        case 0x3B:
            value = page;
            break;
        case 0x3C:
            value = palidx;
            break;
        case 0x3D:
            value = (mfx_pal[palidx].g & 0xF0) | (mfx_pal[palidx].r >> 4);
            break;
        case 0x3E:
            value = mfx_pal[palidx].b >> 4;
            break;
        case 0x3F:
            value = atr2;
            break;
        default:
            value = port;
        }
    diag_message (DIAG_MFX_PORT, "mfx_in (0x%02X) = 0x%02X", port, value);
    return value;
    }

void mfx_out (word port, byte value)
    {
    if ( mfx_emu == 0 ) return;
    port &= 0xFF;
    diag_message (DIAG_MFX_PORT, "mfx_out (0x%02X, 0x%02X)", port, value);
    switch (port)
        {
        case 0x00:
            page = value;
            break;
        case 0x01:
            vdp_out1 (&mfxvdp, value);
            break;
        case 0x02:
            vdp_out2 (&mfxvdp, value);
            break;
        case 0x28:
            ccntr = (ccntr & 0x7F00) | value;
            diag_message (DIAG_MFX_MEM, "Copy %d bytes from 0x%04X to 0x%04X with increment 0x%02X",
                ccntr, caddr, vaddr, vincr);
            while ( ccntr > 0 )
                {
                vram[vaddr] = vram[caddr];
                vaddr = (vaddr + vincr) & 0x7FFF;
                caddr = (caddr + vincr) & 0x7FFF;
                --ccntr;
                }
            break;
        case 0x29:
            ccntr = (((word)(value & 0x7F)) << 8) | (ccntr & 0x00FF);
            diag_message (DIAG_MFX_MEM, "Set VRAM copy counter high bits. Counter = %d", ccntr);
            break;
        case 0x2A:
            caddr = (caddr & 0x7F00) | value;
            diag_message (DIAG_MFX_MEM, "Set VRAM copy address low bits. Address = 0x%04X", caddr);
            break;
        case 0x2B:
            caddr = (((word)(value & 0x7F)) << 8) | (caddr & 0x00FF);
            diag_message (DIAG_MFX_MEM, "Set VRAM copy address high bits. Address = 0x%04X", caddr);
            break;
        case 0x2C:
            vaddr = (vaddr & 0x7F00) | value;
            diag_message (DIAG_MFX_MEM, "Set VRAM destination address low bits. Address = 0x%04X", vaddr);
            vincr = 1;
            break;
        case 0x2D:
            vaddr = (((word)(value & 0x7F)) << 8) | (vaddr & 0x00FF);
            diag_message (DIAG_MFX_MEM, "Set VRAM destination address high bits. Address = 0x%04X", vaddr);
            vincr = 1;
            break;
        case 0x2E:
            vram[vaddr] = value;
            diag_message (DIAG_MFX_MEM, "Write vram[0x%04X] = 0x%02X. Address incremented by 0x%02X",
                vaddr, value, vincr);
            changed = TRUE;
            vaddr = (vaddr + vincr) & 0x7FFF;
            break;
        case 0x2F:
            vincr = value;
            diag_message (DIAG_MFX_MEM, "Set vram address increment = 0x%02X", vincr);
            break;
        case 0x30:
            diag_message (DIAG_MFX_TEXT, "80 column low address = 0x%02X", value);
            taddr = (taddr & 0xFF00) | value;
            mfx_tupdate ();
            break;
        case 0x31:
            diag_message (DIAG_MFX_TEXT, "80 column high address = 0x%02X", value);
            taddr = (((word) value) << 8 ) | (taddr & 0xFF);
            break;
        case 0x32:
            diag_message (DIAG_MFX_TEXT, "Character = '%c' (0x%02X)", ((value >= 0x20) && (value < 0x7F)) ? value : '.', value);
            chr = value;
            break;
        case 0x33:
            diag_message (DIAG_MFX_TEXT, "Character attribute = 0x%02X", value);
            atr1 = value;
            break;
        case 0x34:
            diag_message (DIAG_MFX_TEXT, "Character repeat = 0x%02X", value);
            for (int i = 0; i <= value; ++i)
                {
                mfx_tupdate ();
                ++taddr;
                }
            break;
        case 0x35:
            iSer = value & 0x07;
            break;
        case 0x36:
            fontidx = value;
            fontrow = 0;
            diag_message (DIAG_MFX_FONT, "Set font index = 0x%02X", value);
            break;
        case 0x37:
            (*font)[fontidx][fontrow] = value;
            changed = TRUE;
            diag_message (DIAG_MFX_FONT, "Set font character 0x%02X row %d = 0x%02X", fontidx, fontrow, value);
            if ( ++fontrow >= THEIGHT )
                {
                fontrow = 0;
                ++fontidx;
                }
            break;
        case 0x38:
            raddr = value & 0x1F;
            break;
        case 0x39:
            treg[raddr] = value;
            diag_message (DIAG_MFX_CFG, "MFX register %d = 0x%02X", raddr, value);
            changed = TRUE;
            break;
        case 0x3A:
            byFPGA = value;
            diag_message (DIAG_MFX_CFG, "FPGA configuration = 0x%02X", byFPGA);
            break;
        case 0x3C:
            palidx = value;
            break;
        case 0x3D:
            {
            byte red = value & 0x0F;
            byte green = value >> 4;
            mfx_pal[palidx].r = 0x11 * red;
            mfx_pal[palidx].g = 0x11 * green;
            changed = TRUE;
            win_colour (mfx_win, palidx, &mfx_pal[palidx]);
            diag_message (DIAG_MFX_PAL, "Palette entry %d: Red = 0x%X, Green = 0x%X", palidx, red, green);
            break;
            }
        case 0x3E:
            mfx_pal[palidx].b = 0x11 * (value & 0x0F);
            changed = TRUE;
            win_colour (mfx_win, palidx, &mfx_pal[palidx]);
            diag_message (DIAG_MFX_PAL, "Palette entry %d: Blue = 0x%X", palidx, value & 0x0F);
            break;
        case 0x3F:
            diag_message (DIAG_MFX_TEXT, "Character attribute 2 = 0x%02X", value);
            atr2 = value;
            break;
        }
    }

static byte grapix (byte ch, int k)
    {
    byte pix[4] = { 0x00, 0xF0, 0x0F, 0xFF };
    switch (k)
        {
        case 3:
        case 4:
            ch >>= 2;
            break;
        case 5:
        case 6:
            ch >>= 4;
            break;
        case 7:
        case 8:
        case 9:
            ch >>= 6;
        }
    return pix[ch & 0x03];
    }

static void s0_refresh (BOOLEAN b48)
    {
    int thgt;
    int nrow;
    int nppr;
    word base;
    word mask;
    if ( b48 )
        {
        nrow = 48;
        nppr = WIDTH;
        base = (((word) (treg[0x1F] & 0x40)) << 8);
        mask = 0x3FFF;
        }
    else
        {
        nrow = 24;
        nppr = 2 * WIDTH;
        base = (((word) (treg[0x1F] & 0x60)) << 8);
        mask = 0x1FFF;
        }
    word addr = ((((word) treg[0x0C]) << 8) | treg[0x0D]) << 2;
    addr = (addr & mask) | base;
    word csr =  ((((word) treg[0x0E]) << 8) | treg[0x0F]) << 2;
    csr = (csr & mask) | base;
    BOOLEAN csron;
    switch (treg[0x0A] & 0x60)
        {
        case 0x00: csron = TRUE; break;
        case 0x20: csron = FALSE; break;
        case 0x40: csron = blink; break;
        case 0x60: csron = blink; break;
        }
    // printf ("addr = 0x%04X, csr = 0x%04X, treg[0x0A] = 0x%02X, treg[0x0B] = 0x%02X, csron = %s\n",
    //    addr, csr, treg[0x0A], treg[0x0B], csron ? "Yes" : "No");
    byte *data1 = mfx_win->data;
    for (int i = 0; i < nrow; ++i)
        {
        byte *data2 = data1;
        // printf ("Row %d: data1 = %ld\n", i, data1 - mfx_win->data);
        for (int j = 0; j < TCOLS; ++j)
            {
            // printf ("Column %d: data2 = %ld\n", j, data2 - mfx_win->data);
            byte *data3 = data2;
            byte ch = vram[addr];
            byte a1 = vram[addr+1];
            byte a2 = vram[addr+2];
            byte bgnd;
            byte fgnd;
            if ( treg[0x1E] & 0x02 )
                {
                fgnd = a1 & 0x3F;
                bgnd = a2 & 0x3F;
                }
            else
                {
                fgnd = ((a1 & 0x07) << 3) | (a2 & 0x07);
                bgnd = (a1 & 0x38) | ((a2 & 0x38) >> 3);
                }
            fgnd |= treg[0x1E] & 0xC0;
            bgnd |= treg[0x1E] & 0xC0;
            // printf ("addr = %d, ch = 0x%02X, a1 = 0x%02X, a2 = 0x%02X, fgnd = 0x%02X, bgnd = 0x%02X\n",
            //    addr, ch, a1, a2, fgnd, bgnd);
            for (int k = 0; k < THEIGHT; ++k)
                {
                // printf ("Scan %d: data3 = %ld\n", k, data3 - mfx_win->data);
                byte *data4 = data3;
                byte pix;
                if (csron && (addr == csr) && (k >= (treg[0x0A] & 0x1F)) && (k <= (treg[0x0B] & 0x1F)))
                    {
                    // if (k == (treg[0x0A] & 0x1F)) printf ("Cursor on\n");
                    pix = 0xFF;
                    fgnd |= 0x3F;
                    }
                else
                    {
                    if (a1 & 0x80)  pix = grapix (ch, k);
                    else            pix = (*font)[ch][k];
                    if ((a1 & 0x40) && blink) pix ^= 0xFF;
                    }
                // printf ("pix = 0x%02X\n", pix);
                for (int l = 0; l < TWIDTH; ++l)
                    {
                    if ( pix & 0x80 )   *data4 = fgnd;
                    else                *data4 = bgnd;
                    // printf ("Pixel %d: data4 = %ld, *data4 = 0x%02X\n", l, data4 - mfx_win->data, *data4);
                    pix <<= 1;
                    ++data4;
                    }
                data3 += nppr;
                }
            data2 += TWIDTH;
            addr = ((addr + 4) & mask) | base;
            // if ( addr == csr ) printf ("Cursor row\n");
            }
        if ( b48 )
            {
            data1 += THEIGHT * WIDTH;
            }
        else
            {
            for (int k = 0; k < THEIGHT; ++k)
                {
                memcpy (data1 + WIDTH, data1, WIDTH);
                // printf ("Copy from %ld to %ld\n", data1 - mfx_win->data, data1 + WIDTH - mfx_win->data);
                data1 += 2 * WIDTH;
                }
            }
        }
    // printf ("addr = 0x%04X\n", addr);
    }

static void s2_refresh (void)
    {
    word base = (((word) (treg[0x1F] & 0x40)) << 8);
    word mask = 0x3FFF;
    word addr = ((((word) treg[0x0C]) << 8) | treg[0x0D]) << 4;
    addr = (addr & mask) | base;
    word csr =  ((((word) treg[0x0E]) << 8) | treg[0x0F]) << 4;
    csr == (csr & mask) | base;
    BOOLEAN csron;
    switch (treg[0x0A] & 0x60)
        {
        case 0x00: csron = TRUE; break;
        case 0x20: csron = FALSE; break;
        case 0x40: csron = blink; break;
        case 0x60: csron = blink; break;
        }
    byte *data1 = mfx_win->data;
    for (int i = 0; i < GROWS; ++i)
        {
        // printf ("Row %d: data1 = %ld\n", i, data1 - mfx_win->data);
        byte *data2 = data1;
        for (int j = 0; j < GCOLS; ++j)
            {
            // printf ("Column %d: data2 = %ld\n", j, data2 - mfx_win->data);
            byte *data3 = data2;
            byte bgnd = vram[addr+10];
            byte fgnd = vram[addr+11];
            for (int k = 0; k < THEIGHT; ++k)
                {
                // printf ("Scan %d: data3 = %ld\n", k, data3 - mfx_win->data);
                byte *data4 = data3;
                if ( treg[0x1E] & 0x01 )
                    {
                    if ( i & 0x01 )
                        {
                        if ( k >= 6 )
                            {
                            bgnd = vram[addr+14];
                            fgnd = vram[addr+15];
                            }
                        else if ( k >= 2 )
                            {
                            bgnd = vram[addr+12];
                            fgnd = vram[addr+13];
                            }
                        }
                    else
                        {
                        if ( k >= 8 )
                            {
                            bgnd = vram[addr+14];
                            fgnd = vram[addr+15];
                            }
                        else if ( k >= 4 )
                            {
                            bgnd = vram[addr+12];
                            fgnd = vram[addr+13];
                            }
                        }
                    }
                byte pix = vram[addr + k];
                if (csron && (addr == csr) && (k >= (treg[0x0A] * 0x1F)) && (k <= (treg[0x0B] & 0x1F)))
                    {
                    pix ^= 0xFF;
                    }
                for (int l = 0; l < TWIDTH; ++l)
                    {
                    if ( pix & 0x80 )
                        {
                        *data4 = fgnd;
                        *(data4+1) = fgnd;
                        }
                    else
                        {
                        *data4 = bgnd;
                        *(data4+1) = bgnd;
                        }
                    // printf ("Pixel %d: data4 = %ld, *data4 = 0x%02X\n", l, data4 - mfx_win->data, *data4);
                    pix <<= 1;
                    data4 += 2;
                    }
                data3 += 2 * WIDTH;
                }
            data2 += GWIDTH;
            addr = ((addr + 16) & mask) | base;
            }
        for (int k = 0; k < THEIGHT; ++k)
            {
            // printf ("Copy from %ld to %ld\n", data1 - mfx_win->data, data1 + WIDTH - mfx_win->data);
            memcpy (data1 + WIDTH, data1, WIDTH);
            data1 += 2 * WIDTH;
            }
        }
    }

static void s4_refresh (void)
    {
    byte pal[4];
    word mask = 0x7FFF;
    word addr = ((((word) treg[0x0C]) << 8) | treg[0x0D]) << 5;
    addr = addr & mask;
    word csr =  ((((word) treg[0x0E]) << 8) | treg[0x0F]) << 5;
    csr == csr & mask;
    BOOLEAN csron;
    switch (treg[0x0A] & 0x60)
        {
        case 0x00: csron = TRUE; break;
        case 0x20: csron = FALSE; break;
        case 0x40: csron = blink; break;
        case 0x60: csron = blink; break;
        }
    byte *data1 = mfx_win->data;
    for (int i = 0; i < GROWS; ++i)
        {
        byte *data2 = data1;
        for (int j = 0; j < GCOLS; ++j)
            {
            byte *data3 = data2;
            memcpy (pal, &vram[addr+20], 4);
            for (int k = 0; k < THEIGHT; ++k)
                {
                if ( treg[0x1E] & 0x01 )
                    {
                    if ( i & 0x01 )
                        {
                        if ( k >= 6 )
                            {
                            memcpy (pal, &vram[addr+28], 4);
                            }
                        else if ( k >= 2 )
                            {
                            memcpy (pal, &vram[addr+24], 4);
                            }
                        }
                    else
                        {
                        if ( k >= 8 )
                            {
                            memcpy (pal, &vram[addr+28], 4);
                            }
                        else if ( k >= 4 )
                            {
                            memcpy (pal, &vram[addr+24], 4);
                            }
                        }
                    byte *data4 = data3;
                    word pix = (((word) vram[addr + 2 * k]) << 8) | vram[addr + 2 * k + 1];
                    if (csron && (addr == csr) && (k >= (treg[0x0A] * 0x1F)) && (k <= (treg[0x0B] & 0x1F)))
                        {
                        pix ^= 0xFFFF;
                        }
                    for (int l = 0; l < GWIDTH; ++l)
                        {
                        *data4 = pal[(pix >> 14) & 0x03];
                        *(data4+1) = *data4;
                        pix <<= 2;
                        data4 += 2;
                        }
                    }
                data3 += 2 * WIDTH;
                }
            data2 += GWIDTH;
            addr = (addr + 32) & mask;
            }
        for (int k = 0; k < THEIGHT; ++k)
            {
            memcpy (data1 + WIDTH, data1, WIDTH);
            data1 += 2 * WIDTH;
            }
        }
    }

static void s8_refresh (void)
    {
    // printf ("s8_refresh\n");
    byte pal[8];
    word addr = (treg[0x0d] & 0x7F) << 8;
    word csr =  (((word) treg[0x0E]) << 8) | treg[0x0F];
    byte *data = mfx_win->data;
    word posn = 0;
    word save = 0;
    byte scan = 0;
    BOOLEAN csron;
    switch (treg[0x0A] & 0x60)
        {
        case 0x00: csron = TRUE; break;
        case 0x20: csron = FALSE; break;
        case 0x40: csron = blink; break;
        case 0x60: csron = blink; break;
        }
    for  (int i = 0; i < HEIGHT / 2; ++i)
        {
        if ( scan == 0 )    save = posn;
        else                posn = save;
        // printf ("Row %3d addr = 0x%04X\n", i, addr);
        memcpy (pal, &vram[addr + 120], 8);
        // printf ("Palette: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
        //     pal[0], pal[1], pal[2], pal[3], pal[4], pal[5], pal[6], pal[7]);
        for (int j = 0; j < WIDTH / 16; ++j)
            {
            int blk = (vram[addr] << 16) | (vram[addr+1] << 8) | vram[addr+2];
            if (csron && (posn == csr) && (scan >= (treg[0x0A] * 0x1F)) && (scan <= (treg[0x0B] & 0x1F)))
                {
                blk ^= 0xFFFFFF;
                }
            // printf ("Block %3d: addr = 0x%04X, blk = 0x%06X, data = %ld, pixels =", j, addr, blk, data - mfx_win->data);
            for (int k = 0; k < 8; ++k)
                {
                byte pix = pal[(blk >> 21) & 0x07];
                // printf (" 0x%02X", pix);
                *data = pix;
                ++data;
                *data = pix;
                ++data;
                blk <<= 3;
                }
            // printf ("\n");
            addr += 3;
            }
        // printf ("Copy from %ld to %ld\n", data - mfx_win->data - WIDTH, data - mfx_win->data);
        memcpy (data, data - WIDTH, WIDTH);
        data += WIDTH;
        addr = (addr + 8) & 0x7FFF;
        if ( ++scan == 10 ) scan = 0;
        }
    }

void mfx_refresh (void)
    {
    if ( mfx_emu == 0 ) return;
    if ( byFPGA & 0x40 )
        {
        if ( mfxvdp.changed )
            {
            // diag_message (DIAG_MFX, "MFX refresh in VDP mode");
            vdp_refresh (&mfxvdp);
            byte pal = 0xC0 | (byFPGA & 0x20);
            byte *vdp_data = vdppix;
            memset (mfx_win->data, pal, WIDTH * VDP_YORG);
            byte *mfx_data = mfx_win->data + WIDTH * VDP_YORG;
            for ( int i = 0; i < VDP_HEIGHT; ++i )
                {
                memset (mfx_data, pal, VDP_XORG);
                mfx_data += VDP_XORG;
                for ( int j = 0; j < VDP_WIDTH; ++j )
                    {
                    byte pix = pal | (*vdp_data);
                    *mfx_data = pix;
                    *(mfx_data + WIDTH) = pix;
                    ++mfx_data;
                    *mfx_data = pix;
                    *(mfx_data + WIDTH) = pix;
                    ++mfx_data;
                    ++vdp_data;
                    }
                memset (mfx_data, pal, WIDTH - VDP_XORG - 2 * VDP_WIDTH);
                mfx_data += WIDTH - VDP_XORG - 2 * VDP_WIDTH;
                memcpy (mfx_data, mfx_data - WIDTH, WIDTH);
                mfx_data += WIDTH;
                }
            memset (mfx_data, pal, (HEIGHT - VDP_YORG - 2 * VDP_HEIGHT) * WIDTH);
            }
        }
    else if (changed)
        {
        // diag_message (DIAG_MFX, "MFX refresh in mode 0x%02X", treg[0x1F]);
        switch (treg[0x1F] & 0x0F)
            {
            case 0x00:
            case 0x01:
                s0_refresh (treg[0x1F] & 0x01);
                break;
            case 0x02:
                s2_refresh ();
                break;
            case 0x04:
                s4_refresh ();
                break;
            case 0x08:
                s8_refresh ();
                break;
            }
        changed = FALSE;
        }
    win_refresh (mfx_win);
    }

void mfx_blink (void)
    {
	BOOLEAN new_blink = ( (get_millis()%1000) > 500 );
    if ( new_blink != blink )
        {
        blink = ! blink;
        changed = TRUE;
        mfx_refresh ();
        }
    }

void mfx_show (void)
    {
    if (mfx_win != NULL) win_show (mfx_win);
    }
