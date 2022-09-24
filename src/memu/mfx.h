/*  mfx.h - Minimal emulation of some of the MFX features */

#ifndef MFX_H
#define MFX_H

#ifdef HAVE_MFX
void mfx_out35 (byte value);
byte mfx_in35 (void);
void mfx_out3A (byte value);
byte mfx_in3A (void);
byte mfx_inD8 (void);
#endif

#endif
