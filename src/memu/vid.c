/*

vid.c - Video chip and TV

We maintain the model of the VDP and its memory.
Unlike the 80 column card, we can only present this via a graphical window.
We can elect to display it or not.
If we don't display it, the VDP status register won't get updated.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "Z80.h"
#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"
#include "vid.h"
#include "kbd.h"
#include "mon.h"
// #include "console.h"

/*...vZ80\46\h:0:*/
/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...vvid\46\h:0:*/
/*...vkbd\46\h:0:*/
/*...vmon\46\h:0:*/
/*...e*/

/*...svars:0:*/
static int vid_emu = 0;
static const char *vid_title = NULL;
static const char *vid_display = NULL;

#define	VBORDER 8
#define	HBORDER256 8
#define	HBORDER240 16
#define	WIDTH  (HBORDER256+256+HBORDER256)
#define	HEIGHT (VBORDER   +192+   VBORDER)

#define	N_COLS_VID 16

#define EXTRA_TIME_CHECK    0

/* These look realistic and reflect what I remember */
static COL vid_cols_rfd[N_COLS_VID] =
	{
		{    0,   0,   0 },	/* transparent */
		{    0,   0,   0 },	/* black */
		{   71, 183,  59 },	/* medium green */
		{  124, 207, 111 },	/* light green */
		{   93,  78, 255 },	/* dark blue */
		{  128, 114, 255 },	/* light blue */
		{  182,  98,  71 },	/* dark red */
		{   93, 200, 237 },	/* cyan */
		{  215, 107,  72 },	/* medium red */
		{  251, 143, 108 },	/* light red */
		{  195, 205,  65 },	/* dark yellow */
		{  211, 218, 118 },	/* light yellow */
		{   62, 159,  47 },	/* dark green */
		{  182, 100, 199 },	/* magenta */
		{  204, 204, 204 },	/* grey */
		{  255, 255, 255 },	/* white */
	};

/* But these look richer and are said to align with the hardware */
static COL vid_cols_mf_mdk[N_COLS_VID] =
	{
		{    0,   0,   0 },	/* transparent */
		{    0,   0,   0 },	/* black */
		{   32, 192,  32 },	/* medium green */
		{   96, 224,  96 },	/* light green */
		{   32,  32, 224 },	/* dark blue */
		{   64,  96, 224 },	/* light blue */
		{  160,  32,  32 },	/* dark red */
		{   64, 192, 224 },	/* cyan */
		{  224,  32,  32 },	/* medium red */
		{  224,  96,  96 },	/* light red */
		{  192, 192,  32 },	/* dark yellow */
		{  192, 192, 128 },	/* light yellow */
		{   32, 128,  32 },	/* dark green */
		{  192,  64, 160 },	/* magenta */
		{  160, 160, 160 },	/* grey */
		{  224, 224, 224 },	/* white */
	};

static COL *vid_cols; /* One of the above */

/* See http://users.stargate.net/~drushel/pub/coleco/twwmca/wk970202.html */

static WIN *vid_win = NULL;

static byte vid_regs[8];
static byte vid_regs_zeros[8] = { 0xfc,0x04,0xf0,0x00,0xf8,0x80,0xf8,0x00 };
static byte vid_status = 0x00;
static byte vid_memory[VID_MEMORY_SIZE];
static byte vid_spr_lines[192];
static byte vid_spr_coincidence[192][256];
static word vid_addr;
static BOOLEAN vid_read_mode;
static int vid_last_mode;

static BOOLEAN vid_latched = FALSE;
static byte vid_latch = 0;

typedef enum {timNone, timRead, timWrite, timAddr} TimMode;
static TimMode timLast = timNone;
static unsigned long long vid_elapsed_refresh = 0;
static unsigned long long vid_elapsed_last_data = 0;
static unsigned long long vid_elapsed_last_addr = 0;

/* These numbers are for 4MHz Z80, 50Hz refresh, PAL */
static unsigned vid_t_2us   =  8;
static unsigned vid_t_8us   = 32;
static unsigned vid_t_blank = 30769; /* (312-192)/312 scan lines */
static unsigned vid_t_frame = 80000;

static char *vid_colour_names[] =
	{
	"transparent",
	"black",
	"medium-green",
	"light-green",
	"dark-blue",
	"light-blue",
	"dark-red",
	"cyan",
	"medium-red",
	"light-red",
	"dark-yellow",
	"light-yellow",
	"dark-green",
	"magenta",
	"grey",
	"white"
	};

BOOLEAN vid_dump (void);

static int vid_dump_vdp_number = 0;
// static BOOLEAN vid_auto_dump_vdp_enabled = FALSE;

static int vid_snapshot_number = 0;
// static BOOLEAN vid_auto_snapshot_enabled = FALSE; // WJB - Never used.
/*...e*/

