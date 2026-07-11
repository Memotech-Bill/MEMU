/*  fpu.h - Emulation of the Arithmetic accelerator in v4 of the MFX FPGA */

#ifndef FPU_H
#define FPU_H

#include "types.h"

#define R_BUSY      0
#define R_OK        1
#define R_DIV0      2
#define R_OVER      3
#define R_UNDR      4

void fpu_out (word port, byte value);
byte fpu_in (word port);

#endif
