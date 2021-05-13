/*  Dummy unistd.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of unistd.h loaded
#endif

#ifndef H_UNISTD
#define H_UNISTD

#ifdef __cplusplus
extern "C"
    {
#endif

    int usleep (int iMs);

#ifdef __cplusplus
    }
#endif

#endif
