/* hardware.c -	  Defines hardware used for emulation of MTX */

#include <stdio.h>
#include <unistd.h>
#include "types.h"
#include "TxtRead.h"
#include "hardware.h"
#include "gpio.h"
#include "win.h"
#include "kbd.h"
#include "vid.h"
#include "mon.h"
#include "config.h"
#include "common.h"
#include "diag.h"
#include "memu.h"
#ifdef __circle__
#include "kfuncs.h"
#endif

#if HAVE_GPU
void set_gpu_mode (int mode);
int get_gpu_mode (void);
#endif

#define	DEBOUNCE	10000	//	Keyboard debounce delay in micro-seconds

const char *psHWCfg = NULL;

BOOLEAN	 hwKbd	  =  FALSE;
BOOLEAN  hwKbdInv =  FALSE;
BOOLEAN	 hwJoy1	  =	 FALSE;
BOOLEAN	 hwJoy2	  =	 FALSE;
BOOLEAN	 hwPrn	  =  FALSE;
BOOLEAN	 hwPIO    =  FALSE;

int	  iRstDrive	  =	 0xFF;

struct gio_pin pinKbdD[8];
struct gio_pin pinKbdS[12];
struct gio_pin pinJoy1[5];
struct gio_pin pinJoy2[5];
struct gio_pin pinPrnD[8];
struct gio_pin pinPrnStb;
struct gio_pin pinPrnSta[4];
struct gio_pin pinPOT[8];
struct gio_pin pinPIN[8];

void hw_pindef (TXR *ptxr, struct gio_pin *ppin)
	{
	char sText[20];
	int	 iPin;
	TxrGetText (ptxr, sizeof (sText), sText);
	if ( ! strcasecmp (sText, "GPIO") )
		{
#if HAVE_HW_GPIO
		struct gio_dev *pdev  =	 gdev;
		char   sDev[LDEVNAME];
		TxrGetText (ptxr, sizeof (sDev), sDev);
		while ( pdev )
			{
			if ( ( pdev->type == gio_gpio ) && ( ! strcasecmp (sDev, pdev->sDev) ) ) break;
			pdev  =	 pdev->pnext;
			}
		if ( ! pdev )
			{
			pdev  =	 (struct gio_dev *) calloc (1, sizeof (struct gio_dev));
			if ( pdev == NULL )	 fatal ("Failed to allocate I/O device definition");
			pdev->type = gio_gpio;
			strcpy (pdev->sDev, sDev);
			pdev->pnext	=  gdev;
			gdev	=  pdev;
			}
		ppin->pdev	 =	pdev;
		iPin   =  TxrGetInt (ptxr);
		if ( ( iPin < 0 ) || ( iPin >= 30 ) )	fatal ("Invalid GPIO pin number: %d", iPin);
		ppin->iMask	 =	1 << iPin;
        pdev->iPins |= ppin->iMask;
#else
        fatal ("GPIO pins not supported");
#endif
		}
	else if ( ! strcasecmp (sText, "MCP23017") )
		{
#if HAVE_HW_MCP23017
		struct gio_dev *pdev  =	 gdev;
		char   sDev[LDEVNAME];
		int	   iAddr;
		TxrGetText (ptxr, sizeof (sDev), sDev);
		iAddr  =  TxrGetInt (ptxr);
		while ( pdev )
			{
			if ( ( pdev->type == gio_xio ) && ( ! strcasecmp (sDev, pdev->sDev) ) && ( iAddr == pdev->iAddr ) )	 break;
			pdev  =	 pdev->pnext;
			}
		if ( ! pdev )
			{
			pdev  =	 (struct gio_dev *) calloc (1, sizeof (struct gio_dev));
			if ( pdev == NULL )	 fatal ("Failed to allocate I/O device definition");
            pdev->type = gio_xio;
			strcpy (pdev->sDev, sDev);
			pdev->iAddr	=  iAddr;
			pdev->pnext	=  gdev;
			gdev	=  pdev;
			}
		ppin->pdev	 =	pdev;
		TxrGetText (ptxr, sizeof (sText), sText);
		iPin = -1;
		if ( ( ( sText[0] == 'A' ) || ( sText[0] == 'a' ) ) && ( sText[2] == '\0' ) )
			iPin = sText[1] - '0';
		else if ( ( ( sText[0] == 'B' ) || ( sText[0] == 'b' ) ) && ( sText[2] == '\0' ) )
			iPin = sText[1] - '0' + 8;
		else sscanf (sText, "%i", &iPin);
		if ( ( iPin < 0 ) || ( iPin >= 16 ) )	fatal ("Invalid MCP23017 pin number: %s", sText);
		ppin->iMask	 =	1 << iPin;
        pdev->iPins |= ppin->iMask;
#else
        fatal ("MCP23017 pins not supported");
#endif
		}
	else
		{
		fatal ("Invalid hardware pin definition: %s", sText);
		}
	}

