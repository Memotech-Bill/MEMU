/*  Dummy errno.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of errno.h loaded
#endif

#ifndef H_STDLIB
#define H_STDLIB

#define EINVAL          1
#define EAGAIN          2
#define EWOULDBLOCK     3

extern int errno;

#endif
