/*

memu.c - Memotech Emulator

*/

/*...sincludes:0:*/
#include "ff_stdio.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#if defined(AIX) || defined(LINUX) || defined(SUN) || defined(MACOSX)
#include <unistd.h>
#elif defined(__circle__)
#include <io.h>
#elif defined(__Pico__)
#include "display_pico.h"
#endif
#include <time.h>

#if defined(BEMEMU)
  #if defined(UNIX)
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/stat.h>
  #elif defined(WIN32)
    #define BOOLEAN BOOLEANx
    #include <windows.h>
    #undef BOOLEAN
  #endif
#endif

#include "Z80.h"
#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"
#include "roms.h"
#include "mem.h"
#include "vid.h"
#include "kbd.h"
#include "tape.h"
#include "config.h"
#ifdef HAVE_JOY
#include "joy.h"
#endif
#ifdef HAVE_DART
#include "dart.h"
#endif
#include "snd.h"
#include "ctc.h"
#include "mon.h"
#include "sdxfdc.h"
#ifdef HAVE_SID
#include "sid.h"
#endif
#include "printer.h"
#ifdef HAVE_SPEC
#include "spec.h"
#endif
#ifdef HAVE_OSFS
#include "cpm.h"
#endif
#ifdef HAVE_DISASS
#include "dis.h"
#endif
#ifdef HAVE_UI
#include "ui.h"
#endif
#ifdef HAVE_VDEB
#include "vdeb.h"
#endif
#include "memu.h"
#ifdef HAVE_CFX2
#include "cfx2.h"
#endif
#ifdef HAVE_VGA
#include "vga.h"
#endif
#ifdef HAVE_NFX
#include "nfx.h"
#endif
#include "dirmap.h"

/*...vZ80\46\h:0:*/
/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...vroms\46\h:0:*/
/*...vmem\46\h:0:*/
/*...vvid\46\h:0:*/
/*...vkbd\46\h:0:*/
/*...vjoy\46\h:0:*/
/*...vdart\46\h:0:*/
/*...vsnd\46\h:0:*/
/*...vctc\46\h:0:*/
/*...vmon\46\h:0:*/
/*...vsdxfdc\46\h:0:*/
/*...vsid\46\h:0:*/
/*...vprinter\46\h:0:*/
/*...vspec\46\h:0:*/
/*...vcpm\46\h:0:*/
/*...vdis\46\h:0:*/
/*...vui\46\h:0:*/
/*...vmemu\46\h:0:*/
/*...e*/

/*...susage:0:*/
#ifdef ALT_USAGE
extern void ALT_USAGE (void);
#endif

#ifdef ALT_EXIT
extern void ALT_EXIT (int reason);
#endif

const char *psExe = "memu";

void usage(const char *psErr, ...)
	{
	fprintf(stderr, "usage: %s [flags]\n", psExe);
    fprintf(stderr, "Flags may be specified on command line or in files\n");
    fprintf(stderr, "\"memu.cfg\" or \"memu0.cfg\" in the program folder.\n");
	fprintf(stderr, "flags: -help                display this list\n");
    fprintf(stderr, "       -ignore              Allow flags for unimplemented features\n");
#ifdef ALT_USAGE
	ALT_USAGE ();
#endif
	fprintf(stderr, "       -iobyte iobyte       specify IOBYTE (initially 0x00)\n");
#ifndef SMALL_MEM
	fprintf(stderr, "       -subpage subpage     set ROM subpage (initially 0)\n");
#endif
	fprintf(stderr, "       -addr addr           set the address (initially 0x0000)\n");
	fprintf(stderr, "       -mem file            load file at address\n");
	fprintf(stderr, "       -mem-blocks n        number of 16KB memory blocks (default 4)\n");
	fprintf(stderr, "       -mem-mtx500          equivelent to -mem-blocks 2\n");
#ifndef SMALL_MEM
	fprintf(stderr, "       -n-subpages rom n    set number of subpages\n");
	fprintf(stderr, "       -romX file           load ROM X from file\n");
	fprintf(stderr, "       -rompairX file       load ROM X and X+1 from file\n");
#endif
	fprintf(stderr, "       -vid-win             emulate VDP and TV using a graphical window\n");
	fprintf(stderr, "       -vid-win-big,-v      increase size of VDP window (repeat as necessary)\n");
	fprintf(stderr, "       -vid-win-max         make VDP window maximum size\n");
	fprintf(stderr, "       -vid-win-hw-palette  use an alternate palette\n");
	fprintf(stderr, "       -vid-win-title       set title for VDP window\n");
	fprintf(stderr, "       -vid-win-display     set display to use for VDP window\n");
	fprintf(stderr, "       -vid-ntsc            refresh at 60Hz (instead of 50Hz)\n");
	fprintf(stderr, "       -snd-portaudio,-s    emulate sound chip using portaudio\n");
	fprintf(stderr, "       -snd-latency value   instruct portaudio to use a given latency\n");
	fprintf(stderr, "       -mon-win             emulate 80 column card using a graphical window\n");
	fprintf(stderr, "       -mon-win-big,-mw     increase size of MON window (repeat as necessary)\n");
	fprintf(stderr, "       -mon-win-max         make MON window maximum size\n");
	fprintf(stderr, "       -mon-win-mono        green screen monochrome\n");
#ifdef HAVE_TH
	fprintf(stderr, "       -mon-th,-mt          emulate 80 column card using full screen text mode\n");
#endif
#ifdef HAVE_CONSOLE
	fprintf(stderr, "       -mon-console,-mc     emulate 80 column card using console only\n");
	fprintf(stderr, "       -mon-console-nokey   keyboard status shows no keys pressed\n");
#endif
	fprintf(stderr, "       -mon-no-ignore-init  don't ignore writes to non-emulated registers\n");
	fprintf(stderr, "       -mon-win-title       set title for 80 column window\n");
	fprintf(stderr, "       -mon-win-display     set display to use for 80 column window\n");
#ifdef HAVE_VGA
    fprintf(stderr, "       -vga                 emulate Propeller VGA display\n");
#endif
	fprintf(stderr, "       -kbd-remap           remaps MTX keyboard (despite shift state)\n");
	fprintf(stderr, "       -kbd-country n       sets the country code switches to n (default 0)\n");
#ifndef __Pico__
	fprintf(stderr, "       -kbd-type string     auto type keys in this string\n");
	fprintf(stderr, "       -kbd-type-file fn    auto type keys in this file\n");
#endif
#ifdef HAVE_JOY
	fprintf(stderr, "       -joy,-j              enable joystick support\n");
	fprintf(stderr, "       -joy-buttons string  define left,right,up,down and fire buttons\n");
	fprintf(stderr, "       -joy-central n       percentage off-centre to press direction\n");
#endif
#ifdef HAVE_DART
	fprintf(stderr, "       -serial1-dev dev     serial 1 in/out from device\n");
	fprintf(stderr, "       -serial1-in fn       serial 1 input from file/pipe\n");
	fprintf(stderr, "       -serial1-out fn      serial 1 output to file/pipe\n");
	fprintf(stderr, "       -serial2-dev dev     serial 2 in/out from device\n");
	fprintf(stderr, "       -serial2-in fn       serial 2 input from file/pipe\n");
	fprintf(stderr, "       -serial2-out fn      serial 2 output to file/pipe\n");
#endif
	fprintf(stderr, "       -sdx-tracks n        specify tracks of first drive (default 80)\n");
	fprintf(stderr, "       -sdx-mfloppy file    specify .mfloppy file in SDX first drive\n");
	fprintf(stderr, "       -sdx-tracks2 n       specify tracks of second drive (default 80)\n");
	fprintf(stderr, "       -sdx-mfloppy2 file   specify .mfloppy file in SDX second drive\n");
#ifdef HAVE_SID
	fprintf(stderr, "       -sidisc-huge         enable Silicon Disc huge mode\n");
	fprintf(stderr, "       -sidisc-no-save      don't save Silicon Disc content on termination\n");
	fprintf(stderr, "       -sidisc-file n file  specify Silicon Disc content for a drive\n");
#endif
#ifdef HAVE_CFX2
    fprintf(stderr, "       -cfx2 rom_file       enable CFX-II emulation and specify ROM image file\n");
    fprintf(stderr, "       -cf-image c:p file   specify data image for partition (p) on card (c)\n");
#endif
	fprintf(stderr, "       -prn-file file       specify file to receive printer output\n");
	fprintf(stderr, "       -tape-dir path       .mtx files are in this directory\n");
	fprintf(stderr, "       -tape-overwrite      SAVE can overwrite an existing file\n");
	fprintf(stderr, "       -tape-disable        don't patch INOUT to LOAD/SAVE/VERIFY .mtx files\n");
#ifdef HAVE_SPEC
	fprintf(stderr, "       -tap-file fn         specify ZX tape file (default memu.tap)\n");
	fprintf(stderr, "       -sna-file fn         specify ZX snapshot file (default memu.sna)\n");
#endif
	fprintf(stderr, "       -cassette-in         hardware emulation - MTX or WAV file to load\n");
	fprintf(stderr, "       -cassette-out        hardware emulation - MTX or WAV file to save\n");
#ifdef HAVE_OSFS
	fprintf(stderr, "       -cpm                 emulate CP/M BDOS\n");
	fprintf(stderr, "       -cpm-drive-a path    where CP/M BDOS finds A: files (default: .)\n");
	fprintf(stderr, "       -cpm-invert-case     invert between CP/M and host filenames\n");
	fprintf(stderr, "       -cpm-tail tail       construct CP/M command tail\n");
	fprintf(stderr, "       -cpm-open-hack       don't insist on EX,S1,S2,RC being 0\n");
	fprintf(stderr, "       -sdx                 SDX support in ROM 5 (or -sdx3 for ROM 3)\n");
	fprintf(stderr, "       -fdxb                FDXB CP/M support\n");
#endif
#ifdef HAVE_NFX
	fprintf(stderr, "       -nfx-port-offset off offset to add to NFX port numbers\n");
#endif
	fprintf(stderr, "       -speed hz            set CPU speed (default is 4000000, ie: 4MHz)\n");
	fprintf(stderr, "       -fast                don't limit speed, run as fast as possible\n");
	/*
	fprintf(stderr, "       -ui-mem-title        set title for memory window\n");
	fprintf(stderr, "       -ui-mem-display      set display to use for memory window\n");
	fprintf(stderr, "       -ui-vram-title       set title for video memory window\n");
	fprintf(stderr, "       -ui-vram-display     set display to use for video memory window\n");
	fprintf(stderr, "       -ui-dis-title        set title for disassembly window\n");
	fprintf(stderr, "       -ui-dis-display      set display to use for disassembly window\n");
	*/
#ifndef SMALL_MEM
	fprintf(stderr, "       file.com tail ...    -cpm -iobyte 0x80 -addr 0x0100 -mem file.com\n");
	fprintf(stderr, "       file.run             -iobyte 0x00 -addr 0xAAAA (from header)\n");
#endif
	fprintf(stderr, "       file.mtx             subsequent LOAD/SAVE/VERIFY \"\" will use this file\n");

    if ( psErr )
        {
        fprintf (stderr, "\nError: ");
        va_list va;
        va_start (va, psErr);
        vfprintf (stderr, psErr, va);
        va_end (va);
        fprintf (stderr, "\n");
        }
#ifdef ALT_EXIT
	ALT_EXIT(2);
#else
	exit(2);
#endif
	}
/*...e*/
static BOOLEAN bIgnore = FALSE;

void unimplemented (const char *psErr)
    {
    if ( bIgnore ) diag_message (DIAG_ALWAYS, "Feature %s is not implemented in this version", psErr);
    else usage ("Feature %s is not implemented in this version", psErr);
    }

void opterror (const char *psOpt)
    {
    usage ("At option %s", psOpt);
    }

/*...sread_file:0:*/
int read_file(const char *fn, byte *buf, int buflen)
	{
	FILE *fp = efopen(fn, "rb");
	long length;
	fseek(fp, 0, SEEK_END);
	length = (long) ftell(fp);
	if ( length > (long) buflen )
		{
		fclose(fp);
		fatal("%s is %ld bytes and can only be %d bytes at most", fn, length, buflen);
		}
	fseek(fp, 0, SEEK_SET);
	fread(buf, 1, (size_t) length, fp);
	fclose(fp);
	return (int) length;
	}
/*...e*/
/*...sread_file_path:0:*/
/* Honor the path, if specified */
static int read_file_path(const char *fn, byte *buf, int buflen, const char *path)
	{
	if ( path != NULL )
		{
		char *full_fn = emalloc(strlen(path)+1+strlen(fn)+1);
		int n;
		sprintf(full_fn, "%s/%s", path, fn);
		n = read_file(full_fn, buf, buflen);
		free(full_fn);
		return n;
		}
	else
		return read_file(fn, buf, buflen);
	}
/*...e*/
/*...sget_word:0:*/
static word get_word(byte *p)
	{
	return (word) p[0] + (((word)p[1])<<8);
	}
/*...e*/
/*...sput_word:0:*/
static void put_word(byte *p, word w)
	{
	p[0] = (byte)  w    ;
	p[1] = (byte) (w>>8);
	}
/*...e*/

/*...svars:0:*/
CFG cfg;

