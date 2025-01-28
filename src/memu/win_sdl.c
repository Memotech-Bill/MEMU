/* win_sdl.c - An SDL implementation of the Window routines */

#include "win.h"
#include "kbd.h"
#include "common.h"
#include "types.h"
#include "diag.h"
#include <SDL3/SDL.h>

typedef struct st_win_sdl
	{
    int iWin;
	int width, height;
	int width_scale, height_scale;
	int n_cols;
	byte *data;
	void (*keypress)(struct st_win *, int);
	void (*keyrelease)(struct st_win *, int);
    TXTBUF *tbuf;
	/* Private SDL window data here */
    SDL_Window *sdl_win;
    SDL_Surface *sdl_sfc;
    SDL_Surface *sdl_drw;
    SDL_Palette *sdl_pal;
    // const char *title;
	} WIN_PRIV;

static struct
    {
    SDL_Window *sdl;
    WIN_PRIV *  win;
    }
    win_idx[MAX_WINS];

static int nwin = 0;

static BOOLEAN bInit = FALSE;

static void win_init (void)
    {
    if ( SDL_Init (SDL_INIT_VIDEO) < 0 )
        fatal ("SDL_Init Error: %s", SDL_GetError ());
    bInit = TRUE;
    }

static void win_add (WIN_PRIV *win)
    {
    int i = n_wins - 1;
    while ((i > 0) && (win_idx[i-1].sdl > win->sdl_win))
        {
        win_idx[i] = win_idx[i-1];
        --i;
        }
    win_idx[i].sdl = win->sdl_win;
    win_idx[i].win = win;
    }

static int win_match (SDL_Window *sdl)
    {
    int i1 = -1;
    int i2 = n_wins;
    while (i2 > i1 + 1)
        {
        int i3 = (i1 + i2) / 2;
        if (win_idx[i3].sdl == sdl) return i3;
        if (win_idx[i3].sdl < sdl) i1 = i3;
        else i2 = i3;
        }
    return -1;
    }

static WIN_PRIV *win_get (SDL_Window *sdl)
    {
    int iwin = win_match (sdl);
    if (iwin < 0) return NULL;
    return win_idx[iwin].win;
    }

static void win_del (WIN_PRIV *win)
    {
    int iwin = win_match (win->sdl_win);
    if (iwin < 0) return;
    while (iwin < n_wins - 1)
        {
        win_idx[iwin] = win_idx[iwin + 1];
        ++iwin;
        }
    }

WIN *win_create(
	int width, int height,
	int width_scale, int height_scale,
	const char *title,
	const char *display,
	const char *geometry,
	COL *col, int n_cols,
	void (*keypress)(WIN *, int),
	void (*keyrelease)(WIN *, int)
	)
    {
    if ( ! bInit ) win_init ();
	WIN_PRIV *win = (WIN_PRIV *) win_alloc (sizeof(WIN_PRIV), 0);

	win->width        = width;
	win->height       = height;
	win->width_scale  = width_scale;
	win->height_scale = height_scale;
    win->n_cols       = n_cols;
	win->keypress     = keypress;
	win->keyrelease   = keyrelease;
    // win->title        = strdup (title);
    diag_message (DIAG_WIN_HW, "%s: (%p)", title, win);
    
    win->sdl_win = SDL_CreateWindow (
        title,
        width * width_scale, height * height_scale,
        0
        );
    if ( win->sdl_win == NULL )
        fatal ("SDL error creating window %s: %s", title, SDL_GetError ());
    win_add (win);
    win->sdl_sfc = SDL_GetWindowSurface (win->sdl_win);
    if ( win->sdl_sfc == NULL )
        fatal ("SDL error getting window surface %s: %s", title, SDL_GetError ());
    win->sdl_drw = SDL_CreateSurface (width, height, SDL_PIXELFORMAT_INDEX8);
    if ( win->sdl_drw == NULL )
        fatal ("SDL error creating drawing surface %s: %s", title, SDL_GetError ());
    win->data = win->sdl_drw->pixels;
    win->sdl_pal = SDL_GetSurfacePalette (win->sdl_drw);
    if (win->sdl_pal == NULL) win->sdl_pal = SDL_CreateSurfacePalette (win->sdl_drw);
    for (int i = 0; i < n_cols; ++i)
        win_colour ((WIN *) win, i, &col[i]);
    return (WIN *) win;
    }

