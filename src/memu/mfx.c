/*  mfx.c - Minimal emulation of some of the MFX features */

#include "types.h"
#include "mfx.h"

static int iSer = 0;
static const char sSer[] = "ME\01";
static byte byFPGA = 0;

void mfx_out35 (byte value)
    {
    iSer = value & 0x03;
    }

byte mfx_in35 (void)
    {
    byte by = sSer[iSer];
    ++iSer;
    iSer &= 0x03;
    return by;
    }

void mfx_out3A (byte value)
    {
    byFPGA = value;
    }

byte mfx_in3A (void)
    {
    return byFPGA;
    }

byte mfx_inD8 (void)
    {
    return 5;
    }