/*...svid_dump_vdp:0:*/
static BOOLEAN vid_dump_vdp(void)
	{
	char fn[100+1];
	FILE *fp;
	sprintf(fn, "memu%06d.vdp", vid_dump_vdp_number);
	if ( (fp = fopen(fn, "wb")) == NULL )
		return FALSE;
	if ( fwrite(vid_memory, 1, VID_MEMORY_SIZE, fp) != VID_MEMORY_SIZE )
		{
		fclose(fp);
		remove(fn);
		return FALSE;
		}
	if ( fwrite(vid_regs, 1, 8, fp) != 8 )
		{
		fclose(fp);
		remove(fn);
		return FALSE;
		}
	if ( fwrite(&vid_status, 1, 1, fp) != 1 )
		{
		fclose(fp);
		remove(fn);
		return FALSE;
		}
	fclose(fp);
	vid_dump_vdp_number++;
	return TRUE;
	}
/*...e*/

/*...svid_snapshot:0:*/
#define	BFT_BMAP   0x4d42
#define	BCA_UNCOMP 0x00000000L

static void put16(byte *b, unsigned short v)
	{
	*b++ = (byte) v; v >>= 8;
	*b   = (byte) v;
	}
static void put32(byte *b, unsigned long v)
	{
	*b++ = (byte) v; v >>= 8;
	*b++ = (byte) v; v >>= 8;
	*b++ = (byte) v; v >>= 8;
	*b   = (byte) v;
	}

static BOOLEAN vid_snapshot(void)
	{
	char fn[100+1];
	FILE *fp;
	byte hdr[54+N_COLS_VID*4];
	int i, y, x;
	sprintf(fn, "memu%06d.bmp", vid_snapshot_number);
	if ( (fp = fopen(fn, "wb")) == NULL )
		return FALSE;

	put16(hdr+ 0, BFT_BMAP);
	put32(hdr+ 2, sizeof(hdr)+WIDTH/2*HEIGHT); // cbSize
	put16(hdr+ 6, 0); // xHotspot
	put16(hdr+ 8, 0); // yHotspot
	put32(hdr+10, sizeof(hdr)); // offBits
	put32(hdr+14, 40); // cbFix
	put32(hdr+18, WIDTH); // cx
	put32(hdr+22, HEIGHT); // cy
	put16(hdr+26, 1); // cPlanes 
	put16(hdr+28, 4); // cBitCount 
	put32(hdr+30, BCA_UNCOMP); // ulCompression
	put32(hdr+34, WIDTH/2*HEIGHT); // cbImage
	put32(hdr+38, 0); // cxResolution 
	put32(hdr+42, 0); // cyResolution 
	put32(hdr+46, N_COLS_VID); // cclrUsed
	put32(hdr+50, N_COLS_VID); // cclrImportant
	for ( i = 0; i < N_COLS_VID; i++ )
		{
		hdr[54+i*4+0] = vid_cols[i].b;
		hdr[54+i*4+1] = vid_cols[i].g;
		hdr[54+i*4+2] = vid_cols[i].r;
		hdr[54+i*4+3] = 0;
		}
	if ( fwrite(hdr, 1, sizeof(hdr), fp) != sizeof(hdr) )
		{
		fclose(fp);
		remove(fn);
		return FALSE;
		}
	for ( y = 0; y < HEIGHT; y++ )
		{
		byte *s = vid_win->data + (HEIGHT-1-y)*WIDTH;
		byte line[WIDTH/2], *d = line;
		for ( x = 0; x < WIDTH; x += 2 )
			{
			*d++ = ( s[0]<<4 | s[1] );
			s += 2;
			}
		if ( fwrite(line, 1, WIDTH/2, fp) != WIDTH/2 )
			{
			fclose(fp);
			remove(fn);
			return FALSE;
			}
		}
	fclose(fp);
	vid_snapshot_number++;
	return TRUE;
	}
/*...e*/

/*...svid_reset:0:*/
void vid_reset(void)
	{
	vid_latched = FALSE;
	vid_addr = 0x000;
	}
/*...e*/

/*...svid_dat_xfer:0:*/
static void vid_data_xfer(const char *direction, word vid_addr, byte val)
	{
	diag_message(DIAG_VID_DATA, "VDP %s memory[0x%04x]=0x%02x %c%c%c%c%c%c%c%c %12s/%-12s '%c'",
		direction,
		vid_addr,
		val,
		(val&0x80)?'#':'.',
		(val&0x40)?'#':'.',
		(val&0x20)?'#':'.',
		(val&0x10)?'#':'.',
		(val&0x08)?'#':'.',
		(val&0x04)?'#':'.',
		(val&0x02)?'#':'.',
		(val&0x01)?'#':'.',
		vid_colour_names[val>>4],
		vid_colour_names[val&15],
		( val >= ' ' && val <= '~' ) ? val : '.'
		);
	}
/*...e*/

/*...svid_setup_timing_check:0:*/
void vid_setup_timing_check(unsigned t_2us, unsigned t_8us, unsigned t_blank)
	{
	vid_t_2us   = t_2us;
	vid_t_8us   = t_8us;
	vid_t_blank = t_blank;
	}
