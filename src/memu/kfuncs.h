/* Operating system type functions defined by the Circle kernel */

#ifndef H_KFUNCS
#define H_KFUNCS

#include <circle/stdarg.h>
#include "types.h"

#ifdef __cplusplus
extern "C"
    {
#endif

    struct FBInfo
        {
        int     xres;
        int     yres;
        int     xstride;
        byte *  pfb;
        };

    unsigned int GetUSec (void);
    void GetFBInfo (struct FBInfo *pfinfo);
    void SetFBPalette (int iCol, unsigned int iRGB);
    void UpdateFBPalette (void);
    int GetKeyEvent (byte *pkey);
    void SetKbdLeds (u8 uLeds);
    int InitSound (void);
    void TermSound (void);
    void CircleYield (void);
    void Quit (void);
    const char * MakeString (const char *fmt, va_list va);
    
#ifdef __cplusplus
    }
#endif

#endif
    
