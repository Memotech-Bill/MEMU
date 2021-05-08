/* vga.c - Emulation of Propeller VGA display */

#include "vga.h"
#include "monprom.h"
#include "win.h"
#include "vid.h"
#include "kbd.h"
#include "common.h"
#include "diag.h"

#define WIDTH       640
#define HEIGHT      480
#define TWIDTH      8
#define THEIGHT     20
#define ROWS        ( HEIGHT / THEIGHT )
#define COLS        ( WIDTH / TWIDTH )
#define GWIDTH      ( 2 * TWIDTH )
#define GCOLS       ( WIDTH / GWIDTH )
#define GHEIGHT     10
#define GSPAN       3
#define NCOLOUR     64
#define BSIZE       ( ( ROWS + 1 ) * COLS )             /* Size of text mode buffer */
#define GSIZE       ( GSPAN * ( ROWS + 1 ) * GCOLS )    /* Size of graphics mode buffer */
#define MSIZE       4096U   /* Big enough for BSIZE, MSIZE, and VDP memory */
#define N_VS        8
#define TBLINK      0x20
#define TCURSOR     0x10
#define NRESET      5

#define gmode       ( 1 << ( 16 + 8 ) )
#define uscore      ( 1 << ( 16 + 9 ) )
#define invers      ( 1 << ( 16 + 10 ) )
#define blink       ( 1 << ( 16 + 11 ) )
#define cursor      ( 1 << ( 16 + 12 ) )
#define xorplt      ( 1 << ( 16 + 13 ) )

#define csNormal    0
#define csAlternate 1
#define csSpecial   2

#define mdCompat    0
#define mdMono      1
#define mdEnhan     2
#define mdGraph     3
#define mdVDP       4

#define wmBoth      0
#define wmChar      1
#define wmAttr      2

static WIN *vga_win = NULL;
static unsigned int btop = 0;
static unsigned int *buffer = NULL;
static unsigned int iTime = 0;
static enum s_state {
    stNormal,   //  Normal character state
    stXPos,     //  Next byte defines X position
    stYPos,     //  Next byte defines Y position
    stBackgd,   //  Next byte defines background colour
    stForegd,   //  Next byte defines foreground colour
    stPatChr,   //  Next byte specifies character to redefine
    stPatDat,   //  Bytes redefine font pattern
    stEscape,   //  Escape sequence
    stEscCont,  //  Emulate a control code
    stPAttr,    //  Set print attributes
    stNAttr,    //  Set non-print attributes
    stBAttr,    //  Set both attributes
    stPBit,     //  Set bit of print attributes
    stNBit,     //  Set bit of non-print attributes
    stBBit,     //  Set bit of bith attributes
    stWMask,    //  Set write mask
    stPlot,     //  Plot a point
    stDraw,     //  Draw a line
    st8BLen,    //  Get length of 8-bit data
    st8BData,   //  Read 8-bit data
    stRawLen,   //  Get length of raw data
    stRawData,  //  Read raw buffer data
    stCRVS,     //  Define a virtual screen
    stVS,       //  Select a virtual screen
    stBlank,    //  Output space characters
    stCopy      //  Copy characters
    } state;

static const char *psState[] = {
    "stNormal",   //  Normal character state
    "stXPos",     //  Next byte defines X position
    "stYPos",     //  Next byte defines Y position
    "stBackgd",   //  Next byte defines background colour
    "stForegd",   //  Next byte defines foreground colour
    "stPatChr",   //  Next byte specifies character to redefine
    "stPatDat",   //  Bytes redefine font pattern
    "stEscape",   //  Escape sequence
    "stEscCont",  //  Emulate a control code
    "stPAttr",    //  Set print attributes
    "stNAttr",    //  Set non-print attributes
    "stBAttr",    //  Set both attributes
    "stPBit",     //  Set bit of print attributes
    "stNBit",     //  Set bit of non-print attributes
    "stBBit",     //  Set bit of bith attributes
    "stWMask",    //  Set write mask
    "stPlot",     //  Plot a point
    "stDraw",     //  Draw a line
    "st8BLen",    //  Get length of 8-bit data
    "st8BData",   //  Read 8-bit data
    "stRawLen",   //  Get length of raw data
    "stRawData",  //  Read raw buffer data
    "stCRVS",     //  Define a virtual screen
    "stVS"        //  Select a virtual screen
    "stBlank",    //  Output space characters
    "stCopy"      //  Copy characters
    };

static byte mode = mdCompat;

struct s_vscr               // Virtual screen properties
    {
    unsigned int    pattr;  // Print attributes
    unsigned int    nattr;  // Non-print attributes
    byte            wwth;   // Width
    byte            whgt;   // Height
    byte            wxorg;  // Position of left
    byte            wyorg;  //  top corner
    byte            wscr;   // Screen or window scroll
    char            xpos;   // Cursor column
    char            ypos;   // Cursor row
    byte            csron;  // Display cursor
    byte            page;   // Page mode
    byte            cset;   // Current character set
    byte            wm;     // Write mask
    byte            vs;     // Virtual screen number
    };

static struct s_vscr vs[N_VS];
static struct s_vscr vs_def =
    { 0x00002000, 0x00002000, COLS, ROWS, 0, 0, 1, 0, 0, 1, 0, csNormal, wmBoth, 0 };
static struct s_vscr *active = &vs[0];
#define NREG    28

#define NPARAM  20
static byte par[NPARAM];
static unsigned int pcntr;
static unsigned int pdata;

#define NGLYPH  256
#define NGRAPH  256
#define NFONT   ( NGLYPH + NGRAPH )
static byte (*vga_font)[THEIGHT] = NULL;

static byte vdpclr[16] =
    { 0x00, 0x00, 0x09, 0x0E, 0x30, 0x38, 0x02, 0x3D,
      0x03, 0x07, 0x1B, 0x1F, 0x04, 0x22, 0x26, 0x3F };

static enum { rbZero, rbText, rbGraph, rbReg, rbFont, rbVDPRAM, rbVDPReg } rbmode = rbZero;
static unsigned int rrow = 0;
static unsigned int rcol = 0;
static unsigned int raddr = 0;
static unsigned int nwait = NRESET;
static byte bout = 0;

#ifndef WIN32
#define min(x,y)    ( x < y ? x : y )
#endif

static byte bit_reverse[256] =
    {
    0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0, 0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8, 0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4, 0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC, 0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2, 0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA, 0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6, 0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE, 0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1, 0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9, 0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5, 0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED, 0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3, 0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB, 0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7, 0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF, 0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF};

/* Variables used for VDP Shadow emulation */

/*...svars:0:*/
#define	VBORDER 8
#define	HBORDER256 8
#define	HBORDER240 16
#define	VDP_WIDTH  (HBORDER256+256+HBORDER256)
#define	VDP_HEIGHT (VBORDER   +192+   VBORDER)
#define VDP_XORG    ( WIDTH / 2 - VDP_WIDTH )
#define VDP_YORG    ( HEIGHT / 2 - VDP_HEIGHT )

static byte vgavdp_pix[VDP_WIDTH*VDP_HEIGHT];
static byte vgavdp_regs[8];
static byte vgavdp_regs_zeros[8] = { 0xfc,0x04,0xf0,0x00,0xf8,0x80,0xf8,0x00 };
#define	VGAVDP_MEMORY_SIZE 0x4000
static byte vgavdp_spr_lines[192];
static word vgavdp_addr;
static BOOLEAN vgavdp_read_mode;
static int vgavdp_last_mode;

static BOOLEAN vgavdp_latched = FALSE;
static byte vgavdp_latch = 0;

/* Forward definition of functions used for VDP mode emulation */
static void vgavdp_init (void);
static void vgavdp_refresh (void);

void vga_reset (void)
    {
    unsigned int i, j;
    diag_message (DIAG_VGA_MODE, "VGA Reset");
    for ( i = 0; i < N_VS; ++i )
        {
        memcpy (&vs[i], &vs_def, sizeof (vs_def));
        vs[i].vs = i;
        }
    active = &vs[0];
    byte bg = ( vs[0].nattr >> 2 ) & 0x3F;
    memset (vga_win->data, bg, WIDTH * HEIGHT);
    unsigned int blank = vs[0].nattr | 0x200000;
    for ( i = 0; i < BSIZE; ++i ) buffer[i] = blank;
    mode = mdCompat;
    btop = 0;
    if ( vga_font == NULL )
        {
        vga_font = emalloc (NFONT * THEIGHT);
        diag_message (DIAG_VGA_MODE, "vga_font = %p", vga_font);
        }
    for ( i = 0; i < NGLYPH; ++i )
        {
        for ( j = 0; j < GLYPH_HEIGHT; ++j )
            {
            byte pix = bit_reverse[mon_alpha_prom[i][j]];
            vga_font[i][2*j] = pix;
            vga_font[i][2*j+1] = pix;
            }
        }
    for ( i = 0; i < NGRAPH; ++i )
        {
        byte chr = i;
        for ( j = 0; j < 4; ++j )
            {
            byte pix = 0;
            if ( chr & 0x01 ) pix |= 0x0F;
            if ( chr & 0x02 ) pix |= 0xF0;
            vga_font[i+NGLYPH][5*j] = pix;
            vga_font[i+NGLYPH][5*j+1] = pix;
            vga_font[i+NGLYPH][5*j+2] = pix;
            vga_font[i+NGLYPH][5*j+3] = pix;
            vga_font[i+NGLYPH][5*j+4] = pix;
            chr >>= 2;
            }
        }
    rrow = 0;
    rcol = 0;
    raddr = 0;
    rbmode = rbZero;
    bout = 0x61;
    nwait = NRESET;
    }

