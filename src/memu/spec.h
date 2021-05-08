/*

spec.h - Speculator

*/

#ifndef SPEC_H
#define SPEC_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	NMI_ENABLED   0x01
#define	NMI_IMMEDIATE 0x02

extern void spec_out1F(byte value);
extern byte spec_in1F(void);

extern void spec_out7F(byte value);
extern byte spec_in7F(void);

extern void spec_outFB(byte value);
extern byte spec_inFB(void);

extern void spec_outFE(byte value);
extern byte spec_in7E(void);

extern void spec_out7E(byte rowsel, byte value);
extern byte spec_inFE(byte rowsel);

extern void spec_outFF(byte value);

extern byte spec_getNMI(void);

extern void spec_init(void);
extern void spec_term(void);

#endif
