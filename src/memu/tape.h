/*	tape.h	-	Cassette tape hardware emulation */

#ifndef	H_TAPE
#define	H_TAPE

#include "types.h"

extern void tape_set_input (const char *psFile);
extern const char *tape_get_input (void);
extern void tape_set_output (const char *psFile);
extern const char *tape_get_output (void);
extern void tape_out3 (byte value);
extern void tape_out1F (byte value);
extern void tape_play (void);
extern void tape_advance (int clks);
extern void tape_term (void);

#endif
