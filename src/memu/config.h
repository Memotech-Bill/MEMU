/*      config.h  -  Display a user interface for setting configuration options */

#ifndef H_CONFIG
#define H_CONFIG

#include "types.h"
#include "win.h"

#define WINCFG_WTH          80
#define WINCFG_HGT          24

#define STY_NORMAL          0   // ( 16 * CLR_NORMAL    + CLR_BACKGROUND )
#define STY_HIGHLIGHT       1   // ( 16 * CLR_HIGHLIGHT + CLR_HIGHBACK )
#define STY_DISABLED        2   // ( 16 * CLR_DISABLED  + CLR_BACKGROUND )
#define STY_HELP            3   // ( 16 * CLR_HELP      + CLR_BACKGROUND )
#define STY_COUNT           4

// extern void config (void);
extern void config (void);
extern void config_set_file (const char *psFile);
extern BOOLEAN test_cfg_key (int wk);
extern BOOLEAN read_config (const char *psFile, int *pargc, const char ***pargv);
extern BOOLEAN cfg_options (int *pargc, const char ***pargv, int *pi);
extern void cfg_usage (void);
extern void cfg_set_disk_dir (const char *psDir);
extern BOOLEAN cfg_test_file (const char *psPath);
extern char * cfg_find_file (const char *argv0);
extern WIN * get_cfg_win (void);
extern void cfg_keypress(int wk);
extern void cfg_keyrelease(int wk);

#endif
