/*

ui.c - User Interface

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>

#include "types.h"
#include "common.h"
#include "win.h"
#include "kbd.h"
#include "mem.h"
#include "vid.h"
#include "monprom.h"
#include "dis.h"
#include "ui.h"

/*...vtypes\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...vmem\46\h:0:*/
/*...vvid\46\h:0:*/
/*...vmonprom\46\h:0:*/
/*...vdis\46\h:0:*/
/*...vui\46\h:0:*/
/*...e*/

/*...svars:0:*/
#define	COL_BLACK    0
#define	COL_BLUE     1
#define	COL_GREEN    2
#define	COL_CYAN     3
#define	COL_RED      4
#define	COL_MAGENTA  5
#define	COL_YELLOW   6
#define	COL_WHITE    7
#define	N_COLS_POPUP 8

static COL ui_cols[N_COLS_POPUP] =
	{
		{    0,   0,   0 },	/* black */
		{    0,   0, 255 },	/* blue */
		{    0, 255,   0 },	/* green */
		{    0, 255, 255 },	/* cyan */
		{  255,   0,   0 },	/* red */
		{  255,   0, 255 },	/* magenta */
		{  255, 255,   0 },	/* yellow */
		{  255, 255, 255 },	/* white */
	};

static const char *ui_mem_title = NULL;
static const char *ui_vram_title = NULL;
static const char *ui_dis_title = NULL;
static const char *ui_mem_display = NULL;
static const char *ui_vram_display = NULL;
static const char *ui_dis_display = NULL;

static byte ui_refreshes = 0;
#define	UI_FLASH 0x10
/*...e*/

/*...sVIRTKBD:0:*/
/* Deal with the fact keypresses can arrive on UI threads,
   but we want to pick them up on the main thread.
   This is admittedly a little unsafe.
   I'm not using any semaphores or critical sections.
   Instead I rely on the fact a single word store or fetch is likely to
   be atomic on almost all architectures, and I also rely on the fact that
   we poll for keys very quickly, and so aren't likely to miss one. */

typedef struct
	{
	int wk;
	} VIRTKBD;

static void ui_vk_keypress(VIRTKBD *vk, int wk)
	{
	vk->wk = wk;
	}

static int ui_vk_key(VIRTKBD *vk)
	{
	int wk;
	if ( (wk = vk->wk) != -1 )
		{
		vk->wk = -1;
		return wk;
		}
	else
		return -1;
	}

static void ui_vk_init(VIRTKBD *vk)
	{
	vk->wk = -1;
	}
/*...e*/

/*...sui_print_char:0:*/
static void ui_print_char(WIN *win, int x, int y, byte ch, int fg)
	{
	byte *p = win->data+(y*GLYPH_HEIGHT)*win->width+(x*GLYPH_WIDTH);
	int x2, y2;
	for ( y2 = 0; y2 < GLYPH_HEIGHT; y2++ )
		{
		for ( x2 = 0; x2 < GLYPH_WIDTH; x2++ )
			*p++ = ( (mon_alpha_prom[ch][y2]&(0x80>>x2)) != 0 )
				? fg : COL_BLACK;
		p += win->width-GLYPH_WIDTH;
		}
	}
/*...e*/
/*...sui_print_string:0:*/
static void ui_print_string(WIN *win, int x, int y, const char *s, int fg)
	{
	while ( *s )
		ui_print_char(win, x++, y, (byte) *s++, fg);
	}
/*...e*/
/*...sui_highlight:0:*/
static void ui_highlight(WIN *win, int x, int y, int w)
	{
	byte *p = win->data+(y*GLYPH_HEIGHT)*win->width+(x*GLYPH_WIDTH);
	int x2, y2;
	for ( y2 = 0; y2 < GLYPH_HEIGHT; y2++ )
		{
		for ( x2 = 0; x2 < GLYPH_WIDTH*w; x2++ )
			{
			if ( *p == COL_BLACK )
				*p = COL_RED;
			++p;
			}
		p += win->width-(GLYPH_WIDTH*w);
		}
	}
/*...e*/
/*...sui_invert:0:*/
static void ui_invert(WIN *win, int x, int y, int w)
	{
	byte *p = win->data+(y*GLYPH_HEIGHT)*win->width+(x*GLYPH_WIDTH);
	int x2, y2;
	for ( y2 = 0; y2 < GLYPH_HEIGHT; y2++ )
		{
		for ( x2 = 0; x2 < GLYPH_WIDTH*w; x2++ )
			{
			*p ^= (N_COLS_POPUP-1);
			++p;
			}
		p += win->width-(GLYPH_WIDTH*w);
		}
	}
/*...e*/

/*...sis_hex:0:*/
static BOOLEAN is_hex(int wk)
	{
	return ( wk >= '0' && wk <= '9' ) || ( wk >= 'a' && wk <= 'f' );
	}
/*...e*/
/*...sval_hex:0:*/
static int val_hex(int wk)
	{
	if ( wk >= '0' && wk <= '9' )
		return wk-'0';
	if ( wk >= 'a' && wk <= 'f' )
		return wk-'a'+10;
	return -1; /* Not reached */
	}
