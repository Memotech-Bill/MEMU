/* pico_display.h - Display control */

#ifndef PICO_DISPLAY_H
#define PICO_DISPLAY_H

#define DBLPIX      // Use 640x240 timing for VDP mode with each pixel duplicated

#include <stdbool.h>
#include "win.h"

typedef enum {dispNone, dispVDP, disp80col} DisplayMode;

extern volatile DisplayMode dmode;
extern volatile TXTBUF *p80column;
extern volatile bool bFrameInt;

void vdp_video (void);
void twin_video (void);
void display_loop (void);
void display_vdp (void);
void display_tbuf (TXTBUF *tbuf);
void display_wait_for_frame (void);

#endif
