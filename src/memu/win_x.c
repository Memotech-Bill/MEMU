/*

win.c - XWindows Window

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...e*/

/*...sDPY:0:*/
/*
We may wish to display a number of XWindows windows.
Each may be on a different display, or not.
So windows share connections to displays.
*/

typedef struct dpystruct
	{
	char *display;
	Display *disp;
	int scrn;
	Visual *v;
	int n_wins;
	} DPY;
/*...e*/
/*...sWIN_PRIV:0:*/
typedef struct
	{
	int width, height;
	int width_scale, height_scale;
	int n_cols;
	byte *data;
	void (*keypress)(int);
	void (*keyrelease)(int);
	/* Private window data below - Above must match definition of WIN in win.h */
	DPY *dpy;
	GC gc;
	Window w, w_bitmap;
	Atom delete_atom;
	COL cols[N_COLS_MAX];
	int n_pixs;
	byte pix[N_COLS_MAX];
	XImage *ximage;
    const char *title;
	} WIN_PRIV;
/*...e*/

/*...sdpy_create:0:*/
static DPY *dpy_create(const char *d)
	{
    Bool detectable = True;
	DPY *dpy = (DPY *) emalloc(sizeof(DPY));
	if ( d != NULL )
		dpy->display = estrdup(d);
	else
		dpy->display = NULL;
	if ( (dpy->disp = XOpenDisplay(dpy->display)) == 0 )
		fatal("can't open display");
    // XSynchronize (dpy->disp, True);
	dpy->scrn   = DefaultScreen(dpy->disp);
	dpy->v      = DefaultVisual(dpy->disp, dpy->scrn);
	dpy->n_wins = 1;
/*
	XAutoRepeatOff(dpy->disp);
*/
    XkbSetDetectableAutoRepeat (dpy->disp, detectable, &detectable);
	return dpy;
	}
/*...e*/
/*...sdpy_delete:0:*/
static void dpy_delete(DPY *dpy)
	{
/*
	XAutoRepeatOn(dpy->disp);
*/
	XCloseDisplay(dpy->disp);
	if ( dpy->display != NULL )
		free(dpy->display);
	free(dpy);
	}
/*...e*/

#define	MAX_DPYS 10
static int n_dpys = 0;
static DPY *dpys[MAX_DPYS];

/*...sdpy_connect:0:*/
static DPY *dpy_connect(const char *display)
	{
	int i;
	for ( i = 0; i < n_dpys; i++ )
		if ( dpys[i]->display == NULL && display == NULL )
			break;
		else if ( dpys[i]->display != NULL && display != NULL &&
			  !strcmp(dpys[i]->display, display) )
			break;
	if ( i < n_dpys )
		{
		++(dpys[i]->n_wins);
		return dpys[i];
		}
	else if ( n_dpys == MAX_DPYS )
		fatal("too many displays");
	else
		{
		dpys[n_dpys] = dpy_create(display);
		return dpys[n_dpys++];
		}
	return NULL; /* Can't get here */
	}
/*...e*/
/*...sdpy_disconnect:0:*/
static void dpy_disconnect(DPY *dpy)
	{
	int i;
	for ( i = 0; dpys[i] != dpy; i++ )
		;
	if ( --(dpys[i]->n_wins) == 0 )
		{
		dpy_delete(dpys[i]);	
		dpys[i] = dpys[--n_dpys];
		}
	}
/*...e*/

void win_max_size (const char *display, int *pWth, int *pHgt)
    {
    DPY *dpy = dpy_connect (display);
    *pWth = XDisplayWidth (dpy->disp, dpy->scrn);
    *pHgt = XDisplayHeight (dpy->disp, dpy->scrn);
    dpy_disconnect (dpy);
    }