/*...e*/
/*...supdate_byte:0:*/
static BOOLEAN update_byte(byte *b, int *digit, int wk, BOOLEAN *next)
	{
	*next = FALSE;
	switch ( wk )
		{
		case WK_BackSpace:
		case ' ':
			*digit = 1-*digit;
			return TRUE;
		}
	if ( ! is_hex(wk) )
		return FALSE;
	switch ( *digit )
		{
		case 0:
			*b = (*b & 0x0f) | (val_hex(wk)<<4);
			*digit = 1;
			break; 
		case 1:
			*b = (*b & 0xf0) |  val_hex(wk)    ;
			*digit = 0;
			*next = TRUE;
			break; 
		}
	return TRUE;
	}
/*...e*/
/*...supdate_word:0:*/
static BOOLEAN update_word(word *w, int *digit, int wk, BOOLEAN *next)
	{
	*next = FALSE;
	switch ( wk )
		{
		case WK_BackSpace:
			*digit = (*digit-1)%4;
			return TRUE;
		case ' ':
			*digit = (*digit+1)%4;
			return TRUE;
		}
	if ( !is_hex(wk) )
		return FALSE;
	switch ( *digit )
		{
		case 0:
			*w = (*w & 0x0fff) | (val_hex(wk)<<12);
			*digit = 1;
			break;
		case 1:
			*w = (*w & 0xf0ff) | (val_hex(wk)<< 8);
			*digit = 2;
			break;
		case 2:
			*w = (*w & 0xff0f) | (val_hex(wk)<< 4);
			*digit = 3;
			break;
		case 3:
			*w = (*w & 0xfff0) |  val_hex(wk)     ;
			*digit = 0;
			*next = TRUE;
			break;
		}
	return TRUE;
	}
/*...e*/

/*...sMemory:0:*/
/* Show the contents of memory. */

#define	UI_MEM_COLS 16
#define	UI_MEM_ROWS 32

#define	UI_MEM_WIDTH  ((4+1+UI_MEM_COLS*3+UI_MEM_COLS)*GLYPH_WIDTH)
#define	UI_MEM_HEIGHT ((1+UI_MEM_ROWS)*GLYPH_HEIGHT)

static WIN *ui_mem_win = NULL;
#define	UI_MEM_FOCUS_IOBYTE  0
#define	UI_MEM_FOCUS_SUBPAGE 1
#define	UI_MEM_FOCUS_START   2
#define	UI_MEM_FOCUS_ADDR    3
#define	UI_MEM_FOCUS_DATA    4
#define	UI_MEM_FOCUS_COUNT   5
static int ui_mem_focus = UI_MEM_FOCUS_IOBYTE;
static byte ui_mem_iobyte = 0x00;
static byte ui_mem_subpage = 0x00;
static word ui_mem_start = 0x0000;
static word ui_mem_addr = 0x0000;
static int ui_mem_digit = 0;
static BOOLEAN ui_mem_snapshot = FALSE;

static VIRTKBD ui_mem_vk;

/*...sui_mem_keypress:0:*/
static void ui_mem_keypress(WIN *win, int wk)
	{
	ui_vk_keypress(&ui_mem_vk, wk);
	}
/*...e*/
/*...sui_mem_keyrelease:0:*/
static void ui_mem_keyrelease(WIN *win, int wk)
	{
	}
