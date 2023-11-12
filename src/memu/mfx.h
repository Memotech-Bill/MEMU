/*  mfx.h - Minimal emulation of some of the MFX features */

#ifndef MFX_H
#define MFX_H

#ifdef HAVE_MFX

#define MFXEMU_MAX  0x8000
void mfx_init (int mfx_emu);
byte mfx_in (word port);
void mfx_out (word port, byte value);
void mfx_refresh (void);
void mfx_blink (void);
void mfx_show (void);
#endif

#endif
