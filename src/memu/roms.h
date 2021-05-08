/*

roms.h - MTX ROMs

*/

#ifndef ROMS_H
#define	ROMS_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	ROM_SIZE 0x2000

//#ifdef SMALL_MEM
extern const byte rom_os   [ROM_SIZE];
extern const byte rom_basic[ROM_SIZE];
extern const byte rom_assem[ROM_SIZE];
extern const byte rom_cpm  [ROM_SIZE];
extern const byte rom_sdx  [ROM_SIZE];
//#else
#if 0
extern byte rom_os   [ROM_SIZE];
extern byte rom_basic[ROM_SIZE];
extern byte rom_assem[ROM_SIZE];
extern byte rom_cpm  [ROM_SIZE];
extern byte rom_sdx  [ROM_SIZE];
#endif

#endif
