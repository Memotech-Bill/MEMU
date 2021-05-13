/*

  win_cir.c - Frame buffer Window for Circle Bare Metal environment.

*/

/*...sincludes:0:*/
/*
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <termios.h>
*/
#include <circle/alloc.h>

#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"
#include "keyeventbuffer.h"
#include "kfuncs.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...e*/

typedef struct
	{
	int width, height;
	int width_scale, height_scale;
	int n_cols;
	byte *data;
	void (*keypress)(int);
	void (*keyrelease)(int);
	/* Private window data below - Above must match definition of WIN in win.h */
	int iWin;
	int left, top;
	int byte_per_pixel;
	unsigned int *cols;
	} WIN_PRIV;

#define   MAX_WINS 10
static int n_wins = 0; 
static int  iActiveWin  =  -1;
static WIN_PRIV *wins[MAX_WINS];

static struct FBInfo finfo;
static int screensize = 0;
static byte *fbp = NULL;
static byte *pbline = NULL;

/*
static BOOLEAN tty_init =  FALSE;
static int ttyfd  =  0;
static struct termios tty_attr_old;
static int old_keyboard_mode;
*/

static int mod_keys  =  0;

void win_fb_init (void)
	{
	byte *fbd;
	int  i;

    GetFBInfo (&finfo);
	pbline   =  (byte *) malloc (finfo.xstride);
	if ( pbline == NULL )   fatal ("Unable to allocate video line buffer.");

	// Figure out the size of the screen in bytes
	screensize = finfo.xres * finfo.yres;

	// Map the device to memory
	fbp = finfo.pfb;
	if ( fbp == NULL )
		{
		fatal ("Failed to map frame buffer memory.");
		}
	diag_message (DIAG_WIN_HW, "The framebuffer device was mapped to memory successfully.");

	// Clear the screen
	fbd   =  fbp;
	for ( i = 0; i < screensize; ++i )
		{
		*fbd  =  0;
		++fbd;
		}
	}

void win_max_size (const char *display, int *pWth, int *pHgt)
    {
    GetFBInfo (&finfo);
    *pWth = finfo.xres;
    *pHgt = finfo.yres;
    }

/*...swin_refresh:0:*/
void win_refresh(WIN *win_pub)
	{
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
	int  x, y;
	int  i, j;
	byte *pix;
	byte *fbd;
	int  line_length;

    // diag_message (DIAG_WIN_HW, "Refresh window %d (%X), Active window = %d", win->iWin, win, iActiveWin);
	if ( win->iWin != iActiveWin )   return;
	line_length =  win->width * win->width_scale;
	fbd   =  fbp + win->top * finfo.xstride + win->left;
	pix   =  win->data;
    // diag_message (DIAG_WIN_HW, "line_length = %d, fbp = %X, pix = %X, pbline = %X",
    //     line_length, fbp, pix, pbline);
	for ( y = 0; y < win->height; ++y )
		{
        byte *pdw  =  pbline;
        if ( win->width_scale == 1 )
            {
            for ( x = 0; x < win->width; ++x )
                {
                *pdw  =  *pix;
                ++pdw;
                ++pix;
                }
            }
        else if ( win->width_scale == 2 )
            {
            for ( x = 0; x < win->width; ++x )
                {
                *pdw  =  *pix;
                ++pdw;
                *pdw  =  *pix;
                ++pdw;
                ++pix;
                }
            }
        else
            {
            for ( x = 0; x < win->width; ++x )
                {
                for ( i = 0; i < win->width_scale; ++i )
                    {
                    *pdw  =  *pix;
                    ++pdw;
                    }
                ++pix;
                }
            }
		for ( j = 0; j < win->height_scale; ++j )
			{
			memcpy (fbd, pbline, line_length);
			fbd   += finfo.xstride;
			}
		}
//   diag_message (DIAG_WIN_HW, "Refresh complete");
	}

static void win_swap (WIN_PRIV *win)
	{
	byte *fbd   =  fbp;
	int  i;
	diag_message (DIAG_WIN_HW, "Swap to window %d", win->iWin);
	iActiveWin  =  win->iWin;
	for ( i = 0; i < screensize; ++i )
		{
		*fbd  =  0;
		++fbd;
		}
    for ( i = 0; i < win->n_cols; ++i )
        {
        SetFBPalette (i, win->cols[i]);
        }
//	diag_message (DIAG_WIN_HW, "Update palette");
    UpdateFBPalette ();
	win_refresh ((WIN *) win);
	}

