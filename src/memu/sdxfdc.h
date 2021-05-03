/*

sdxfdc.h -  Partial emulation of the SDX floppy disc controller.

WJB   17/ 7/12 First draft.
AK    01/04/13 Fix DRVS_80TRACK #define.

*/

#ifndef SDXFDX_H
#define SDXFDC_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

/* Switch values (first 4 bits) are inverted */

#define	DRVS_NO_HEAD_LOAD 0x01
#define	DRVS_HEAD_LOAD    0x00
#define	DRVS_SINGLE_SIDED 0x02
#define	DRVS_DOUBLE_SIDED 0x00
#define	DRVS_40TRACK      0x04
#define	DRVS_80TRACK      0x00
#define	DRVS_1_DRIVE      0x08
#define	DRVS_2_DRIVES     0x00
#define	DRVS_LINK         0x10

extern void sdxfdc_out(word port, byte value);
extern byte sdxfdc_in(word port);

extern void sdxfdc_init(int drive, const char *psFile);
extern void sdxfdc_term(void);
extern void sdxfdc_drvcfg(int drive, byte cfg);

#endif
