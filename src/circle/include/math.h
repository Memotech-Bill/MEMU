/*  Dummy math.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of math.h loaded
#endif

#ifndef H_MATH
#define H_MATH

#ifdef __cplusplus
extern "C"
    {
#endif

inline double fmod (double x, double y) { return x - y * ((int)( x / y )); }

#ifdef __cplusplus
    }
#endif

#endif
