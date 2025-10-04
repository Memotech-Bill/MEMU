/*

mon.c - 80 column card and its monitor

This code can maintain a detailed model of the state of the 80 column card,
and its associated low level driver code.
A basic emulation of the 6845 CRTC chip and the memory behind it is done.
The driver protocol documented in FDX manual is implemented, as per ACRT.MAC.
SCPM implements a subset, and also ^[Q and ^[R, in ASCRT.MAC.
If the documentation and code differ, the .MAC file is considered definitive.
Don't yet handle DOT, VCT and write masks.

We have the following emulation modes :-
  * MONEMU_WIN
       maintain the model
       render to window,
       in colour (or monochrome),
       pixel perfect (hopefully)
       respond to keypresses/releases made in the window
       handling ctrl-keys, shift-keys, shift-lock, caps-lock
  * MONEMU_TH
       maintain the model
       render to text mode screen,
       using the Terminal Handler,
       in colour,
       only printable characters
       use th_key_status/th_key_read to get keys
  * MONEMU_CONSOLE
       no model
       directly output to stdout
       only printable characters
       use cooked blocking fgetc(stdin)

http://www.tinyvga.com/6845 describes the chip used.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_TH
#define	BOOLEAN BOOLEANx
#include "th.h"
#undef BOOLEAN
#endif

#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"
#include "kbd.h"
#include "monprom.h"
#include "mon.h"
#ifdef HAVE_CONFIG
#include "config.h"
#endif

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...vkbd\46\h:0:*/
/*...vmonprom\46\h:0:*/
/*...vmon\46\h:0:*/
/*...e*/

/*...svars:0:*/
static int mon_emu = MONEMU_IGNORE_INIT;

#define	ROWS         24
#define	COLUMNS      80

static const char *mon_title = NULL;
static const char *mon_display = NULL;

/* The hardware level emulation state */

static byte mon_adr_lo;
static byte mon_adr_hi;
static byte mon_ascd;
static byte mon_atrd;
static byte mon_crtc_address;

/* The driver emulation state */

#define	ATTR_FG_R       0x01
#define	ATTR_FG_G       0x02
#define	ATTR_FG_B       0x04
#define	ATTRS_FG        0x07
#define	ATTR_BG_R       0x08
#define	ATTR_BG_G       0x10
#define	ATTR_BG_B       0x20
#define	ATTRS_BG        0x38
#define	ATTR_BLINK      0x40
#define	ATTR_GRAPHICS   0x80

#define	ATTR_ULINE      0x01
#define	ATTR_BRIGHT     0x04
#define	ATTR_REVERSE    0x10
#define	ATTR_BACKGROUND 0x20

#define	SCREEN_RAM      2048

static WIN *mon_win;
static TXTBUF *mon_tbuf;
static int mon_x, mon_y;
static byte mon_print_attr;
static byte mon_non_print_attr;
static BOOLEAN mon_cursor_on;
static BOOLEAN mon_scroll;
static BOOLEAN mon_scrolled = FALSE;
static byte mon_write_mask;

static BOOLEAN mon_ascrt_no_colour;

/* How to map characters to glyphs */
#define	MODE_STANDARD  0
#define	MODE_ALTERNATE 1
#define	MODE_GRAPHIC   2
static int mon_mode;

/* State names reflect what we expect next */
#define	STATE_NORMAL    0
#define	STATE_DOT_X     1
#define	STATE_DOT_Y     2
#define	STATE_VCT_X1    3
#define	STATE_VCT_Y1    4
#define	STATE_VCT_X2    5
#define	STATE_VCT_Y2    6
#define	STATE_CXY_X     7
#define	STATE_CXY_Y     8
#define	STATE_BKG_N     9
#define	STATE_ATR_M    10
#define	STATE_ESC_CH   11
#define	STATE_ESC_X_CH 12
#define	STATE_ESC_T_N  13
#define	STATE_ESC_U_N  14
#define	STATE_ESC_V_N  15
#define	STATE_ESC_P_N  16
#define	STATE_ESC_N_N  17
#define	STATE_ESC_B_N  18
static int mon_state;

static BOOLEAN mon_win_changed;
#ifdef HAVE_TH
static BOOLEAN mon_th_changed;
#endif
static BOOLEAN mon_blink_blank;
static BOOLEAN mon_kbd_pressed;
/*...e*/

#ifdef __Pico__
#define DRAW_GLYPH(...)
#else
#define DRAW_GLYPH(...) if ( mon_win != NULL ) twin_draw_glyph (__VA_ARGS__)
#endif

/*...smon_out30 \45\ set address low\44\ and copy to screen:0:*/
/* Address low.
   Latches the data onto the screen. */
void mon_out30(byte value)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return;
	diag_message(DIAG_MON_HW, "write address low 0x%02x", value);
	mon_adr_lo = value;
	if ( mon_adr_hi & 0x80 )
		/* Its a write */
		{
		word adr = ( (((word)(mon_adr_hi&0x07))<<8) | mon_adr_lo );
		if ( mon_adr_hi & 0x40 )
			mon_tbuf->ram[adr].ch = mon_ascd;
		if ( mon_adr_hi & 0x20 )
			mon_tbuf->ram[adr].at = mon_atrd;
		if ( mon_adr_hi & 0x60 ) DRAW_GLYPH (mon_win, adr);
		mon_refresh();
		}
	}
/*...e*/
/*...smon_out31 \45\ set address high and masks:0:*/
/* Address high */
void mon_out31(byte value)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return;
	diag_message(DIAG_MON_HW, "write address high 0x%02x", value);
	mon_adr_hi = value;
	}
/*...e*/
/*...smon_out32 \45\ write ASCII data:0:*/
/* ASCII data */
void mon_out32(byte value)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return;
	diag_message(DIAG_MON_HW, "write ASCII data 0x%02x", value);
	mon_ascd = value;
	}
/*...e*/
/*...smon_out33 \45\ write attribute data:0:*/
/* Attribute data */
void mon_out33(byte value)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return;
	diag_message(DIAG_MON_HW, "write attribute data 0x%02x", value);
	mon_atrd = value;
	}
/*...e*/
/*...smon_out38 \45\ select a CRTC register:0:*/
/* CRTC address */
void mon_out38(byte value)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return;
	diag_message(DIAG_MON_HW, "select CRTC register %d", value);
	value &= 0x1f; /* Only low 5 bits are significant */
	if ( value >= NREG80C )
		{
		if ( ( mon_emu & MONEMU_IGNORE_INIT ) == 0 )
			fatal("attempt to select CRTC register %d", value);
		else
			diag_message(DIAG_MON_HW, "attempt to select CRTC register %d", value);
		}
	mon_crtc_address = value;
	}
