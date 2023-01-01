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

static EightyColumn ec_con;

static bool bInit = false;
static int iRow = 0;
static int iCol = 0;
static int iAddr = 0;

void printf_pico (const char *psMsg)
    {
    PRINTF ("%s", psMsg);
    if ( ! bInit )
        {
        memset (&ec_con, 0, sizeof (ec_con));
        ec_con.reg[10] = 0x20;                     // Hide cursor
        bInit = true;
        }
    int iCsr = iAddr + EC_COLS * iRow + iCol;
    while ( *psMsg )
        {
        if ( iRow >= EC_ROWS )
            {
            iCsr = iAddr + EC_ROWS * EC_COLS;
            for (int i = 0; i < EC_COLS; ++i)
                {
                iCsr &= ( NRAM80C - 1 );
                ec_con.ram[iCsr].ch = ' ';
                ++iCsr;
                }
            iAddr = ( iAddr + EC_COLS ) & ( NRAM80C - 1 );
            ec_con.reg[12] = iAddr >> 8;
            ec_con.reg[13] = iAddr & 0xFF;
            iRow = EC_ROWS - 1;
            iCsr = iAddr + EC_COLS * iRow + iCol;
            }
        iCsr &= ( NRAM80C - 1 );
        char ch = *psMsg;
        if ( ch == '\r' )
            {
            iCol = 0;
            iCsr = iAddr + EC_COLS * iRow;
            }
        else if ( ch == '\n' )
            {
            ++iRow;
            iCol = 0;
            iCsr = iAddr + EC_COLS * iRow;
            }
        else
            {
            ec_con.ram[iCsr].ch = ch;
            ec_con.ram[iCsr].at = 0x07;
            ++iCsr;
            if ( ++iCol >= EC_COLS )
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
    display_80column (&ec_con);
    while (true)
        {
        sleep_ms (1000);
        }
    }
