/*

ctc.h - Z80 CTC

*/

#ifndef CTC_H
#define	CTC_H

/*...sincludes:0:*/
#include "types.h"
#include "common.h"

/*...vtypes\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...e*/

extern void ctc_init(void);
extern byte ctc_in(int channel);
extern void ctc_out(int channel, byte value);
extern void ctc_reload(int channel);
extern void ctc_trigger(int channel);
extern void ctc_advance(int adv);
extern BOOLEAN ctc_int_pending(void);
extern BOOLEAN ctc_int_ack(word *);
extern byte ctc_get_int_vector(void);
extern BOOLEAN ctc_reti (void);
extern double ctc_freq (int channel);
extern void ctc_stats (void);

#endif
