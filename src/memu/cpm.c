/*

cpm.c - CP/M emulation

We only emulate a subset of CP/M 2.2.
We support the console, but not reader or punch.
Basic printer support is provided. 
We don't support changing the CP/M I/O byte.
As disc emulation is done at the file level, the CP/M disc/directory layout is
not emulated, so programs like CONFIG.COM and RCHECK.COM won't work.
We only support A:, and any attempt to use other disks will fail.
Only host files matching the CP/M 8.3 upper-case naming convention are visible.
Directories are not supported (CP/M 2.2 has no such concept).
Readonly and system bits are not supported.
The user is 0, and cannot be changed.
Calling BDOS with illegal arguments can terminate the emulation.
Includes a change to map CP/M filenames with no extensions to host files
without a trailing dot, because on Windows this causes problems.

Note that we also emulate keyboard and screen related CBIOS calls,
and also keyboard and screen related low level driver calls.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#if defined(AIX) || defined(UNIX) || defined(SUN) || defined(MACOSX)
#include <unistd.h>
#else
#include <io.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include "dirt.h"

#include "Z80.h"
#include "types.h"
#include "diag.h"
#include "common.h"
#include "mem.h"
#include "mon.h"
#include "printer.h"
#include "cpm.h"

/*...vZ80\46\h:0:*/
/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vmem\46\h:0:*/
/*...vmon\46\h:0:*/
/*...vprinter\46\h:0:*/
/*...vcpm\46\h:0:*/
/*...e*/

/* The I/O byte returned is
     10        LIST=LPT:
       11      PUNCH=UP2:
         01    READER=UR2:
           01  CONSOLE=TTY:   */
#define	IOBYTE 0xbc

static BOOLEAN cpm_inited = FALSE;
static BOOLEAN cpm_inited_sdx = FALSE;
static BOOLEAN cpm_inited_fdxb = FALSE;
#define	DRIVE_PREFIX_LEN 500
static char *drive_a = NULL;
static BOOLEAN invert_case = FALSE;
static word dma_addr = 0x0080;
static const char *force_fn = NULL;

static BOOLEAN cpm_open_hack = FALSE;

#define	CPM_DPB_ADDR     0xff80
#define	CPM_DPB_ADDR_SDX 0xd800

void cpm_force_filename (const char *fn)
    {
    if ( force_fn != NULL ) free ((void *)force_fn);
    force_fn = fn;
    }

/*...sstr_upper:0:*/
static void str_upper(char *s)
	{
	for ( ; *s; s++ )
		if ( islower(*s) )
			*s = toupper(*s);
	}
/*...e*/
/*...sstr_invert:0:*/
static void str_invert(char *s)
	{
	for ( ; *s; s++ )
		if ( isupper(*s) )
			*s = tolower(*s);
		else if ( islower(*s) )
			*s = toupper(*s);
	}
/*...e*/

/*...sFCB layout:0:*/
/*
If a byte is at file position p in the file, then
its location within the sector is  p&0x0000007f          range 0-127
its current record             is (p&0x00003f80)>>7      range 0-127
the low byte of its extent     is (p&0x0007c000)>>14     range 0-31
the high byte of its extent    is (p&0x03fc0000)>>22     range 0-255
*/

/* FCB structure in memory */
#define	FCB_DR 0
#define	FCB_Fn 1
#define	FCB_Fn_LEN 8
#define	FCB_Tn 9
#define	FCB_Tn_LEN 3
#define	FCB_EX 12	/* bits 4-0  of extent, ie: (filepointer%524288)/16384 */
#define	FCB_S1 13
#define	FCB_S2 14	/* bits 12-5 of extent, ie: filepointer/524288 */
#define	FCB_RC 15
#define	FCB_CR 32	/* current record, ie: (filepointer%16384)/128 */
#define	FCB_R0 33	/* random record number low */
#define	FCB_R1 34	/* random record number middle */
#define	FCB_R2 35	/* random record number high */

/* 2 standard FCBs in zero page */
#define	FCB_ADDR1 0x005c
#define	FCB_ADDR2 0x006c
/*...e*/

/*...scheck_drive:0:*/
static void check_drive(word fcb_addr)
	{
	if ( mem_read_byte(fcb_addr+FCB_DR) != 0 &&
	     mem_read_byte(fcb_addr+FCB_DR) != 1 )
		fatal("we only allow the FCB DR (drive) to be 0 (=default) or 1 (=A:)");
	}
/*...e*/
/*...svalid_filename_char:0:*/
static BOOLEAN valid_filename_char(char ch, BOOLEAN wild)
	{
	/* Good cases early out quickly */
	if ( ch >= '0' && ch <= '9' ) return TRUE;
	if ( ch >= 'A' && ch <= 'Z' ) return TRUE;
	/* Actually works in CP/M, but we'll not allow it */
	if ( ch >= 'a' && ch <= 'z' ) return FALSE;
	/* Delete or characters with high bits are not supported */
	if ( ch > '~' ) return FALSE;
	/* Things prohibited by the CP/M manual */
	if ( ch == '<' ||
	     ch == '>' ||
	     ch == '.' ||
	     ch == ',' ||
	     ch == ';' ||
	     ch == ':' ||
	     ch == '=' ||
	     ch == '[' ||
	     ch == ']' ||
	     ch == '%' ||
	     ch == '|' ||
	     ch == '(' ||
	     ch == ')' ||
	     ch == '/' ||
	     ch == '\\' )
		return FALSE;
	/* Sometimes we'll allow wildcards */
	if ( ! wild && ch == '?' )
		return FALSE;
	/* * is expanded to ???? by the caller, so reject it here */
	if ( ch == '*' )
		return FALSE;
	/* Assuming its some ASCII punctuation we allow */
	return TRUE;
	}
/*...e*/
/*...sget_filename:0:*/
/* Resulting filenames look like this
     "QUASAR.COM"
     "DATA"
*/

static void get_filename(word fcb_addr, char *filename, BOOLEAN wild)
	{
	int i, j = 0, first_bad_char = -1;
	for ( i = 0; i < FCB_Fn_LEN; i++ )
		{
		char ch = (char) mem_read_byte(fcb_addr+FCB_Fn+i);
		if ( ch == ' ' )
			break;
		if ( ! valid_filename_char(ch, wild) && first_bad_char == -1 )
			first_bad_char = j;
		filename[j++] = ch;
		}
	if ( (char) mem_read_byte(fcb_addr+FCB_Tn) != ' ' )
		{
		filename[j++] = '.';
		for ( i = 0; i < FCB_Tn_LEN; i++ )
			{
			char ch = (char) mem_read_byte(fcb_addr+FCB_Tn+i);
			if ( ch == ' ' )
				break;
			if ( ! valid_filename_char(ch, wild) && first_bad_char == -1 )
				first_bad_char = j;
			filename[j++] = ch;
			}
		}
	filename[j++] = '\0';
	if ( first_bad_char != -1 )
		fatal("invalid character in FCB filename %s, at position %d", filename, first_bad_char);
	}
/*...e*/
/*...svalid_cpm_filename:0:*/
/* Valid filenames can include :-
     "FILE.TXT"
     "FILE."
     "FILE", as a shorthand for "FILE."
   But not
     ""
     ".TXT"
*/

static BOOLEAN valid_cpm_filename(
	const char *filename, char *fn, char *tn, BOOLEAN wild
	)
	{
	int i;
	int j = 0;
	for ( i = 0; i < FCB_Fn_LEN; i++ )
		if ( filename[j] == '\0' )
			{
			if ( j == 0 )
				return FALSE;
			while ( i < FCB_Fn_LEN )
				fn[i++] = ' ';
			for ( i = 0; i < FCB_Tn_LEN; i++ )
				tn[i] = ' ';
			return TRUE;
			}
		else if ( filename[j] == '.' )
			{
			if ( j == 0 )
				return FALSE;
			while ( i < FCB_Fn_LEN )
				fn[i++] = ' ';
			break;
			}
		else if ( valid_filename_char(filename[j], wild) )
			fn[i] = filename[j++];
		else if ( filename[j] == '*' && wild )
			{
			j++;
			while ( filename[j] != '.' && filename[j] != '\0' )
				j++;
			while ( i < FCB_Fn_LEN )
				fn[i++] = '?';
			break;
			}
		else
			return FALSE;
	if ( filename[j] != '.' )
		return FALSE;
	j++;
	for ( i = 0; i < FCB_Tn_LEN; i++ )
		if ( filename[j] == '\0' )
			{
			while ( i < FCB_Tn_LEN )
				tn[i++] = ' ';
			break;
			}
		else if ( valid_filename_char(filename[j], wild) )
			tn[i] = filename[j++];
		else if ( filename[j] == '*' && wild )
			{
			j++;
			while ( filename[j] != '\0' )
				j++;
			while ( i < FCB_Tn_LEN )
				tn[i++] = '?';
			break;
			}
		else
			return FALSE;
	return filename[j] == '\0';
	}
