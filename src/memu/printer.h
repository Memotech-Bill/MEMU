/*

printer.h - Hardware level printer emulation

Based on a patch from William Brendling.
Style amended per rest of MEMU source.
CP/M related stuff moved to CP/M emulation.

*/

#ifndef PRINTER_H
#define	PRINTER_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

/* Used by CP/M support */
extern void print_byte(byte value);
extern BOOLEAN print_ready(void);

/* Direct port level access */
extern void print_out4(byte value);
extern byte print_in0(void);
extern byte print_in4(void);

extern void print_init(const char *fn);
extern void print_term(void);

#endif