#define	MAX_WINS 10
static int n_wins = 0; 
static WIN_PRIV *wins[MAX_WINS];

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
	WIN_PRIV *win = (WIN_PRIV *) emalloc(sizeof(WIN_PRIV));
    diag_message (DIAG_WIN_HW, "%s: win = %p", title, win);
	int winx, winy, winw, winh, flags;

	if ( n_wins == MAX_WINS )
		fatal("too many windows");

	win->width        = width;
	win->height       = height;
	win->width_scale  = width_scale;
	win->height_scale = height_scale;
    win->title        = title;
    diag_message (DIAG_WIN_HW, "%s: (%p)", title, win);

	win->dpy = dpy_connect(display);

	wins[n_wins++] = win;

	if ( geometry != 0 )
		{
		XGeometry(win->dpy->disp, win->dpy->scrn, geometry, "100x100+0+0",
			0, /* border width */
			1, 1, /* font width and height */
			0, 0, /* additional interior padding */
			&winx, &winy, &winw, &winh);
		flags = USPosition|USSize; /* User specified */
		}
	else
		{
		winx = 0;
		winy = 0;
		winw = width*width_scale;
		winh = height*height_scale;
		flags = PSize; /* Program determined size */
		}

	{
	XSetWindowAttributes swa;
	swa.background_pixel = BlackPixel(win->dpy->disp, win->dpy->scrn);
	swa.bit_gravity      = NorthWestGravity;
	swa.backing_store    = NotUseful;
	swa.event_mask       = KeyPressMask | KeyReleaseMask | ExposureMask;
	swa.save_under       = False;
	win->w = XCreateWindow(
		win->dpy->disp, RootWindow(win->dpy->disp, win->dpy->scrn),
		winx, winy, winw, winh,
		0,
		DefaultDepth(win->dpy->disp, win->dpy->scrn),
		InputOutput,
		win->dpy->v,
		CWBackPixel|CWBitGravity|CWBackingStore|CWEventMask|CWSaveUnder,
		&swa
		);
	}

	XStoreName(win->dpy->disp, win->w, title);
	XSetIconName(win->dpy->disp, win->w, title);

	{
	XSizeHints sh;
	sh.width      = winw;
	sh.height     = winh;
	sh.min_width  = sh.max_width  = width*width_scale;
	sh.min_height = sh.max_height = height*height_scale;
	sh.x          = winx;
	sh.y          = winy;
	sh.flags      = PMinSize | PMaxSize | flags;
	XSetNormalHints(win->dpy->disp, win->w, &sh);
	}

	{
	XClassHint ch;
	ch.res_class = (char *) "Xmemuxwin";
	ch.res_name  = (char *) "xmemuxwin";
	XSetClassHint(win->dpy->disp, win->w, &ch);
	}

	{
	Atom proto_atom;
	proto_atom       = XInternAtom(win->dpy->disp, (char *) "WM_PROTOCOLS"    , False);
	win->delete_atom = XInternAtom(win->dpy->disp, (char *) "WM_DELETE_WINDOW", False);
	if ( proto_atom != None && win->delete_atom != None )
		XChangeProperty(win->dpy->disp, win->w, proto_atom, XA_ATOM, 32,
			PropModeReplace, (unsigned char *) &(win->delete_atom), 1);
	}

	{
	XSetWindowAttributes swa;
	swa.background_pixel = BlackPixel(win->dpy->disp, win->dpy->scrn);
	swa.bit_gravity      = NorthWestGravity;
	swa.backing_store    = WhenMapped;
	swa.event_mask       = ExposureMask;
	swa.save_under       = False;
	win->w_bitmap = XCreateWindow(
		win->dpy->disp, win->w,
		0, 0, width*width_scale, height*height_scale,
		0,
		DefaultDepth(win->dpy->disp, win->dpy->scrn),
		InputOutput,
		win->dpy->v,
		CWBackPixel|CWBitGravity|CWBackingStore|CWEventMask|CWSaveUnder,
		&swa
		);
	}

	{
	XSizeHints sh;
	sh.width      = width*width_scale;
	sh.height     = height*height_scale;
	sh.min_width  = sh.max_width  = width*width_scale;
	sh.min_height = sh.max_height = height*height_scale;
	sh.flags      = PMinSize | PMaxSize | PSize;
	XSetNormalHints(win->dpy->disp, win->w_bitmap, &sh);
	}

	{
	XClassHint ch;
	ch.res_class = (char *) "Xmemubitmap";
	ch.res_name  = (char *) "xmemubitmap";
	XSetClassHint(win->dpy->disp, win->w_bitmap, &ch);
	}

	{
	XGCValues gcv;
	gcv.function = GXcopy;
	win->gc = XCreateGC(win->dpy->disp, win->w_bitmap, GCFunction, &gcv);
	}

	win->data = emalloc(width*height);
	memset(win->data, 0, width*height);
    diag_message (DIAG_WIN_HW, "%s: win->data = %p", title, win->data);

	switch ( win->dpy->v->class )
		{
/*...sTrueColor\44\ DirectColor:16:*/
case TrueColor:
case DirectColor:
	{
	int i;
	int bpp; /* bits per pixel */
	int bypp; /* bytes per pixel, padded */
	     if ( win->dpy->v->red_mask   == 0xff0000 &&
	          win->dpy->v->green_mask == 0x00ff00 &&
	          win->dpy->v->blue_mask  == 0x0000ff )
		/* eg: my Linux Fedora x86_64 box running X.Org */
		{
		bpp = 24;
		bypp = 4;
		}
	else if ( win->dpy->v->red_mask   == 0x0000ff &&
	          win->dpy->v->green_mask == 0x00ff00 &&
	          win->dpy->v->blue_mask  == 0xff0000 )
		{
		bpp = 24;
		bypp = 4;
		}
	else if ( win->dpy->v->red_mask   == 0x00f800 &&
	          win->dpy->v->green_mask == 0x0007e0 &&
	          win->dpy->v->blue_mask  == 0x00001f )
		/* Raspberry Pi */
		{
		bpp = 16;
		bypp = 2;
		}
	else
		fatal("visual has unsupported red/green/blue masks (%08x/%08x/%08x)",
			win->dpy->v->red_mask,
			win->dpy->v->green_mask,
			win->dpy->v->blue_mask);
    if ( n_cols > N_COLS_MAX ) fatal ("Window has too many colours");
	for ( i = 0; i < n_cols; i++ )
		{
		win->cols[i].r = cols[i].r;
		win->cols[i].g = cols[i].g;
		win->cols[i].b = cols[i].b;
		}
	int xstride = width*width_scale * bypp;
	win->ximage = XCreateImage(
		win->dpy->disp, win->dpy->v,	/* display and visual */
		bpp,				/* image depth */
		ZPixmap,			/* XImage format */
		0,				/* offset */
		NULL,				/* data */
		width*width_scale, height*height_scale, /* size in pixels */
		32,				/* scanline alignment */
		0				/* let it work out bytes per line */
		);
	win->ximage->data = (char *) emalloc(xstride*height*height_scale);
    diag_message (DIAG_WIN_HW, "%s: ximage->data = %p", title, win->ximage->data);
	win->ximage->byte_order = LSBFirst;
	}
	break;
/*...e*/
/*...sPseudoColor:16:*/
case PseudoColor:
	{
	int i;
	win->n_pixs = n_cols;
	for ( i = 0; i < n_cols; i++ )
		{
		XColor xcol;
		xcol.red   = cols[i].r * 0x0101;
		xcol.green = cols[i].g * 0x0101;
		xcol.blue  = cols[i].b * 0x0101;
		if ( XAllocColor(win->dpy->disp, DefaultColormap(win->dpy->disp, win->dpy->scrn), &xcol) )
			win->pix[i] = xcol.pixel;
		else
			fatal("can't allocate enough palette entries");
		}
	int xstride = width*width_scale;
	win->ximage = XCreateImage(
		win->dpy->disp, win->dpy->v,	/* display and visual */
		8,				/* image depth */
		ZPixmap,			/* XImage format */
		0,				/* offset */
		NULL,				/* data */
		width*width_scale, height*height_scale, /* size in pixels */
		8,				/* scanline alignment */
		0				/* let it work out bytes per line */
		);
	win->ximage->data = (char *) emalloc(xstride*height*height_scale);
    diag_message (DIAG_WIN_HW, "%s: ximage->data = %p", title, win->ximage->data);
	win->ximage->byte_order = LSBFirst;
	}
	break;
/*...e*/
/*...sdefault:16:*/
default:
	fatal("can't work with current XWindows visual");
/*...e*/
		}

	win->keypress   = keypress;
	win->keyrelease = keyrelease;

	XMapWindow(win->dpy->disp, win->w_bitmap);
	XMapWindow(win->dpy->disp, win->w);