static Z80 z80;
static BOOLEAN moderate_speed = TRUE;
// static BOOLEAN panel_hack = FALSE;

static byte *run_buf = NULL;
static word run_hdr_base;
static word run_hdr_length;

#ifdef SMALL_MEM
FILE *fp_tape = NULL;
static byte tape_buf[512];
#else
static byte tape_buf[0xfff0];
#endif
static word tape_len;

static byte last_trace = TRUE;
static byte no_trace[0x10000>>3] = { 0 };

static BOOLEAN loadmtx_hack = FALSE;
/*...e*/

#ifdef SMALL_MEM
static BOOLEAN bTapePatch = TRUE;
#endif

void tape_patch (BOOLEAN bPatch)
    {
#ifdef SMALL_MEM
    bTapePatch = bPatch;
#else
    byte b = mem_get_iobyte();
    mem_set_iobyte(0x00);
    if ( bPatch )
        {
        diag_message (DIAG_TAPE, "Patch tape I/O code");
        mem_write_byte(0x0aae, 0xed);   // Call PatchZ80
        mem_write_byte(0x0aaf, 0xfe);
        mem_write_byte(0x0ab0, 0xc9);
        }
    else
        {
        diag_message (DIAG_TAPE, "Use CTC for tape save / load");
        mem_write_byte(0x0aae, 0x7a);   // ld a,d
        mem_write_byte(0x0aaf, 0xb3);   // or e
        mem_write_byte(0x0ab0, 0xc8);   // ret z
        }
    mem_set_iobyte(b);
#endif
    }

/*...s\46\mtx files:0:*/
/* Logic for implementing the MTX LOAD command
   is taken from MacTX by Per Persson.
   Fixed to use CALCST from Paul Daniels.
   Logic for SAVE and VERIFY added in a similar fashion. */

/*...sget_tape_name:0:*/
static const char *get_tape_name(word addr, const char *fn_prefix, char *fn_buf)
	{
	int i;
	char *p = fn_buf;
	if ( fn_prefix != NULL )
		{
		strcpy(p, PMapPath (fn_prefix));
		strcat(p, "/");
        // printf ("Tape prefix: %s\n", fn_buf);
		p += strlen(fn_buf);
		}
	for ( i = 0; i < 15; i++ )
		p[i] = mem_read_byte(addr+i);
	while ( i > 0 && p[i-1] == ' ' )
		--i;
    // printf ("MTX name length = %d\n", i);
	if ( i > 0 )
        {
		strcpy(p+i, ".mtx");
        }
	else if ( cfg.tape_fn != NULL )
        {
        // printf ("cfg.tape_fn = %s\n", cfg.tape_fn);
		strcpy(p, cfg.tape_fn);
        }
	else
        {
		strcpy(p, "default.mtx");
        }
    // printf ("fn_buf = %s\n", fn_buf);
	return fn_buf;
	}
/*...e*/

#define	MAX_TAPE_PREFIX 300
static char tape_name_fn_buf[MAX_TAPE_PREFIX+15+1+3+1];
static const char *tape_name_fn;

void hexdump (byte *ptr, int n);

static void mtx_tape(Z80 *r)
	{
	word base   = r->HL.W;
	word length = r->DE.W;
	word calcst = mem_read_byte(0xfa81) + mem_read_byte(0xfa82) * 256;
    // printf ("mtx_tape: base = 0x%04X, length = %d\n", base, length);
	if ( mem_read_byte(0xfd68) == 0 )
		/* SAVE */
		{
		FILE *fp;
		if ( base == calcst && length == 20 )
			{
			tape_name_fn = get_tape_name(calcst+1, cfg.tape_name_prefix, tape_name_fn_buf);
			diag_message(DIAG_TAPE, "SAVE fn=%s", tape_name_fn);
			if ( !cfg.tape_overwrite )
				{
				if ( (fp = fopen(tape_name_fn, "rb")) != NULL )
					{
					fclose(fp);
					fatal("not allowed to overwrite file %s", tape_name_fn);
					}
				}
			remove(tape_name_fn);
			length -= 2; /* Work around a bug in the MTX ROM */
			}
		diag_message(DIAG_TAPE, "SAVE base=0x%04x length=0x%04x iobyte=0x%02x", base, length, mem_get_iobyte());
#ifdef SMALL_MEM
		if ( (fp = fopen(tape_name_fn, "ab+")) == NULL )
			fatal("can't append to file %s", tape_name_fn);
		while ( length > 0 )
		    {
		    word len;
		    byte *ptr = mem_ram_ptr (base, &len);
		    if ( len > length ) len = length;
		    fwrite (ptr, 1, len, fp);
		    base += len;
		    length -= len;
		    }
		fclose(fp);
#else
		if ( length > sizeof(tape_buf) )
			fatal("attempt to SAVE 0x%04x byte chunk to tape", length);
		mem_read_block(base, length, tape_buf);
		if ( (fp = fopen(tape_name_fn, "ab+")) == NULL )
			fatal("can't append to file %s", tape_name_fn);
		fwrite(tape_buf, 1, (size_t) length, fp);
		fclose(fp);
#endif
		}
	else if ( mem_read_byte(0xfd67) != 0 )
		/* VERIFY.
		   Normally, if verification fails, the MTX BASIC ROM
		   stops the tape, cleans up and does rst 0x28.
		   That rst instruction is at 0x0adb. */
		{
		if ( base == 0xc011 && length == 18 )
			{
			tape_name_fn = get_tape_name(0xc002, cfg.tape_name_prefix, tape_name_fn_buf);
			diag_message(DIAG_TAPE, "VERIFY fn=%s", tape_name_fn);
#ifdef SMALL_MEM
			if ( fp_tape != NULL ) fclose (fp_tape);
			fp_tape = efopen (tape_name_fn, "rb");
#else
			tape_len = read_file(tape_name_fn, tape_buf, sizeof(tape_buf));
#endif
			}
		/* Then verify chunks as requested */
		diag_message(DIAG_TAPE, "VERIFY base=0x%04x length=0x%04x iobyte=0x%02x", base, length, mem_get_iobyte());
#ifdef SMALL_MEM
		size_t l1 = 0;
		byte *b1 = NULL;
		word l2 = 0;
		byte *b2 = NULL;
		while ( length > 0 )
			{
			if ( l1 == 0 )
				{
				b1 = tape_buf;
				l1 = fread (tape_buf, 1, sizeof (tape_buf), fp_tape);
				if ( l1 < sizeof (tape_buf) )
					{
					fclose (fp_tape);
					fp_tape = NULL;
					if ( l1 < length )
						{
						r->PC.W = 0x0adb;
						break;
						}
					}
				}
			if ( l2 == 0 )
				{
				b2 = mem_ram_ptr (base, &l2);
				}
			size_t l3 = l1;
			if ( l2 < l3 ) l3 = l2;
			if ( memcmp (b1, b2, l3) )
				{
				r->PC.W = 0x0adb;
				break;
				}
			l1 -= l3;
			l2 -= l3;
			length -= l3;
			b1 += l3;
			b2 += l3;
			base += l3;
			}
#else
		if ( length > tape_len )
			r->PC.W = 0x0adb;
		else
			{
			int i;
			for ( i = 0; i < length; i++ )
				if ( tape_buf[i] != mem_read_byte(base+i) )
					{
					r->PC.W = 0x0adb;
					break;
					}
			tape_len -= length;
			memmove(tape_buf, tape_buf+length, tape_len);
			}
#endif
		}
	else
		/* LOAD */
		{
		if ( base == 0xc011 && length == 18 )
			/* Load header, so read whole file */
			{
			tape_name_fn = get_tape_name(0xc002, cfg.tape_name_prefix, tape_name_fn_buf);
			diag_message(DIAG_TAPE, "LOAD fn=%s", tape_name_fn);
#ifdef SMALL_MEM
			if ( fp_tape != NULL ) fclose (fp_tape);
			fp_tape = efopen (tape_name_fn, "rb");
#else
			tape_len = read_file(tape_name_fn, tape_buf, sizeof(tape_buf));
#endif
			}
		/* Then return chunks as requested */
		diag_message(DIAG_TAPE, "LOAD base=0x%04x length=0x%04x iobyte=0x%02x", base, length, mem_get_iobyte());
#ifdef SMALL_MEM
		while ( length > 0 )
			{
		    word len;
		    byte *ptr = mem_ram_ptr (base, &len);
		    if ( len > length ) len = length;
		    if ( fread (ptr, 1, len, fp_tape) < len )
				fatal("attempt to LOAD 0x%04x byte chunk from tape and only 0x%04x remaining",
					length, tape_len);
            // printf ("0x%04X:", base);
            // hexdump (ptr, ( len > 20 ) ? 20 : len);
		    base += len;
		    length -= len;
			}
#else
		if ( length > tape_len )
			fatal("attempt to LOAD 0x%04x byte chunk from tape and only 0x%04x remaining", length, tape_len);
		mem_write_block(base, length, tape_buf);
		tape_len -= length;
		memmove(tape_buf, tape_buf+length, tape_len);
#endif
		}
	/* The real routine disables interrupts */
	ctc_out(0, 0xf0);
	ctc_out(0, 0x03);
	ctc_out(1, 0x03);
	ctc_out(2, 0x03);
	ctc_out(3, 0x03);
	vid_clear_int();
	/* Then re-enables the video interrupt */
	ctc_out(0, 0xa5);
	ctc_out(0, 0x7d);
	}
/*...e*/
/*...s\46\tap tapes:0:*/
/* ZX Spectrum .tap format.
   See http://www.zxmodules.de/fileformats/tapformat.html */

#define	MAX_TAP_LENGTH 0xc000

static const char *tap_fn = "memu.tap";
static long tap_ptr = 0L;

/*...sxor_buf:0:*/
static byte xor_buf(const byte *b, int len)
	{
	byte x = 0;
	while ( len-- )
		x ^= *b++;
	return x;
	}
/*...e*/

#ifdef HAVE_SPEC
/*...stap_load_block:0:*/
/* In the wild, we find .tap files which have blocks bigger than what
   the program wants to load, usually only by a few bytes.
   We also find complete blocks that need to be skipped,
   usually unwanted headers.
   We signal we found the wrong kind of block, or too short,
   by clearing the carry flag.
   The block lengths displayed by this function are without the
   1 byte flag before the data and 1 byte xor sum afterwards. */
static void tap_load_block(word base, word length, byte flag, byte *f)
	{
	byte *buf = emalloc(2+1+MAX_TAP_LENGTH+1);
	FILE *fp;
	word length_block;
	byte x;
	diag_message(DIAG_TAPE, "TAP LOAD base=0x%04x length=0x%04x flag=0x%02x", base, length, flag);
	if ( length > MAX_TAP_LENGTH )
		{
		diag_message(DIAG_TAPE, "TAP LOAD length can't be > 0x%04x, returning error", MAX_TAP_LENGTH);
		free(buf);
		*f &= ~C_FLAG;
		return;
		}
	if ( (fp = fopen(tap_fn, "rb")) == NULL )
		{
		diag_message(DIAG_TAPE, "TAP LOAD can't open .tap file %s, returning error", tap_fn);
		free(buf);
		*f &= ~C_FLAG;
		return;
		}
	fseek(fp, tap_ptr, SEEK_SET);
	if ( ftell(fp) != tap_ptr )
		{
		diag_message(DIAG_TAPE, "TAP LOAD can't seek in .tap file %s, returning error", tap_fn);
		fclose(fp);
		free(buf);
		*f &= ~C_FLAG;
		return;
		}
	if ( fread(buf, 1, 2+1, fp) != 2+1 )
		{
		diag_message(DIAG_TAPE, "TAP LOAD can't read start of block in .tap file %s, returning error", tap_fn);
		fclose(fp);
		free(buf);
		*f &= ~C_FLAG;
		return;
		}
	length_block = get_word(buf);
		/* This includes 1 byte type before and 1 byte xor sum after */
	if ( length_block < 1+1 )
		/* We simply don't cope with these at all */
		{
		diag_message(DIAG_TAPE, "TAP LOAD found unusually short block length=0x%04x, returning error", length_block-2);
		fclose(fp);
		free(buf);
		tap_ptr += (2+length_block);
		*f &= ~C_FLAG; /* Error */
		return;
		} 
	if ( buf[2] != flag )
		{
		diag_message(DIAG_TAPE, "TAP LOAD found wrong block type length=0x%04x flag=0x%02x, returning error", length_block-2, buf[2]);
		fclose(fp);
		free(buf);
		tap_ptr += (2+length_block);
		*f &= ~C_FLAG; /* Error */
		return;
		} 
	if ( length_block < (word) 1+length+1 )
		{
		diag_message(DIAG_TAPE, "TAP LOAD found short block length=0x%04x type=0x%02x, returning error", length_block-2, buf[2]);
		fclose(fp);
		free(buf);
		tap_ptr += (2+length_block);
		*f &= ~C_FLAG; /* Error */
		return;
		} 
	if ( length_block > 1+MAX_TAP_LENGTH+1 )
		{
		diag_message(DIAG_TAPE, "TAP LOAD found excessively long block length=0x%04x type=0x%02x, returning error", length_block-2, buf[2]);
		fclose(fp);
		free(buf);
		tap_ptr += (2+length_block);
		*f &= ~C_FLAG; /* Error */
		return;
		} 
	if ( length_block > 1+length+1 )
		diag_message(DIAG_TAPE, "TAP LOAD only returning first 0x%04x bytes from block of length 0x%04x",
			length, length_block-2);
	if ( fread(buf+2+1, 1, (size_t)(length_block-1), fp) != length_block-1 )
		{
		diag_message(DIAG_TAPE, "TAP LOAD can't read rest of block in .tap file %s, returning error", tap_fn);
		fclose(fp);
		free(buf);
		tap_ptr += (2+length_block);
		*f &= ~C_FLAG; /* Error */
		return;
		} 
	x = xor_buf(buf+2, length_block-1);
	if ( x != buf[2+length_block-1] )
		/* We give lots of detail here so that I can patch .tap
		   files and try to load them, and then fix the checksum. */
		{
		diag_message(DIAG_TAPE, "TAP LOAD data actually checksums to 0x%02x but checksum in .tap file is 0x%02x, returning error", x, buf[2+length_block-1]);
		fclose(fp);
		free(buf);
		tap_ptr += (2+length_block);
		*f &= ~C_FLAG; /* Error */
		return;
		}
	tap_ptr += (2+length_block);
	fclose(fp);
	if ( *f & C_FLAG )
		/* LOAD */
		mem_write_block(base, length, buf+2+1);
#if 0
		/* You might think that we need to do this, but Speculator
		   loads to buffers below 0x4000, and at least one game
		   loads a block stradding below and above 0x4000. */
		{
		int i;
		for ( i = 0; i < length; i++ )
			if ( (word)(base+i) >= 0x4000 )
				mem_write_byte(base+i, buf[2+1+i]);
		}
#endif
	else
		/* VERIFY */
		{
		int i;
		*f |= (C_FLAG|Z_FLAG);
		for ( i = 0; i < length; i++ )
			if ( buf[2+1+i] != mem_read_byte(base+i) )
				{
				*f &= ~(C_FLAG|Z_FLAG);
				break;
				}
		}
	free(buf);
	}
