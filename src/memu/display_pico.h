/* pico_display.h - Display control */

#ifndef PICO_DISPLAY_H
#define PICO_DISPLAY_H

#define DBLPIX      // Use 640x240 timing for VDP mode with each pixel duplicated

#include <stdbool.h>
#include "80col_pico.h"

typedef enum {dispNone, dispVDP, disp80col} DisplayMode;

extern volatile DisplayMode dmode;
extern volatile EightyColumn *p80column;
extern volatile bool bFrameInt;

typedef struct st_dspstate
    {
    DisplayMode     dm;
    EightyColumn    *pec;
    } DisplayState;

void display_loop (void);
void display_vdp (void);
void display_80column (EightyColumn *p80c);
void display_save (DisplayState *pst);
void display_load (const DisplayState *pst);
void display_wait_for_frame (void);

#endif
