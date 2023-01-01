/*  vid_pico - Raspberry Pi Pico version of VDP emulation */

#include <string.h>
#include "pico.h"
#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "display_pico.h"
#include "diag.h"
#include "vid.h"

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define VWIDTH      320
#define VHEIGHT     240
#define GWIDTH      256
#define GHEIGHT     192
#define TWIDTH      240
#define GTOP        ( ( VHEIGHT - GHEIGHT ) / 2 )
#define GBOT        ( GTOP + GHEIGHT )
#define GLEFT       ( ( VWIDTH - GWIDTH ) / 2 )
#define GRIGHT      ( VWIDTH - GWIDTH - GLEFT )
#define TLEFT       ( ( VWIDTH - TWIDTH ) / 2 )
#define TRIGHT      ( VWIDTH - TWIDTH - TLEFT )
#define TCOL        40
#define TROW        24
#define TWTH        6
#define THGT        8
#define GCOL        32
#define GROW        24
#define GWTH        8
#define GHGT        8
#define NSPRITE     32
#define VRAM_SIZE   0x4000
#define VMASK       ( VRAM_SIZE - 1 )

#ifdef DBLPIX
#define CLRMULT(x)  (( (x) << 16 ) | (x))
#define PIX_T       uint32_t
#else
#define CLRMULT(x)  x
#define PIX_T       uint16_t
#endif

static PIX_T colours[16] =
    {
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0,   0,   0 ) ),	/* transparent */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(    0,   0,   0 ) ),	/* black */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(   32, 192,  32 ) ),	/* medium green */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(   96, 224,  96 ) ),	/* light green */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(   32,  32, 224 ) ),	/* dark blue */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(   64,  96, 224 ) ),	/* light blue */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  160,  32,  32 ) ),	/* dark red */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(   64, 192, 224 ) ),	/* cyan */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  224,  32,  32 ) ),	/* medium red */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  224,  96,  96 ) ),	/* light red */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  192, 192,  32 ) ),	/* dark yellow */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  192, 192, 128 ) ),	/* light yellow */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(   32, 128,  32 ) ),	/* dark green */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  192,  64, 160 ) ),	/* magenta */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  160, 160, 160 ) ),	/* grey */
	CLRMULT( PICO_SCANVIDEO_PIXEL_FROM_RGB8(  224, 224, 224 ) )	    /* white */
    };

#ifdef DEBUG
extern uint8_t vram[VRAM_SIZE];
extern uint8_t regs[8];
#else
static uint8_t vram[VRAM_SIZE];
static uint8_t regs[8];
#endif
static uint16_t *pmode = (uint16_t *) regs;
static uint8_t vstat;

#define MODE_M1 0x1000
#define MODE_M2 0x0800
#define MODE_M3 0x0002
#define MODE_BL 0x4000
#define MODE_MX ( MODE_M1 | MODE_M2 | MODE_M3 | MODE_BL )
#define MODE_G1 MODE_BL
#define MODE_G2 ( MODE_M3 | MODE_BL )
#define MODE_MC ( MODE_M2 | MODE_BL )
#define MODE_TX ( MODE_M1 | MODE_BL )

#define VS_FRAME 0x80
#define VS_FIFTH 0x40
#define VS_COINC 0x20
#define VS_5MASK 0x1F