//	XSync(win->dpy->disp, False);
	XSync(win->dpy->disp, True);
    diag_message (DIAG_WIN_HW, "%s: (%p)", win->title, win);
        diag_message (DIAG_WIN_HW, "disp = %p", win->dpy->disp);         // Display
        diag_message (DIAG_WIN_HW, "w_bitmap = %p",	win->w_bitmap);          // Drawable
        diag_message (DIAG_WIN_HW, "gc = %p", win->gc);                // Graphics context
        diag_message (DIAG_WIN_HW, "ximage = %p", win->ximage);           // Image to combine
    /*
        diag_message (DIAG_WIN_HW, "%s: (%p) win_create (%p, %p, %p, %p)",
            win->title, win,
			win->dpy->disp,         // Display
			win->w_bitmap,          // Drawable
			win->gc,                // Graphics context
			win->ximage);           // Image to combine
    */
	return (WIN *) win;
	}
/*...e*/
/*...swin_delete:0:*/
void win_delete(WIN *win_pub)
	{
	if ( win_pub == NULL ) return;
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
	XFreeGC(win->dpy->disp, win->gc);

    diag_message (DIAG_WIN_HW, "win_delete %s: win->data = %p", win->title, win->data);
	free(win->data);

    diag_message (DIAG_WIN_HW, "win_delete %s: ximage->data = %p", win->title, win->ximage->data);
	free(win->ximage->data);
	win->ximage->data = NULL;
	XDestroyImage(win->ximage);

	switch ( win->dpy->v->class )
		{
/*...sPseudoColor:16:*/
case PseudoColor:
	{
	unsigned long pixl[0x100];
	int i;
	for ( i = 0; i < win->n_pixs; i++ )
		pixl[i] = win->pix[i];
	XFreeColors(win->dpy->disp, DefaultColormap(win->dpy->disp, win->dpy->scrn), pixl, win->n_pixs, 0);
	}
	break;
/*...e*/
		}

	XDestroyWindow(win->dpy->disp, win->w_bitmap);
	XDestroyWindow(win->dpy->disp, win->w);

	dpy_disconnect(win->dpy);

	int i;
	for ( i = 0; wins[i] != win; i++ )
		;
	wins[i] = wins[--n_wins];
    diag_message (DIAG_WIN_HW, "win_delete %s: win = %p", win->title, win);
	free(win);
	}
