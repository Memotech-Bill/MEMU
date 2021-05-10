/*

diag.c - Diagnostic code

*/

/*...sincludes:0:*/
#include "ff_stdio.h"
#include <string.h>
#include <time.h>

#ifdef WIN32
  #define WIN32_LEAN_AND_MEAN
  #define BOOLEAN BOOLEANx
  #include <windows.h>
  #undef BOOLEAN
#elif defined(UNIX)
  #include <unistd.h>
#endif

#include "types.h"
#include "diag.h"
#ifndef SMALL_MEM
#include "vdeb.h"
#endif
#include "memu.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...e*/

/*...svars:0:*/
unsigned int diag_methods = 0;
BOOLEAN diag_flags[DIAG_COUNT];
static const char *diag_file_fn = "memu.log";

#ifndef SMALL_MEM
static const char *diag_ring_fn = "memu.ring";
#define	RING_SIZE 0x10000
static int produce = 0, consume = 0;
static char *ring[RING_SIZE];
#endif

#define CLOG_SIZE   255
static char sChipLog[CLOG_SIZE+2];
static int nCLog = 0;
/*...e*/

/*...smethodvals:0:*/
typedef struct
	{
	const char *name;
	unsigned int value;
	} METHODVAL;

static METHODVAL methodvals[] =
	{
		{"console"	,DIAGM_CONSOLE},
		{"file"		,DIAGM_FILE},
#ifndef SMALL_MEM
		{"ring"		,DIAGM_RING},
#endif
	};
/*...e*/
/*...sdiag_method_of:0:*/
unsigned int diag_method_of(const char *s)
	{
	int i;
	if ( !strncmp(s, "file=", 5) )
		{
		diag_file_fn = s+5;
		return DIAGM_FILE;
		}
#ifndef SMALL_MEM
	if ( !strncmp(s, "ring=", 5) )
		{
		diag_ring_fn = s+5;
		return DIAGM_RING;
		}
#endif
	for ( i = 0; i < sizeof(methodvals)/sizeof(methodvals[0]); i++ )
		if ( !strcmp(s, methodvals[i].name) )
			return methodvals[i].value;
	return 0;
	}
/*...e*/

/*...sflagvals:0:*/
typedef struct
	{
	const char *name;
	unsigned int value;
	} FLAGVAL;

static FLAGVAL flagvals[] =
	{
		{"time"	,DIAG_TIME},
		{"win-unknown-key"	,DIAG_WIN_UNKNOWN_KEY},
		{"mem-iobyte"		,DIAG_MEM_IOBYTE},
		{"mem-subpage"		,DIAG_MEM_SUBPAGE},
		{"mem-dump"		,DIAG_MEM_DUMP},
		{"vid-status"		,DIAG_VID_STATUS},
		{"vid-registers"	,DIAG_VID_REGISTERS},
		{"vid-address"		,DIAG_VID_ADDRESS},
		{"vid-data"		,DIAG_VID_DATA},
		{"vid-refresh"		,DIAG_VID_REFRESH},
		{"vid-markers"		,DIAG_VID_MARKERS},
		{"vid-time-check"	,DIAG_VID_TIME_CHECK},
		{"vid-time-check-abort"	,DIAG_VID_TIME_CHECK_ABORT},
		{"vid-time-check-drop" 	,DIAG_VID_TIME_CHECK_DROP},
		{"kbd-win-key"		,DIAG_KBD_WIN_KEY},
		{"kbd-drive"		,DIAG_KBD_DRIVE},
		{"kbd-sense"		,DIAG_KBD_SENSE},
		{"kbd-auto-type"	,DIAG_KBD_AUTO_TYPE},
		{"kbd-remap"		,DIAG_KBD_REMAP},
		{"joy-init"		,DIAG_JOY_INIT},
		{"joy-usage"		,DIAG_JOY_USAGE},
		{"snd-init"		,DIAG_SND_INIT},
		{"snd-registers"	,DIAG_SND_REGISTERS},
		{"snd-latency"		,DIAG_SND_LATENCY},
		{"ctc-registers"	,DIAG_CTC_REGISTERS},
		{"ctc-pending"		,DIAG_CTC_PENDING},
		{"ctc-interrupt"	,DIAG_CTC_INTERRUPT},
		{"ctc-count"	,DIAG_CTC_COUNT},
		{"mon-hw"		,DIAG_MON_HW},
		{"mon-kbd-win-key"	,DIAG_MON_KBD_WIN_KEY},
		{"mon-kbd-map-th"	,DIAG_MON_KBD_MAP_TH},
		{"sdxfdc-ports"		,DIAG_SDXFDC_PORT},
		{"sdxfdc-hw"		,DIAG_SDXFDC_HW},
		{"sdxfdc-data"		,DIAG_SDXFDC_DATA},
		{"sdxfdc-status"	,DIAG_SDXFDC_STATUS},
		{"sidisc-file"		,DIAG_SIDISC_FILE},
		{"sidisc-address"	,DIAG_SIDISC_ADDRESS},
		{"sidisc-data"		,DIAG_SIDISC_DATA},
		{"print"		,DIAG_PRINT},
 		{"cpm-driver"		,DIAG_CPM_DRIVER},
 		{"cpm-cbios"		,DIAG_CPM_CBIOS},
 		{"cpm-bdos-file"	,DIAG_CPM_BDOS_FILE},
 		{"cpm-bdos-other"	,DIAG_CPM_BDOS_OTHER},
		{"z80-instructions"	,DIAG_Z80_INSTRUCTIONS},
		{"z80-instructions-new"	,DIAG_Z80_INSTRUCTIONS_NEW}, /* undocumented */
		{"z80-interrupts"	,DIAG_Z80_INTERRUPTS},
 		{"z80-time"		,DIAG_Z80_TIME},
 		{"z80-time-iperiod"	,DIAG_Z80_TIME_IPERIOD},
		{"bad-port-display"	,DIAG_BAD_PORT_DISPLAY},
		{"bad-port-ignore"	,DIAG_BAD_PORT_IGNORE},
		{"panel-hack"		,DIAG_PANEL_HACK},
		{"tape"			,DIAG_TAPE},
		{"speed"		,DIAG_SPEED},
		{"exit"			,DIAG_EXIT},
		{"win-hw"		,DIAG_WIN_HW},
		{"kbd-hw"		,DIAG_KBD_HW},
		{"dart-ports"		,DIAG_DART_PORTS},
		{"dart-config"		,DIAG_DART_CFG},
		{"dart-data"		,DIAG_DART_DATA},
		{"dart-hw"			,DIAG_DART_HW},
		{"spec-ports"		,DIAG_SPEC_PORTS},
		{"spec-interrupts"	,DIAG_SPEC_INTERRUPTS},
		{"node-pkt-tx"	,DIAG_NODE_PKT_TX},
		{"node-pkt-rx"	,DIAG_NODE_PKT_RX},
		{"log-type", DIAG_LOG_TYPE},
        {"init", DIAG_INIT},
        {"gpio", DIAG_GPIO},
        {"vga-mode", DIAG_VGA_MODE},
        {"vga-port", DIAG_VGA_PORT},
        {"vga-refresh", DIAG_VGA_REFRESH},
        {"nfx-port", DIAG_NFX_PORT},
        {"nfx-reg", DIAG_NFX_REG},
        {"nfx-event", DIAG_NFX_EVENT},
        {"nfx-data", DIAG_NFX_DATA},
        {"chip-log", DIAG_CHIP_LOG}
	};