void vga_init (void)
    {
    COL clr[NCOLOUR];
    int i;
    diag_message (DIAG_VGA_MODE, "VGA Initialise");
    buffer = (unsigned int *) emalloc (MSIZE * sizeof (unsigned int));
    diag_message (DIAG_VGA_MODE, "VGA Buffer Initialised: buffer = %p", buffer);
    for ( i = 0; i < NCOLOUR; ++i )
        {
        clr[i].r = 0x55 * ( i & 0x03 );
        clr[i].g = 0x55 * ( ( i & 0x0C ) >> 2 );
        clr[i].b = 0x55 * ( ( i & 0x30 ) >> 4 );
        }
    vga_win = win_create (
        WIDTH, HEIGHT,
        1, 1,
        "VGA Display",
        NULL, NULL,
        clr, NCOLOUR,
        kbd_win_keypress, kbd_win_keyrelease);
    diag_message (DIAG_VGA_MODE, "VGA window created");
    vga_reset ();
    diag_message (DIAG_VGA_MODE, "VGA Initialise completed");
    }

void vga_term (void)
    {
    diag_message (DIAG_VGA_MODE, "VGA Terminate");
    if ( vga_win != NULL ) win_delete (vga_win);
    vga_win = NULL;
    if ( vga_font != NULL )
        {
        diag_message (DIAG_VGA_MODE, "vga_term: vga_font = %p", vga_font);
        free (vga_font);
        vga_font = NULL;
        }
    if ( buffer != NULL )
        {
        diag_message (DIAG_VGA_MODE, "vga_term: buffer = %p", buffer);
        free (buffer);
        buffer = NULL;
        }
    }

static unsigned int abits[4][8] = {
    { 0x0C00, 0x3000, 0xC000, 0x000C, 0x0030, 0x00C0, blink, gmode },
    { uscore,      0, 0x1000,      0, invers, 0x0010, blink, gmode },
    { uscore,      0,      0,      0, invers, xorplt, blink, gmode },
    { uscore,      0,      0,      0, invers, xorplt, blink, gmode }};

static unsigned int vga_attr (byte b)
    {
    unsigned int attr = ( mode == mdMono ) ? 0x2000 : 0;
    int i;
    for ( i = 0; i < 8; ++i )
        {
        if ( b & 0x01 ) attr |= abits[mode][i];
        b >>= 1;
        }
    return attr;
    }

static void vga_put_glyph (int iRow, int iCol, unsigned int iGlyph)
    {
    byte bg = ( iGlyph >> 2 ) & 0x3F;
    byte fg = ( iGlyph >> 10 ) & 0x3F;
    int iCh = ( iGlyph >> 16 ) & 0x1FF;
    byte *ptr = vga_win->data + WIDTH * THEIGHT * iRow + TWIDTH * iCol;
    int i, j;
    if ( COLS * iRow + iCol >= ROWS * COLS ) return;
    for ( i = 0; i < THEIGHT; ++i )
        {
        byte scan = vga_font[iCh][i];
        if ( iGlyph & invers ) scan ^= 0xFF;
        if ( ( iTime & TBLINK ) && ( iGlyph & blink ) ) scan ^= 0xFF;
        if ( i >= THEIGHT - 2 )
            {
            if ( iGlyph & uscore ) scan ^= 0xFF;
            if ( ( iTime & TCURSOR ) && ( iGlyph & cursor ) ) scan ^= 0xFF;
            }
        for ( j = 0; j < TWIDTH; ++j )
            {
            if ( ( ptr < vga_win->data ) || ( ptr >= vga_win->data + WIDTH * HEIGHT ) )
                {
                diag_message (DIAG_ALWAYS, "ptr = %p outside [%p, %p)", ptr, vga_win->data,
                    vga_win->data + WIDTH * HEIGHT);
                diag_message (DIAG_ALWAYS, "iRow = %d, iCol = %d, i = %d, j = %d", iRow, iCol, i, j);
                terminate ("Memory error");
                }
            if ( scan & 0x01 )  *ptr = fg;
            else                *ptr = bg;
            ++ptr;
            scan >>= 1;
            }
        ptr += WIDTH - TWIDTH;
        }
    }

static void vga_put_graph (int iRow, int iCol, const unsigned int *graph)
    {
    unsigned int i, j;
    byte *ptr = vga_win->data + WIDTH * THEIGHT * iRow + GWIDTH * iCol;
    byte bg = ( graph[0] >> 2 ) & 0x3F;
    byte fg = ( graph[0] >> 10 ) & 0x3F;
    byte bits, clr;
    const byte *pix = (const byte *) graph + 2;
    if ( COLS * iRow + iCol >= ROWS * COLS ) return;
    for ( i = 0; i < GHEIGHT; ++i )
        {
        bits = *pix;
        for ( j = 0; j < TWIDTH; ++j )
            {
            if ( bits & 0x01 )  clr = fg;
            else                clr = bg;
            *ptr = clr;
            *(ptr + 1) = clr;
            *(ptr + WIDTH) = clr;
            *(ptr + WIDTH + 1) = clr;
            ptr += 2;
            bits >>= 1;
            }
        ptr += 2 * WIDTH - GWIDTH;
        ++pix;
        }
    }

static void vga_blank (int iRow, int iCol, int n)
    {
    unsigned int addr;
    unsigned int attr;
    int i;
    if ( mode == mdGraph )
        {
        attr = active->nattr & 0xFFFF;
        addr = btop + GSPAN * ( GCOLS * iRow + iCol );
        if ( addr >= GSIZE ) addr -= GSIZE;
        for ( i = 0; i < n; ++i )
            {
            if ( active->wm == wmBoth )
                {
                buffer[addr] = attr;
                buffer[addr+1] = 0;
                buffer[addr+2] = 0;
                }
            else if ( active->wm == wmAttr )
                {
                buffer[addr] = ( buffer[addr] & 0xFFFF0000 ) | active->nattr;
                }
            else
                {
                buffer[addr] &= 0xFFFF;
                buffer[addr+1] = 0;
                buffer[addr+2] = 0;
                }
            vga_put_graph (iRow, iCol, &buffer[addr]);
            addr += GSPAN;
            if ( addr >= GSIZE ) addr -= GSIZE;
            if ( ++iCol >= GCOLS )
                {
                iCol = 0;
                ++iRow;
                }
            }
        }
    else
        {
        addr = btop + COLS * iRow + iCol;
        if ( addr >= BSIZE ) addr -= BSIZE;
        attr = active->nattr | 0x200000;
        for ( i = 0; i < n; ++i )
            {
            if ( active->wm == wmBoth )         buffer[addr] = attr;
            else if ( active->wm == wmAttr )    buffer[addr] = ( buffer[addr] & 0x01FF0000 ) | active->nattr;
            else                                buffer[addr] = ( buffer[addr] & 0xFE00FFFF ) | 0x200000;
            vga_put_glyph (iRow, iCol, buffer[addr]);
            if ( ++addr >= BSIZE ) addr = 0;
            if ( ++iCol >= COLS )
                {
                iCol = 0;
                ++iRow;
                }
            }
        }
    }

static void vga_mask_write (int iRow, int iCol, byte ch, unsigned int attr)
    {
    if ( mode == mdGraph )
        {
        // diag_message (DIAG_ALWAYS,"Graphics char 0x%02X ('%c'), attr = 0x%08X",
        //    ch, (((ch>=0x20) && (ch<0x7F))? ch : '.'), attr);
        unsigned int addr = btop + GSPAN * ( GCOLS * iRow + iCol );
        if ( addr >= GSIZE ) addr -= GSIZE;
        if ( active->wm != wmChar ) buffer[addr] = (buffer[addr] & 0xFFFF0000) | (attr & 0xFFFF);
        if ( active->wm != wmAttr )
            {
            byte *pix = (byte *) &buffer[addr] + 2;
            byte *font = vga_font[ch];
            for (int i = 0; i < GHEIGHT; ++i)
                {
                byte pat = *font;
                if ( attr & invers ) pat = ~pat;
                if ( attr & xorplt ) *pix ^= pat;
                else                 *pix = pat;
                ++pix;
                font += 2;
                }
            }
        vga_put_graph (iRow, iCol, &buffer[addr]);
        }
    else
        {
        unsigned int addr = btop + COLS * iRow + iCol;
        if ( addr >= BSIZE ) addr -= BSIZE;
        if ( active->wm == wmBoth )
            {
            buffer[addr] = ((unsigned long) ch << 16) | attr;
            }
        else if ( active->wm == wmChar )
            {
            buffer[addr] = ( buffer[addr] & 0xFF00FFFF ) | ((unsigned long) ch << 16);
            }
        else if ( active->wm == wmAttr )
            {
            buffer[addr] = ( buffer[addr] & 0x01FF0000 ) | attr;
            }
        vga_put_glyph (iRow, iCol, buffer[addr]);
        }
    }

