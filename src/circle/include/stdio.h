/*  stdio.h  -   Circle dummy header - Use FatFS library to emulate stdio.h routines */

#ifdef SHOW_HDR
#warning Circle version of stdio.h loaded
#endif

#ifndef H_STDIO
#define H_STDIO

#include <circle/stdarg.h>
#include <linux/kernel.h>       // Provides [v]s[n]printf functions

#ifndef NULL
#define NULL    0
#endif
#define FILE    void
#ifndef EOF
#define EOF     (-1)
#endif
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

typedef unsigned int size_t;

#define stdout  (void *)(-1)
#define stderr  (void *)(-2)

#ifdef __cplusplus
extern "C"
    {
#endif

int vsprintf (char *ps, const char *fmt, va_list va);

FILE * fopen (const char *psPath, const char *psMode);
int fclose (FILE *pf);
int fputc (char ch, FILE *pf);
int fputs (const char *ps, FILE *pf);
char *fgets (char *ps, int size, FILE *pf);
size_t fread (void *ptr, size_t size, size_t nmemb, FILE *pf);
size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *pf);
int fprintf (FILE *pf, const char *fmt, ...);
int vfprintf (FILE *pf, const char *fmt, va_list va);
long ftell (FILE *pf);
int fseek (FILE *pf, long offset, int whence);
int fflush (FILE *pf);
int remove (const char *psPath);
int rename (const char *psOld, const char *psNew);
int sscanf (const char *psStr, const char *psFmt, ...);
int atoi (const char *ps);
double atof (const char *ps);

// The following two routines only work reliably on one stream at a time
int getc (FILE *pf);
int ungetc (int iCh, FILE *pf);

#ifdef __cplusplus
    }
#endif

#endif