/*...e*/

/*...swin_refresh:0:*/
void win_refresh(WIN *win_pub)
	{
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
	switch ( win->dpy->v->class )
		{
/*...sTrueColor\44\ DirectColor:16:*/
case TrueColor:
case DirectColor:
	{
	int bypp = ( win->dpy->v->red_mask == 0x00f800 ) ? 2 : 4;
	int xstride = win->width*win->width_scale * bypp;
	int x, y, xdup, ydup, w = win->width, h = win->height;
	COL *cols = win->cols;
	switch ( win->dpy->v->red_mask )
		{
/*...s0xff0000:32:*/
case 0xff0000:
	for ( y = 0; y < h; y++ )
		{
		byte *src = win->data +                  y * w;
		byte *dst = (byte *) win->ximage->data + y * xstride * win->height_scale;
		if ( win->width_scale == 1 )
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				*dst++ = cols[p].b;
				*dst++ = cols[p].g;
				*dst++ = cols[p].r;
				 dst++;
				}
		else if ( win->width_scale == 2 )
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				*dst++ = cols[p].b;
				*dst++ = cols[p].g;
				*dst++ = cols[p].r;
				 dst++;
				*dst++ = cols[p].b;
				*dst++ = cols[p].g;
				*dst++ = cols[p].r;
				 dst++;
				}
		else
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				for ( xdup = 0; xdup < win->width_scale; xdup++ )
					{
					*dst++ = cols[p].b;
					*dst++ = cols[p].g;
					*dst++ = cols[p].r;
					 dst++;
					}
				}
		for ( ydup = 1; ydup < win->height_scale; ydup++ )
			{
			memcpy(dst, dst-xstride, xstride);
			dst += xstride;
			}
		}
	break;
/*...e*/
/*...s0x0000ff:32:*/
case 0x0000ff:
	for ( y = 0; y < h; y++ )
		{
		byte *src = win->data +                  y * w;
		byte *dst = (byte *) win->ximage->data + y * xstride * win->height_scale;
		if ( win->width_scale == 1 )
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				*dst++ = cols[p].r;
				*dst++ = cols[p].g;
				*dst++ = cols[p].b;
				 dst++;
				}
		else if ( win->width_scale == 2 )
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				*dst++ = cols[p].r;
				*dst++ = cols[p].g;
				*dst++ = cols[p].b;
				 dst++;
				*dst++ = cols[p].r;
				*dst++ = cols[p].g;
				*dst++ = cols[p].b;
				 dst++;
				}
		else
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				for ( xdup = 0; xdup < win->width_scale; xdup++ )
					{
					*dst++ = cols[p].r;
					*dst++ = cols[p].g;
					*dst++ = cols[p].b;
					 dst++;
					}
				}
		for ( ydup = 1; ydup < win->height_scale; ydup++ )
			{
			memcpy(dst, dst-xstride, xstride);
			dst += xstride;
			}
		}
	break;