static void vga_move_chars (int iRow1, int iCol1, int iRow2, int iCol2, int nCh)
    {
    if ( mode == mdGraph )
        {
        unsigned int addr1 = btop + GSPAN * (GCOLS * iRow1 + iCol1);
        unsigned int addr2 = btop + GSPAN * (GCOLS * iRow2 + iCol2);
        byte *ptr1 = vga_win->data + THEIGHT * WIDTH * iRow1 + GWIDTH * iCol1;
        byte *ptr2 = vga_win->data + THEIGHT * WIDTH * iRow2 + GWIDTH * iCol2;
        if ( addr1 >= GSIZE ) addr1 -= GSIZE;
        if ( addr2 >= GSIZE ) addr2 -= GSIZE;
        if ( active->wm == wmBoth )
            {
            unsigned int i;
            memmove (&buffer[addr2], &buffer[addr1], GSPAN * nCh * sizeof (buffer[0]));
            for ( i = 0; i < THEIGHT; ++i )
                {
                memmove (ptr2, ptr1, GWIDTH * nCh);
                ptr1 += WIDTH;
                ptr2 += WIDTH;
                }
            }
        else
            {
            while ( nCh > 0 )
                {
                if ( active->wm == wmAttr )
                    {
                    buffer[addr2] = (buffer[addr2] & 0xFFFF0000) | (buffer[addr1] & 0xFFFF);
                    }
                else
                    {
                    buffer[addr2] = (buffer[addr2] & 0xFFFF) | (buffer[addr1] & 0xFFFF0000);
                    buffer[addr2+1] = buffer[addr1+1];
                    buffer[addr2+2] = buffer[addr1+2];
                    }
                vga_put_graph (iRow2, iCol2, &buffer[addr2]);
                addr1 += GSPAN;
                addr2 += GSPAN;
                ++iCol2;
                --nCh;
                }
            }
        }
    else
        {
        unsigned int addr1 = btop + COLS * iRow1 + iCol1;
        unsigned int addr2 = btop + COLS * iRow2 + iCol2;
        byte *ptr1 = vga_win->data + THEIGHT * WIDTH * iRow1 + TWIDTH * iCol1;
        byte *ptr2 = vga_win->data + THEIGHT * WIDTH * iRow2 + TWIDTH * iCol2;
        if ( addr1 >= BSIZE ) addr1 -= BSIZE;
        if ( addr2 >= BSIZE ) addr2 -= BSIZE;
        if ( active->wm == wmBoth )
            {
            unsigned int i;
            memmove (&buffer[addr2], &buffer[addr1], nCh * sizeof (buffer[0]));
            for ( i = 0; i < THEIGHT; ++i )
                {
                memmove (ptr2, ptr1, TWIDTH * nCh);
                ptr1 += WIDTH;
                ptr2 += WIDTH;
                }
            }
        else
            {
            while ( nCh > 0 )
                {
                if ( active->wm == wmAttr )
                    {
                    buffer[addr2] = (buffer[addr2] & 0x01FF0000) | (buffer[addr1] & 0xFE00FFFF);
                    }
                else
                    {
                    buffer[addr2] = (buffer[addr2] & 0xFE00FFFF) | (buffer[addr1] & 0x01FF0000);
                    }
                vga_put_glyph (iRow2, iCol2, buffer[addr2]);
                ++addr1;
                ++addr2;
                ++iCol2;
                --nCh;
                }
            }
        }
    }

static void vga_insert_line (int iRow)
    {
    int i;
    for ( i = active->whgt - 1; i > iRow; --i )
        {
        vga_move_chars (active->wyorg + i - 1, active->wxorg, active->wyorg + i, active->wxorg,
            active->wwth);
        }
    vga_blank (active->wyorg + iRow, active->wxorg, active->wwth);
    }

static void vga_delete_line (int iRow)
    {
    unsigned int i;
    for ( i = iRow + 1; i < active->whgt; ++i )
        {
        vga_move_chars (active->wyorg + i, active->wxorg, active->wyorg + i - 1, active->wxorg,
            active->wwth);
        }
    vga_blank (active->wyorg + active->whgt - 1, active->wxorg, active->wwth);
    }

static void vga_scroll (void)
    {
    if ( active->wscr )
        {
        if ( mode == mdGraph )
            {
            btop += GSPAN * GCOLS;
            if ( btop >= GSIZE ) btop -= GSIZE;
            memmove (vga_win->data, vga_win->data + THEIGHT * WIDTH, ( ROWS - 1 ) * THEIGHT * WIDTH);
            vga_blank (ROWS - 1, 0, GCOLS);
            }
        else
            {
            btop += COLS;
            if ( btop >= BSIZE ) btop -= BSIZE;
            memmove (vga_win->data, vga_win->data + THEIGHT * WIDTH, ( ROWS - 1 ) * THEIGHT * WIDTH);
            vga_blank (ROWS - 1, 0, COLS);
            }
        }
    else
        {
        vga_delete_line (0);
        }
    }

static void vga_copy_chars (int iRow1, int iCol1, int iRow2, int iCol2, int nCh)
    {
    int nCol = ( mode == mdGraph ) ? GCOLS : COLS;
    if ( nCh == 0 ) nCh = 256;
    if ( ( nCol * iRow2 + iCol2 < nCol * iRow1 + iCol1 )
        || ( nCol * iRow2 + iCol2 >= nCol * iRow1 + iCol1 + nCh ) )
        {
        while ( nCh > 0 )
            {
            int nMove = nCol - (( iCol1 >= iCol2 ) ? iCol1 : iCol2);
            if ( nMove > nCh ) nMove = nCh;
            vga_move_chars (iRow1, iCol1, iRow2, iCol2, nMove);
            iCol1 += nMove;
            if ( iCol1 >= nCol )
                {
                iCol1 -= nCol;
                if ( ++iRow1 >= ROWS ) break;
                }
            iCol2 += nMove;
            if ( iCol2 >= nCol )
                {
                iCol2 -= nCol;
                if ( ++iRow1 >= ROWS ) break;
                }
            nCh -= nMove;
            }
        }
    else
        {
        iCol1 += nCol * iRow1;
        iCol2 += nCol * iRow2;
        if ( iCol1 + nCh > ROWS * nCol ) nCh = ROWS * nCol - iCol1;
        if ( iCol2 + nCh > ROWS * nCol ) nCh = ROWS * nCol - iCol2;
        iCol1 += nCh;
        iCol2 += nCh;
        iRow1 = iCol1 / nCol;
        iRow2 = iCol2 / nCol;
        iCol1 -= nCol * iRow1;
        iCol2 -= nCol * iRow2;
        while ( nCh > 0 )
            {
            if ( --iCol1 < 0 )
                {
                --iRow1;
                iCol1 = nCol - 1;
                }
            if ( --iCol2 < 0 )
                {
                --iRow2;
                iCol2 = nCol - 2;
                }
            vga_move_chars (iRow1, iCol1, iRow2, iCol2, 1);
            --nCh;
            }
        }
    }

static void vga_csrdown (void)
    {
    if ( ++active->ypos >= active->whgt )
        {
        if ( active->page )
            {
            active->ypos = 0;
            }
        else
            {
            vga_scroll ();
            active->ypos = active->whgt - 1;
            }
        }
    }

static void vga_csrright (void)
    {
    if ( ++active->xpos >= active->wwth )
        {
        active->xpos = 0;
        vga_csrdown ();
        }
    }

static void vga_addchr (byte ch)
    {
    vga_mask_write (active->wyorg + active->ypos, active->wxorg + active->xpos, ch, active->pattr);
    vga_csrright ();
    }

static void vga_plot (int iX, int iY)
    {
    int attr = active->nattr;
    if ( ( mode <= mdMono ) && ( ( attr & 0xFC00 ) == 0 ) ) attr |= invers;
    if ( mode == mdGraph )
        {
        if ( ( iX >= 0 ) && ( iX < TWIDTH * active->wwth ) && ( iY >= 0 ) && ( iY < GHEIGHT * active->whgt ) )
            {
            byte pix = 1 << ( iX & (TWIDTH - 1) );
            unsigned int ofs = iY % GHEIGHT + 2;
            int iRow = active->wyorg + iY / GHEIGHT;
            int iCol = active->wxorg + iX / TWIDTH;
            unsigned int addr = btop + GSPAN * ( GCOLS * iRow + iCol );
            if ( addr >= GSIZE ) addr -= GSIZE;
            if ( active->wm != wmAttr )
                {
                switch ( attr & ( invers | xorplt ) )
                    {
                    case 0:         *((byte *) &buffer[addr] + ofs) |= pix;     break;
                    case invers:    *((byte *) &buffer[addr] + ofs) &= ~ pix;   break;
                    case xorplt:    *((byte *) &buffer[addr] + ofs) ^= pix;     break;
                    }
                }
            if ( active->wm != wmChar )
                {
                buffer[addr] = ( buffer[addr] & 0xFFFF0000 ) | ( active->nattr & 0xFFFF );
                }
            vga_put_graph (iRow, iCol, &buffer[addr]);
            }
        }
    else
        {
        if ( ( iX >= 0 ) && ( iX < 2 * active->wwth ) && ( iY >= 0 ) && ( iY < 4 * active->whgt ) )
            {
            unsigned int gchr;
            unsigned int gpix = 1 << ( ( ( iX & 0x01 ) | ( ( iY & 0x03 ) << 1 ) ) + 16 );
            int iRow = active->wyorg + iY / 4;
            int iCol = active->wxorg + iX / 2;
            unsigned int addr = btop + COLS * iRow + iCol;
            if ( addr >= BSIZE ) addr -= BSIZE;
            gchr = buffer[addr] & 0x1FF0000;
            if ( ! ( gchr & gmode ) ) gchr = gmode;
            switch ( attr & ( invers | xorplt ) )
                {
                case 0:         gchr |= gpix;   break;
                case invers:    gchr &= ~ gpix; break;
                case xorplt:    gchr ^= gpix;   break;
                }
            attr &= ~ invers;
            switch ( active->wm )
                {
                case wmBoth:    buffer[addr] = gchr | attr;                             break;
                case wmChar:    buffer[addr] = ( buffer[addr] & 0xFE00FFFF ) | gchr;    break;
                case wmAttr:    buffer[addr] = ( buffer[addr] & 0x01FF0000 ) | attr;    break;
                }
            vga_put_glyph (iRow, iCol, buffer[addr]);
            }
        }
    }