void hw_read (const char *psFile)
	{
	TXR	 *ptxr =  TxrOpen (psFile);
	char sText[20];
	int	 iPin;
	if ( ptxr == NULL )	return;
	psHWCfg = psFile;
	TxrSetTerm (ptxr, "=,");
	TxrGetText (ptxr, sizeof (sText), sText);
	while ( ! TxrEof (ptxr) )
		{
		if ( ! strcasecmp (sText, "[KEYBOARD]") )
			{
			hwKbd	=  TRUE;
            diag_message (DIAG_KBD_HW, "Processing HW Keyboard definition");
			memset (pinKbdD, 0, sizeof (pinKbdD));
			memset (pinKbdS, 0, sizeof (pinKbdS));
			while ( ! TxrEof (ptxr) )
				{
				TxrNextLine (ptxr, 1);
				TxrGetText (ptxr, sizeof (sText), sText);
				if ( ! sText[0] ) continue;
				if ( sText[0] == '[' ) break;
				if ( ! strncasecmp (sText, "DR", 2) )
					{
					iPin   =  atoi (&sText[2]);
					if ( ( iPin < 0 ) || ( iPin > 7 ) )	 fatal ("Invalid keyboard drive pin: %d", iPin);
					hw_pindef (ptxr, &pinKbdD[iPin]);
					}
				else if ( ! strncasecmp (sText, "KB", 2) )
					{
					iPin   =  atoi (&sText[2]);
					if ( ( iPin < 0 ) || ( iPin > 9 ) )	 fatal ("Invalid keyboard sense pin: %d", iPin);
					hw_pindef (ptxr, &pinKbdS[iPin]);
					}
				else if ( ! strcasecmp (sText, "RESET") )
					{
					hw_pindef (ptxr, &pinKbdS[10]);
					}
				else if ( ! strcasecmp (sText, "RESET2") )
					{
					hw_pindef (ptxr, &pinKbdS[11]);
					}
				else if ( ! strcasecmp (sText, "DR_RESET") )
					{
					TxrGetText (ptxr, sizeof (sText), sText);
					if ( ! strcasecmp (sText, "GND") )
						{
						iRstDrive	=  0xFF;
						}
					else if ( ( ! strncasecmp (sText, "DR", 2) ) && ( sText[2] >= '0' )
						&& ( sText[2] <= '7' ) && ( sText[3] == '\0' ) )
						{
						iRstDrive =	 ( ~ ( 1 << ( sText[2] - '0' ) ) ) & 0xFF;
						}
					else
						{
						fatal ("Reset drive line must be GND or DR0-DR7");
						}
					}
				else if ( ! strcasecmp (sText, "INVERT") )
                    {
                    hwKbdInv = TRUE;
                    }
				else fatal ("Invalid keyboard pin definition: %s", sText);
				}
			}
		else if ( ! strcasecmp (sText, "[PRINTER]") )
			{
			hwPrn	=  TRUE;
			memset (pinPrnD, 0, sizeof (pinPrnD));
			memset (&pinPrnStb, 0, sizeof (pinPrnStb));
			memset (pinPrnSta, 0, sizeof (pinPrnSta));
			while ( ! TxrEof (ptxr) )
				{
				TxrNextLine (ptxr, 1);
				TxrGetText (ptxr, sizeof (sText), sText);
				if ( ! sText[0] ) continue;
				if ( sText[0] == '[' ) break;
				if ( sText[0] == 'D' )
					{
					iPin   =  atoi (&sText[1]);
					if ( ( iPin < 0 ) || ( iPin > 7 ) )	 fatal ("Invalid printer data pin: %d", iPin);
					hw_pindef (ptxr, &pinPrnD[iPin]);
					}
				else if ( ! strcasecmp (sText, "STROBE") )
					{
					hw_pindef (ptxr, &pinPrnStb);
					}
				else if ( ! strcasecmp (sText, "BUSY") )
					{
					hw_pindef (ptxr, &pinPrnSta[0]);
					}
				else if ( ! strcasecmp (sText, "ERROR") )
					{
					hw_pindef (ptxr, &pinPrnSta[1]);
					}
				else if ( ! strcasecmp (sText, "PE") )
					{
					hw_pindef (ptxr, &pinPrnSta[2]);
					}
				else if ( ! strcasecmp (sText, "SLCT") )
					{
					hw_pindef (ptxr, &pinPrnSta[3]);
					}
				else fatal ("Invalid printer pin definition: %s", sText);
				}
			}
		else if ( ! strcasecmp (sText, "[JOYSTICK_1]") )
			{
			hwJoy1	=  TRUE;
			memset (pinJoy1, 0, sizeof (pinJoy1));
			while ( ! TxrEof (ptxr) )
				{
				TxrNextLine (ptxr, 1);
				TxrGetText (ptxr, sizeof (sText), sText);
				if ( ! sText[0] ) continue;
				if ( sText[0] == '[' ) break;
				if ( ! strcasecmp (sText, "LEFT") )
					{
					hw_pindef (ptxr, &pinJoy1[0]);
					}
				else if ( ! strcasecmp (sText, "RIGHT") )
					{
					hw_pindef (ptxr, &pinJoy1[1]);
					}
				else if ( ! strcasecmp (sText, "UP") )
					{
					hw_pindef (ptxr, &pinJoy1[2]);
					}
				else if ( ! strcasecmp (sText, "DOWN") )
					{
					hw_pindef (ptxr, &pinJoy1[3]);
					}
				else if ( ! strcasecmp (sText, "FIRE") )
					{
					hw_pindef (ptxr, &pinJoy1[4]);
					}
				else fatal ("Invalid joystick pin definition: %s", sText);
				}
			}
		else if ( ! strcasecmp (sText, "[JOYSTICK_2]") )
			{
			hwJoy2	=  TRUE;
			memset (pinJoy2, 0, sizeof (pinJoy2));
			while ( ! TxrEof (ptxr) )
				{
				TxrNextLine (ptxr, 1);
				TxrGetText (ptxr, sizeof (sText), sText);
				if ( ! sText[0] ) continue;
				if ( sText[0] == '[' ) break;
				if ( ! strcasecmp (sText, "LEFT") )
					{
					hw_pindef (ptxr, &pinJoy2[0]);
					}
				else if ( ! strcasecmp (sText, "RIGHT") )
					{
					hw_pindef (ptxr, &pinJoy2[1]);
					}
				else if ( ! strcasecmp (sText, "UP") )
					{
					hw_pindef (ptxr, &pinJoy2[2]);
					}
				else if ( ! strcasecmp (sText, "DOWN") )
					{
					hw_pindef (ptxr, &pinJoy2[3]);
					}
				else if ( ! strcasecmp (sText, "FIRE") )
					{
					hw_pindef (ptxr, &pinJoy2[4]);
					}
				else fatal ("Invalid joystick pin definition: %s", sText);
				}
			}
		else if ( ! strcasecmp (sText, "[PIO]") )
			{
			hwPIO	=  TRUE;
			memset (pinPIN, 0, sizeof (pinPIN));
			memset (pinPOT, 0, sizeof (pinPOT));
			while ( ! TxrEof (ptxr) )
				{
				TxrNextLine (ptxr, 1);
				TxrGetText (ptxr, sizeof (sText), sText);
				if ( ! sText[0] ) continue;
				if ( sText[0] == '[' ) break;
				if ( ! strncasecmp (sText, "PIN", 3) )
					{
					iPin   =  atoi (&sText[3]);
					if ( ( iPin < 0 ) || ( iPin > 7 ) )	 fatal ("Invalid parallel input pin: %d", iPin);
					hw_pindef (ptxr, &pinPIN[iPin]);
					}
				else if ( ! strncasecmp (sText, "POT", 3) )
					{
					iPin   =  atoi (&sText[3]);
					if ( ( iPin < 0 ) || ( iPin > 7 ) )	 fatal ("Invalid parallel output pin: %d", iPin);
					hw_pindef (ptxr, &pinPOT[iPin]);
					}
				else fatal ("Invalid parallel port definition: %s", sText);
				}
			}
		else
			{
			fatal ("Invalid hardware configuration block: %s", sText);
			}
		}
	TxrClose (ptxr);
	}