/*...e*/
/*...s0x00f800:32:*/
case 0x00f800:
	for ( y = 0; y < h; y++ )
		{
		byte *src = win->data +                  y * w;
		byte *dst = (byte *) win->ximage->data + y * xstride * win->height_scale;
		if ( win->width_scale == 1 )
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				byte r = cols[p].r;
				byte g = cols[p].g;
				byte b = cols[p].b;
				byte rg = (r&0xf8) | (g>>5);
				byte gb = ((g&0x1c)<<3) | (b>>3);
				*dst++ = gb;
				*dst++ = rg;
				}
		else if ( win->width_scale == 2 )
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				byte r = cols[p].r;
				byte g = cols[p].g;
				byte b = cols[p].b;
				byte rg = (r&0xf8) | (g>>5);
				byte gb = ((g&0x1c)<<3) | (b>>3);
				*dst++ = gb;
				*dst++ = rg;
				*dst++ = gb;
				*dst++ = rg;
				}
		else
			for ( x = 0; x < w; x++ )
				{
				byte p = *src++;
				byte r = cols[p].r;
				byte g = cols[p].g;
				byte b = cols[p].b;
				byte rg = (r&0xf8) | (g>>5);
				byte gb = ((g&0x1c)<<3) | (b>>3);
				for ( xdup = 0; xdup < win->width_scale; xdup++ )
					{
					*dst++ = gb;
					*dst++ = rg;
					}
				}
		for ( ydup = 1; ydup < win->height_scale; ydup++ )
			{
			memcpy(dst, dst-xstride, xstride);
			dst += xstride;
			}
		}
	break;
/*...e*/
		}
	}
	break;