void win_show (WIN *win)
    {
    if ( (WIN_PRIV *) win != wins[iActiveWin] ) win_swap ((WIN_PRIV *)win);
    }

void win_next (void)
	{
	if ( ++iActiveWin >= n_wins )	iActiveWin	=  0;
	win_swap (wins[iActiveWin]);
	}

void win_prev (void)
	{
	if ( --iActiveWin < 0 )	iActiveWin	=  n_wins - 1;
	win_swap (wins[iActiveWin]);
	}

BOOLEAN win_active (WIN *win_pub)
    {
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
    return ( win->iWin == iActiveWin );
    }

WIN * win_current (void)
    {
    return (WIN *) wins[iActiveWin];
    }

/*...e*/

static unsigned int win_fb_colour (COL col)
    {
    unsigned int clr = ( ( col.r & 0xFF ) /* << 16 */)
        |  ( ( col.g & 0xFF ) << 8 )
        |  ( ( col.b & 0xFF ) << 16 );
    return   clr;
    }

/*...swin_create:0:*/
WIN *win_create(
	int width, int height,
	int width_scale, int height_scale,
	const char *title,
	const char *display,
	const char *geometry,
	COL *cols, int n_cols,
	void (*keypress)(int k),
	void (*keyrelease)(int k)
	)
	{
	WIN_PRIV *win;
	int  i;

	if ( fbp == NULL )  win_fb_init ();

	if ( n_wins == MAX_WINS )
		fatal("too many windows");

	win = (WIN_PRIV *) emalloc(sizeof(WIN_PRIV));
	win->iWin      = n_wins;
	win->width           = width;
	win->height          = height;
	win->data            = emalloc (width * height);
	memset (win->data, 0, width * height);
	win->width_scale     = finfo.xres / width;
	win->height_scale    = finfo.yres / height;
	if ( win->width_scale > win->height_scale )  win->width_scale  =  win->height_scale;
	win->left      = ( finfo.xres - win->width * win->width_scale ) / 2;
	win->top       = ( finfo.yres - win->height * win->height_scale ) / 2;
	win->keypress        = keypress;
	win->keyrelease      = keyrelease;
	win->n_cols          = n_cols;
	win->cols      = (unsigned int *) emalloc (n_cols * sizeof (unsigned int));
	for ( i = 0; i < n_cols; ++i ) win->cols[i] = win_fb_colour (cols[i]);
	diag_message (DIAG_INIT, "Created window %s (%d), size = %d x %d, scale = %d x %d, position = (%d, %d)",
		title, win->iWin, win->width, win->height, win->width_scale, win->height_scale, win->left, win->top);

	wins[n_wins++] =  win;
	win_swap (win);
	return (WIN *) win;
	}
/*...e*/

/*...swin_delete:0:*/
void win_term (void)
	{
	if ( pbline != NULL )
		{
		free (pbline);
		pbline   =  NULL;
		}
	}

void win_delete(WIN *win_pub)
	{
	if ( win_pub == NULL ) return;
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
	int i;
	for ( i = 0; wins[i] != win; i++ )
		;
	diag_message (DIAG_WIN_HW, "Delete window %d", i);
	wins[i] = wins[--n_wins];
	wins[i]->iWin  =  i;
	free (win->data);
	free (win->cols);
	free (win);
	if ( n_wins )
		{
		diag_message (DIAG_WIN_HW, "Active window was %d", iActiveWin);
		if ( iActiveWin == i )  win_swap (wins[n_wins-1]);
		else if ( iActiveWin >= n_wins ) iActiveWin  =  i;
		}
	else
		{
		win_term ();
		}
	}
/*...e*/

/*...swin_map_key:0:*/
typedef struct
	{
	int wk;
	const char *ps;
	} MAPKS;

