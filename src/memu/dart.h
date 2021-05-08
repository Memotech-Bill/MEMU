/*

dart.h - Serial I/O emulator

Contributed by Bill Brendling.

*/

#ifndef	DART_H
#define	DART_H

#include "types.h"

extern void dart_out (word port, byte value);
extern byte dart_in (word port);
extern int  dart_int_pending (int cycles);
extern BOOLEAN dart_reti (void);
extern void dart_read (int ch, const char *psFile);
extern void dart_write (int ch, const char *psFile);
extern void dart_serial (int ch, const char *psDev);
extern void dart_init (void);
extern void dart_term (void);

#endif