/*...e*/
/*...sPseudoColor:16:*/
case PseudoColor:
	{
	int xstride = win->width*win->width_scale;
	int x, y, xdup, ydup, w = win->width, h = win->height;
	byte *pix = win->pix;
	for ( y = 0; y < h; y++ )
		{
		byte *src = win->data +                  y * w;
		byte *dst = (byte *) win->ximage->data + y * xstride * win->height_scale;
		if ( win->width_scale == 1 )
			for ( x = 0; x < w; x++ )
				*dst++ = pix[src[x]];
		else if ( win->width_scale == 2 )
			for ( x = 0; x < w; x++ )
				{
				byte p = pix[src[x]];
				*dst++ = p;
				*dst++ = p;
				}
		else
			for ( x = 0; x < w; x++ )
				{
				byte p = pix[src[x]];
				for ( xdup = 0; xdup < win->width_scale; xdup++ )
					*dst++ = p;
				}
		for ( ydup = 1; ydup < win->height_scale; ydup++ )
			{
			memcpy(dst, dst-xstride, xstride);
			dst += xstride;
			}
		}
	}
	break;
/*...e*/
		}
	XPutImage(win->dpy->disp, win->w_bitmap, win->gc, win->ximage,
	          0, 0, 0, 0, win->width*win->width_scale, win->height*win->height_scale);
	}
/*...e*/

/*...swin_map_key:0:*/
typedef struct
	{
	KeySym ks;
	int wk;
	} MAPKS;

static MAPKS win_mapks[] =
	{
		{XK_BackSpace		,WK_BackSpace},
		{XK_Tab			,WK_Tab},
		{XK_Linefeed		,WK_Linefeed},
		{XK_Return		,WK_Return},
		{XK_Escape		,WK_Escape},
		{XK_Left		,WK_Left},
		{XK_Right		,WK_Right},
		{XK_Up			,WK_Up},
		{XK_Down		,WK_Down},
		{XK_Page_Up		,WK_Page_Up},
		{XK_Page_Down		,WK_Page_Down},
		{XK_Home		,WK_Home},
		{XK_End			,WK_End},
		{XK_Insert		,WK_Insert},
		{XK_Delete		,WK_Delete},
		{XK_Pause		,WK_Pause},
		{XK_Scroll_Lock		,WK_Scroll_Lock},
		{XK_Sys_Req		,WK_Sys_Req},
		{XK_Shift_L		,WK_Shift_L},
		{XK_Shift_R		,WK_Shift_R},
		{XK_Control_L		,WK_Control_L},
		{XK_Control_R		,WK_Control_R},
		{XK_Caps_Lock		,WK_Caps_Lock},
		{XK_Shift_Lock		,WK_Shift_Lock},
		{XK_Num_Lock		,WK_Num_Lock},
		{XK_F1			,WK_F1},
		{XK_F2			,WK_F2},
		{XK_F3			,WK_F3},
		{XK_F4			,WK_F4},
		{XK_F5			,WK_F5},
		{XK_F6			,WK_F6},
		{XK_F7			,WK_F7},
		{XK_F8			,WK_F8},
		{XK_F9			,WK_F9},
		{XK_F10			,WK_F10},
		{XK_F11			,WK_F11},
		{XK_F12			,WK_F12},
		{XK_KP_Left		,WK_KP_Left},
		{XK_KP_Right		,WK_KP_Right},
		{XK_KP_Up		,WK_KP_Up},
		{XK_KP_Down		,WK_KP_Down},
		{XK_KP_Page_Up		,WK_KP_Page_Up},
		{XK_KP_Page_Down	,WK_KP_Page_Down},
		{XK_KP_Home		,WK_KP_Home},
		{XK_KP_End		,WK_KP_End},
		{XK_KP_Add		,WK_KP_Add},
		{XK_KP_Subtract		,WK_KP_Subtract},
		{XK_KP_Multiply		,WK_KP_Multiply},
		{XK_KP_Divide		,WK_KP_Divide},
		{XK_KP_Enter		,WK_KP_Enter},
		{XK_KP_Begin		,WK_KP_Middle},
		{XK_Super_L		,WK_PC_Windows_L},
		{XK_Super_R		,WK_PC_Windows_R},
		{XK_Alt_L		,WK_PC_Alt_L},
		{XK_Alt_R		,WK_PC_Alt_R},
		{XK_ISO_Level3_Shift	,WK_PC_Alt_R}, /* Alt Gr */
		{XK_Menu		,WK_PC_Menu},
		{XK_Meta_L		,WK_Mac_Cmd_L},
		{XK_Meta_R		,WK_Mac_Cmd_R},
		{XK_Mode_switch		,WK_Mac_Alt},
	};