/*...e*/
/*...smon_out39 \45\ write CRTC register:0:*/
/* CRTC data */
void mon_out39(byte value)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return;
	diag_message(DIAG_MON_HW, "write CRTC register %d,0x%02x", mon_crtc_address, value);
	mon_tbuf->reg[mon_crtc_address] = value;
	switch ( mon_crtc_address )
		{
		case 10:
			/* Cursor Start Raster.
			   We honor the cursor visible bit,
			   but we don't honor the flash rate bit. */
			mon_cursor_on = ( (value&0x40) != 0 );
			mon_refresh();
			break;
		case 11:
			/* Cursor End Raster.
			   Silently ignore it. */
			break;
		case 12:    /* Start address (low) */
		case 13:    /* Start address (high) */
            mon_scrolled = TRUE;
            /* Fall through */
		case 14:    /* Cursor address (low) */
		case 15:    /* Cursor address (high) */
			{
			word base;
			word adr;
			base = (((word)(mon_tbuf->reg[12] & 0x07))<<8) |
			         (word) mon_tbuf->reg[13]              ;
			adr  = (((word)(mon_tbuf->reg[14] & 0x07))<<8) |
			         (word) mon_tbuf->reg[15]              ;
			adr  =  ( adr - base ) & 0x7ff;
			mon_x = adr % COLUMNS;
			mon_y = adr / COLUMNS;
			mon_refresh();
			}
			break;
		default:
			if ( ( mon_emu & MONEMU_IGNORE_INIT ) == 0 )
				fatal("write to CRTC register %d not emulated", mon_crtc_address);
			else
				diag_message(DIAG_MON_HW, "write to CRTC register %d not emulated", mon_crtc_address);  
			break;
		}
	}
/*...e*/

/*...smon_in30 \45\ ring the bell\63\:0:*/
/* Address low.
   According to ACRT.MAC, this will ring the bell! */
byte mon_in30(void)
	{
	diag_message(DIAG_MON_HW, "ring the bell");
	return (byte) 0xff;
	}
/*...e*/
/*...smon_in32 \45\ read ASCII data:0:*/
/* ASCII data */
byte mon_in32(void)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return 0xff;
	word adr = ( (((word)(mon_adr_hi&0x07))<<8) | mon_adr_lo );
	if ( (mon_adr_hi&0x80) == 0x00 )
		{
		byte value = mon_tbuf->ram[adr].ch;
		diag_message(DIAG_MON_HW, "read ASCII data returns 0x%02x", value);
		return value;
		}
	diag_message(DIAG_MON_HW, "read ASCII data returns 0xff (offscreen)");
	return 0xff;
	}
/*...e*/
/*...smon_in33 \45\ read attribute data:0:*/
/* Attribute data */
byte mon_in33(void)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return 0xff;
	word adr = ( (((word)(mon_adr_hi&0x07))<<8) | mon_adr_lo );
	if ( (mon_adr_hi&0x80) == 0x00 )
		{
		byte value = mon_tbuf->ram[adr].at;
		diag_message(DIAG_MON_HW, "read attribute data returns 0x%02x", value);
		return value;
		}
	diag_message(DIAG_MON_HW, "read attribute data returns 0xff (offscreen)");
	return 0xff;
	}
/*...e*/
/*...smon_in38 \45\ query selected CRTC register:0:*/
/* CRTC address.
   Not sure if this is readable on real hardware. */
byte mon_in38(void)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return 0xff;
	diag_message(DIAG_MON_HW, "return selected CRTC register returned %d", mon_crtc_address);
	return mon_crtc_address;
	}
/*...e*/
/*...smon_in39 \45\ read CRTC register:0:*/
/* CRTC data.
   Many of the registers are in fact write-only,
   but we allow them all to be read. */
byte mon_in39(void)
	{
    if (( mon_emu == 0 ) || (mon_tbuf == NULL)) return 0xff;
	if ( mon_crtc_address >= NREG80C )
		fatal("read of CRTC register %d not emulated", mon_crtc_address);
	switch ( mon_crtc_address )
		{
		case 14:
		case 15:
			/* Cursor address.
			   Make sure its right before anyone reads it. */
			{
			word adr = ((((word)(mon_tbuf->reg[12] & 0x07))<<8) |
			        (word)mon_tbuf->reg[13]) + mon_y*COLUMNS+mon_x;
			mon_tbuf->reg[14] = (byte) ((adr>>8)&0x07);
			mon_tbuf->reg[15] = (byte)  adr    ;
			}
			break;
		}
	diag_message(DIAG_MON_HW, "read CRTC register %d returned 0x%02x",
		mon_crtc_address, mon_tbuf->reg[mon_crtc_address]);
	return mon_tbuf->reg[mon_crtc_address];
	}
/*...e*/

/*...smon_write_cr:0:*/
static void mon_write_cr(void)
	{
	mon_x = 0;
	}
/*...e*/
/*...smon_write_lf:0:*/
static void mon_write_lf(void)
	{
	if ( mon_y < ROWS-1 )
		++mon_y;
	else if ( mon_scroll )
		{
		int x;
		word	adr = (((word)(mon_tbuf->reg[12] & 0x07))<<8) |
			        (word)mon_tbuf->reg[13]      ;
		adr   =  ( adr + COLUMNS ) & 0x7ff;
		mon_tbuf->reg[12]  =  (byte) ( adr >> 8 );
		mon_tbuf->reg[13]  =  (byte) adr;
		adr   += ( ROWS - 1 ) * COLUMNS;
		for ( x = 0; x < COLUMNS; x++ )
			{
			adr   &= 0x7ff;
			mon_tbuf->ram[adr].ch = ' ';
			mon_tbuf->ram[adr].at = mon_non_print_attr;
            DRAW_GLYPH (mon_win, adr);
			++adr;
			}
		}
	else
		mon_y = 0;
	}		
/*...e*/
/*...smon_write_up:0:*/
static void mon_write_up(void)
	{
	if ( mon_y > 0 )
		--mon_y;
	/* else
		   You might expect it to scroll the screen down here,
		   but in fact, neither ACRT.MAC nor ASCRT.MAC does this. */
	}
