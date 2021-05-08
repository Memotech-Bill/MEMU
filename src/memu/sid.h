/*

sid.h - Silicon Disc

*/

#ifndef SID_H
#define SID_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	N_SIDISC 4

#define	SIDEMU_HUGE    0x01
#define	SIDEMU_NO_SAVE 0x02

extern void sid_out(word port, byte value);
extern byte sid_in(word port);

/* Call this prior to sid_init only */
extern void sid_set_file(int drive, const char *fn);
extern const char *sid_get_file (int drive);

extern void sid_load (int drive);
extern void sid_save (int drive, BOOLEAN bFree);
extern void sid_mode (int emu);
extern void sid_init(int emu);
extern void sid_term(void);

#endif
