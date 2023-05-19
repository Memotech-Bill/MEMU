/*

  common.c - Handy utilities

*/

/*...sincludes:0:*/
#include "ff_stdio.h"
#include <stdarg.h>
#include <string.h>
#include <time.h>
#ifdef MACOSX
#include <sys/time.h>
#endif

#ifdef WIN32
#define BOOLEAN BOOLEANx
#include <windows.h>
#undef BOOLEAN
#elif defined(UNIX)
#include <unistd.h>
#endif

#include "diag.h"
#include "common.h"
#include "mem.h"
#include "vid.h"
#include "kbd.h"
#ifdef HAVE_JOY
#include "joy.h"
#endif
#ifdef HAVE_DART
#include "dart.h"
#endif
#include "snd.h"
#include "mon.h"
#include "sdxfdc.h"
#include "tape.h"
#include "printer.h"
#ifdef HAVE_CFX2
#include "memu.h"
#include "cfx2.h"
#endif
#ifdef HAVE_VGA
#include "memu.h"
#include "vga.h"
#endif
#ifndef SMALL_MEM
#include "sid.h"
#include "spec.h"
#include "ui.h"
#include "vdeb.h"
#endif
#include "dirmap.h"

/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vmem\46\h:0:*/
/*...vvid\46\h:0:*/
/*...vkbd\46\h:0:*/
/*...vjoy\46\h:0:*/
/*...vdart\46\h:0:*/
/*...vsnd\46\h:0:*/
/*...vmon\46\h:0:*/
/*...vsdxfdc\46\h:0:*/
/*...vsid\46\h:0:*/
/*...vspec\46\h:0:*/
/*...vprinter\46\h:0:*/
/*...vui\46\h:0:*/
/*...e*/

#ifdef BEMEMU
extern void bememu_term();
#endif

#ifdef ALT_EXIT
extern void ALT_EXIT(int reason);
#endif

#ifdef WIN32
static BOOLEAN fine = FALSE;
#endif
/*...sterminate:0:*/
void terminate(const char *reason)
    {
#ifdef WIN32
    if ( fine )
        timeEndPeriod(1);
#endif
#ifndef SMALL_MEM
    diag_message (DIAG_INIT, "vdeb_term");
    vdeb_term();
    diag_message (DIAG_INIT, "ui_term");
    ui_term();
    diag_message (DIAG_INIT, "snd_term");
    snd_term();
    diag_message (DIAG_INIT, "sid_term");
    sid_term();
    diag_message (DIAG_INIT, "spec_term");
    spec_term();
    diag_message (DIAG_INIT, "mem_dump");
    mem_dump();
#endif
#ifdef HAVE_JOY
    diag_message (DIAG_INIT, "joy_term");
    joy_term();
#endif
    diag_message (DIAG_INIT, "print_term");
    print_term();
    diag_message (DIAG_INIT, "tape_term");
    tape_term ();
#ifdef HAVE_DART
    diag_message (DIAG_INIT, "dart_term");
    dart_term();
#endif
    diag_message (DIAG_INIT, "kbd_term");
    kbd_term();
#ifdef HAVE_CFX2
    if ( cfg.bCFX2 )
        {
        diag_message (DIAG_INIT, "cfx2_term");
        cfx2_term ();
        }
    else
#endif
        {
        diag_message (DIAG_INIT, "sdxfdc_term");
        sdxfdc_term();
        }
#ifdef HAVE_VGA
    if ( cfg.bVGA )
        {
        diag_message (DIAG_INIT, "vga_term");
        vga_term ();
        }
#endif
    diag_message (DIAG_INIT, "mon_term");
    mon_term();
    diag_message (DIAG_INIT, "vid_term");
    vid_term();
    win_term();
    // diag_message (DIAG_ALWAYS, "Terminate: %s", reason);
    fprintf (stderr, "Terminate: %s\n", reason);
    diag_message (DIAG_INIT, "diag_term");
    diag_term();
#ifdef BEMEMU
    // diag_message (DIAG_INIT, "bememu_term");
    bememu_term();
#endif
    // diag_message (DIAG_INIT, "exit");
#if defined(ALT_EXIT)
    ALT_EXIT(0);
#else
    exit(0);
#endif
    }