/*...e*/
/*...smon_write_fwd:0:*/
static void mon_write_fwd(void)
	{
	if ( ++mon_x == COLUMNS )
		{
		mon_x = 0;
		mon_write_lf();
		}
	}
/*...e*/
/*...smon_write_bs:0:*/
static void mon_write_bs(void)
	{
	if ( mon_x > 0 )
		--mon_x;
	else if ( mon_y > 0 )
		{
		mon_x = COLUMNS-1;
		--mon_y;
		}
	}
/*...e*/

/*...smon_write_bel:0:*/
static void mon_write_bel(void)
	{
#ifdef HAVE_TH
	if ( mon_emu & MONEMU_TH )
		th_beep();
#endif
	}
/*...e*/
/*...smon_write_cxy:0:*/
static void mon_write_cxy(int x, int y)
	{
	if ( x < COLUMNS )
		mon_x = x;
	if ( y < ROWS )
		mon_y = y;
	}
/*...e*/
/*...smon_write_bg:0:*/
static void mon_write_bkg(byte b)
	{
	mon_print_attr     = (mon_print_attr     & ~ATTRS_BG) | ((b&7)<<3);
	mon_non_print_attr = (mon_non_print_attr & ~ATTRS_BG) | ((b&7)<<3);
	}
/*...e*/
/*...smon_write_eol:0:*/
static void mon_write_eol(void)
	{
	int x;
	word adr = ((((word)(mon_tbuf->reg[12] & 0x07))<<8) |
			        (word)mon_tbuf->reg[13]) + mon_y*COLUMNS+mon_x;
	for ( x = mon_x; x < COLUMNS; x++ )
		{
		adr   &= 0x7ff;
		mon_tbuf->ram[adr].ch = ' ';
		mon_tbuf->ram[adr].at = mon_non_print_attr;
        DRAW_GLYPH (mon_win, adr);
		++adr;
		}
	}
/*...e*/
/*...smon_write_atr:0:*/
static void mon_write_atr(byte b)
	{
	mon_print_attr     = b;
	mon_non_print_attr = b;
	}
/*...e*/
/*...smon_write_tab:0:*/
static void mon_write_tab(void)
	{
	mon_x |= 7;
	mon_write_fwd();
	}
/*...e*/
/*...smon_write_clr:0:*/
static void mon_write_clr(void)
	{
	int x, y;
	word adr = 0;
	mon_tbuf->reg[12] = 0;
	mon_tbuf->reg[13] = 0;
	for ( y = 0; y < ROWS; y++ )
		for ( x = 0; x < COLUMNS; x++ )
			{
			mon_tbuf->ram[adr].ch = ' ';
			mon_tbuf->ram[adr].at = mon_non_print_attr;
            DRAW_GLYPH (mon_win, adr);
			++adr;
			}
	mon_x = 0;
	mon_y = 0;
	}
/*...e*/
/*...smon_write_fg:0:*/
static void mon_write_fg(byte col)
	{
	mon_print_attr = (mon_print_attr & ~ATTRS_FG) | col;
	}
/*...e*/
/*...smon_write_ini:0:*/
static void mon_write_ini(void)
	{
	mon_print_attr      = ATTR_FG_G;
	mon_non_print_attr  = ATTR_FG_G;
	mon_cursor_on       = TRUE;
	mon_scroll          = TRUE;
	mon_write_mask      = 0xe0; /* write to ASCI and ATR */
	mon_ascrt_no_colour = FALSE;
	mon_mode            = MODE_STANDARD;
	mon_write_cr();
	mon_write_lf();
	}
/*...e*/
/*...smon_write_hme:0:*/
static void mon_write_hme(void)
	{
	mon_x = 0;
	mon_y = 0;
	}
/*...e*/
/*...smon_write_inslin:0:*/
static void mon_write_inslin(void)
	{
	int x, y;
	word adr1, adr2;
	word base = ((((word)(mon_tbuf->reg[12] & 0x07))<<8) |
	               (word) mon_tbuf->reg[13]              );
	for ( y = ROWS-1; y > mon_y; y-- )
		{
		adr1  =  base + COLUMNS * ( y - 1 );
		adr2  =  adr1 + COLUMNS;
		for ( x = 0; x < COLUMNS; x++ )
			{
			adr1  &= 0x7ff;
			adr2  &= 0x7ff;
			mon_tbuf->ram[adr2].ch = mon_tbuf->ram[adr1].ch;
			mon_tbuf->ram[adr2].at = mon_tbuf->ram[adr1].at;
            DRAW_GLYPH (mon_win, adr2);
			++adr1;
			++adr2;
			}
		}
	adr1 = base + COLUMNS * mon_y;
	for ( x = 0; x < COLUMNS; x++ )
		{
		adr1  &= 0x7ff;
		mon_tbuf->ram[adr1].ch = ' ';
		mon_tbuf->ram[adr1].at = mon_non_print_attr;
        DRAW_GLYPH (mon_win, adr1);
		++adr1;
		}
	}
/*...e*/
/*...smon_write_dellin:0:*/
static void mon_write_dellin(void)
	{
	int x, y;
	word adr1 = 0;  // Initialisation to keep compiler happy.
    word adr2;
	word base = ((((word)(mon_tbuf->reg[12] & 0x07))<<8) |
	               (word) mon_tbuf->reg[13]              );
	for ( y = mon_y; y < ROWS-1; y++ )
		{
		adr1  =  base + COLUMNS * y;
		adr2  =  adr1 + COLUMNS;
		for ( x = 0; x < COLUMNS; x++ )
			{
			adr1  &= 0x7ff;
			adr2  &= 0x7ff;
			mon_tbuf->ram[adr1].ch = mon_tbuf->ram[adr2].ch;
			mon_tbuf->ram[adr1].at = mon_tbuf->ram[adr2].at;
            DRAW_GLYPH (mon_win, adr1);
			++adr1;
			++adr2;
			}
		}
	for ( x = 0; x < COLUMNS; x++ )
		{
		adr1  &= 0x7ff;
		mon_tbuf->ram[adr1].ch = ' ';
		mon_tbuf->ram[adr1].at = mon_non_print_attr;
        DRAW_GLYPH (mon_win, adr1);
		++adr1;
		}
	}
/*...e*/
/*...smon_write_bit_attr:0:*/
static void mon_write_bit_attr(byte n, byte *attr)
	{
	if ( n == 0 )
		*attr = 0;
	else
		*attr |= ( 1 << ((n-1)&7) );
	}