static MAPKS win_mapks[] =
    {
    { -1,               "NULL0" },                  // 0x00 KEY_NULL    Invalid key
    { -1,               "NULL1" },                  // 0x01 KEY_NULL1   Invalid key
    { -1,               "NULL3" },                  // 0x02 KEY_NULL3   Invalid key
    { -1,               "NULL3" },                  // 0x03 KEY_NULL3   Invalid key
    { 'a',              "A" },                      // 0x04 KEY_A       Keyboard a and A
    { 'b',              "B" },                      // 0x05 KEY_B       Keyboard b and B
    { 'c',              "C" },                      // 0x06 KEY_C       Keyboard c and C
    { 'd',              "D" },                      // 0x07 KEY_D       Keyboard d and D
    { 'e',              "E" },                      // 0x08 KEY_E       Keyboard e and E
    { 'f',              "F" },                      // 0x09 KEY_F       Keyboard f and F
    { 'g',              "G" },                      // 0x0a KEY_G       Keyboard g and G
    { 'h',              "H" },                      // 0x0b KEY_H       Keyboard h and H
    { 'i',              "I" },                      // 0x0c KEY_I       Keyboard i and I
    { 'j',              "J" },                      // 0x0d KEY_J       Keyboard j and J
    { 'k',              "K" },                      // 0x0e KEY_K       Keyboard k and K
    { 'l',              "L" },                      // 0x0f KEY_L       Keyboard l and L
    { 'm',              "M" },                      // 0x10 KEY_M       Keyboard m and M
    { 'n',              "N" },                      // 0x11 KEY_N       Keyboard n and N
    { 'o',              "O" },                      // 0x12 KEY_O       Keyboard o and O
    { 'p',              "P" },                      // 0x13 KEY_P       Keyboard p and P
    { 'q',              "Q" },                      // 0x14 KEY_Q       Keyboard q and Q
    { 'r',              "R" },                      // 0x15 KEY_R       Keyboard r and R
    { 's',              "S" },                      // 0x16 KEY_S       Keyboard s and S
    { 't',              "T" },                      // 0x17 KEY_T       Keyboard t and T
    { 'u',              "U" },                      // 0x18 KEY_U       Keyboard u and U
    { 'v',              "V" },                      // 0x19 KEY_V       Keyboard v and V
    { 'w',              "W" },                      // 0x1a KEY_W       Keyboard w and W
    { 'x',              "X" },                      // 0x1b KEY_X       Keyboard x and X
    { 'y',              "Y" },                      // 0x1c KEY_Y       Keyboard y and Y
    { 'z',              "Z" },                      // 0x1d KEY_Z       Keyboard z and Z
    { '1',              "1" },                      // 0x1e KEY_1       Keyboard 1 and !
    { '2',              "2" },                      // 0x1f KEY_2       Keyboard 2 and "
    { '3',              "3" },                      // 0x20 KEY_3       Keyboard 3 and £
    { '4',              "4" },                      // 0x21 KEY_4       Keyboard 4 and $
    { '5',              "5" },                      // 0x22 KEY_5       Keyboard 5 and %
    { '6',              "6" },                      // 0x23 KEY_6       Keyboard 6 and ^
    { '7',              "7" },                      // 0x24 KEY_7       Keyboard 7 and &
    { '8',              "8" },                      // 0x25 KEY_8       Keyboard 8 and *
    { '9',              "9" },                      // 0x26 KEY_9       Keyboard 9 and (
    { '0',              "0" },                      // 0x27 KEY_ZERO    Keyboard 0 and )
    { WK_Return,        "Enter" },                  // 0x28 KEY_ENTER   Keyboard Return (ENTER)
    { WK_Escape,        "Escape" },                 // 0x29 KEY_ESC     Keyboard ESCAPE
    { WK_BackSpace,     "Back Space" },             // 0x2a KEY_BACKSP  Keyboard DELETE (Backspace)
    { WK_Tab,           "Tab" },                    // 0x2b KEY_TAB     Keyboard Tab
    { ' ',              "Space" },                  // 0x2c KEY_SPACE   Keyboard Spacebar
    { '-',              "Minus" },                  // 0x2d KEY_MINUS   Keyboard - and _
    { '=',              "Equal" },                  // 0x2e KEY_EQUAL   Keyboard = and +
    { '[',              "[" },                      // 0x2f KEY_LSQBRK  Keyboard [ and {
    { ']',              "]" },                      // 0x30 KEY_RSQBRK  Keyboard ] and }
    { '\\',             "\\" },                     // 0x31 KEY_BSLASH  Keyboard \ and |
    { '#',              "#" },                      // 0x32 KEY_HASH    Keyboard Non-US # and ~
    { ';',              ";" },                      // 0x33 KEY_SCOLON  Keyboard ; and :
    { '\'',             "\'" },                     // 0x34 KEY_QUOTE   Keyboard ' and @
    { '`',              "`" },                      // 0x35 KEY_BQUOTE  Keyboard ` and ~
    { ',',              "," },                      // 0x36 KEY_COMMA   Keyboard , and <
    { '.',              "." },                      // 0x37 KEY_STOP    Keyboard . and >
    { '/',              "/" },                      // 0x38 KEY_SLASH   Keyboard / and ?
    { WK_Caps_Lock,     "Caps Lock" },              // 0x39 KEY_CAPLK   Keyboard Caps Lock
    { WK_F1,            "F1" },                     // 0x3a KEY_F1      Keyboard F1
    { WK_F2,            "F2" },                     // 0x3b KEY_F2      Keyboard F2
    { WK_F3,            "F3" },                     // 0x3c KEY_F3      Keyboard F3
    { WK_F4,            "F4" },                     // 0x3d KEY_F4      Keyboard F4
    { WK_F5,            "F5" },                     // 0x3e KEY_F5      Keyboard F5
    { WK_F6,            "F6" },                     // 0x3f KEY_F6      Keyboard F6
    { WK_F7,            "F7" },                     // 0x40 KEY_F7      Keyboard F7
    { WK_F8,            "F8" },                     // 0x41 KEY_F8      Keyboard F8
    { WK_F9,            "F9" },                     // 0x42 KEY_F9      Keyboard F9 - MTX Line Feed
    { WK_F10,           "F10" },                    // 0x43 KEY_F10     Keyboard F10
    { WK_F11,           "F11" },                    // 0x44 KEY_F11     Keyboard F11
    { WK_F12,           "F12" },                    // 0x45 KEY_F12     Keyboard F12
    { WK_Sys_Req,       "Sys Req" },                // 0x46 KEY_PRNSCR  Keyboard Print Screen
    { WK_Scroll_Lock,   "Scroll Lock" },            // 0x47 KEY_SCRLK   Keyboard Scroll Lock
    { WK_Pause,         "Pause" },                  // 0x48 KEY_PAUSE   Keyboard Pause
    { WK_Insert,        "Insert" },                 // 0x49 KEY_INS     Keyboard Insert
    { WK_Home,          "Home" },                   // 0x4a KEY_HOME    Keyboard Home
    { WK_Page_Up,       "Page Up" },                // 0x4b KEY_PGUP    Keyboard Page Up
    { WK_Delete,        "Delete" },                 // 0x4c KEY_DEL     Keyboard Delete Forward
    { WK_End,           "End" },                    // 0x4d KEY_END     Keyboard End
    { WK_Page_Down,     "Page Down" },              // 0x4e KEY_PGDN    Keyboard Page Down
    { WK_Right,         "Right Arrow" },            // 0x4f KEY_RIGHT   Keyboard Right Arrow
    { WK_Left,          "Left Arrow" },             // 0x50 KEY_LEFT    Keyboard Left Arrow
    { WK_Down,          "Down Arrow" },             // 0x51 KEY_DOWN    Keyboard Down Arrow
    { WK_Up,            "Up Arrow" },               // 0x52 KEY_UP      Keyboard Up Arrow
    { WK_Num_Lock,      "Num Lock" },               // 0x53 KEY_NUMLK   Keyboard Num Lock and Clear
    { WK_KP_Divide,     "Keypad /" },               // 0x54 KEY_KPDIV   Keypad /
    { WK_KP_Multiply,   "Keypad *" },               // 0x55 KEY_KPMULT  Keypad *
    { WK_KP_Subtract,   "Keypad -" },               // 0x56 KEY_KPMINUS Keypad -
    { WK_KP_Add,        "Keypad +" },               // 0x57 KEY_KPPLUS  Keypad +
    { WK_KP_Enter,      "Keypad Enter" },           // 0x58 KEY_KPENTER Keypad ENTER
    { WK_KP_End,        "Keypad 1 (End)" },         // 0x59 KEY_KP1     Keypad 1 and End
    { WK_KP_Down,       "Keypad 2 (Down)" },        // 0x5a KEY_KP2     Keypad 2 and Down Arrow
    { WK_KP_Page_Down,  "Keypad 3 (Page Down)" },   // 0x5b KEY_KP3     Keypad 3 and PageDn
    { WK_KP_Left,       "Keypad 4 (Left)" },        // 0x5c KEY_KP4     Keypad 4 and Left Arrow
    { WK_KP_Middle,     "Keypad 5 (Middle)" },      // 0x5d KEY_KP5     Keypad 5
    { WK_KP_Right,      "Keypad 6 (Right)" },       // 0x5e KEY_KP6     Keypad 6 and Right Arrow
    { WK_KP_Home,       "Keypad 7 (Home)" },        // 0x5f KEY_KP7     Keypad 7 and Home
    { WK_KP_Up,         "Keypad 8 (Up)" },          // 0x60 KEY_KP8     Keypad 8 and Up Arrow
    { WK_KP_Page_Up,    "Keypad 9 (Page Up)" },     // 0x61 KEY_KP9     Keypad 9 and Page Up
    { -1,               "KP0" },                    // 0x62 KEY_KP0     Keypad 0 and Insert
    { -1,               "KPSTOP" },                 // 0x63 KEY_KPSTOP  Keypad . and Delete
    { WK_Control_L,     "Left Control" },           // 0x64 KEY_LCTRL   Left Control
    { WK_Shift_L,       "Left Shift" },             // 0x65 KEY_LSHIFT  Left Shift
    { WK_PC_Alt_L,      "Left Alt" },               // 0x66 KEY_LALT    Left Alt
    { WK_PC_Windows_L,  "Windows Left" },           // 0x67 KEY_LMETA   Left Meta
    { WK_Control_R,     "Right Control" },          // 0x68 KEY_RCTRL   Right Control
    { WK_Shift_R,       "Right Shift" },            // 0x69 KEY_RSHIFT  Right Shift
    { WK_PC_Alt_R,      "Right Alt" },              // 0x6a KEY_RALT    Right Alt
    { WK_PC_Windows_R,  "Windows Right" }          // 0x6b KEY_RMETA   Right Meta
    };