/*...e*/
/*...sfatal:0:*/
void fatal(const char *fmt, ...)
    {
    va_list vars;
    char s[256+8];
    strcpy (s, "Fatal: ");
    va_start(vars, fmt);
    vsprintf(s+7, fmt, vars);
    va_end(vars);
    fprintf(stderr, "memu: %s\n", s+7);
    fflush (stderr);
    diag_flags[DIAG_EXIT] = TRUE;
    terminate (s);
    }
/*...e*/
/*...semalloc:0:*/
void *emalloc(size_t size)
    {
    void *p;
    if ( (p = malloc(size)) == NULL )
        fatal("out of memory");
    return p;
    }
/*...e*/
/*...sestrdup:0:*/
char *estrdup(const char *s)
    {
    char *p;
#if defined(WIN32)
    if ( (p = _strdup(s)) == NULL )
#else
        if ( (p = strdup(s)) == NULL )
#endif
            fatal("out of memory");
    return p;
    }
/*...e*/
/*...sefopen:0:*/
FILE *efopen(const char *fn, const char *mode)
	{
	FILE *fp;
	if ( (fp = fopen(fn, mode)) == NULL )
		fatal("can't open %s", fn);
	return fp;
	}
/*...e*/

char * make_path (const char *psDir, const char *psFile)
    {
    psDir = PMapPath (psDir);
    char *psPath   =  emalloc (strlen (psDir) + strlen (psFile) + 2);
    strcpy (psPath, psDir);
    int n = (int) strlen (psPath);
    if ( n > 0 )
        {
        char ch = psPath[n - 1];
        if (( ch != '/' ) && ( ch != '\\' ))
            {
            strcat (psPath, "/");
            ++n;
            }
        }
    strcpy (&psPath[n], psFile);
    return  psPath;
    }

#ifndef SMALL_MEM
/*...sdelay_millis:0:*/
void delay_millis(long ms)
    {
#if defined(WIN32)
    DWORD dwNow, dwEnd;
    if ( !fine )
        {
        timeBeginPeriod(1);
        fine = TRUE;
        }
    dwNow = timeGetTime();
    dwEnd = dwNow+(DWORD)ms;
    while ( dwNow < dwEnd )
        {
        Sleep(dwEnd-dwNow);
        dwNow = timeGetTime();
        }
#elif defined(UNIX)
    struct timespec ts_req, ts_rem;
    ts_req.tv_sec = 0;
    ts_req.tv_nsec = ms * 1000000UL;
    nanosleep(&ts_req, &ts_rem);
#else
#error No suitable delay_millis function
#endif
    }
/*...e*/
/*...sdelay_micros:0:*/
void delay_micros(long us)
    {
#if defined(WIN32)
    Sleep(us/1000);
#elif defined(UNIX)
    struct timespec ts_req, ts_rem;
    ts_req.tv_sec = 0;
    ts_req.tv_nsec = us * 1000000UL;
    nanosleep(&ts_req, &ts_rem);
#else
#error No suitable delay_micros function
#endif
    }
/*...e*/
/*...sget_millis:0:*/
long long get_millis(void)
    {
#if defined(WIN32)
    /* This will only work for the first 49.7 days */
    return timeGetTime();
/*
  return GetTickCount();
*/
/* Not available on Windows XP
   return GetTickCount64();
*/
/*
  LARGE_INTEGER ticksPerSecond;
  LARGE_INTEGER ticks;
  QueryPerformanceCounterAccuracy(&ticksPerSecond);
  QueryPerformanceCounter(&ticks);
  return ticks.QuadPart / (ticksPerSecond.QuadPart/1000);
*/
#elif defined(MACOSX)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000 + tv.tv_usec / 1000;
#elif defined(UNIX)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long) ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#else
#error No suitable get_millis function
#endif
    }
/*...e*/
/*...sget_micros:0:*/
long long get_micros(void)
    {
#ifdef WIN32
    return get_millis()*1000;
/*
  LARGE_INTEGER ticksPerSecond;
  LARGE_INTEGER ticks;
  QueryPerformanceCounterAccuracy(&ticksPerSecond);
  QueryPerformanceCounter(&ticks);
  return ticks.QuadPart / (ticksPerSecond.QuadPart/1000000);
*/
#elif defined(MACOSX)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000000 + tv.tv_usec;
#elif defined(UNIX)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long) ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
#else
#error No suitable get_micros function
#endif
    }
#endif  // ! SMALL_MEM
/*...e*/
