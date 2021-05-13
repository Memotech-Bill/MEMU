/* hardware.h  -  Defines RPi hardware configuration for MTX emulation */

#ifndef	 H_HARDWARE
#define	 H_HARDWARE

#include "TxtRead.h"
#include "gpio.h"
#include "win.h"

void hw_pindef (TXR *ptxr, struct gio_pin *ppin);
void hw_read (const char *psFile);
void hw_save_cfg (FILE *pfil);
void hw_init (void);
void hw_term (void);
word hw_kbd_sense_1 (word wDrive);
word hw_kbd_sense_2 (word wDrive);
BOOLEAN hw_Z80_out (word port, byte value);
BOOLEAN hw_Z80_in (word port, byte *value);
int hw_key (void);
BOOLEAN hw_handle_events (WIN *win);

#endif
