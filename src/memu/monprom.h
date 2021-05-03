
/*

monprom.h - Character generator PROM

*/

#ifndef MONPROM_H
#define	MONPROM_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	GLYPH_WIDTH   8
#define	GLYPH_HEIGHT 10

extern byte mon_alpha_prom[0x100][GLYPH_HEIGHT];
extern byte mon_graphic_prom[0x100][GLYPH_HEIGHT];
extern byte mon_blank_prom[GLYPH_HEIGHT];

extern void mon_init_prom(void);

#endif