void win_delete (WIN *win_pub)
    {
    if ( win_pub == NULL ) return;
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
    SDL_DestroySurface (win->sdl_drw);
    win->data = NULL;
    SDL_DestroyWindow (win->sdl_win);
    win_del (win);
    win_free ((WIN *) win);
    }

void win_colour (WIN *win_pub, int idx, COL *clr)
    {
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
    SDL_Color sdl_clr;
    sdl_clr.r = clr->r;
    sdl_clr.g = clr->g;
    sdl_clr.b = clr->b;
    sdl_clr.a = 0xFF;
    SDL_SetPaletteColors (win->sdl_pal, &sdl_clr, idx, 1);
    }

void win_refresh (WIN *win_pub)
    {
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
    if ( ! SDL_BlitSurfaceScaled (win->sdl_drw, NULL, win->sdl_sfc, NULL, SDL_SCALEMODE_NEAREST) )
        fatal ("%s: win %p: %d x %d (%d x %d)\n", SDL_GetError (), win, win->width, win->height,
            win->width_scale, win->height_scale);
    SDL_UpdateWindowSurface (win->sdl_win);
    }

typedef struct s_kbd_map
    {
    uint32_t    keycode;
    int         wk;
    } KBD_MAP;

// Entries in the following array must be in increasing order of SDLK_ value
// A bisection search is used to locate matches
static KBD_MAP kbd_map[] = {
    {SDLK_DELETE, WK_Delete},
    {SDLK_CAPSLOCK, WK_Caps_Lock},
    {SDLK_F1, WK_F1},
    {SDLK_F2, WK_F2},
    {SDLK_F3, WK_F3},
    {SDLK_F4, WK_F4},
    {SDLK_F5, WK_F5},
    {SDLK_F6, WK_F6},
    {SDLK_F7, WK_F7},
    {SDLK_F8, WK_F8},
    {SDLK_F9, WK_F9},
    {SDLK_F10, WK_F10},
    {SDLK_F11, WK_F11},
    {SDLK_F12, WK_F12},
    {SDLK_SCROLLLOCK, WK_Scroll_Lock},
    {SDLK_PAUSE, WK_Pause},
    {SDLK_INSERT, WK_Insert},
    {SDLK_HOME, WK_Home},
    {SDLK_PAGEUP, WK_Page_Up},
    {SDLK_END, WK_End},
    {SDLK_PAGEDOWN, WK_Page_Down},
    {SDLK_RIGHT, WK_Right},
    {SDLK_LEFT, WK_Left},
    {SDLK_DOWN, WK_Down},
    {SDLK_UP, WK_Up},
    {SDLK_NUMLOCKCLEAR, WK_Num_Lock},
    {SDLK_KP_DIVIDE, WK_KP_Divide},
    {SDLK_KP_MULTIPLY, WK_KP_Multiply},
    {SDLK_KP_MINUS, WK_KP_Subtract},
    {SDLK_KP_PLUS, WK_KP_Add},
    {SDLK_KP_ENTER, WK_KP_Enter},
    {SDLK_KP_1, WK_KP_End},
    {SDLK_KP_2, WK_KP_Down},
    {SDLK_KP_3, WK_KP_Page_Down},
    {SDLK_KP_4, WK_KP_Left},
    {SDLK_KP_5, WK_KP_Middle},
    {SDLK_KP_6, WK_KP_Right},
    {SDLK_KP_7, WK_KP_Home},
    {SDLK_KP_8, WK_KP_Up},
    {SDLK_KP_9, WK_KP_Page_Up},
    {SDLK_APPLICATION, WK_Menu},
    {SDLK_SYSREQ, WK_Sys_Req},
    {SDLK_LCTRL, WK_Control_L},
    {SDLK_LSHIFT, WK_Shift_L},
    {SDLK_LALT, WK_PC_Alt_L},
    {SDLK_LGUI, WK_PC_Windows_L},
    {SDLK_RCTRL, WK_Control_R},
    {SDLK_RSHIFT, WK_Shift_R},
    {SDLK_RALT, WK_PC_Alt_R},
    {SDLK_RGUI, WK_PC_Windows_R},
    {SDLK_MODE, WK_PC_Alt_R},           // SDL3 on Linux is returning this for Right Alt key.
    };

