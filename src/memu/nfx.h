/* nfx.h - Emulation of the NFX Wiznet Interface */

#ifndef NFX_H
#define NFX_H

#ifdef HAVE_NFX
#ifndef NFX_BASE
#define	NFX_BASE    0x90	// Base I/O address for Wiznet
#endif

#include "types.h"

extern void nfx_port_offset (int offset);
extern void nfx_out (byte port, byte value);
extern byte nfx_in (byte port);
#endif

#endif