/*...e*/
/*...stap_save_block:0:*/
static void tap_save_block(word base, word length, byte flag)
	{
	byte *buf = emalloc(2+1+MAX_TAP_LENGTH+1);
	FILE *fp;
	diag_message(DIAG_TAPE, "TAP SAVE base=0x%04x length=0x%04x flag=0x%02x", base, length, flag);
	if ( length > MAX_TAP_LENGTH )
		fatal(".tap save tape request for 0x%04x bytes", length);
	if ( (fp = fopen(tap_fn, "ab+")) == NULL )
		fatal("can't append to .tap file %s", tap_fn);
	put_word(buf, 1+length+1);
	buf[2] = flag;
	mem_read_block(base, length, buf+3);
	buf[2+1+length] = xor_buf(buf+2, 1+length);
	if ( fwrite(buf, 1, (size_t) (2+1+length+1), fp) != 2+1+length+1 )
		fatal("TAP SAVE can't write file %s", tap_fn);
	fclose(fp);
	free(buf);
	}
/*...e*/

/* Entrypoints suitable for Spectrum ROM 0x0556 and 0x04c6, or Z emulator */

static void tap_load(Z80 *r)
	{
	tap_load_block(r->IX.W, r->DE.W, r->AF.B.h, &(r->AF.B.l));
	}

static void tap_save(Z80 *r)
	{
	tap_save_block(r->IX.W, r->DE.W, r->AF.B.h);
	}

/* Entrypoints suitable for Speculator 0x0738,0x0804 and 0x06a2 */

static void spec_load(Z80 *r)
	{
	byte f = C_FLAG; /* Always load */
	/* Some Speculator code appears to be asking for A 0x74 or 0x7b.
	   Yet to find a .tap file with that in.
	   Not sure what to do with this at this time. */
	tap_load_block(r->BC.W, r->DE.W, r->AF.B.h, &f);
	r->AF.B.h = 0; /* Success */
	}

static void spec_save(Z80 *r)
	{
	/* @@@ UNTESTED */
	tap_save_block(r->BC.W, r->DE.W, r->AF.B.h);
	}

/* Manually invoked */

static void tap_rewind(void)
	{
	tap_ptr = 0L;
	diag_message(DIAG_ALWAYS, "TAP rewind");
	}
/*...e*/
/*...s\46\sna snapshots:0:*/
/* ZX Spectrum .sna format (48KB variant only).
   See http://www.worldofspectrum.org/faq/reference/formats.htm */

#define	SNA_HEADER 27
#define	SNA_BASE 0x4000
#define	SNA_DUMP 0xc000
#define	SNA_SIZE (SNA_HEADER+SNA_DUMP)

static const char *sna_fn = "memu.sna";

static void sna_load(Z80 *r)
	{
	byte *buf;
	word sp, pc;
	if ( mem_get_iobyte() != 0x80 )
		{
		diag_message(DIAG_ALWAYS, "snapshot cannot be loaded when IOBYTE is not 0x80");
		return;
		}
	buf = emalloc(SNA_SIZE);
	if ( read_file(sna_fn, buf, SNA_SIZE) != SNA_SIZE )
		{
		free(buf);
		diag_message(DIAG_ALWAYS, "snapshot file %s is invalid, wrong size", sna_fn);
		return;
		}
	sp = get_word(buf+23);
	if ( sp < SNA_BASE || sp >= (SNA_BASE+SNA_DUMP-1) )
		{
		free(buf);
		diag_message(DIAG_ALWAYS, "snapshot file %s is invalid, SP=0x%04x", sna_fn, sp);
		return;
		}
	pc = get_word(buf+SNA_HEADER+sp-SNA_BASE);
	if ( pc == 0x0038 )
		; /* Probably saved by fuse emulator, tolerate this */
	else if ( pc < SNA_BASE )
		{
		free(buf);
		diag_message(DIAG_ALWAYS, "snapshot file %s cannot be loaded, PC=0x%04x", sna_fn, pc);
		return;
		}
	mem_write_block(SNA_BASE, SNA_DUMP, buf+SNA_HEADER);
	r->I = buf[0];
	r->HL1.W = get_word(buf+ 1);
	r->DE1.W = get_word(buf+ 3);
	r->BC1.W = get_word(buf+ 5);
	r->AF1.W = get_word(buf+ 7);
	r->HL .W = get_word(buf+ 9);
	r->DE .W = get_word(buf+11);
	r->BC .W = get_word(buf+13);
	r->IY .W = get_word(buf+15);
	r->IX .W = get_word(buf+17);
	if ( buf[19] & 0x04 ) /* IFF2 flag, which RETN would copy to IFF1 */
		r->IFF |=  0x41;
	else
		r->IFF &= ~0x41;
	/* don't restore R from buf[20] */
	r->AF .W = get_word(buf+21);
	r->PC .W = pc;
	mem_write_byte(sp++, 0x00);
	mem_write_byte(sp++, 0x00);
	r->SP .W = sp;
	switch ( buf[25] ) /* IntMode */
		{
		case 0: r->IFF &= ~0x06;                 break;
		case 1: r->IFF &= ~0x06; r->IFF |= 0x02; break;
		case 2: r->IFF &= ~0x06; r->IFF |= 0x04; break;
		}		
	spec_outFE(0xf8|buf[26]); /* BorderColor */
	free(buf);
	diag_message(DIAG_ALWAYS, "snapshot loaded");

	/* Its highly likely we'll have interrupted the NMI code
	   before it has reprogrammed the CTC for the next interrupt.
	   As it won't ever get to that point, we do it here. */
	ctc_out(0, 0xc5);
	ctc_out(0, 0x01);
	}

static void sna_save(Z80 *r)
	{
	byte *buf;
	word sp = r->SP.W;
	FILE *fp;
	if ( mem_get_iobyte() != 0x80 )
		{
		diag_message(DIAG_ALWAYS, "snapshot cannot be saved when IOBYTE is not 0x80");
		return;
		}
	buf = emalloc(SNA_SIZE);
	buf[0] = r->I;
	put_word(buf+ 1, r->HL1.W);
	put_word(buf+ 3, r->DE1.W);
	put_word(buf+ 5, r->BC1.W);
	put_word(buf+ 7, r->AF1.W);
	put_word(buf+ 9, r->HL .W);
	put_word(buf+11, r->DE .W);
	put_word(buf+13, r->BC .W);
	put_word(buf+15, r->IY .W);
	put_word(buf+17, r->IX .W);
	if ( r->IFF & 0x01 )
		buf[19] = 0x04;
	else
		buf[19] = 0x00;
	buf[20] = ( (-r->ICount&0xFF) &0x7F ); /* This is how Z80 computes it */
	put_word(buf+21, r->AF .W);
	put_word(buf+23, sp-2);
	switch ( r->IFF & 0x06 )
		{
		case 0x00: buf[25] = 0; break;
		case 0x02: buf[25] = 1; break;
		case 0x04: buf[25] = 2; break;
		}
	buf[26] = ( spec_in7E() & 0x07 ); /* BorderColor */
	mem_read_block(SNA_BASE, SNA_DUMP, buf+SNA_HEADER);
	put_word(buf+SNA_HEADER+sp-2-SNA_BASE, r->PC.W);
	if ( (fp = fopen(sna_fn, "wb")) == NULL )
		{
		free(buf);
		diag_message(DIAG_ALWAYS, "snapshot file %s cannot opened for writing", sna_fn);
		return;
		}
	if ( fwrite(buf, 1, SNA_SIZE, fp) != SNA_SIZE )
		{
		free(buf);
		fclose(fp);
		remove(sna_fn);
		diag_message(DIAG_ALWAYS, "snapshot file %s cannot be written", sna_fn);
		return;
		}		
	fclose(fp);
	free(buf);
	diag_message(DIAG_ALWAYS, "snapshot saved");
	}
/*...e*/
#endif  // HAVE_SPEC

#ifdef HAVE_OSFS
/*...ssetup_sdx:0:*/
static void setup_sdx(int rom)
	{
	byte b = mem_get_iobyte();
	mem_set_iobyte(rom<<4);

	/* Copy the ROM into place */
	mem_write_block(0x2000, 0x2000, rom_sdx);

	/* During system initialisation, or after ROM 3/5, init_rom is called.
	   Ensure init_rom doesn't call fdsc_initlz */
	mem_write_byte(0x21a0, 0x00);
	mem_write_byte(0x21a1, 0x00);
	mem_write_byte(0x21a2, 0x00);

	/* When running a USER command, user_command is called.
	   If BDOS isn't initialised, init_bdos is called.
	   init_bdos calls init_bdos_initlz.
	   Ensure init_bdos_initlz doesn't call fdsc_initlz. */
	mem_write_byte(0x20bd, 0x00);
	mem_write_byte(0x20be, 0x00);
	mem_write_byte(0x20bf, 0x00);
	/*  Ensure init_bdos doesn't call fdsc_rwgo. */
	mem_write_byte(0x203d, 0x00);
	mem_write_byte(0x203e, 0x00);
	mem_write_byte(0x203f, 0x00);
	/* It then calls init_bdos_initlz.
	   init_bdos It then reads 35 blocks from track 0 to 0xd700.
	   Ensure init_bdos doesn't call
	     fdsc_blkrd
	     init_bdos_loaded
	     init_bdos_patch
	     init_bdos_patch2
	   and replace it with code to place our magic instruction. */
	mem_write_byte(0x2048, 0x21); /* LD HL,#D706 */
	mem_write_byte(0x2049, 0x06);
	mem_write_byte(0x204a, 0xd7);
	mem_write_byte(0x204b, 0x36); /* LD (HL),#ED */
	mem_write_byte(0x204c, 0xed);
	mem_write_byte(0x204d, 0x23); /* INC HL */
	mem_write_byte(0x204e, 0x36); /* LD (HL),#FE */
	mem_write_byte(0x204f, 0xfe);
	mem_write_byte(0x2050, 0x23); /* INC HL */
	mem_write_byte(0x2051, 0x36); /* LD (HL),#C9 */
	mem_write_byte(0x2052, 0xc9);
	mem_write_byte(0x2053, 0x00); /* NOP */
	
	/* Hook in our CP/M BDOS support */
	cpm_init_sdx();

	/* Ensure that USER SYSCOPY is safe */
	mem_write_byte(0x321e, 0xef); /* RST #28 */
	mem_write_byte(0x321f, 0x01); /* "Mistake" */

	/* Ensure that USER FORMAT is safe */
	mem_write_byte(0x33d8, 0xef); /* RST #28 */
	mem_write_byte(0x33d9, 0x01); /* "Mistake" */

	/* Ensure that USER STAT is safe */
	mem_write_byte(0x3983, 0xef); /* RST #28 */
	mem_write_byte(0x3984, 0x01); /* "Mistake" */

	mem_set_iobyte(b);
	}
/*...e*/
#endif

/*...sbememu:0:*/
/* This is a special backdoor into MEMU memory.
   It relies on UNIX / POSIX style file I/O to FIFOs.
   Or on Windows, it uses named pipes.
   For use by a Andys Binary Folding Editor memory extension. */

#if defined(BEMEMU)

#if defined(UNIX)
static char fn_be_in[100+1];
static char fn_be_out[100+1];
static int fd_be_in  = -1;
static int fd_be_out = -1;
#elif defined(WIN32)
static HANDLE hf_be = INVALID_HANDLE_VALUE;
#endif

