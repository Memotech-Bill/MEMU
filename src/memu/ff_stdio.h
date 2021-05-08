/*  stdio.h  -   Use FatFS library to emulate stdio.h routines */

#ifndef H_STDIO
#define H_STDIO

#include <stdio.h>

#ifdef __Pico__

#define fopen       fio_fopen
#define fclose      fio_fclose
#define fputc       fio_fputc
#define fputs       fio_fputs
#define fgets       fio_fgets
#define fread       fio_fread
#define fwrite      fio_fwrite
#define fprintf     fio_fprintf
#define vfprintf    fio_vfprintf
#define ftell       fio_ftell
#define fseek       fio_fseek
#define fflush      fio_fflush
#define remove      fio_remove
#define rename      fio_rename
#define getc        fio_getc
#define ungetc      fio_ungetc

#ifndef NULL
#define NULL    0
#endif
#undef FILE
#define FILE    void
#ifndef EOF
#define EOF     (-1)
#endif
#ifndef SEEK_SET
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2
#endif

typedef unsigned int size_t;

#undef stdout
#undef stderr
#define stdout  (void *)(-1)
#define stderr  (void *)(-2)

#ifdef __cplusplus
extern "C"
    {
#endif

void fio_mount ();
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

// The following two routines only work reliably on one stream at a time
int getc (FILE *pf);
int ungetc (int iCh, FILE *pf);

#ifdef __cplusplus
    }
#endif

#endif

#endif