/*...e*/
/*...svalid_cpm_drive_and_filename:0:*/
/* Used when parsing command line arguments */

static BOOLEAN valid_cpm_drive_and_filename(
	const char *filename, byte *drive, char *fn, char *tn, BOOLEAN wild
	)
	{
	if ( isalpha(filename[0]) && filename[1] == ':' )
		{
		*drive = isupper(filename[0])
			? filename[0]-'A'+1
			: filename[0]-'a'+1;
		filename += 2;
		}
	else
		*drive = 1; /* A: */
	return valid_cpm_filename(filename, fn, tn, wild);
	}
/*...e*/
/*...sis_file:0:*/
static BOOLEAN is_file(const char *filename)
	{
	struct stat buf;
	if ( stat(filename, &buf) == -1 )
		return FALSE;
	return (buf.st_mode & S_IFREG) != 0;
	}
/*...e*/
/*...smatches_wild:0:*/
static BOOLEAN matches_wild(
	const char *fn,
	const char *tn,
	const char *fn_wild,
	const char *tn_wild
	)
	{
	int i;
	for ( i = 0; i < FCB_Fn_LEN; i++ )
		if ( fn[i] != fn_wild[i] && fn_wild[i] != '?' ) 
			return FALSE;
	for ( i = 0; i < FCB_Tn_LEN; i++ )
		if ( tn[i] != tn_wild[i] && tn_wild[i] != '?' ) 
			return FALSE;
	return TRUE;
	}
/*...e*/

/*...sOPENFILE:0:*/
typedef struct _OPENFILE OPENFILE;
struct _OPENFILE
	{
	OPENFILE *next;
	word addr;
	FILE *fp;
	};
static OPENFILE *openfiles = NULL;
/*...sopenfile_find:0:*/
static OPENFILE *openfile_find(word addr)
	{
	OPENFILE *openfile;
	for ( openfile = openfiles; openfile != NULL; openfile = openfile->next )
		if ( addr == openfile->addr )
			return openfile;
	return NULL;
	}
/*...e*/
/*...sopenfile_remember:0:*/
static void openfile_remember(OPENFILE *openfile)
	{
	openfile->next = openfiles;
	openfiles = openfile;
	}
/*...e*/
/*...sopenfile_forget:0:*/
static void openfile_forget(OPENFILE *openfile)
	{
	OPENFILE **last = &openfiles;
	OPENFILE *f;
	for ( f = openfiles; f != openfile; f = f->next )
		last = &(f->next);
	(*last) = f->next;
	}
/*...e*/
/*...sopenfile_clear:0:*/
static void openfile_clear(word addr)
	{
	OPENFILE *openfile;
	if ( (openfile = openfile_find(addr)) != NULL )
		{
		fclose(openfile->fp);
		openfile_forget(openfile);
		free(openfile);
		}
	}
/*...e*/
/*...e*/

/*...ssearch stuff:0:*/
typedef struct _MATCH MATCH;
struct _MATCH
	{
	MATCH *next;
	char fn[FCB_Fn_LEN];
	char tn[FCB_Tn_LEN];
	};

static MATCH *search_matches = NULL;
static word search_fcb_addr = 0;

/*...ssearch_clear:0:*/
/* Discard whats left of the snapshot of files visible to CP/M */

static void search_clear(void)
	{
	while ( search_matches != NULL )
		{
		MATCH *next = search_matches->next;
		free(search_matches);
		search_matches = next;
		}
	}
/*...e*/
/*...ssearch_build:0:*/
static void search_build(void)
	{
	DIRT *dirt;
	int rc;
	const char *filename;
	if ( (dirt = dirt_open(drive_a, &rc)) == NULL )
		fatal("can't search directory %s: %s", drive_a, dirt_error(rc));
	while ( (filename = dirt_next(dirt)) != NULL )
		{
		int len = (int) strlen(filename);
		if ( len >= 1 && filename[len-1] != '.' )
			/* Must be careful not to include files ending
			   with a ., because CP/M files with just a filename
			   but no extension are mapped to host files
			   without the trailing . */
			{
			char *filename2 = estrdup(filename);
			char fn[FCB_Fn_LEN];
			char tn[FCB_Tn_LEN];
			if ( invert_case )
				str_invert(filename2);
			if ( valid_cpm_filename(filename2, fn, tn, FALSE) )
				{
				char hostfilename[DRIVE_PREFIX_LEN+1+FCB_Fn_LEN+1+FCB_Tn_LEN+1];
				sprintf(hostfilename, "%s/%s", drive_a, filename);
				if ( is_file(hostfilename) )
					{
					MATCH *match = emalloc(sizeof(MATCH));
					match->next = search_matches;
					memcpy(match->fn, fn, FCB_Fn_LEN);
					memcpy(match->tn, tn, FCB_Tn_LEN);
					search_matches = match;
					}
				}
			free(filename2);
			}
		}
	dirt_close(dirt);
	}
/*...e*/
/*...ssearch_for:0:*/
static void search_for(Z80 *r, const char *which)
	{
	char fn_wild[FCB_Fn_LEN];
	char tn_wild[FCB_Tn_LEN];
	if ( mem_read_byte(search_fcb_addr+FCB_EX) != 0 )
		/* We don't support finding all extents of a file,
		   as we don't really model extents */
		fatal("BDOS search for %s fcb=0x%04x: FCB EX should be zero", which, search_fcb_addr);
	check_drive(search_fcb_addr);
	mem_read_block(search_fcb_addr+FCB_Fn, FCB_Fn_LEN, (byte *) fn_wild);
	mem_read_block(search_fcb_addr+FCB_Tn, FCB_Tn_LEN, (byte *) tn_wild);
	while ( search_matches != NULL )
		{
		MATCH *next = search_matches->next;
		if ( matches_wild(search_matches->fn, search_matches->tn, fn_wild, tn_wild) )
			{
			mem_write_byte(dma_addr+FCB_DR, 1); /* A: */
			mem_write_block(dma_addr+FCB_Fn, FCB_Fn_LEN, (byte *) search_matches->fn);
			mem_write_block(dma_addr+FCB_Tn, FCB_Tn_LEN, (byte *) search_matches->tn);
			r->AF.B.h = 0; /* Always slot 0 */
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS search for %s fcb=0x%04x (cmfn=%*.*s.%*.*s) found %*.*s.%*.*s",
				which,
				search_fcb_addr,
				FCB_Fn_LEN, FCB_Fn_LEN, fn_wild           , FCB_Tn_LEN, FCB_Tn_LEN, tn_wild,
				FCB_Fn_LEN, FCB_Fn_LEN, search_matches->fn, FCB_Tn_LEN, FCB_Tn_LEN, search_matches->tn
				);
			free(search_matches);
			search_matches = next;
			return;
			}
		free(search_matches);
		search_matches = next;
		}
	r->AF.B.h = 0xff;
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS search for %s fcb=0x%04x (cmfn=%*.*s.%*.*s) not found",
		which,
		search_fcb_addr,
		FCB_Fn_LEN, FCB_Fn_LEN, fn_wild, FCB_Tn_LEN, FCB_Tn_LEN, tn_wild
		);
	}
/*...e*/
/*...e*/

/*...sdriver_kbd_input:0:*/
static void driver_kbd_input(Z80 *r)
	{
	char ch = mon_kbd_read();
	r->AF.B.h = ch;
	mem_write_byte(0xf001, ch);
	diag_message(DIAG_CPM_DRIVER, "KBD driver returned 0x%02x", ch);
	}