/*...svdp_read\47\write:0:*/
static byte vdp_read(word addr)
	{
	if ( addr < VID_MEMORY_SIZE )
		return vid_vram_read(addr);
	else if ( addr < VID_MEMORY_SIZE+8 )
		return vid_reg_read(addr-VID_MEMORY_SIZE);
	else if ( addr == VID_MEMORY_SIZE+8 )
		return vid_status_read();
	else
		return 0xff;
	}
static void vdp_write(word addr, byte b)
	{
	if ( addr < VID_MEMORY_SIZE )
		vid_vram_write(addr, b);
	else if ( addr < VID_MEMORY_SIZE+8 )
		vid_reg_write(addr-VID_MEMORY_SIZE, b);
	}
/*...e*/

static void be_poll(void)
	{
	byte cmd;
	byte buf[0x100];
#if defined(UNIX)
	if ( fd_be_in != -1 )
		while ( read(fd_be_in, &cmd, 1) == 1 )
#elif defined(WIN32)
	DWORD cb;
	if ( hf_be != INVALID_HANDLE_VALUE )
		while ( ReadFile(hf_be, &cmd, 1, &cb, NULL) == 1 )
			{
			DWORD dwMode = PIPE_READMODE_BYTE | PIPE_WAIT;
			SetNamedPipeHandleState(hf_be, &dwMode, NULL, NULL);
#endif
			switch ( cmd )
				{
/*...s0x00 \45\ read bytes:32:*/
case 0x00:
	{
	byte iobyte, b;
	word addr;
	word len;
#if defined(UNIX)
	read(fd_be_in, buf, 5);
#elif defined(WIN32)
	ReadFile(hf_be, buf, 5, &cb, NULL);
#endif
	iobyte = buf[0];
	addr = get_word(buf+1);
	len = get_word(buf+3);
	b = mem_get_iobyte();
	mem_set_iobyte(iobyte);
	while ( len > 0 )
		{
		word thisgo = ( len > (word)sizeof(buf) ) ? (word)sizeof(buf) : len;
		mem_read_block(addr, thisgo, buf);
#if defined(UNIX)
		write(fd_be_out, buf, thisgo);
#elif defined(WIN32)
		WriteFile(hf_be, buf, thisgo, &cb, NULL);
#endif
		addr += thisgo;
		len  -= thisgo;
		}
	mem_set_iobyte(b);
	}
	break;
/*...e*/
/*...s0x01 \45\ write bytes:32:*/
case 0x01:
	{
	byte iobyte, b;
	word addr;
	word len;
#if defined(UNIX)
	read(fd_be_in, buf, 5);
#elif defined(WIN32)
	ReadFile(hf_be, buf, 5, &cb, NULL);
#endif
	iobyte = buf[0];
	addr = get_word(buf+1);
	len = get_word(buf+3);
	b = mem_get_iobyte();
	mem_set_iobyte(iobyte);
	while ( len > 0 )
		{
		word thisgo = ( len > (word)sizeof(buf) ) ? (word)sizeof(buf) : len;
#if defined(UNIX)
		read(fd_be_in, buf, thisgo);
#elif defined(WIN32)
		ReadFile(hf_be, buf, thisgo, &cb, NULL);
#endif
		mem_write_block(addr, thisgo, buf);
		addr += thisgo;
		len  -= thisgo;
		}
	mem_set_iobyte(b);
	cmd = 0; /* Dummy byte */
#if defined(UNIX)
	write(fd_be_out, &cmd, 1);
#elif defined(WIN32)
	WriteFile(hf_be, &cmd, 1, &cb, NULL);
#endif
	}
	break;
/*...e*/
/*...s0x02 \45\ read bytes from VDP:32:*/
case 0x02:
	{
	word addr;
	word len;
#if defined(UNIX)
	read(fd_be_in, buf, 4);
#elif defined(WIN32)
	ReadFile(hf_be, buf, 4, &cb, NULL);
#endif
	addr = get_word(buf);
	len = get_word(buf+2);
	while ( len > 0 )
		{
		word thisgo = ( len > (word)sizeof(buf) ) ? (word)sizeof(buf) : len;
		word i;
		for ( i = 0; i < thisgo; i++ )
			buf[i] = vdp_read(addr+i);
#if defined(UNIX)
		write(fd_be_out, buf, thisgo);
#elif defined(WIN32)
		WriteFile(hf_be, buf, thisgo, &cb, NULL);
#endif
		addr += thisgo;
		len  -= thisgo;
		}
	}
	break;
/*...e*/
/*...s0x03 \45\ write bytes to VDP:32:*/
case 0x03:
	{
	word addr;
	word len;
#if defined(UNIX)
	read(fd_be_in, buf, 4);
#elif defined(WIN32)
	ReadFile(hf_be, buf, 4, &cb, NULL);
#endif
	addr = get_word(buf);
	len = get_word(buf+2);
	while ( len-- > 0 )
		{
		byte b;
#if defined(UNIX)
		read(fd_be_in, &b, 1);
#elif defined(WIN32)
		ReadFile(hf_be, &b, 1, &cb, NULL);
#endif
		vdp_write(addr++, b);
		}
	cmd = 0; /* Dummy byte */
#if defined(UNIX)
	write(fd_be_out, &cmd, 1);
#elif defined(WIN32)
	WriteFile(hf_be, &cmd, 1, &cb, NULL);
#endif
	}
	break;
/*...e*/
				}
#if defined(WIN32)
			dwMode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
			SetNamedPipeHandleState(hf_be, &dwMode, NULL, NULL);
			}
#endif
	}

static void be_init(const char *pipe_id)
	{
#if defined(UNIX)
	if ( fd_be_in != -1 )
		close(fd_be_in);
	if ( fd_be_out != -1 )
		close(fd_be_out);
	sprintf(fn_be_in, "/tmp/bememu-cmd-%s", pipe_id);
	remove(fn_be_in);
	if ( mkfifo(fn_be_in, 0666) )
		fatal("can't make FIFO %s", fn_be_in);
	if ( (fd_be_in = open(fn_be_in, O_RDONLY|O_NONBLOCK)) == -1 )
		fatal("can't open FIFO for reading %s", fn_be_in);
	sprintf(fn_be_out, "/tmp/bememu-stat-%s", pipe_id);
	remove(fn_be_out);
	if ( mkfifo(fn_be_out, 0666) )
		fatal("can't make FIFO %s", fn_be_out);
	if ( (fd_be_out = creat(fn_be_out, 0777)) == -1 )
		fatal("can't open FIFO for writing %s", fn_be_out);
#elif defined(WIN32)
	char pipe[100+1];
	DWORD dwMode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
	if ( hf_be != INVALID_HANDLE_VALUE )
		CloseHandle(hf_be);
	sprintf(pipe, "\\\\.\\pipe\\bememu-%s", pipe_id);
	if ( (hf_be = CreateNamedPipe(
		pipe,
		PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE |
		PIPE_READMODE_BYTE |
		PIPE_WAIT,
		1,
		0x200,
		0x200,
		0,
		NULL)) == INVALID_HANDLE_VALUE )
		fatal("can't create named pipe %s", pipe);
	if ( ConnectNamedPipe(hf_be, NULL) )
		; /* Connected fine */
	else if ( GetLastError() == ERROR_PIPE_CONNECTED )
		; /* Client connected between CreateNamedPipe
		     and ConnectNamedPipe, which is also fine */
	else
		fatal("can't open named pipe %s", pipe_id);
	SetNamedPipeHandleState(hf_be, &dwMode, NULL, NULL);
#endif
	}

void bememu_term(void)
	{
#if defined(UNIX)
	if ( fd_be_in != -1 )
		{
		close(fd_be_in);
		remove(fn_be_in);
		}
	if ( fd_be_out != -1 )
		{
		close(fd_be_out);
		remove(fn_be_out);
		}
#elif defined(WIN32)
	if ( hf_be != INVALID_HANDLE_VALUE )
		CloseHandle(hf_be);
#endif
	}

#endif
/*...e*/

/*...sPatchZ80:0:*/
void PatchZ80(Z80 *r)
	{
#ifdef SMALL_MEM
    if (( (mem_get_iobyte() & 0x80) == 0 ) && ( r->PC.W-2 == 0x0AAE ))
        {
        if ( bTapePatch )
            {
            mtx_tape(r);
            }
        else
            {
            // Emulate original instructions
            // ld a,d
            r->AF.B.h = r->DE.B.h;
			// or e
            r->AF.B.h |= r->DE.B.l;
			// ret z
            if ( r->AF.B.h == 0 )
                {
                r->PC.B.l = RdZ80(r->SP.W++);
                r->PC.B.h = RdZ80(r->SP.W++);
                }
            else
                {
                ++r->PC.W;
                }
            }
        }
    else
        fatal("magic instruction unexpectedly encountered at 0x%04x", (word)(r->PC.W-2));
#else
	if ( mem_get_iobyte() & 0x80 )
		{
		if ( cpm_patch_fdxb(r) )
			;
		else if ( cpm_patch(r) )
			;
		else if ( loadmtx_hack && cpm_patch_sdx(r) )
			;
#ifdef HAVE_SPEC
		else if ( r->PC.W-2 == 0xe833 )
			/* Patched version of the Z emulator load routine */
			tap_load(r);
		else if ( r->PC.W-2 == 0xe920 )
			/* Patched version of the Z emulator save routine */
			tap_save(r);
		else if ( r->PC.W-2 == 0x0738 || r->PC.W-2 == 0x0804 )
			/* Patched version of the Speculator load routine */
			spec_load(r);
		else if ( r->PC.W-2 == 0x06a2 )
			/* Patched version of the Speculator save routine */
			spec_save(r);
#endif
		else
			fatal("magic instruction unexpectedly encountered at 0x%04x", (word)(r->PC.W-2));
		}
	else
		{
		if ( cpm_patch_sdx(r) )
			;
		else if ( r->PC.W-2 == 0x3627 && run_buf != NULL )
			/* Hack used to load .RUN file into memory
			   when MTX BASIC has started itself up */
			{
			mem_set_iobyte(0x00);
				/* In a real USER RUN, the IOBYTE
				   reflects the SDX BASIC ROM slot */
			mem_write_byte(0x3627, 0xd3);
			mem_write_byte(0x3628, 0x05);
			mem_write_block(run_hdr_base, run_hdr_length, run_buf+4);
			free(run_buf);
			run_buf = NULL;
			r->IFF&=0xFE; /* Disable interrupts, otherwise the
			                 interrupts set up by MTX BASIC will
			                 continue to happen, and can interfere. */
			r->PC.W = run_hdr_base; /* Jump to the code we loaded */
			vid_reset();
			mem_snapshot();
			}
		else if ( r->PC.W-2 == 0x0AAE )
			mtx_tape(r);
		else
			fatal("magic instruction unexpectedly encountered at 0x%04x", (word)(r->PC.W-2));
		}
#endif
	}
/*...e*/
/*...sLoopZ80:0:*/
/* CTC usage :-

   Channel  Counter Input  Output
   -------  -------------  ------
   0        VPDINT         nothing
   1        4MHz/13        DART serial clock 0 *
   2        4MHz/13        DART serial clock 1 *
   3        cassette       nothing

   The timer input is always 4MHz, to all channels.

   *=> not emulated

*/

/* High speed hardware updating */
void Z80Step (Z80 *r, unsigned int uStep)
	{
	ctc_advance ((int) uStep);
	tape_advance ((int) uStep);
	}

void RaiseInt (const char *psSource)
	{
    diag_message (DIAG_Z80_INTERRUPTS, "Z80 interrupt raised by %s", psSource);
	Z80Int (&z80);
	}

/*...sRetiZ80:0:*/
void RetiZ80(Z80 *R)
	{
	if ( ctc_reti () ) return;
#ifdef HAVE_DART
    dart_reti ();
#endif
	}
/*...e*/
/*...sIntAckZ80:0:*/
BOOLEAN Z80IntAck (Z80 *r, word *pvec)
	{
    byte vec;
    if ( ctc_int_ack (pvec) )
        {
        diag_message (DIAG_Z80_INTERRUPTS, "CTC Interrupt vector: 0x%02X", vec);
        return TRUE;
        }
    else
        {
        diag_message (DIAG_Z80_INTERRUPTS, "Interrupt request gone away");
        }
    return FALSE;
	}
/*...e*/

static unsigned long long clock_speed = 4000000;
#ifdef __Pico__
word LoopZ80(Z80 *r)
    {
    win_handle_events ();
	display_wait_for_frame ();
    ctc_trigger (0);
    return INT_NONE;
    }
#else
static long long ms_last_win_handle_events = 0;
static long long ms_last_mon_refresh_blink = 0;
static long long ms_last_moderate = 0;
static long long ms_last_speed_check = 0;
static unsigned long long elapsed_now = 0;
static unsigned long long elapsed_last_vid_refresh = 0;
#ifdef HAVE_DART
static unsigned long long elapsed_last_dart = 0;
#endif
static unsigned long long elapsed_last_moderate = 0;
static unsigned long long elapsed_last_speed_check = 0;
static BOOLEAN force_moderate = FALSE;
static BOOLEAN nmi_next_time = TRUE;

