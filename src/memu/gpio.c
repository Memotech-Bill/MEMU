/* gpio.c - Routines to access RPi GPIO for joystick emulation.

Original version strongly based upon Gertboard test software:
   Copyright (C) Gert Jan van Loo & Myra VanInwegen 2012
   No rights reserved
   You may treat this program as if it was in the public domain

Revised based upon tiny_gpio.c by @joan
   tiny_gpio.c
   http://abyz.co.uk/rpi/pigpio/code/tiny_gpio.zip
   2015-09-12
   Public Domain

I2C code based upon "Interfacing an I2C GPIO expander (MCP23017) to the Raspberry Pi using C++ (i2cdev)"
   http://hertaville.com/interfacing-an-i2c-gpio-expander-mcp23017-to-the-raspberry-pi-using-c.html
   2014 Hussam Al-Hertani

*/

#include "gpio.h"
#include "diag.h"

#include <stdint.h>
#include <unistd.h>
#ifdef __circle__
#include <circle/bcm2835.h>
#else
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

// I/O access
static volatile uint32_t *gpio	=  NULL;

#define	 GPIO_LEN		0xB4
#define	 GPSET0			7
#define	 GPSET1			8
#define	 GPCLR0			10
#define	 GPCLR1			11
#define	 GPLEV0			13
#define	 GPLEV1			14
#define	 GPPUD			37
#define	 GPPUDCLK0		38
#define	 GPPUDCLK1		39

/* gpio modes. */

#define PI_INPUT  0
#define PI_OUTPUT 1
#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

/* Values for pull-ups/downs off, pull-down and pull-up. */

#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2

// General I/O devices

struct gio_dev *gdev = NULL;

