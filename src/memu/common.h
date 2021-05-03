/*

common.h - Handy utilities

*/

#ifndef COMMON_H
#define	COMMON_H

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#ifdef __cplusplus
extern "C"
    {
#endif

extern void terminate(const char *reason);
extern void fatal(const char *fmt, ...);
extern void *emalloc(size_t size);
extern char *estrdup(const char *s);
extern FILE *efopen(const char *fn, const char *mode);
extern char *make_path (const char *psDir, const char *psFile);
extern void delay_millis(long ms);
extern void delay_micros(long us);
extern long long get_millis(void);
extern long long get_micros(void);

#ifdef __circle__
extern void con_alert_on ();
extern void con_alert_off ();
#define ALERT_ON()    con_alert_on ()
#define ALERT_OFF()   con_alert_off ()
#else
#define ALERT_ON()
#define ALERT_OFF()
#endif

#ifdef __cplusplus
    }
#endif

#endif