/*...e*/
/*...svid_timing_checks:0:*/
static BOOLEAN vid_timing_checks(unsigned long long elapsed, TimMode tim)
	{
	BOOLEAN ok = TRUE;
	if ( diag_flags[DIAG_VID_TIME_CHECK] )
		{
		BOOLEAN m1 = ( (vid_regs[1]&0x10) != 0 );
		BOOLEAN m2 = ( (vid_regs[1]&0x08) != 0 );
		BOOLEAN m3 = ( (vid_regs[0]&0x02) != 0 );
		unsigned gap;
        while (elapsed - vid_elapsed_refresh > vid_t_frame) vid_elapsed_refresh += vid_t_frame;
		/* These timings good for 4MHz CPU and 50Hz refresh only */
		if ( (vid_regs[1]&0x40) == 0 )
			/* Screen not enabled */
			gap = vid_t_2us;
		else if ( m1 && !m2 && !m3 )
			/* Text mode */
			gap = vid_t_2us;
		else
			/* Other modes */
			gap = ( elapsed - vid_elapsed_refresh < vid_t_blank ) ? vid_t_2us : vid_t_8us;

        unsigned long long since_last = elapsed;
        if ((tim == timRead) && (timLast == timAddr)) since_last -= vid_elapsed_last_addr;
        else since_last -= vid_elapsed_last_data;
#if EXTRA_TIME_CHECK    
        diag_message (DIAG_VID_TIME_CHECK, "VDP timing: gap = %llu, required = %llu, elapsed = %lluT, current frame = %lluT",
            since_last, gap, elapsed, elapsed - vid_elapsed_refresh);
#endif
		if ( since_last < gap )
			{
			diag_message(DIAG_VID_TIME_CHECK, "VDP error, %uT gap required, %lluT actual (elapsed=%lluT)", gap, since_last, elapsed);
			if ( diag_flags[DIAG_VID_TIME_CHECK_ABORT] )
				fatal("VDP timing constraint violated, so exiting");
			if ( diag_flags[DIAG_VID_TIME_CHECK_DROP] )
				ok = FALSE;
			}
		}
    if (tim == timAddr) vid_elapsed_last_addr = elapsed;
    else vid_elapsed_last_data = elapsed;
    timLast = tim;
	return ok;
	}
/*...e*/

#if EXTRA_TIME_CHECK == 2
unsigned long long io_last = 0;
#endif

/*...svid_out1 \45\ data write:0:*/
void vid_out1(byte val, unsigned long long elapsed)
	{
#if EXTRA_TIME_CHECK == 2
    unsigned long long gap = elapsed - io_last;
    diag_message (DIAG_VID_TIME_CHECK, "out1 (0x%02X) elapsed = %lld, gap = %lld %s", val, elapsed, gap,
        gap < vid_t_2us ? "****" : gap < vid_t_8us ? "**" : "");
    io_last = elapsed;
#endif
	// diag_message (DIAG_ALWAYS,"Out 0x01: 0x%02X", val);
	if ( vid_read_mode )
		/* VDEB.COM can do this, so don't consider it fatal */
		{
		diag_message(DIAG_VID_DATA, "VDP error, out(1,0x%02x) when in read mode", (unsigned) val);
		vid_read_mode = FALSE; /* Prevent lots of warnings */
		}
	if ( vid_timing_checks(elapsed, timWrite) )
		{
		vid_memory[vid_addr] = val;
		if ( diag_flags[DIAG_VID_DATA] )
			vid_data_xfer("output", vid_addr, val);
		vid_addr = ( (vid_addr+1) & (VID_MEMORY_SIZE-1) );
		vid_latched = FALSE; /* According to http://bifi.msxnet.org/msxnet/tech/tms9918a.txt, section 2.3 */
		}
	}
/*...e*/
/*...svid_out2 \45\ latch value\44\ then act:0:*/
void vid_out2(byte val, unsigned long long elapsed)
	{
    static word vid_addr_old;
#if EXTRA_TIME_CHECK == 2
    unsigned long long gap = elapsed - io_last;
    diag_message (DIAG_VID_TIME_CHECK, "out2 (0x%02X) elapsed = %lld, gap = %lld %s", val, elapsed,  gap,
        gap < vid_t_2us ? "****" : gap < vid_t_8us ? "**" : "");
    io_last = elapsed;
#endif
	// diag_message (DIAG_ALWAYS,"Out 0x02: 0x%02X", val);
	if ( !vid_latched )
		/* First write to port 2, record the value */
		{
        vid_timing_checks(elapsed, timAddr);
        vid_addr_old = vid_addr;
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
                vid_timing_checks(elapsed, timAddr);
				vid_addr = ( ((val&0x3f)<<8)|vid_latch );
				vid_read_mode = TRUE;
				diag_message(DIAG_VID_ADDRESS, "VDP address set to 0x%04x for reading (was 0x%04x)", vid_addr, vid_addr_old);
				break;
			case 0x40:
				/* Set up for writing to VRAM */
                vid_timing_checks(elapsed, timAddr);
				vid_addr = ( ((val&0x3f)<<8)|vid_latch );
				vid_read_mode = FALSE;
				diag_message(DIAG_VID_ADDRESS, "VDP address set to 0x%04x for writing (was 0x%04x)", vid_addr, vid_addr_old);
				break;
			case 0x80:
				/* Write VDP register.
				   Various bits must be zero. */
				val &= 7;
				if ( (vid_latch & vid_regs_zeros[val]) != 0x00 )
					diag_message(DIAG_VID_REGISTERS, "VDP error, attempt to set VDP register %d to 0x%02x", val, vid_latch);
				vid_regs[val] = vid_latch;
				diag_message(DIAG_VID_REGISTERS, "VDP register %d set to 0x%02x", val, vid_latch);
				break;
			case 0xc0:
				/* VDEB.COM can do this, so don't consider it fatal */
				diag_message(DIAG_VID_REGISTERS, "VDP error, out(2,0x%02x) then out(2,0x%02x)", vid_latch, val);
				break;
			}
		vid_latched = FALSE;
		}
	}
