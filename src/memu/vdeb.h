/*  vdeb.h  - Interface to visual debugger */

#ifndef H_VDEB
#define H_VDEB

#include "Z80.h"

extern void vdeb_break (void);      // Call in response to activation key press (e.g. from diag_control)
extern void vdeb (Z80 *R);          // Call from DebugZ80(Z80 *R);
extern void vdeb_term (void);       // Call from terminate();
extern void vdeb_mwrite (byte iob, word addr);  // Called from WrZ80 if write checking requested
extern void vdeb_iobyte (byte iob);  // Called from mem_set_iobyte when iobyte is changed

#endif