#if HAVE_HW_GPIO
//
// Set up memory regions to access the peripherals.
// No longer requires root access.
//
int gpio_init (void)
	{
#ifdef __circle__
    gpio = (uint32_t *) ARM_GPIO_BASE;
	return	GPIO_OK;
#else
    int	 fd;
    if ( ( fd = open("/dev/gpiomem", O_RDWR|O_SYNC) ) < 0 )	 return	  GPIO_ERR_MEM;
	diag_message (DIAG_GPIO, "gpio_init: fd = %d", fd);

    //	 mmap GPIO

    gpio = (uint32_t *) mmap (NULL, GPIO_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	diag_message (DIAG_GPIO, "gpio_init: gpio = %p", gpio);
	close (fd);

	if ( (long) gpio < 0 ) return	GPIO_ERR_MAP;

	return	GPIO_OK;
#endif
	}

void gpio_term (void)
	{
#ifdef __circle__
    gpio = NULL;
#else
	if ( gpio != NULL )
		{
		munmap ((void *) gpio, GPIO_LEN);
		gpio  =	 NULL;
		}
#endif
	}

void gpio_input (int iMask)
	{
	int	 iPin;
	int	 iReg;
	int	 iBits;
	diag_message (DIAG_GPIO, "gpio_input (0x%08x)", iMask);
	if ( gpio == NULL )	return;
	for ( iReg = 0; iReg < 3; ++iReg )
		{
		iBits  =  7;
		for ( iPin = 0; iPin < 10; ++iPin )
			{
			if ( iMask & 1 )   gpio[iReg]	&= ~iBits;
			iMask >>=	1;
			iBits <<=	3;
			}
		}
	}

void gpio_output (int iMask)
	{
	int	 iPin;
	int	 iReg;
	int	 iBits;
	int	 iOut;
	diag_message (DIAG_GPIO, "gpio_output (0x%08x)", iMask);
	if ( gpio == NULL )	return;
	for ( iReg = 0; iReg < 3; ++iReg )
		{
		iBits  =  7;
		iOut   =  PI_OUTPUT;
		for ( iPin = 0; iPin < 10; ++iPin )
			{
			if ( iMask & 1 )   gpio[iReg]	=	( gpio[iReg] & (~iBits) ) | iOut;
			iMask >>=	1;
			iBits <<=	3;
			iOut  <<=	3;
			}
		}
	}

void gpio_pullup (int iMask)
	{
	diag_message (DIAG_GPIO, "gpio_pullup (0x%08x)", iMask);
	if ( gpio == NULL ) return;
	gpio[GPPUD]		 =	PI_PUD_UP;
	usleep(20);
	gpio[GPPUDCLK0]	 =	iMask;
	usleep(20);
	gpio[GPPUD]		 =	0;
	gpio[GPPUDCLK0]	 =	0;
	}

void gpio_pullnone (int iMask)
	{
	diag_message (DIAG_GPIO, "gpio_pullnone (0x%08x)", iMask);
	if ( gpio == NULL ) return;
	gpio[GPPUD]		 =	PI_PUD_OFF;
	usleep(20);
	gpio[GPPUDCLK0]	 =	iMask;
	usleep(20);
	gpio[GPPUD]		 =	0;
	gpio[GPPUDCLK0]	 =	0;
	}

int gpio_get (int iMask)
	{
	int iValue;
	diag_message (DIAG_GPIO, "gpio_get: gpio = %p", gpio);
	if ( gpio == NULL )	return	 0;
	iValue = iMask & gpio[GPLEV0];
	diag_message (DIAG_GPIO, "gpio_get (0x%08x) = 0x%08x", iMask, iValue);
	return iValue;
	}


void gpio_put (int iMask, int iBits)
	{
	diag_message (DIAG_GPIO, "gpio_put (0x%08x, 0x%08x)\n", iMask, iBits);
	if ( gpio == NULL )	return;
	gpio[GPSET0]  =	 iMask & iBits;
	gpio[GPCLR0]  =	 iMask & ( ~iBits );
	}

#ifndef __circle__
int gpio_revision (void)
	{
	char sLine [256], *ps, ch;
	FILE *pfil;
	static int	iRev   =  -1;
	if ( iRev > 0 )	 return	  iRev;
	pfil =  fopen ("/proc/cpuinfo", "r");
	if	( pfil == NULL ) return	 -1;
	while ( fgets (sLine, sizeof (sLine), pfil) )
		{
		if ( strncmp (sLine, "Revision", 8) == 0 )
			{
            int iCode = 0;
			ps =  &sLine[8];
			while ( *ps )
				{
                ch = *ps;
                if ( ( ch >= '0' ) && ( ch <= '9' ) ) iCode = 16 * iCode + ch - '0';
                else if ( ( ch >= 'A' ) && ( ch <= 'F' ) ) iCode = 16 * iCode + ch - 'A' + 10;
                else if ( ( ch >= 'a' ) && ( ch <= 'f' ) ) iCode = 16 * iCode + ch - 'a' + 10;
				++ps;
				}
            if ( iCode & ( 1 << 23 ) )
                {
/*
uuuuuuuuFMMMCCCCPPPPTTTTTTTTRRRR

Bits    Part 	Represents 	    Options
24-31   uuuuuuuu 	Unused 	    Unused
23      F 	    New flag 	    1: new-style revision
                                0: old-style revision
20-22   MMM 	Memory size 	0: 256 MB
		                        1: 512 MB
		                        2: 1 GB
16-19   CCCC 	Manufacturer 	0: Sony UK
		                        1: Egoman
                                2: Embest
                                3: Sony Japan
                                4: Embest
                                5: Stadium
12-15   PPPP 	Processor 	    0: BCM2835
		                        1: BCM2836
		                        2: BCM2837
4-11    TTTTTTTT 	Type 	    0: A
		                        1: B
                                2: A+
                                3: B+
                                4: 2B
                                5: Alpha (early prototype)
                                6: CM1
                                8: 3B
                                9: Zero
                                a: CM3
                                c: Zero W
                                d: 3B+
                                e: 3A+
                                f: Internal use only
                                10: CM3+
0-3     RRRR 	    Revision 	0, 1, 2, etc.
 */
                if ( ( ( iCode >> 4 ) & 0xFF ) < 2 )
                    {
                    // Model A or B.
                    if ( ( iCode & 0x0F ) == 1 ) iRev = 1;
                    else                         iRev = 2;
                    }
                else
                    {
                    iRev = 3;
                    }
                }
            else
                {
/*
Code 	Model 	Revision 	RAM 	Manufacturer
0002 	B 	    1.0 	    256 MB 	Egoman
0003 	B 	    1.0 	    256 MB 	Egoman
0004 	B 	    2.0 	    256 MB 	Sony UK
0005 	B 	    2.0 	    256 MB 	Qisda
0006 	B 	    2.0 	    256 MB 	Egoman
0007 	A 	    2.0 	    256 MB 	Egoman
0008 	A 	    2.0 	    256 MB 	Sony UK
0009 	A 	    2.0 	    256 MB 	Qisda
000d 	B 	    2.0 	    512 MB 	Egoman
000e 	B 	    2.0 	    512 MB 	Sony UK
000f 	B 	    2.0 	    512 MB 	Egoman
0010 	B+ 	    1.2 	    512 MB 	Sony UK
0011 	CM1 	1.0 	    512 MB 	Sony UK
0012 	A+ 	    1.1 	    256 MB 	Sony UK
0013 	B+ 	    1.2 	    512 MB 	Embest
0014 	CM1 	1.0 	    512 MB 	Embest
0015 	A+ 	    1.1 	    256 MB / 512 MB 	Embest
 */
                if ( iCode <= 0x03 )      iRev = 1;
                else if ( iCode <= 0x0F ) iRev = 2;
                else                      iRev = 3;
                }
			break;
			}
		}
	fclose (pfil);
	return	iRev;
	}
#endif
#endif

#if HAVE_HW_MCP23017
int i2c_init (const char *psDev, int iAddr)
	{
	int	  fd   =  open (psDev, O_RDWR);
	if ( fd < 0 )	return	 I2C_EOPEN;
	if ( ioctl (fd, I2C_SLAVE, iAddr) < 0 ) return	  I2C_EADDR;
	return	fd;
	}

void i2c_term (int fd)
	{
	if ( fd > 0 ) close (fd);
	}

int i2c_put (int fd, int iAddr, int iLen, unsigned char *pbData)
	{
	int	 iSta  =  I2C_OK;
    struct i2c_rdwr_ioctl_data pkt;
    struct i2c_msg msg[1];
    unsigned char *pbBuff = NULL;
    if ( fd < 0 ) return I2C_EOPEN;
    pbBuff = (unsigned char *) malloc (iLen + 1);
    if ( pbBuff == NULL )	return	 I2C_EMEM;
    pbBuff[0]  =  (unsigned char) iAddr;
    memcpy (&pbBuff[1], pbData, iLen);

    msg[0].addr	  =	 iAddr;
    msg[0].flags  =	 0;
    msg[0].len	  =	 iLen + 1;
    msg[0].buf	  =	 pbBuff;

    pkt.msgs   =  msg;
    pkt.nmsgs  =  1;

    if ( ioctl (fd, I2C_RDWR, &pkt) < 0 ) iSta =  I2C_EIO;
	free (pbBuff);
    return	iSta;
	}

int i2c_get (int fd, int iAddr, int iLen, unsigned char *pbData)
	{
    unsigned char bAddr = (unsigned char) iAddr;
    struct i2c_rdwr_ioctl_data pkt;
    struct i2c_msg msg[2];

	if ( fd < 0 ) return I2C_EOPEN;

    msg[0].addr	  =	 iAddr;
    msg[0].flags  =	 0;
    msg[0].len	  =	 iLen + 1;
    msg[0].buf	  =	 &bAddr;
    msg[1].addr	  =	 iAddr;
    msg[1].flags  =	 I2C_M_RD;
    msg[1].len	  =	 iLen;
    msg[1].buf	  =	 pbData;

    pkt.msgs   =  msg;
    pkt.nmsgs  =  2;

    if ( ioctl (fd, I2C_RDWR, &pkt) < 0 ) return   I2C_EIO;
    return	I2C_OK;
	}

int xio_init (const char *psDev, int iAddr)
	{
	return	i2c_init (psDev, iAddr);
	}

void xio_term (int fd)
	{
	i2c_term (fd);
	}

void xio_input (int fd, int iMask)
	{
	unsigned char bDir[2];
	i2c_get (fd, 0x00, 2, bDir);
	bDir[0]	|= (unsigned char) ( 0xFF & iMask );
	bDir[1]	|= (unsigned char) ( 0xFF & ( iMask >> 8 ) );
	i2c_put (fd, 0x00, 2, bDir);
	}

void xio_output (int fd, int iMask)
	{
	unsigned char bDir[2];
	i2c_get (fd, 0x00, 2, bDir);
	bDir[0]	&= ~ (unsigned char) ( 0xFF & iMask );
	bDir[1]	&= ~ (unsigned char) ( 0xFF & ( iMask >> 8 ) );
	i2c_put (fd, 0x00, 2, bDir);
	}

void xio_pullup (int fd, int iMask)
	{
	unsigned char bPull[2];
	i2c_get (fd, 0x0C, 2, bPull);
	bPull[0]	|= (unsigned char) ( 0xFF & iMask );
	bPull[1]	|= (unsigned char) ( 0xFF & ( iMask >> 8 ) );
	i2c_put (fd, 0x0C, 2, bPull);
	}

void xio_pullnone (int fd, int iMask)
	{
	unsigned char bPull[2];
	i2c_get (fd, 0x0C, 2, bPull);
	bPull[0]	&= ~ (unsigned char) ( 0xFF & iMask );
	bPull[1]	&= ~ (unsigned char) ( 0xFF & ( iMask >> 8 ) );
	i2c_put (fd, 0x0C, 2, bPull);
	}

int xio_get (int fd, int iMask)
	{
	unsigned char bData[2];
	i2c_get (fd, 0x12, 2, bData);
	return	( ( (int) bData[1] << 8 ) | ( (int) bData[0] ) ) & iMask;
	}

void xio_put (int fd, int iMask, int iBits)
	{
	unsigned char bData[2];
	i2c_get (fd, 0x14, 2, bData);
	bData[0]	&= ~ (unsigned char) ( 0xFF & iMask );
	bData[1]	&= ~ (unsigned char) ( 0xFF & ( iMask >> 8 ) );
	iBits	&= iMask;
	bData[0]	|= (unsigned char) ( 0xFF & iBits );
	bData[1]	|= (unsigned char) ( 0xFF & ( iBits >> 8 ) );
	i2c_put (fd, 0x14, 2, bData);
	}
#endif

int gio_init (void)
	{
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            int	 iSta  =  gpio_init ();
            if ( iSta != GPIO_OK ) return	iSta;
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            pdev->fd  =	 xio_init (pdev->sDev, pdev->iAddr);
            if ( pdev->fd < 0 )	  return   pdev->fd;
            }
		pdev   =  pdev->pnext;
		}
