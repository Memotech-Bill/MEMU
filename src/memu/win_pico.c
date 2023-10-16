/* win_pico.c - Display Control */

#include <stdio.h>
#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "display_pico.h"
#include "win.h"

#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

volatile DisplayMode dmode = dispNone;
volatile TXTBUF *p80column;
volatile bool bFrameInt;

void __time_critical_func(null_render_loop) (void)
    {
    while ( dmode == dispNone )
        {
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        uint16_t *pix = (uint16_t *) buffer->data;
        *pix = COMPOSABLE_COLOR_RUN;
        ++pix;
        *pix = 0x7C00;
        ++pix;
        *pix = 640 - 3;
        ++pix;
        *pix = COMPOSABLE_RAW_1P;
        ++pix;
        *pix = 0;
        ++pix;
        *pix = COMPOSABLE_EOL_ALIGN;
        buffer->data_used = 3;
        int iScan = scanvideo_scanline_number (buffer->scanline_id);
        scanvideo_end_scanline_generation (buffer);
        if ( iScan == 240 - 1 ) bFrameInt = true;
        }
    }

#ifdef DBLPIX
const scanvideo_mode_t vga_mode_640x240_60 =
    {
    .default_timing = &vga_timing_640x480_60_default,
    .pio_program = &video_24mhz_composable,
    .width = 640,
    .height = 240,
    .xscale = 1,
    .yscale = 2,
    };
#endif

void display_loop (void)
    {
#ifdef DBLPIX
    PRINTF ("scanvideo_setup (640x320)\n");
    scanvideo_setup(&vga_mode_640x240_60);
    PRINTF ("scanvideo_timing_enable (true)\n");
    scanvideo_timing_enable(true);
#endif
    while (true)
        {
        switch (dmode)
            {
            case dispNone:
                null_render_loop ();
                break;
            case dispVDP:
                PRINTF ("Start VDP mode\n");
                vdp_video ();
                break;
            case disp80col:
                PRINTF ("Start 80 column mode\n");
                twin_video ();
                break;
            }
        PRINTF ("Switch display mode\n");
        }
    }

void display_wait_for_frame (void)
    {
    static int n = 31;
    n = ( ++n ) & 63;
    gpio_put(PICO_DEFAULT_LED_PIN, n >> 5);
    while ( ! bFrameInt )
        {
        tight_loop_contents();
        }
    bFrameInt = false;
    }

void win_max_size (const char *display, int *pWth, int *pHgt)
    {
    *pWth = 640;
    *pHgt = 480;
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
    WIN *win = win_alloc (sizeof (WIN), 0);
    win->width = width;
    win->height = height;
    win->width_scale = width_scale;
    win->height_scale = height_scale;
    win->keypress = keypress;
    win->keyrelease = keyrelease;
    return win;
    }

void win_delete (WIN *win)
    {
    win_free (win);
    }

BOOLEAN win_active (WIN *win)
    {
    return ( win == active_win );
    }

void display_vdp (void)
        {
        PRINTF ("Display VDP screen\n");
        dmode = dispVDP;
        }
    
void display_tbuf (TXTBUF *tbuf)
        {
        PRINTF ("Display 80 column screen\n");
        p80column = tbuf;
        dmode = disp80col;
        }

void win_show (WIN *win)
    {
    active_win = win;
    if ( win->tbuf != NULL ) display_tbuf (win->tbuf);
    else display_vdp ();
    }

void win_refresh (WIN *win)
    {
    }

void win_term (void)
    {
    }
