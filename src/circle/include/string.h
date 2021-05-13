/*  Dummy string.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of string.h loaded
#endif

#ifndef H_STRING
#define H_STRING

#include <circle/util.h>

#ifndef NULL
#define NULL    0
#endif

#ifdef __cplusplus
extern "C"
    {
#endif

    char *strdup (const char *);
    const char *strrchr (const char *psStr, int ch);
    char *strtok (char *str, const char *delim);

#ifdef __cplusplus
    }
#endif

#endif