/*...e*/
/*...smon_write_dot:0:*/
static void mon_write_dot(int x, int y)
	{
	fatal("80 column card DOT command not implemented yet");
	}
/*...e*/
/*...smon_write_vct:0:*/
static void mon_write_vct(int x1, int y1, int x2, int y2)
	{
	fatal("80 column card VCT command not implemented yet");
	}
/*...e*/

/*...smon_write_ctrl:0:*/
static void mon_write_ctrl(char c)
	{
	switch ( c )
		{
/*...sNUL:16:*/
case 0:
	break;
/*...e*/
/*...sDOT:16:*/
case 1:
	mon_state = STATE_DOT_X;
	break;
/*...e*/
/*...sVCT:16:*/
case 2:
	mon_state = STATE_VCT_X1;
	break;
/*...e*/
/*...sCXY:16:*/
case 3:
	mon_state = STATE_CXY_X;
	break;
/*...e*/
/*...sBKG:16:*/
case 4:
	mon_state = STATE_BKG_N;
	break;
/*...e*/
/*...sEOL:16:*/
case 5:
	mon_write_eol();
	break;
/*...e*/
/*...sATR:16:*/
case 6:
	mon_state = STATE_ATR_M;
	break;
/*...e*/
/*...sBEL:16:*/
case 7:
	mon_write_bel();
	break;
/*...e*/
/*...sBS:16:*/
case 8:
	mon_write_bs();
	break;
/*...e*/
/*...sTAB:16:*/
case 9:
	mon_write_tab();
	break;
/*...e*/
/*...sLF:16:*/
case 10:
	mon_write_lf();
	break;
/*...e*/
/*...sUP:16:*/
case 11:
	mon_write_up();
	break;
/*...e*/
/*...sCLR:16:*/
case 12:
	mon_write_clr();
	break;
/*...e*/
/*...sCR:16:*/
case 13:
	mon_write_cr();
	break;
/*...e*/
/*...sBL:16:*/
case 14:
	mon_print_attr |= ATTR_BLINK;
	break;
/*...e*/
/*...sBLO:16:*/
case 15:
	mon_print_attr &= ~ATTR_BLINK;
	break;
/*...e*/
/*...sBLK\44\RED\44\GRN\44\YEL\44\BLU\44\MAG\44\CYN\44\WHT:16:*/
case 16:
case 17:
case 18:
case 19:
case 20:
case 21:
case 22:
case 23:
	mon_write_fg((byte)(c&7));
	break;
/*...e*/
/*...sINI:16:*/
case 24:
	mon_write_ini();
	break;
/*...e*/
/*...sFWD:16:*/
case 25:
	mon_write_fwd();
	break;
/*...e*/
/*...sHME:16:*/
case 26:
	mon_write_hme();
	break;
/*...e*/
/*...sESC:16:*/
case 27:
	mon_state = STATE_ESC_CH;
	break;
/*...e*/
/*...sSCR:16:*/
case 28:
	mon_scroll = TRUE;
	break;
/*...e*/
/*...sPGE:16:*/
case 29:
	mon_scroll = FALSE;
	break;	
/*...e*/
/*...sCON:16:*/
case 30:
	mon_cursor_on = TRUE;
	break;
/*...e*/
/*...sCSO:16:*/
case 31:
	mon_cursor_on = FALSE;
	break;	
/*...e*/
		}
	}
/*...e*/
/*...smon_write_esc:0:*/
static void mon_write_esc(char c)
	{
	switch ( c )
		{
/*...sS \45\ standard character font:16:*/
case 'S':
	mon_state = STATE_NORMAL;
	mon_mode = MODE_STANDARD;
	break;
/*...e*/
/*...sA \45\ alternate character font:16:*/
case 'A':
	mon_state = STATE_NORMAL;
	mon_mode = MODE_ALTERNATE;
	break;
/*...e*/
/*...sG \45\ graphics character font:16:*/
case 'G':
	mon_state = STATE_NORMAL;
	mon_mode = MODE_GRAPHIC;
	break;
/*...e*/
/*...sC \45\ scroll mode:16:*/
case 'C':
	mon_state = STATE_NORMAL;
	mon_scroll = TRUE;
	break;
/*...e*/
/*...sD \45\ page mode:16:*/
case 'D':
	mon_state = STATE_NORMAL;
	mon_scroll = FALSE;
	break;
/*...e*/
/*...sE \45\ cursor on:16:*/
case 'E':
	mon_state = STATE_NORMAL;
	mon_cursor_on = TRUE;
	break;
/*...e*/
/*...sF \45\ cursor off:16:*/
case 'F':
	mon_state = STATE_NORMAL;
	mon_cursor_on = FALSE;
	break;
/*...e*/
/*...sX \45\ simulate control character:16:*/
case 'X':
	mon_state = STATE_ESC_X_CH;
	break;
/*...e*/
/*...sI \45\ insert line:16:*/
case 'I':
	mon_state = STATE_NORMAL;
	mon_write_inslin();
	break;
/*...e*/
/*...sJ \45\ delete line:16:*/
case 'J':
	mon_state = STATE_NORMAL;
	mon_write_dellin();
	break;
/*...e*/
/*...sW \45\ set write mask:16:*/
/* Note that the FDX manual says that the argument may be in ASCII or binary,
   but this is not true. ACRT.MAC insists on ASCII, and ASCRT.MAC doesn't
   implement write masks at all. */

case 'W':
	switch ( c )
		{
		case '0': mon_write_mask = 0xe0; break;
		case '1': mon_write_mask = 0xc0; break;
		case '2': mon_write_mask = 0xa0; break;
		}
	break;
/*...e*/
/*...sT \45\ set printing attribute:16:*/
case 'T':
	mon_state = STATE_ESC_T_N;
	break;
/*...e*/
/*...sU \45\ set non\45\printing attribute:16:*/
case 'U':
	mon_state = STATE_ESC_U_N;
	break;
/*...e*/
/*...sV \45\ set both attributes:16:*/
case 'V':
	mon_state = STATE_ESC_V_N;
	break;
/*...e*/
/*...sP \45\ set bit n of printing attribute:16:*/
case 'P':
	mon_state = STATE_ESC_P_N;
	break;
/*...e*/
/*...sN \45\ set bit n of non\45\printing attribute:16:*/
case 'N':
	mon_state = STATE_ESC_N_N;
	break;
/*...e*/
/*...sB \45\ set bit n of both attributes:16:*/
case 'B':
	mon_state = STATE_ESC_B_N;
	break;
/*...e*/
/*...sQ \45\ SCPM \45\ quicker screen update:16:*/
case 'Q':
	/* This is an SCPM ROM special feature, aka "quicker" screen update.
	   It clears the screen and then disables subsequent colour updates.
	   We just do the clear screen. */
	mon_write_clr();
	mon_ascrt_no_colour = TRUE;
	mon_state = STATE_NORMAL;
	break;
/*...e*/
/*...sR \45\ SCPM \45\ restore colour update:16:*/
case 'R':
	/* This is an SCPM ROM special feature, aka "restore" slow update.
	   It enables colour updates, then clears the screen. */
	mon_ascrt_no_colour = FALSE;
	mon_write_clr();
	mon_state = STATE_NORMAL;
	break;
/*...e*/
/*...sdefault:16:*/
default:
	mon_state = STATE_NORMAL;
	break;
/*...e*/
		}
	}
