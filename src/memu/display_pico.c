/* display_pico.c - Display Control */

#include <stdio.h>
#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "display_pico.h"
#include "80col_pico.h"
void vdp_video (void);

volatile DisplayMode dmode = dispNone;
volatile EightyColumn *p80column;
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
    printf ("scanvideo_setup (640x320)\n");
    scanvideo_setup(&vga_mode_640x240_60);
    printf ("scanvideo_timing_enable (true)\n");
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
                printf ("Start VDP mode\n");
                vdp_video ();
                break;
            case disp80col:
                printf ("Start 80 column mode\n");
                ecol_video ();
                break;
            }
        printf ("Switch display mode\n");
        }
    }

void display_vdp (void)
    {
    printf ("Display VDP screen\n");
    dmode = dispVDP;
    }

void display_80column (EightyColumn *p80c)
    {
    printf ("Display 80 column screen\n");
    p80column = p80c;
    dmode = disp80col;
    }

void display_save (DisplayState *pst)
    {
    pst->dm = dmode;
    pst->pec = (EightyColumn *) p80column;
    }

void display_load (const DisplayState *pst)
    {
    if ( pst->dm == dispVDP ) display_vdp ();
    else if ( pst->dm == disp80col ) display_80column (pst->pec);
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