/*...e*/
/*...sui_mem_refresh:0:*/
/*...sui_mem_key:0:*/
static void ui_mem_key(int wk)
	{
	BOOLEAN next;
	switch ( wk )
		{
		case 'i':
			ui_mem_focus = UI_MEM_FOCUS_IOBYTE;
			ui_mem_digit = 0;
			return;
		case 'u':
			ui_mem_focus = UI_MEM_FOCUS_SUBPAGE;
			ui_mem_digit = 0;
			return;
		case 's':
			ui_mem_focus = UI_MEM_FOCUS_START;
			ui_mem_digit = 0;
			return;
		case 'p':
			ui_mem_focus = UI_MEM_FOCUS_ADDR;
			ui_mem_digit = 0;
			return;
		case 't':
			ui_mem_focus = UI_MEM_FOCUS_DATA;
			ui_mem_digit = 0;
			return;
		case 'o':
			mem_snapshot();
			return;
		case 'v':
			ui_mem_snapshot ^= TRUE;
			return;
		case WK_Tab:
			ui_mem_focus = (ui_mem_focus+1) % UI_MEM_FOCUS_COUNT;
			ui_mem_digit = 0;
			return;
		case WK_Left:
		case WK_KP_Left:
			--ui_mem_addr;
			if ( (sword)(ui_mem_addr-ui_mem_start) < 0 )
				ui_mem_start -= UI_MEM_COLS;
			return;
		case WK_Right:
		case WK_KP_Right:
			++ui_mem_addr;
			if ( (sword)(ui_mem_addr-ui_mem_start) >= UI_MEM_ROWS*UI_MEM_COLS )
				ui_mem_start += UI_MEM_COLS;
			return;
		case WK_Up:
		case WK_KP_Up:
			ui_mem_addr -= UI_MEM_COLS;
			if ( (sword)(ui_mem_addr-ui_mem_start) < 0 )
				ui_mem_start -= UI_MEM_COLS;
			return;
		case WK_Down:
		case WK_KP_Down:
			ui_mem_addr += UI_MEM_COLS;
			if ( (sword)(ui_mem_addr-ui_mem_start) >= UI_MEM_ROWS*UI_MEM_COLS )
				ui_mem_start += UI_MEM_COLS;
			return;
		case WK_Page_Up:
		case WK_KP_Page_Up:
			ui_mem_addr  -= UI_MEM_ROWS*UI_MEM_COLS;
			ui_mem_start -= UI_MEM_ROWS*UI_MEM_COLS;
			return;
		case WK_Page_Down:
		case WK_KP_Page_Down:
			ui_mem_addr  += UI_MEM_ROWS*UI_MEM_COLS;
			ui_mem_start += UI_MEM_ROWS*UI_MEM_COLS;
			return;
		}
	switch ( ui_mem_focus )
		{
		case UI_MEM_FOCUS_IOBYTE:
			update_byte(&ui_mem_iobyte, &ui_mem_digit, wk, &next);
			break;
		case UI_MEM_FOCUS_SUBPAGE:
			update_byte(&ui_mem_subpage, &ui_mem_digit, wk, &next);
			break;
		case UI_MEM_FOCUS_START:
			update_word(&ui_mem_start, &ui_mem_digit, wk, &next);
			if ( ui_mem_addr < ui_mem_start )
				ui_mem_addr = ui_mem_start;
			else if ( ui_mem_addr >= ui_mem_start+UI_MEM_ROWS*UI_MEM_COLS )
				ui_mem_addr = ui_mem_start+(UI_MEM_ROWS*UI_MEM_COLS-1);
			break;
		case UI_MEM_FOCUS_ADDR:
			update_word(&ui_mem_addr, &ui_mem_digit, wk, &next);
			if ( ui_mem_addr < ui_mem_start )
				{
				ui_mem_start = ui_mem_addr;
				ui_mem_start /= UI_MEM_COLS;
				ui_mem_start *= UI_MEM_COLS;
				}
			else if ( ui_mem_addr >= ui_mem_start+UI_MEM_ROWS*UI_MEM_COLS )
				{
				ui_mem_start = ui_mem_addr;
				ui_mem_start /= UI_MEM_COLS;
				ui_mem_start *= UI_MEM_COLS;
				ui_mem_start -= (UI_MEM_ROWS-1)*UI_MEM_COLS;
				}
			break;
		case UI_MEM_FOCUS_DATA:
			{
			byte iobyte_saved = mem_get_iobyte();
			byte b;
			mem_set_iobyte(ui_mem_iobyte);
			b = mem_read_byte(ui_mem_addr);
			update_byte(&b, &ui_mem_digit, wk, &next);
			mem_write_byte(ui_mem_addr, b);
			mem_set_iobyte(iobyte_saved);
			if ( next )
				{
				++ui_mem_addr;
				if ( (sword)(ui_mem_addr-ui_mem_start) >= UI_MEM_ROWS*UI_MEM_COLS )
					ui_mem_start += UI_MEM_COLS;
				}
			}
			break;
		}
	}
/*...e*/