/*...e*/
/*...smon_write_char:0:*/
/*...smon_char_to_glyph:0:*/
static byte mon_char_to_glyph(char c)
	{
	/* Convert from (7-bit) character to (8-bit) glyph */
	switch ( mon_mode )
		{
		case MODE_STANDARD:
			/* Normal alpha characters are glyphs 32-127.
			   I suppose this should say c & ~0x80,
			   but neither ACRT.MAC nor ASCRT.MAC do. */
			return (byte) c;
		case MODE_ALTERNATE:
			/* Alternate alpha characters are 160-255. */
			return (byte) ( c | 0x80 );
		case MODE_GRAPHIC:
			/* Special graphic characters are glyphs
			   0-31 and 128-159.
			   This algorithm is bizarre, but reflects
			   both ACRT.MAC and ASCRT.MAC. */
			if ( c & 0x40 )
				c = ((c & 0x20)<<2) | (c & 0x1f);
			return (byte) c;
		}
	return 0; /* Should never happen, but keep compiler happy */
	}
/*...e*/

static void mon_write_char(char c)
	{
	word adr = (((((word)(mon_tbuf->reg[12] & 0x07))<<8) |
	               (word) mon_tbuf->reg[13]              ) + mon_y*COLUMNS+mon_x) & 0x7ff;
	mon_tbuf->ram[adr].ch = mon_char_to_glyph(c);
	mon_tbuf->ram[adr].at = mon_print_attr;
    DRAW_GLYPH (mon_win, adr);
	mon_write_fwd();
	}
/*...e*/

#ifndef __Pico__
void mon_refresh_win(void)
	{
	int x, y;
    if ( mon_emu == 0 ) return;
	word adr = ((((word)(mon_tbuf->reg[12] & 0x07))<<8) | (word)mon_tbuf->reg[13]);
	for ( y = 0; y < ROWS; y++ )
		{
		for ( x = 0; x < COLUMNS; x++ )
			{
			adr   &= 0x7ff;
			if (mon_scrolled || (mon_tbuf->ram[adr].at & ATTR_BLINK)) twin_draw_glyph (mon_win, adr);
			++adr;
			}
		}
    mon_scrolled = FALSE;
	if ( mon_cursor_on && !mon_blink_blank && (mon_y<ROWS))
		/* Draw a white block over the character */
		{
        adr = (((((word)(mon_tbuf->reg[12] & 0x07))<<8) | (word)mon_tbuf->reg[13])
            + COLUMNS * mon_y + mon_x) & 0x7FF;
        twin_draw_csr (mon_win, adr);
		}
	win_refresh (mon_win);
	if ( mon_cursor_on && !mon_blink_blank && (mon_y<ROWS))
		/* Restore character at cursor */
		{
        adr = (((((word)(mon_tbuf->reg[12] & 0x07))<<8) | (word)mon_tbuf->reg[13])
            + COLUMNS * mon_y + mon_x) & 0x7FF;
        twin_draw_glyph (mon_win, adr);
		}
	}
#endif
/*...e*/
/*...smon_refresh_th:0:*/
#ifdef HAVE_TH

static int mon_map_to_th_col[] =
	{
	COL_BLACK,
	COL_RED,
	COL_GREEN,
	COL_YELLOW,
	COL_BLUE,
	COL_MAGENTA,
	COL_CYAN,
	COL_WHITE,
	};

#if defined(UNIX)
  #define COL_GRAPHICS COL(COL_BLACK,COL_WHITE,COL_STAND)
  #define COL_OUTSIDE  COL(COL_WHITE,COL_BLACK,COL_NORM)
#elif defined(WIN32)
  #define COL_GRAPHICS COL(COL_BLACK,COL_WHITE,FALSE,FALSE)
  #define COL_OUTSIDE  COL(COL_WHITE,COL_BLACK,FALSE,FALSE)
#else
  #error need to decide on colours on this platform
#endif

static void mon_refresh_th_glyph(byte glyph, byte attr)
	{
	int ch, col;
	if ( attr & ATTR_GRAPHICS )
		{
		ch = 'g';
		col = COL_GRAPHICS;
		}
	else
		{
		int fg = mon_map_to_th_col[ attr & ATTRS_FG      ];
		int bg = mon_map_to_th_col[(attr & ATTRS_BG) >> 3];
		if ( glyph >= 160 )
			/* 160-255 are alternatives glyphs for 32-127 */
			ch = (char) (glyph-128);
		else
			ch = (char) glyph;
#if defined(UNIX)
		col = COL(fg,bg,fg==COL_WHITE?COL_STAND:COL_NORM);
#elif defined(WIN32)
		col = COL(fg,bg,FALSE,FALSE);
#else
  #error need to decide colour on this platform
#endif
		}
	th_setcol(col);
	if ( glyph >= ' ' && glyph <= '~' )
		th_p_raw_chr((char) glyph);
	else
		th_p_raw_chr((char) '?');
	}