/*...e*/
/*...svid_in1  \45\ data read:0:*/
byte vid_in1(unsigned long long elapsed)
	{
#if EXTRA_TIME_CHECK == 2
    unsigned long long gap = elapsed - io_last;
    diag_message (DIAG_VID_TIME_CHECK, "in1 () elapsed = %lld, gap = %lld %s", elapsed, gap,
        gap < vid_t_2us ? "****" : gap < vid_t_8us ? "**" : "");
    io_last = elapsed;
#endif
	byte val;
	if ( ! vid_read_mode )
		/* VDEB.COM can do this, so don't consider it fatal */
		{
		diag_message(DIAG_VID_DATA, "VDP error, in(1) when in write mode");
		vid_read_mode = TRUE; /* Prevent lots of warnings */
		val = 0xff; /* There was no prefetch */
		vid_latched = FALSE; /* For symmetry with vid_out2. */
		/* Note that we don't do the timing check that perhaps we should. */
		}
	else if ( vid_timing_checks(elapsed, timRead) )
		{
		val = vid_memory[vid_addr];
        // diag_message (DIAG_ALWAYS, "VDP Read:      addr = 0x%04X, val = 0x%02X", vid_addr, val);
		if ( diag_flags[DIAG_VID_DATA] )
			vid_data_xfer("input ", vid_addr, val);
		vid_addr = ( (vid_addr+1) & (VID_MEMORY_SIZE-1) );
		vid_latched = FALSE; /* For symmetry with vid_out2.
					Not seen it explicitly documented.
					Downstream Danger relies on this. */
		}
	else
		val = 0xff;
	return val;
	}
/*...e*/
/*...svid_in2  \45\ read status register:0:*/
/* Best to only do this while VDP interrupt pending */
byte vid_in2(unsigned long long elapsed)
	{
	// diag_message (DIAG_ALWAYS,"In  0x02");
	byte value = vid_status;
	diag_message(DIAG_VID_STATUS, "VDP status register read 0x%02x", value);
	vid_status = 0; /* Clear F, C, 5S, FSN, for subsequent reads */
	if ( vid_latched )
		{
		diag_message(DIAG_VID_STATUS, "VDP status register read clears the latch");
		vid_latched = FALSE;
		}
	return value;
	}
/*...e*/

/*...svid_refresh_win_blank:0:*/
static void vid_refresh_win_blank(void)
	{
	memset(vid_win->data, (vid_regs[7]&0x0f), WIDTH*HEIGHT);
	}
/*...e*/
/*...svid_refresh_win_sprites:0:*/
/*...svid_refresh_win_sprites_line_check:0:*/
static BOOLEAN vid_refresh_win_sprites_line_check(int scn_y, int sprite)
	{
	if ( scn_y < 0 || scn_y >= 192 )
		return FALSE;
	if ( ++vid_spr_lines[scn_y] <= 4 )
		return TRUE;
	else
		{
		if ( (vid_status & 0x40) == 0 )
			vid_status |= (0x40|sprite); /* 5S and FSN */
		if ( diag_flags[DIAG_VID_MARKERS] )
			memset(vid_win->data+(VBORDER+scn_y)*WIDTH+HBORDER256+3, 0x0d, 3); /* 5S MARKER */
		return FALSE;
		}
	}
/*...e*/
/*...svid_refresh_win_sprites_plot:0:*/
static void vid_refresh_win_sprites_plot(int scn_x, int scn_y, int spr_col)
	{
	if ( scn_x >= 0 && scn_x < 256 )
		{
		if ( vid_spr_coincidence[scn_y][scn_x] )
			/* Already a dot from a higher priority sprite at this point */
			vid_status |= 0x20; /* C */
		else
			{
			if ( spr_col != 0 )
				vid_win->data[(VBORDER+scn_y)*WIDTH+(HBORDER256+scn_x)] = spr_col;
			vid_spr_coincidence[scn_y][scn_x] = TRUE;
			}
		}
	}
/*...e*/