static void vga_draw (int iX1, int iY1, int iX2, int iY2)
    {
    int iTmp;
    int n;
    // diag_message (DIAG_ALWAYS, "Draw (%d, %d, %d, %d)", iX1, iY1, iX2, iY2);
    if ( iX2 < iX1 )
        {
        iTmp = iX1;
        iX1 = iX2;
        iX2 = iTmp;
        iTmp = iY1;
        iY1 = iY2;
        iY2 = iTmp;
        }
    if ( mode == mdGraph )
        {
        if ( ( iX1 >= TWIDTH * active->wwth ) || ( ( iY1 >= GHEIGHT * active->whgt )
                && ( iY2 >= GHEIGHT * active->whgt ) ) ) return;
        }
    else
        {
        if ( ( iX1 >= 2 * active->wwth ) || ( ( iY1 >= 4 * active->whgt )
                && ( iY2 >= 4 * active->whgt ) ) ) return;
        }
    iX2 -= iX1;
    iY2 -= iY1;
    vga_plot (iX1, iY1);
    if ( iY2 > iX2 )
        {
        iTmp = iY2 / 2;
        n = iY2;
        // diag_message (DIAG_ALWAYS, "Mode 1: n = %d", n);
        while ( n )
            {
            ++iY1;
            iTmp += iX2;
            if ( iTmp >= iY2 )
                {
                ++iX1;
                iTmp -= iY2;
                }
            vga_plot (iX1, iY1);
            // diag_message (DIAG_ALWAYS, "   (%d, %d)", iX1, iY1);
            --n;
            }
        }
    else if ( iY2 >= 0 )
        {
        iTmp = iX2 / 2;
        n = iX2;
        // diag_message (DIAG_ALWAYS, "Mode 2: n = %d", n);
        while ( n )
            {
            ++iX1;
            iTmp += iY2;
            if ( iTmp >= iX2 )
                {
                ++iY1;
                iTmp -= iX2;
                }
            vga_plot (iX1, iY1);
            // diag_message (DIAG_ALWAYS, "   (%d, %d)", iX1, iY1);
            --n;
            }
        }
    else if ( iY2 >= ( -iX2 ) )
        {
        iY2 = - iY2;
        iTmp = iX2 / 2;
        n = iX2;
        // diag_message (DIAG_ALWAYS, "Mode 3: n = %d", n);
        while ( n )
            {
            ++iX1;
            iTmp += iY2;
            if ( iTmp >= iX2 )
                {
                --iY1;
                iTmp -= iX2;
                }
            vga_plot (iX1, iY1);
            // diag_message (DIAG_ALWAYS, "   (%d, %d)", iX1, iY1);
            --n;
            }
        }
    else
        {
        iY2 = - iY2;
        iTmp = iY2 / 2;
        n = iY2;
        // diag_message (DIAG_ALWAYS, "Mode 4: n = %d", n);
        while ( n )
            {
            --iY1;
            iTmp += iX2;
            if ( iTmp >= iY2 )
                {
                ++iX1;
                iTmp -= iY2;
                }
            vga_plot (iX1, iY1);
            // diag_message (DIAG_ALWAYS, "   (%d, %d)", iX1, iY1);
            --n;
            }
        }
    }

static void vga_set_mode (int md)
    {
    diag_message (DIAG_VGA_MODE, "VGA Set Mode: %d", md);
    if ( md == -1 )
        {
        mode = mdVDP;
        memset (vga_win->data, 0, WIDTH * HEIGHT);
        rbmode = rbZero;
        raddr = 0;
        bout = 0x61;
        nwait = NRESET;
        vgavdp_init ();
        vga_out2 (0x08);
        vga_out2 (0x87);
        }
    else if ( md == mdGraph )
        {
        if ( mode == mdGraph ) return;
        mode = mdGraph;
        active->wwth /= 2;
        active->wxorg /= 2;
        }
    else if ( mode == mdGraph )
        {
        mode = md;
        active->wwth *= 2;
        active->wxorg *= 2;
        }
    else
        {
        mode = md;
        return;
        }
    btop = 0;
    active->xpos = 0;
    active->ypos = 0;
    vga_blank (0, 0, ( mode == mdGraph ) ? GSIZE / GSPAN : BSIZE);
    }
    
