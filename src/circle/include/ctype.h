/*  Dummy ctypes.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of ctype.h loaded
#endif

#ifndef H_CTYPES
#define H_CTYPES

#ifdef __cplusplus
extern "C"
    {
#endif

    inline int isupper (int ch) { return ( ch >= 'A' ) && ( ch <= 'Z' ); }
    inline int islower (int ch) { return ( ch >= 'a' ) && ( ch <= 'z' ); }
    inline int isalpha (int ch) { return isupper (ch) || islower (ch); }
    inline int tolower (int ch) { return isupper (ch) ? ( ch | 0x20 ) : ch; }
    inline int toupper (int ch) { return islower (ch) ? ( ch & (~ 0x20) ) : ch; }
    inline int isprint (int ch) { return ((ch >= 0x20) && (ch < 0x7F )); }

#ifdef __cplusplus
    }
#endif

#endif