static void ui_mem_refresh(void)
	{
	if ( ui_mem_win != NULL )
		{
		word addr = ui_mem_start;
		char s[100+1];
		byte iobyte_saved = mem_get_iobyte();
		byte subpage_saved = mem_get_rom_subpage();
		int x, y;
		int wk;
		if ( (wk = ui_vk_key(&ui_mem_vk)) != -1 )
			ui_mem_key(wk);
		mem_set_iobyte(ui_mem_iobyte);
		mem_set_rom_subpage(ui_mem_subpage);
		sprintf(s, "iobyte %02x subpage %02x start %04x address %04x %s",
			ui_mem_iobyte,
			ui_mem_subpage,
			(unsigned) ui_mem_start,
			(unsigned) ui_mem_addr,
			ui_mem_snapshot ? "snapshot" : "        "
			);
		ui_print_string(ui_mem_win,0,0, s, COL_YELLOW);
		ui_highlight(ui_mem_win,40,0,4);
		for ( y = 0; y < UI_MEM_ROWS; y++ )
			{
			sprintf(s, "%04x", addr);
			ui_print_string(ui_mem_win,0,y+1, s, COL_YELLOW);
			for ( x = 0; x < UI_MEM_COLS; x++ )
				{
				byte b  = mem_read_byte(addr);
				int fg;
				switch ( mem_type_at_address(addr) )
					{
					case MEMT_VOID:
						b = mem_read_byte(addr);
						fg = COL_BLUE;
						break;
					case MEMT_ROM:
						b = mem_read_byte(addr);
						fg = COL_MAGENTA;
						break;
					case MEMT_RAM_NO_SNAPSHOT:
						b = mem_read_byte(addr);
						fg = COL_CYAN;
						break;
					case MEMT_RAM_SNAPSHOT:
						{
						byte bs = mem_read_byte_snapshot(addr);
						if ( ui_mem_snapshot )
							{
							b = bs;
							fg = COL_GREEN;
							}
						else
							fg = ( b == bs ) ? COL_GREEN : COL_WHITE;
						}
						break;
                    default:    // To keep compiler happy.
                        fg = COL_WHITE;
                        break;
					}
				addr++;
				sprintf(s, "%02x", (unsigned)b);
				ui_print_string(ui_mem_win,5+x*3,y+1, s, fg);
				ui_print_char(ui_mem_win,5+UI_MEM_COLS*3+x,y+1, b, fg);
				}
			}
		mem_set_rom_subpage(subpage_saved);
		mem_set_iobyte(iobyte_saved);
		y = ((word)(ui_mem_addr-ui_mem_start))/UI_MEM_COLS;
		x = ((word)(ui_mem_addr-ui_mem_start))%UI_MEM_COLS;
		ui_highlight(ui_mem_win,5+x*3,1+y,2);
		ui_highlight(ui_mem_win,5+UI_MEM_COLS*3+x,1+y,1);
		switch ( ui_mem_focus )
			{
			case UI_MEM_FOCUS_IOBYTE:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_mem_win,7+ui_mem_digit,0,1);
				break;
			case UI_MEM_FOCUS_SUBPAGE:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_mem_win,18+ui_mem_digit,0,1);
				break;
			case UI_MEM_FOCUS_START:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_mem_win,27+ui_mem_digit,0,1);
				break;
			case UI_MEM_FOCUS_ADDR:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_mem_win,40+ui_mem_digit,0,1);
				break;
			case UI_MEM_FOCUS_DATA:
				if ( ui_refreshes & UI_FLASH )
					{
					ui_invert(ui_mem_win,5+x*3+ui_mem_digit,1+y,1);
					ui_invert(ui_mem_win,5+UI_MEM_COLS*3+x,1+y,1);
					}
				break;
			}
		win_refresh(ui_mem_win);
		}
	}
/*...e*/

/*...sui_mem_init:0:*/
static void ui_mem_init(void)
	{
	ui_vk_init(&ui_mem_vk);
	ui_mem_win = win_create(
		UI_MEM_WIDTH, UI_MEM_HEIGHT,
		1, 1,
		ui_mem_title ? ui_mem_title : "Memu Memory Inspector",
		ui_mem_display, /* display */
		NULL, /* geometry */
		ui_cols, N_COLS_POPUP,
		ui_mem_keypress,
		ui_mem_keyrelease
		);
	win_refresh(ui_mem_win);
	}
/*...e*/
/*...sui_mem_term:0:*/
static void ui_mem_term(void)
	{
	if ( ui_mem_win != NULL )
		{
		win_delete(ui_mem_win);
		ui_mem_win = NULL;
		}
	}
/*...e*/
/*...e*/
/*...sVRAM:0:*/
/* Show the contents of VRAM.
   Pretend we have a 16 bit address space (ie: address is word),
   and mask it down to 16KB at the last moment when we display. */

#define	UI_VRAM_COLS 16
#define	UI_VRAM_ROWS 32

#define	UI_VRAM_WIDTH  ((4+1+UI_VRAM_COLS*3+UI_VRAM_COLS)*GLYPH_WIDTH)
#define	UI_VRAM_HEIGHT ((1+UI_VRAM_ROWS)*GLYPH_HEIGHT)

static WIN *ui_vram_win = NULL;
#define	UI_VRAM_FOCUS_START  0
#define	UI_VRAM_FOCUS_ADDR   1
#define	UI_VRAM_FOCUS_DATA   2
#define	UI_VRAM_FOCUS_COUNT  3
static int ui_vram_focus = UI_VRAM_FOCUS_START;
static word ui_vram_start = 0x0000;
static word ui_vram_addr = 0x0000;
static int ui_vram_digit = 0;

static VIRTKBD ui_vram_vk;

/*...sui_vram_keypress:0:*/
static void ui_vram_keypress(WIN *win, int wk)
	{
	ui_vk_keypress(&ui_vram_vk, wk);
	}
/*...e*/
/*...sui_vram_keyrelease:0:*/
static void ui_vram_keyrelease(WIN *win, int wk)
	{
	}