static void mon_refresh_th(void)
	{
	int w = ( x_size < COLUMNS ) ? x_size : COLUMNS;
	int h = ( y_size < ROWS    ) ? y_size : ROWS   ;
	byte  glyph, attr;
	int x, y;
	word adr = ((((word)(mon_tbuf->reg[12] & 0x07))<<8) |
	              (word) mon_tbuf->reg[13]              );
	th_setcsr(CSR_OFF);
	for ( y = 0; y < h; y++ )
		{
		th_cy(y);
		for ( x = 0; x < w; x++ )
			{
			adr   &= 0x7ff;
			glyph = mon_tbuf->ram[adr].ch;
			attr  = mon_tbuf->ram[adr].at;
			++adr;
			mon_refresh_th_glyph(glyph, attr);
			}
		th_setcol(COL_OUTSIDE);
		for ( ; x < x_size; x++ )
			th_p_raw_chr(' ');
		}
	th_setcol(COL_OUTSIDE);
	for ( ; y < y_size; y++ )
		{
		th_cy(y);
		th_deleol();
		}

	if ( mon_cursor_on )
		{
		th_setcsr(CSR_BLOCK);
		th_cxy(mon_x, mon_y);
		}
	else
		{
		/* Just in case this terminal type can't hide its cursor,
		   move it to the bottom right. */
		th_cxy(x_size-1, y_size-1);
		}

	th_sync_screen();
	}

#endif
/*...e*/
/*...smon_refresh:0:*/
void mon_refresh(void)
	{
    if ( mon_emu == 0 ) return;
	if ( mon_emu & MONEMU_WIN )
		mon_win_changed = TRUE; /* Cause a repaint soon */
#ifdef HAVE_TH
	if ( mon_emu & MONEMU_TH )
		mon_th_changed = TRUE; /* Cause a redraw soon */
#endif
	}
/*...e*/
/*...smon_refresh_blink:0:*/
/* This is called regularly, so we can make the cursor blink,
   and any flashing text. */

void mon_refresh_blink(void)
	{
    if ( mon_emu == 0 ) return;
#ifndef __Pico__
	BOOLEAN new_blink_blank = ( (get_millis()%1000) > 500 );
	if ( new_blink_blank != mon_blink_blank || mon_win_changed )
		{
		mon_blink_blank = new_blink_blank;
		mon_win_changed = FALSE;
		if ( mon_emu & MONEMU_WIN )
            {
            twin_set_blink (mon_blink_blank);
			mon_refresh_win();
            }
		}
#endif
#ifdef HAVE_TH
	if ( mon_th_changed )
		{
		mon_th_changed = FALSE;
		if ( mon_emu & MONEMU_TH )
			mon_refresh_th();
		}
#endif
	}

void mon_refresh_vdeb (void)
    {
    if ( mon_emu == 0 ) return;
#ifndef __Pico__
	if ( mon_emu & MONEMU_WIN ) mon_refresh_win ();
#endif
#ifdef HAVE_TH
	if ( mon_emu & MONEMU_TH ) mon_refresh_th ();
#endif
    }
/*...e*/

/*...smon_write_model:0:*/
/* Write a character, bearing in mind that we may have previously encountered
   a control character, or escape sequence, and now we must accumulate more
   input before actually performing an action. */

static int mon_x1, mon_y1, mon_x2, mon_y2;

static void mon_write_model(char c)
	{
	switch ( mon_state )
		{
/*...sSTATE_NORMAL:16:*/
case STATE_NORMAL:
	if ( (byte) c < ' ' )
		mon_write_ctrl(c);
	else
		mon_write_char(c);
	break;
/*...e*/
/*...sSTATE_DOT_X:16:*/
case STATE_DOT_X:
	mon_x1 = (byte)c - 32;
	mon_state = STATE_DOT_Y;
	break;
/*...e*/
/*...sSTATE_DOT_Y:16:*/
case STATE_DOT_Y:
	mon_y1 = (byte)c - 32;
	mon_state = STATE_NORMAL;
	mon_write_dot(mon_x1, mon_y1);
	break;
/*...e*/
/*...sSTATE_VCT_X1:16:*/
case STATE_VCT_X1:
	mon_x1 = (byte)c - 32;
	mon_state = STATE_VCT_Y1;
	break;
/*...e*/
/*...sSTATE_VCT_Y2:16:*/
case STATE_VCT_Y1:
	mon_y1 = (byte)c - 32;
	mon_state = STATE_VCT_X2;
	break;
/*...e*/
/*...sSTATE_VCT_X2:16:*/
case STATE_VCT_X2:
	mon_x2 = (byte)c - 32;
	mon_state = STATE_VCT_Y1;
	break;
/*...e*/
/*...sSTATE_VCT_Y2:16:*/
case STATE_VCT_Y2:
	mon_y2 = (byte)c - 32;
	mon_state = STATE_NORMAL;
	mon_write_vct(mon_x1, mon_y1, mon_x2, mon_y2);
	break;
/*...e*/
/*...sSTATE_CXY_X:16:*/
case STATE_CXY_X:
	mon_x1 = (byte)c - 32;
	mon_state = STATE_CXY_Y;
	break;
/*...e*/
/*...sSTATE_CXY_Y:16:*/
case STATE_CXY_Y:
	mon_y1 = (byte)c - 32;
	mon_state = STATE_NORMAL;
	mon_write_cxy(mon_x1, mon_y1);
	break;
/*...e*/
/*...sSTATE_BKG_N:16:*/
case STATE_BKG_N:
	mon_state = STATE_NORMAL;
	mon_write_bkg((byte) c);
	break;
/*...e*/
/*...sSTATE_ATR_M:16:*/
case STATE_ATR_M:
	mon_state = STATE_NORMAL;
	mon_write_atr((byte) c);
	break;
/*...e*/
/*...sSTATE_ESC_CH:16:*/
/* Note that in the FDX manual, ch. is incorrectly shown as being an
   argument to the ^Z HME character, not the ^[ ESC character. */
case STATE_ESC_CH:
	mon_write_esc(c);
	break;
/*...e*/
/*...sSTATE_ESC_X_CH:16:*/
case STATE_ESC_X_CH:
	mon_state = STATE_NORMAL;
	mon_write_ctrl(c&31);
	break;
/*...e*/
/*...sSTATE_ESC_T_N:16:*/
case STATE_ESC_T_N:
	mon_state = STATE_NORMAL;
	mon_print_attr = (byte) c;
	break;
/*...e*/
/*...sSTATE_ESC_U_N:16:*/
case STATE_ESC_U_N:
	mon_state = STATE_NORMAL;
	mon_non_print_attr = (byte) c;
	break;
/*...e*/
/*...sSTATE_ESC_V_N:16:*/
case STATE_ESC_V_N:
	mon_state = STATE_NORMAL;
	mon_print_attr = (byte) c;
	mon_non_print_attr = (byte) c;
	break;
/*...e*/
/*...sSTATE_ESC_P_N:16:*/
case STATE_ESC_P_N:
	mon_state = STATE_NORMAL;
	mon_write_bit_attr((int) c, &mon_print_attr);
	break;
/*...e*/
/*...sSTATE_ESC_N_N:16:*/
case STATE_ESC_N_N:
	mon_state = STATE_NORMAL;
	mon_write_bit_attr((int) c, &mon_non_print_attr);
	break;
/*...e*/
/*...sSTATE_ESC_B_N:16:*/
case STATE_ESC_B_N:
	mon_state = STATE_NORMAL;
	mon_write_bit_attr((int) c, &mon_print_attr);
	mon_write_bit_attr((int) c, &mon_non_print_attr);
	break;
/*...e*/
		}
	if ( mon_state == STATE_NORMAL )
		mon_refresh();
	}
