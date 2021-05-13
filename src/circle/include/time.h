/*  Dummy time.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of time.h loaded
#endif

#ifndef H_TIME
#define H_TIME

#define CLOCK_REALTIME  0

#ifdef __cplusplus
extern "C"
    {
#endif

	struct timespec
        {
        unsigned int tv_sec;
        unsigned int tv_nsec;
        };
    
	int nanosleep (struct timespec *ts_req, struct timespec *ts_rem);
    int clock_gettime (int clock, struct timespec *ts);

#ifdef __cplusplus
    }
#endif

#endif