/*...e*/
/*...sui_vram_refresh:0:*/
/*...sui_vram_key:0:*/
static void ui_vram_key(int wk)
	{
	BOOLEAN next;
	switch ( wk )
		{
		case 's':
			ui_vram_focus = UI_VRAM_FOCUS_START;
			ui_vram_digit = 0;
			return;
		case 'p':
			ui_vram_focus = UI_VRAM_FOCUS_ADDR;
			ui_vram_digit = 0;
			return;
		case 't':
			ui_vram_focus = UI_VRAM_FOCUS_DATA;
			ui_vram_digit = 0;
			return;
		case WK_Tab:
			ui_vram_focus = (ui_vram_focus+1) % UI_VRAM_FOCUS_COUNT;
			ui_vram_digit = 0;
			return;
		case WK_Left:
		case WK_KP_Left:
			--ui_vram_addr;
			if ( (sword)(ui_vram_addr-ui_vram_start) < 0 )
				ui_vram_start -= UI_VRAM_COLS;
			return;
		case WK_Right:
		case WK_KP_Right:
			++ui_vram_addr;
			if ( (sword)(ui_vram_addr-ui_vram_start) >= UI_VRAM_ROWS*UI_VRAM_COLS )
				ui_vram_start += UI_VRAM_COLS;
			return;
		case WK_Up:
		case WK_KP_Up:
			ui_vram_addr -= UI_VRAM_COLS;
			if ( (sword)(ui_vram_addr-ui_vram_start) < 0 )
				ui_vram_start -= UI_VRAM_COLS;
			return;
		case WK_Down:
		case WK_KP_Down:
			ui_vram_addr += UI_VRAM_COLS;
			if ( (sword)(ui_vram_addr-ui_vram_start) >= UI_VRAM_ROWS*UI_VRAM_COLS )
				ui_vram_start += UI_VRAM_COLS;
			return;
		case WK_Page_Up:
		case WK_KP_Page_Up:
			ui_vram_addr  -= UI_VRAM_ROWS*UI_VRAM_COLS;
			ui_vram_start -= UI_VRAM_ROWS*UI_VRAM_COLS;
			return;
		case WK_Page_Down:
		case WK_KP_Page_Down:
			ui_vram_addr  += UI_VRAM_ROWS*UI_VRAM_COLS;
			ui_vram_start += UI_VRAM_ROWS*UI_VRAM_COLS;
			return;
		}
	switch ( ui_vram_focus )
		{
		case UI_VRAM_FOCUS_START:
			update_word(&ui_vram_start, &ui_vram_digit, wk, &next);
			if ( ui_vram_addr < ui_vram_start )
				ui_vram_addr = ui_vram_start;
			else if ( ui_vram_addr >= ui_vram_start+UI_VRAM_ROWS*UI_VRAM_COLS )
				ui_vram_addr = ui_vram_start+UI_VRAM_ROWS*UI_VRAM_COLS-1;
			break;
		case UI_VRAM_FOCUS_ADDR:
			update_word(&ui_vram_addr, &ui_vram_digit, wk, &next);
			if ( ui_vram_addr < ui_vram_start )
				{
				ui_vram_start = ui_vram_addr;
				ui_vram_start /= UI_VRAM_COLS;
				ui_vram_start *= UI_VRAM_COLS;
				}
			else if ( ui_vram_addr >= ui_vram_start+UI_VRAM_ROWS*UI_VRAM_COLS )
				{
				ui_vram_start = ui_vram_addr;
				ui_vram_start /= UI_VRAM_COLS;
				ui_vram_start *= UI_VRAM_COLS;
				ui_vram_start -= (UI_VRAM_ROWS-1)*UI_VRAM_COLS;
				}
			break;
		case UI_VRAM_FOCUS_DATA:
			{
			byte b = vid_vram_read(ui_vram_addr);
			update_byte(&b, &ui_vram_digit, wk, &next);
			vid_vram_write(ui_vram_addr, b);
			if ( next )
				{
				++ui_vram_addr;
				if ( (sword)(ui_vram_addr-ui_vram_start) >= UI_VRAM_ROWS*UI_VRAM_COLS )
					ui_vram_start += UI_VRAM_COLS;
				}
			}
			break;
		}
	}
/*...e*/

static void ui_vram_refresh(void)
	{
	if ( ui_vram_win != NULL )
		{
		word addr = ui_vram_start;
		char s[30+1];
		int x, y;
		int wk;
		if ( (wk = ui_vk_key(&ui_vram_vk)) != -1 )
			ui_vram_key(wk);
		sprintf(s, "start %04x address %04x", (unsigned) ui_vram_start&0x3fff, (unsigned) ui_vram_addr&0x3fff);
		ui_print_string(ui_vram_win,0,0, s, COL_YELLOW);
		ui_highlight(ui_vram_win,19,0,4);
		for ( y = 0; y < UI_VRAM_ROWS; y++ )
			{
			sprintf(s, "%04x", addr&0x3fff);
			ui_print_string(ui_vram_win,0,y+1, s, COL_YELLOW);
			for ( x = 0; x < UI_VRAM_COLS; x++ )
				{
				byte b = vid_vram_read(addr++);
				sprintf(s, "%02x", (unsigned)b);
				ui_print_string(ui_vram_win,5+x*3,y+1, s, COL_CYAN);
				ui_print_char(ui_vram_win,5+UI_VRAM_COLS*3+x,y+1, b, COL_CYAN);
				}
			}
		y = ((word)(ui_vram_addr-ui_vram_start))/UI_VRAM_COLS;
		x = ((word)(ui_vram_addr-ui_vram_start))%UI_VRAM_COLS;
		ui_highlight(ui_vram_win,5+x*3,1+y,2);
		ui_highlight(ui_vram_win,5+UI_VRAM_COLS*3+x,1+y,1);
		switch ( ui_vram_focus )
			{
			case UI_VRAM_FOCUS_START:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_vram_win,6+ui_vram_digit,0,1);
				break;
			case UI_VRAM_FOCUS_ADDR:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_vram_win,19+ui_vram_digit,0,1);
				break;
			case UI_VRAM_FOCUS_DATA:
				if ( ui_refreshes & UI_FLASH )
					{
					ui_invert(ui_vram_win,5+x*3+ui_vram_digit,1+y,1);
					ui_invert(ui_vram_win,5+UI_VRAM_COLS*3+x,1+y,1);
					}
				break;
			}
		win_refresh(ui_vram_win);
		}
	}