void vga_out60 (byte chr)
    {
    static unsigned int addr = 0;
    unsigned int attr;
    diag_message(DIAG_VGA_PORT, "VGA write to port 0x60: 0x%02X ('%c'), state = %s",
        chr, (((chr >= 0x20) && (chr < 0x7F))? chr : '.'), psState[state]);
    if ( mode == mdVDP ) return;
    if ( ( active->csron ) && ( state == stNormal ) )
        {
        if ( mode == mdGraph )
            {
            addr = btop + GSPAN * ( GCOLS * ( active->wyorg + active->ypos ) + active->wxorg + active->xpos );
            if ( addr >= GSIZE ) addr -= GSIZE;
            buffer[addr+2] ^= 0xFF0000;
            }
        else
            {
            addr = btop + COLS * ( active->wyorg + active->ypos ) + active->wxorg + active->xpos;
            if ( addr >= BSIZE ) addr -= BSIZE;
            buffer[addr] &= ~ cursor;
            vga_put_glyph (active->wyorg + active->ypos, active->wxorg + active->xpos, buffer[addr]);
            }
        }
    if ( state == stEscCont )
        {
        chr &= 0x1F;
        state = stNormal;
        }
    switch ( state )
        {
        case stNormal:
            if ( chr >= 0x20 )
                {
                switch ( active->cset )
                    {
                    case csNormal:
                        vga_addchr (chr);
                        break;
                    case csAlternate:
                        vga_addchr (chr | 0x80);
                        break;
                    case csSpecial:
                        chr &= 0x7F;
                        if ( chr & 0x40 )
                            {
                            if ( chr & 0x20 ) chr |= 0x80;
                            chr &= 0x9F;
                            }
                        vga_addchr (chr);
                        break;
                    }
                }
            else if ( chr == 0x0D )
                {
                active->xpos = 0;
                }
            else if ( chr == 0x0A )
                {
                vga_csrdown ();
                }
            else if ( chr == 0x09 )
                {
                active->xpos |= 0x07;
                vga_csrright ();
                }
            else if ( chr == 0x08 )
                {
                --active->xpos;
                if ( active->xpos < 0 )
                    {
                    if ( active->ypos > 0 )
                        {
                        --active->ypos;
                        active->xpos = active->wwth - 1;
                        }
                    else
                        {
                        active->xpos = 0;
                        }
                    }
                }
            else if ( chr == 0x0B )
                {
                if ( active->ypos > 0 ) --active->ypos;
                }
            else if ( chr == 0x19 )
                {
                vga_csrright ();
                }
            else if ( chr == 0x1A )
                {
                active->xpos = 0;
                active->ypos = 0;
                }
            else if ( chr == 0x06 )
                {
                state = stForegd;
                }
            else if ( chr == 0x04 )
                {
                state = stBackgd;
                }
            else if ( chr == 0x03 )
                {
                state = stXPos;
                }
            else if ( ( chr & 0xF8 ) == 0x10 )
                {
                if ( mode == mdMono )
                    {
                    if ( chr & 0x01 )   active->pattr |= uscore;
                    else                active->pattr &= ~ uscore;
                    if ( chr & 0x04 )   active->pattr |= 0x3000;
                    else                active->pattr &= ~ 0x1000;
                    }
                else
                    {
                    active->pattr &= 0xFF0000FF;
                    if ( chr & 0x01 )   active->pattr |= 0x0C00;
                    if ( chr & 0x02 )   active->pattr |= 0x3000;
                    if ( chr & 0x04 )   active->pattr |= 0xC000;
                    }
                }
            else if ( chr == 0x05 )
                {
                vga_blank (active->wyorg + active->ypos, active->wxorg + active->xpos,
                    active->wwth - active->xpos);
                }
            else if ( chr == 0x0C )
                {
                if ( ( active->wscr ) && ( active->wm == wmBoth ) )
                    {
                    btop = 0;
                    if ( mode == mdGraph )
                        {
                        attr = active->nattr & 0xFFFF;
                        while ( addr < GSIZE )
                            {
                            buffer[addr] = attr;
                            buffer[addr+1] = 0;
                            buffer[addr+2] = 0;
                            addr += 3;
                            }
                        }
                    else
                        {
                        attr = active->nattr | 0x200000;
                        for ( addr = 0; addr < BSIZE; ++addr ) buffer[addr] = attr;
                        }
                    memset (vga_win->data, ( active->nattr & 0xFC ) >> 2, WIDTH * HEIGHT);
                    }
                else
                    {
                    int iRow;
                    if ( active->wscr ) btop = 0;
                    for ( iRow = active->wyorg; iRow < active->wyorg + active->whgt; ++iRow )
                        vga_blank (iRow, active->wxorg, active->wwth);
                    }
                active->xpos = 0;
                active->ypos = 0;
                }
            else if ( chr == 0x18 )
                {
                active->page = 0;
                if ( mode == mdMono )   active->pattr = 0x2000;
                else                    active->pattr = 0x3000;
                active->nattr = active->pattr;
                active->csron = 1;
                active->xpos = 0;
                vga_csrdown ();
                active->cset = csNormal;
                }
            else if ( chr == 0x0E )
                {
                active->pattr |= blink;
                }
            else if ( chr == 0x0F )
                {
                active->pattr &= ~ blink;
                }
            else if ( chr == 0x1E )
                {
                active->csron = 1;
                }
            else if ( chr == 0x1F )
                {
                active->csron = 0;
                }
            else if ( chr == 0x1C )
                {
                active->page = 0;
                }
            else if ( chr == 0x1D )
                {
                active->page = 1;
                }
            else if ( chr == 0x1B )
                {
                state = stEscape;
                }
            else if ( chr == 0x01 )
                {
                state = stPlot;
                pcntr = ( mode == mdGraph ) ? 3 : 2;
                }
            else if ( chr == 0x02 )
                {
                state = stDraw;
                pcntr = ( mode == mdGraph ) ? 6 : 4;
                }
            break;
        case stXPos:
            if ( ( chr >= 32 ) && ( chr < active->wwth + 32 ) ) active->xpos = chr - 32;
            state = stYPos;
            break;
        case stYPos:
            if ( ( chr >= 32 ) && ( chr < active->whgt + 32 ) ) active->ypos = chr - 32;
            state = stNormal;
            break;
        case stForegd:
            if ( ( mode == mdEnhan ) || ( mode == mdGraph ) )
                {
                attr = (unsigned int)( chr & 0x3F ) << 10;
                if ( chr < 0x40 )
                    {
                    active->pattr = ( active->pattr & 0xFF0000FF ) | attr;
                    active->nattr = ( active->nattr & 0xFF0000FF ) | attr;
                    }
                else if ( chr < 0x80 )
                    {
                    active->pattr = ( active->pattr & 0xFF0000FF ) | attr;
                    }
                else if ( chr < 0xC0 )
                    {
                    active->nattr = ( active->nattr & 0xFF0000FF ) | attr;
                    }
                }
            else
                {
                active->pattr = vga_attr (chr);
                active->nattr = active->pattr;
                }
            state = stNormal;
            break;
        case stBackgd:
            switch (mode)
                {
                case mdCompat:
                    attr = 0;
                    if ( chr & 0x01 )   attr |= 0x0C;
                    if ( chr & 0x02 )   attr |= 0x30;
                    if ( chr & 0x04 )   attr |= 0xC0;
                    active->pattr = ( active->pattr & 0xFF00FF00 ) | attr;
                    active->nattr = ( active->nattr & 0xFF00FF00 ) | attr;
                    break;
                case mdMono:
                    active->pattr &= 0xFF00FF00;
                    active->pattr &= 0xFF00FF00;
                    if ( chr & 0x02 )
                        {
                        active->pattr |= invers;
                        active->nattr |= invers;
                        }
                    else
                        {
                        active->pattr &= ~ invers;
                        active->nattr &= ~ invers;
                        }
                    if ( chr & 0x04 )
                        {
                        active->pattr |= 0x10;
                        active->nattr |= 0x10;
                        }
                    break;
                default:
                    attr = (unsigned int)( chr & 0x3F ) << 2;
                    if ( chr < 0x40 )
                        {
                        active->pattr = ( active->pattr & 0xFF00FF00 ) | attr;
                        active->nattr = ( active->nattr & 0xFF00FF00 ) | attr;
                        }
                    else if ( chr < 0x80 )
                        {
                        active->pattr = ( active->pattr & 0xFF00FF00 ) | attr;
                        }
                    else if ( chr < 0xC0 )
                        {
                        active->nattr = ( active->nattr & 0xFF00FF00 ) | attr;
                        }
                    break;
                }
            state = stNormal;
            break;
        case stEscape:
            state = stNormal;
            if ( ( chr >= 0x9B ) && ( chr <= 0x9F ) )
                {
                vga_set_mode ((int)chr - 0x9C);
                }
            else if ( chr >= 0x20 )
                {
                switch ( ( chr & 0x1F ) | 0x40 )
                    {
                    case 'A':
                        active->cset = csAlternate;
                        break;
                    case 'B':
                        state = stBBit;
                        break;
                    case 'C':
                        active->page = 0;
                        break;
                    case 'D':
                        active->page = 1;
                        break;
                    case 'E':
                        active->csron = 1;
                        break;
                    case 'F':
                        active->csron = 0;
                        break;
                    case 'G':
                        active->cset = csSpecial;
                        break;
                    case 'H':
                        {
                        unsigned int iRow = active->wyorg + active->ypos;
                        if ( active->xpos < active->wwth - 1 )
                            {
                            unsigned int iCol = active->wxorg + active->xpos;
                            vga_move_chars (iRow, iCol + 1, iRow, iCol, active->wwth - active->xpos - 1);
                            }
                        vga_blank (iRow, active->wxorg + active->wwth - 1, 1);
                        }
                        break;
                    case 'I':
                        vga_insert_line (active->wyorg + active->ypos);
                        vga_blank (active->wyorg + active->ypos, active->wxorg, active->wwth);
                        break;
                    case 'J':
                        vga_delete_line (active->wyorg + active->ypos);
                        vga_blank (active->wyorg + active->whgt - 1, active->wxorg, active->wwth);
                        break;
                    case 'K':
                        vga_insert_line (active->wyorg + active->ypos);
                        break;
                    case 'L':
                        break;
                    case 'M':
                        state = stPatChr;
                        pcntr = 2;
                        break;
                    case 'N':
                        state = stNBit;
                        break;
                    case 'O':
                        state = stVS;
                        break;
                    case 'P':
                        state = stPBit;
                        break;
                    case 'Q':
                        state = st8BLen;
                        break;
                    case 'R':
                        state = stRawLen;
                        break;
                    case 'S':
                        active->cset = csNormal;
                        break;
                    case 'T':
                        state = stPAttr;
                        break;
                    case 'U':
                        state = stNAttr;
                        break;
                    case 'V':
                        state = stBAttr;
                        break;
                    case 'W':
                        state = stWMask;
                        break;
                    case 'X':
                        state = stEscCont;
                        break;
                    case 'Y':
                        pcntr = 5;
                        state = stCRVS;
                        break;
                    case 'Z':
                        vga_reset ();
                        break;
                    case '_':
                        state = stBlank;
                        break;
                    case '^':
                        state = stCopy;
                        pcntr = 5;
                        break;
                    }
                }
            break;
        case stCopy:
            par[--pcntr] = chr;
            if ( pcntr == 0 )
                {
                int swth = ( mode == mdGraph ? 40 : 80 );
                if ( par[3] >= swth ) par[3] = swth - 1;
                if ( par[2] >= ROWS ) par[2] = ROWS - 1;
                if ( par[1] >= swth ) par[1] = swth - 1;
                if ( par[0] >= ROWS ) par[0] = ROWS - 1;
                vga_copy_chars (par[2], par[3], par[0], par[1], par[4]);
                state = stNormal;
                }
            break;
        case stBlank:
            vga_blank (active->ypos, active->xpos, chr);
            state = stNormal;
            break;
        case stPAttr:
            attr = vga_attr (chr);
            if (( mode == mdEnhan ) || ( mode == mdGraph ))
                {
                active->pattr = ( active->pattr & 0xFFFF ) | ( attr & 0xFF000000 );
                }
            else
                {
                active->pattr = attr;
                }
            // diag_message (DIAG_ALWAYS, "Print attributes: chr = 0x%02X, attr = 0x%08X, pattr = 0x%08X",
            //    chr, attr, active->pattr);
            state = stNormal;
            break;
        case stNAttr:
            attr = vga_attr (chr);
            if (( mode == mdEnhan ) || ( mode == mdGraph ))
                {
                active->nattr = ( active->nattr & 0xFFFF ) | ( attr & 0xFF000000 );
                }
            else
                {
                active->nattr = attr;
                }
            state = stNormal;
            // diag_message (DIAG_ALWAYS, "Plot attributes: chr = 0x%02X, attr = 0x%08X, nattr = 0x%08X",
            //    chr, attr, active->nattr);
            break;
        case stBAttr:
            attr = vga_attr (chr);
            if (( mode == mdEnhan ) || ( mode == mdGraph ))
                {
                active->pattr = ( active->pattr & 0xFFFF ) | ( attr & 0xFF000000 );
                active->nattr = ( active->nattr & 0xFFFF ) | ( attr & 0xFF000000 );
                }
            else
                {
                active->pattr = attr;
                active->nattr = attr;
                }
            state = stNormal;
            break;
        case stPBit:
            if ( chr == 0 )
                {
                if ( mode == mdCompat )     active->pattr = 0;
                else if ( mode == mdMono )  active->pattr = 0x2000;
                else                        active->pattr &= 0xFFFF;
                }
            else
                {
                active->pattr |= abits[mode][(chr - 1) & 0x07];
                }
            state = stNormal;
            break;
        case stNBit:
            if ( chr == 0 )
                {
                if ( mode == mdCompat )     active->nattr = 0;
                else if ( mode == mdMono )  active->nattr = 0x2000;
                else                        active->nattr &= 0xFFFF;
                }
            else
                {
                active->nattr |= abits[mode][(chr - 1) & 0x07];
                }
            state = stNormal;
            break;
        case stBBit:
            if ( chr == 0 )
                {
                if ( mode == mdCompat )
                    {
                    active->pattr = 0;
                    active->nattr = 0;
                    }
                else if ( mode == mdMono )
                    {
                    active->pattr = 0x2000;
                    active->nattr = 0x2000;
                    }
                else
                    {
                    active->pattr &= 0xFFFF;
                    active->nattr &= 0xFFFF;
                    }
                }
            else
                {
                active->pattr |= abits[mode][(chr - 1) & 0x07];
                active->nattr |= abits[mode][(chr - 1) & 0x07];
                }
            state = stNormal;
            break;
        case stWMask:
            if ( ( chr >= '0' ) && ( chr <= '2' ) ) active->wm = chr - '0';
            state = stNormal;
            break;
        case stPlot:
            par[--pcntr] = chr;
            if ( pcntr == 0 )
                {
                if ( mode == mdGraph )  vga_plot (256 * par[1] + par[2], par[0]);
                else                    vga_plot ((par[1] - 32) & 0xFF, (par[0] - 32) & 0xFF);
                state = stNormal;
                }
            break;
        case stDraw:
            par[--pcntr] = chr;
            if ( pcntr == 0 )
                {
                if ( mode == mdGraph )
                    {
                    vga_draw (256 * par[4] + par[5], par[3], 256 * par[1] + par[2], par[0]);
                    }
                else
                    {
                    vga_draw ((par[3] - 32) & 0xFF, (par[2] - 32) & 0xFF,
                        (par[1] - 32) & 0xFF, (par[0] - 32) & 0xFF);
                    }
                state = stNormal;
                }
            break;
        case stPatChr:
            par[--pcntr] = chr;
            if ( pcntr == 0 )
                {
                pdata = ( ( par[0] & 0x01 ) << 8 ) | par[1];
                pcntr = ( par[0] & 0x70 ) >> 4;
                par[0] = ( par[0] & 0x0E ) >> 1;
                if ( par[0] > 4 ) par[0] = 4;
                if ( pcntr > ( 5U - par[0] ) ) pcntr = 5 - (int) par[0];
                par[0] <<= 2;
                pcntr <<= 2;
                state = stPatDat;
                }
            break;
        case stPatDat:
            vga_font[pdata][par[0]] = chr;
            ++par[0];
            if ( --pcntr == 0 ) state = stNormal;
            break;
        case st8BLen:
            pcntr = chr;
            state = st8BData;
            break;
        case st8BData:
            vga_addchr (chr);
            if ( --pcntr == 0 ) state = stNormal;
            break;
        case stRawLen:
            pcntr = 0;
            pdata = chr;
            if ( mode == mdGraph )
                {
                addr = btop + GSPAN * ( GCOLS * ( active->wyorg + active->ypos )
                    + active->wxorg + active->xpos );
                if ( addr >= GSIZE ) addr -= GSIZE;
                }
            else
                {
                addr = btop + COLS * ( active->wyorg + active->ypos )
                    + active->wxorg + active->xpos;
                if ( addr >= BSIZE ) addr -= BSIZE;
                // diag_message (DIAG_ALWAYS,
                //     "Raw position: btop = %d, org = (%d, %d), pos = (%d, %d), addr = %d",
                //     btop, active->wxorg, active->wyorg, active->xpos, active->ypos, addr);
                }
            state = stRawData;
            break;
        case stRawData:
            par[pcntr] = chr;
            ++pcntr;
            if ( mode == mdGraph )
                {
                if ( pcntr == 12 )
                    {
                    int offset = addr - btop;
                    if ( offset < 0 ) offset += GSIZE;
                    memcpy (&buffer[addr], par, 12);
                    vga_put_graph (( offset / 3 ) / GCOLS, ( offset / 3 ) % GCOLS, &buffer[addr]);
                    pcntr = 0;
                    addr += 3;
                    if ( addr >= GSIZE ) addr -= GSIZE;
                    if ( --pdata == 0 ) state = stNormal;
                    }
                }
            else
                {
                if ( pcntr == 4 )
                    {
                    int offset = addr - btop;
                    if ( offset < 0 ) offset += BSIZE;
                    memcpy (&buffer[addr], par, 4);
                    // diag_message (DIAG_ALWAYS, "Raw write: buffer[%4d] = 0x%08X, Row = %d, Col = %d",
                    //     addr, buffer[addr], offset / COLS, offset % COLS);
                    vga_put_glyph (offset / COLS, offset % COLS, buffer[addr]);
                    pcntr = 0;
                    if ( ++addr >= BSIZE ) addr -= BSIZE;
                    if ( --pdata == 0 ) state = stNormal;
                    }
                }
            break;
        case stCRVS:
            par[--pcntr] = chr;
            if ( pcntr == 0 )
                {
                struct s_vscr *pvs = &vs[par[4] & 0x07];
                pvs->wxorg = min (par[1], COLS - 1);
                pvs->wyorg = min (par[0], ROWS - 1);
                pvs->wwth = min (par[3], COLS - pvs->wxorg);
                pvs->whgt = min (par[2], ROWS - pvs->wyorg);
                state = stNormal;
                }
            break;
        case stVS:
            if ( mode == mdGraph )
                {
                active->wxorg *= 2;
                active->xpos *= 2;
                }
            active = &vs[chr & 0x07];
            if ( mode == mdGraph )
                {
                active->wxorg /= 2;
                active->xpos /= 2;
                }
            state = stNormal;
            break;
        case stEscCont:
            break;  // Never reached but required to keep compiler happy.
        }
    if ( ( active->csron ) && ( state == stNormal ) )
        {
        if ( mode == mdGraph )
            {
            addr = btop + GSPAN * ( GCOLS * ( active->wyorg + active->ypos ) + active->wxorg + active->xpos );
            if ( addr >= GSIZE ) addr -= GSIZE;
            buffer[addr+2] ^= 0xFF0000;
            }
        else if ( mode != mdVDP )
            {
            addr = btop + COLS * ( active->wyorg + active->ypos ) + active->wxorg + active->xpos;
            if ( addr >= BSIZE ) addr -= BSIZE;
            buffer[addr] |= cursor;
            }
        }
    diag_message(DIAG_VGA_PORT, "VGA exit port 0x60: state = %s, cursor = (%d, %d)", psState[state],
        active->xpos, active->ypos);
    }