static void vid_refresh_win_sprites(void)
	{
	byte size = ( (vid_regs[1]&0x02) != 0 );
	byte mag  = ( (vid_regs[1]&0x01) != 0 );
	word sprgen = ((word)(vid_regs[6]&0x07)<<11);
	word spratt = ((word)(vid_regs[5]&0x7f)<<7);
	int sprite, x, y, scn_x;
	byte spr_pat_mask = size ? 0xfc : 0xff;
	for ( sprite = 0; sprite < 32; sprite++ )
		{
		int spr_y     = (int) (unsigned) vid_memory[spratt++]; /* 0-255, partially signed */
		int spr_x     = (int) (unsigned) vid_memory[spratt++]; /* 0-255 */
		byte spr_pat  = vid_memory[spratt++] & spr_pat_mask;
		word spr_addr = sprgen+((word)spr_pat<<3);
		byte spr_flag = vid_memory[spratt++];
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
				word bits = ((word)vid_memory[spr_addr]<<8)|((word)vid_memory[spr_addr+16]);
				spr_addr++;
				if ( mag )
/*...sMAG\61\1\44\ SIZE\61\1:40:*/
{
int scn_y = spr_y+y*2;
if ( vid_refresh_win_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x+=2 )
		if ( bits & (0x8000>>x) )
			{
			vid_refresh_win_sprites_plot(scn_x  , scn_y, spr_col);
			vid_refresh_win_sprites_plot(scn_x+1, scn_y, spr_col);
			}
++scn_y;
if ( vid_refresh_win_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x+=2 )
		if ( bits & (0x8000>>x) )
			{
			vid_refresh_win_sprites_plot(scn_x  , scn_y, spr_col);
			vid_refresh_win_sprites_plot(scn_x+1, scn_y, spr_col);
			}
}
/*...e*/
				else
/*...sMAG\61\0\44\ SIZE\61\1:40:*/
{
int scn_y = spr_y+y;
if ( vid_refresh_win_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 16; x++, scn_x++ )
		if ( bits & (0x8000>>x) )
			vid_refresh_win_sprites_plot(scn_x, scn_y, spr_col);
}
/*...e*/
				}
		else
			for ( y = 0; y < 8; y++ )
				{
				byte bits = vid_memory[spr_addr++];
				if ( mag )
/*...sMAG\61\1\44\ SIZE\61\0:40:*/
{
int scn_y = spr_y+y*2;
if ( vid_refresh_win_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x+=2 )
		if ( bits & (0x80>>x) )
			{
			vid_refresh_win_sprites_plot(scn_x  , scn_y, spr_col);
			vid_refresh_win_sprites_plot(scn_x+1, scn_y, spr_col);
			}
++scn_y;
if ( vid_refresh_win_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x+=2 )
		if ( bits & (0x80>>x) )
			{
			vid_refresh_win_sprites_plot(scn_x  , scn_y, spr_col);
			vid_refresh_win_sprites_plot(scn_x+1, scn_y, spr_col);
			}
}
/*...e*/
				else
/*...sMAG\61\0\44\ SIZE\61\0:40:*/
{
int scn_y = spr_y+y;
if ( vid_refresh_win_sprites_line_check(scn_y, sprite) )
	for ( x = 0, scn_x = spr_x; x < 8; x++, scn_x++ )
		if ( bits & (0x80>>x) )
			vid_refresh_win_sprites_plot(scn_x, scn_y, spr_col);
}
/*...e*/
				}
		}

	/* Wipe out the coincidence buffer for next time.
	   We can do it quickly now, as we know which lines
	   we have written to */
	for ( y = 0; y < 192; y++ )
		if ( vid_spr_lines[y] > 0 )
			{
			vid_spr_lines[y] = 0;
			memset(vid_spr_coincidence[y], FALSE, 256);
			}
	}
/*...e*/
/*...svid_refresh_win_smooth:0:*/
/* This is a diagnostic simply to allow me to see on
   the screen how smoothly simulated time is progressing. */

static int vid_ymarker = 0;

static void vid_refresh_win_smooth(int hborder)
	{
	if ( diag_flags[DIAG_VID_MARKERS] )
		{
		memset(vid_win->data + WIDTH*(VBORDER+vid_ymarker)+hborder, 0x0f, 3);
		if ( ++vid_ymarker == 192 ) vid_ymarker = 0;
		}
	}
/*...e*/
/*...svid_refresh_win_graphics1:0:*/
static void vid_refresh_win_graphics1_pat(
	byte pat,
	word patgen,
	word patcol,
	byte *d
	)
	{
	word genptr = patgen + pat*8;
	word colptr = patcol + pat/8;
	byte col = vid_memory[colptr];
	byte fg = (col>>4);
	byte bg = (col&15);
	int x, y;
	if ( fg == 0 ) fg = (vid_regs[7]&0x0f);
	if ( bg == 0 ) bg = (vid_regs[7]&0x0f);
	for ( y = 0; y < 8; y++ )
		{
		byte gen = vid_memory[genptr++];
		for ( x = 0; x < 8; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -8 + WIDTH ); 
		}
	}

static void vid_refresh_win_graphics1_third(
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
			byte pat = vid_memory[patnam++];
			vid_refresh_win_graphics1_pat(pat, patgen, patcol, d);
			d += 8;
			}
		d += ( -32*8 + WIDTH*8 );
		}
	}

static void vid_refresh_win_graphics1(void)
	{
	word patnam = ( ((word)(vid_regs[2]&0x0f)) << 10 );
	word patgen = ( ((word)(vid_regs[4]&0x07)) << 11 );
	word patcol = ( ((word)(vid_regs[3]&0xff)) <<  6 );
	byte *d = vid_win->data + WIDTH*VBORDER + HBORDER256;

	vid_refresh_win_graphics1_third(patnam       , patgen, patcol, d           );
	vid_refresh_win_graphics1_third(patnam+0x0100, patgen, patcol, d+WIDTH* 8*8);
	vid_refresh_win_graphics1_third(patnam+0x0200, patgen, patcol, d+WIDTH*16*8);

	vid_refresh_win_sprites();

	vid_refresh_win_smooth(HBORDER256);
	}