void __time_critical_func(vid_render_sprites) (PIX_T *pix, int iScan)
    {
    uint8_t hit[VWIDTH];
    memset (hit, 0, sizeof (hit));
    bool bSize = ( regs[1] & 0x02 ) > 0;
    bool bMag = ( regs[1] & 0x01 ) > 0;
    int iSize = 8;
	int nPix = 8;
	int nSkip = 1;
    if ( bSize )
		{
		iSize *= 2;
		nPix *= 2;
		}
    if ( bMag )
		{
		iSize *= 2;
		nSkip = 2;
		}
    int iAttr = ( regs[5] & 0x7F ) << 7;
    int iPatt = ( regs[6] & 0x07 ) << 11;
    int nSpr = 0;
    pix += GLEFT;
    for (int i = 0; i < NSPRITE; ++i )
        {
        int iY = (vram[iAttr] + 0x20u) & 0xFF;
		if ( iY == 0xF0 ) break;
		iY -= 32;
        if ( ( iScan >= iY ) && ( iScan < iY + iSize ) )
            {
            if ( ++nSpr > 4 )
                {
                if ( ! ( vstat & VS_FIFTH ) )
					vstat = ( vstat & ( VS_FRAME | VS_COINC ) ) | VS_FIFTH | i;
                break;
                }
			int iLine = iScan - iY;
			if ( bMag ) iLine /= 2;
            ++iAttr;
            int iX = vram[iAttr];
            ++iAttr;
            int iP = (iPatt + GHGT * vram[iAttr] + iLine) & VMASK;
            ++iAttr;
            PIX_T clr = colours[vram[iAttr] & 0x0F];
            if ( vram[iAttr] & 0x80 ) iX -= 32;
            ++iAttr;
			int iBits = vram[iP] << 8;
			if ( bSize ) iBits |= vram[iP + 2 * GHGT];
            if ( ( clr != 0 ) && ( iBits != 0 ) )
                {
                for ( int j = 0; ( j < nPix ) && ( iX < GWIDTH ); ++j )
                    {
                    if ( ( iX >= 0 ) && ( iBits & 0x8000 ) )
                        {
                        pix[iX] = clr;
                        if ( hit[iX] ) vstat |= VS_COINC;
                        hit[iX] = 1;
                        ++iX;
                        if (( bMag ) && ( iX < GWIDTH ))
                            {
                            pix[iX] = clr;
                            if ( hit[iX] ) vstat |= VS_COINC;
                            hit[iX] = 1;
                            ++iX;
                            }
                        }
                    else
                        {
                        iX += nSkip;
                        }
                    iBits <<= 1;
                    }
                }
            }
        else
            {
            iAttr += 4;
            }
        }
    }

void __time_critical_func(vid_render_multicolour) (PIX_T *pix, int iScan)
    {
    PIX_T bd = colours[regs[7] & 0x0F];
    int iRow = iScan / GHGT;
    iScan -= GHGT * iRow;
    int iName = (((regs[2] & 0x0F) << 10 ) + GCOL * iRow) & VMASK;
    int iPatt = ((regs[4] & 0x07) << 11) + ((iRow & 0x03) << 1);
    if ( iScan >= 4 ) ++iPatt;
    for ( int i = 0; i < GLEFT; ++i )
        {
        *pix = bd;
        ++pix;
        }
    for ( int i = 0; i < GCOL; ++i )
        {
        int iP = vram[(iPatt + GHGT * vram[iName]) & VMASK];
        PIX_T clr = colours[iP >> 4];
        *pix = clr;
        ++pix;
        *pix = clr;
        ++pix;
        *pix = clr;
        ++pix;
        *pix = clr;
        ++pix;
        clr = colours[iP & 0x0F];
        *pix = clr;
        ++pix;
        *pix = clr;
        ++pix;
        *pix = clr;
        ++pix;
        *pix = clr;
        ++pix;
        iName = ( iName + 1 ) & VMASK;
        }
    for ( int i = 0; i < GRIGHT; ++i )
        {
        *pix = bd;
        ++pix;
        }
	}

void __time_critical_func(vid_render_graphics_2) (PIX_T *pix, int iScan)
    {
    PIX_T bd = colours[regs[7] & 0x0F];
    int iRow = iScan / GHGT;
    iScan -= GHGT * iRow;
    int iName = (((regs[2] & 0x0F) << 10 ) + GCOL * iRow) & VMASK;
    int iPatt = (regs[4] & 0x04) << 11;
    int iClrs = (regs[3] & 0x80) << 6;
    int iPart = iRow / 8;
    if ( iPart == 1 )
        {
        if ( regs[4] & 0x01 ) iPatt += 0x0800;
        if ( regs[3] & 0x20 ) iClrs += 0x0800;
        }
    else if ( iPart == 2 )
        {
        if ( regs[4] & 0x02 ) iPatt += 0x1000;
        if ( regs[3] & 0x40 ) iClrs += 0x1000;
        }
    for ( int i = 0; i < GLEFT; ++i )
        {
        *pix = bd;
        ++pix;
        }
    for ( int i = 0; i < GCOL; ++i )
        {
        int iN = GHGT * vram[iName] + iScan;
        int iC = iClrs + iN;
        PIX_T fg = colours[vram[iC]>>4];
        PIX_T bg = colours[vram[iC]&0x0F];
        int iBits = vram[iPatt + iN];
        if ( iBits & 0x80 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x40 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x20 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x10 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x08 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x04 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x02 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x01 ) *pix = fg;
        else *pix = bg;
        ++pix;
        iName = ( iName + 1 ) & VMASK;
        }
    for ( int i = 0; i < GRIGHT; ++i )
        {
        *pix = bd;
        ++pix;
        }
    }

