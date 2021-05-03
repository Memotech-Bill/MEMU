/* pico_80col.h - 80 Column display */

#ifndef PICO_80COL_H
#define PICO_80COL_H

#include <stdint.h>

#define EC_ROWS 24
#define EC_COLS 80
#define NRAM80C 2048
#define NREG80C 16

typedef struct st_80col
    {
    struct st_monram
        {
        uint8_t ch;
        uint8_t at;
        } ram[NRAM80C];

    uint8_t reg[NREG80C];
    uint32_t mode;
    } EightyColumn;

void ecol_video (void);

#endif