/*...e*/
/*...svid_refresh_win_graphics2:0:*/
static void vid_refresh_win_graphics2_pat(
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
		byte gen = vid_memory[genptr++];
		byte col = vid_memory[colptr++];
		byte fg = (col>>4);
		byte bg = (col&15);
		if ( fg == 0 ) fg = (vid_regs[7]&0x0f);
		if ( bg == 0 ) bg = (vid_regs[7]&0x0f);
		for ( x = 0; x < 8; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -8 + WIDTH ); 
		}
	}

static void vid_refresh_win_graphics2_third(
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
			byte pat = vid_memory[patnam++];
			vid_refresh_win_graphics2_pat(pat, patgen, patcol, d);
			d += 8;
			}
		d += ( -32*8 + WIDTH*8 );
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

static void vid_refresh_win_graphics2(void)
	{
	word patnam  = ( ((word)(vid_regs[2]&0x0f)) << 10 );
	word patgen1 = ( ((word)(vid_regs[4]&0x04)) << 11 );
	word patcol1 = ( ((word)(vid_regs[3]&0x80)) <<  6 );
	word patgen2 = (vid_regs[4]&0x01) ? patgen1+0x0800 : patgen1;
	word patcol2 = (vid_regs[3]&0x20) ? patcol1+0x0800 : patcol1;
	word patgen3 = (vid_regs[4]&0x02) ? patgen1+0x1000 : patgen1;
	word patcol3 = (vid_regs[3]&0x40) ? patcol1+0x1000 : patcol1;
	byte *d = vid_win->data + WIDTH*VBORDER + HBORDER256;

	/* There are other bits in register 3 which should be set. */
	if ( (vid_regs[3]&0x1f) != 0x1f )
		diag_message(DIAG_VID_REGISTERS, "VDP error, register 3 is 0x%02x and bottom 5 bits should be 1s", vid_regs[3]);

	vid_refresh_win_graphics2_third(patnam        , patgen1, patcol1, d           );
	vid_refresh_win_graphics2_third(patnam+0x0100 , patgen2, patcol2, d+WIDTH* 8*8);
	vid_refresh_win_graphics2_third(patnam+0x0200 , patgen3, patcol3, d+WIDTH*16*8);

	vid_refresh_win_sprites();

	vid_refresh_win_smooth(HBORDER256);
	}
/*...e*/
/*...svid_refresh_win_multicolour:0:*/
static void vid_refresh_win_multicolour(void)
	{
	fatal("VDP multicolour mode not yet implemented");
	}
/*...e*/
/*...svid_refresh_win_text:0:*/
static void vid_refresh_win_text_pat(
	byte pat,
	word patgen,
	byte *d
	)
	{
	word genptr = patgen + pat*8;
	byte col = vid_regs[7];
	byte fg = (col>>4);
	byte bg = (col&15);
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		byte gen = vid_memory[genptr++];
		for ( x = 0; x < 6; x++ )
			*d++ = ( gen & (0x80>>x) ) ? fg : bg;
		d += ( -6 + WIDTH ); 
		}
	}

static void vid_refresh_win_text_third(
	word patnam,
	word patgen,
	byte *d
	)
	{
	int x, y;
	for ( y = 0; y < 8; y++ )
		{
		for ( x = 0; x < 40; x++ )
			{
			byte pat = vid_memory[patnam++];
			vid_refresh_win_text_pat(pat, patgen, d);
			d += 6;
			}
		d += ( -40*6 + WIDTH*8 );
		}
	}

static void vid_refresh_win_text(void)
	{
	word patnam = ( ((word)(vid_regs[2]&0x0f)) << 10 );
	word patgen = ( ((word)(vid_regs[4]&0x07)) << 11 );
	byte *d = vid_win->data + WIDTH*VBORDER + HBORDER240;

	vid_refresh_win_text_third(patnam      , patgen, d           );
	vid_refresh_win_text_third(patnam+ 8*40, patgen, d+WIDTH* 8*8);
	vid_refresh_win_text_third(patnam+16*40, patgen, d+WIDTH*16*8);

	vid_refresh_win_smooth(HBORDER240);
	}
/*...e*/
/*...svid_refresh_win:0:*/
#define	MODE(m1,m2,m3) ( ((m1)<<2) | ((m2)<<1) | (m3) )