void hw_save_cfg (FILE *pfil)
	{
#if HAVE_GPU
	int mode = get_gpu_mode ();
	if ( mode ) fprintf (pfil, "-gpu-mode %d\n", mode);
#endif
	if ( psHWCfg != NULL )	fprintf (pfil, "-hw-config \"%s\"\n", psHWCfg);
	}

//  Process additional command line options
BOOLEAN hw_options (int *pargc, const char ***pargv, int *pi)
    {
    if ( cfg_options (pargc, pargv, pi) )
        {
        return TRUE;
        }
#if HAVE_GPU
	else if ( !strcmp((*pargv)[*pi], "-gpu-mode") )
		{
		if ( ++(*pi) == (*pargc) )
			usage((*pargv)[*pi-1]);
		set_gpu_mode (atoi ((*pargv)[*pi]));
        return TRUE;
		}
#endif
    else if ( !strcmp((*pargv)[*pi], "-hw-config") )
        {
        if ( ++(*pi) == (*pargc) )
            usage((*pargv)[*pi-1]);
        hw_read ((*pargv)[*pi]);
        hw_init ();
        return TRUE;
        }
    return FALSE;
    }

//  Display additional command line options
void hw_usage (void)
    {
    cfg_usage ();
#if HAVE_GPU
	fprintf(stderr, "       -gpu-mode            mode for scaling window to display\n");
#endif
    fprintf(stderr, "       -hw-config file      read definitions of external hardware for MEMU\n");
    }