word LoopZ80(Z80 *r)
	{
	long long ms_now = get_millis();
	int periph_int = -1;
	BOOLEAN vid_int_pending_before;

#if defined(BEMEMU)
	be_poll();
#endif

	/* Diagnostic features */
	if ( diag_flags[DIAG_ACT_Z80_REGS] )
		{
		diag_message(DIAG_ALWAYS, "a=%02x f=%c%c%c%c%c%c bc=%04x de=%04x hl=%04x ix=%04x iy=%04x sp=%04x pc=%04x i=%02x iff=%02x",
			r->AF.B.h,
			(r->AF.B.l & S_FLAG)!=0?'S':'.',
			(r->AF.B.l & Z_FLAG)!=0?'Z':'.',
			(r->AF.B.l & H_FLAG)!=0?'H':'.',
			(r->AF.B.l & P_FLAG)!=0?'P':'.',
			(r->AF.B.l & N_FLAG)!=0?'N':'.',
			(r->AF.B.l & C_FLAG)!=0?'C':'.',
			r->BC.W,
			r->DE.W,
			r->HL.W,
			r->IX.W,
			r->IY.W,
			r->SP.W,
			r->PC.W,
			r->I,
			r->IFF
			);
		diag_flags[DIAG_ACT_Z80_REGS] = FALSE;
		}
	if ( diag_flags[DIAG_ACT_MEM_DUMP] )
		{
		diag_flags[DIAG_MEM_DUMP] = TRUE;
		mem_dump();
		diag_flags[DIAG_MEM_DUMP] = FALSE;
		diag_flags[DIAG_ACT_MEM_DUMP] = FALSE;
		}

	if ( diag_flags[DIAG_ACT_SNA_LOAD] )
		{
		sna_load(r);
		diag_flags[DIAG_ACT_SNA_LOAD] = FALSE;
		}
	if ( diag_flags[DIAG_ACT_SNA_SAVE] &&
	     r->PC.W >= 0x4000             &&
	     r->SP.W >= 0x4002             )
		{
		sna_save(r);
		diag_flags[DIAG_ACT_SNA_SAVE] = FALSE;
		}
	if ( diag_flags[DIAG_ACT_TAP_REWIND] )
		{
		tap_rewind();
		diag_flags[DIAG_ACT_TAP_REWIND] = FALSE;
		}
	
	/* Ensure XWindows is kept happy */
	if ( ms_now - ms_last_win_handle_events > 10 )
		{
        // diag_message (DIAG_ALWAYS, "win_handle_events ()");
		win_handle_events();
		ms_last_win_handle_events = ms_now;
		}

	/* Ensure the cursor (and text) flashes on the monitor screen */
	if ( ms_now - ms_last_mon_refresh_blink > 50 )
		{
		mon_refresh_blink();
		ms_last_mon_refresh_blink = ms_now;
		}

	elapsed_now = r->IElapsed;

	if ( moderate_speed )
		if ( force_moderate || ms_now - ms_last_moderate > 5 )
			{
			unsigned long long elapsed = (elapsed_now-elapsed_last_moderate);
			long ms_elapsed  = (long) (ms_now-ms_last_moderate);
			long ms_realtime = (long) (elapsed/(clock_speed/1000));
			if ( ms_elapsed < ms_realtime )
				if ( ! diag_flags[DIAG_SPEED_UP] )
					delay_millis(ms_realtime-ms_elapsed);
			ms_last_moderate  = ms_now;
			elapsed_last_moderate = elapsed_now;
			force_moderate = FALSE;
			}

	if ( ms_now - ms_last_speed_check > 1000 )
		{
		if ( diag_flags[DIAG_SPEED] )
			{
			unsigned long long elapsed = (elapsed_now-elapsed_last_speed_check);
			long ms = (long) (ms_now-ms_last_speed_check);
			diag_message(DIAG_SPEED, "Speed %4.2lfMHz",
				( ( elapsed * 1000.0 ) / ms ) / 1000000.0
				);
			}
		ms_last_speed_check = get_millis();
			/* We fetch this again as we know the
			   diag_message call is going to take
			   a non-trivial length of time. */
		elapsed_last_speed_check = elapsed_now;
		}

	vid_int_pending_before = vid_int_pending();
	
	if ( elapsed_now - elapsed_last_vid_refresh > clock_speed / cfg.screen_refresh )
		{
		vid_refresh(elapsed_now);
			/* This will cause the frame to be displayed,
			   and thus the F bit in the status register to be
			   set, and also, if IE, VPDINT to be asserted. */
		elapsed_last_vid_refresh = elapsed_now;
		force_moderate = TRUE;

		ui_refresh();
			/* Redraw UI windows 50 or 60 times a second as well. */
		kbd_periodic();
			/* Do keyboard autotype at this rate too. */
#ifdef HAVE_JOY
		joy_periodic();
			/* Do joystick at this rate too. */
#endif
#ifdef HAVE_VGA
        if ( cfg.bVGA ) vga_refresh ();
#endif
		}
	else if ( elapsed_now - elapsed_last_vid_refresh > clock_speed / 300 )
		vid_clear_int(); /* Ensure F bit is clear */

	if ( !vid_int_pending_before && vid_int_pending() )
		ctc_trigger(0);
			/* As the CTC is typically configured with channel 0
			   in counter mode, with a count of 1, this will likely
			   cause an interrupt to be asserted to the Z80. */

	/* Now feed the system clock into all CTC channels */
	/* Now done in Z80 step
	int elapsed = (int) (elapsed_now - elapsed_last_ctc);
	ctc_advance(0, elapsed);
	ctc_advance(1, elapsed);
	ctc_advance(2, elapsed);
	ctc_advance(3, elapsed);
	elapsed_last_ctc = elapsed_now;

	if ( panel_hack )
		{
		diag_message(DIAG_PANEL_HACK, "panelHack FIRED pc=0x%04x ICount=%ld", r->PC.W, (long) r->ICount);
		periph_int = ( ctc_get_int_vector()|0x04 );
		panel_hack = FALSE;
		}
	else */
		{
		periph_int = -1;
#ifdef HAVE_DART
		if ( ! ctc_int_pending() )
            {
			periph_int = dart_int_pending (elapsed_now - elapsed_last_dart);
            elapsed_last_dart = elapsed_now;
            }
#endif
		}

	/* For Speculator */
#ifdef HAVE_SPEC
	if ( spec_getNMI() & NMI_ENABLED )
		{
		if ( nmi_next_time )
			{
			diag_message(DIAG_SPEC_INTERRUPTS, "delayed NMI now");
			periph_int = -2;
			nmi_next_time = FALSE;
			}
		else if ( periph_int != -1 )
			{
			if ( spec_getNMI() & NMI_IMMEDIATE )
				{
				diag_message(DIAG_SPEC_INTERRUPTS, "immediate NMI now");
				periph_int = -2;
				}
			else
				{
				diag_message(DIAG_SPEC_INTERRUPTS, "delayed NMI soon");
				nmi_next_time = TRUE;
				}
			}
		}
#endif
    
	if ( periph_int == -1 )
		return INT_NONE;
	else if ( periph_int == -2 )
		{
		static unsigned long long elapsed_nmi;
		diag_message(DIAG_Z80_INTERRUPTS, "Z80 NMI interrupt, at elapsed=%llu elapsed_diff=%llu",
			elapsed_now,
			elapsed_now-elapsed_nmi
			);
		elapsed_nmi = elapsed_now;
		return INT_NMI;
		}
	else
		{
		static long long elapsed_int[4];
		if ( diag_flags[DIAG_Z80_INTERRUPTS] )
			{
			int i = (periph_int>>1)&3;
			diag_message(DIAG_Z80_INTERRUPTS, "Z80 interrupt, periph=0x%02x at elapsed=%llu elapsed_diff=%llu",
				periph_int,
				elapsed_now,
				elapsed_now-elapsed_int[i]
				);
			elapsed_int[i] = elapsed_now;
			}
		if ( periph_int == INT_NMI )
			/* This is to work around a bug in the Z80 CPU
			   implementation. The peripheral byte can
			   legitimately be 0x66, in which case it would be
			   combined with the I register to make the final
			   vector. However, Z80.c IntZ80() checks for NMI
			   ahead of combining with I.
			   The MTX Videowall code happens to trigger this
			   bug by accident/fluke. */
			{
			diag_message(DIAG_Z80_INTERRUPTS, "Z80 interrupt, Z80.c IntZ80() bug workaround, suppressing interrupt");
			periph_int = INT_NONE;
			}
		return periph_int;
		}
	}
/*...e*/
#endif

#ifdef Z80_DEBUG
/*...sDebugZ80:0:*/
/*...sDebugZ80Instruction:0:*/
/* Note: P and V flags are one and the same. */

static unsigned long long elapsed_last_log = 0;

static void DebugZ80Instruction(Z80 *r, const char *instruction)
	{
	char t_prefix[40+1];
	if ( diag_flags[DIAG_Z80_TIME] )
		{
		unsigned long long since = r->IElapsed-elapsed_last_log;
		if ( since < 30 )
			sprintf(t_prefix, "+%2lldT %12lluT: ",
				since,
				r->IElapsed
				);
		else
			sprintf(t_prefix, "     %12lluT: ",
				r->IElapsed
				);
		if ( diag_flags[DIAG_Z80_TIME_IPERIOD] )
			sprintf(t_prefix+strlen(t_prefix)," %4d: ", r->ICount);
		elapsed_last_log = r->IElapsed;
		}
	else
		t_prefix[0] = '\0';
	diag_message(DIAG_Z80_INSTRUCTIONS, "%sa=%02x f=%c%c%c%c%c%c bc=%04x de=%04x hl=%04x ix=%04x iy=%04x sp=%04x pc=%04x i=%02x iff=%02x  %s",
		t_prefix,
		r->AF.B.h,
		(r->AF.B.l & S_FLAG)!=0?'S':'.',
		(r->AF.B.l & Z_FLAG)!=0?'Z':'.',
		(r->AF.B.l & H_FLAG)!=0?'H':'.',
		(r->AF.B.l & P_FLAG)!=0?'P':'.',
		(r->AF.B.l & N_FLAG)!=0?'N':'.',
		(r->AF.B.l & C_FLAG)!=0?'C':'.',
		r->BC.W,
		r->DE.W,
		r->HL.W,
		r->IX.W,
		r->IY.W,
		r->SP.W,
		r->PC.W,
		r->I,
		r->IFF,
		instruction
		);
	}
/*...e*/

byte DebugZ80(Z80 *r)
	{
    vdeb (r);
	if ( diag_flags[DIAG_Z80_INSTRUCTIONS] )
		{
		word pc = r->PC.W;
		if ( ( no_trace[pc>>3]&(0x01<<(pc&7)) ) == 0 )
			{
			char buf[500+1];
			if ( diag_flags[DIAG_Z80_INSTRUCTIONS_NEW] )
				no_trace[pc>>3] |= (0x01<<(pc&7));
			dis_instruction(&pc, buf);
			DebugZ80Instruction(r, buf);
			last_trace = TRUE;
			}
		else if ( last_trace )
			{
			DebugZ80Instruction(r, "...");
			last_trace = FALSE;
			}
		}
	return 1;
	}
/*...e*/

void show_instruction (void)
    {
    BOOLEAN save_flag = diag_flags[DIAG_Z80_INSTRUCTIONS];
    byte save_last = last_trace;
    word pc = z80.PC.W;
    byte save_trace = no_trace[pc>>3];
    diag_flags[DIAG_Z80_INSTRUCTIONS] = TRUE;
    no_trace[pc>>3] = 0;
    DebugZ80 (&z80);
    diag_flags[DIAG_Z80_INSTRUCTIONS] = save_flag;
    no_trace[pc>>3] = save_trace;
    last_trace = save_last;
    }
#endif

/*...sOutZ80:0:*/
/*...sOutZ80_bad:0:*/
void OutZ80_bad(const char *hardware, word port, byte value, BOOLEAN stop)
	{
	diag_message(DIAG_BAD_PORT_DISPLAY, "no emulation of %s, out 0x%04x,0x%02x", hardware, port, value);
	if ( diag_flags[DIAG_BAD_PORT_DISPLAY] )
	    {
	    BOOLEAN dbgflg = diag_flags[DIAG_Z80_INSTRUCTIONS];
	    diag_flags[DIAG_Z80_INSTRUCTIONS] = TRUE;
	    DebugZ80 (&z80);
	    diag_flags[DIAG_Z80_INSTRUCTIONS] = dbgflg;
	    }
	if ( stop && !diag_flags[DIAG_BAD_PORT_IGNORE] )
		fatal("no emulation of %s, out 0x%04x,0x%02x, so stopping emulation", hardware, port, value);
	}
/*...e*/

#ifdef ALT_Z80_OUT
BOOLEAN ALT_Z80_OUT (word, byte);
#endif