byte vga_in60 (void)
    {
    byte b = 0;
    if ( mode == mdVDP ) b = 0x60;
    else if ( nwait > 0 )
        {
        b = 0x60;
        --nwait;
        }
    diag_message(DIAG_VGA_PORT, "VGA read from port 0x60: 0x%02X", b);
    return b;
    }

static byte vga_getram (unsigned int addr)
    {
    return *((byte *)buffer + addr);
    }

static byte vga_getreg (unsigned int addr)
    {
    static byte bVern[3] = {20, 5, 8};
    if (addr == 0)
        {
        return  mode;
        }
    else if (addr <= 3)
        {
        return bVern[addr-1];
        }
    else if (addr == 4)
        {
        return state;
        }
    else if (addr == 5)
        {
        return (mode == mdGraph) ? 40 : 80;
        }
    else if (addr == 6)
        {
        return (mode == mdGraph) ? 12 : 4;
        }
    else if (addr == 7)
        {
        return btop / ((mode == mdGraph) ? GSPAN * GCOLS : COLS);
        }
    else if (addr < 8 + sizeof (struct s_vscr))
        {
        return ((byte *)active)[addr-8];
        }
    return 0;
    }

byte vga_next61 (void)
    {
    static byte vgavdp_vern[8] = { 0x80, 20, 4, 14, 0, 0, 0, 0 };
    byte b = 0;
    switch (rbmode)
        {
        case rbZero:
            b = 0;
            break;
        case rbText:
            b = vga_getram (raddr);
            if ( ++raddr >= 4 * BSIZE ) raddr -= 4 * BSIZE;
            break;
        case rbGraph:
            b = vga_getram (raddr);
            if ( ++raddr >= 4 * GSIZE ) raddr -= 4 * GSIZE;
            break;
        case rbReg:
            b = vga_getreg (raddr);
            if ( ++raddr >= NREG ) raddr -= NREG;
            break;
        case rbFont:
            b = ((byte *)vga_font)[raddr];
            if ( ++raddr > NFONT * THEIGHT ) raddr -= NFONT * THEIGHT - 4;
            break;
        case rbVDPRAM:
                b = ((byte *)buffer)[raddr];
            raddr = ( raddr + 1 ) & 0x3FFF;
            break;
        case rbVDPReg:
            if ( raddr < 8 ) b = vgavdp_vern[raddr];
            else b = vgavdp_regs[raddr-8];
            raddr = ( raddr + 1 ) & 0x0F;
            break;
        }
    return b;
    }

void vga_out61 (byte chr)
    {
    diag_message(DIAG_VGA_PORT, "VGA write to port 0x61: 0x%02X", chr);
    if ( chr == 0xFF )
        {
        vga_reset ();
        }
    else if ( mode == mdVDP )
        {
        if ( chr < 0x80 )
            {
            rcol = chr;
            }
        else if ( chr < 0xC0 )
            {
            rbmode = rbVDPRAM;
            raddr = (((unsigned int)chr & 0x3F) << 9) + (rcol << 1);
            }
        else if ( chr < 0xE0 )
            {
            rbmode = rbVDPReg;
            raddr = chr & 0x0F;
            }
        else
            {
            rbmode = rbZero;
            }
        }
    else
        {
        if (  chr < 0x80 )
            {
            rcol = chr;
            }
        else if ( chr < 0xA0 )
            {
            rbmode = rbText;
            rrow = chr & 0x1F;
            if ( rrow >= ROWS + 1 ) rrow -= ROWS + 1;
            if ( rcol >= COLS ) rcol -= COLS;
            raddr = btop + COLS * rrow + rcol;
            if ( raddr >= BSIZE ) raddr -= BSIZE;
            raddr *= 4;
            }
        else if ( chr < 0xC0 )
            {
            rbmode = rbGraph;
            rrow = chr & 0x1F;
            if ( rrow >= ROWS + 1 ) rrow -= ROWS + 1;
            rcol &= 0x3F;
            if ( rcol >= GCOLS ) rcol -= GCOLS;
            raddr = btop + GSPAN * ( GCOLS * rrow + rcol );
            if ( raddr >= GSIZE ) raddr -= GSIZE;
            raddr *= 4;
            }
        else if ( chr < 0xE0 )
            {
            rbmode = rbReg;
            raddr = chr & 0x1F;
            }
        else if ( chr < 0xF0 )
            {
            rbmode = rbFont;
            raddr = THEIGHT * ( ( ((unsigned int)chr & 0x03) << 7 ) + rcol );
            }
        else
            {
            rbmode = rbZero;
            }
        }
    bout = vga_next61();
    }

