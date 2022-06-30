/*

memu.h - Structure to hold MEMU configuration

*/

#ifndef H_MEMU
#define H_MEMU

/*...sincludes:0:*/
#include "types.h"
#ifdef HAVE_SID
#include "sid.h"
#endif
#ifdef HAVE_CFX2
#include "cfx2.h"
#endif

/*...vtypes\46\h:0:*/
/*...vsid\46\h:0:*/
/*...e*/

typedef struct s_cfg
	{
	int vid_emu;
	int vid_width_scale;
	int vid_height_scale;
	int mon_emu;
	int mon_width_scale;
	int mon_height_scale;
	int kbd_emu;
#ifdef HAVE_JOY
	int joy_emu;
#endif
	int snd_emu;
#ifdef HAVE_SID
	int sid_emu;
	int ui_opts;
#endif
	int iperiod;
	int tracks_sdxfdc[2];
	int joy_central;
	double latency;
#ifdef HAVE_JOY
	const char *joy_buttons;
#endif
	const char *fn_sdxfdc[2];
#ifdef HAVE_SID
	const char *sid_fn[N_SIDISC];
#endif
	const char *rom_fn[8];
	const char *fn_print;
	const char *tape_name_prefix;
	const char *tape_fn;
	const char *fn_serial_in[2];
	const char *fn_serial_out[2];
	BOOLEAN bSerialDev[2];
    BOOLEAN tape_overwrite;
	BOOLEAN tape_disable;
	int screen_refresh;
#ifdef HAVE_CFX2
    BOOLEAN bCFX2;
    const char *rom_cfx2;
    const char *fn_cfx2[NCF_CARD][NCF_PART];
#endif
#ifdef HAVE_VGA
    BOOLEAN bVGA;
#endif
	} CFG;

extern CFG cfg;

#ifdef __cplusplus
extern "C"
    {
#endif

extern int memu (int argc, const char *argv[]);
extern void OutZ80_bad(const char *hardware, word port, byte value, BOOLEAN stop);
extern byte InZ80_bad(const char *hardware, word port, BOOLEAN stop);
extern void memu_reset(void);
    extern void usage(const char *psErr, ...);
extern void unimplemented (const char *psErr);
extern void RaiseInt (const char *psSource);
extern unsigned long long get_Z80_clocks (void);
extern void tape_patch (BOOLEAN bPatch);
extern int read_file(const char *fn, byte *buf, int buflen);
extern void show_instruction (void);
    
#include "Z80.h"
extern Z80 *get_Z80_regs (void);

#ifdef __cplusplus
    }
#endif

#endif