static int win_map_key(KeySym ks)
	{
	int i;
	if ( ks < 0x100 )
		return ks;
	for ( i = 0; i < sizeof(win_mapks)/sizeof(win_mapks[0]); i++ )
		if ( ks == win_mapks[i].ks )
			return win_mapks[i].wk;
	diag_message(DIAG_WIN_UNKNOWN_KEY, "win_map_key can't map ks=0x%04x", ks);
	return -1;
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
		case '1':	return '!';
		case '2':	return '"';
		case '3':	return '#'; /* pound */
		case '4':	return '$';
		case '5':	return '%';
		case '6':	return '^';
		case '7':	return '&';
		case '8':	return '*';
		case '9':	return '(';
		case '0':	return ')';
		case '-':	return '_';
		case '=':	return '+';
		case '[':	return '{';
		case ']':	return '}';
		case ';':	return ':';
		case '\'':	return '@';
		case '#':	return '~';
		case '\\':	return '|';
		case ',':	return '<';
		case '.':	return '>';
		case '/':	return '?';
		default:	return ( wk >= 0 && wk < 0x100 ) ? wk : -1;
		}
	}
/*...e*/

/*...swin_handle_events:0:*/
/*...sfind_win_w:0:*/
static WIN_PRIV *find_win_w(Window w)
	{
	int i;
	for ( i = 0; i < n_wins; i++ )
		if ( wins[i]->w == w )
			return wins[i];
	return NULL;
	}
/*...e*/
/*...sfind_win_w_bitmap:0:*/
static WIN_PRIV *find_win_w_bitmap(Window w)
	{
	int i;
	for ( i = 0; i < n_wins; i++ )
		if ( wins[i]->w_bitmap == w )
			return wins[i];
	return NULL;
	}
/*...e*/