void hw_exit (int iReason)
    {
    hw_term ();
#ifdef __circle__
    Quit ();
#else
    exit (iReason);
#endif
    }

void hw_init (void)
	{
    diag_message (DIAG_INIT, "hw_init: hwKbd = %c, hwJoy1 = %c, hwJoy2 = %c, hwPrn = %c, hwPIO = %c",
        hwKbd ? 'Y' : 'N', hwJoy1 ? 'Y' : 'N', hwJoy2 ? 'Y' : 'N', hwPrn ? 'Y' : 'N', hwPIO ? 'Y' : 'N');
	if ( hwKbd || hwJoy1 || hwJoy2 || hwPrn || hwPIO )
		{
		if ( gio_init () )	fatal ("Failed to initialise IO hardware");
		}
	if ( hwKbd )
		{
        diag_message (DIAG_KBD_HW, "Initialise keyboard GPIO");
		gio_input (8, pinKbdD, 0xFF);
		gio_pullup (8, pinKbdD, 0xFF);
		gio_input (12, pinKbdS, 0xFFF);
		gio_pullup (12, pinKbdS, 0xFFF);
		}
	if ( hwJoy1 )
		{
		gio_input (5, pinJoy1, 0x1F);
		gio_pullup (5, pinJoy1, 0x1F);
		}
	if ( hwJoy2 )
		{
		gio_input (5, pinJoy2, 0x1F);
		gio_pullup (5, pinJoy2, 0x1F);
		}
	if ( hwPrn )
		{
		gio_input (4, pinPrnSta, 0x0F);
		gio_pullup (4, pinPrnSta, 0x0F);
		gio_output (8, pinPrnD, 0xFF);
		gio_put (1, &pinPrnStb, 0x01);
		gio_output (1, &pinPrnStb, 0x01);
		gio_put (1, &pinPrnStb, 0x01);
		}
	if ( hwPIO )
		{
		gio_input (8, pinPIN, 0xFF);
		gio_output (8, pinPOT, 0xFF);
		}
	}