/*...e*/
/*...smon_write_stdout:0:*/
#ifdef HAVE_CONSOLE
static void mon_write_stdout(char c)
	{
	if ( c >= ' ' && c <= '~' )
		fputc(c, stdout);
	else if ( c == '\r' || c == '\n' || c == '\t' || c == '\b' || c == 7 )
		fputc(c, stdout);
	else if ( c == 0 )
		; /* NUL, so ignore */
	else if ( c < ' ' )
		fprintf(stdout, "^%c", c+'@');
	else
		/* Display something for diagnostic reasons */
		fprintf(stdout, "<%02x>", c);
	fflush(stdout);
	}
#endif
/*...e*/
/*...smon_write:0:*/
void mon_write(char c)
	{
#ifdef HAVE_TH
	if ( mon_emu & (MONEMU_WIN|MONEMU_TH) )
#else
	if ( mon_emu & MONEMU_WIN )
#endif
		mon_write_model(c);
#ifdef HAVE_CONSOLE
	if ( mon_emu & MONEMU_CONSOLE )
		mon_write_stdout(c);
#endif
	}
/*...e*/
/*...smon_map_wk:0:*/
/* Map WK_ values to the values you'd get when reading through the
   keyboard driver. */

typedef struct { int wk, k; } KEYMAP;

static KEYMAP mon_keymap[] =
	{
		{WK_Page_Up ,0x1d}, {WK_End      ,0x05}, {WK_Pause       ,0x03},
		{WK_Tab     ,0x09}, {WK_Up       ,0x0b}, {WK_Delete      ,0x7f},
		{WK_Left    ,0x08}, {WK_Home     ,0x1a}, {WK_Right       ,0x19},
		{WK_Insert  ,0x15}, {WK_Down     ,0x0a}, {WK_Page_Down   ,0x0c},

		{WK_Num_Lock,0x1d}, {WK_KP_Divide,0x05}, {WK_KP_Multiply ,0x03},
		{WK_KP_Home ,0x09}, {WK_KP_Up    ,0x0b}, {WK_KP_Page_Up  ,0x7f},
		{WK_KP_Left ,0x08}, {WK_KP_Middle,0x1a}, {WK_KP_Right    ,0x19},
		{WK_KP_End  ,0x15}, {WK_KP_Down  ,0x0a}, {WK_KP_Page_Down,0x0c},

		{WK_F1,0x80}, {WK_F2,0x81}, {WK_F3,0x82}, {WK_F4,0x83},
		{WK_F5,0x84}, {WK_F6,0x85}, {WK_F7,0x86}, {WK_F8,0x87},
	};

static int mon_map_wk(int wk)
	{
	int i;
	for ( i = 0; i < sizeof(mon_keymap)/sizeof(mon_keymap[0]); i++ )
		if ( wk == mon_keymap[i].wk )
			return mon_keymap[i].k;
	return wk;
	}
/*...e*/
/*...e*/
/*...smon_kdb_map_th:0:*/
#ifdef HAVE_TH
static int mon_kbd_map_th(int k)
	{
	if ( k > 0 && k <= '~' )
		return k;
	switch ( k )
		{
		case K_DEL:		return '\b';
		case K_F1:		return 128;
		case K_F2:		return 129;
		case K_F3:		return 130;
		case K_F4:		return 131;
		case K_F5:		return 132;
		case K_F6:		return 133;
		case K_F7:		return 134;
		case K_F8:		return 135;
		case K_SHIFT_F1:	return 136;
		case K_SHIFT_F2:	return 137;
		case K_SHIFT_F3:	return 138;
		case K_SHIFT_F4:	return 139;
		case K_SHIFT_F5:	return 140;
		case K_SHIFT_F6:	return 141;
		case K_SHIFT_F7:	return 142;
		case K_SHIFT_F8:	return 143;
		}
	diag_message(DIAG_MON_KBD_MAP_TH, "mon_kbd_map_th keycode=0x%04x", k);
	return 0;
	}
#endif
/*...e*/
/*...smon_kbd_read:0:*/
/* This emulation is used by the CP/M emulation.
   Its allowed to suspend the emulation. */

int mon_kbd_read(void)
	{
#ifdef HAVE_TH
	if ( (mon_emu & (MONEMU_CONSOLE|MONEMU_WIN|MONEMU_TH)) == 0 )
#else
	if ( (mon_emu & (MONEMU_CONSOLE|MONEMU_WIN)) == 0 )
#endif
		fatal("blocking keyboard read attempted with no monitor keyboard device");
#ifdef HAVE_CONSOLE
	if ( mon_emu & MONEMU_CONSOLE )
		/* This will suspend the emulation,
		   which probably isn't a big issue most of the time.
		   The input has already been cooked by the host.
		   Note that any XWindows won't repaint if covered and exposed,
		   and any flashing graphics and cursor won't update. */
		{
		int ch = fgetc(stdin);
		if ( ch == '\n' )
			ch = '\r';
		return ch;
		}
#endif
#ifdef HAVE_TH
	if ( ( mon_emu & (MONEMU_WIN|MONEMU_TH) ) == MONEMU_TH )
		/* If only one source, then do a blocking wait */
		{
		int k, ch;
		mon_refresh_blink(); /* Make sure screen is correct before suspending */
		k = th_key_read();
		ch = mon_kbd_map_th(k);
		return ch;
		}
#endif
	for ( ;; )
		{
		if ( mon_emu & MONEMU_WIN )
			{
            int wk = twin_kbd_test (mon_win);
            if ( wk >= 0 )
                {
                int ch = mon_map_wk (wk);
                if ( ch != 0 )
                    return ch;
                }
			}
#ifdef HAVE_TH
		if ( mon_emu & MONEMU_TH )
			{
			if ( th_key_status() != 0 )
				{
				int k = th_key_read();
				int ch = mon_kbd_map_th(k);
				if ( ch != 0 )
					return ch;
				}
			}
#endif
		mon_refresh_blink();
		delay_millis(20);
		}
	}