void __time_critical_func(vid_render_graphics_1) (PIX_T *pix, int iScan)
    {
    PIX_T bd = colours[regs[7] & 0x0F];
    int iRow = iScan / GHGT;
    iScan -= GHGT * iRow;
    int iName = (((regs[2] & 0x0F) << 10 ) + GCOL * iRow) & VMASK;
    int iPatt = (regs[4] & 0x07) << 11;
    int iClrs = regs[3] << 6;
    for ( int i = 0; i < GLEFT; ++i )
        {
        *pix = bd;
        ++pix;
        }
    for ( int i = 0; i < GCOL; ++i )
        {
        int iN = vram[iName];
        int iC = iClrs + (iN >> 3);
        PIX_T fg = colours[vram[iC]>>4];
        PIX_T bg = colours[vram[iC]&0x0F];
        int iBits = vram[( iPatt + GHGT * iN + iScan ) & VMASK];
        if ( iBits & 0x80 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x40 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x20 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x10 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x08 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x04 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x02 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x01 ) *pix = fg;
        else *pix = bg;
        ++pix;
        iName = ( iName + 1 ) & VMASK;
        }
    for ( int i = 0; i < GRIGHT; ++i )
        {
        *pix = bd;
        ++pix;
        }
    }

void __time_critical_func(vid_render_text) (PIX_T *pix, int iScan)
    {
    PIX_T bg = colours[regs[7] & 0x0F];
    PIX_T fg = colours[regs[7] >> 4];
    int iRow = iScan / THGT;
    iScan -= THGT * iRow;
    int iName = ((regs[2] << 10 ) + TCOL * iRow) & VMASK;
    int iPatt = regs[4] << 11;
    for ( int i = 0; i < TLEFT; ++i )
        {
        *pix = bg;
        ++pix;
        }
    for ( int i = 0; i < TCOL; ++i )
        {
        uint8_t iBits = vram[(iPatt + THGT * vram[iName] + iScan) & VMASK];
        if ( iBits & 0x80 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x40 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x20 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x10 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x08 ) *pix = fg;
        else *pix = bg;
        ++pix;
        if ( iBits & 0x04 ) *pix = fg;
        else *pix = bg;
        ++pix;
        iName = ( iName + 1 ) & VMASK;
        }
    for ( int i = 0; i < TRIGHT; ++i )
        {
        *pix = bg;
        ++pix;
        }
    }

void __time_critical_func(vid_render_blank) (PIX_T *pix)
    {
    PIX_T bg = colours[regs[7] & 0x0F];
    for ( int i = 0; i < VWIDTH; ++i )
        {
        *pix = bg;
        ++pix;
        }
    }

void __time_critical_func(vid_render_loop) (void)
    {
//    static int iLast = 0;
#ifdef TEST
    for ( int i = 0; i < 16; ++i )
        {
        PRINTF ("colours[%d] = 0x%08X\n", i, colours[i]);
        }
#endif
    while ( dmode == dispVDP )
        {
        struct scanvideo_scanline_buffer *buffer = scanvideo_begin_scanline_generation (true);
        int iScan = scanvideo_scanline_number (buffer->scanline_id);
//        if ( ( iScan > 0 ) && ( iScan < iLast ) )
//            PRINTF ("Scanline reversed %d < %d\n", iScan, iLast);
//        iLast = iScan;
        PIX_T *pix = (PIX_T *) (buffer->data + 1);
        if ( ( iScan >= GTOP ) && ( iScan < GBOT ) )
            {
            iScan -= GTOP;
            switch ( (*pmode) & MODE_MX )
                {
                case MODE_TX:
                    vid_render_text ((PIX_T *) pix, iScan);
                    break;
                case MODE_G1:
                    vid_render_graphics_1 ((PIX_T *) pix, iScan);
                    vid_render_sprites ((PIX_T *) pix, iScan);
                    break;
                case MODE_G2:
                    vid_render_graphics_2 ((PIX_T *) pix, iScan);
                    vid_render_sprites ((PIX_T *) pix, iScan);
                    break;
                case MODE_MC:
                    vid_render_multicolour ((PIX_T *) pix, iScan);
                    vid_render_sprites ((PIX_T *) pix, iScan);
                    break;
                default:
                    vid_render_blank ((PIX_T *) pix);
                    break;
                }
            }
        else
            {
            vid_render_blank ((PIX_T *) pix);
            }
        pix += VWIDTH;
#ifdef DBLPIX
        *pix = COMPOSABLE_EOL_ALIGN << 16;
        pix = (PIX_T *) buffer->data;
        pix[0] = COMPOSABLE_RAW_RUN | ( pix[1] << 16 );
        pix[1] = ( 2 * VWIDTH - 2 ) | ( pix[1] & 0xFFFF0000 );
        buffer->data_used = ( 2 * VWIDTH + 4 ) / 2;
#else
        *pix = 0;
        ++pix;
        *pix = COMPOSABLE_EOL_ALIGN;
        pix = (PIX_T *) buffer->data;
        pix[0] = COMPOSABLE_RAW_RUN;
        pix[1] = pix[2];
        pix[2] = VWIDTH - 2;
        buffer->data_used = ( VWIDTH + 4 ) / 2;
#endif
        scanvideo_end_scanline_generation (buffer);
        if ( iScan == GBOT ) bFrameInt = true;
        }
    }