#endif
	return	GPIO_OK;
	}

void gio_term (void)
	{
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            gpio_term ();
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            if ( pdev->fd > 0 )	  xio_term (pdev->fd);
            }
#endif
		pdev   =  pdev->pnext;
		}
	}

void gio_clear (void)
	{
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
		pdev->iMask	 =	0;
		pdev->iData	 =	0;
		pdev   =  pdev->pnext;
		}
	}

void gio_set (struct gio_pin *ppin, int iData)
	{
	if ( ppin->pdev )
		{
		ppin->pdev->iMask |=	ppin->iMask;
		if ( iData )  ppin->pdev->iData	|= ppin->iMask;
		}
	}

void gio_pins (int nPin, struct gio_pin *ppin, int iData)
    {
	int	 iMask =  1;
	int	 iPin;
	gio_clear ();
	for ( iPin = 0; iPin < nPin; ++iPin )
		{
		gio_set (ppin, iData & iMask);
		iMask  <<=	 1;
		++ppin;
		}
    }

void gio_input (int nPin, struct gio_pin *ppin, int iData)
	{
	diag_message (DIAG_GPIO, "gio_input (%d, %p, 0x%02x)", nPin, ppin, iData);
    gio_pins (nPin, ppin, iData);
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            if ( pdev->iData )	gpio_input (pdev->iData);
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            if ( ( pdev->fd > 0 ) && ( pdev->iData ) )	  xio_input (pdev->fd, pdev->iData);
            }
