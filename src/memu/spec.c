/*

spec.c - Speculator

Real hardware address decodes
  x11xxxxx, to cover 7E,7F,FB,FE,FF and maybe more?
  xxx11111, to cover 1F, and maybe more?
we just handle the main intended addresses.

This is a problem, as the Speculator code at F800 tests these port ranges
      1F, 60-6D, 7F,
  9F, BF, E0-FD
We therefore require the use of a patched Speculator program that doesn't
test as much, much like the Tatung Einstein version didn't.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "types.h"
#include "diag.h"
#include "common.h"
#include "spec.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vspec\46\h:0:*/
/*...e*/

/*...svars:0:*/
static byte spec_kempston = 0xff;
static byte spec_fuller = 0x00;
static byte spec_printer = 0x40;
static byte spec_border = 0xff;
static byte spec_rows[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

static byte spec_nmi = 0x00;
/*...e*/

/*...sspec_out1F:0:*/
void spec_out1F(byte value)
	{
	diag_message(DIAG_SPEC_PORTS, "store into emulated kempston joystick, value=0x%02x", value);
	spec_kempston = value;
	}
/*...e*/
/*...sspec_in1F:0:*/
byte spec_in1F(void)
	{
	diag_message(DIAG_SPEC_PORTS, "fetch from emulated kempston joystick, value=0x%02x", spec_kempston);
	return spec_kempston;
	}
/*...e*/

/*...sspec_out7F:0:*/
void spec_out7F(byte value)
	{
	diag_message(DIAG_SPEC_PORTS, "store into emulated fuller joystick, value=0x%02x", value);
	spec_fuller = value;
	}
/*...e*/
/*...sspec_in7F:0:*/
byte spec_in7F(void)
	{
	diag_message(DIAG_SPEC_PORTS, "fetch from emulated fuller joystick, value=0x%02x", spec_fuller);
	return spec_fuller;
	}
/*...e*/

/*...sspec_outFB:0:*/
void spec_outFB(byte value)
	{
	diag_message(DIAG_SPEC_PORTS, "store into emulated printer, value=0x%02x", value);
	spec_printer = value;
	}
/*...e*/
/*...sspec_inFB:0:*/
byte spec_inFB(void)
	{
	diag_message(DIAG_SPEC_PORTS, "fetch from emulated printer, value=0x%02x", spec_printer);
	return spec_printer;
	}
/*...e*/

/*...sspec_outFE:0:*/
void spec_outFE(byte value)
	{
	diag_message(DIAG_SPEC_PORTS, "store into emulated border, value=0x%02x", value);
	spec_border = value;
	}
/*...e*/
/*...sspec_in7E:0:*/
byte spec_in7E(void)
	{
	diag_message(DIAG_SPEC_PORTS, "fetch from emulated border, value=0x%02x", spec_border);
	return spec_border;
	}
/*...e*/

/*...sspec_out7E:0:*/
/* Only one bit of rowsel will be low.
   Speculator hardware can't cope with multiple bits low. */
void spec_out7E(byte rowsel, byte value)
	{
	int i;
	diag_message(DIAG_SPEC_PORTS, "store into emulated keyboard, rowsel=0x%02x value=0x%02x", rowsel, value);
	for ( i = 0; i < 8; i++ )
		if ( ( rowsel&(0x80>>i) ) == 0 )
			{
			spec_rows[i] = value;
			break;
			}
	}
/*...e*/
/*...sspec_inFE:0:*/
/* Multiple bits of rowsel could be low.
   Speculator hardware can't cope with multiple bits low.
   But we can, and we do.
   So code like this
     XOR A         ; A is used as A15-A8 of I/O address in the following
     IN A,(0FEH)
   which looks for any key pressed will work properly here,
   but on a real Speculator, would only check the first row. */
byte spec_inFE(byte rowsel)
	{
	int i;
	byte value = 0xff;
	for ( i = 0; i < 8; i++ )
		if ( ( rowsel&(0x80>>i) ) == 0 )
			{
			value &= spec_rows[i];
/* if we were being pure, we'd
			break;
   here */
			}
	diag_message(DIAG_SPEC_PORTS, "fetch from emulated keyboard, rowsel=0x%02x value=0x%02x", rowsel, value);
	return value;
	}
/*...e*/

/*...sspec_outFF:0:*/
void spec_outFF(byte value)
	{
	diag_message(DIAG_SPEC_PORTS, "store to NMI write register 0x%02x", value);
	spec_nmi = value;
	}
/*...e*/
/*...sspec_getNMI:0:*/
byte spec_getNMI(void)
	{
	return spec_nmi;
	}
/*...e*/

/*...sspec_init:0:*/
void spec_init(void)
	{
	}
/*...e*/
/*...sspec_term:0:*/
void spec_term(void)
	{
	}
/*...e*/
