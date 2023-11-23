/*

The Propeller emulation needs it own copy of the VDP emulation, as it
will not be updated when the Propeller is in Native mode.

Also needed for MFX emulation, therefore separated out.

What follows is a cut-down version of vid.c with sprite coincidence
checks and timing checks removed.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "types.h"
#include "diag.h"
#include "common.h"
#include "vdp.h"
#include "diag.h"

/*...vZ80\46\h:0:*/
/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...vvid\46\h:0:*/
/*...vkbd\46\h:0:*/
/*...vmon\46\h:0:*/
/*...e*/

static byte vdp_regs_zeros[8] = { 0xfc,0x04,0xf0,0x00,0xf8,0x80,0xf8,0x00 };

/*...svgavdp_reset:0:*/
void vdp_reset (VDP *vdp)
	{
	vdp->latched = FALSE;
	vdp->addr = 0x000;
	}
/*...e*/

/*...svga_out1 \45\ data write:0:*/
void vdp_out1 (VDP *vdp, byte val)
	{
	vdp->read_mode = FALSE; /* Prevent lots of warnings */
	((byte *)vdp->ram)[vdp->addr] = val;
    // if ( ! vdp->changed ) diag_message (DIAG_ALWAYS, "VDP Write: addr = 0x%04X, val = 0x%02X", vdp->addr, val);
    vdp->changed = TRUE;
	vdp->addr = ( (vdp->addr+1) & (VDP_MEMORY_SIZE-1) );
	vdp->latched = FALSE;
	}
/*...e*/
/*...svga_out2 \45\ latch value\44\ then act:0:*/
void vdp_out2 (VDP *vdp, byte val)
	{
	if ( !vdp->latched )
		/* First write to port 2, record the value */
		{
		vdp->latch = val;
		vdp->addr = ( (vdp->addr&0xff00)|val );
			/* Son Of Pete relies on the low part of the
			   address being updated during the first write.
			   HexTrain also does partial address updates. */
		vdp->latched = TRUE;
		}
	else
		/* Second write to port 2, act */
		{
		switch ( val & 0xc0 )
			{
			case 0x00:
				/* Set up for reading from VRAM */
				vdp->addr = ( ((val&0x3f)<<8)|vdp->latch );
				vdp->read_mode = TRUE;
				break;
			case 0x40:
				/* Set up for writing to VRAM */
				vdp->addr = ( ((val&0x3f)<<8)|vdp->latch );
				vdp->read_mode = FALSE;
				break;
			case 0x80:
				/* Write VDP register.
				   Various bits must be zero. */
				val &= 7;
				vdp->latch &= ~vdp_regs_zeros[val];
				vdp->regs[val] = vdp->latch;
                // if ( ! vdp->changed ) diag_message (DIAG_ALWAYS, "VDP register %d = 0x%02X", val, vdp->latch);
                vdp->changed = TRUE;
				break;
			case 0xc0:
				break;
			}
		vdp->latched = FALSE;
		}
	}
/*...e*/

/*...svgavdp_refresh_blank:0:*/
static void vdp_refresh_blank (VDP *vdp)
	{
	memset(vdp->pix, (vdp->regs[7]&0x0f), VDP_WIDTH*VDP_HEIGHT);
	}
/*...e*/
/*...svgavdp_refresh_sprites:0:*/
/*...svgavdp_refresh_sprites_line_check:0:*/
static BOOLEAN vdp_refresh_sprites_line_check (VDP *vdp, int scn_y, int sprite)
	{
	if ( scn_y < 0 || scn_y >= 192 )
		return FALSE;
	if ( ++vdp->spr_lines[scn_y] <= 4 )
		return TRUE;
	return FALSE;
	}
/*...e*/
/*...svgavdp_refresh_sprites_plot:0:*/
static void vdp_refresh_sprites_plot (VDP *vdp, int scn_x, int scn_y, int spr_col)
	{
	if ( scn_x >= 0 && scn_x < 256 )
		{
		if ( spr_col != 0 )
			vdp->pix[(VBORDER+scn_y)*VDP_WIDTH+(HBORDER256+scn_x)] = spr_col;
		}
	}
/*...e*/

