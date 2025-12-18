/* cfx.c - Emulation of the CFX interface */

#ifndef CFX_H
#define CFX_H

#include "types.h"

void cfx_out (word port, byte value);
byte cfx_in (word port);

#endif