static int win_map_key (unsigned char ks)
	{
#ifndef	NOKBD
    if ( ks < KEY_COUNT ) return win_mapks[ks].wk;
#endif
	return   -1;
	}

/*...e*/

/*...swin_shifted_wk:0:*/
/* Keys of the host keyboard have an unshifted label and a shifted label
   written on them, eg: unshifted "1", shifted "!". Alphabetic keys typically
   omit the unshifted lowercase letter, but notionally it is there.
   This module returns WK_ values with names which reflect unshifted label.
   Sometimes the module user will want to know the equivelent shifted label.

   The problem with this code is that it assumes the UK keyboard layout. */

int win_shifted_wk(int wk)
	{
	if ( wk >= 'a' && wk <= 'z' )
		return wk-'a'+'A';
	switch ( wk )
		{
		case '1':   return '!';
		case '2':   return '"';
		case '3':   return '#'; /* pound */
		case '4':   return '$';
		case '5':   return '%';
		case '6':   return '^';
		case '7':   return '&';
		case '8':   return '*';
		case '9':   return '(';
		case '0':   return ')';
		case '-':   return '_';
		case '=':   return '+';
		case '[':   return '{';
		case ']':   return '}';
		case ';':   return ':';
		case '\'':   return '@';
		case '#':   return '~';
		case '\\':   return '|';
		case ',':   return '<';
		case '.':   return '>';
		case '/':   return '?';
		default:   return ( wk >= 0 && wk < 0x100 ) ? wk : -1;
		}
	}