/*...e*/

/*...sui_vram_init:0:*/
static void ui_vram_init(void)
	{
	ui_vk_init(&ui_vram_vk);
	ui_vram_win = win_create(
		UI_VRAM_WIDTH, UI_VRAM_HEIGHT,
		1, 1,
		ui_vram_title ? ui_vram_title : "Memu VRAM Inspector",
		ui_vram_display, /* display */
		NULL, /* geometry */
		ui_cols, N_COLS_POPUP,
		ui_vram_keypress,
		ui_vram_keyrelease
		);
	win_refresh(ui_vram_win);
	}
/*...e*/
/*...sui_vram_term:0:*/
static void ui_vram_term(void)
	{
	if ( ui_vram_win != NULL )
		{
		win_delete(ui_vram_win);
		ui_vram_win = NULL;
		}
	}
/*...e*/
/*...e*/
/*...sDisassembly:0:*/
/* Disassemble the contents of memory. */

#define	UI_DIS_COLS_CODE 75
#define	UI_DIS_COLS      (4+1+UI_DIS_COLS_CODE)
#define	UI_DIS_ROWS_CODE 31
#define	UI_DIS_ROWS      (1+UI_DIS_ROWS_CODE)

#define	UI_DIS_WIDTH  (UI_DIS_COLS*GLYPH_WIDTH)
#define	UI_DIS_HEIGHT (UI_DIS_ROWS*GLYPH_HEIGHT)

static WIN *ui_dis_win = NULL;
#define	UI_DIS_FOCUS_IOBYTE  0
#define	UI_DIS_FOCUS_SUBPAGE 1
#define	UI_DIS_FOCUS_START   2
#define	UI_DIS_FOCUS_COUNT   3
static int ui_dis_focus = UI_DIS_FOCUS_IOBYTE;
static int ui_dis_digit = 0;

#define	N_DIS_HIST 10000

typedef struct _DISCTX DISCTX;

struct _DISCTX
	{
	DISCTX *link;
	byte iobyte;
	byte subpage;
	word starts[N_DIS_HIST+UI_DIS_ROWS_CODE+1];
	int row_start;
	int row_highlighted;
	int depth;
	};

static DISCTX *ui_dis_ctx;

#define	UI_DIS_MAX_DEPTH 20

static VIRTKBD ui_dis_vk;

/*...sui_dis_keypress:0:*/
static void ui_dis_keypress(WIN *win, int wk)
	{
	ui_vk_keypress(&ui_dis_vk, wk);
	}
/*...e*/
/*...sui_dis_keyrelease:0:*/
static void ui_dis_keyrelease(WIN *win, int wk)
	{
	}
/*...e*/
/*...sui_dis_refresh:0:*/
/*...sui_dis_key:0:*/
/*...sui_dis_ref_code:0:*/
static BOOLEAN ui_dis_ref_code(word *r)
	{
	byte iobyte_saved = mem_get_iobyte();
	BOOLEAN ref;
	mem_set_iobyte(ui_dis_ctx->iobyte);
	ref = dis_ref_code(ui_dis_ctx->starts[ui_dis_ctx->row_highlighted], r);
	mem_set_iobyte(iobyte_saved);
	return ref;
	}
/*...e*/

