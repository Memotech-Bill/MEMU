/*

diag.h - Diagnostic code

*/

#ifndef DIAG_H
#define	DIAG_H

/*...sincludes:0:*/
#include <stdlib.h>
#include <stdarg.h>

#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#ifdef HAVE_VDEB
#ifndef HAVE_DISASS
#define HAVE_DISASS 1
#endif
#ifndef Z80_DEBUG
#define Z80_DEBUG   1
#endif
#endif

#define	DIAGM_CONSOLE               0x00000001
#define	DIAGM_FILE                  0x00000002
#define	DIAGM_RING                  0x00000004

enum DIAGS {
    DIAG_TIME,
    DIAG_ALWAYS,
    DIAG_WIN_UNKNOWN_KEY,
    DIAG_MEM_IOBYTE,
    DIAG_MEM_SUBPAGE,
    DIAG_MEM_DUMP,
    DIAG_VID_STATUS,
    DIAG_VID_REGISTERS,
    DIAG_VID_ADDRESS,
    DIAG_VID_DATA,
    DIAG_VID_REFRESH,
    DIAG_VID_MARKERS,
    DIAG_VID_TIME_CHECK,
    DIAG_VID_TIME_CHECK_ABORT,
    DIAG_VID_TIME_CHECK_DROP,
    DIAG_KBD_WIN_KEY,
    DIAG_KBD_DRIVE,
    DIAG_KBD_SENSE,
    DIAG_KBD_AUTO_TYPE,
    DIAG_KBD_REMAP,
    DIAG_JOY_INIT,
    DIAG_JOY_USAGE,
    DIAG_SND_INIT,
    DIAG_SND_REGISTERS,
    DIAG_SND_LATENCY,
    DIAG_CTC_REGISTERS,
    DIAG_CTC_PENDING,
    DIAG_CTC_INTERRUPT,
    DIAG_CTC_COUNT,
    DIAG_MON_HW,
    DIAG_MON_KBD_WIN_KEY,
    DIAG_MON_KBD_MAP_TH,
    DIAG_SDXFDC_PORT,
    DIAG_SDXFDC_HW,
    DIAG_SDXFDC_DATA,
    DIAG_SDXFDC_STATUS,
    DIAG_SIDISC_FILE,
    DIAG_SIDISC_ADDRESS,
    DIAG_SIDISC_DATA,
    DIAG_PRINT,
    DIAG_CPM_DRIVER,
    DIAG_CPM_CBIOS,
    DIAG_CPM_BDOS_FILE,
    DIAG_CPM_BDOS_OTHER,
    DIAG_Z80_INSTRUCTIONS,
    DIAG_Z80_INSTRUCTIONS_NEW,
    DIAG_Z80_INTERRUPTS,
    DIAG_Z80_TIME,
    DIAG_Z80_TIME_IPERIOD,
    DIAG_BAD_PORT_DISPLAY,
    DIAG_BAD_PORT_IGNORE,
    DIAG_PANEL_HACK,
    DIAG_TAPE,
    DIAG_SPEED,
    DIAG_EXIT,
    DIAG_WIN_HW,
    DIAG_KBD_HW,
    DIAG_DART_PORTS,
    DIAG_DART_CFG,
    DIAG_DART_DATA,
    DIAG_SPEC_PORTS,
    DIAG_SPEC_INTERRUPTS,
    DIAG_DART_HW,
    DIAG_NODE_PKT_TX,
    DIAG_NODE_PKT_RX,
    DIAG_LOG_TYPE,
    DIAG_INIT,
    DIAG_GPIO,
    DIAG_I2C,
    DIAG_VGA_MODE,
    DIAG_VGA_PORT,
    DIAG_VGA_REFRESH,
    DIAG_CHIP_LOG,
    DIAG_NFX_PORT,
    DIAG_NFX_REG,
    DIAG_NFX_EVENT,
    DIAG_NFX_DATA,
    DIAG_MFX_CFG,
    DIAG_MFX_FONT,
    DIAG_MFX_MEM,
    DIAG_MFX_PAL,
    DIAG_MFX_PORT,
    DIAG_MFX_TEXT,
    DIAG_ALL_COUNT,
// The below are actions, and should not be activated by -diag-all
    DIAG_ACT_VID_REGS = DIAG_ALL_COUNT,
    DIAG_ACT_Z80_REGS,
    DIAG_ACT_MEM_DUMP,
    DIAG_SPEED_UP,
    DIAG_ACT_SNA_LOAD,
    DIAG_ACT_SNA_SAVE,
    DIAG_ACT_TAP_REWIND,
	DIAG_ACT_VID_DUMP_VDP,
	DIAG_VID_AUTO_DUMP_VDP,
    DIAG_ACT_VID_SNAPSHOT,
    DIAG_VID_AUTO_SNAPSHOT,
    DIAG_ACT_VID_DUMP,
    DIAG_COUNT
    };

extern unsigned int diag_methods;
extern unsigned int diag_method_of(const char *s);
extern BOOLEAN diag_flags[DIAG_COUNT];
extern BOOLEAN diag_flag_of(const char *s);
extern void diag_message(unsigned int flag, const char *fmt, ...);
extern void diag_out(word port, byte value);
extern void diag_control(int c);
extern void diag_init(void);
extern void diag_term(void);

#endif