/*...e*/
/*...sdriver_vdu_output:0:*/
static void driver_vdu_output(Z80 *r)
	{
	char ch = (char) (r->BC.B.l);
	mon_write(ch);
	diag_message(DIAG_CPM_DRIVER, "VDU driver output 0x%02x", ch);
	}
/*...e*/
/*...sdriver_kbd_status:0:*/
static void driver_kbd_status(Z80 *r)
	{
	if ( mon_kbd_status() )
		{
		r->AF.B.h = 0xff;
		r->AF.B.l &= ~Z_FLAG;
		diag_message(DIAG_CPM_DRIVER, "KBD driver key ready");
		}
	else
		{
		r->AF.B.h = 0x00;
		r->AF.B.l |= Z_FLAG;
		diag_message(DIAG_CPM_DRIVER, "KBD driver key not ready");
		}
	}
/*...e*/

/*...scbios_unsupported:0:*/
static void cbios_unsupported(const char *desc)
	{
	fatal("CBIOS %s not supported", desc);
	}
/*...e*/
/*...scbios_console_status:0:*/
static void cbios_console_status(Z80 *r)
	{
	if ( mon_kbd_status() )
		{
		r->AF.B.h = 0xff;
		diag_message(DIAG_CPM_CBIOS, "CBIOS console status returns key ready");
		}
	else
		{
		r->AF.B.h = 0x00;
		diag_message(DIAG_CPM_CBIOS, "CBIOS console status returns key not ready");
		}
	}
/*...e*/
/*...scbios_console_input:0:*/
static void cbios_console_input(Z80 *r)
	{
	char ch = mon_kbd_read();
	r->AF.B.h = ch;
	diag_message(DIAG_CPM_CBIOS, "CBIOS console input returned 0x%02x", ch);
	}
/*...e*/
/*...scbios_console_output:0:*/
static void cbios_console_output(Z80 *r)
	{
	char ch = (char) r->BC.B.l;
	mon_write(ch);
	diag_message(DIAG_CPM_CBIOS, "CBIOS console output 0x%02x", ch);
	}
/*...e*/
/*...scbios_printer_output:0:*/
static void cbios_printer_output(Z80 *r)
	{
	byte value = r->BC.B.l;
	print_byte(value);
	diag_message(DIAG_CPM_CBIOS, "CBIOS printer output 0x%02x", value);
	}
/*...e*/
/*...scbios_printer_status:0:*/
static void cbios_printer_status(Z80 *r)
	{
	if ( print_ready() )
		{
		r->AF.B.l = 0xff;
		diag_message(DIAG_CPM_CBIOS, "CBIOS printer status ready");
		}
	else
		{
		r->AF.B.l = 0x00;
		diag_message(DIAG_CPM_CBIOS, "CBIOS printer status ready");
		}
	}
/*...e*/

/*...sbdos_terminate:0:*/
static void bdos_terminate(Z80 *r)
	{
	terminate("BDOS terminate");
	}
/*...e*/
/*...sbdos_console_input:0:*/
static void bdos_console_input(Z80 *r)
	{
	char ch = mon_kbd_read();
	mon_write(ch);
	r->AF.B.h = r->HL.B.l = ch;
	diag_message(DIAG_CPM_BDOS_OTHER, "BDOS console input returned 0x%02", ch);
	}
/*...e*/
/*...sbdos_console_output:0:*/
static void bdos_console_output(Z80 *r)
	{
	char ch = (char) r->DE.B.l;
	mon_write(ch);
	diag_message(DIAG_CPM_BDOS_OTHER, "BDOS console output 0x%02x", ch);
	}
/*...e*/
/*...sbdos_printer_output:0:*/
/* Based on code from William Brendling. */
static void bdos_printer_output(Z80 *r)
	{
	byte value = r->DE.B.l;
	print_byte(value);
	diag_message(DIAG_CPM_BDOS_OTHER, "BDOS printer output 0x%02x", value);
	}
/*...e*/
/*...sbdos_direct_console_io:0:*/
static void bdos_direct_console_io(Z80 *r)
	{
	if ( r->DE.B.l == 0xff )
		{
		char ch = mon_kbd_read_non_wait();
		r->AF.B.h = ch;
		diag_message(DIAG_CPM_BDOS_OTHER, "BDOS direct console io returns 0x%02x", ch);
		}
	else
		/* Any E value not understood results in echoing E */
		{
		char ch = (char) r->DE.B.l;
		mon_write(ch);
		diag_message(DIAG_CPM_BDOS_OTHER, "BDOS direct console io output 0x%02x", ch);
		}
	}
/*...e*/
/*...sbdos_get_io_byte:0:*/
static void bdos_get_io_byte(Z80 *r)
	{
	byte iobyte = IOBYTE;
	r->AF.B.h = iobyte;
	diag_message(DIAG_CPM_BDOS_OTHER, "BDOS get io byte returned 0x%02x", iobyte);
	}
/*...e*/
/*...sbdos_console_output_string:0:*/
static void bdos_console_output_string(Z80 *r)
	{
	word de = r->DE.W;
	char ch;
	while ( (ch = mem_read_byte(de++)) != '$' )
		mon_write(ch);
	diag_message(DIAG_CPM_BDOS_OTHER, "BDOS console output string");
	}
/*...e*/
/*...sbdos_console_input_string:0:*/
static void bdos_console_input_string(Z80 *r)
	{
	word de = r->DE.W;
	word addr = ( de != 0 ) ? de : dma_addr;
	int size = mem_read_byte(addr);
	int n = ( de != 0 ) ? 0 : mem_read_byte(addr+1);
	int ch;
	while ( (ch = mon_kbd_read()) != '\r' )
		if ( ch == '\b' )
			{
			if ( n > 0 )
				{
				--n;
				mon_write('\b');
				mon_write(' ');
				mon_write('\b');
				}
			}
		else
			{
			if ( n < size )
				{
				mem_write_byte(addr+2+n++, (byte) ch);
				mon_write(ch);
				}
			}
	mem_write_byte(addr+1, (byte) n);
	diag_message(DIAG_CPM_BDOS_OTHER, "BDOS console input string");
	}
/*...e*/
/*...sbdos_console_status:0:*/
static void bdos_console_status(Z80 *r)
	{
	if ( mon_kbd_status() )
		{
		r->AF.B.h = r->HL.B.l = 0xff;
		diag_message(DIAG_CPM_CBIOS, "BDOS console status returns key ready");
		}
	else
		{
		r->AF.B.h = r->HL.B.l = 0x00;
		diag_message(DIAG_CPM_CBIOS, "BDOS console status returns key not ready");
		}
	}
/*...e*/
/*...sbdos_return_version:0:*/
static void bdos_return_version(Z80 *r)
	{
	r->BC.B.h = r->HL.B.h = 0x00; /* system type = 8080, plain CP/M */
	r->AF.B.h = r->HL.B.l = 0x22; /* CP/M 2.2 */
	diag_message(DIAG_CPM_BDOS_OTHER, "BDOS return version returned 8080, CP/M 2.2");
	}
/*...e*/
/*...sbdos_reset_disc_drives:0:*/
/* Reset disc drives should
     log out all disc and empties buffers (so we close files and searches)
   and also
     set currently selected drive to A:
     any R/O drives become R/W
   however, we silently ignore this. */

static void bdos_reset_disc_drives(Z80 *r)
	{
	while ( openfiles != NULL )
		{
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS reset disc drives, closing open file fcb=0x%04x", openfiles->addr);
		openfile_clear(openfiles->addr);
		}
	if ( search_matches != NULL )
		{
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS reset disc drives, closing open search fcb=0x%04x", search_fcb_addr);
		search_clear();
		}
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS reset disc drives");
	}
/*...e*/
/*...sbdos_select_disc:0:*/
/* Select disc.
   We only support drive A: */

static void bdos_select_disc(Z80 *r)
	{
	int disc = r->DE.B.l;
	if ( disc == 0 )
		{
		r->AF.B.h = r->HL.B.l = 0;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS select disc A:");
		}
	else
		{
		r->AF.B.h = r->HL.B.l = 0xff;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS select disc %c:, return error", disc+'A');
		}
	}