/*...e*/
/*...sdiag_flag_of:0:*/
BOOLEAN diag_flag_of(const char *s)
	{
	int i;
    int n = (int) strlen (s);
	BOOLEAN found = FALSE;
	if ( !strcmp (s, "all") )
		{
		for ( i = DIAG_ALWAYS; i < DIAG_ALL_COUNT; ++i )
			diag_flags[i] = TRUE;
		return TRUE;
		}
	for ( i = 0; i < sizeof(flagvals)/sizeof(flagvals[0]); i++ )
		{
		if ( ( !strncmp(s, flagvals[i].name, n) ) && ( ( flagvals[i].name[n] == '\0' )
                || ( flagvals[i].name[n] == '-' ) ) )
			{
            if ( flagvals[i].value == DIAG_Z80_INSTRUCTIONS_NEW )
                {
                // This should not be activated by "-diag-instructions"
                if ( flagvals[i].name[n] != '\0' ) continue;
                }
            diag_flags[flagvals[i].value] = TRUE;
            found = TRUE;
			}
		}
	return found;
	}
/*...e*/

/*...sdiag_message:0:*/
void diag_message(unsigned int flag, const char *fmt, ...)
	{
	if ( diag_flags[flag] )
		{
		va_list	vars;
		char s[1024+1];     // char s[256+1]; - WJB: Increased to allow for long disassembly of RST codes.
		va_start(vars, fmt);
        if ( diag_flags[DIAG_TIME] )
            {
            sprintf (s, "%16llX: ", get_Z80_clocks ());
            vsprintf(&s[18], fmt, vars);
            }
        else
            {
            vsprintf(s, fmt, vars);
            }
		va_end(vars);
        strcat (s, "\n");
		if ( diag_methods & DIAGM_CONSOLE )
			{
			fputs(s, stdout);
			fflush(stdout);
			}
		if ( diag_methods & DIAGM_FILE )
			{
			FILE *fp;
			if ( (fp = fopen(diag_file_fn, "a+")) != NULL )
				{
				fputs(s, fp);
				fclose(fp);
				}
			}
#ifndef SMALL_MEM
		if ( diag_methods & DIAGM_RING )
			{
			char *s2;
			if ( ring[produce%RING_SIZE] != NULL )
				free(ring[produce%RING_SIZE]);
			s2 = malloc(strlen(s)+1);
			if ( s2 != NULL )
				{
				strcpy(s2, s);
				ring[produce%RING_SIZE] = s2;
				++produce;
				if ( produce%RING_SIZE == consume%RING_SIZE )
					++consume;
				}
			else
				diag_methods &= ~DIAGM_RING;
			}
#endif
		}
	}