void hw_term (void)
	{
	if ( hwKbd )
		{
		gio_pullnone (10, pinKbdS, 0x3FF);
		}
	if ( hwJoy1 )
		{
		gio_pullnone (5, pinJoy1, 0x1F);
		}
	if ( hwJoy2 )
		{
		gio_pullnone (5, pinJoy2, 0x1F);
		}
	if ( hwPrn )
		{
		gio_pullnone (4, pinPrnSta, 0x0F);
		gio_input (8, pinPrnD, 0xFF);
		gio_input (1, &pinPrnStb, 0x01);
		}
	if ( hwPIO )
		{
		gio_input (8, pinPOT, 0xFF);
		}
	if ( hwKbd || hwJoy1 || hwJoy2 || hwPrn )	gio_term ();
	}

void hw_kbd_drive (int iOut)
	{
	if ( hwKbd )
		{
        static	int	  iState   =  0xFF;
        if ( iOut != iState )
            {
            //	Simulate open collector / open drain outputs by switching between
            //	output low (0) and input (high impedance) (1).
            diag_message (DIAG_KBD_HW, "GPIO Keyboard drive = 0x%02X", iOut);
            gio_input (8, pinKbdD, iOut);
            gio_output (8, pinKbdD, ~iOut);
            gio_put (8, pinKbdD, iOut);
            iState =  iOut;
            }
        }
	}

word hw_joy1_sense (word wDrive)
	{
	word wSense	  =	 0xFFFF;
	if ( hwJoy1 && ( ( wDrive & 0x7C ) != 0x7C ) )
		{
		int	iJoy  =	 gio_get (5, pinJoy1);
		if ( ( ! ( wDrive & 0x08 ) ) && ( ! ( iJoy & 0x01 ) ) )	  wSense   &= 0x7F;
		if ( ( ! ( wDrive & 0x10 ) ) && ( ! ( iJoy & 0x02 ) ) )	  wSense   &= 0x7F;
		if ( ( ! ( wDrive & 0x04 ) ) && ( ! ( iJoy & 0x04 ) ) )	  wSense   &= 0x7F;
		if ( ( ! ( wDrive & 0x40 ) ) && ( ! ( iJoy & 0x08 ) ) )	  wSense   &= 0x7F;
		if ( ( ! ( wDrive & 0x20 ) ) && ( ! ( iJoy & 0x10 ) ) )	  wSense   &= 0x7F;
		}
	return	wSense;
	}

word hw_joy2_sense_1 (word wDrive)
	{
	word wSense	  =	 0xFFFF;
	if ( hwJoy2 && ( ! ( wDrive & 0x80 ) ) )
		{
		int	iJoy  =	 gio_get (4, pinJoy2);
		wSense &= ( iJoy | 0xFFF0 );
		}
	return	wSense;
	}

word hw_joy2_sense_2 (word wDrive)
	{
	word wSense	  =	 0xFFFF;
	if ( hwJoy2 && ( ! ( wDrive & 0x80 ) ) )
		{
		int	iJoy  =	 gio_get (1, &pinJoy2[4]);
		if ( ! ( iJoy & 0x01 ) ) wSense	  &= 0xFFFE;
		}
	return	wSense;
	}

