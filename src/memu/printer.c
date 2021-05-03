/*

printer.c - Hardware level printer emulation

Based on a patch from William Brendling.
Style amended per rest of MEMU source.
CP/M related stuff moved to CP/M emulation.

*/

/*...sincludes:0:*/
#include "ff_stdio.h"
#include <ctype.h>
#include <string.h>

#include "types.h"
#include "diag.h"
#include "common.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...e*/

/*...svars:0:*/
#define	PRN_BUSY  0x01				/* Busy */
#define	PRN_NERR  0x02				/* Not error */
#define	PRN_PE    0x04				/* Paper empty */
#define	PRN_SLCT  0x08				/* Select */
#define	PRN_READY ( PRN_NERR | PRN_SLCT )	/* Printer ready */

static FILE *fp_print = NULL;
static byte output_byte;
static BOOLEAN strobe = FALSE;
/*...e*/

/*...sprint_byte:0:*/
void print_byte(byte value)
	{
	byte valuediag = isprint(value) ? value : '.';
	if ( fp_print != NULL )
		{
		diag_message(DIAG_PRINT, "Printing: %c", valuediag);
		if ( fputc(value, fp_print) == EOF )
			fatal("printer output file full");
		}
	else
		diag_message(DIAG_PRINT, "No printer destination for: %c", valuediag);
	}
/*...e*/
/*...sprint_ready:0:*/
BOOLEAN print_ready(void)
	{
	return fp_print != NULL;
	}
/*...e*/

/*...sprint_out4:0:*/
/* Specify the byte to print */
#ifdef	 ALT_OUT4
extern void ALT_OUT4 (byte value);
#endif

void print_out4(byte value)
	{
#ifdef	 ALT_OUT4
	ALT_OUT4 (value);
#endif
	output_byte = value;
	}
/*...e*/
/*...sprint_in0:0:*/
/* Raise strobe line */
#ifdef	 ALT_IN0
extern BOOLEAN ALT_IN0 (byte *);
#endif

byte print_in0(void)
	{
	byte byStatus =	 0xFF;
#ifdef	 ALT_IN0
	if ( ALT_IN0 (&byStatus) )	 return byStatus;
#endif
	strobe = TRUE;
	return byStatus;
	}
/*...e*/
/*...sprint_in4:0:*/
/* Lower strobe to cause byte to be printed.
   Also, return printer status. */
#ifdef	 ALT_IN4
extern BOOLEAN ALT_IN4 (byte *);
#endif

byte print_in4(void)
	{
#ifdef	 ALT_IN4
	byte byStatus =	 0xFF;
	if ( ALT_IN4 (&byStatus) )	 return byStatus;
#endif
	if ( strobe )
		{
		print_byte(output_byte);
		strobe = FALSE;
		}
	return fp_print != NULL
		? PRN_READY		/* No error and selected */
		: PRN_PE;		/* Paper error */
	}
/*...e*/

/*...sprint_init:0:*/
void print_init(const char *fn)
	{
	diag_message(DIAG_PRINT, "Printer output will be sent to %s", fn);
	if ( (fp_print = fopen(fn, "wb")) == NULL )
		fatal("error opening print file/device: %s", fn);
	}
/*...e*/
/*...sprint_term:0:*/
void print_term(void)
	{
	if ( fp_print != NULL )
		fclose(fp_print);
	}
/*...e*/
