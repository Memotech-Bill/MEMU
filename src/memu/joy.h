/*

joy.h - Joystick

*/

#ifndef JOY_H
#define	JOY_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	JOYEMU_JOY   0x01

extern void joy_set_buttons(const char *buttons);
extern void joy_set_central(int central);

extern void joy_periodic(void);

extern void joy_init(int emu);
extern void joy_term(void);

#endif