void OutZ80(word port, byte value)
	{
#ifdef ALT_Z80_OUT
	if ( ALT_Z80_OUT (port, value) ) return;
#endif
	switch ( port & 0xff )
		{
		case 0x00:
			mem_out0(value);
			break;
		case 0x01:
			vid_out1(value, z80.IElapsed);
#ifdef HAVE_VGA
            vga_out1(value);
#endif
			break;
		case 0x02:
			vid_out2(value);
#ifdef HAVE_VGA
            vga_out2(value);
#endif
			break;
		case 0x03:
			tape_out3 (value);
			/* OutZ80_bad("cassette tape", port, value, FALSE); */
			break;
		case 0x04:
			print_out4(value);
			break;
		case 0x05:
			kbd_out5(value);
			break;
		case 0x06:
			snd_out6(value);
			break;
		case 0x07:
			OutZ80_bad("PIO", port, value, FALSE);
			break;
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			ctc_out(port&0x03, value);
#if 0
			if ( (port&0x03) == 2 )
				/* Its a write to channel 2 */
				if ( ( z80.PC.W == 0x218c && (mem_get_iobyte()&0xf0) == 0x10 ) || /* MTX BASIC PANEL */
				     ( z80.PC.W == 0xa4fa && (mem_get_iobyte()&0x80) == 0x80 ) )  /* VDEB.COM */
/*...sPANEL hack:40:*/
/* Allow PANEL single step to work.
   It very cleverly programs channel 2 in timer mode with prescaler 16
   and value 13, which is 208 clks, which means that the instant the
   single instruction is executed, an interrupt will be pending.
   This means they can single step ROM, as they don't have to patch
   the following instruction.

   Normally, in MEMU, interrupts aren't checked for after every instruction.
   So actually, we'd "single step" several instructions at once.
   To avoid this, we reduce the ICount to just enough to start executing
   the single instruction, then call LoopZ80 immediately afterwards.

   The normal CTC operation in response to the previous register write
   will not have caused a reload of prescaler or counter.
   To be sure of consistent behaviour, we must cause the CTC to reload
   to exactly what we've asked for. */

                    {
                    ctc_reload(2); /* Get consistent behaviour */
                    clk_skipped = z80.ICount; /* Be sure not to forget to account for this time */
                    z80.ICount = 13*16+3; /* Took some fiddling to get this right */
                    panel_hack = TRUE;
                    diag_message(DIAG_PANEL_HACK, "panelHack ARMED pc=0x%04x", z80.PC.W);
                    }
#endif
/*...e*/
			break;
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
#ifdef HAVE_DART
			dart_out(port&0x03, value);
#else
			OutZ80_bad("DART", port, value, FALSE);
#endif
			break;
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
			sdxfdc_out(port&0xff, value);
			break;
		case 0x1f:
			/* MTX BASIC LOAD/SAVE/VERIFY doesn't use this,
			   but certain cassette loader implementations do,
			   such as the one I had the source for (INOUT.MAC),
			   and the one used by Jet Set Willy. */
			switch ( value )
				{
				case 0xaa:
					diag_message(DIAG_TAPE, "Cassette remote control start");
					break;
				case 0x55:
					diag_message(DIAG_TAPE, "Cassette remote control stop");
					break;
				}
			tape_out1F (value);
#ifdef HAVE_SPEC
			spec_out1F(value);
#endif
			break;
		case 0x30:
			mon_out30(value);
			break;
		case 0x31:
			mon_out31(value);
			break;
		case 0x32:
			mon_out32(value);
			break;
		case 0x33:
			mon_out33(value);
			break;
		case 0x38:
			mon_out38(value);
			break;
		case 0x39:
			mon_out39(value);
			break;
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
			OutZ80_bad("SJM's FDX FDC", port, value, TRUE);
			break;
		case 0x50: /* Low address */
		case 0x51: /* High address */
		case 0x52: /* Huge address */
		case 0x53: /* Data R/W */
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5a:
		case 0x5b:
		case 0x5c:
		case 0x5d:
		case 0x5e:
		case 0x5f:
#ifdef HAVE_SID
			sid_out(port&0xff, value);
#else
            OutZ80_bad("Silicon Disc", port, value, TRUE);
#endif
			break;
#ifdef HAVE_VGA
        case 0x60:
            if ( ! cfg.bVGA ) OutZ80_bad("MTXplus+/CFX-II VGA", port, value, TRUE);
            vga_out60 (value);
            break;
        case 0x61:
            if ( ! cfg.bVGA ) OutZ80_bad("MTXplus+/CFX-II VGA", port, value, TRUE);
            vga_out61 (value);
            break;
#endif
		case 0x6c:
		case 0x6d:
		case 0x6e:
		case 0x6f:
			OutZ80_bad("MTXplus+/CFX CF", port, value, TRUE);
			break;
		case 0x70:
		case 0x71:
			OutZ80_bad("MTXplus+ RTC", port, value, TRUE);
			break;
#ifdef HAVE_SPEC
		case 0x7e:
			spec_out7E(port>>8, value);
			break;
		case 0x7f:
			spec_out7F(value);
			break;
#endif
#ifdef HAVE_NFX
 		case NFX_BASE:
 		case NFX_BASE + 1:
 		case NFX_BASE + 2:
 		case NFX_BASE + 3:
		    nfx_out(port & 0x03, value);
		    break;
#endif
#ifdef HAVE_CFX2
        case 0xb0:
        case 0xb1:
        case 0xb2:
        case 0xb3:
        case 0xb4:
        case 0xb5:
        case 0xb6:
        case 0xb7:
            if ( ! cfg.bCFX2 ) OutZ80_bad("/CFX-II CF", port, value, TRUE);
            cfx2_out (port, value);
            break;
#endif
		case 0xc0:
		case 0xc1:
			OutZ80_bad("REMEMOTECH HEXs", port, value, TRUE);
			break;
		case 0xc4:
			OutZ80_bad("REMEMOTECH LEDGs", port, value, TRUE);
			break;
		case 0xd0:
		case 0xd1:
		case 0xd2:
		case 0xd3:
			OutZ80_bad("REMEMOTECH/REMEMOrizer page registers", port, value, TRUE);
			break;
		case 0xd4:
		case 0xd6:
			OutZ80_bad("REMEMOTECH/REMEMOrizer SD Card", port, value, TRUE);
			break;
		case 0xd9:
			OutZ80_bad("REMEMOrizer flags", port, value, TRUE);
			break;
        case 0xee:
        case 0xef:
            if ( diag_flags[DIAG_CHIP_LOG] ) diag_out (port, value);
            else OutZ80_bad("unknown hardware", port, value, FALSE);
            break;
#ifdef HAVE_SPEC
		case 0xfb:
			/* This is used by Speculator, but unfortunately
			   is also used as the MAGROM page register.
			   We only support Speculator, not MAGROM. */
			spec_outFB(value);
			break;
		case 0xfe:
			spec_outFE(value);
			break;
		case 0xff:
			spec_outFF(value);
			if ( value & NMI_ENABLED )
				{
				diag_message(DIAG_SPEC_INTERRUPTS, "fake NMI soon");
				nmi_next_time = TRUE;
				}
			break;
#endif
		default:
			OutZ80_bad("unknown hardware", port, value, FALSE);
			break;
		}
	}
/*...e*/
/*...sInZ80:0:*/
/*...sInZ80_bad:0:*/
byte InZ80_bad(const char *hardware, word port, BOOLEAN stop)
	{
	diag_message(DIAG_BAD_PORT_DISPLAY, "no emulation of %s, in 0x%04x", hardware, port);
	if ( diag_flags[DIAG_BAD_PORT_DISPLAY] )
	    {
	    BOOLEAN dbgflg = diag_flags[DIAG_Z80_INSTRUCTIONS];
	    diag_flags[DIAG_Z80_INSTRUCTIONS] = TRUE;
	    DebugZ80 (&z80);
	    diag_flags[DIAG_Z80_INSTRUCTIONS] = dbgflg;
	    }
	if ( stop && !diag_flags[DIAG_BAD_PORT_IGNORE] )
		fatal("no emulation of %s, in 0x%04x, so stopping emulation", hardware, port);
	return 0xff;
	}
/*...e*/

#ifdef	ALT_Z80_IN
BOOLEAN ALT_Z80_IN (word, byte *);
#endif

byte InZ80(word port)
	{
#ifdef	ALT_Z80_IN
	byte value;
	if ( ALT_Z80_IN (port, &value) ) return value;
#endif
	switch ( port & 0xff )
		{
		case 0x00:
			return print_in0();
		case 0x01:
			return vid_in1(z80.IElapsed);
		case 0x02:
			return vid_in2();
		case 0x03:
			return snd_in3();
		case 0x04:
			return print_in4();
		case 0x05:
			return kbd_in5();
		case 0x06:
			return kbd_in6();
		case 0x07:
			return InZ80_bad("PIO", port, FALSE);
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
			return ctc_in(port&0x03);
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
#ifdef HAVE_DART
			return dart_in(port&0x03);
#else
			return InZ80_bad("DART", port, TRUE);
#endif
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
			return sdxfdc_in(port&0xff);
#ifdef HAVE_SPEC
		case 0x1f:
			return spec_in1F();
#endif
		case 0x30:
			return mon_in30();
		case 0x32:
			return mon_in32();
		case 0x33:
			return mon_in33();
		case 0x38:
			return mon_in38();
		case 0x39:
			return mon_in39();
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
			return InZ80_bad("SJM's FDX FDC", port, TRUE);
		case 0x50:
		case 0x51:
		case 0x52:
		case 0x53:
		case 0x54:
		case 0x55:
		case 0x56:
		case 0x57:
		case 0x58:
		case 0x59:
		case 0x5a:
		case 0x5b:
		case 0x5c:
		case 0x5d:
		case 0x5e:
		case 0x5f:
#ifdef HAVE_SID
			return sid_in(port&0xff);
#else
            InZ80_bad("Silicon disc", port, TRUE);
            return 0xFF;
#endif
#ifdef HAVE_VGA
        case 0x60:
            if ( ! cfg.bVGA ) InZ80_bad("MTXplus+/CFX-II VGA", port, TRUE);
            return vga_in60 ();
        case 0x61:
            if ( ! cfg.bVGA ) InZ80_bad("MTXplus+/CFX-II VGA", port, TRUE);
            return vga_in61 ();
#endif
		case 0x6c:
		case 0x6d:
			return InZ80_bad("MTXplus+/CFX CF", port, TRUE);
		case 0x70:
		case 0x71:
			return InZ80_bad("MTXplus+ RTC", port, TRUE);
#ifdef HAVE_SPEC
		case 0x7e:
			return spec_in7E();
		case 0x7f:
			return spec_in7F();
#endif
#ifdef HAVE_NFX
 		case NFX_BASE:
 		case NFX_BASE + 1:
 		case NFX_BASE + 2:
 		case NFX_BASE + 3:
		    return nfx_in(port & 0x03);
#endif
#ifdef HAVE_CFX2
        case 0xb0:
        case 0xb1:
        case 0xb2:
        case 0xb3:
        case 0xb4:
        case 0xb5:
        case 0xb6:
        case 0xb7:
            if ( ! cfg.bCFX2 ) InZ80_bad("/CFX-II CF", port, TRUE);
            return cfx2_in (port);
#endif
		case 0xc0:
		case 0xc1:
			return InZ80_bad("REMEMOTECH HEXs", port, TRUE);
		case 0xc4:
			return InZ80_bad("REMEMOTECH LEDGs", port, TRUE);
		case 0xc5:
			return InZ80_bad("REMEMOTECH KEYs", port, TRUE);
		case 0xc7:
			return InZ80_bad("REMEMOTECH XKEYs", port, TRUE);
		case 0xd0:
		case 0xd1:
		case 0xd2:
		case 0xd3:
			return InZ80_bad("REMEMOTECH/REMEMOrizer page registers", port, TRUE);
		case 0xd4:
		case 0xd6:
		case 0xd7:
			return InZ80_bad("REMEMOTECH/REMEMOrizer SD Card", port, TRUE);
		case 0xd8:
			return InZ80_bad("REMEMOTECH/REMEMOrizer clock divider", port, TRUE);
		case 0xd9:
			return InZ80_bad("REMEMOrizer flags", port, TRUE);
#ifdef HAVE_SPEC
		case 0xfb:
			return spec_inFB();
		case 0xfe:
			return spec_inFE(port>>8);
#endif
		default:
			return InZ80_bad("unknown hardware", port, TRUE);
		}
	}
/*...e*/

/*...smemu_reset:0:*/
void memu_reset(void)
	{
#ifdef HAVE_VGA
    if ( cfg.bVGA ) vga_reset ();
#endif
	ctc_init();
    vid_reset ();
	mem_set_iobyte(0x00);
	mem_set_rom_subpage(0x00);
	ResetZ80(&z80);
	}
/*...e*/

/*...smain:0:*/
/*...ssdxcfg:0:*/
static int sdxcfg(int tracks)
	{
	switch ( tracks )
		{
		case 40: return DRVS_DOUBLE_SIDED | DRVS_40TRACK | DRVS_1_DRIVE;
		case 80: return DRVS_DOUBLE_SIDED | DRVS_80TRACK | DRVS_1_DRIVE;
		}
	fatal("unsupported number of tracks %d", tracks);
	return -1; /* Not reached */
	}
/*...e*/

#ifdef ALT_OPTIONS
extern BOOLEAN ALT_OPTIONS (int *pargc, const char ***pargv, int *pi);
#endif

#ifdef ALT_INIT
extern void ALT_INIT (void);
#endif

