/*

dis.h - Z80 Disassembler

*/

#ifndef DIS_H
#define	DIS_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

extern BOOLEAN use_syms;
extern BOOLEAN show_opcode;
extern BOOLEAN dis_show_ill;
extern BOOLEAN dis_mtx_exts;

extern void dis_init(void);
extern BOOLEAN dis_instruction(word *a, char *s);
extern BOOLEAN dis_ref_code(word a, word *r);

#endif