#endif
		pdev   =  pdev->pnext;
		}
	}

void gio_output (int nPin, struct gio_pin *ppin, int iData)
	{
	diag_message (DIAG_GPIO, "gio_output (%d, %p, 0x%02x)", nPin, ppin, iData);
    gio_pins (nPin, ppin, iData);
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            if ( pdev->iData )	gpio_output (pdev->iData);
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            if ( ( pdev->fd > 0 ) && ( pdev->iData ) )	  xio_output (pdev->fd, pdev->iData);
            }
#endif
		pdev   =  pdev->pnext;
		}
	}

void gio_pullup (int nPin, struct gio_pin *ppin, int iData)
	{
	diag_message (DIAG_GPIO, "gio_pullup (%d, %p, 0x%02x)", nPin, ppin, iData);
    gio_pins (nPin, ppin, iData);
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            if ( pdev->iData )	gpio_pullup (pdev->iData);
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            if ( ( pdev->fd > 0 ) && ( pdev->iData ) )	  xio_pullup (pdev->fd, pdev->iData);
            }
#endif
		pdev   =  pdev->pnext;
		}
	}

void gio_pullnone (int nPin, struct gio_pin *ppin, int iData)
	{
	diag_message (DIAG_GPIO, "gio_pullnone (%d, %p, 0x%02x)", nPin, ppin, iData);
    gio_pins (nPin, ppin, iData);
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            if ( pdev->iData )	gpio_pullnone (pdev->iData);
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            if ( ( pdev->fd > 0 ) && ( pdev->iData ) )	  xio_pullnone (pdev->fd, pdev->iData);
            }
