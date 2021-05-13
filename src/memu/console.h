/* An on-screen text display compatible with Circle Logger and MEMU */

#ifndef H_CONSOLE
#define H_CONSOLE

#define BOOT_DUMP   "/boot.log"

#ifdef __cplusplus

#include <circle/device.h>

class CConsole : public CDevice
	{
    public:
	CConsole (void);
	~CConsole (void);
    boolean Initialize (void);
	int Write (const void *pBuffer, size_t nCount);
    void Show (void);
#ifdef BOOT_DUMP
    void Dump (void);
#endif
	};

extern "C"
	{
#endif

extern void con_init (void);
extern void con_term (void);
extern void con_print (const char *ps, int nCh);
extern void con_alert (const char *psFmt, ...);
extern void con_show (void);
extern void printf_circle (const char *ps);

#ifdef __cplusplus
	}
#endif

#endif
