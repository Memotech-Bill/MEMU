/*

cpm.h - Interface to CP/M emulation

*/

#ifndef CPM_H
#define	CPM_H

/*...sincludes:0:*/
#include "Z80.h"

/*...vZ80\46\h:0:*/
/*...e*/

extern BOOLEAN cpm_patch(Z80 *r);
extern BOOLEAN cpm_patch_sdx(Z80 *r);
extern BOOLEAN cpm_patch_fdxb(Z80 *r);

extern void cpm_init(void);
extern void cpm_init_sdx(void);
extern void cpm_init_fdxb(void);
extern void cpm_set_drive_a(const char *cpm_drive_a);
extern const char *cpm_get_drive_a(void);
extern void cpm_set_invert_case(void);
extern void cpm_set_tail(const char *tail);
extern void cpm_allow_open_hack(BOOLEAN bAllow);
extern void cpm_force_filename (const char *fn);

#endif
