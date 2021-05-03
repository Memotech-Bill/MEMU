/*

snd.h - Sound

*/

#ifndef SND_H
#define	SND_H

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	SNDEMU_PORTAUDIO       0x01

#ifdef __cplusplus
extern "C"
    {
#endif

extern void snd_out6(byte val);
extern byte snd_in3(void);

extern void snd_init(int emu, double latency);
extern void snd_term(void);

#ifdef __circle__
int snd_callback (short *outputBuffer, unsigned long framesPerBuffer);
#endif

#ifdef __cplusplus
    }
#endif

#endif
