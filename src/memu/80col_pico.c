/* 80col_pico.c - 80 column display basics */

#include <stdio.h>
#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "display_pico.h"
#include "80col_pico.h"
#include "monprom.h"

#define NCLR80C 8
#define WTH80C ( EC_COLS * GLYPH_WIDTH )
#define HGT80C ( EC_ROWS * GLYPH_HEIGHT )

uint16_t clr80c[NCLR80C] = {
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0,   0,   0 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  255,   0,   0 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0, 255,   0 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  255, 255,   0 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0,   0, 255 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  255,   0, 255 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0, 255, 255 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(  255, 255, 255 )
    };

uint16_t mono80c[] = {
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0,   0,   0 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0,  64,   0 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0, 128,   0 ),
    PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0, 255,   0 ),
    };

extern volatile EightyColumn *p80column;

void __time_critical_func(ecol_render_loop) (void)
    {
    printf ("In render loop: mode = 0x%02X\n", p80column->mode);
    while ( dmode == disp80col )
        {
        EightyColumn *p80c = (EightyColumn *) p80column;
        bool bMono = ( p80c->mode == 0x02 );
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        int iScan = scanvideo_scanline_number (buffer->scanline_id);
        int iFrame = scanvideo_frame_number (buffer->scanline_id);
        int iRow = iScan / GLYPH_HEIGHT;
        iScan -= GLYPH_HEIGHT * iRow;
        int iAddr = EC_COLS * iRow + ( p80c->reg[12] << 8 ) + p80c->reg[13];
        int iCsrAddr = ( p80c->reg[14] << 8 ) + p80c->reg[15];
        bool bBlink = ( iFrame & 0x20 );
        bool bCsrBlk = false;
        if ( ( iScan >= ( p80c->reg[10] & 0x1F ) ) && ( iScan <= ( p80c->reg[11] & 0x1F ) ) )
            {
            switch ( p80c->reg[10] & 0x60 )
                {
                case 0x00: bCsrBlk = true; break;
                case 0x20: bCsrBlk = false; break;
                case 0x40: bCsrBlk = iFrame & 0x10;
                case 0x60: bCsrBlk = iFrame & 0x20;
                }
            }
        uint16_t *pix = (uint16_t *) buffer->data;
        ++pix;
        for (int iCol = 0; iCol < EC_COLS; ++iCol)
            {
            iAddr &= ( NRAM80C - 1 );
            uint16_t fg;
            uint16_t bg;
            uint8_t attr = p80c->ram[iAddr].at;
            if ( bMono )
                {
                if ( attr & 0x04 ) fg = mono80c[3];
                else fg = mono80c[2];
                if ( attr & 0x20 ) bg = mono80c[1];
                else bg = mono80c[0];
                if ( ( attr & 0x01 ) || ( ( attr & 0x01 ) && ( iRow == ( GLYPH_HEIGHT - 1 ) ) ) )
                    {
                    uint16_t tmp = fg;
                    fg = bg;
                    bg = tmp;
                    }
                }
            else
                {
                fg = clr80c[attr & 0x07];
                bg = clr80c[(attr >> 3) & 0x07];
                }
            if ( ( bBlink && ( attr & 0x40 ) )
                || ( bCsrBlk && ( iAddr == iCsrAddr ) ) )
                {
                uint16_t tmp = fg;
                fg = bg;
                bg = tmp;
                }
            uint8_t bits;
            if ( attr & 0x80 ) bits = mon_graphic_prom[p80c->ram[iAddr].ch][iScan];
            else bits = mon_alpha_prom[p80c->ram[iAddr].ch][iScan];
            ++pix;
            if ( bits & 0x80 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x40 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x20 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x10 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x08 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x04 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x02 ) *pix = fg;
            else *pix = bg;
            ++pix;
            if ( bits & 0x01 ) *pix = fg;
            else *pix = bg;
            ++iAddr;
            }
        *pix = 0;
        ++pix;
        *pix = COMPOSABLE_EOL_ALIGN;
        pix = (uint16_t *) buffer->data;
        pix[0] = COMPOSABLE_RAW_RUN;
        pix[1] = pix[2];
        pix[2] = WTH80C - 2;
        buffer->data_used = ( WTH80C + 4 ) / 2;
        iScan = scanvideo_scanline_number (buffer->scanline_id);
        scanvideo_end_scanline_generation (buffer);
        if ( iScan == HGT80C - 1 ) bFrameInt = true;
        }
    printf ("Exit render loop: mode = 0x%02X\n", p80column->mode);
    }

#ifndef DBLPIX
const scanvideo_mode_t vga_mode_640x240_60 =
    {
    .default_timing = &vga_timing_640x480_60_default,
    .pio_program = &video_24mhz_composable,
    .width = WTH80C,
    .height = HGT80C,
    .xscale = 1,
    .yscale = 2,
    };
#endif

void ecol_video (void)
    {
#ifndef DBLPIX
    printf ("scanvideo_setup (640x320)\n");
    scanvideo_setup(&vga_mode_640x240_60);
    printf ("scanvideo_timing_enable (true)\n");
    scanvideo_timing_enable(true);
#endif
    ecol_render_loop ();
#ifndef DBLPIX
    printf ("scanvideo_timing_enable (false)\n");
    scanvideo_timing_enable(false);
#endif
    }
