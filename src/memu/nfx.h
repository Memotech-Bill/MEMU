/* nfx.h - Emulation of the NFX Wiznet Interface */

#ifndef NFX_H
#define NFX_H

#include "types.h"

extern void nfx_port_offset (int offset);
extern void nfx_out (byte port, byte value);
extern byte nfx_in (byte port);

#endif