static void ui_dis_key(int wk)
	{
	BOOLEAN next;
	switch ( wk )
		{
		case 'i':
			ui_dis_focus = UI_DIS_FOCUS_IOBYTE;
			ui_dis_digit = 0;
			return;
		case 'u':
			ui_dis_focus = UI_DIS_FOCUS_SUBPAGE;
			ui_dis_digit = 0;
			return;
		case 's':
			ui_dis_focus = UI_DIS_FOCUS_START;
			ui_dis_digit = 0;
			return;
		case 'j':
			{
			word r;
			if ( ui_dis_ref_code(&r) )
				{
				ui_dis_ctx->starts[0]       = r;
				ui_dis_ctx->row_start       = 0;
				ui_dis_ctx->row_highlighted = 0;
				}
			}
			return;
		case 'k':
		case WK_Right:
		case WK_KP_Right:
			{
			word r;
			if ( ui_dis_ctx->depth < UI_DIS_MAX_DEPTH &&
			     ui_dis_ref_code(&r) )
				{
				DISCTX *c = (DISCTX *) emalloc(sizeof(DISCTX));
				c->link = ui_dis_ctx;
				c->iobyte = ui_dis_ctx->iobyte;
				c->subpage = ui_dis_ctx->subpage;
				c->starts[0] = r;
				c->row_start = 0;
				c->row_highlighted = 0;
				c->depth = ui_dis_ctx->depth+1;
				ui_dis_ctx = c;
				}
			}
			return;
		case 'l':
			dis_show_ill ^= TRUE;
			return;
		case 'm':
			dis_mtx_exts ^= TRUE;
			return;
		case WK_Escape:
		case WK_Left:
		case WK_KP_Left:
			if ( ui_dis_ctx->link != NULL )
				{
				DISCTX *c = ui_dis_ctx;
				ui_dis_ctx = ui_dis_ctx->link;
				free(c);
				}
			return;
		case WK_Tab:
			ui_dis_focus = (ui_dis_focus+1) % UI_DIS_FOCUS_COUNT;
			ui_dis_digit = 0;
			return;
		case WK_Up:
		case WK_KP_Up:
			if ( ui_dis_ctx->row_highlighted > ui_dis_ctx->row_start )
				--ui_dis_ctx->row_highlighted; 
			else if ( ui_dis_ctx->row_start > 0 )
				{
				--ui_dis_ctx->row_start;
				--ui_dis_ctx->row_highlighted;
				}
			return;
		case WK_Down:
		case WK_KP_Down:
			if ( ui_dis_ctx->row_highlighted - ui_dis_ctx->row_start < UI_DIS_ROWS_CODE-1 )
				++ui_dis_ctx->row_highlighted;
			else
				{
				if ( ui_dis_ctx->row_start == N_DIS_HIST )
					{
					int i;
					for ( i = 0; i <= N_DIS_HIST; i++ )
						ui_dis_ctx->starts[i] = ui_dis_ctx->starts[i+1];
					}
				else
					{
					++ui_dis_ctx->row_start;
					++ui_dis_ctx->row_highlighted;
					}
				}
			return;
		case WK_Page_Up:
		case WK_KP_Page_Up:
			if ( ui_dis_ctx->row_start >= UI_DIS_ROWS_CODE )
				{
				ui_dis_ctx->row_start       -= UI_DIS_ROWS_CODE;
				ui_dis_ctx->row_highlighted -= UI_DIS_ROWS_CODE;
				}
			else
				{
				ui_dis_ctx->row_start       = 0;
				ui_dis_ctx->row_highlighted = 0;
				}
			return;
		case WK_Page_Down:
		case WK_KP_Page_Down:
			if ( ui_dis_ctx->row_start + UI_DIS_ROWS_CODE > N_DIS_HIST )
				{
				int i;
				for ( i = 0; i <= N_DIS_HIST; i++ )
					ui_dis_ctx->starts[i] = ui_dis_ctx->starts[i+UI_DIS_ROWS_CODE];
				}
			else
				{
				ui_dis_ctx->row_start       += UI_DIS_ROWS_CODE;
				ui_dis_ctx->row_highlighted += UI_DIS_ROWS_CODE;
				}
			return;
		}
	switch ( ui_dis_focus )
		{
		case UI_DIS_FOCUS_IOBYTE:
			if ( update_byte(&ui_dis_ctx->iobyte, &ui_dis_digit, wk, &next) )
				{
				ui_dis_ctx->row_start       = 0;
				ui_dis_ctx->row_highlighted = 0;
				}
			break;
		case UI_DIS_FOCUS_SUBPAGE:
			if ( update_byte(&ui_dis_ctx->subpage, &ui_dis_digit, wk, &next) )
				{
				ui_dis_ctx->row_start       = 0;
				ui_dis_ctx->row_highlighted = 0;
				}
			break;
		case UI_DIS_FOCUS_START:
			if ( update_word(&ui_dis_ctx->starts[0], &ui_dis_digit, wk, &next) )
				{
				ui_dis_ctx->row_start       = 0;
				ui_dis_ctx->row_highlighted = 0;
				}
			break;
		}
	}
/*...e*/