byte vga_in61 (void)
    {
    byte b = bout;
    if ( nwait > 0 )
        {
        bout = 0x61;
        --nwait;
        }
    else
        {
        bout = vga_next61 ();
        }
    diag_message(DIAG_VGA_PORT, "VGA read from port 0x61: 0x%02X", b);
    return b;
    }

void vga_refresh (void)
    {
    unsigned int iTChg = iTime;
    ++iTime;
    if ( ! win_active (vga_win) ) return;
    diag_message(DIAG_VGA_REFRESH, "VGA refresh: mode = %d", mode);
    if ( mode == mdVDP )
        {
        unsigned int i, j;
        byte *vdp_data = vgavdp_pix;
        diag_message(DIAG_VGA_REFRESH, "VGA echo VDP");
        vgavdp_refresh ();
        for ( i = 0; i < VDP_HEIGHT; ++i )
            {
            byte *vga_data = vga_win->data + WIDTH * ( 2 * i + VDP_YORG ) + VDP_XORG;
            for ( j = 0; j < VDP_WIDTH; ++j )
                {
                byte pix = vdpclr[*vdp_data];
                *vga_data = pix;
                *(vga_data + WIDTH) = pix;
                ++vga_data;
                *vga_data = pix;
                *(vga_data + WIDTH) = pix;
                ++vga_data;
                ++vdp_data;
                }
            }
        }
    else if ( mode != mdGraph )
        {
        unsigned int i, j;
        unsigned int addr = btop;
        iTChg ^= iTime;
        for ( i = 0; i < ROWS; ++i )
            {
            for ( j = 0; j < COLS; ++j )
                {
                if ( ( ( iTChg & TBLINK ) && ( buffer[addr] & blink ) )
                    || ( ( iTChg & TCURSOR ) && ( buffer[addr] & cursor ) ) )
                    {
                    vga_put_glyph (i, j, buffer[addr]);
                    }
                if ( ++addr >= BSIZE ) addr -= BSIZE;
                }
            }
        }
    win_refresh (vga_win);
    diag_message(DIAG_VGA_REFRESH, "VGA refresh completed");
    }
/*

The Propeller emulation needs it own copy of the VDP emulation, as it
will not be updated when the Propeller is in Native mode.

What follows is a cut-down version of vid.c with sprite coincidence
checks and timing checks removed.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "diag.h"
#include "common.h"

/*...vZ80\46\h:0:*/
/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...vvid\46\h:0:*/
/*...vkbd\46\h:0:*/
/*...vmon\46\h:0:*/
/*...e*/

/*...svgavdp_reset:0:*/
void vgavdp_reset(void)
	{
	vgavdp_latched = FALSE;
	vgavdp_addr = 0x000;
	}
/*...e*/

/*...svga_out1 \45\ data write:0:*/
void vga_out1(byte val)
	{
    if ( mode != mdVDP ) return;

	vgavdp_read_mode = FALSE; /* Prevent lots of warnings */
	((byte *)buffer)[vgavdp_addr] = val;
    // diag_message (DIAG_ALWAYS, "VGA VDP Write: addr = 0x%04X, val = 0x%02X", vgavdp_addr, val);
	vgavdp_addr = ( (vgavdp_addr+1) & (VGAVDP_MEMORY_SIZE-1) );
	vgavdp_latched = FALSE;
	}
/*...e*/
/*...svga_out2 \45\ latch value\44\ then act:0:*/
void vga_out2(byte val)
	{
    if ( mode != mdVDP ) return;

	if ( !vgavdp_latched )
		/* First write to port 2, record the value */
		{
		vgavdp_latch = val;
		vgavdp_addr = ( (vgavdp_addr&0xff00)|val );
			/* Son Of Pete relies on the low part of the
			   address being updated during the first write.
			   HexTrain also does partial address updates. */
		vgavdp_latched = TRUE;
		}
	else
		/* Second write to port 2, act */
		{
		switch ( val & 0xc0 )
			{
			case 0x00:
				/* Set up for reading from VRAM */
				vgavdp_addr = ( ((val&0x3f)<<8)|vgavdp_latch );
				vgavdp_read_mode = TRUE;
				break;
			case 0x40:
				/* Set up for writing to VRAM */
				vgavdp_addr = ( ((val&0x3f)<<8)|vgavdp_latch );
				vgavdp_read_mode = FALSE;
				break;
			case 0x80:
				/* Write VDP register.
				   Various bits must be zero. */
				val &= 7;
				vgavdp_latch &= ~vgavdp_regs_zeros[val];
				vgavdp_regs[val] = vgavdp_latch;
				break;
			case 0xc0:
				break;
			}
		vgavdp_latched = FALSE;
		}
	}
/*...e*/

/*...svgavdp_refresh_blank:0:*/
static void vgavdp_refresh_blank(void)
	{
	memset(vgavdp_pix, (vgavdp_regs[7]&0x0f), VDP_WIDTH*VDP_HEIGHT);
	}
/*...e*/
/*...svgavdp_refresh_sprites:0:*/
/*...svgavdp_refresh_sprites_line_check:0:*/
static BOOLEAN vgavdp_refresh_sprites_line_check(int scn_y, int sprite)
	{
	if ( scn_y < 0 || scn_y >= 192 )
		return FALSE;
	if ( ++vgavdp_spr_lines[scn_y] <= 4 )
		return TRUE;
	return FALSE;
	}
/*...e*/
/*...svgavdp_refresh_sprites_plot:0:*/
static void vgavdp_refresh_sprites_plot(int scn_x, int scn_y, int spr_col)
	{
	if ( scn_x >= 0 && scn_x < 256 )
		{
		if ( spr_col != 0 )
			vgavdp_pix[(VBORDER+scn_y)*VDP_WIDTH+(HBORDER256+scn_x)] = spr_col;
		}
	}
/*...e*/

static void vgavdp_refresh_sprites(void)
	{
	byte size = ( (vgavdp_regs[1]&0x02) != 0 );
	byte mag  = ( (vgavdp_regs[1]&0x01) != 0 );
	word sprgen = ((word)(vgavdp_regs[6]&0x07)<<11);
	word spratt = ((word)(vgavdp_regs[5]&0x7f)<<7);
	int sprite, x, y, scn_x;
	byte spr_pat_mask = size ? 0xfc : 0xff;
	for ( sprite = 0; sprite < 32; sprite++ )
		{
		int spr_y     = (int) (unsigned) ((byte *)buffer)[spratt++]; /* 0-255, partially signed */
		int spr_x     = (int) (unsigned) ((byte *)buffer)[spratt++]; /* 0-255 */
		byte spr_pat  = ((byte *)buffer)[spratt++] & spr_pat_mask;
		word spr_addr = sprgen+((word)spr_pat<<3);
		byte spr_flag = ((byte *)buffer)[spratt++];
		byte spr_col  = (spr_flag & 0x0f);
		if ( spr_y == 0xd0 )
			break;
		if ( spr_y <= 192 )
			++spr_y;
		else
			spr_y -= 255;
		if ( spr_flag & 0x80 )
			spr_x -= 32;
		if ( size )
			for ( y = 0; y < 16; y++ )
				{
				word bits = ((word)((byte *)buffer)[spr_addr]<<8)|((word)((byte *)buffer)[spr_addr+16]);
				spr_addr++;
				if ( mag )
/*...sMAG\61\1\44\ SIZE\61\1:40:*/
{
int scn_y = spr_y+y*2;
if ( vgavdp_refresh_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x+=2 )
		if ( bits & (0x8000>>x) )
			{
			vgavdp_refresh_sprites_plot(scn_x  , scn_y, spr_col);
			vgavdp_refresh_sprites_plot(scn_x+1, scn_y, spr_col);
			}
++scn_y;
if ( vgavdp_refresh_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x+=2 )
		if ( bits & (0x8000>>x) )
			{
			vgavdp_refresh_sprites_plot(scn_x  , scn_y, spr_col);
			vgavdp_refresh_sprites_plot(scn_x+1, scn_y, spr_col);
			}
}
/*...e*/
				else
/*...sMAG\61\0\44\ SIZE\61\1:40:*/
{
int scn_y = spr_y+y;
if ( vgavdp_refresh_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x++ )
		if ( bits & (0x8000>>x) )
			vgavdp_refresh_sprites_plot(scn_x, scn_y, spr_col);
}
/*...e*/
				}
		else
			for ( y = 0; y < 8; y++ )
				{
				byte bits = ((byte *)buffer)[spr_addr++];
				if ( mag )
/*...sMAG\61\1\44\ SIZE\61\0:40:*/
{
int scn_y = spr_y+y*2;
if ( vgavdp_refresh_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x+=2 )
		if ( bits & (0x80>>x) )
			{
			vgavdp_refresh_sprites_plot(scn_x  , scn_y, spr_col);
			vgavdp_refresh_sprites_plot(scn_x+1, scn_y, spr_col);
			}
++scn_y;
if ( vgavdp_refresh_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x+=2 )
		if ( bits & (0x80>>x) )
			{
			vgavdp_refresh_sprites_plot(scn_x  , scn_y, spr_col);
			vgavdp_refresh_sprites_plot(scn_x+1, scn_y, spr_col);
			}
}
/*...e*/
				else
/*...sMAG\61\0\44\ SIZE\61\0:40:*/
{
int scn_y = spr_y+y;
if ( vgavdp_refresh_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x++ )
		if ( bits & (0x80>>x) )
			vgavdp_refresh_sprites_plot(scn_x, scn_y, spr_col);
}
/*...e*/
				}
		}
	}