void win_handle_events()
	{
	int i;
	for ( i = 0; i < n_dpys; i++ )
		{
		DPY *dpy = dpys[i];
		while ( XPending(dpy->disp) )
			{
			XEvent event;
			XNextEvent(dpy->disp, &event);
			WIN_PRIV *win;
			switch ( event.type )
				{
/*...sExpose:32:*/
case Expose:
	if ( (win = find_win_w_bitmap(event.xexpose.window)) != NULL )
        {
        diag_message (DIAG_WIN_HW, "%s: (%p) XPutImage (%p, %p, %p, %p, %d, %d, %d, %d, %d, %d)",
            win->title, win,
			win->dpy->disp,         // Display
			win->w_bitmap,          // Drawable
			win->gc,                // Graphics context
			win->ximage,            // Image to combine
			event.xexpose.x,        // Source X
			event.xexpose.y,        // Source Y
			event.xexpose.x,        // Destination X
			event.xexpose.y,        // Destination Y
			event.xexpose.width,    // Width
			event.xexpose.height    // Height
			);
		XPutImage(
			win->dpy->disp,         // Display
			win->w_bitmap,          // Drawable
			win->gc,                // Graphics context
			win->ximage,            // Image to combine
			event.xexpose.x,        // Source X
			event.xexpose.y,        // Source Y
			event.xexpose.x,        // Destination X
			event.xexpose.y,        // Destination Y
			event.xexpose.width,    // Width
			event.xexpose.height    // Height
			);
        }
	break;
/*...e*/
/*...sKeyPress:32:*/
case KeyPress:
	if ( (win = find_win_w(event.xkey.window)) != NULL )
		{
		KeySym ks;
#if 0
		char buf[128];
		XComposeStatus cs;
		if ( XLookupString(&(event.xkey), buf, 128, &ks, &cs) == 1 )
			win_event_keypress(win, buf[0]);
#endif
		unsigned int modifiers = 0;
		int wk;
		XkbLookupKeySym(win->dpy->disp, event.xkey.keycode, modifiers, &modifiers, &ks);
		if ( (wk = win_map_key(ks)) != -1 )
			(*win->keypress)(wk);
		}
	break;
/*...e*/
/*...sKeyRelease:32:*/
case KeyRelease:
	if ( (win = find_win_w(event.xkey.window)) != NULL )
		{
		KeySym ks;
#if 0
		char buf[128];
		XComposeStatus cs;
		if ( XLookupString(&(event.xkey), buf, 128, &ks, &cs) == 1 )
			win_event_keyrelease(win, buf[0]);
#endif
		unsigned int modifiers = 0;
		int wk;
		XkbLookupKeySym(win->dpy->disp, event.xkey.keycode, modifiers, &modifiers, &ks);
		if ( (wk = win_map_key(ks)) != -1 )
			(*win->keyrelease)(wk);
		}
	break;
/*...e*/
/*...sClientMessage:32:*/
case ClientMessage:
	if ( (win = find_win_w(event.xclient.window)) != NULL )
		if ( event.xclient.data.l[0] == win->delete_atom )
			terminate("user closed window");
	break;
/*...e*/
				}
			}
		}
	}	
/*...e*/

BOOLEAN win_active (WIN *win)
    {
    return TRUE;
    }

// LED ordering to be confirmed
#define KEYB_LED_CAPS_LOCK      3
#define KEYB_LED_NUM_LOCK       2
#define KEYB_LED_SCROLL_LOCK    1

void win_kbd_leds (BOOLEAN bCaps, BOOLEAN bNum, BOOLEAN bScroll)
    {
    static byte uLast = 0xFF;
    byte uLeds = 0;
    diag_message (DIAG_KBD_HW, "win_kbd_leds (%d, %d, %d)", bCaps, bNum, bScroll);
    if ( bCaps )    uLeds |= 1 << KEYB_LED_CAPS_LOCK;
    if ( bNum )     uLeds |= 1 << KEYB_LED_NUM_LOCK;
    if ( bScroll )  uLeds |= 1 << KEYB_LED_SCROLL_LOCK;
    if ( ( uLeds != uLast ) && ( n_dpys > 0 ) )
        {
        XKeyboardControl kbctl;
        diag_message (DIAG_KBD_HW, "win_kbd_leds (%d, %d, %d) - act", bCaps, bNum, bScroll);
        uLast = uLeds;
        kbctl.led = KEYB_LED_CAPS_LOCK;
        kbctl.led_mode = bCaps ? LedModeOn : LedModeOff;
        XChangeKeyboardControl (dpys[0]->disp, KBLed | KBLedMode, &kbctl);
        kbctl.led = KEYB_LED_NUM_LOCK;
        kbctl.led_mode = bNum ? LedModeOn : LedModeOff;
        XChangeKeyboardControl (dpys[0]->disp, KBLed | KBLedMode, &kbctl);
        kbctl.led = KEYB_LED_SCROLL_LOCK;
        kbctl.led_mode = bScroll ? LedModeOn : LedModeOff;
        XChangeKeyboardControl (dpys[0]->disp, KBLed | KBLedMode, &kbctl);
        }
    diag_message (DIAG_KBD_HW, "win_kbd_leds (%d, %d, %d) - done", bCaps, bNum, bScroll);
    }