static void vid_refresh_win(void)
	{
	if ( (vid_regs[1]&0x40) == 0 )
		vid_refresh_win_blank();
	else
		{
		BOOLEAN m1 = ( (vid_regs[1]&0x10) != 0 );
		BOOLEAN m2 = ( (vid_regs[1]&0x08) != 0 );
		BOOLEAN m3 = ( (vid_regs[0]&0x02) != 0 );
		int mode = MODE(m1,m2,m3);
        // diag_message(DIAG_VID_REFRESH, "VDP mode %d", mode);
		if ( vid_win->data[0] != (vid_regs[7]&0x0f) ||
		     mode != vid_last_mode )
			/* Ensure the border is redrawn.
			   Perhaps could do this more efficiently.
			   But it isn't going to happen very often. */
			vid_refresh_win_blank();
		switch ( mode )
			{
			case MODE(0,0,0):
				vid_refresh_win_graphics1();
				break;
			case MODE(0,0,1):
				vid_refresh_win_graphics2();
				break;
			case MODE(0,1,0):
				vid_refresh_win_multicolour();
				break;
			case MODE(1,0,0):
				vid_refresh_win_text();
				break;
			default:
				/* I don't bomb at this point,
				   although this is an invalid mode,
				   as we may be half way through updating the
				   VDP registers when we are called here */
				vid_refresh_win_blank();
				break;
			}
		vid_last_mode = mode;
		}
	win_refresh(vid_win);
	}

void vid_refresh_vdeb(void)
	{
	if ( vid_emu & VIDEMU_WIN )	vid_refresh_win();
    }

/*...e*/
/*...svid_refresh:0:*/
/* This should happen once every 50th/60th of a second.
   Its as if the picture appears at the end of the "vertical active display".
   In the real hardware, the picture will have been sent out to the TV over
   the whole of the "vertical active display" period.
   So we raise the VDP interrupt, saying its safe to make changes. */
void vid_refresh(unsigned long long elapsed)
	{
	diag_message(DIAG_VID_REFRESH, "VDP refresh (elapsed=%lluT)", elapsed);
	vid_status = 0x00;
	if ( vid_emu & VIDEMU_WIN )
		{
		vid_refresh_win();
		/* Now lets VRAM to a file, if required */
		if ( diag_flags[DIAG_ACT_VID_DUMP_VDP] )
			{
			if ( vid_dump_vdp() )
				diag_message(DIAG_ALWAYS, "VDP dump");
			diag_flags[DIAG_ACT_VID_DUMP_VDP] = FALSE;
			}
		else if ( diag_flags[DIAG_VID_AUTO_DUMP_VDP] )
			{
			if ( vid_dump_vdp() )
				diag_message(DIAG_ALWAYS, "VDP auto dump");
			}
		/* Now lets dump screen to bitmap file, if required */
		if ( diag_flags[DIAG_ACT_VID_DUMP] )
			{
			if ( vid_dump () && vid_snapshot() )
				diag_message(DIAG_ALWAYS, "VDP memory dump");
			diag_flags[DIAG_ACT_VID_DUMP] = FALSE;
			}
		else if ( diag_flags[DIAG_ACT_VID_SNAPSHOT] )
			{
			if ( vid_snapshot() )
				diag_message(DIAG_ALWAYS, "VDP snapshot");
			diag_flags[DIAG_ACT_VID_SNAPSHOT] = FALSE;
			}
		else if ( diag_flags[DIAG_VID_AUTO_SNAPSHOT] )
			{
			if ( vid_snapshot() )
				diag_message(DIAG_ALWAYS, "VDP auto snapshot");
			}
		}
	vid_status |= 0x80; /* Frame is drawn */
    if (vid_regs[1] & 0x20)
        {
        vid_elapsed_refresh = elapsed; /* Remember when redrawn */
        }
    else
        {
        while (elapsed - vid_elapsed_refresh > vid_t_frame) vid_elapsed_refresh += vid_t_frame;
        }
	if ( diag_flags[DIAG_ACT_VID_REGS] )
		{
		int i;
		for ( i = 0; i < 8; i++ )
			diag_message(DIAG_ALWAYS, "VPD register %d is 0x%02x", i, vid_regs[i]);
		diag_flags[DIAG_ACT_VID_REGS] = FALSE;
		}
	// diag_message(DIAG_VID_REFRESH, "VDP refresh completed");
	}
/*...e*/

/*...svid_int_pending:0:*/
BOOLEAN vid_int_pending(void)
	{
	return (vid_status&0x80) != 0 && /* F bit set */
	       (vid_regs[1]&0x20) != 0; /* Interrupts are enabled */
	}
/*...e*/
/*...svid_clear_int:0:*/
void vid_clear_int(void)
	{
	vid_status &= ~0x80;
	}
/*...e*/

/*...svid_vram_read:0:*/
byte vid_vram_read(word addr)
	{
	return vid_memory[addr%VID_MEMORY_SIZE];
	}
/*...e*/
/*...svid_vram_write:0:*/
void vid_vram_write(word addr, byte b)
	{
	vid_memory[addr%VID_MEMORY_SIZE] = b;
	}
/*...e*/
/*...svid_reg_read:0:*/
byte vid_reg_read(int reg)
	{
	return vid_regs[reg];
	}
/*...e*/
/*...svid_reg_write:0:*/
void vid_reg_write(int reg, byte b)
	{
	vid_regs[reg] = b;
	}
/*...e*/
/*...svid_status_read:0:*/
byte vid_status_read(void)
	{
	return vid_status;
	}
/*...e*/

/*...svid_init:0:*/