/*...e*/
/*...svgavdp_refresh_graphics1:0:*/
static void vgavdp_refresh_graphics1_pat(
	byte pat,
	word patgen,
	word patcol,
	byte *d
	)
	{
	word genptr = patgen + pat*8;
	word colptr = patcol + pat/8;
	byte col = ((byte *)buffer)[colptr];
	byte fg = (col>>4);
	byte bg = (col&15);
	int x, y;
	if ( fg == 0 ) fg = (vgavdp_regs[7]&0x0f);
	if ( bg == 0 ) bg = (vgavdp_regs[7]&0x0f);
	for ( y = 0; y < 8; y++ )
		{
		byte gen = ((byte *)buffer)[genptr++];
		for ( x = 0; x < 8; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -8 + VDP_WIDTH ); 
		}
	}

static void vgavdp_refresh_graphics1_third(
	word patnam,
	word patgen,
	word patcol,
	byte *d
	)
	{
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		for ( x = 0; x < 32; x++ )
			{
			byte pat = ((byte *)buffer)[patnam++];
			vgavdp_refresh_graphics1_pat(pat, patgen, patcol, d);
			d += 8;
			}
		d += ( -32*8 + VDP_WIDTH*8 );
		}
	}

static void vgavdp_refresh_graphics1(void)
	{
	word patnam = ( ((word)(vgavdp_regs[2]&0x0f)) << 10 );
	word patgen = ( ((word)(vgavdp_regs[4]&0x07)) << 11 );
	word patcol = ( ((word)(vgavdp_regs[3]&0xff)) <<  6 );
	byte *d = vgavdp_pix + VDP_WIDTH*VBORDER + HBORDER256;

	vgavdp_refresh_graphics1_third(patnam       , patgen, patcol, d           );
	vgavdp_refresh_graphics1_third(patnam+0x0100, patgen, patcol, d+VDP_WIDTH* 8*8);
	vgavdp_refresh_graphics1_third(patnam+0x0200, patgen, patcol, d+VDP_WIDTH*16*8);

	vgavdp_refresh_sprites();
	}
/*...e*/
/*...svgavdp_refresh_graphics2:0:*/
static void vgavdp_refresh_graphics2_pat(
	byte pat,
	word patgen,
	word patcol,
	byte *d
	)
	{
	word genptr = patgen + pat*8;
	word colptr = patcol + pat*8;
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		byte gen = ((byte *)buffer)[genptr++];
		byte col = ((byte *)buffer)[colptr++];
		byte fg = (col>>4);
		byte bg = (col&15);
		if ( fg == 0 ) fg = (vgavdp_regs[7]&0x0f);
		if ( bg == 0 ) bg = (vgavdp_regs[7]&0x0f);
		for ( x = 0; x < 8; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -8 + VDP_WIDTH ); 
		}
	}

static void vgavdp_refresh_graphics2_third(
	word patnam,
	word patgen,
	word patcol,
	byte *d
	)
	{
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		for ( x = 0; x < 32; x++ )
			{
			byte pat = ((byte *)buffer)[patnam++];
			vgavdp_refresh_graphics2_pat(pat, patgen, patcol, d);
			d += 8;
			}
		d += ( -32*8 + VDP_WIDTH*8 );
		}
	}

/* According to spec, bits 1 and 0 of register 4 (patgen) should be set.
   According to spec, bits 6 and 5 of register 3 (patcol) should be set.
   Experimentally, we discovered undocumented features:
   If bit 0 of register 4 is clear, 2nd third uses patgen from 1st third.
   If bit 1 of register 4 is clear, 3rd third uses patgen from 1st third.
   If bit 5 of register 3 is clear, 2nd third uses patcol from 1st third.
   If bit 6 of register 3 is clear, 3rd third uses patcol from 1st third.
   Maybe this is an attempt at saving VRAM.
   Anyway, we support it in our emulation. */

static void vgavdp_refresh_graphics2(void)
	{
	word patnam  = ( ((word)(vgavdp_regs[2]&0x0f)) << 10 );
	word patgen1 = ( ((word)(vgavdp_regs[4]&0x04)) << 11 );
	word patcol1 = ( ((word)(vgavdp_regs[3]&0x80)) <<  6 );
	word patgen2 = (vgavdp_regs[4]&0x01) ? patgen1+0x0800 : patgen1;
	word patcol2 = (vgavdp_regs[3]&0x20) ? patcol1+0x0800 : patcol1;
	word patgen3 = (vgavdp_regs[4]&0x02) ? patgen1+0x1000 : patgen1;
	word patcol3 = (vgavdp_regs[3]&0x40) ? patcol1+0x1000 : patcol1;
	byte *d = vgavdp_pix + VDP_WIDTH*VBORDER + HBORDER256;

	vgavdp_refresh_graphics2_third(patnam        , patgen1, patcol1, d           );
	vgavdp_refresh_graphics2_third(patnam+0x0100 , patgen2, patcol2, d+VDP_WIDTH* 8*8);
	vgavdp_refresh_graphics2_third(patnam+0x0200 , patgen3, patcol3, d+VDP_WIDTH*16*8);

	vgavdp_refresh_sprites();
	}
/*...e*/
/*...svgavdp_refresh_text:0:*/
static void vgavdp_refresh_text_pat(
	byte pat,
	word patgen,
	byte *d
	)
	{
	word genptr = patgen + pat*8;
	byte col = vgavdp_regs[7];
	byte fg = (col>>4);
	byte bg = (col&15);
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		byte gen = ((byte *)buffer)[genptr++];
		for ( x = 0; x < 6; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -6 + VDP_WIDTH ); 
		}
	}

static void vgavdp_refresh_text_third(
	word patnam,
	word patgen,
	byte *d
	)
	{
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		for ( x = 0; x < 40; x++ )
			{
			byte pat = ((byte *)buffer)[patnam++];
			vgavdp_refresh_text_pat(pat, patgen, d);
			d += 6;
			}
		d += ( -40*6 + VDP_WIDTH*8 );
		}
	}

static void vgavdp_refresh_text(void)
	{
	word patnam = ( ((word)(vgavdp_regs[2]&0x0f)) << 10 );
	word patgen = ( ((word)(vgavdp_regs[4]&0x07)) << 11 );
	byte *d = vgavdp_pix + VDP_WIDTH*VBORDER + HBORDER240;

	vgavdp_refresh_text_third(patnam      , patgen, d           );
	vgavdp_refresh_text_third(patnam+ 8*40, patgen, d+VDP_WIDTH* 8*8);
	vgavdp_refresh_text_third(patnam+16*40, patgen, d+VDP_WIDTH*16*8);
	}
/*...e*/
/*...svgavdp_refresh:0:*/
#define	MODE(m1,m2,m3) ( ((m1)<<2) | ((m2)<<1) | (m3) )

static void vgavdp_refresh(void)
	{
	if ( (vgavdp_regs[1]&0x40) == 0 )
		vgavdp_refresh_blank();
	else
		{
		BOOLEAN m1 = ( (vgavdp_regs[1]&0x10) != 0 );
		BOOLEAN m2 = ( (vgavdp_regs[1]&0x08) != 0 );
		BOOLEAN m3 = ( (vgavdp_regs[0]&0x02) != 0 );
		int mode = MODE(m1,m2,m3);
		if ( vgavdp_pix[0] != (vgavdp_regs[7]&0x0f) ||
		     mode != vgavdp_last_mode )
			/* Ensure the border is redrawn.
			   Perhaps could do this more efficiently.
			   But it isn't going to happen very often. */
			vgavdp_refresh_blank();
		switch ( mode )
			{
			case MODE(0,0,0):
				vgavdp_refresh_graphics1();
				break;
			case MODE(0,0,1):
				vgavdp_refresh_graphics2();
				break;
			case MODE(0,1,0):
                /* Should really be Multicolour mode */
				vgavdp_refresh_blank();
				break;
			case MODE(1,0,0):
				vgavdp_refresh_text();
				break;
			default:
				vgavdp_refresh_blank();
				break;
			}
		vgavdp_last_mode = mode;
		}
	}

/*...e*/

void vgavdp_init(void)
	{
    memset (vga_win->data, 0, WIDTH * HEIGHT);
    memset (buffer, 0, MSIZE * sizeof (unsigned int));
    memset (vgavdp_regs, 0, sizeof (vgavdp_regs));
	vgavdp_addr    = 0x0000;
	vgavdp_read_mode = FALSE;
	vgavdp_last_mode = -1; /* None of the valid modes */

	memset(vgavdp_spr_lines, FALSE, sizeof(vgavdp_spr_lines));
	}