/*...e*/

void diag_out (word port, byte value)
    {
    static BOOLEAN bEnable = TRUE;
    BOOLEAN bFlush = FALSE;
    switch (port & 0xFF)
        {
        case 0xEE:
            if ( value == 0x00 ) bEnable = FALSE;
            else if ( value == 0x01 ) bEnable = TRUE;
            else if ( value == 0x02 ) diag_flags[DIAG_Z80_INSTRUCTIONS] = FALSE;
            else if ( value == 0x03 ) diag_flags[DIAG_Z80_INSTRUCTIONS] = bEnable;
            else if ( value == 0x0A ) bFlush = bEnable;
            else if ( bEnable )
                {
                sChipLog[nCLog] = (char) value;
                ++nCLog;
                }
            break;
        case 0xEF:
            if ( bEnable )
                {
                sprintf (&sChipLog[nCLog], "%02X", value);
                nCLog += 2;
                }
            break;
        }
    if ( bFlush || ( nCLog > CLOG_SIZE ) )
        {
        sChipLog[nCLog] = '\0';
        diag_message (DIAG_CHIP_LOG, "%s", sChipLog);
        nCLog = 0;
        }
    }

#ifdef SMALL_MEM
#define LM(x)
#else
#define LM(x) x
#endif

/*...sdiag_control:0:*/
void diag_control(int c)
	{
	switch ( c )
		{
     LM(case 'h': vdeb_break (); break;)
		case 'c': diag_methods ^= DIAGM_CONSOLE; break;
		case 'a': diag_flags[DIAG_SPEED_UP         ] ^= TRUE; break;
		case 'b': diag_flags[DIAG_MEM_IOBYTE       ] ^= TRUE; break;
     LM(case 'd': diag_flags[DIAG_ACT_MEM_DUMP     ]  = TRUE; break;)
//      case 'e': snd_query (); break;
		case 'f': diag_flags[DIAG_CPM_BDOS_FILE    ] ^= TRUE; break;
		case 'i': diag_flags[DIAG_Z80_INTERRUPTS   ] ^= TRUE; break;
		case 'k': diag_flags[DIAG_KBD_SENSE        ] ^= TRUE; break;
     LM(case 'l': diag_flags[DIAG_ACT_SNA_LOAD     ]  = TRUE; break;)
		case 'm': diag_flags[DIAG_SPEED            ] ^= TRUE; break;
 		case 'n': diag_flags[DIAG_ACT_VID_DUMP_VDP ]  = TRUE; break;
 		case 'o': diag_flags[DIAG_VID_AUTO_DUMP_VDP] ^= TRUE; break;
		case 'p': diag_flags[DIAG_BAD_PORT_DISPLAY ] ^= TRUE; break;
		case 'q': diag_flags[DIAG_BAD_PORT_IGNORE  ] ^= TRUE; break;
     LM(case 'r': diag_flags[DIAG_ACT_Z80_REGS	   ]  = TRUE; break;)
	 LM(case 's': diag_flags[DIAG_ACT_SNA_SAVE	   ]  = TRUE; break;)
	 LM(case 't': diag_flags[DIAG_ACT_TAP_REWIND   ]  = TRUE; break;)
	 LM(case 'v': diag_flags[DIAG_ACT_VID_REGS	   ]  = TRUE; break;)
	 LM(case 'w': diag_flags[DIAG_ACT_VID_SNAPSHOT ]  = TRUE; break;)
	 LM(case 'x': diag_flags[DIAG_VID_AUTO_SNAPSHOT] ^= TRUE; break;)
	 LM(case 'y': diag_flags[DIAG_SPEC_PORTS	   ] ^= TRUE; break;)
		case 'z': diag_flags[DIAG_Z80_INSTRUCTIONS ] ^= TRUE; break;
		}
	}	
/*...e*/

/*...sdiag_init:0:*/
void diag_init(void)
	{
	int i;
	for ( i = 0; i < DIAG_COUNT; ++i )
	   diag_flags[i]  =  FALSE;
	diag_flags[DIAG_ALWAYS] =  TRUE;
#ifndef SMALL_MEM
	for ( i = 0; i < RING_SIZE; i++ )
		ring[i] = NULL;
#endif
	}
/*...e*/
/*...sdiag_term:0:*/
void diag_term(void)
	{
    sChipLog[nCLog] = '\0';
    if ( nCLog > 0 ) diag_message (DIAG_CHIP_LOG, "%s", sChipLog);
#ifndef SMALL_MEM
	if ( diag_methods & DIAGM_RING )
		{
		FILE *fp;
		if ( (fp = fopen(diag_ring_fn, "w")) != NULL )
			{
			while ( consume < produce )
				{
				fputs(ring[(consume++)%RING_SIZE], fp);
				}
			fclose(fp);
			}
		}
#endif
	}
/*...e*/
