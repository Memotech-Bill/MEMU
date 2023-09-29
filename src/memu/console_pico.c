/* Console_pico.c - Provides a display of console messages for Pico */

#include "pico.h"
#include "pico/stdlib.h"
#include "display_pico.h"
#include <string.h>
#include <stdio.h>

#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

static TXTBUF con_tbuf;

static bool bInit = false;
static int iRow = 0;
static int iCol = 0;
static int iAddr = 0;

void printf_pico (const char *psMsg)
    {
    PRINTF ("%s", psMsg);
    if ( ! bInit )
        {
        memset (&con_tbuf, 0, sizeof (con_tbuf));
        con_tbuf.reg[10] = 0x20;                     // Hide cursor
        bInit = true;
        }
    int iCsr = iAddr + TW_COLS * iRow + iCol;
    while ( *psMsg )
        {
        if ( iRow >= TW_ROWS )
            {
            iCsr = iAddr + TW_ROWS * TW_COLS;
            for (int i = 0; i < TW_COLS; ++i)
                {
                iCsr &= ( NRAM80C - 1 );
                con_tbuf.ram[iCsr].ch = ' ';
                ++iCsr;
                }
            iAddr = ( iAddr + TW_COLS ) & ( NRAM80C - 1 );
            con_tbuf.reg[12] = iAddr >> 8;
            con_tbuf.reg[13] = iAddr & 0xFF;
            iRow = TW_ROWS - 1;
            iCsr = iAddr + TW_COLS * iRow + iCol;
            }
        iCsr &= ( NRAM80C - 1 );
        char ch = *psMsg;
        if ( ch == '\r' )
            {
            iCol = 0;
            iCsr = iAddr + TW_COLS * iRow;
            }
        else if ( ch == '\n' )
            {
            ++iRow;
            iCol = 0;
            iCsr = iAddr + TW_COLS * iRow;
            }
        else
            {
            con_tbuf.ram[iCsr].ch = ch;
            con_tbuf.ram[iCsr].at = 0x07;
            ++iCsr;
            if ( ++iCol >= TW_COLS )
                {
                ++iRow;
                iCol = 0;
                }
            }
        ++psMsg;
        }
    }

void exit_pico (int iExit)
    {
    display_tbuf (&con_tbuf);
    while (true)
        {
        sleep_ms (1000);
        }
    }
