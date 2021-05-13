/*  Dummy fcntl.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of fcntl.h loaded
#endif

#ifndef H_FCNTL
#define H_FCNTL

#define O_RDONLY    0x01
#define O_WRONLY    0x02
#define O_RDWR      0x03
#define O_NONBLOCK  0x04
#define O_NOCTTY    0x08
#define O_NDELAY    0x10

#ifdef __cplusplus
extern "C"
    {
#endif

    int open (const char *psOpen, int flags);
    int creat (const char *psOpen, int mode);
    int close (int fd);
    int read (int fd, void *pbuf, int nbyte);
    int write (int fd, void *pbuf, int nbyte);
    int isatty (int fd);

#ifdef __cplusplus
    }
#endif

#endif