static void ui_dis_refresh(void)
	{
	if ( ui_dis_win != NULL )
		{
		char s[100+1];
		byte iobyte_saved;
		byte subpage_saved;
		int y;
		int wk;
		if ( (wk = ui_vk_key(&ui_dis_vk)) != -1 )
			ui_dis_key(wk);
		iobyte_saved = mem_get_iobyte();
		subpage_saved = mem_get_rom_subpage();
		mem_set_iobyte(ui_dis_ctx->iobyte);
		mem_set_rom_subpage(ui_dis_ctx->subpage);
		sprintf(s, "iobyte %02x subpage %02x start %04x depth %-2d",
			ui_dis_ctx->iobyte,
			ui_dis_ctx->subpage,
			(unsigned) ui_dis_ctx->starts[0],
			ui_dis_ctx->depth);
		ui_print_string(ui_dis_win,0,0, s, COL_YELLOW);
		for ( y = 0; y < UI_DIS_ROWS_CODE; y++ )
			{
			char s2[500+1];
			word a1 = ui_dis_ctx->starts[ui_dis_ctx->row_start+y];
			word a2 = a1;
			word r;
			dis_instruction(&a2, s2);
			ui_dis_ctx->starts[ui_dis_ctx->row_start+y+1] = a2;
			s2[UI_DIS_COLS_CODE] = '\0';
			sprintf(s, "%04x",
				(unsigned)a1);
			ui_print_string(ui_dis_win,0,y+1, s, COL_YELLOW);
			sprintf(s, " %-*s",
				(int)UI_DIS_COLS_CODE,
				s2);
			ui_print_string(ui_dis_win,4,y+1, s, COL_CYAN);
			if ( dis_ref_code(a1, &r) )
				{
				sprintf(s, " -> %04x ", (unsigned)r);
				ui_print_string(ui_dis_win,UI_DIS_COLS-9,y+1, s, COL_GREEN);
				}
			}
		mem_set_rom_subpage(subpage_saved);
		mem_set_iobyte(iobyte_saved);
		ui_highlight(ui_dis_win,0,ui_dis_ctx->row_highlighted-ui_dis_ctx->row_start+1,4+1+UI_DIS_COLS_CODE);
		switch ( ui_dis_focus )
			{
			case UI_DIS_FOCUS_IOBYTE:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_dis_win,7+ui_dis_digit,0,1);
				break;
			case UI_DIS_FOCUS_SUBPAGE:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_dis_win,18+ui_dis_digit,0,1);
				break;
			case UI_DIS_FOCUS_START:
				if ( ui_refreshes & UI_FLASH )
					ui_invert(ui_dis_win,27+ui_dis_digit,0,1);
				break;
			}
		win_refresh(ui_dis_win);
		}
	}
/*...e*/

/*...sui_dis_init:0:*/
static void ui_dis_init(void)
	{
	ui_dis_ctx = (DISCTX *) emalloc(sizeof(DISCTX));
	ui_dis_ctx->link            = NULL;
	ui_dis_ctx->iobyte          = 0x00;
	ui_dis_ctx->subpage         = 0x00;
	ui_dis_ctx->starts[0]       = 0x0000;
	ui_dis_ctx->row_start       = 0;
	ui_dis_ctx->row_highlighted = 0;
	ui_dis_ctx->depth           = 1;

	ui_vk_init(&ui_dis_vk);

	ui_dis_win = win_create(
		UI_DIS_WIDTH, UI_DIS_HEIGHT,
		1, 1,
		ui_dis_title ? ui_dis_title : "Memu Disassembly Inspector",
		ui_dis_display, /* display */
		NULL, /* geometry */
		ui_cols, N_COLS_POPUP,
		ui_dis_keypress,
		ui_dis_keyrelease
		);
	win_refresh(ui_dis_win);
	}
/*...e*/
/*...sui_dis_term:0:*/
static void ui_dis_term(void)
	{
	if ( ui_dis_win != NULL )
		{
		win_delete(ui_dis_win);
		ui_dis_win = NULL;
		}
	}
/*...e*/
/*...e*/

/*...sui_refresh:0:*/
void ui_refresh(void)
	{
	++ui_refreshes;
	ui_mem_refresh();
	ui_vram_refresh();
	ui_dis_refresh();
	}
/*...e*/

/*...sui_init:0:*/
void ui_init(int opts)
	{
	if ( opts & UI_MEM )
		ui_mem_init();
	if ( opts & UI_VRAM )
		ui_vram_init();
	if ( opts & UI_DIS )
		ui_dis_init();
	}
/*...e*/
/*...sui_term:0:*/
void ui_term(void)
	{
	ui_mem_term();
	ui_vram_term();
	ui_dis_term();
	}
/*...e*/
void ui_mem_set_title (const char *title)
	{
    ui_mem_title = title;
	}

const char * ui_mem_get_title (void)
	{
	return ui_mem_title;
	}

void ui_mem_set_display (const char *display)
	{
    ui_mem_display = display;
	}

const char * ui_mem_get_display (void)
	{
	return ui_mem_display;
	}

void ui_vram_set_title (const char *title)
	{
    ui_vram_title = title;
	}

const char * ui_vram_get_title (void)
	{
	return ui_vram_title;
	}

void ui_vram_set_display (const char *display)
	{
    ui_vram_display = display;
	}

const char * ui_vram_get_display (void)
	{
	return ui_vram_display;
	}

void ui_dis_set_title (const char *title)
	{
    ui_dis_title = title;
	}

const char * ui_dis_get_title (void)
	{
	return ui_dis_title;
	}

void ui_dis_set_display (const char *display)
	{
    ui_dis_display = display;
	}

const char * ui_dis_get_display (void)
	{
	return ui_dis_display;
	}