/*...e*/
/*...sbdos_open_file:0:*/
static void bdos_open_file(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	char filename[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	char filename2[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	char hostfilename[DRIVE_PREFIX_LEN+1+FCB_Fn_LEN+1+FCB_Tn_LEN+1];
    const char *hostfn = hostfilename;
	OPENFILE *openfile;
	FILE *fp;
	check_drive(fcb_addr);
	get_filename(fcb_addr, filename, FALSE);
	if ( cpm_open_hack )
		{
		mem_write_byte(fcb_addr+FCB_EX, 0);
		mem_write_byte(fcb_addr+FCB_S1, 0);
		mem_write_byte(fcb_addr+FCB_S2, 0);
		mem_write_byte(fcb_addr+FCB_RC, 0);
		}
	else
		{
		if ( mem_read_byte(fcb_addr+FCB_EX) != 0 ) fatal("BDOS open file: FCB EX should be zero");
		if ( mem_read_byte(fcb_addr+FCB_S1) != 0 ) fatal("BDOS open file: FCB S1 should be zero");
		if ( mem_read_byte(fcb_addr+FCB_S2) != 0 ) fatal("BDOS open file: FCB S2 should be zero");
		if ( mem_read_byte(fcb_addr+FCB_RC) != 0 ) fatal("BDOS open file: FCB RC should be zero");
		}
	openfile_clear(fcb_addr);
	search_clear();
	strcpy(filename2, filename);
	if ( invert_case )
		str_invert(filename2);
    if ( force_fn == NULL )
        sprintf(hostfilename, "%s/%s", drive_a, filename2);
    else
        hostfn = force_fn;
	if ( (fp = fopen(hostfn, "rb+")) != NULL ) 
		{
		openfile = emalloc(sizeof(OPENFILE));
		openfile->addr = fcb_addr;
		openfile->fp   = fp;
		openfile_remember(openfile);
		r->AF.B.h = 0;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS open file fcb=0x%04x (cfn=%s, hfn=%s)", fcb_addr, filename, hostfilename);
		}
	else
		{
		r->AF.B.h = 0xff;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS open file 0x%04x (cfn=%s, hfn=%s) failed", fcb_addr, filename, hostfilename);
		}
    if ( force_fn != NULL )
        {
        free ((void *)force_fn);
        force_fn = NULL;
        }
	}
/*...e*/
/*...sbdos_close_file:0:*/
static void bdos_close_file(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	OPENFILE *openfile;
	if ( (openfile = openfile_find(fcb_addr)) != NULL )
		{
		fclose(openfile->fp);
		openfile_forget(openfile);
		free(openfile);
		r->AF.B.h = 0;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS close file fcb=0x%04x", fcb_addr);
		}
	else
		{
		r->AF.B.h = 0xff;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS close file fcb=0x%04x failed", fcb_addr);
		}
	}
/*...e*/
/*...sbdos_search_for_first:0:*/
static void bdos_search_for_first(Z80 *r)
	{
	search_fcb_addr = r->DE.W;
	openfile_clear(search_fcb_addr);
	search_clear();
	search_build();
	search_for(r, "first");
	}
/*...e*/
/*...sbdos_search_for_next:0:*/
/* Note that I don't beleive the statement on
   http://www.seasip.demon.co.uk/Cpm/bdos.html for F_SNEXT regarding DE.
   Looking at cpm22.asm, I see no evidence search-for-next ever uses anything
   other than the FCB address saved by search-for-first. */
   
static void bdos_search_for_next(Z80 *r)
	{
	search_for(r, "next");
	}
/*...e*/
/*...sbdos_delete_files:0:*/
static void bdos_delete_files(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	char fn_wild[FCB_Fn_LEN];
	char tn_wild[FCB_Tn_LEN];
	DIRT *dirt;
	int rc;
	int n;
	const char *filename;
	openfile_clear(fcb_addr);
	search_clear();
	check_drive(fcb_addr);
	mem_read_block(fcb_addr+FCB_Fn, FCB_Fn_LEN, (byte *) fn_wild);
	mem_read_block(fcb_addr+FCB_Tn, FCB_Tn_LEN, (byte *) tn_wild);
	if ( (dirt = dirt_open(drive_a, &rc)) == NULL )
		fatal("can't search directory %s: %s", drive_a, dirt_error(rc));
	r->AF.B.h = 0;
	n = 0;
	while ( (filename = dirt_next(dirt)) != NULL )
		{
		char fn[FCB_Fn_LEN];
		char tn[FCB_Tn_LEN];
		char *filename2 = estrdup(filename);
		if ( invert_case )
			str_invert(filename2);
		if ( valid_cpm_filename(filename2, fn, tn, FALSE) &&
		     matches_wild(fn, tn, fn_wild, tn_wild) )
			{
			char hostfilename[DRIVE_PREFIX_LEN+1+FCB_Fn_LEN+1+FCB_Tn_LEN+1];
			sprintf(hostfilename, "%s/%s", drive_a, filename);
			if ( is_file(hostfilename) )
				{
				++n;
				if ( remove(hostfilename) == 0 )
					diag_message(DIAG_CPM_BDOS_FILE, "BDOS delete files fcb=0x%04x (cmfn=%*.*s.%*.*s, cfn=%*.*s.%*.*s, hfn=%s)",
						fcb_addr,
						FCB_Fn_LEN, FCB_Fn_LEN, fn_wild, FCB_Tn_LEN, FCB_Tn_LEN, tn_wild,
						FCB_Fn_LEN, FCB_Fn_LEN, fn     , FCB_Tn_LEN, FCB_Tn_LEN, tn     ,
						hostfilename
						);
				else
					{
					r->AF.B.h = 0xff;
					diag_message(DIAG_CPM_BDOS_FILE, "BDOS delete files fcb=0x%04x (cmfn=%*.*s.%*.*s, cfn=%*.*s.%*.*s, hfn=%s) failed",
						fcb_addr,
						FCB_Fn_LEN, FCB_Fn_LEN, fn_wild, FCB_Tn_LEN, FCB_Tn_LEN, tn_wild,
						FCB_Fn_LEN, FCB_Fn_LEN, fn     , FCB_Tn_LEN, FCB_Tn_LEN, tn     ,
						hostfilename
						);
					}
				}
			}
		free(filename2);
		}
	dirt_close(dirt);
	if ( n == 0 )
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS delete files fcb=0x%04x (cmfn=%*.*s.%*.*s) no matching files",
			fcb_addr,
			FCB_Fn_LEN, FCB_Fn_LEN, fn_wild, FCB_Tn_LEN, FCB_Tn_LEN, tn_wild
			);
	}
/*...e*/
/*...sbdos_read_next_record:0:*/
static void bdos_read_next_record(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	OPENFILE *openfile;
	if ( (openfile = openfile_find(fcb_addr)) != NULL )
		{
		byte cr, ex, s2;
		long fileaddr;
		cr = mem_read_byte(fcb_addr+FCB_CR);
		if ( cr > 127 )
			fatal("BDOS read next record: FCB CR must be between 0 and 127, its %d", cr);
		ex = mem_read_byte(fcb_addr+FCB_EX);
		if ( ex > 31 )
			fatal("BDOS read next record: FCB EX must be between 0 and 31, its %d", ex);
		s2 = mem_read_byte(fcb_addr+FCB_S2);
		fileaddr = ((unsigned long)s2<<19)
		         | ((unsigned long)ex<<14)
		         | ((unsigned long)cr<< 7);
		if ( fseek(openfile->fp, fileaddr, SEEK_SET) != 0 )
			{
			r->AF.B.h = 0x01; /* end of file */
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS read next record fcb=0x%04x fileaddr=%lu returned EOF (seeking)", fcb_addr, fileaddr);
			}
		else
			{
			byte buf[128];
			long n = (long) fread(buf, 1, 128, openfile->fp);
			if ( n <= 0 )
				{
				r->AF.B.h = 0x01; /* end of file */
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS read next record fcb=0x%04x fileaddr=%lu returned EOF (reading)", fcb_addr, fileaddr);
				}
			else
				{
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS read next record fcb=0x%04x fileaddr=%lu n=%ld", fcb_addr, fileaddr, n);
				memset(buf+n, 0x1a, 128-n);
				mem_write_block(dma_addr, 128, buf);
				fileaddr += 128;
				s2 = (byte) ((fileaddr>>19)&0xff);
				ex = (byte) ((fileaddr>>14)&0x1f);
				cr = (byte) ((fileaddr>> 7)&0x7f);
				mem_write_byte(fcb_addr+FCB_S2, s2);
				mem_write_byte(fcb_addr+FCB_EX, ex);
				mem_write_byte(fcb_addr+FCB_CR, cr);
				r->AF.B.h = 0; /* ok */
				}
			}
		}
	else
		{
		r->AF.B.h = 0x09; /* invalid FCB */
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS read next record fcb=0x%04x returned invalid FCB", fcb_addr);
		}
	}
/*...e*/
/*...sbdos_write_next_record:0:*/
static void bdos_write_next_record(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	OPENFILE *openfile;
	if ( (openfile = openfile_find(fcb_addr)) != NULL )
		{
		byte cr, ex, s2;
		long fileaddr;
		cr = mem_read_byte(fcb_addr+FCB_CR);
		if ( cr > 127 )
			fatal("BDOS write next record: FCB CR must be between 0 and 127, its %d", cr);
		ex = mem_read_byte(fcb_addr+FCB_EX);
		if ( ex > 31 )
			fatal("BDOS write next record: FCB EX must be between 0 and 31, its %d", ex);
		s2 = mem_read_byte(fcb_addr+FCB_S2);
		fileaddr = ((unsigned long)s2<<19)
		         | ((unsigned long)ex<<14)
		         | ((unsigned long)cr<< 7);
		if ( fseek(openfile->fp, fileaddr, SEEK_SET) != 0 )
			{
			r->AF.B.h = 0x01; /* end of file */
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS write next record fcb=0x%04x fileaddr=%lu returned EOF", fcb_addr, fileaddr);
			}
		else
			{
			byte buf[128];
			long n;
			mem_read_block(dma_addr, 128, buf);
			n = (long) fwrite(buf, 1, 128, openfile->fp);
			if ( n < 128 )
				{
				r->AF.B.h = 0x02; /* disk full */
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS write next record fcb=0x%04x fileaddr=%lu returned disk full", fcb_addr, fileaddr);
				}
			else
				{
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS write next record fcb=0x%04x fileaddr=%lu", fcb_addr, fileaddr);
				fileaddr += 128;
				s2 = (byte) ((fileaddr>>19)&0xff);
				ex = (byte) ((fileaddr>>14)&0x1f);
				cr = (byte) ((fileaddr>> 7)&0x7f);
				mem_write_byte(fcb_addr+FCB_S2, s2);
				mem_write_byte(fcb_addr+FCB_EX, ex);
				mem_write_byte(fcb_addr+FCB_CR, cr);
				r->AF.B.h = 0; /* ok */
				}
			}
		}
	else
		{
		r->AF.B.h = 0x09; /* invalid FCB */
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS read next record fcb=0x%04x returned invalid FCB", fcb_addr);
		}
	}
/*...e*/
/*...sbdos_make_file:0:*/
static void bdos_make_file(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	char filename[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	char filename2[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	char hostfilename[DRIVE_PREFIX_LEN+1+FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	OPENFILE *openfile;
	FILE *fp;
	check_drive(fcb_addr);
	get_filename(fcb_addr, filename, FALSE);
	mem_write_byte(fcb_addr+FCB_EX, 0);
	mem_write_byte(fcb_addr+FCB_S1, 0);
	mem_write_byte(fcb_addr+FCB_S2, 0);
	mem_write_byte(fcb_addr+FCB_RC, 0);
	openfile_clear(fcb_addr);
	search_clear();
	strcpy(filename2, filename);
	if ( invert_case )
		str_invert(filename2);
	sprintf(hostfilename, "%s/%s", drive_a, filename2);
	if ( (fp = fopen(hostfilename, "wb+")) != NULL ) 
		{
		openfile = emalloc(sizeof(OPENFILE));
		openfile->addr = fcb_addr;
		openfile->fp   = fp;
		openfile_remember(openfile);
		r->AF.B.h = 0;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS make file fcb=0x%04x (cfn=%s, hfn=%s)", fcb_addr, filename, hostfilename);
		}
	else
		{
		r->AF.B.h = 0xff;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS make file fcb=0x%04x (cfn=%s, hfn=%s) failed", fcb_addr, filename, hostfilename);
		}
	}
/*...e*/
/*...sbdos_rename_file:0:*/
static void bdos_rename_file(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	char old_filename[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	char new_filename[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	char old_hostfilename[DRIVE_PREFIX_LEN+1+FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	char new_hostfilename[DRIVE_PREFIX_LEN+1+FCB_Fn_LEN+1+FCB_Tn_LEN+1];
	openfile_clear(fcb_addr);
	search_clear();
	check_drive(fcb_addr);
	get_filename(fcb_addr, old_filename, FALSE);
	if ( invert_case )
		str_invert(old_filename);
	sprintf(old_hostfilename, "%s/%s", drive_a, old_filename);
	check_drive(fcb_addr+16);
	get_filename(fcb_addr+16, new_filename, FALSE);
	if ( invert_case )
		str_invert(new_filename);
	sprintf(new_hostfilename, "%s/%s", drive_a, new_filename);
	if ( rename(old_hostfilename, new_hostfilename) == 0 )
		{
		r->AF.B.h = 0;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS rename file fcb=0x%04x (ocfn=%s, ohfn=%s, ncfn=%s, nhfn=%s)",
			fcb_addr, old_filename, old_hostfilename, new_filename, new_hostfilename);
		}
	else
		{
		r->AF.B.h = 0xff;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS rename file fcb=0x%04x (ocfn=%s, ohfn=%s, ncfn=%s, nhfn=%s) failed",
			fcb_addr, old_filename, old_hostfilename, new_filename, new_hostfilename);
		}
	}
/*...e*/
/*...sbdos_return_logged_drives:0:*/
static void bdos_return_logged_drives(Z80 *r)
	{
	r->HL.W = 0x0001; /* A: is logged in */
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS return logged drives returned A:");
	}
/*...e*/
/*...sbdos_return_current_drive:0:*/
static void bdos_return_current_drive(Z80 *r)
	{
	r->AF.B.h = 0; /* A: */
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS return current drive returned A:");
	}
/*...e*/
/*...sbdos_set_dma_address:0:*/
static void bdos_set_dma_address(Z80 *r)
	{
	dma_addr = r->DE.W;
	if ( dma_addr > 0xff00 - 0x80 )
		fatal("BDOS set DMA address: 0x%04x is too high", dma_addr);
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS set dma address to 0x%04x", dma_addr);
	}
/*...e*/
/*...sbdos_return_ro_drives:0:*/
static void bdos_return_ro_drives(Z80 *r)
	{
	r->HL.W = 0; /* No drives are R/O */
	}
/*...e*/
/*...sbdos_set_file_attrs:0:*/
static void bdos_set_file_attrs(Z80 *r)
	{
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS set file attrs (silently ignored)");
	r->AF.B.h = 0x00;
	}
/*...e*/
/*...sbdos_get_dpb:0:*/
static void bdos_get_dpb(Z80 *r)
	{
	r->HL.W = cpm_inited_sdx ? CPM_DPB_ADDR_SDX : CPM_DPB_ADDR;
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS get DPB returned dummy result");
	}
/*...e*/
/*...sbdos_get_set_user:0:*/
static void bdos_get_set_user(Z80 *r)
	{
	if ( r->DE.B.l == 0xff )
		{
		r->AF.B.h = 0; /* User 0 */
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS get user returned 0");
		}
	else if ( r->DE.B.l == 0 )
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS set user to 0");
	else
		fatal("BDOS get/set user: can't set user %d", r->DE.B.l);
	}
/*...e*/
/*...sbdos_read_random_record:0:*/
/* OK, so we cheat a little here.
   We don't distinguish between reading unwritten extents and unwritten data
   in an extent, we'll just return error number 1 (reading unwritten data). */

static void bdos_read_random_record(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	OPENFILE *openfile;
	if ( (openfile = openfile_find(fcb_addr)) != NULL )
		{
		byte r0 = mem_read_byte(fcb_addr+FCB_R0);
		byte r1 = mem_read_byte(fcb_addr+FCB_R1);
		byte r2 = mem_read_byte(fcb_addr+FCB_R2);
		long fileaddr = ((unsigned long)r2<<23)
		              | ((unsigned long)r1<<15)
		              | ((unsigned long)r0<< 7);
		if ( fseek(openfile->fp, fileaddr, SEEK_SET) != 0 )
			{
			r->AF.B.h = 0x01; /* end of file */
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS read random record fcb=0x%04x fileaddr=%lu returned EOF (seeking)", fcb_addr, fileaddr);
			}
		else
			{
			byte buf[128];
			long n = (long) fread(buf, 1, 128, openfile->fp);
			if ( n <= 0 )
				{
				r->AF.B.h = 0x01; /* end of file */
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS read random record fcb=0x%04x fileaddr=%lu returned EOF (reading)", fcb_addr, fileaddr);
				}
			else
				{
				byte s2, ex, cr;
				memset(buf+n, 0x1a, 128-n);
				mem_write_block(dma_addr, 128, buf);
				/* Allow read/write next to work from here */
				s2 = (byte) ((fileaddr>>19)&0xff);
				ex = (byte) ((fileaddr>>14)&0x1f);
				cr = (byte) ((fileaddr>> 7)&0x7f);
				mem_write_byte(fcb_addr+FCB_S2, s2);
				mem_write_byte(fcb_addr+FCB_EX, ex);
				mem_write_byte(fcb_addr+FCB_CR, cr);
				r->AF.B.h = 0; /* ok */
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS read random record fcb=0x%04x fileaddr=%lu n=%ld", fcb_addr, fileaddr, n);
				}
			}
		}
	else
		{
		r->AF.B.h = 0x09; /* invalid FCB */
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS read random record fcb=0x%04x returned invalid FCB", fcb_addr);
		}
	}
/*...e*/
/*...sbdos_write_random_record:0:*/
/* OK, so we cheat a little here.
   We don't distinguish between the various kinds of failure.
   Error 2 (disk full) can be returned. */

static void bdos_write_random_record(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	OPENFILE *openfile;
	if ( (openfile = openfile_find(fcb_addr)) != NULL )
		{
		byte r0 = mem_read_byte(fcb_addr+FCB_R0);
		byte r1 = mem_read_byte(fcb_addr+FCB_R1);
		byte r2 = mem_read_byte(fcb_addr+FCB_R2);
		long fileaddr = ((unsigned long)r2<<23)
		              | ((unsigned long)r1<<15)
		              | ((unsigned long)r0<< 7);
		if ( fseek(openfile->fp, fileaddr, SEEK_SET) != 0 )
			{
			r->AF.B.h = 0x01; /* end of file */
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS write random record fcb=0x%04x fileaddr=%lu returned EOF", fcb_addr, fileaddr);
			}
		else
			{
			byte buf[128];
			long n;
			mem_read_block(dma_addr, 128, buf);
			n = (long) fwrite(buf, 1, 128, openfile->fp);
			if ( n < 128 )
				{
				r->AF.B.h = 0x02; /* disk full */
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS write random record fcb=0x%04x fileaddr=%lu returned disk full", fcb_addr, fileaddr);
				}
			else
				{
				/* Allow read/write next to work from here */
				byte s2 = (byte) ((fileaddr>>19)&0xff);
				byte ex = (byte) ((fileaddr>>14)&0x1f);
				byte cr = (byte) ((fileaddr>> 7)&0x7f);
				mem_write_byte(fcb_addr+FCB_S2, s2);
				mem_write_byte(fcb_addr+FCB_EX, ex);
				mem_write_byte(fcb_addr+FCB_CR, cr);
				r->AF.B.h = 0; /* ok */
				diag_message(DIAG_CPM_BDOS_FILE, "BDOS write random record fcb=0x%04x fileaddr=%lu", fcb_addr, fileaddr);
				}
			}
		}
	else
		{
		r->AF.B.h = 0x09; /* invalid FCB */
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS write random record fcb=0x%04x returned invalid FCB", fcb_addr);
		}
	}
/*...e*/
/*...sbdos_file_size:0:*/
static void bdos_file_size(Z80 *r)
	{
	word fcb_addr = r->DE.W;
	OPENFILE *openfile;
	long fileaddr;
	if ( (openfile = openfile_find(fcb_addr)) != NULL )
		{
		long fileaddr2 = ftell(openfile->fp);
		fseek(openfile->fp, 0, SEEK_END);
		fileaddr = ftell(openfile->fp);
		fseek(openfile->fp, fileaddr2, SEEK_SET);
		r->AF.B.h = 0;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS file size fcb=0x%04x (open) returns %ld", fcb_addr, fileaddr);
		}
	else
		{
		char filename[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
		char filename2[FCB_Fn_LEN+1+FCB_Tn_LEN+1];
		char hostfilename[DRIVE_PREFIX_LEN+1+FCB_Fn_LEN+1+FCB_Tn_LEN+1];
		FILE *fp;
		check_drive(fcb_addr);
		get_filename(fcb_addr, filename, FALSE);
		strcpy(filename2, filename);
		if ( invert_case )
			str_invert(filename2);
		sprintf(hostfilename, "%s/%s", drive_a, filename2);
		if ( (fp = fopen(hostfilename, "rb+")) == NULL ) 
			{
			r->AF.B.h = 0xff;
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS file size 0x%04x (cfn=%s, hfn=%s) failed", fcb_addr, filename, hostfilename);
			return;
			}
		fseek(fp, 0, SEEK_END);
		fileaddr = ftell(fp);
		fclose(fp);
		r->AF.B.h = 0;
		diag_message(DIAG_CPM_BDOS_FILE, "BDOS file size fcb=0x%04x (cfn=%s, hfn=%s) returns %ld", fcb_addr, filename, hostfilename, fileaddr);
		}

	if ( fileaddr & 0x7f )
		fileaddr = ( (fileaddr+0x7f) & ~0x7f );

	{
	byte s2 = (byte) ((fileaddr>>19)&0xff);
	byte ex = (byte) ((fileaddr>>14)&0x1f);
	byte cr = (byte) ((fileaddr>> 7)&0x7f);
	byte r2 = (byte) ((fileaddr>>23)&0xff);
	byte r1 = (byte) ((fileaddr>>15)&0xff);
	byte r0 = (byte) ((fileaddr>> 7)&0xff);
	mem_write_byte(fcb_addr+FCB_S2, s2);
	mem_write_byte(fcb_addr+FCB_EX, ex);
	mem_write_byte(fcb_addr+FCB_CR, cr);
	mem_write_byte(fcb_addr+FCB_R2, r2);
	mem_write_byte(fcb_addr+FCB_R1, r1);
	mem_write_byte(fcb_addr+FCB_R0, r0);
	}

	}
/*...e*/
/*...sbdos_reset_specific_discs:0:*/
/* Like bdos_reset_disc_drives, except for specific drives.
   As we only support A:, we'll look to see if that was requested. */

static void bdos_reset_specific_discs(Z80 *r)
	{
	if ( r->DE.W & 0x0001 )
		{
		while ( openfiles != NULL )
			{
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS reset specific disc drives, closing open file fcb=0x%04x", openfiles->addr);
			openfile_clear(openfiles->addr);
			}
		if ( search_matches != NULL )
			{
			diag_message(DIAG_CPM_BDOS_FILE, "BDOS reset specific disc drives, closing open search fcb=0x%04x", search_fcb_addr);
			search_clear();
			}
		}
	diag_message(DIAG_CPM_BDOS_FILE, "BDOS reset specific disc drives");
	}
/*...e*/

/*...scpm_bdos:0:*/
static void cpm_bdos(Z80 *r)
	{
	byte c = r->BC.B.l;
	switch ( c )
		{
		case  0: bdos_terminate            (r); break;
		case  1: bdos_console_input        (r); break;
		case  2: bdos_console_output       (r); break;
		case  5: bdos_printer_output       (r); break;
		case  6: bdos_direct_console_io    (r); break;
		case  7: bdos_get_io_byte          (r); break;
		case  9: bdos_console_output_string(r); break;
		case 10: bdos_console_input_string (r); break;
		case 11: bdos_console_status       (r); break;
		case 12: bdos_return_version       (r); break;
		case 13: bdos_reset_disc_drives    (r); break;
		case 14: bdos_select_disc          (r); break;
		case 15: bdos_open_file            (r); break;
		case 16: bdos_close_file           (r); break;
		case 17: bdos_search_for_first     (r); break;
		case 18: bdos_search_for_next      (r); break;
		case 19: bdos_delete_files         (r); break;
		case 20: bdos_read_next_record     (r); break;
		case 21: bdos_write_next_record    (r); break;
		case 22: bdos_make_file            (r); break;
		case 23: bdos_rename_file          (r); break;
		case 24: bdos_return_logged_drives (r); break;
		case 25: bdos_return_current_drive (r); break;
		case 26: bdos_set_dma_address      (r); break;
		case 29: bdos_return_ro_drives     (r); break;
		case 30: bdos_set_file_attrs       (r); break;
		case 31: bdos_get_dpb              (r); break;
		case 32: bdos_get_set_user         (r); break;
		case 33: bdos_read_random_record   (r); break;
		case 34: bdos_write_random_record  (r); break;
		case 35: bdos_file_size            (r); break;
		case 37: bdos_reset_specific_discs (r); break;
		case  3: /* aux reader input */
		case  4: /* aux punch output */
		case  8: /* set I/O byte */
		case 27: /* return address of allocation map */
		case 28: /* software write protect current disc */
		case 36: /* update random access pointer */
		case 40: /* write random with zero fill */
			/* These are a problem, as the caller is probably
			   relying their behaviour */
			fatal("unimplemented BDOS call %d", c);
		default:
			/* These may not be a problem, so long as we
			   fail in the same way CP/M 2.2 would have.
			   Look in cpm22.z80, at FBASE1 and GOBACK. */
			r->HL.W   = 0; /* status will be 0 */
			r->AF.B.h = 0; /* CP/M 1.4 compatability */
			r->BC.B.h = 0; /* CP/M 1.4 compatability */
			diag_message(DIAG_CPM_BDOS_OTHER, "unimplemented non-CP/M 2.2 BDOS call %d", c);
			break;
		}
	}
/*...e*/
/*...scpm_patch:0:*/
BOOLEAN cpm_patch(Z80 *r)
	{
	if ( cpm_inited )
		switch ( (word) (r->PC.W-2) )
			{
			case 0xfdfe:
				/* Probably direct use of driver code
				   normally found in high memory,
				   which isn't in our emulation. */
				fatal("jump to high memory, caught at 0xfdfe");
			case 0xfe00:
				cpm_bdos(r);
				return TRUE;
			case 0xfefe:
				/* Probably direct use of driver code
				   normally found in high memory,
				   which isn't in our emulation. */
				fatal("jump to high memory, probably above 0xfe03, caught at 0xfefe");
			case 0xff00: terminate("CBIOS cold start");
			case 0xff03: terminate("CBIOS warm boot");
			case 0xff06: cbios_console_status(r); return TRUE;
			case 0xff09: cbios_console_input (r); return TRUE;
			case 0xff0c: cbios_console_output(r); return TRUE;
			case 0xff0f: cbios_printer_output(r); return TRUE;
			case 0xff12: cbios_unsupported("punch output");
			case 0xff15: cbios_unsupported("reader input");
			case 0xff18: cbios_unsupported("move disc head to track 0");
			case 0xff1b: cbios_unsupported("select disc drive");
			case 0xff1e: cbios_unsupported("set track number");
			case 0xff21: cbios_unsupported("set sector number");
			case 0xff24: cbios_unsupported("set DMA address");
			case 0xff27: cbios_unsupported("read a sector");
			case 0xff2a: cbios_unsupported("write a sector");
			case 0xff2d: cbios_printer_status(r); return TRUE;
			case 0xff30: cbios_unsupported("sector translation for skewing");
			case 0xffce:
				/* Possibly an attempt to jump to
				   CBIOS code through a vector. */
				fatal("jump to high memory, probably above 0xff33, caught at 0xffce");
			case 0xffd0: driver_kbd_input (r); return TRUE;
			case 0xffd3: driver_vdu_output(r); return TRUE;
			case 0xffd6: driver_kbd_status(r); return TRUE;
			case 0xfffa:
				/* Possibly an attempt to jump to
				   driver code through a vector. */
				fatal("jump to high memory, probably above 0xffd9. caught at 0xfffa");
			case 0xfffc:
				terminate("call to system initialize");
			}
	return FALSE;
	}
/*...e*/
/*...scpm_patch_sdx:0:*/
BOOLEAN cpm_patch_sdx(Z80 *r)
	{
	if ( cpm_inited_sdx )
		switch ( (word) (r->PC.W-2) )
			{
			case 0xd706:
				cpm_bdos(r);
				return TRUE;
			}
	return FALSE;
	}
/*...e*/
/*...scpm_patch_fdxb:0:*/
BOOLEAN cpm_patch_fdxb(Z80 *r)
	{
	if ( cpm_inited_fdxb )
		switch ( (word) (r->PC.W-2) )
			{
			case 0x6308:
				cpm_bdos(r);
				return TRUE;
			}
	return FALSE;
	}
/*...e*/

/*...scpm_place_jp:0:*/
static void cpm_place_jp(word addr, word dest)
	{
	mem_write_byte(addr  , 0xc3);
	mem_write_byte(addr+1, (byte) (dest&0xff));
	mem_write_byte(addr+2, (byte) (dest>>8));
	}
/*...e*/
/*...scpm_place_edfe:0:*/
static void cpm_place_edfe(word addr)
	{
	mem_write_byte(addr  , 0xed);
	mem_write_byte(addr+1, 0xfe);
	}
/*...e*/
/*...scpm_place_edfec9:0:*/
static void cpm_place_edfec9(word addr)
	{
	mem_write_byte(addr  , 0xed);
	mem_write_byte(addr+1, 0xfe);
	mem_write_byte(addr+2, 0xc9);
	}
/*...e*/

/*...scpm_init_dummy_dpb:0:*/
/* See http://www.seasip.demon.co.uk/Cpm/format22.html for the DPB format.
   See ADISC.MAC or AFDSC.MAC from boot ROM source for Memotech values.
   The values here come from type 07 disks. */

static void cpm_init_dummy_dpb(word addr)
	{
	mem_write_byte(addr   , (byte) ( 26&0xff)); /* 128 byte records per track (low) */
	mem_write_byte(addr+ 1, (byte) ( 26>>8  )); /* 128 byte records per track (high) */
	mem_write_byte(addr+ 2, (byte) (       4)); /* block shift */
	mem_write_byte(addr+ 3, (byte) (      15)); /* block mask */
	mem_write_byte(addr+ 4, (byte) (       0)); /* extent mask */
	mem_write_byte(addr+ 5, (byte) (314&0xff)); /* blocks on the disk - 1 (low) */
	mem_write_byte(addr+ 6, (byte) (314>>8  )); /* blocks on the disk - 1 (high) */
	mem_write_byte(addr+ 7, (byte) (127&0xff)); /* directory entries - 1 (low) */
	mem_write_byte(addr+ 8, (byte) (127>>8  )); /* directory entries - 1 (high) */
	mem_write_byte(addr+ 9, (byte) (    0xc0)); /* directory allocation bitmap 1 */
	mem_write_byte(addr+10, (byte) (    0x00)); /* directory allocation bitmap 2 */
	mem_write_byte(addr+11, (byte) ( 32&0xff)); /* checksum vector size (low) */
	mem_write_byte(addr+12, (byte) ( 32>>8  )); /* checksum vector size (high) */
	mem_write_byte(addr+13, (byte) (  2&0xff)); /* number of reserved tracks (low) */
	mem_write_byte(addr+14, (byte) (  2>>8  )); /* number of reserved tracks (high) */
	}
/*...e*/

/*...scpm_init:0:*/
void cpm_init(void)
	{
	if ( cpm_inited )
		/* Tolerate -cpm twice on the command line */
		return;

	mem_set_iobyte(0x80);

	/* Catch code that jumps into high memory,
	   just before BDOS */
	cpm_place_edfe(0xfdfe);

	/* Prepare BDOS.
	   CP/M programs CALL 5, which is assumed to be JP xxxx
	   and they assume everything below xxxx is available memory. */
	cpm_place_edfec9(0xfe00); /* BDOS */
	cpm_place_jp(0x0005, 0xfe00); /* jump to BDOS */
	mem_write_byte(0x0003, IOBYTE); /* CP/M IO byte */

	/* Catch code that jumps into high memory,
	   just before CBIOS vector area */
	cpm_place_edfe(0xfefe);

	/* Prepare CBIOS jump table, at 0xff00.
	   CBIOS jump table must be on 0x100 byte boundary.
	   I've seen Newword use the Console output vector. */
	cpm_place_edfec9(0xff00); /* Cold start */
	cpm_place_edfec9(0xff03); /* Warm boot */
	cpm_place_edfec9(0xff06); /* Console status */
	cpm_place_edfec9(0xff09); /* Console input */
	cpm_place_edfec9(0xff0c); /* Console output */
	cpm_place_edfec9(0xff0f); /* Printer output */
	cpm_place_edfec9(0xff12); /* Punch output */
	cpm_place_edfec9(0xff15); /* Reader input */
	cpm_place_edfec9(0xff18); /* Move disk head to track 0 */
	cpm_place_edfec9(0xff1b); /* Select disc drive */
	cpm_place_edfec9(0xff1e); /* Set track number */
	cpm_place_edfec9(0xff21); /* Set sector number */
	cpm_place_edfec9(0xff24); /* Set DMA address */
	cpm_place_edfec9(0xff27); /* Read a sector */
	cpm_place_edfec9(0xff2a); /* Write a sector */
	cpm_place_edfec9(0xff2d); /* Status of list device */
	cpm_place_edfec9(0xff30); /* Sector translation for skewing */
	cpm_place_jp(0x0000, 0xff03); /* jump to warm boot vector */

	/* Put a dummy "SIGNON MESSAGE" in CBIOS area like CBIOS.ASM does.
	   I think NewWord relies on finding this... */
	mem_write_block(0xff40, 38, (const byte *) "\x18\x17MTX 63K Memotech Bios: 23-Aug-11\r\n\x16\x00");

	mem_write_block(0xffc0, 6, (const byte *) "@SDX01");

	cpm_init_dummy_dpb(CPM_DPB_ADDR);

	/* Catch code that jumps into high memory,
	   just before driver vector area */
	cpm_place_edfe(0xffce);

	/* Prepare certain FDX/SDX specific high memory vectors.
	   I've seen Newword use the KBD ready vector. */
	cpm_place_edfec9(0xffd0); /* KBD input */
	cpm_place_edfec9(0xffd3); /* VDU output */
	cpm_place_edfec9(0xffd6); /* KBD ready */

	/* Catch code that jumps into high memory,
	   right at the top of memory */
	cpm_place_edfe(0xfffa);

	/* Prepare the initialise vector */
	cpm_place_edfec9(0xfffc); /* Initialise */

	/* Initially no command tail */
	mem_write_byte(0x0080, 0x00);

	/* Initially FCBs not parsed */
	mem_write_byte(FCB_ADDR1+FCB_DR, 0);
	mem_write_block(FCB_ADDR1+FCB_Fn, FCB_Fn_LEN, (byte *) "        ");
	mem_write_block(FCB_ADDR1+FCB_Tn, FCB_Tn_LEN, (byte *) "   ");
	mem_write_byte(FCB_ADDR2+FCB_DR, 0);
	mem_write_block(FCB_ADDR2+FCB_Fn, FCB_Fn_LEN, (byte *) "        ");
	mem_write_block(FCB_ADDR2+FCB_Tn, FCB_Tn_LEN, (byte *) "   ");

	if ( drive_a == NULL )
		drive_a = estrdup(".");

	cpm_inited = TRUE;
	}
/*...e*/
/*...scpm_init_sdx:0:*/
void cpm_init_sdx(void)
	{
	if ( cpm_inited_sdx )
		/* Tolerate -sdx twice on the command line */
		return;

	if ( drive_a == NULL )
		drive_a = estrdup(".");

	dma_addr = 0xe680;

	cpm_init_dummy_dpb(CPM_DPB_ADDR_SDX);

	cpm_inited_sdx = TRUE;
	}
/*...e*/
/*...scpm_init_fdxb:0:*/
void cpm_init_fdxb(void)
	{
	byte b;

	if ( !cpm_inited )
		fatal("FDXB CP/M support is an extension of CP/M support");

	if ( cpm_inited_fdxb )
		/* Tolerate -fdxb twice on the command line */
		return;

	if ( drive_a == NULL )
		drive_a = estrdup(".");

	dma_addr = 0x7080;

	cpm_inited_fdxb = TRUE;

	b = mem_get_iobyte();
	mem_set_iobyte(0x80);
	if ( mem_read_byte(0x0103+0x6308) == 0xeb &&
	     mem_read_byte(0x0103+0x6309) == 0x22 &&
	     mem_read_byte(0x0103+0x630a) == 0x43 &&
	     mem_read_byte(0x0103+0x630b) == 0x64 )
		;
	else
		fatal("FDXB CP/M entrypoint not found");

	cpm_place_edfec9(0x0103+0x6308); /* BDOS */

	/* FDXB copies and patches keyboard tables, SDXB doesn't.
	   If we find this, skip it, as we don't emulate keyboard tables. */
	if ( mem_read_byte(0x813d) == 0x3a &&
	     mem_read_byte(0x813e) == 0xf2 &&
	     mem_read_byte(0x813f) == 0xff )
		{
		mem_write_byte(0x813d, 0xc3);
		mem_write_byte(0x813e, 0x7e);
		mem_write_byte(0x813f, 0x81);
		}

	mem_set_iobyte(b);
	}
/*...e*/

/*...scpm_set_drive_a:0:*/
void cpm_set_drive_a(const char *cpm_drive_a)
	{
	if ( drive_a != NULL )
		free(drive_a);
	if ( strlen(cpm_drive_a) > DRIVE_PREFIX_LEN )
		fatal("CP/M drive A: path is too long");
	drive_a = estrdup(cpm_drive_a);
	}
/*...e*/
/*...scpm_get_drive_a:0:*/
const char *cpm_get_drive_a(void)
	{
	return drive_a;
	}
/*...e*/
/*...scpm_set_invert_case:0:*/
void cpm_set_invert_case(void)
	{
	invert_case = TRUE;
	}	
/*...e*/
/*...scpm_set_tail:0:*/
void cpm_set_tail(const char *tail)
	{
	int n = (int) strlen(tail);

	if ( n > 126 )
		fatal("CP/M command tail too long");

	mem_set_iobyte(0x80);

	if ( n == 0 )
		mem_write_byte(0x0080, 0);
	else
		{
		byte drive;
		char fn[FCB_Fn_LEN];
		char tn[FCB_Tn_LEN];
		char *tail2 = estrdup(tail);
		char *p;
		mem_write_byte(0x0080, (byte) n+1);
		mem_write_byte(0x0081, ' ');
		mem_write_block(0x0082, (word) n, (byte *) tail);
		str_upper(tail2);
		if ( (p = strtok(tail2, " ")) != NULL &&
		     valid_cpm_drive_and_filename(p, &drive, fn, tn, TRUE) )
			{
			mem_write_byte(FCB_ADDR1+FCB_DR, drive);
			mem_write_block(FCB_ADDR1+FCB_Fn, FCB_Fn_LEN, (byte *) fn);
			mem_write_block(FCB_ADDR1+FCB_Tn, FCB_Tn_LEN, (byte *) tn);
			if ( (p = strtok(NULL, " ")) != NULL &&
			     valid_cpm_drive_and_filename(p, &drive, fn, tn, TRUE) )
				{
				mem_write_byte(FCB_ADDR2+FCB_DR, drive);
				mem_write_block(FCB_ADDR2+FCB_Fn, FCB_Fn_LEN, (byte *) fn);
				mem_write_block(FCB_ADDR2+FCB_Tn, FCB_Tn_LEN, (byte *) tn);
				}
			}
		free(tail2);
		}
	}	
/*...e*/
/*...scpm_allow_open_hack:0:*/
void cpm_allow_open_hack(BOOLEAN bAllow)
	{
	cpm_open_hack = bAllow;
	}
/*...e*/
