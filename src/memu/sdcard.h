/*  sdcard.c - Emulation of the MFX SD Card Interface

    A very minimal emulation of a real SD Card,
    just enough to satisfy the MFX ROM (and hextrain).

*/

#ifndef SDCARD_H
#define SDCARD_H

#if defined (HAVE_MFX) && ! defined (HAVE_SD_CARD)
#define HAVE_SD_CARD
#endif

#ifdef HAVE_SD_CARD
#include "types.h"

int sdcard_set_type (const char *psType);
void sdcard_out (byte port, byte value);
byte sdcard_in (byte port);
void sdcard_set_image (int iImage, const char *psFile);
#endif

#endif