word hw_kbd_sense_1 (word wDrive)
	{
	word wSense	  =	 0xFFFF;
	if ( hwKbd )
		{
		// static int nCount = 0;
		hw_kbd_drive (wDrive);
		wSense   &= (word) gio_get (8, pinKbdS);
        diag_message (DIAG_KBD_HW, "GPIO Keyboard sense 1 = 0x%02X", wSense);
        if ( hwKbdInv ) wSense ^= 0xFF;
		// diag_message (DIAG_ALWAYS, "hw_kbd_sense_1 (%d) = 0x%02x", ++nCount, wSense);
		// if ( nCount >= 5 )	fatal ("End of test.");
		// if ( ( wSense & 0xFF ) != 0xFF )
		//   {
		//    diag_message (DIAG_ALWAYS, "wDrive = 0x%02x  wSense = 0x%02x", wDrive, wSense);
		//     fatal ("Key press detected");
		//   }
		}
	if ( hwJoy1 ) wSense   &= hw_joy1_sense (wDrive);
	if ( hwJoy2 ) wSense   &= hw_joy2_sense_1 (wDrive);
	return	wSense;
	}

word hw_kbd_sense_2 (word wDrive)
	{
	word wSense	  =	 0xFFFF;
	if ( hwKbd )
		{
		hw_kbd_drive (wDrive);
		wSense   &= (word) gio_get (2, &pinKbdS[8]);
        diag_message (DIAG_KBD_HW, "GPIO Keyboard sense 2 = 0x%02X", wSense);
        if ( hwKbdInv ) wSense ^= 0x03;
		}
	if ( hwJoy2 ) wSense   &= hw_joy2_sense_2 (wDrive);
	return	wSense;
	}

BOOLEAN hw_Z80_out (word port, byte value)
	{
	if ( hwPrn && ( ( port & 0xff ) == 0x04 ) )
		{
		//	Printer data
		gio_put (8, pinPrnD, value);
		return	TRUE;
		}
	if ( hwPIO && ( ( port & 0xff ) == 0x07 ) )
		{
		//	Parallel port
		gio_put (8, pinPOT, value);
		return	TRUE;
		}
	return	FALSE;
	}

BOOLEAN hw_Z80_in (word port, byte *value)
	{
	if ( hwPrn && ( ( port & 0xff ) == 0x00 ) )
		{
		//	Printer strobe low
		gio_put (1, &pinPrnStb, 0);
		*value	 =	0xFF;
		return TRUE;
		}
	if ( hwPrn && ( ( port & 0xff ) == 0x04 ) )
		{
		//	Printer strobe high and read status
		gio_put (1, &pinPrnStb, 1);
		*value	 =	gio_get (4, pinPrnSta) | 0xf0;
		return TRUE;
		}
	if ( hwPIO && ( ( port & 0xff ) == 0x07 ) )
		{
		*value = (byte) gio_get (8, pinPIN);
		return	TRUE;
		}
	return	FALSE;
	}


#define	 KEY_SHFT1	 60
#define	 KEY_SHFT2	 66
#define	 KEY_CTRL	 20
#define	 KEY_LOCK	 40

