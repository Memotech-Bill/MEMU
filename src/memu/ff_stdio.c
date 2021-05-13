/*  ff_stdio.c  -   Use FatFS library to emulate stdio.h routines */

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ff_stdio.h>
#include <fatfs/ff.h>

#ifdef ALT_PRINTF
void ALT_PRINTF (const char *ps);
#endif
void fatal (const char *ps);

// #define DEBUG

static FATFS   vol;

void fio_mount (void)
    {
#ifdef DEBUG
    printf ("Mount FatFS\n");
#endif
    FRESULT fr = f_mount (&vol, "0:", 1);
    if ( fr != FR_OK ) fatal ("Failed to mount SD Card");
#ifdef DEBUG
    printf ("fr = %d\n", fr);
#endif
    }

static const char * MakeString (const char *fmt, va_list va)
    {
    static char sErr[] = "No space";
    static char *psBuff = NULL;
    static int nBuff = 0;
    if ( psBuff == NULL )
        {
        nBuff = 256;
        psBuff = (char *) malloc (nBuff);
        if ( psBuff == NULL ) return sErr;
        }
    int nCh = vsnprintf (psBuff, nBuff, fmt, va);
    if ( nCh >= nBuff )
        {
        free (psBuff);
        nBuff = nCh + 1;
        psBuff = (char *) malloc (nBuff);
        if ( psBuff == NULL ) return sErr;
        vsnprintf (psBuff, nBuff, fmt, va);
        }
    return (const char *) psBuff;
    }

FILE * fopen (const char *psPath, const char *psMode)
    {
    unsigned char mode = 0;
    FIL *pf = NULL;
#ifdef DEBUG
    printf ("fopen (%s, %s)\n", psPath, psMode);
#endif
    while ( *psMode )
        {
        switch (*psMode)
            {
            case 'r':
                mode |= FA_READ;
                break;
            case 'w':
                mode |= FA_CREATE_ALWAYS | FA_WRITE;
                break;
            case '+':
                mode |= FA_READ | FA_WRITE;
                break;
            case 'a':
                mode &= ~FA_CREATE_ALWAYS;
                mode |= FA_OPEN_APPEND | FA_WRITE;
                break;
            case 'x':
                mode |= FA_CREATE_NEW;
                break;
            default:
                break;
            }
        ++psMode;
        }
#ifdef DEBUG
    printf ("mode - %d\n", mode);
#endif
    pf = (FIL *) malloc (sizeof (FIL));
    if ( pf == NULL )
        {
#ifdef DEBUG
        printf ("Failed to allocate buffer\n");
#endif
        return pf;
        }
    FRESULT fr = f_open (pf, psPath, mode);
    if ( fr != FR_OK )
        {
#ifdef DEBUG
        printf ("Failed\n");
#endif
        free (pf);
        pf = NULL;
        }
#ifdef DEBUG
    printf ("fr = %d, pf = %p\n", fr, pf);
#endif
    return pf;
    }

int fclose (FILE *pf)
    {
#ifdef DEBUG
    printf ("fclose (%p)\n", pf);
#endif
    if ( ( pf == NULL ) || ( pf == stdout ) || ( pf ==stderr ) ) return 0;
    FRESULT fr = f_close ((FIL *) pf);
#ifdef DEBUG
    printf ("Close fr = %d\n");
#endif
    if ( fr != FR_OK ) return EOF;
    free (pf);
    return 0;
    }

int fputc (char ch, FILE *pf)
    {
    if ( ( pf == NULL ) || ( pf == stdout ) || ( pf ==stderr ) ) return EOF;
    if ( f_putc (ch, (FIL *) pf) != FR_OK ) return EOF;
    return (int)(unsigned char) ch;
    }

int fputs (const char *ps, FILE *pf)
    {
    if ( pf == NULL ) return EOF;
    else if ( ( pf == stdout ) || ( pf ==stderr ) )
        {
#ifdef ALT_PRINTF
        ALT_PRINTF (ps);
#else
        printf ("%s", ps);
#endif
        }
    else if ( f_puts (ps, (FIL *) pf) != FR_OK ) return EOF;
    return 1;
    }

char *fgets (char *ps, int size, FILE *pf)
    {
    return f_gets (ps, size, (FIL *)pf);
    }

size_t fread (void *ptr, size_t size, size_t nmemb, FILE *pf)
    {
    size_t in;
#ifdef DEBUG
    printf ("fread (%p, %d, %d, %p)\n", ptr, size, nmemb, pf);
#endif
    FRESULT fr = f_read ((FIL *) pf, ptr, size * nmemb, &in);
#ifdef DEBUG
    printf ("Read fr = %d, in = %d\n", fr, in);
#endif
    if ( fr != FR_OK ) return EOF;
    return in / size;
    }

size_t fwrite (const void *ptr, size_t size, size_t nmemb, FILE *pf)
    {
    size_t out;
#ifdef DEBUG
    printf ("fwrite (%p, %d, %d, %p)\n", ptr, size, nmemb, pf);
#endif
    if ( ( pf == NULL ) || ( pf == stdout ) || ( pf ==stderr ) ) return EOF;
    FRESULT fr = f_write ((FIL *) pf, ptr, size * nmemb, &out);
#ifdef DEBUG
    printf ("Write fr = %d, out = %d\n", fr, out);
#endif
    if ( fr != FR_OK ) return EOF;
    return out / size;
    }

int fprintf (FILE *pf, const char *fmt, ...)
    {
    int nByte;
    va_list va;
    va_start (va, fmt);
    nByte = vfprintf (pf, fmt, va);
    va_end (va);
    return nByte;
    }

int vfprintf (FILE *pf, const char *fmt, va_list va)
    {
    size_t out;
    const char *sOut;
    if ( pf == NULL ) return EOF;
    sOut = MakeString (fmt, va);
    if ( ( pf == stdout ) || ( pf ==stderr ) )
        {
        out = strlen (sOut);
#ifdef ALT_PRINTF
        ALT_PRINTF (sOut);
#else
        printf ("%s", sOut);
#endif
        }
    else
        {
        f_write ((FIL *) pf, sOut, strlen (sOut), &out);
        }
    return out;
    }

long ftell (FILE *pf)
    {
    return f_tell ((FIL *)pf);
    }

int fseek (FILE *pf, long offset, int whence)
    {
    if ( whence == SEEK_END )
        {
        offset += f_size ((FIL *)pf);
        }
    else if ( whence == SEEK_CUR )
        {
        offset += f_tell ((FIL *)pf);
        }
    else if ( whence != SEEK_SET )
        {
        return -1;
        }
    if ( f_lseek ((FIL *)pf, offset) != FR_OK ) return -1;
    return 0;
    }

int fflush (FILE *pf)
	{
	return 0;
	}

int remove (const char *psPath)
    {
    if ( f_unlink (psPath) != FR_OK ) return -1;
    return 0;
    }

int rename (const char *psOld, const char *psNew)
    {
    if ( f_rename (psOld, psNew) != FR_OK ) return -1;
    return 0;
    }

static int iUngetCh = -1;

int getc (FILE *pf)
    {
    int iCh = iUngetCh;
    if ( iCh >= 0 )
        {
        iUngetCh = -1;
        }
    else
        {
        size_t in;
        iCh = 0;
        f_read ((FIL *) pf, &iCh, 1, &in);
        if ( in != 1 ) iCh = EOF;
        }
    return iCh;
    }

int ungetc (int iCh, FILE *pf)
    {
    int iRes = iCh;
    if ( iUngetCh != -1 ) iRes = EOF;
    iUngetCh = iCh;
    return iRes;
    }
