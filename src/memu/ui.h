/*

ui.h - User Interface

*/

#ifndef UI_H
#define	UI_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	UI_MEM  0x01
#define	UI_VRAM 0x02
#define	UI_DIS  0x04

extern void ui_refresh(void);

extern void ui_init(int opts);
extern void ui_term(void);

extern void ui_mem_set_title (const char *title);
extern const char * ui_mem_get_title (void);
extern void ui_mem_set_display (const char *display);
extern const char * ui_mem_get_display (void);

extern void ui_vram_set_title (const char *title);
extern const char * ui_vram_get_title (void);
extern void ui_vram_set_display (const char *display);
extern const char * ui_vram_get_display (void);

extern void ui_dis_set_title (const char *title);
extern const char * ui_dis_get_title (void);
extern void ui_dis_set_display (const char *display);
extern const char * ui_dis_get_display (void);

#endif
