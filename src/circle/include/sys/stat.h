/*  Dummy stat.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of sys/stat.h loaded
#endif

#ifndef H_STAT
#define H_STAT

#define S_IFREG 0x01
#define S_IFDIR 0x02
#define S_ISREG(m)  ( m & S_IFREG )

#ifdef __cplusplus
extern "C"
    {
#endif

    struct stat
        {
        int st_mode;
        int st_size;
        };

    int stat (const char *psPath, struct stat *st);

#ifdef __cplusplus
    }
#endif

#endif
