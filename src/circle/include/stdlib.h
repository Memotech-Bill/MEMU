/*  Dummy stdlib.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of stdlib.h loaded
#endif

#ifndef H_STDLIB
#define H_STDLIB

#include <circle/alloc.h>

#ifndef NULL
#define NULL    0
#endif

#ifdef __cplusplus
extern "C"
    {
#endif

    void exit (int status);

#ifdef __cplusplus
    }
#endif

#endif