void vdp_video (void)
    {
#ifndef DBLPIX
    PRINTF ("scanvideo_setup (320x320)\n");
    scanvideo_setup (&vga_mode_320x240_60);
    PRINTF ("scanvideo_timing_enable (true)\n");
    scanvideo_timing_enable (true);
#endif
    vid_render_loop ();
#ifndef DBLPIX
    PRINTF ("scanvideo_timing_enable (false)\n");
    scanvideo_timing_enable(false);
#endif
    }

static word vid_addr;
static BOOLEAN vid_read_mode;
static BOOLEAN vid_latched = FALSE;
static byte vid_latch = 0;

void vid_out1 (byte val, unsigned long long elapsed)
	{
    vram[vid_addr] = val;
    // if ( diag_flags[DIAG_VID_DATA] )
    // 	vid_data_xfer("output", vid_addr, val);
    vid_addr = ( (vid_addr+1) & VMASK );
    vid_latched = FALSE; /* According to http://bifi.msxnet.org/msxnet/tech/tms9918a.txt, section 2.3 */
	}

void vid_out2(byte val)
	{
	if ( !vid_latched )
		/* First write to port 2, record the value */
		{
		vid_latch = val;
		vid_addr = ( (vid_addr&0xff00)|val );
			/* Son Of Pete relies on the low part of the
			   address being updated during the first write.
			   HexTrain also does partial address updates. */
		vid_latched = TRUE;
		}
	else
		/* Second write to port 2, act */
		{
		switch ( val & 0xc0 )
			{
			case 0x00:
				/* Set up for reading from VRAM */
				vid_addr = ( ((val&0x3f)<<8)|vid_latch );
				vid_read_mode = TRUE;
				break;
			case 0x40:
				/* Set up for writing to VRAM */
				vid_addr = ( ((val&0x3f)<<8)|vid_latch );
				vid_read_mode = FALSE;
				break;
			case 0x80:
				/* Write VDP register.
				   Various bits must be zero. */
				val &= 7;
				regs[val] = vid_latch;
				break;
			case 0xc0:
				break;
			}
		vid_latched = FALSE;
		}
	}

byte vid_in1 (unsigned long long elapsed)
	{
	byte val;
	if ( ! vid_read_mode )
		/* VDEB.COM can do this, so don't consider it fatal */
		{
		vid_read_mode = TRUE; /* Prevent lots of warnings */
		val = 0xff; /* There was no prefetch */
		vid_latched = FALSE; /* For symmetry with vid_out2. */
		/* Note that we don't do the timing check that perhaps we should. */
		}
	else
		{
		val = vram[vid_addr];
		vid_addr = ( (vid_addr+1) & VMASK );
		vid_latched = FALSE; /* For symmetry with vid_out2.
					Not seen it explicitly documented.
					Downstream Danger relies on this. */
		}
	return val;
	}

byte vid_in2 (void)
	{
	// diag_message (DIAG_ALWAYS,"In  0x02");
	byte value = vstat;
	vstat = 0; /* Clear F, C, 5S, FSN, for subsequent reads */
    vid_latched = FALSE;
	return value;
	}

void vid_clear_int (void)
    {
    }

void vid_init(int emu, int width_scale, int height_scale)
    {
    display_vdp ();
    }

void vid_reset (void)
    {
    }

void vid_term(void)
    {
    }

void vid_max_scale (int *pxscl, int *pyscl)
    {
    *pxscl = 2;
    *pyscl = 2;
    }
