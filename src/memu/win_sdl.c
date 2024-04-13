/* win_sdl.c - An SDL implementation of the Window routines */

#include "win.h"
#include "kbd.h"
#include "common.h"
#include "types.h"
#include "diag.h"
#include <SDL2/SDL.h>

// The Blit is also performing conversion from paletted to direct colou
// Doing this only seems to work if the scaling is 1 to 1.
// Hence disabled by default
#ifndef USE_SCALED_BLIT
#define USE_SCALED_BLIT     0
#endif

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
#if USE_SCALED_BLIT
    SDL_Surface *sdl_drw;
#else
    uint32_t *pal;
#endif
	} WIN_PRIV;

static BOOLEAN bInit = FALSE;

static void win_init (void)
    {
    if ( SDL_Init (SDL_INIT_VIDEO) < 0 )
        fatal ("SDL_Init Error: %s", SDL_GetError ());
    bInit = TRUE;
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
#if USE_SCALED_BLIT
	WIN_PRIV *win = (WIN_PRIV *) win_alloc (sizeof(WIN_PRIV), 0);
#else
	WIN_PRIV *win = (WIN_PRIV *) win_alloc (sizeof(WIN_PRIV), width * height);
#endif

	win->width        = width;
	win->height       = height;
	win->width_scale  = width_scale;
	win->height_scale = height_scale;
    win->n_cols       = n_cols;
	win->keypress     = keypress;
	win->keyrelease   = keyrelease;
    diag_message (DIAG_WIN_HW, "%s: (%p)", title, win);
    
    win->sdl_win = SDL_CreateWindow (
        title,
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width * width_scale, height * height_scale,
        SDL_WINDOW_SHOWN
        );
    if ( win->sdl_win == NULL )
        fatal ("SDL error creating window %s: %s", title, SDL_GetError ());
    win->sdl_sfc = SDL_GetWindowSurface (win->sdl_win);
    SDL_SetWindowData (win->sdl_win, "MEMU", win);
    if ( win->sdl_sfc == NULL )
        fatal ("SDL error getting window surface %s: %s", title, SDL_GetError ());
#if USE_SCALED_BLIT
    win->sdl_drw = SDL_CreateRGBSurface (0, width, height, 8, 0, 0, 0, 0);
    if ( win->sdl_drw == NULL )
        fatal ("SDL error creating drawing surface %s: %s", title, SDL_GetError ());
    win->data = win->sdl_drw->pixels;
#else
    if ( win->sdl_sfc->format->palette == NULL )
        win->pal = (uint32_t *) emalloc (n_cols * sizeof (uint32_t));
    else
        win->pal = NULL;
#endif
    for (int i = 0; i < n_cols; ++i)
        win_colour ((WIN *) win, i, &col[i]);
    return (WIN *) win;
    }

void win_delete (WIN *win_pub)
    {
    if ( win_pub == NULL ) return;
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
#if USE_SCALED_BLIT
    SDL_FreeSurface (win->sdl_drw);
    win->data = NULL;
#else
    if ( win->pal != NULL ) free (win->pal);
#endif
    SDL_DestroyWindow (win->sdl_win);
    win_free ((WIN *) win);
    }

void win_colour (WIN *win_pub, int idx, COL *clr)
    {
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
#if USE_SCALED_BLIT
    SDL_Color sdl_clr;
    sdl_clr.r = clr->r;
    sdl_clr.g = clr->g;
    sdl_clr.b = clr->b;
    sdl_clr.a = 0xFF;
    SDL_SetPaletteColors (win->sdl_drw->format->palette, &sdl_clr, idx, 1);
#else
    if ( win->pal == NULL )
        {
        SDL_Color sdl_clr;
        sdl_clr.r = clr->r;
        sdl_clr.g = clr->g;
        sdl_clr.b = clr->b;
        sdl_clr.a = 0xFF;
        SDL_SetPaletteColors (win->sdl_sfc->format->palette, &sdl_clr, idx, 1);
        }
    else
        {
        SDL_PixelFormat *f = win->sdl_sfc->format;
        win->pal[idx] = ((clr->r >> f->Rloss) << f->Rshift) | ((clr->g >> f->Gloss) << f->Gshift)
            | ((clr->b >> f->Bloss) << f->Bshift);
        if ( f->Amask != 0 ) win->pal[idx] |= (0xFF >> f->Aloss) << f->Ashift;
        }
#endif
    }