int hw_key (void)
	{
	static int iLast =	-1;
	static BOOLEAN bAlpha	=  FALSE;
	static BOOLEAN bOldLk	=  FALSE;
	BOOLEAN bShift	 =	FALSE;
	BOOLEAN bCtrl	 =	FALSE;
	BOOLEAN bLock	 =	FALSE;
	BOOLEAN bChg	 =	FALSE;
	int	 iPress	  =	 -1;
	int	 iKey;
	int	 iDrive;
	int	 iScan;
	int	 iSense;
	static const char chUnSh[]  =	 "13579-\\\x0C\x03\x81"
		"\x1B" "24680^\x05\x08\x85"
		"\x00wryip[\x0B\x09\x82"
		"qetuo@\x0A\x08\x7F\x86"
		"\x00sfhk;]\x19\x00\x87"
		"adgjl:\x0D\x1A\x00\x83"
		"\x00xvn,/\x00\x0A\x00\x88"
		"zcbm._\x15\x0C \x84";
	static const char chShft[]  =	 "!#%')=|79\x89"
		"\x1B\"$&(0~8\x08\x8D"
		"\x00WRYIP{54\x8A"
		"QETUO`\x0A" "16\x8E"
		"\x00SFHK+}3\x00\x8F"
		"ADGJL*\x0D" "2\x00\x8B"
		"\x00XVN<?\x00.\x00\x90"
		"ZCBM>_0\x0D \x8C";
	iKey =	0;
	for ( iDrive = 0x01; iDrive <= 0x80; iDrive <<= 1 )
		{
		hw_kbd_drive (~iDrive);
		iSense = gio_get (10, pinKbdS);
		for ( iScan = 0x01; iScan <= 0x200; iScan <<= 1 )
			{
			if ( ! ( iSense & iScan ) )
				{
				if ( ( iKey == KEY_SHFT1 ) || ( iKey == KEY_SHFT2 ) )
					{
					bShift =  TRUE;
					}
				else if ( iKey == KEY_CTRL )
					{
					bCtrl =	  TRUE;
					}
				else if ( iKey == KEY_LOCK )
					{
					bLock  =  TRUE;
					}
				else if ( iPress == -1 )
					{
					iPress =  iKey;
					}
				}
			++iKey;
			}
		}
	if ( bLock && ( ! bOldLk ) )
		{
		bAlpha	 =  ! bAlpha;
		bChg	 =	TRUE;
		}
	if ( ( iPress >= 0 ) && ( iPress != iLast ) )
		{
		bChg  =	 TRUE;
		if ( bShift )  iKey	 =	chShft[iPress];
		else		   iKey	 =	chUnSh[iPress];
		if ( bCtrl )   iKey	 &=	0x1F;
		else if ( bAlpha && ( iKey >= 'a' ) && ( iKey <= 'z' ) ) iKey  -= 0x20;
		}
	else
		{
		iKey  =	 -1;
		}
	iLast	=  iPress;
	bOldLk	=  bLock;
	if ( bChg )	  usleep (DEBOUNCE);
	return	iKey;
	}

int hw_key_scan (void)
	{
	static int iScan[] = { 0x00, 0x01, 0x02, 0x03,
						   0x04, WK_End, 0x06, 0x07,
						   WK_Left, 0x09, WK_Down, WK_Up,
						   0x0C, 0x0D, 0x0E, 0x0F,
						   0x10, 0x11, 0x12, 0x13,
						   0x14, WK_Insert, WK_Delete, 0x17,
						   0x18, WK_Right, WK_Home, 0x1B,
						   0x1C, 0x1D, 0x1E, 0x1F};
	int iKey = hw_key ();
	if ( ( iKey >= 0x00 ) && ( iKey < 0x20 ) )
		iKey = iScan[iKey];
	return iKey;
	}

BOOLEAN hw_handle_events (WIN *win)
	{
	static int iLast =	0x03;
	int	 iSense;
	if ( ! hwKbd )	 return	  FALSE;
	if ( iRstDrive != 0xFF )  hw_kbd_drive (iRstDrive);
	iSense = gio_get (2, &pinKbdS[10]);
	if ( ( win != get_cfg_win () ) && ( ! ( iSense & 0x01 ) ) )
		{
		hw_kbd_drive (0xBF);
		iSense = gio_get (8, pinKbdS);
		if ( ! ( iSense & 0x01 ) )
			{
			// Left shift
			win_next ();
			}
		else if ( ! ( iSense & 0x40 ) )
			{
			// Right shift
			win_prev ();
			}
		else
			{
			// No shift
			config ();
			}
		return TRUE;
		}
	else if ( ( win != get_cfg_win () ) && ( ! ( iSense & 0x02 ) ) && ( iLast & 0x02 ) )
		{
		win_next ();
		usleep (DEBOUNCE);
		return TRUE;
		}
	iLast	=	iSense;
	return	FALSE;
	}