static void vdp_refresh_sprites (VDP *vdp)
	{
	byte size = ( (vdp->regs[1]&0x02) != 0 );
	byte mag  = ( (vdp->regs[1]&0x01) != 0 );
	word sprgen = ((word)(vdp->regs[6]&0x07)<<11);
	word spratt = ((word)(vdp->regs[5]&0x7f)<<7);
	int sprite, x, y, scn_x;
	byte spr_pat_mask = size ? 0xfc : 0xff;
	for ( sprite = 0; sprite < 32; sprite++ )
		{
		int spr_y     = (int) (unsigned) ((byte *)vdp->ram)[spratt++]; /* 0-255, partially signed */
		int spr_x     = (int) (unsigned) ((byte *)vdp->ram)[spratt++]; /* 0-255 */
		byte spr_pat  = ((byte *)vdp->ram)[spratt++] & spr_pat_mask;
		word spr_addr = sprgen+((word)spr_pat<<3);
		byte spr_flag = ((byte *)vdp->ram)[spratt++];
		byte spr_col  = (spr_flag & 0x0f);
		if ( spr_y == 0xd0 )
			break;
		if ( spr_y <= 192 )
			++spr_y;
		else
			spr_y -= 255;
		if ( spr_flag & 0x80 )
			spr_x -= 32;
		if ( size )
			for ( y = 0; y < 16; y++ )
				{
				word bits = ((word)((byte *)vdp->ram)[spr_addr]<<8)|((word)((byte *)vdp->ram)[spr_addr+16]);
				spr_addr++;
				if ( mag )
/*...sMAG\61\1\44\ SIZE\61\1:40:*/
{
int scn_y = spr_y+y*2;
if ( vdp_refresh_sprites_line_check (vdp, scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x+=2 )
		if ( bits & (0x8000>>x) )
			{
			vdp_refresh_sprites_plot (vdp, scn_x  , scn_y, spr_col);
			vdp_refresh_sprites_plot (vdp, scn_x+1, scn_y, spr_col);
			}
++scn_y;
if ( vdp_refresh_sprites_line_check (vdp, scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x+=2 )
		if ( bits & (0x8000>>x) )
			{
			vdp_refresh_sprites_plot (vdp, scn_x  , scn_y, spr_col);
			vdp_refresh_sprites_plot (vdp, scn_x+1, scn_y, spr_col);
			}
}
/*...e*/
				else
/*...sMAG\61\0\44\ SIZE\61\1:40:*/
{
int scn_y = spr_y+y;
if ( vdp_refresh_sprites_line_check (vdp, scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x++ )
		if ( bits & (0x8000>>x) )
			vdp_refresh_sprites_plot (vdp, scn_x, scn_y, spr_col);
}
/*...e*/
				}
		else
			for ( y = 0; y < 8; y++ )
				{
				byte bits = ((byte *)vdp->ram)[spr_addr++];
				if ( mag )
/*...sMAG\61\1\44\ SIZE\61\0:40:*/
{
int scn_y = spr_y+y*2;
if ( vdp_refresh_sprites_line_check (vdp, scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x+=2 )
		if ( bits & (0x80>>x) )
			{
			vdp_refresh_sprites_plot (vdp, scn_x  , scn_y, spr_col);
			vdp_refresh_sprites_plot (vdp, scn_x+1, scn_y, spr_col);
			}
++scn_y;
if ( vdp_refresh_sprites_line_check (vdp, scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x+=2 )
		if ( bits & (0x80>>x) )
			{
			vdp_refresh_sprites_plot (vdp, scn_x  , scn_y, spr_col);
			vdp_refresh_sprites_plot (vdp, scn_x+1, scn_y, spr_col);
			}
}
/*...e*/
				else
/*...sMAG\61\0\44\ SIZE\61\0:40:*/
{
int scn_y = spr_y+y;
if ( vdp_refresh_sprites_line_check (vdp, scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x++ )
		if ( bits & (0x80>>x) )
			vdp_refresh_sprites_plot (vdp, scn_x, scn_y, spr_col);
}
/*...e*/
				}
		}
	}
/*...e*/
/*...svgavdp_refresh_graphics1:0:*/
static void vdp_refresh_graphics1_pat (VDP *vdp, 
	byte pat,
	word patgen,
	word patcol,
	byte *d
	)
	{
	word genptr = patgen + pat*8;
	word colptr = patcol + pat/8;
	byte col = ((byte *)vdp->ram)[colptr];
	byte fg = (col>>4);
	byte bg = (col&15);
	int x, y;
	if ( fg == 0 ) fg = (vdp->regs[7]&0x0f);
	if ( bg == 0 ) bg = (vdp->regs[7]&0x0f);
	for ( y = 0; y < 8; y++ )
		{
		byte gen = ((byte *)vdp->ram)[genptr++];
		for ( x = 0; x < 8; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -8 + VDP_WIDTH ); 
		}
	}

static void vdp_refresh_graphics1_third (VDP *vdp, 
	word patnam,
	word patgen,
	word patcol,
	byte *d
	)
	{
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		for ( x = 0; x < 32; x++ )
			{
			byte pat = ((byte *)vdp->ram)[patnam++];
			vdp_refresh_graphics1_pat (vdp, pat, patgen, patcol, d);
			d += 8;
			}
		d += ( -32*8 + VDP_WIDTH*8 );
		}
	}

static void vdp_refresh_graphics1 (VDP *vdp)
	{
	word patnam = ( ((word)(vdp->regs[2]&0x0f)) << 10 );
	word patgen = ( ((word)(vdp->regs[4]&0x07)) << 11 );
	word patcol = ( ((word)(vdp->regs[3]&0xff)) <<  6 );
	byte *d = vdp->pix + VDP_WIDTH*VBORDER + HBORDER256;

	vdp_refresh_graphics1_third (vdp, patnam       , patgen, patcol, d           );
	vdp_refresh_graphics1_third (vdp, patnam+0x0100, patgen, patcol, d+VDP_WIDTH* 8*8);
	vdp_refresh_graphics1_third (vdp, patnam+0x0200, patgen, patcol, d+VDP_WIDTH*16*8);

	vdp_refresh_sprites (vdp);
	}
/*...e*/
/*...svgavdp_refresh_graphics2:0:*/
static void vdp_refresh_graphics2_pat (VDP *vdp, 
	byte pat,
	word patgen,
	word patcol,
	byte *d
	)
	{
	word genptr = patgen + pat*8;
	word colptr = patcol + pat*8;
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		byte gen = ((byte *)vdp->ram)[genptr++];
		byte col = ((byte *)vdp->ram)[colptr++];
		byte fg = (col>>4);
		byte bg = (col&15);
		if ( fg == 0 ) fg = (vdp->regs[7]&0x0f);
		if ( bg == 0 ) bg = (vdp->regs[7]&0x0f);
		for ( x = 0; x < 8; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -8 + VDP_WIDTH ); 
		}
	}

static void vdp_refresh_graphics2_third (VDP *vdp, 
	word patnam,
	word patgen,
	word patcol,
	byte *d
	)
	{
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		for ( x = 0; x < 32; x++ )
			{
			byte pat = ((byte *)vdp->ram)[patnam++];
			vdp_refresh_graphics2_pat (vdp, pat, patgen, patcol, d);
			d += 8;
			}
		d += ( -32*8 + VDP_WIDTH*8 );
		}
	}

/* According to spec, bits 1 and 0 of register 4 (patgen) should be set.
   According to spec, bits 6 and 5 of register 3 (patcol) should be set.
   Experimentally, we discovered undocumented features:
   If bit 0 of register 4 is clear, 2nd third uses patgen from 1st third.
   If bit 1 of register 4 is clear, 3rd third uses patgen from 1st third.
   If bit 5 of register 3 is clear, 2nd third uses patcol from 1st third.
   If bit 6 of register 3 is clear, 3rd third uses patcol from 1st third.
   Maybe this is an attempt at saving VRAM.
   Anyway, we support it in our emulation. */

static void vdp_refresh_graphics2 (VDP *vdp)
	{
	word patnam  = ( ((word)(vdp->regs[2]&0x0f)) << 10 );
	word patgen1 = ( ((word)(vdp->regs[4]&0x04)) << 11 );
	word patcol1 = ( ((word)(vdp->regs[3]&0x80)) <<  6 );
	word patgen2 = (vdp->regs[4]&0x01) ? patgen1+0x0800 : patgen1;
	word patcol2 = (vdp->regs[3]&0x20) ? patcol1+0x0800 : patcol1;
	word patgen3 = (vdp->regs[4]&0x02) ? patgen1+0x1000 : patgen1;
	word patcol3 = (vdp->regs[3]&0x40) ? patcol1+0x1000 : patcol1;
	byte *d = vdp->pix + VDP_WIDTH*VBORDER + HBORDER256;

	vdp_refresh_graphics2_third (vdp, patnam        , patgen1, patcol1, d           );
	vdp_refresh_graphics2_third (vdp, patnam+0x0100 , patgen2, patcol2, d+VDP_WIDTH* 8*8);
	vdp_refresh_graphics2_third (vdp, patnam+0x0200 , patgen3, patcol3, d+VDP_WIDTH*16*8);

	vdp_refresh_sprites (vdp);
	}
/*...e*/
/*...svgavdp_refresh_text:0:*/
static void vdp_refresh_text_pat (VDP *vdp, 
	byte pat,
	byte col,
	byte *d
	)
	{
	word patgen = ( ((word)(vdp->regs[4]&0x07)) << 11 );
	word genptr = patgen + pat*8;
	byte fg = (col>>4);
	byte bg = (col&15);
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		byte gen = ((byte *)vdp->ram)[genptr++];
		for ( x = 0; x < 6; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -6 + VDP_WIDTH ); 
		}
	}

static void vdp_refresh_text (VDP *vdp)
	{
    word colptr = ((word)(vdp->regs[3] & 0xF0)) << 6;
	byte col = vdp->regs[7];
	word patnam = ( ((word)(vdp->regs[2]&0x0f)) << 10 );
	byte *d = vdp->pix + VDP_WIDTH*VBORDER + HBORDER240;
	int x, y;
	for ( y = 0; y < 24; y++ )
		{
		for ( x = 0; x < 40; x++ )
			{
            if (vdp->regs[1] & 0x08) col = vdp->ram[colptr++];
			byte pat = ((byte *)vdp->ram)[patnam++];
			vdp_refresh_text_pat (vdp, pat, col, d);
			d += 6;
			}
		d += ( -40*6 + VDP_WIDTH*8 );
		}
	}
/*...e*/
/*...svgavdp_refresh:0:*/
#define	MODE(m1,m2,m3) ( ((m1)<<2) | ((m2)<<1) | (m3) )

void vdp_refresh (VDP *vdp)
	{
	if ( (vdp->regs[1]&0x40) == 0 )
		vdp_refresh_blank (vdp);
	else
		{
		BOOLEAN m1 = ( (vdp->regs[1]&0x10) != 0 );
		BOOLEAN m2 = ( (vdp->regs[1]&0x08) != 0 );
		BOOLEAN m3 = ( (vdp->regs[0]&0x02) != 0 );
		int mode = MODE(m1,m2,m3);
		if ( vdp->pix[0] != (vdp->regs[7]&0x0f) ||
		     mode != vdp->last_mode )
			/* Ensure the border is redrawn.
			   Perhaps could do this more efficiently.
			   But it isn't going to happen very often. */
			vdp_refresh_blank (vdp);
		switch ( mode )
			{
			case MODE(0,0,0):
				vdp_refresh_graphics1 (vdp);
				break;
			case MODE(0,0,1):
				vdp_refresh_graphics2 (vdp);
				break;
			case MODE(0,1,0):
                /* Should really be Multicolour mode */
				vdp_refresh_blank (vdp);
				break;
			case MODE(1,0,0):
            case MODE(1,1,0):
				vdp_refresh_text (vdp);
				break;
			default:
				vdp_refresh_blank (vdp);
				break;
			}
		vdp->last_mode = mode;
		}
    vdp->changed = FALSE;
	}

/*...e*/

void vdp_init (VDP *vdp)
	{
    memset (vdp->regs, 0, sizeof (vdp->regs));
    vdp->changed = FALSE;
	vdp->addr    = 0x0000;
	vdp->read_mode = FALSE;
	vdp->last_mode = -1; /* None of the valid modes */
    vdp->latched = FALSE;
    vdp->latch = 0;
	memset(vdp->spr_lines, FALSE, sizeof(vdp->spr_lines));
	}