int memu (int argc, const char *argv[])
	{
	int i;
	unsigned addr = 0;
	BOOLEAN fdxb = FALSE;
    psExe = argv[0];
	memset (&cfg, 0, sizeof (cfg));
	cfg.vid_width_scale = 1;
	cfg.vid_height_scale = 1;
	cfg.mon_width_scale = 1;
	cfg.mon_height_scale = 1;
    cfg.mon_emu = MONEMU_IGNORE_INIT;
#ifdef __Pico__
    cfg.iperiod = 4000000 / 50;
#else
	cfg.iperiod = 1000;
#endif
	cfg.tracks_sdxfdc[0] = 80;
	cfg.tracks_sdxfdc[1] = 80;
	cfg.screen_refresh = 50;

	if ( argc == 1 )
		usage(NULL);

	diag_init();
    // diag_methods = DIAGM_CONSOLE;
    // diag_flags[DIAG_INIT] = TRUE;
	mem_init_mtx();
#ifdef SMALL_MEM
    // Set minimum working configuration
    static char sTapeDir[] = "tapes";
    mem_alloc(2);
    cfg.vid_emu |= VIDEMU_WIN;
    cfg.tape_name_prefix = sTapeDir;
    cfg_set_disk_dir ("disks");
#endif
#ifdef HAVE_DISASS
	dis_init();
#endif

	for ( i = 1; i < argc; i++ )
		{
        diag_message (DIAG_INIT, "argc = %d, argv[%d] = \"%s\"", argc, i, argv[i]);
		// printf ("argc = %d argv[%d] = \"%s\"\n", argc, i, argv[i]);
        if ( !strcmp(argv[i], "-help") )
			{
            usage (NULL);
            }
        else if ( !strcmp(argv[i], "-ignore") )
			{
            bIgnore = TRUE;
            }
#ifdef ALT_OPTIONS
        else if ( ALT_OPTIONS (&argc, &argv, &i) )
			{
            }
#endif
        else if ( !strcmp(argv[i], "-iobyte") )
			{
			unsigned iobyte;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &iobyte);
			mem_set_iobyte((byte) iobyte);
			}
		else if ( !strcmp(argv[i], "-subpage") )
			{
#ifndef SMALL_MEM
			int subpage;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &subpage);
			mem_set_rom_subpage((byte) subpage);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-addr") )
			{
#ifndef SMALL_MEM
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &addr);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-mem") )
			{
#ifndef SMALL_MEM
			byte buf[0x10000];
			int len;
			if ( ++i == argc )
				opterror (argv[i-1]);
			len = read_file(argv[i], buf, sizeof(buf));
			mem_write_block((word) addr, (word) len, buf);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-mem-blocks") )
			{
			int nblocks;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &nblocks);
			mem_alloc(nblocks);
			}
		else if ( !strcmp(argv[i], "-mem-mtx500") )
			mem_alloc(2);
		else if ( !strcmp(argv[i], "-mem-blocks-snapshot") )
			{
#ifndef SMALL_MEM
			int nblocks;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &nblocks);
			mem_alloc_snapshot(nblocks);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-n-subpages") )
			{
#ifndef SMALL_MEM
			int rom, n_subpages;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &rom);
			if ( rom < 0 || rom >= 8 )
				fatal("rom must be between 0 and 7");
			if ( ++i == argc )
				opterror (argv[i-2]);
			sscanf(argv[i], "%i", &n_subpages);
			if ( n_subpages ==   1 ||
			     n_subpages ==   2 ||
			     n_subpages ==   4 ||
			     n_subpages ==   8 ||
			     n_subpages ==  16 ||
			     n_subpages ==  32 ||
			     n_subpages ==  64 ||
			     n_subpages == 128 ||
			     n_subpages == 256 )
				; /* valid number of subpages */
			else
				fatal("n_subpages must be 1, 2, 4, 8, 16, 32, 64, 128 or 256");
			mem_set_n_subpages(rom, n_subpages);
#else
            unimplemented (argv[i]);
            i += 2;
#endif
			}
		else if ( !strncmp(argv[i], "-rompair", 8) )
			{
#ifndef SMALL_MEM
			int rom;
			sscanf(argv[i]+8, "%i", &rom);
			if ( ++i == argc )
				opterror (argv[i-1]);
            load_rompair (rom, argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strncmp(argv[i], "-rom", 4) )
			{
#ifndef SMALL_MEM
			int rom;
			sscanf(argv[i]+4, "%i", &rom);
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.rom_fn[rom] = argv[i];
            load_rom (rom, argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-kbd-remap") )
			cfg.kbd_emu |= KBDEMU_REMAP;
		else if ( !strcmp(argv[i], "-kbd-country") )
			{
			int country;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &country);
			if ( ( country < 0 ) || ( country > 3 ) )
				usage (argv[i-1]);
			cfg.kbd_emu |= ( country << 2 );
			}
		else if ( !strcmp(argv[i], "-kbd-type") )
			{
#ifdef HAVE_AUTOTYPE
			if ( ++i == argc )
				opterror (argv[i-1]);
			kbd_add_events(argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-kbd-type-file") )
			{
#ifdef HAVE_AUTOTYPE
			if ( ++i == argc )
				opterror (argv[i-1]);
			kbd_add_events_file(PMapPath (argv[i]));
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-joy") ||
		          !strcmp(argv[i], "-j")   )
            {
#ifdef HAVE_JOY
			cfg.joy_emu |= JOYEMU_JOY;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-joy-buttons") )
			{
#ifdef HAVE_JOY
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.joy_buttons = argv[i];
			joy_set_buttons(argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-joy-central") )
			{
#ifdef HAVE_JOY
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%d", &cfg.joy_central);
			joy_set_central(cfg.joy_central);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-vid-win") )
            {
			cfg.vid_emu |= VIDEMU_WIN;
            }
		else if ( !strcmp(argv[i], "-vid-win-big") ||
			  !strcmp(argv[i], "-v")           )
			{
			cfg.vid_emu |= VIDEMU_WIN;
			++cfg.vid_width_scale;
			++cfg.vid_height_scale;
			}
		else if ( !strcmp(argv[i], "-vid-win-max") )
            {
			cfg.vid_emu |= VIDEMU_WIN_MAX;
            vid_max_scale (&cfg.vid_width_scale, &cfg.vid_height_scale);
            }
		else if ( !strcmp(argv[i], "-vid-win-hw-palette") )
            {
			cfg.vid_emu |= (VIDEMU_WIN|VIDEMU_WIN_HW_PALETTE);
            }
		else if ( !strcmp(argv[i], "-vid-ntsc") )
            {
			cfg.screen_refresh = 60;
            }
		else if ( !strcmp(argv[i], "-vid-time-check") )
			{
#ifdef HAVE_VID_TIMING
			unsigned t_2us, t_8us, t_blank;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%u,%u,%u", &t_2us, &t_8us, &t_blank);
			vid_setup_timing_check(t_2us, t_8us, t_blank);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-snd-portaudio") ||
			  !strcmp(argv[i], "-s")             )
            {
			cfg.snd_emu |= SNDEMU_PORTAUDIO;
            }
		else if ( !strcmp(argv[i], "-snd-latency") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%lf", &cfg.latency);
			}
		else if ( !strcmp(argv[i], "-mon-win") )
            {
			cfg.mon_emu |= MONEMU_WIN;
            }
		else if ( !strcmp(argv[i], "-mon-win-big") ||
			  !strcmp(argv[i], "-mw")          )
			{
			cfg.mon_emu |= MONEMU_WIN;
			++cfg.mon_height_scale;
			cfg.mon_width_scale = cfg.mon_height_scale/2;
			}
		else if ( !strcmp(argv[i], "-mon-win-max") )
            {
			cfg.mon_emu |= MONEMU_WIN_MAX;
            mon_max_scale (&cfg.mon_width_scale, &cfg.mon_height_scale);
            }
		else if ( !strcmp(argv[i], "-mon-win-mono") )
            {
			cfg.mon_emu |= MONEMU_WIN_MONO;
            }
		else if ( !strcmp(argv[i], "-mon-th") ||
			  !strcmp(argv[i], "-mt")     )
            {
#ifdef HAVE_TH
			cfg.mon_emu |= MONEMU_TH;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-mon-console") ||
			  !strcmp(argv[i], "-mc")          )
            {
#ifdef HAVE_CONSOLE
			cfg.mon_emu |= MONEMU_CONSOLE;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-mon-console-nokey") ||
			  !strcmp(argv[i], "-mk")                )
            {
#ifdef HAVE_CONSOLE
			cfg.mon_emu |= (MONEMU_CONSOLE|MONEMU_CONSOLE_NOKEY);
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-mon-no-ignore-init") )
            {
			cfg.mon_emu &= ~MONEMU_IGNORE_INIT;
            }
		else if ( !strcmp(argv[i], "-sdx-tracks") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &cfg.tracks_sdxfdc[0]);
			}
		else if ( !strcmp(argv[i], "-sdx-mfloppy") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.fn_sdxfdc[0] = argv[i];
			}
		else if ( !strcmp(argv[i], "-sdx-tracks2") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &cfg.tracks_sdxfdc[1]);
			}
		else if ( !strcmp(argv[i], "-sdx-mfloppy2") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.fn_sdxfdc[1] = argv[i];
			}
		else if ( !strcmp(argv[i], "-sidisc-huge") )
            {
#ifdef HAVE_SID
			cfg.sid_emu |= SIDEMU_HUGE;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-sidisc-no-save") )
            {
#ifdef HAVE_SID
			cfg.sid_emu |= SIDEMU_NO_SAVE;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-sidisc-file") )
			{
#ifdef HAVE_SID
			int drive;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &drive);
			if ( drive < 0 || drive >= N_SIDISC )
				fatal("bad Silicon Disc number");
			if ( ++i == argc )
				opterror (argv[i-2]);
			cfg.sid_fn[drive] = argv[i];
			sid_set_file(drive, argv[i]);
#else
            unimplemented (argv[i]);
            i += 2;
#endif
			}
        else if ( !strcmp (argv[i], "-cfx2") )
            {
#ifdef HAVE_CFX2
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.rom_cfx2 = argv[i];
            load_rompair (4, cfg.rom_cfx2);
            cfg.bCFX2 = TRUE;
#ifdef HAVE_VGA
            cfg.bVGA = TRUE;
#endif
#else
            unimplemented (argv[i]);
            ++i;
#endif
            }
        else if ( !strcmp (argv[i], "-cf-image") )
            {
#ifdef HAVE_CFX2
            int iDrive;
            int iPart;
			if ( ++i == argc )
				opterror (argv[i-1]);
			if ( sscanf(argv[i], "%i:%i", &iDrive, &iPart) == 1 )
                {
                iPart = iDrive;
                iDrive = 0;
                }
			if ( ++i == argc )
				opterror (argv[i-2]);
            cfx2_set_image (iDrive, iPart, argv[i]);
            cfg.fn_cfx2[iDrive][iPart] = argv[i];
#else
            unimplemented (argv[i]);
            i += 2;
#endif
            }
        else if ( !strcmp (argv[i], "-vga") )
            {
#ifdef HAVE_VGA
            cfg.bVGA = TRUE;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-prn-file") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.fn_print = argv[i];
			}
		else if ( !strcmp(argv[i], "-tape-dir") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			if ( strlen(argv[i]) > MAX_TAPE_PREFIX )
				fatal("tape prefix is too long");
			cfg.tape_name_prefix = argv[i];
			}
		else if ( !strcmp(argv[i], "-tape-overwrite") )
			cfg.tape_overwrite = TRUE;
		else if ( !strcmp(argv[i], "-tape-disable") )
			cfg.tape_disable = TRUE;
		else if ( !strcmp(argv[i], "-cassette-in") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			tape_set_input (argv[i]);
			cfg.tape_disable = TRUE;
			}
		else if ( !strcmp(argv[i], "-cassette-out") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			tape_set_output (argv[i]);
			cfg.tape_disable = TRUE;
			}
		else if ( !strcmp(argv[i], "-tap-file") )
			{
#ifdef HAVE_SPEC
			if ( ++i == argc )
				opterror (argv[i-1]);
			tap_fn = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-sna-file") )
			{
#ifdef HAVE_SPEC
			if ( ++i == argc )
				opterror (argv[i-1]);
			sna_fn = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-cpm") )
			{
#ifdef HAVE_OSFS
			cpm_init();
			mem_set_iobyte(0x80);
			addr = 0x100;
#else
            unimplemented (argv[i]);
#endif
			}
		else if ( !strcmp(argv[i], "-cpm-drive-a") )
			{
#ifdef HAVE_OSFS
			if ( ++i == argc )
				opterror (argv[i-1]);
			cpm_set_drive_a(PMapPath (argv[i]));
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-cpm-invert-case") )
            {
#ifdef HAVE_OSFS
			cpm_set_invert_case();
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-cpm-tail") )
			{
#ifdef HAVE_OSFS
			if ( ++i == argc )
				opterror (argv[i-1]);
			cpm_set_tail(argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-cpm-open-hack") )
            {
#ifdef HAVE_OSFS
			cpm_allow_open_hack(TRUE);
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-sdx5") || !strcmp(argv[i], "-sdx") )
            {
#ifdef HAVE_OSFS
			setup_sdx(5);
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-sdx3") )
            {
#ifdef HAVE_OSFS
			setup_sdx(3);
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-fdxb") )
            {
#ifdef HAVE_OSFS
			fdxb = TRUE;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-speed") )
			{
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%llu", &clock_speed);
			if ( clock_speed < 1000000 || clock_speed > 1000000000 )
				fatal("clock speed must be between 1000000 (1MHz) and 1000000000 (1GHz)");
#ifdef __Pico__
            cfg.iperiod = clock_speed / 60;
#endif
			}
		else if ( !strcmp(argv[i], "-fast") )
            {
			moderate_speed = FALSE;
            }
		else if ( !strcmp(argv[i], "-iperiod") )
			{
			/* Undocumented feature.
			   I use this to investigate when I suspect a program
			   may be very timing sensitive. */
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%d", &cfg.iperiod);
			if ( cfg.iperiod < 10 || cfg.iperiod > 1000000 )
				fatal("iperiod must be between 10 and 1000000");
			}
		else if ( !strcmp(argv[i], "-loadmtx") )
            {
#ifdef HAVE_OSFS
			loadmtx_hack = TRUE;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-serial1-dev") )
			{
#ifdef HAVE_DART
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.bSerialDev[0] = TRUE;
			cfg.fn_serial_in[0] = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-serial1-in") )
			{
#ifdef HAVE_DART
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.fn_serial_in[0] = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-serial1-out") )
			{
#ifdef HAVE_DART
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.fn_serial_out[0] = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-serial2-dev") )
			{
#ifdef HAVE_DART
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.bSerialDev[1] = TRUE;
			cfg.fn_serial_in[1] = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-serial2-in") )
			{
#ifdef HAVE_DART
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.fn_serial_in[1] = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-serial2-out") )
			{
#ifdef HAVE_DART
			if ( ++i == argc )
				opterror (argv[i-1]);
			cfg.fn_serial_out[1] = argv[i];
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-z80-instructions-exclude") )
			{
#ifdef Z80_DEBUG
			int addr_from, addr_to, addr;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i-%i", &addr_from, &addr_to);
			for ( addr = addr_from; addr <= addr_to; addr++ )
				no_trace[(addr&0xffff)>>3] |= (0x01<<(addr&7));
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-z80-instructions-include") )
			{
#ifdef Z80_DEBUG
			int addr_from, addr_to, addr;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i-%i", &addr_from, &addr_to);
			for ( addr = addr_from; addr <= addr_to; addr++ )
				no_trace[(addr&0xffff)>>3] &= ~(0x01<<(addr&7));
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-nfx-port-offset") )
			{
#ifdef HAVE_NFX
			int offset;
			if ( ++i == argc )
				opterror (argv[i-1]);
			sscanf(argv[i], "%i", &offset);
            nfx_port_offset (offset);
#else
            unimplemented (argv[i]);
            ++i;
#endif
            }
		else if ( !strcmp(argv[i], "-vid-win-title") )
			{
#ifdef HAVE_GUI
			if ( ++i == argc )
				opterror (argv[i-1]);
			vid_set_title (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-vid-win-display") )
			{
#ifdef HAVE_GUI
			if ( ++i == argc )
				opterror (argv[i-1]);
			vid_set_display (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-mon-win-title") )
			{
#ifdef HAVE_GUI
			if ( ++i == argc )
				opterror (argv[i-1]);
			mon_set_title (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-mon-win-display") )
			{
#ifdef HAVE_GUI
			if ( ++i == argc )
				opterror (argv[i-1]);
			mon_set_display (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-ui-mem-win-title") )
			{
#ifdef HAVE_UI
			if ( ++i == argc )
				opterror (argv[i-1]);
			ui_mem_set_title (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-ui-mem-win-display") )
			{
#ifdef HAVE_UI
			if ( ++i == argc )
				opterror (argv[i-1]);
			ui_mem_set_display (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-ui-vram-win-title") )
			{
#ifdef HAVE_UI
			if ( ++i == argc )
				opterror (argv[i-1]);
			ui_vram_set_title (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-ui-vram-win-display") )
			{
#ifdef HAVE_UI
			if ( ++i == argc )
				opterror (argv[i-1]);
			ui_vram_set_display (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-ui-dis-win-title") )
			{
#ifdef HAVE_UI
			if ( ++i == argc )
				opterror (argv[i-1]);
			ui_dis_set_title (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-ui-dis-win-display") )
			{
#ifdef HAVE_UI
			if ( ++i == argc )
				opterror (argv[i-1]);
			ui_dis_set_display (argv[i]);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		else if ( !strcmp(argv[i], "-diag-ui-mem") )
            {
#ifdef HAVE_UI
			cfg.ui_opts |= UI_MEM;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-diag-ui-vram") )
            {
#ifdef HAVE_UI
			cfg.ui_opts |= UI_VRAM;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strcmp(argv[i], "-diag-ui-dis") )
            {
#ifdef HAVE_UI
			cfg.ui_opts |= UI_DIS;
#else
            unimplemented (argv[i]);
#endif
            }
		else if ( !strncmp(argv[i], "-diag-", 6) )
			{
			int m;
			if ( (m = diag_method_of(argv[i]+6)) != 0 )
				diag_methods |= m;
			else if ( diag_flag_of(argv[i]+6) )
				;
			else
				opterror (argv[i]);
			}
#if defined(BEMEMU)
		else if ( !strcmp(argv[i], "-be") )
			{
			const char *pipe_id;
			if ( ++i == argc )
				opterror (argv[i-1]);
			pipe_id = argv[i];
			be_init(pipe_id);
			}
#endif
		else
			break;
		}

#ifdef HAVE_AUTOTYPE
	kbd_add_events_done();
#endif

	/* Now try to support some convenient shorthands */
	/* printf ("argc = %d, i = %d\n", argc, i); */
	if ( i < argc )
		{
		const char *dot = strrchr(argv[i], '.');
		if ( dot == NULL )
			opterror (argv[i]);
        if ( !strcmp(dot, ".mtx") || !strcmp(dot, ".MTX") )
            {
			/* Save the filename for later LOAD "" */
			cfg.tape_fn = argv[i++];
            }
		else if ( !strcmp(dot, ".com") || !strcmp(dot, ".COM") )
			{
#ifdef HAVE_OSFS
			byte buf[0xfe00-0x0100];
			int len = read_file_path(argv[i], buf, sizeof(buf), cpm_get_drive_a());
			char tail[128+1];
			cpm_init();
			mem_set_iobyte(0x80);
			mem_write_block(0x0100, len, buf);
			addr = 0x0100;
			++i;
			tail[0] = '\0';
			tail[1] = '\0';
			for ( ; i < argc; i++ )
				{
				if ( strlen(tail) + 1 + strlen(argv[i]) > 128 )
					fatal("command tail too long");
				strcat(tail, " ");
				strcat(tail, argv[i]);
				}
			cpm_set_tail( (tail[1]!='\0') ? tail : "" );
#else
            unimplemented (argv[i]);
            i = argc;
#endif
			}
		else if ( !strcmp(dot, ".run") || !strcmp(dot, ".RUN") )
			{
#ifdef HAVE_OSFS
			int len;
			run_buf = emalloc(0x10000-0x4000);
			len = read_file_path(argv[i], run_buf, 0x10000-0x4000, cpm_get_drive_a());
			if ( len < 4 )
				fatal("RUN file is way too short");
			run_hdr_base   = get_word(run_buf  );
			run_hdr_length = get_word(run_buf+2);
			if ( len < run_hdr_length )
				/* Per SDX User Manual, the length in the
				   header is doesn't include the header. */
				fatal("RUN file is too short");
			if ( run_hdr_base < 0x4000 ||
			     (unsigned long) run_hdr_base + (unsigned long) run_hdr_length >= 0x10000 )
				fatal("RUN file base and length fields aren't credible"); 
			mem_set_iobyte(0x00);
			mem_write_byte(0x3627, 0xed); /* was 0xd3 */
			mem_write_byte(0x3628, 0xfe); /* was 0x05 */
			addr = 0x0000;
			++i;
			// fflush(stdout);
#else
            unimplemented (argv[i]);
            ++i;
#endif
			}
		}

	if ( i != argc )
		usage ("Unrecognised command %s", argv[i]);

    /* Test for no display - missing config file ? */

    if ( ( cfg.vid_emu == 0 ) && ( ( cfg.mon_emu & ~MONEMU_IGNORE_INIT ) == 0 )
#ifdef HAVE_VGA
        && ( ! cfg.bVGA )
#endif
        )
        fatal ("No display specified");

#ifdef HAVE_TH
	if ( (diag_methods & DIAGM_CONSOLE) != 0 &&
	     (cfg.mon_emu & MONEMU_TH) != 0 )
		fatal("cannot emulate the monitor using TH and send diagnostics to the console at the same time");
	if ( (cfg.mon_emu & MONEMU_TH     ) != 0 &&
	     (cfg.mon_emu & MONEMU_CONSOLE) != 0 ) 
		fatal("cannot emulate the monitor using TH and the console at the same time");
#endif

#if ! (defined(__circle__) || defined(__Pico__))
	{
	time_t t_start = time(NULL);
	char buf[26+1];
	strcpy(buf, ctime(&t_start));
	buf[24] = '\0';
	diag_message(DIAG_ALWAYS, "=== memu started at %s ===", buf);
	}
#endif

#ifdef ALT_INIT
    diag_message (DIAG_INIT, "ALT_INIT");
	ALT_INIT ();
#endif
    diag_message (DIAG_INIT, "ctc_init");
	ctc_init();
    diag_message (DIAG_INIT, "vid_init (0x%02X, %d, %d)",
        cfg.vid_emu, cfg.vid_width_scale, cfg.vid_height_scale);
	vid_init(cfg.vid_emu, cfg.vid_width_scale, cfg.vid_height_scale);
    diag_message (DIAG_INIT, "vid_init");
	kbd_init(cfg.kbd_emu);
#ifdef HAVE_JOY
    diag_message (DIAG_INIT, "joy_init");
	joy_init(cfg.joy_emu);
#endif
    diag_message (DIAG_INIT, "snd_init");
	snd_init(cfg.snd_emu, cfg.latency);
    diag_message (DIAG_INIT, "mon_init (0x%02X)", cfg.mon_emu);
	mon_init(cfg.mon_emu, cfg.mon_width_scale, cfg.mon_height_scale);
#ifdef HAVE_DART
    diag_message (DIAG_INIT, "dart_init");
	dart_init();
	if ( cfg.bSerialDev[0] )
		{
		dart_serial(0, cfg.fn_serial_in[0]);
		}
	else
		{
		dart_read(0, cfg.fn_serial_in[0]);
		dart_write(0, cfg.fn_serial_out[0]);
		}
	if ( cfg.bSerialDev[1] )
		{
		dart_serial(1, cfg.fn_serial_in[0]);
		}
	else
		{
		dart_read(1, cfg.fn_serial_in[1]);
		dart_write(1, cfg.fn_serial_out[1]);
		}
#endif
#ifdef HAVE_SPEC
    diag_message (DIAG_INIT, "spec_init");
	spec_init();
#endif

#ifdef HAVE_CFX2
    if ( cfg.bCFX2 )
        {
        diag_message (DIAG_INIT, "cfx2_init");
        cfx2_init ();
        }
#endif
    if ( cfg.fn_sdxfdc[0] != NULL )
        {
        diag_message (DIAG_INIT, "sdxfdc_init (0)");
        sdxfdc_drvcfg(0, sdxcfg(cfg.tracks_sdxfdc[0]));
        sdxfdc_init(0, cfg.fn_sdxfdc[0]);
        }
    if ( cfg.fn_sdxfdc[1] != NULL )
        {
        diag_message (DIAG_INIT, "sdxfdc_init (1)");
        sdxfdc_drvcfg(1, sdxcfg(cfg.tracks_sdxfdc[1]));
        sdxfdc_init(1, cfg.fn_sdxfdc[1]);
        }
    
#ifdef HAVE_VGA
    if ( cfg.bVGA )
        {
        diag_message (DIAG_INIT, "vga_init");
        vga_init ();
        }
#endif

#ifdef HAVE_SID
    diag_message (DIAG_INIT, "sid_init");
	sid_init(cfg.sid_emu);
#endif

	if ( cfg.fn_print != NULL )
        {
        diag_message (DIAG_INIT, "print_init");
		print_init(cfg.fn_print);
        }

	if ( !cfg.tape_disable ) tape_patch (TRUE);

#ifdef HAVE_UI
    diag_message (DIAG_INIT, "ui_init");
	ui_init(cfg.ui_opts);
#endif

#ifdef HAVE_OSFS
	if ( fdxb )
        {
        diag_message (DIAG_INIT, "cpm_init_fdxb");
		cpm_init_fdxb();
        }
#endif

#ifndef SMALL_MEM
    diag_message (DIAG_INIT, "mem_snapshot");
	mem_snapshot();
#endif

    diag_message (DIAG_INIT, "ResetZ80");
	ResetZ80(&z80);
	z80.PC.W = (word) addr;
	z80.Trace = 1;
	z80.IPeriod = cfg.iperiod;
    diag_message (DIAG_INIT, "Z80Run");
	Z80Run (&z80); // RunZ80(&z80);
    diag_message (DIAG_INIT, "Z80Run Terminated");

	return 0;
	}
/*...e*/

unsigned long long get_Z80_clocks (void)
	{
	return	z80.IElapsed;
	}

/* For diagnostics */

Z80 *get_Z80_regs (void)
	{
	return &z80;
	}

