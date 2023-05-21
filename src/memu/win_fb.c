/*

  win.c - Frame buffer Window

*/

/*...sincludes:0:*/
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

#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"
#include "mon.h"
#include "vid.h"

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
	__u32 *cols;
	} WIN_PRIV;

#define   MAX_WINS 10
static int n_wins = 0; 
static int  iActiveWin  =  0;
static WIN_PRIV *wins[MAX_WINS];

static int fbfd = 0;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static int screensize = 0;
static byte *fbp = NULL;
static byte *pbline = NULL;

static BOOLEAN tty_init =  FALSE;
static int ttyfd  =  0;
static struct termios tty_attr_old;
static int old_keyboard_mode;

#define MKY_LSHIFT   0x01
#define MKY_RSHIFT   0x02
#define MKY_LCTRL    0x04
#define MKY_RCTRL    0x08
#define MKY_LALT     0x10
#define MKY_RALT     0x20
#define MKY_CAPSLK   0x40

static int mod_keys  =  0;

void win_fb_init (void)
	{
	struct termios tty_attr;
	int flags;
	byte *fbd   =  fbp;
	int  i;

	/* save old keyboard mode */
	if (ioctl (ttyfd, KDGKBMODE, &old_keyboard_mode) < 0)
		{
		fatal ("Unable to get existing keyboard mode.");
		}

	tcgetattr (ttyfd, &tty_attr_old);
	tty_init   =  TRUE;

	/* make stdin non-blocking */
	flags = fcntl (ttyfd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl (ttyfd, F_SETFL, flags);

	/* turn off buffering, echo and key processing */
	tty_attr = tty_attr_old;
	tty_attr.c_lflag &= ~(ICANON | ECHO | ISIG);
	tty_attr.c_iflag &= ~(ISTRIP | INLCR | ICRNL | IGNCR | IXON | IXOFF);
	tcsetattr (ttyfd, TCSANOW, &tty_attr);

	ioctl (ttyfd, KDSKBMODE, K_MEDIUMRAW);

	// Open the file for reading and writing
	fbfd = open("/dev/fb0", O_RDWR);
	if (fbfd == -1)
		{
		fatal ("Unable to open framebuffer device.");
		}
	diag_message (DIAG_WIN_HW, "Opened framebuffer.");

	// Get fixed screen information
	if ( ioctl (fbfd, FBIOGET_FSCREENINFO, &finfo) == -1 )
		{
		fatal ("Error reading famebuffer fixed information.");
		}
	diag_message (DIAG_WIN_HW, "Screen ID: %s", finfo.id);
	diag_message (DIAG_WIN_HW, "Line length = %d.", finfo.line_length);
	pbline   =  (byte *) malloc (finfo.line_length);
	if ( pbline == NULL )   fatal ("Unable to allocate video line buffer.");

	// Get variable screen information
	if ( ioctl (fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1 )
		{
		fatal ("Error reading famebuffer variable information.");
		}
	diag_message (DIAG_WIN_HW, "Resolution: %d x %d, Bits per pixel: %d", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
	diag_message (DIAG_WIN_HW, "Red: offset = %d, length = %d, msb = %d", vinfo.red.offset, vinfo.red.length,
		vinfo.red.msb_right);
	diag_message (DIAG_WIN_HW, "Green: offset = %d, length = %d, msb = %d", vinfo.green.offset, vinfo.green.length,
		vinfo.green.msb_right);
	diag_message (DIAG_WIN_HW, "Blue: offset = %d, length = %d, msb = %d", vinfo.blue.offset, vinfo.blue.length,
		vinfo.blue.msb_right);

	// Figure out the size of the screen in bytes
	screensize = finfo.line_length * vinfo.yres;

	// Map the device to memory
	fbp = (byte *) mmap (0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
	if (  fbp == (byte *) -1 )
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

static __u32 win_fb_component (byte val, struct fb_bitfield *pbf)
	{
	__u32 comp;
	if ( pbf->length < 8 )  comp  =  val >> ( 8 - pbf->length );
	else if ( pbf->length == 8 )  comp  =  val;
	else comp   =  val << ( pbf->length - 8 );
	return   comp << pbf->offset;
	}

static __u32 win_fb_colour (COL col)
	{
	__u32 clr   =  win_fb_component (col.r, &vinfo.red )
		|  win_fb_component (col.g, &vinfo.green )
		|  win_fb_component (col.b, &vinfo.blue );
	return   clr;
	}

/*...swin_refresh:0:*/
void win_refresh(WIN *win_pub)
	{
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
	int  x, y;
	int  i, j;
	__u32 *cols;
	byte *pix;
	byte *fbd;
	int  line_length;
	int  byte_per_pixel;

//   diag_message (DIAG_WIN_HW, "Refresh window %d, Active window = %d", win->iWin, iActiveWin);
	if ( win->iWin != iActiveWin )   return;
	line_length =  win->width * win->width_scale * win->byte_per_pixel;
	fbd   =  fbp + win->top * finfo.line_length + win->left * win->byte_per_pixel;
	pix   =  win->data;
	cols  =  win->cols;
	byte_per_pixel =  win->byte_per_pixel;
	for ( y = 0; y < win->height; ++y )
		{
		switch (byte_per_pixel)
			{
			case  4:    // 32 bit colour (or 16 bit colour duplicated over two pixels)
			{
			__u32 *pdw  =  (__u32 *) pbline;
			if ( win->width_scale == 1 )
				{
				for ( x = 0; x < win->width; ++x )
					{
					*pdw  =  cols[*pix];
					++pdw;
					++pix;
					}
				}
			else if ( win->width_scale == 2 )
				{
				for ( x = 0; x < win->width; ++x )
					{
					__u32 col   =  cols[*pix];
					*pdw  =  col;
					++pdw;
					*pdw  =  col;
					++pdw;
					++pix;
					}
				}
			else
				{
				for ( x = 0; x < win->width; ++x )
					{
					__u32 col   =  cols[*pix];
					for ( i = 0; i < win->width_scale; ++i )
						{
						*pdw  =  col;
						++pdw;
						}
					++pix;
					}
				}
			break;
			}
			case  3:    // 24 bit colour
			{
			byte  *pbl  =  pbline;
			if ( win->width_scale == 1 )
				{
				for ( x = 0; x < win->width; ++x )
					{
					__u32 col   =  cols[*pix];
					*pbl  =  *((byte *) &col);
					++pbl;
					*pbl  =  *((byte *) &col + 1);
					++pbl;
					*pbl  =  *((byte *) &col + 2);
					++pbl;
					++pix;
					}
				}
			else if ( win->width_scale == 2 )
				{
				for ( x = 0; x < win->width; ++x )
					{
					__u32 col   =  cols[*pix];
					*pbl  =  *((byte *) &col);
					++pbl;
					*pbl  =  *((byte *) &col + 1);
					++pbl;
					*pbl  =  *((byte *) &col + 2);
					++pbl;
					*pbl  =  *((byte *) &col);
					++pbl;
					*pbl  =  *((byte *) &col + 1);
					++pbl;
					*pbl  =  *((byte *) &col + 2);
					++pbl;
					++pix;
					}
				}
			else
				{
				for ( x = 0; x < win->width; ++x )
					{
					__u32 col   =  cols[*pix];
					for ( i = 0; i < win->width_scale; ++i )
						{
						*pbl  =  *((byte *) &col);
						++pbl;
						*pbl  =  *((byte *) &col + 1);
						++pbl;
						*pbl  =  *((byte *) &col + 2);
						++pbl;
						}
					++pix;
					}
				}
			break;
			}
			case  2:    // 16 bit colour
			{
			__u16 *pw   =  (__u16 *) pbline;
			if ( win->width_scale == 1 )
				{
				for ( x = 0; x < win->width; ++x )
					{
					*pw   =  cols[*pix];
					++pw;
					++pix;
					}
				}
			else if ( win->width_scale == 2 )
				{
				for ( x = 0; x < win->width; ++x )
					{
					__u32 col   =  cols[*pix];
					*pw   =  col;
					++pw;
					*pw   =  col;
					++pw;
					++pix;
					}
				}
			else
				{
				for ( x = 0; x < win->width; ++x )
					{
					__u32 col   =  cols[*pix];
					for ( i = 0; i < win->width_scale; ++i )
						{
						*pw   =  col;
						++pw;
						}
					++pix;
					}
				}
			break;
			}
			default:
			{
			fatal ("Unsupported bits per pixel.");
			break;
			}
			}
		for ( j = 0; j < win->height_scale; ++j )
			{
			memcpy (fbd, pbline, line_length);
			fbd   += finfo.line_length;
			}
		}
//   diag_message (DIAG_WIN_HW, "Refresh complete");
	}

void win_swap (WIN *win)
	{
	WIN_PRIV   *pwin =	(WIN_PRIV *) win;
	if ( iActiveWin != pwin->iWin )
		{
		byte *fbd   =  fbp;
		int  i;
		diag_message (DIAG_WIN_HW, "Swap to window %d", pwin->iWin);
		iActiveWin  =  pwin->iWin;
		for ( i = 0; i < screensize; ++i )
			{
			*fbd  =  0;
			++fbd;
			}
		win_refresh (win);
		}
	}
/*...e*/

BOOLEAN win_active (WIN *win_pub)
    {
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
    return ( win->iWin == iActiveWin );
    }


void win_max_size (const char *display, int *pWth, int *pHgt)
    {
	if ( fbfd == 0 )  win_fb_init ();
    *pWth = vinfo.xres;
    *pHgt = vinfo.yres;
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

	if ( fbfd == 0 )  win_fb_init ();

	if ( n_wins == MAX_WINS )
		fatal("too many windows");

	win = (WIN_PRIV *) emalloc(sizeof(WIN_PRIV));
	win->iWin      = n_wins;
	win->width           = width;
	win->height          = height;
	win->byte_per_pixel  =  vinfo.bits_per_pixel / 8;
	win->data            = emalloc (width * height);
	memset (win->data, 0, width * height);
	win->width_scale     = vinfo.xres / width;
	win->height_scale    = vinfo.yres / height;
	if ( win->width_scale > win->height_scale )  win->width_scale  =  win->height_scale;
	win->left      = ( vinfo.xres - win->width * win->width_scale ) / 2;
	win->top       = ( vinfo.yres - win->height * win->height_scale ) / 2;
	win->keypress        = keypress;
	win->keyrelease      = keyrelease;
	win->n_cols          = n_cols;
	win->cols      = (__u32 *) emalloc (n_cols * sizeof (__u32));
	for ( i = 0; i < n_cols; ++i ) win->cols[i]   =  win_fb_colour (cols[i]);
	if ( ( win->width_scale == 2 ) && ( win->byte_per_pixel == 2 ) )
		{
		win->byte_per_pixel  *= 2;
		win->width_scale           /= 2;
		win->left            /= 2;
		for ( i = 0; i < n_cols; ++i ) win->cols[i]  *= 0x10001;
		}

	wins[n_wins++] =  win;
	diag_message (DIAG_WIN_HW, "Created window %d, size = %d x %d, scale = %d x %d, position = (%d, %d)",
		n_wins-1, win->width, win->height, win->width_scale, win->height_scale, win->left, win->top);
	win_swap ((WIN *) win);
	return (WIN *) win;
	}
/*...e*/

/*...swin_delete:0:*/
void win_term (void)
	{
	if ( fbfd != 0 )
		{
		munmap(fbp, screensize);
		close(fbfd);
		fbfd = 0;
		}
	if ( pbline != NULL )
		{
		free (pbline);
		pbline   =  NULL;
		}
	if ( tty_init )
		{
		tcsetattr (ttyfd, TCSAFLUSH, &tty_attr_old);
		ioctl (ttyfd, KDSKBMODE, old_keyboard_mode);
		tty_init = FALSE;
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
		if ( iActiveWin == i )  win_swap ((WIN *) wins[n_wins-1]);
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
	unsigned char ks;
	int wk;
	const char *ps;
	} MAPKS;

static MAPKS win_mapks[] =
	{
	{KEY_ENTER,         WK_Return,               "Enter"},
	{KEY_BACKSPACE,     WK_BackSpace,            "Back Space"},
	{KEY_LEFT,          WK_Left,                 "Left Arrow"},
	{KEY_RIGHT,         WK_Right,                "Right Arrow"},
	{KEY_UP,            WK_Up,                   "Up Arrow"},
	{KEY_DOWN,          WK_Down,                 "Down Arrow"},
	{KEY_PAGEUP,        WK_Page_Up,              "Page Up"},
	{KEY_PAGEDOWN,      WK_Page_Down,            "Page Down"},
	{KEY_HOME,          WK_Home,                 "Home"},
	{KEY_END,           WK_End,                  "End"},
	{KEY_INSERT,        WK_Insert,               "Insert"},
	{KEY_DELETE,        WK_Delete,               "Delete"},
	{KEY_PAUSE,         WK_Pause,                "Pause"},
	{KEY_SCROLLLOCK,    WK_Scroll_Lock,          "Scroll Lock"},
	{KEY_SYSRQ,         WK_Sys_Req,              "Sys Req"},
	{KEY_LEFTSHIFT,     WK_Shift_L,              "Left Shift"},
	{KEY_RIGHTSHIFT,    WK_Shift_R,              "Right Shift"},
	{KEY_LEFTCTRL,      WK_Control_L,            "Left Control"},
	{KEY_RIGHTCTRL,     WK_Control_R,            "Right Control"},
	{KEY_CAPSLOCK,      WK_Caps_Lock,            "Caps Lock"},
	{KEY_NUMLOCK,       WK_Num_Lock,             "Num Lock"},
	{KEY_F1,            WK_F1,                   "F1"},
	{KEY_F2,            WK_F2,                   "F2"},
	{KEY_F3,            WK_F3,                   "F3"},
	{KEY_F4,            WK_F4,                   "F4"},
	{KEY_F5,            WK_F5,                   "F5"},
	{KEY_F6,            WK_F6,                   "F6"},
	{KEY_F7,            WK_F7,                   "F7"},
	{KEY_F8,            WK_F8,                   "F8"},
	{KEY_F9,            WK_F9,                   "F9"},
	{KEY_F10,           WK_F10,                  "F10"},
	{KEY_F11,           WK_F11,                  "F11"},
	{KEY_F12,           WK_F12,                  "F12"},
	{KEY_KP4,           WK_KP_Left,              "Keypad 4 (Left)"},
	{KEY_KP6,           WK_KP_Right,             "Keypad 6 (Right)"},
	{KEY_KP8,           WK_KP_Up,                "Keypad 8 (Up)"},
	{KEY_KP2,           WK_KP_Down,              "Keypad 2 (Down)"},
	{KEY_KP9,           WK_KP_Page_Up,           "Keypad 9 (Page Up)"},
	{KEY_KP3,           WK_KP_Page_Down,         "Keypad 3 (Page Down)"},
	{KEY_KP7,           WK_KP_Home,              "Keypad 7 (Home)"},
	{KEY_KP1,           WK_KP_End,               "Keypad 1 (End)"},
	{KEY_KPPLUS,        WK_KP_Add,               "Keypad +"},
	{KEY_KPMINUS,       WK_KP_Subtract,          "Keypad -"},
	{KEY_KPASTERISK,    WK_KP_Multiply,          "Keypad *"},
	{KEY_KPSLASH,       WK_KP_Divide,            "Keypad /"},
	{KEY_KPENTER,       WK_KP_Enter,             "Keypad Enter"},
	{KEY_KP5,           WK_KP_Middle,            "Keypad 5 (Middle)"},
	{KEY_LEFTMETA,      WK_PC_Windows_L,         "Windows Left"}, /* WK_Mac_Cmd_L */
	{KEY_RIGHTMETA,     WK_PC_Windows_R,         "Windows Right"}, /* WK_Mac_Cmd_R */
	{KEY_LEFTALT,       WK_PC_Alt_L,             "Left Alt"},
	{KEY_RIGHTALT,      WK_PC_Alt_R,             "Right Alt"},
	{KEY_MENU,          WK_PC_Menu,              "Menu"},
/*      {KEY_MODE,          WK_Mac_Alt},     /* Alt Gr */
	};

static int win_map_key (unsigned char ks)
	{
	struct kbentry kbe;
	int  i;

	// Special keys.
	for ( i = 0; i < sizeof(win_mapks)/sizeof(win_mapks[0]); i++ )
		{
		if ( ks == win_mapks[i].ks )
			{
			diag_message (DIAG_KBD_HW, "Mapped special key 0x%02x to %s", (int) ks, win_mapks[i].ps);
			return win_mapks[i].wk;
			}
		}

	// Have to deal with shift 3 (Pound sign) as a special case on UK keyboards.
    /*
	if ( ( ks == KEY_3 ) && ( mod_keys & ( MKY_LSHIFT | MKY_RSHIFT ) )
		&& ( ( mod_keys & ( MKY_LCTRL | MKY_RCTRL | MKY_LALT | MKY_RALT ) ) == 0 ) )
		{
		diag_message (DIAG_KBD_HW, "Mapped UK keyboard key 0x%02x to '#'", (int) ks);
		int   key   =  '#';
		return   key;
		}
    */

	// Use keyboard mapping.
	kbe.kb_table   =  K_NORMTAB;
	// if ( mod_keys & ( MKY_LSHIFT | MKY_RSHIFT ) )   kbe.kb_table   |= K_SHIFTTAB;
	// if ( mod_keys & ( MKY_LALT | MKY_RALT ) )       kbe.kb_table   |= K_ALTTAB;
	kbe.kb_index   =  ks;
	kbe.kb_value   =  0;
	if ( ioctl (ttyfd, KDGKBENT, &kbe) >= 0 )
		{
		int   type  =  kbe.kb_value >> 8;
		int   key   =  kbe.kb_value & 0xff;
		if ( ( type == 0x00 ) || ( type == 0x0b ) )
			{
			// if ( ( type == 0x0b ) && ( mod_keys & MKY_CAPSLK ) )  key   ^= 0x20;
			diag_message (DIAG_KBD_HW, "Mapped key 0x%02x to '%c' 0x%02x", (int) ks,
				( ( key >= 0x20 ) && ( key < 0x7f ) ) ? ((char) key) : '.',
				key);
			return   key;
			}
		diag_message (DIAG_KBD_HW, "Key 0x%02x is type 0x%02x code 0x%02x", (int) ks, type, key);
		}
	diag_message (DIAG_KBD_HW, "Can't map ks = 0x%02x", ks);
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
#if (TRUE)
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
#endif
	// This version of the code actually returns the shifted codes. No further conversion is necessary.
	return   wk;
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

/*...swin_handle_events:0:*/

#ifdef	 ALT_HANDLE_EVENTS
extern BOOLEAN ALT_HANDLE_EVENTS (WIN *);
#endif

void win_handle_events()
	{
	unsigned char  key = 0;

#ifdef	 ALT_HANDLE_EVENTS
	if ( ALT_HANDLE_EVENTS (wins[iActiveWin]) )	 return;
#endif

	while ( read (ttyfd, &key, 1) > 0 )
		{
		if ( ( key & 0x80 ) == 0 )
			{
			// Key press.
			diag_message (DIAG_KBD_HW, "Key down event: 0x%02x, Modifiers:%s", key, ListModifiers ());

			// Modifier keys.
			if ( key == KEY_LEFTSHIFT )         mod_keys |= MKY_LSHIFT;
			else if ( key == KEY_RIGHTSHIFT )   mod_keys |= MKY_RSHIFT;
			else if ( key == KEY_LEFTCTRL )     mod_keys |= MKY_LCTRL;
			else if ( key == KEY_RIGHTCTRL )    mod_keys |= MKY_RCTRL;
			else if ( key == KEY_LEFTALT )      mod_keys |= MKY_LALT;
			else if ( key == KEY_RIGHTALT )     mod_keys |= MKY_RALT;
			else if ( key == KEY_CAPSLOCK )     mod_keys |= MKY_CAPSLK;

			// Select window.
			if ( ( mod_keys & MKY_LCTRL ) && ( key >= KEY_F1 ) && ( key <= KEY_F8 ) )
				{
/*
				if ( key == KEY_F1 )
					{
					WIN *win  =	 get_vid_win ();
					if ( win )	 win_swap (win);
					}
				else if ( key == KEY_F2 )
					{
					WIN *win  =	 get_mon_win ();
					if ( win )	 win_swap (win);
					}
				else
*/
                if ( ( key - KEY_F1 ) < n_wins )  win_swap ((WIN *) wins[key - KEY_F1]);
				return;
				}

			// Exit on break.

			if ( key == KEY_PAUSE )
				{
				diag_message (DIAG_KBD_HW, "Break key pressed.");
				terminate ("Break key pressed.");
				}

			// Process key press.
			wins[iActiveWin]->keypress (win_map_key (key));
			}
		else
			{
			// Key release.
			key &= 0x7f;
			diag_message (DIAG_KBD_HW, "Key up event: 0x%x, Modifiers:%02s", key, ListModifiers ());

			// Modifier keys.
			if ( key == KEY_LEFTSHIFT )         mod_keys &= ~MKY_LSHIFT;
			else if ( key == KEY_RIGHTSHIFT )   mod_keys &= ~MKY_RSHIFT;
			else if ( key == KEY_LEFTCTRL )     mod_keys &= ~MKY_LCTRL;
			else if ( key == KEY_RIGHTCTRL )    mod_keys &= ~MKY_RCTRL;
			else if ( key == KEY_LEFTALT )      mod_keys &= ~MKY_LALT;
			else if ( key == KEY_RIGHTALT )     mod_keys &= ~MKY_RALT;
			else if ( key == KEY_CAPSLOCK )     mod_keys &= ~MKY_CAPSLK;

			// Process key release.
			wins[iActiveWin]->keyrelease (win_map_key (key));
			}
		}
	}   
/*...e*/

#define KEYB_LED_CAPS_LOCK      0x04
#define KEYB_LED_NUM_LOCK       0x02
#define KEYB_LED_SCROLL_LOCK    0x01

void win_kbd_leds (BOOLEAN bCaps, BOOLEAN bNum, BOOLEAN bScroll)
    {
    static unsigned int uLast = 0xFF;
    unsigned int uLeds = 0;
    if ( bCaps )    uLeds |= KEYB_LED_CAPS_LOCK;
    if ( bNum )     uLeds |= KEYB_LED_NUM_LOCK;
    if ( bScroll )  uLeds |= KEYB_LED_SCROLL_LOCK;
    if ( uLeds != uLast ) ioctl (ttyfd, KDSETLED, &uLeds);
    uLast = uLeds;
    }