/*...e*/
/*...smon_kbd_read_non_wait:0:*/
int mon_kbd_read_non_wait(void)
	{
#ifdef HAVE_CONSOLE
	if ( mon_emu & MONEMU_CONSOLE )
		{
		if ( mon_emu & MONEMU_CONSOLE_NOKEY )
			/* User has said there will never be any input */
			return 0;
		/* This will suspend the emulation,
		   which could be a big issue (supposed to be non-blocking).
		   The input has already been cooked by the host.
		   Worse, any XWindows won't repaint if covered and exposed,
		   and any flashing graphics and cursor won't update. */
		mon_kbd_pressed = !mon_kbd_pressed;
		if ( mon_kbd_pressed )
			{
			int ch = fgetc(stdin);
			if ( ch == '\n' )
				ch = '\r';
			return ch;
			}
		else
			return 0;
		}
#endif
	if ( mon_emu & MONEMU_WIN )
		{
        int wk = twin_kbd_test (mon_win);
        if ( wk >= 0 )
            {
            int ch = mon_map_wk (wk);
            if ( ch != 0 )
                return ch;
            }
		win_handle_events();
		mon_refresh_blink();
		}
#ifdef HAVE_TH
	if ( mon_emu & MONEMU_TH )
		{
		if ( th_key_status() != 0 )
			{
			int k = th_key_read();
			int ch = mon_kbd_map_th(k);
			if ( ch != 0 )
				return ch;
			}
		}
#endif
	return 0;
	}
/*...e*/
/*...smon_kbd_status:0:*/
BOOLEAN mon_kbd_status(void)
	{
#ifdef HAVE_CONSOLE
	if ( mon_emu & MONEMU_CONSOLE )
		{
		if ( mon_emu & MONEMU_CONSOLE_NOKEY )
			/* There will never be any key */
			return FALSE;
		/* By answering TRUE here, we can cause the program
		   to try to read the key, and thus suspend */
		return TRUE;
		}
#endif
	if ( mon_emu & MONEMU_WIN )
		{
		if ( twin_kbd_stat (mon_win) )
			return TRUE;
		win_handle_events();
		mon_refresh_blink();
		}
#ifdef HAVE_TH
	if ( mon_emu & MONEMU_TH )
		if ( th_key_status() != 0 )
			return TRUE;
#endif
	return FALSE;
	}
/*...e*/

/*...smon_init:0:*/
/*...skeypress:0:*/
static void keypress (WIN *win, int wk)
	{
#if HAVE_CONFIG
    if ( test_cfg_key (wk) ) return;
#endif
	kbd_win_keypress (win, wk);
	twin_keypress (win, wk);
	}
/*...e*/
/*...skeyrelease:0:*/
static void keyrelease (WIN *win, int wk)
	{
	kbd_win_keyrelease (win, wk);
	twin_keyrelease (win, wk);
	}
/*...e*/

void mon_init(int emu, int width_scale, int height_scale)
	{
    diag_message (DIAG_INIT, "mon_init (0x%02X, %d, %d)", emu, width_scale, height_scale);
	mon_emu = emu;

	mon_crtc_address    = 0;
	mon_adr_hi          = 0;
	mon_adr_lo          = 0;
	mon_ascd            = 0;
	mon_atrd            = 0;

	mon_print_attr      = ATTR_FG_G|ATTR_FG_B;
	mon_non_print_attr  = ATTR_FG_G|ATTR_FG_B;
	mon_cursor_on       = TRUE;
	mon_scroll          = TRUE;
	mon_write_mask      = 0xe0; /* means write to ASC and ATR */
	mon_ascrt_no_colour = FALSE;
	mon_mode            = MODE_STANDARD;
	mon_state           = STATE_NORMAL;

	mon_kbd_pressed     = TRUE; /* for stdin based emulation */

	if ( mon_emu & MONEMU_WIN )
		{
        mon_win = twin_create (width_scale, height_scale,
			mon_title ? mon_title : "Memu Monitor",
			mon_display, /* display */
			NULL, /* geometry */
            keypress, keyrelease,
            mon_emu & MONEMU_WIN_MONO
			);
        mon_tbuf = mon_win->tbuf;

        mon_write_clr();
        mon_win_changed = TRUE;
#ifndef __Pico__
        win_refresh(mon_win);
#endif
        win_show (mon_win);
		}
    else
        {
        mon_win = NULL;
        mon_tbuf = tbuf_create (mon_emu & MONEMU_WIN_MONO);
        mon_write_clr();
        }
        
#ifdef HAVE_TH
	if ( mon_emu & MONEMU_TH )
		{
		th_init(MODE_NORMAL, 0, 0);
		mon_th_changed = TRUE;
		}
#endif
	}
/*...e*/
/*...smon_term:0:*/
void mon_term(void)
	{
    diag_message (DIAG_INIT, "mon_term");
	if ( mon_emu & MONEMU_WIN )
		{
        win_delete(mon_win);
		mon_win = NULL;
		}
    else
        {
        free (mon_tbuf);
        }
    mon_tbuf = NULL;
#ifdef HAVE_TH
	if ( mon_emu & MONEMU_TH )
		th_deinit();
#endif
	mon_emu = MONEMU_IGNORE_INIT;
	}
/*...e*/
void mon_set_title (const char *title)
	{
    mon_title = title;
	}

const char * mon_get_title (void)
	{
	return mon_title;
	}

void mon_set_display (const char *display)
	{
    mon_display = display;
	}

const char * mon_get_display (void)
	{
	return mon_display;
	}

void mon_max_scale (int *pxscl, int *pyscl)
    {
    int ix;
    int iy;
    win_max_size (mon_display, &ix, &iy);
    ix /= 640;
    iy /= 240;
    if ( ( ix == 0 ) || ( iy <= 1 ) )
        {
        *pxscl = 1;
        *pyscl = 1;
        }
    else if ( iy >= 2 * ix )
        {
        *pxscl = ix;
        *pyscl = 2 * ix;
        }
    else
        {
        *pxscl = iy / 2;
        *pyscl = iy;
        }
    }

WIN * mon_getwin (void)
    {
    return mon_win;
    }

void mon_show (void)
    {
    if (mon_win != NULL) win_show (mon_win);
    }