static int sdlkey_wk (int keycode)
    {
    if (( keycode > 0 ) && ( keycode < 127 )) return keycode;
    int iFst = -1;
    int iLst = sizeof (kbd_map) / sizeof (kbd_map[0]);
    while ( (iLst - iFst) > 1 )
        {
        int iNxt = (iFst + iLst) / 2;
        if ( kbd_map[iNxt].keycode == keycode ) return kbd_map[iNxt].wk;
        else if ( kbd_map[iNxt].keycode > keycode ) iLst = iNxt;
        else iFst = iNxt;
        }
    return 0;
    }

#ifdef HAVE_JOY
void joy_handle_events (SDL_Event *e);
#endif

void win_handle_events (void)
    {
    SDL_Window *sdl_win;
    SDL_Event e;
    while ( SDL_PollEvent (&e) != 0 )
        {
        switch (e.type)
            {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                terminate("user closed window");
                break;
            case SDL_EVENT_WINDOW_EXPOSED:
                sdl_win = SDL_GetWindowFromID (e.window.windowID);
                if ( sdl_win == NULL ) break;
                WIN_PRIV *win = win_get (sdl_win);
                if (win != NULL) win_refresh ((WIN *) win);
                break;
            case SDL_EVENT_KEY_DOWN:
                if ( ! e.key.repeat )
                    {
                    // printf ("key down: windowID = %d, key = 0x%02X, scan = 0x%02X, raw = 0x%02X\n",
                    //     e.key.windowID, e.key.key, e.key.scancode, e.key.raw);
                    sdl_win = SDL_GetWindowFromID (e.key.windowID);
                    if ( sdl_win == NULL ) break;
                    WIN_PRIV *win = win_get (sdl_win);
                    if (win == NULL) break;
                    int wk = sdlkey_wk (e.key.key);
                    // printf ("keypress: %s, wk = 0x%02X\n", win->title, wk);
                    if (wk > 0) win->keypress ((WIN *) win, wk);
                    }
                break;
            case SDL_EVENT_KEY_UP:
                if ( ! e.key.repeat )
                    {
                    // printf ("key up: windowID = %d, key = 0x%02X, scan = 0x%02X, raw = 0x%02X\n",
                    //     e.key.windowID, e.key.key, e.key.scancode, e.key.raw);
                    sdl_win = SDL_GetWindowFromID (e.key.windowID);
                    if ( sdl_win == NULL ) break;
                    WIN_PRIV *win = win_get (sdl_win);
                    if (win == NULL) break;
                    int wk = sdlkey_wk (e.key.key);
                    // printf ("keyrelease: %s, wk = 0x%02X\n", win->title, wk);
                    if (wk > 0) win->keyrelease ((WIN *) win, wk);
                    }
                break;
            default:
#ifdef HAVE_JOY
                joy_handle_events (&e);
#endif
                break;
            }
        }
    }

void win_show (WIN *win)
    {
    }

BOOLEAN win_active (WIN *win)
    {
    return TRUE;
    }

void win_term (void)
    {
    }

void win_max_size (const char *display, int *pWth, int *pHgt)
    {
    if ( ! bInit ) win_init ();
    int num_displays;
    SDL_DisplayID *displays = SDL_GetDisplays(&num_displays);
    if ((displays == NULL) || (num_displays == 0)) fatal ("No displays defined");
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds (displays[0], &rect);
    *pWth = rect.w;
    *pHgt = rect.h;
    SDL_free (displays);
    }

extern void kbd_chk_leds (int *mods)
    {
    }