void vid_init(int emu, int width_scale, int height_scale)
	{
	vid_emu = emu;

	vid_regs[0] =
	vid_regs[1] = 0x0000; /* per VDP spec., others undefined */
	vid_addr    = 0x0000;
	vid_read_mode = FALSE;
	vid_last_mode = -1; /* None of the valid modes */

	memset(vid_spr_lines, FALSE, sizeof(vid_spr_lines));
	memset(vid_spr_coincidence, FALSE, sizeof(vid_spr_coincidence));

	if ( vid_emu & VIDEMU_WIN )
		{
		vid_cols = ( emu & VIDEMU_WIN_HW_PALETTE )
			? vid_cols_mf_mdk : vid_cols_rfd;
        // con_print ("Before win_create\n",0);
		vid_win = win_create(
			WIDTH, HEIGHT,
			width_scale, height_scale,
			vid_title ? vid_title : "Memu Video",
			vid_display, /* display */
			NULL, /* geometry */
			vid_cols, N_COLS_VID,
			kbd_win_keypress,
			kbd_win_keyrelease
			);
        // con_print ("After win_create\n",0);
        return;
		win_refresh(vid_win);
        // con_print ("After win_refresh\n",0);
		}
	}
/*...e*/
/*...svid_term:0:*/
void vid_term(void)
	{
	if ( vid_emu & VIDEMU_WIN )
        {
		if ( vid_win != NULL ) win_delete(vid_win);
        vid_win = NULL;
        }
	vid_emu = 0;
	}
/*...e*/
void vid_set_title (const char *title)
	{
    vid_title = title;
	}

const char * vid_get_title (void)
	{
	return vid_title;
	}

void vid_set_display (const char *display)
	{
    vid_display = display;
	}

const char * vid_get_display (void)
	{
	return vid_display;
	}

void vid_max_scale (int *pxscl, int *pyscl)
    {
    int ix;
    int iy;
    win_max_size (vid_display, &ix, &iy);
    ix /= 256;
    iy /= 192;
    if ( ( ix == 0 ) || ( iy == 0 ) )
        {
        *pxscl = 1;
        *pyscl = 1;
        }
    else if ( ix > iy )
        {
        *pxscl = iy;
        *pyscl = iy;
        }
    else
        {
        *pxscl = ix;
        *pyscl = iy;
        }
    }

#if 0
//#ifdef HAVE_VGA - No longer required. vga.c has its own copy.
byte vid_getram (unsigned int addr)
    {
    return vid_memory[addr];
    }

byte vid_getreg (unsigned int addr)
    {
    return vid_regs[addr];
    }

WIN *vid_getwin (void)
    {
    return vid_win;
    }
#endif

BOOLEAN vid_dump (void)
    {
    const char *psTables[5] = { " Name table", " Colour table", " Pattern table", " Sprite attributes",
                                " Sprite patterns" };
    int taddr[5];
    char sName[20];
    FILE *pfil;
    sprintf (sName, "vdpdump_%06d.c", vid_snapshot_number);
    pfil = fopen (sName, "w");
    if ( pfil == NULL ) return FALSE;
    fprintf (pfil, "/* %s - VDP register and VRAM dump corresponding to snapshot \"memu%06d.bmp\" */\n\n",
        sName, vid_snapshot_number);
    fprintf (pfil, "#include \"vdpdump.h\"\n\nuint8_t regs[8] = {");
    for ( int i = 0; i < 7; ++i )
        {
        fprintf (pfil, " 0x%02X,", vid_regs[i]);
        }
    fprintf (pfil, " 0x%02X};\n\n", vid_regs[7]);
    taddr[0] = ( vid_regs[2] & 0x0F ) << 10;
    taddr[1] = vid_regs[3] << 6;
    taddr[2] = ( vid_regs[4] & 0x07 ) << 11;
    taddr[3] = ( vid_regs[5] & 0x7F ) << 7;
    taddr[4] = ( vid_regs[6] & 0x07 ) << 11;
    if ( ( ( vid_regs[0] & 0x02 ) == 0x02 ) && ( ( vid_regs[1] & 0x18 ) == 0 ) )
        {
        taddr[1] &= 0x2000;
        taddr[2] &= 0x2000;
        }
    fprintf (pfil, "uint8_t vram[NVRAM] = {\n  ");
    for ( int i = 0; i < VID_MEMORY_SIZE - 8; i += 8 )
        {
        for ( int j = 0; j < 8; ++j )
            {
            fprintf (pfil, " 0x%02X,", vid_memory[i + j]);
            }
        fprintf (pfil, "\t0x%04X", i);
        for ( int j = 0; j < 5; ++j )
            {
            if ( i == taddr[j] ) fprintf (pfil, "%s", psTables[j]);
            }
        fprintf (pfil, "\n  ");
        }
    for ( int i = VID_MEMORY_SIZE - 8; i < VID_MEMORY_SIZE - 1; ++ i)
        {
        fprintf (pfil, " 0x%02X,", vid_memory[i]);
        }
    fprintf (pfil, " 0x%02X };\n", vid_memory[VID_MEMORY_SIZE - 1]);
    fclose (pfil);
    return TRUE;
    }

extern void vid_show (void)
    {
    if (vid_win != NULL) win_show (vid_win);
    }