/*...e*/

static char * ListModifiers (void)
	{
	static char sMods[19];
	sMods[0] =  '\0';
	if ( mod_keys == 0 ) strcpy (sMods, " None");
	if ( mod_keys & MKY_LSHIFT )  strcat (sMods, " LS");
	if ( mod_keys & MKY_RSHIFT )  strcat (sMods, " RS");
	if ( mod_keys & MKY_LCTRL )   strcat (sMods, " LC");
	if ( mod_keys & MKY_RCTRL )   strcat (sMods, " RC");
	if ( mod_keys & MKY_LALT )    strcat (sMods, " LA");
	if ( mod_keys & MKY_RALT )    strcat (sMods, " RA");
	if ( mod_keys & MKY_CAPSLK )  strcat (sMods, " CL");
	return   sMods;
	}

int win_mod_keys (void)
    {
    return mod_keys;
    }

/*...swin_handle_events:0:*/

#ifdef	 ALT_HANDLE_EVENTS
extern BOOLEAN ALT_HANDLE_EVENTS (WIN *);
#endif

void win_handle_events()
	{
	unsigned char  key = 0;
    int iKey = 0;

#ifdef	 ALT_HANDLE_EVENTS
    // diag_message (DIAG_INIT, "ALT_HANDLE_EVENTS");
	if ( ALT_HANDLE_EVENTS ((WIN *) wins[iActiveWin]) )	 return;
#endif

    // Keep Circle Scheduler happy.
//    diag_message (DIAG_INIT, "CircleYield");
    CircleYield ();
//    diag_message (DIAG_INIT, "Exit CircleYield");
    
#ifndef NOKBD
    // diag_message (DIAG_INIT, "GetKeyEvent");
	while ( GetKeyEvent (&key) > 0 )
		{
		if ( ( key & 0x80 ) == 0 )
			{
			// Key press.
			diag_message (DIAG_KBD_HW, "Key down event: 0x%02x, Modifiers:%s", key, ListModifiers ());

			// Modifier keys.
			if ( key == KEY_LSHIFT )        mod_keys |= MKY_LSHIFT;
			else if ( key == KEY_RSHIFT )   mod_keys |= MKY_RSHIFT;
			else if ( key == KEY_LCTRL )    mod_keys |= MKY_LCTRL;
			else if ( key == KEY_RCTRL )    mod_keys |= MKY_RCTRL;
			else if ( key == KEY_LALT )     mod_keys |= MKY_LALT;
			else if ( key == KEY_RALT )     mod_keys |= MKY_RALT;
			else if ( key == KEY_CAPLK )    mod_keys |= MKY_CAPSLK;

			// Select window.
			if ( ( mod_keys & MKY_LCTRL ) && ( key >= KEY_F1 ) && ( key <= KEY_F8 ) )
				{
				if ( ( key - KEY_F1 ) < n_wins )  win_swap (wins[key - KEY_F1]);
				return;
				}

			// Exit on ctrl+break.

			if ( ( key == KEY_PAUSE ) && ( mod_keys & MKY_LCTRL ) )
				{
				diag_message (DIAG_KBD_HW, "Ctrl+Break keys pressed.");
				terminate ("Ctrl+Break keys pressed.");
				}

			// Process key press.
			if ( (iKey = win_map_key (key)) > 0 ) wins[iActiveWin]->keypress (iKey);
			}
		else
			{
			// Key release.
			key &= 0x7f;
			diag_message (DIAG_KBD_HW, "Key up event: 0x%x, Modifiers:%02s", key, ListModifiers ());

			// Modifier keys.
			if ( key == KEY_LSHIFT )        mod_keys &= ~MKY_LSHIFT;
			else if ( key == KEY_RSHIFT )   mod_keys &= ~MKY_RSHIFT;
			else if ( key == KEY_LCTRL )    mod_keys &= ~MKY_LCTRL;
			else if ( key == KEY_RCTRL )    mod_keys &= ~MKY_RCTRL;
			else if ( key == KEY_LALT )     mod_keys &= ~MKY_LALT;
			else if ( key == KEY_RALT )     mod_keys &= ~MKY_RALT;
			else if ( key == KEY_CAPLK )    mod_keys &= ~MKY_CAPSLK;

			// Process key release.
			if ( (iKey = win_map_key (key)) > 0 ) wins[iActiveWin]->keyrelease (iKey);
			}
		}
#endif
    // diag_message (DIAG_INIT, "Exit win_handle_events");
	}   
/*...e*/

void win_kbd_leds (BOOLEAN bCaps, BOOLEAN bNum, BOOLEAN bScroll)
    {
    static byte uLast = 0xFF;
    byte uLeds = 0;
    if ( bCaps )    uLeds |= KEYB_LED_CAPS_LOCK;
    if ( bNum )     uLeds |= KEYB_LED_NUM_LOCK;
    if ( bScroll )  uLeds |= KEYB_LED_SCROLL_LOCK;
    if ( uLeds != uLast ) SetKbdLeds (uLeds);
    uLast = uLeds;
    }

// Dummy functions for now
void set_gpu_mode (int iMode)
    {}

int get_gpu_mode (void)
    {
    return 0;
    }