#endif
		pdev   =  pdev->pnext;
		}
	}

int gio_get (int nPin, struct gio_pin *ppin)
	{
	diag_message (DIAG_GPIO, "gio_get (%d, %p)", nPin, ppin);
    gio_pins (nPin, ppin, -1);
	struct gio_dev * pdev = gdev;
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            diag_message (DIAG_GPIO, "pdev->iMask = 0x%08x", pdev->iMask);
            if ( pdev->iMask )	pdev->iData	=  gpio_get (pdev->iMask);
            diag_message (DIAG_GPIO, "*(%p) = 0x%08x", &gdev, pdev->iData);
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            if ( ( pdev->fd > 0 ) && ( pdev->iMask ) )	  pdev->iData =	 xio_get (pdev->fd, pdev->iMask);
            }
#endif
		pdev   =  pdev->pnext;
		}
    int iData = 0;
    int iMask = 1;
    int iPin;
	for ( iPin = 0; iPin < nPin; ++iPin )
		{
		if ( ppin->pdev )
			{
			diag_message (DIAG_GPIO, "iPin = %d, pdev = %p, iMask = 0x%08x, iData = 0x%08x, iTest = 0x%08x",
				iPin, ppin->pdev, ppin->iMask, ppin->pdev->iData, ppin->iMask & ppin->pdev->iData);
			if ( ppin->iMask & ppin->pdev->iData )  iData	|= iMask;
			}
		else
			{
			// Undefined pins return high.
			diag_message (DIAG_GPIO, "iPin = %d Undefined", iPin);
			iData	|= iMask;
			}
		iMask  <<=	 1;
		++ppin;
		}
	diag_message (DIAG_GPIO, "gio_get = 0x%02x", iData);
	return	iData;
	}

void gio_put (int nPin, struct gio_pin *ppin, int iData)
	{
	diag_message (DIAG_GPIO, "gio_put (%d, %p, 0x%02x)", nPin, ppin, iData);
    gio_pins (nPin, ppin, iData);
	struct gio_dev * pdev = gdev;
	diag_message (DIAG_GPIO, "iMask = 0x%08x, iData = 0x%08x", pdev->iMask, pdev->iData);
	while ( pdev != NULL )
		{
#if HAVE_HW_GPIO        
        if ( pdev->type == gio_gpio )
            {
            if ( pdev->iData )	gpio_put (pdev->iMask, pdev->iData);
            }
#endif
#if HAVE_HW_MCP23017
        if ( pdev->type == gio_xio )
            {
            if ( ( pdev->fd > 0 ) && ( pdev->iData ) )	  xio_put (pdev->fd, pdev->iMask, pdev->iData);
            }
#endif
		pdev   =  pdev->pnext;
		}
	}