void win_refresh (WIN *win_pub)
    {
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
#if USE_SCALED_BLIT
    if ( SDL_BlitScaled (win->sdl_drw, NULL, win->sdl_sfc, NULL) )
        fatal ("%s: win %p: %d x %d (%d x %d)\n", SDL_GetError (), win, win->width, win->height, win->width_scale, win->height_scale);
#else
    // uint8_t *pixels = win->sdl_sfc->pixels;
    for (int iRow = 0; iRow < win->height; ++iRow)
        {
        for (int iHScl = 0; iHScl < win->height_scale; ++iHScl)
            {
            uint8_t *data = win->data + iRow * win->width;
            uint8_t *pixels = win->sdl_sfc->pixels + (iRow * win->height_scale + iHScl) * win->sdl_sfc->pitch;
            for (int iCol = 0; iCol < win->width; ++iCol)
                {
                uint32_t clr;
                uint8_t *pclr = (uint8_t *) &clr;
                if ( win->pal == NULL ) clr = *data;
                else clr = win->pal[*data];
                for (int iWScl = 0; iWScl < win->width_scale; ++iWScl)
                    {
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
                    switch (win->sdl_sfc->format->BytesPerPixel)
                        {
                        case 1:
                            *pixels = clr;
                            break;
                        case 2:
                            *((uint16_t *)pixels) = clr;
                            break;
                        case 3:
                            pixels[0] = pclr[0];
                            pixels[1] = pclr[1];
                            pixels[2] = pclr[2];
                            break;
                        case 4:
                            *((uint32_t *)pixels) = clr;
                            break;
                        }
#else
                    switch (win->sdl_sfc->format->BytesPerPixel)
                        {
                        case 1:
                            *pixels = pclr[4];
                            break;
                        case 2:
                            *((uint16_t *)pixels) = *((uint16_t)&pclr[2]);
                            break;
                        case 3:
                            pixels[0] = pclr[1];
                            pixels[1] = pclr[2];
                            pixels[2] = pclr[3];
                            break;
                        case 4:
                            *((uint32_t *)pixels) = clr;
                            break;
                        }
#endif
                    pixels += win->sdl_sfc->format->BytesPerPixel;
                    }
                ++data;
                }
            }
        }
#endif
    SDL_UpdateWindowSurface (win->sdl_win);
    }

typedef struct s_kbd_map
    {
    uint32_t    keycode;
    int         wk;
    } KBD_MAP;

// Entries in the following array must be in increasing order of SDLK_ value
// A bisection searrch is used to locate matches
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
    {SDLK_RGUI, WK_PC_Windows_R}};

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
            case SDL_WINDOWEVENT:
                switch (e.window.event)
                    {
                    case SDL_WINDOWEVENT_CLOSE:
                        terminate("user closed window");
                        break;
                    case SDL_WINDOWEVENT_EXPOSED:
                        win_refresh ((WIN *) SDL_GetWindowData (SDL_GetWindowFromID (e.window.windowID), "MEMU"));
                        break;
                    }
                break;
            case SDL_KEYDOWN:
                if ( e.key.repeat == 0 )
                    {
                    sdl_win = SDL_GetWindowFromID (e.key.windowID);
                    if ( sdl_win == NULL ) break;
                    WIN_PRIV *win = (WIN_PRIV *) SDL_GetWindowData (sdl_win, "MEMU");
                    int wk = sdlkey_wk (e.key.keysym.sym);
                    if (wk > 0) win->keypress ((WIN *) win, wk);
                    }
                break;
            case SDL_KEYUP:
                if ( e.key.repeat == 0 )
                    {
                    sdl_win = SDL_GetWindowFromID (e.key.windowID);
                    if ( sdl_win == NULL ) break;
                    WIN_PRIV *win = (WIN_PRIV *) SDL_GetWindowData (sdl_win, "MEMU");
                    int wk = sdlkey_wk (e.key.keysym.sym);
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

const SDL_DisplayMode **SDL_GetFullscreenDisplayModes(int displayID, int *count);

void win_max_size (const char *display, int *pWth, int *pHgt)
    {
    if ( ! bInit ) win_init ();
    SDL_Rect rect;
    SDL_GetDisplayUsableBounds (0, &rect);
    *pWth = rect.w;
    *pHgt = rect.h;
    }

extern void kbd_chk_leds (int *mods)
    {
    }
