/* misc.c - Implementation of some routines missing from Circle */

#include <circle/timer.h>
#include <circle/alloc.h>
#include <circle/util.h>
#include <circle/startup.h>
#include <fatfs/ff.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include "console.h"

void *memmove (void *pvDst, void *pvSrc, size_t nByte)
    {
    unsigned char *pbSrc = (unsigned char *) pvSrc;
    unsigned char *pbDst = (unsigned char *) pvDst;
    if ( ( pbDst < pbSrc ) || ( pbDst > pbSrc + nByte ) )
        {
        memcpy (pvDst, pvSrc, nByte);
        }
    else
        {
        pbSrc += nByte;
        pbDst += nByte;
        while ( nByte > 0 )
            {
            *(--pbDst) = *(--pbSrc);
            --nByte;
            }
        }
    return pvDst;
    }

const char *strrchr (const char *psStr, int ch)
    {
    const char *ps = NULL;
    while ( *psStr )
        {
        if ( *psStr == ch ) ps = psStr;
        ++psStr;
        }
    return ps;
    }

char *strtok (char *str, const char *delim)
    {
    static char *psSave;
    return strtok_r (str, delim, &psSave);
    }

/*
char *strtok (char *str, const char *delim)
    {
    static char *psText = NULL;
    static char *psNext = NULL;
    char *psTok;
    if ( str != NULL )
        {
        if ( psText != NULL ) free (psText);
        psText = (char *) malloc (strlen (str) + 1);
        if ( psText == NULL ) return NULL;
        strcpy (psText, str);
        psNext = psText;
        }
    while ( ( *psNext ) && ( strchr (delim, *psNext) ) ) ++psNext;
    if ( ! ( *psNext ) ) return NULL;
    psTok = psNext;
    while ( ( *psNext ) && ( ! strchr (delim, *psNext) ) ) ++psNext;
    if ( *psNext )
        {
        *psNext = '\0';
        ++psNext;
        }
    return psTok;
    }
*/

void exit (int status)
    {
    con_print ("MEMU exited", 0);
    halt ();
    }

int strncasecmp (const char *ps1, const char *ps2, int n)
    {
    char c1, c2;
    while ( ( n ) && ( c1 = *ps1 ) && ( c2 = *ps2 ) )
        {
        if ( ( c1 >= 'a' ) && ( c1 <= 'z' ) )   c1 -= 0x20;
        if ( ( c2 >= 'a' ) && ( c2 <= 'z' ) )   c2 -= 0x20;
        if ( c1 != c2 ) return c1 - c2;
        ++ps1;
        ++ps2;
        --n;
        }
    return 0;
    }

char * strdup (const char *ps)
    {
    char *psDup = (char *) malloc (strlen (ps) + 1);
    if ( psDup != NULL ) strcpy (psDup, ps);
    return psDup;
    }

int usleep (int iUs)
    {
    CTimer::SimpleusDelay (iUs);
    return 0;
    }
    
int nanosleep (struct timespec *ts_req, struct timespec *ts_rem)
    {
    unsigned long us = 1000000L * ts_req->tv_sec + ts_req->tv_nsec / 1000L;
    CTimer::SimpleusDelay (us);
    if ( ts_rem != NULL )
        {
        ts_rem->tv_sec = 0;
        ts_rem->tv_nsec = 0;
        }
    return 0;
    }

int clock_gettime (int clock, struct timespec *ts)
    {
    static unsigned int usSec = 0;
    static unsigned int sec = 0;
    unsigned int usNow = CTimer::GetClockTicks ();
    unsigned int step = ( usNow - usSec ) / 1000000L;
    sec += step;
    usSec += 1000000L * step;
    ts->tv_sec = sec;
    ts->tv_nsec = 1000L * ( usNow - usSec );
    return 0;
    }

int stat (const char *psPath, struct stat *st)
    {
	FILINFO buf;
	if ( f_stat (psPath, &buf) != FR_OK ) return -1;
	if ( st )
        {
        st->st_mode = (buf.fattrib & AM_DIR) ? S_IFDIR : S_IFREG;
        st->st_size = buf.fsize;
        }
    return 0;
    }
