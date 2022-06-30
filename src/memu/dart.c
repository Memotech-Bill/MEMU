/*

dart.c - Serial I/O emulator

Contributed by Bill Brendling.

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "dart.h"
#include "ctc.h"
#include "common.h"
#include "diag.h"
#include "dirmap.h"

#define	 NUM_CH				  2		// Number of DART channels

// Register bits

#define	 RR0_RX_AVAILABLE	  0x01
#define	 RR0_INT_PENDING	  0x02
#define	 RR0_TX_EMPTY		  0x04
#define	 RR0_DCD			  0x08
#define	 RR0_RI				  0x10
#define	 RR0_CTS			  0x20
#define	 RR0_BREAK			  0x80
#define	 RR0_MODEM			  ( RR0_DCD | RR0_RI | RR0_CTS )

#define	 RR1_ALL_SENT		  0x01
#define	 RR1_PARITY_ERROR	  0x10
#define	 RR1_RX_OVERRUN		  0x20
#define	 RR1_FRAMING_ERROR	  0x40

#define	 WR0_REGSEL			  0x07
#define	 WR0_CMD			  0x38
#define	 WR0_CMD_NULL		  0x00
#define	 WR0_CMD_NIL		  0x08
#define	 WR0_CMD_RST_STI	  0x10
#define	 WR0_CMD_RESET		  0x18
#define	 WR0_CMD_INT_NEXT	  0x20
#define	 WR0_CMD_RST_TXI	  0x28
#define	 WR0_CMD_RST_ERR	  0x30
#define	 WR0_CMD_RETI		  0x38

#define	 WR1_EXT_INT		  0x01
#define	 WR1_TX_INT			  0x02
#define	 WR1_STA_VEC		  0x04
#define	 WR1_RX_INT			  0x18
#define	 WR1_RXI_NONE		  0x00
#define	 WR1_RXI_FST		  0x80
#define	 WR1_RXI_ALL		  0x10
#define	 WR1_RXI_ALL_PAR	  0x10
#define	 WR1_RXI_ALL_NP		  0x18
#define	 WR1_WR_ON_RT		  0x20
#define	 WR1_WR_FUNC		  0x40
#define	 WR1_WR_ENABLE		  0x80

#define	 WR3_RX_ENABLE		  0x01
#define	 WR3_AUTO_ENABLE	  0x20
#define	 WR3_RX_SIZE		  0xc0
#define	 WR3_RX_5BIT		  0x00
#define	 WR3_RX_7BIT		  0x40
#define	 WR3_RX_6BIT		  0x80
#define	 WR3_RX_8BIT		  0xC0

#define	 WR4_PARITY_ENABLE	  0x01
#define	 WR4_PARITY_EVEN	  0x02
#define	 WR4_STOP_BITS		  0x0C
#define	 WR4_STOP_NIL		  0x00
#define	 WR4_STOP_1_BIT		  0x04
#define	 WR4_STOP_1P5_BIT	  0x08
#define	 WR4_STOP_2_BIT		  0x0C
#define	 WR4_CLOCK			  0xC0
#define	 WR4_CLOCK_X1		  0x00
#define	 WR4_CLOCK_X16		  0x40
#define	 WR4_CLOCK_X32		  0x80
#define	 WR4_CLOCK_X64		  0xC0

#define	 WR5_RTS			  0x02
#define	 WR5_TX_ENABLE		  0x80
#define	 WR5_SEND_BREAK		  0x10
#define	 WR5_TX_SIZE		  0x60
#define	 WR5_TX_5BIT		  0x00
#define	 WR5_TX_7BIT		  0x20
#define	 WR5_TX_6BIT		  0x40
#define	 WR5_TX_8BIT		  0x60
#define	 WR5_DTR			  0x80

// Interrupts (highest to lowest priority)

#define	 INT_SPECIAL		  0
#define	 INT_RX_AVAIL		  1
#define	 INT_TX_EMPTY		  2
#define	 INT_EXT_CHG		  3
#define	 NUM_INT			  4
#define	 INT_FLAG			  ( 1 << ( NUM_CH * NUM_INT - 1 ) )
#define	 CH0_FLAG			  INT_FLAG
#define	 CH0_SPECIAL		  ( CH0_FLAG >> INT_SPECIAL )
#define	 CH0_RX_AVAIL		  ( CH0_FLAG >> INT_RX_AVAIL )
#define	 CH0_TX_EMPTY		  ( CH0_FLAG >> INT_TX_EMPTY )
#define	 CH0_EXT_CHG		  ( CH0_FLAG >> INT_EXT_CHG )
#define	 CH1_FLAG			  ( INT_FLAG >> NUM_INT )
#define	 CH1_SPECIAL		  ( CH1_FLAG >> INT_SPECIAL )
#define	 CH1_RX_AVAIL		  ( CH1_FLAG >> INT_RX_AVAIL )
#define	 CH1_TX_EMPTY		  ( CH1_FLAG >> INT_TX_EMPTY )
#define	 CH1_EXT_CHG		  ( CH1_FLAG >> INT_EXT_CHG )

static	 byte  ivec = 0;		  // Interrupt vector
static	 int   iflags = 0;		  // Interrupt flags
static	 int   ius = 0;		  // Interrupt under service flags
static	 int   iflags_old = -1;
static	 int   ius_old = -1;
static struct
	{
	struct termios  config;	  // For configuration of Linux hardware serial port
	int		fdIn;			  // Input file descriptor
	int		fdOut;			  // Output file descriptor
	int		reg;			  // Control register address
	BOOLEAN	rxint;			  // True if next received character generates an interrupt
	BOOLEAN hw;				  // True if Linux serial port hardware
	byte	wr1;			  // Channel write registers
	byte	wr3;
	byte	wr4;
	byte	wr5;
	byte	rr0;			  // Channel read registers
	byte	rr1;
	byte	rx;				  // Received data
	byte	mlst;			  // Last status of modem registers
	BOOLEAN	stchg;			  // Modem status changed flag
	BOOLEAN	nomodem;		  // Indicates hardware does not support modem status.
	byte	txbuf;			  // Transmit character buffer
	BOOLEAN	txfull;			  // Data in tx buffer
	int		txcyc;			  // Number of cycles requred to transmit a character
	int		txtim;			  // Character transmit timer
	} channel[NUM_CH];

static char *psInt[] = { "RX_Special", "RX_Avail", "TX_Empty", "Ext_Change" };

static struct pkt_data
	{
	char sType[20];
	byte data[256];
	struct oop_data
		{
		int nEOP;
		int nNUP;
		byte data;
		} oop[1];
	BOOLEAN bCont;
	int	 nWrap;
	int	 nChr;
	int	 nData;
	int	 nEOP;
	int	 nNUP;
	int	 nErr;
	} pkt_tx, pkt_rx;

static int  nIUS		=  0;
static int	nRETI		=  0;
static int	nInt0_Spec	=  0;
static int	nInt0_RX	=  0;
static int	nInt0_TX	=  0;
static int	nInt0_Chg	=  0;
static int	nCha0_Rd    =  0;
static int  nCha0_Wr    =  0;

static void pkt_init (struct pkt_data *pkt, char *psType)
	{
	memset (pkt, 0, sizeof (struct pkt_data));
	strncpy (pkt->sType, psType, sizeof (pkt->sType));
	pkt->sType[sizeof (pkt->sType)-1] = '\0';
	}

static void pkt_ints (void)
	{
	ctc_stats ();
	diag_message (DIAG_ALWAYS, "Channel 0 Interrupts: %d Special, %d RX, %d TX, %d Status, %d IUS, %d RETI",
		nInt0_Spec, nInt0_RX, nInt0_TX, nInt0_Chg, nIUS, nRETI);
	diag_message (DIAG_ALWAYS, "Channel 0 Data: %d Read, %d Write", nCha0_Rd, nCha0_Wr);
	nIUS	   =  0;
	nRETI	   =  0;
	nInt0_Spec =  0;
	nInt0_RX   =  0;
	nInt0_TX   =  0;
	nInt0_Chg  =  0;
	nCha0_Rd   =  0;
	nCha0_Wr   =  0;
	}

static void pkt_stat (struct pkt_data *pkt)
	{
	int iErr;
	diag_message (DIAG_ALWAYS, "%s %d Bytes, %d <Ctrl+Z>, %d NULL packets, %d Next packet",
		pkt->sType, pkt->nChr, pkt->nEOP, pkt->nNUP, pkt->nData + pkt->nWrap );
	if ( pkt->nErr )
		{
		diag_message (DIAG_ALWAYS, "%s Out of packet data:", pkt->sType);
		for ( iErr = 0; iErr < pkt->nErr; ++iErr )
			{
			byte b = pkt->oop[iErr].data;
			diag_message (DIAG_ALWAYS, "\t0x%02x \"%c\" after %d <Ctrl+Z>, %d NULL packets",
				b, (( b >= 0x20 ) && ( b < 0x7f )) ? b : '.', pkt->oop[iErr].nEOP, pkt->oop[iErr].nNUP);
			}
		}
	pkt->nChr = 0;
	pkt->nEOP = 0;
	pkt->nNUP = 0;
	pkt->nErr = 0;
	}

static void pkt_log (struct pkt_data *pkt, struct pkt_data *pkt2, byte ch)
	{
	++pkt->nChr;
	if ( pkt->nWrap )
		{
		if ( ( ch == 0x1A ) || ( pkt->nData >= 256 ) )
			{
			/* End of packet */
			if ( ( pkt->nData == 1 ) && ( pkt->data[0] == '5' ) && ( ! pkt->bCont ) )
				{
				++pkt->nNUP;
				pkt->nWrap = 0;
				pkt->nData = 0;
				return;
				}
			byte sLine[256];
			int nLine = ( pkt->nData - 1 ) / 64 + 1;
			int iLine;
			if ( ch == 0x1A ) pkt->nWrap = 2;
			pkt_ints ();
			if ( pkt2 ) pkt_stat (pkt2);
			pkt_stat (pkt);
			diag_message (DIAG_ALWAYS, "%s %s Packet:",	pkt->sType, pkt->bCont ? "Continuation" : "Ring");
			for ( iLine = 0; iLine < nLine; ++iLine )
				{
				int iCh;
				int	 iTxt = 0;
				int nCh1 = 64 * iLine + 1;
				int nCh2 = nCh1 + 64;
				if ( iLine == 0 ) nCh1 = 0;
				if ( nCh2 > pkt->nData ) nCh2 = pkt->nData;
				for ( iCh = nCh1; iCh < nCh2; ++iCh )
					{
					byte ch2 = pkt->data[iCh];
					if ( ch2 == '\\' )
						{
						sLine[iTxt++] = '\\';
						sLine[iTxt++] = '\\';
						}
					else if ( ( ch2 < 0x20 ) || ( ch2 > 0x7E ) )
						{
						sLine[iTxt++] = '\\';
						sprintf (&sLine[iTxt], "%02X", ch2);
						iTxt += 2;
						}
					else
						{
						sLine[iTxt++] = ch2;
						}
					}
				sLine[iTxt] = '\0';
				if ( iLine == 0 ) diag_message (DIAG_ALWAYS, "\t%s", (char *) sLine);
				else diag_message (DIAG_ALWAYS, "\t %s", (char *) sLine);
				if ( ( pkt->data[0] == '9' ) || ( pkt->data[0] == 'A' ) )
					{
					iCh = ( iLine == 0 ) ? 1 : nCh1;
					iTxt = 0;
					while ( iCh < nCh2 )
						{
						byte  ch2	=  pkt->data[iCh++];
						byte  ch3	=  0;
						BOOLEAN	 bErr = FALSE;
						if ( ( ch2 >= '0' ) && ( ch2 <= '9' ) ) ch3	= ch2 - '0';
						else if ( ( ch2 >= 'A' ) && ( ch2 <= 'F' ) ) ch3 = ch2 - 'A' + 10;
						else bErr = TRUE;
						ch3 <<= 4;
						ch2	  =	 pkt->data[iCh++];
						if ( ( ch2 >= '0' ) && ( ch2 <= '9' ) ) ch3	+= ch2 - '0';
						else if ( ( ch2 >= 'A' ) && ( ch2 <= 'F' ) ) ch3 += ch2 - 'A' + 10;
						else bErr = TRUE;
						if ( bErr ) ch3 = '?';
						else if ( ( ch3 < 0x20 ) || ( ch3 > 0x7e ) ) ch3 = '.';
						sLine[iTxt++] = ' ';
						sLine[iTxt++] = ch3;
						}
					sLine[iTxt] = '\0';
					diag_message (DIAG_ALWAYS, "\t %s", (char *) sLine);
					}
				}
			pkt->nChr = 0;
			pkt->nData = 0;
			pkt->nEOP = 0;
			pkt->nNUP = 0;
			pkt->nErr = 0;
			if ( ch == 0x1A )
				{
				pkt->nWrap = 0;
				pkt->bCont = FALSE;
				return;
				}
			pkt->bCont = TRUE;
			}
		pkt->data[pkt->nData++] = ch;
		}
	else
		{
		if ( ch == 0x02 )  pkt->nWrap = 1;
		else if ( ch == 0x1A ) ++pkt->nEOP;
		else
			{
			pkt->oop[pkt->nErr].data = ch;
			pkt->oop[pkt->nErr].nEOP = pkt->nEOP;
			pkt->oop[pkt->nErr].nNUP = pkt->nNUP;
			++pkt->nErr;
			if ( pkt->nErr >= sizeof (pkt->oop) / sizeof (pkt->oop[0]) ) pkt_stat (pkt);
			}
		}
	}

static void dart_config (int ch)
	{
	diag_message (DIAG_DART_HW, "Calling tcsetattr on channel %d", ch);
	if ( tcsetattr (channel[ch].fdIn, TCSADRAIN, &channel[ch].config) < 0)
		fatal ("Error %d when setting serial configuration on DART channel %d.",
			errno, ch);
	diag_message (DIAG_DART_HW, "Returned from tcsetattr on channel %d", ch);
	}

static void dart_baud (int ch)
	{
	struct s_rate
		{
		int	freq;
		speed_t	baud;
		}  rates[] = {
                {50, B50},
				{75, B75},
				{110, B110},
				{134, B134},
				{150, B150},
				{200, B200},
				{300, B300},
				{600, B600},
				{1200, B1200},
				{1800, B1800},
				{2400, B2400},
				{4800, B4800},
				{9600, B9600},
				{19200, B19200},
				{38400, B38400},
				{57600, B57600},
				{115200, B115200},
				{230400 , B230400 }};
	int	 i;
	int	 fset = -1;
	int	 nbits =  0;
	speed_t	 baud;
	double	 freq  =  ctc_freq (ch + 1);
	diag_message (DIAG_DART_HW, "CTC Channel %d frequency = %d Hz.", ch + 1, freq);
	if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_5BIT )		  nbits = 5;
	else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_7BIT )  nbits = 7;
	else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_6BIT )  nbits = 6;
	else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_8BIT )  nbits = 8;
	if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_NIL )			nbits += 1;
	else if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_1_BIT )	nbits += 1;
	else if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_1P5_BIT )	nbits += 2;
	else if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_2_BIT )	nbits += 2;
	if ( ( channel[ch].wr4 & WR4_CLOCK ) == WR4_CLOCK_X16 )		  freq	 /=	16;
	else if ( ( channel[ch].wr4 & WR4_CLOCK ) == WR4_CLOCK_X32 )  freq	 /=	32;
	else if ( ( channel[ch].wr4 & WR4_CLOCK ) == WR4_CLOCK_X64 )  freq	 /=	64;
	diag_message (DIAG_DART_HW, "Requested channel %d baud rate = %f", ch, freq);
	channel[ch].txcyc = 4000000 * nbits / freq;
	if ( channel[ch].hw )
		{
		for ( i = 0; i < sizeof (rates) / sizeof (rates[0]); ++i )
			{
			if ( rates[i].freq <= freq )
				{
				fset  =	rates[i].freq;
				baud  =	rates[i].baud;
				}
			else   break;
			}
		if ( fset > 0 )
			{
			if ( cfsetspeed (&channel[ch].config, baud) != 0 )
				fatal ("Error %d setting baud rate %d on DART channel %d.", errno, freq, ch);
			diag_message (DIAG_DART_HW, "DART Channel %d: Baud rate set to %d bps", ch, fset);
			}
		if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_2_BIT )
			channel[ch].config.c_cflag |= CSTOPB;
		else
			channel[ch].config.c_cflag &= ~CSTOPB;
		}
	}

static void set_int (int ch, int type)
	{
	iflags	|= ( INT_FLAG >> ( 4 * ch + type ) );
	channel[0].rr0	 |=	RR0_INT_PENDING;
	diag_message (DIAG_DART_CFG, "DART Channel %d: Set interrupt %s: iflags = 0x%02x",
		ch, psInt[type], iflags);
	iflags_old = -1;
	}

static void clear_int (int ch, int type)
	{
	iflags	&= ~ ( INT_FLAG >> ( NUM_INT * ch + type ) );
	diag_message (DIAG_DART_CFG, "DART Channel %d: Clear interrupt %s: iflags = 0x%02x",
		ch, psInt[type], iflags);
	if ( iflags == 0 )
		{
		channel[0].rr0 &= ~RR0_INT_PENDING;
		}
	iflags_old = -1;
	}

static void clear_ius (int ch, int type)
	{
	int	 iact = ( INT_FLAG >> ( NUM_INT * ch + type ) );
	if ( ius & iact )
		{
		ius	&= ~iact;
		diag_message (DIAG_DART_CFG,
			"DART Channel %d: Clear interrupt under service %s: iflags = 0x%02x, ius = 0x%02x",
			ch, psInt[type], iflags, ius);
		}
	ius_old = -1;
	}

static int high_bit (int value)
	{
	int	 hbit  =  INT_FLAG;
	while ( hbit )
		{
		if ( hbit & value )	  break;
		hbit >>= 1;
		}
	return	hbit;
	}

static void channel_reset (int ch)
	{
	channel[ch].reg		=	0;
	channel[ch].wr1		=	0;
	channel[ch].wr3		=	0;
	channel[ch].wr4		=	0;
	channel[ch].wr5		=	0;
	channel[ch].rr0		=	RR0_TX_EMPTY;
	channel[ch].rr1		=	RR1_ALL_SENT;
	channel[ch].stchg	=	FALSE;
	channel[ch].txfull	=	FALSE;
	channel[ch].rxint	=	FALSE;
    channel[ch].txcyc   =   0;
	channel[ch].txtim   =   0;
	clear_int (ch, INT_SPECIAL);
	clear_int (ch, INT_RX_AVAIL);
	clear_int (ch, INT_TX_EMPTY);
	clear_int (ch, INT_EXT_CHG);
	ius	 &=	~ ( ( CH0_SPECIAL | CH0_RX_AVAIL | CH0_TX_EMPTY | CH0_EXT_CHG ) >> ( NUM_INT * ch ) );
	if ( channel[ch].hw )
		{
		if ( tcflush (channel[ch].fdIn, TCIOFLUSH) == -1 )
			fatal ("Error %d resetting DART channel %d.", errno, ch);
		}
	}

static int dart_get_vector (void)
	{
	if ( channel[1].wr1 & WR1_STA_VEC )
		{
		if ( iflags & CH0_SPECIAL )		return	( ivec & 0xf1 ) | 0x0E;
		else if ( iflags & CH0_RX_AVAIL )	return	( ivec & 0xf1 ) | 0x0C;
		else if ( iflags & CH0_TX_EMPTY )	return	( ivec & 0xf1 ) | 0x08;
		else if ( iflags & CH0_EXT_CHG )	return	( ivec & 0xf1 ) | 0x0A;
		else if ( iflags & CH1_SPECIAL )	return	( ivec & 0xf1 ) | 0x06;
		else if ( iflags & CH1_RX_AVAIL )	return	( ivec & 0xf1 ) | 0x04;
		else if ( iflags & CH1_TX_EMPTY )	return	( ivec & 0xf1 ) | 0x00;
		else if ( iflags & CH1_EXT_CHG )	return	( ivec & 0xf1 ) | 0x02;
		return	( ivec & 0xf1 ) | 0x06;
		}
	return	ivec;
	}

static BOOLEAN check_parity (int ch, byte value)
	{
	int	 npar = 0;
	int	 i;
	if ( ! ( channel[ch].wr4 & WR4_PARITY_ENABLE ) )  return	TRUE;
	if ( ! ( channel[ch].wr4 & WR4_PARITY_EVEN ) )	  npar	=  1;
	for ( i = 0; i < 8; ++i )
		{
		if ( value & 1 )   ++npar;
		value  >>=	 1;
		}
	return	( npar & 1 ) ? FALSE : TRUE;
	}

static void dart_rx (int ch, byte value)
	{
	diag_message (DIAG_DART_DATA, "DART Channel %d: Received character = 0x%02x", ch, value);
	if ( ( ch == 0 ) && ( diag_flags[DIAG_NODE_PKT_RX] ) )
		pkt_log (&pkt_rx, diag_flags[DIAG_NODE_PKT_TX] ? &pkt_tx : NULL, value);
	channel[ch].rx	 =	value;
	channel[ch].rr0	 |=	RR0_RX_AVAILABLE;
	// if ( ! check_parity (ch, value) )  channel[ch].rr1	 |=	RR1_PARITY_ERROR;
	if ( (channel[ch].wr1 & WR1_RX_INT) == WR1_RXI_FST )
		{
		if ( channel[ch].rxint )
			{
			set_int (ch, INT_RX_AVAIL);
			channel[ch].rxint  =  FALSE;
			}
		}
	else if ( ( channel[ch].wr1 & WR1_RX_INT ) == WR1_RXI_ALL_PAR )
		{
		if ( channel[ch].rr1 & RR1_PARITY_ERROR )  set_int (ch, INT_SPECIAL);
		else   set_int (ch, INT_RX_AVAIL);
		}
	else if ( ( channel[ch].wr1 & WR1_RX_INT ) == WR1_RXI_ALL_NP )
		{
		set_int (ch, INT_RX_AVAIL);
		}
	}

static void dart_pump (int cycles)
	{
	int	 ch;
	int	 status;
	byte value;
	for ( ch = 0; ch < 2; ++ch )
		{
		// diag_message (DIAG_DART_CFG, "Entered dart_pump for channel %d: fdIn = %d, fdOut = %d",
		// 	ch, channel[ch].fdIn, channel[ch].fdOut);
		if ( ( channel[ch].wr3 & WR3_RX_ENABLE )
			&& ( ! ( channel[ch].rr0 & RR0_RX_AVAILABLE ) )
			&& ( channel[ch].fdIn >= 0 ) )
			{
			// diag_message (DIAG_DART_HW,"Attempt to read a character");
			status	 =	read (channel[ch].fdIn, &value, 1);
			// diag_message (DIAG_DART_HW,"status = %d   errno = %d", status, errno);
			if ( status == 1 )
				{
				dart_rx (ch, value);
				}
			else if ( ( status == -1 ) && ( errno != EAGAIN ) && ( errno != EWOULDBLOCK ) )
				{
				fatal ("Read error %d on DART Channel %d", errno, ch);
				}
			}
		if ( ( channel[ch].wr5 & WR5_TX_ENABLE )
			&& ( ! ( channel[ch].rr1 & RR1_ALL_SENT ) )
			&& ( channel[ch].fdOut >= 0 ) )
			{
			channel[ch].txtim -= cycles;
			if ( ( channel[ch].txtim <= 0 ) && ( channel[ch].txfull ) )
				{
				diag_message (DIAG_DART_HW, "Transmit character 0x%02x", channel[ch].txbuf);
				write (channel[ch].fdOut, &channel[ch].txbuf, 1);
				channel[ch].txfull = FALSE;
				channel[ch].txtim = channel[ch].txcyc;
				channel[ch].rr0	|= RR0_TX_EMPTY;
				if ( channel[ch].wr1 & WR1_TX_INT )  set_int (ch, INT_TX_EMPTY);
				}
			if ( ( channel[ch].txtim <= 0 ) && ( ! ( channel[ch].rr1 & RR1_ALL_SENT ) ) )
				{
				if ( channel[ch].hw )
					{
					int	 ntxq;
					diag_message (DIAG_DART_HW, "Query transmit queue");
					if ( ioctl (channel[ch].fdIn, TIOCOUTQ, &ntxq) == -1 )
						fatal ("Error %d getting transmit status on DART channel %d.", errno, ch);
					diag_message (DIAG_DART_HW, "ntxq = %d", ntxq);
					if ( ntxq == 0 )
						{
						channel[ch].rr1	|= RR1_ALL_SENT;
						channel[ch].txtim = 0;
						}
					}
				else
					{
					channel[ch].rr1	|= RR1_ALL_SENT;
					channel[ch].txtim = 0;
					}
				}
			}
		if ( channel[ch].hw )
			{
			if ( ( ! channel[ch].nomodem ) && ( ! channel[ch].stchg ) )
				{
				int	 mflags = 0;
				// diag_message (DIAG_DART_HW, "Get modem status");
				if ( ioctl (channel[ch].fdIn, TIOCMGET, &mflags) == -1 )
					{
					if ( errno == EINVAL )
						{
						diag_message (DIAG_DART_HW,
							"Serial device on channel %d does not support modem status",
							ch);
						channel[ch].nomodem = TRUE;
						}
					else
						{
						fatal ("Error %d getting modem status on DART channel %d.", errno, ch);
						}
					}
				// diag_message (DIAG_DART_HW, "mflags = 0x%08x", mflags);
				byte  mcur	=  0;
				if ( mflags & TIOCM_CAR ) mcur	|= RR0_DCD;
				if ( mflags & TIOCM_RNG ) mcur	|= RR0_RI;
				if ( mflags & TIOCM_RNG ) mcur	|= RR0_CTS;
				if ( mcur != channel[ch].mlst )
					{
					channel[ch].stchg  =  TRUE;
					channel[ch].mlst   =  mcur;
					channel[ch].rr0	&=	~RR0_MODEM;
					channel[ch].rr0	|=	mcur;
					if ( channel[ch].wr1 & WR1_EXT_INT )	set_int (ch, INT_EXT_CHG);
					if ( channel[ch].wr3 & WR3_AUTO_ENABLE )
						{
						if ( mcur & RR0_DCD )	channel[ch].wr3	  |= WR3_RX_ENABLE;
						if ( mcur & RR0_CTS )	channel[ch].wr5	  |= WR5_TX_ENABLE;
						}
					}
				}
			}
		}
	// diag_message (DIAG_DART_HW, "Exit dart_pump");
	}

BOOLEAN dart_reti (void)
	{
	if ( ius )
		{
		int iack = high_bit (ius);
		++nRETI;
		ius	&= ~ iack;
		diag_message (DIAG_DART_CFG,
			"Interrupt under service 0x%02x cleared by RETI: iflags = 0x%02x, ius = 0x%02x",
			iack, iflags, ius);
		return TRUE;
		}
	return	FALSE;
	}

void dart_out (word port, byte value)
	{
	int	 ch	=  port & 0x01;
	diag_message (DIAG_DART_PORTS, "Write to DART port 0x%02x: 0x%02x", port, value);
	if ( port & 0x02 )
		{
		switch ( channel[ch].reg )
			{
			case  0:
			{
			channel[ch].reg	  =	 value & WR0_REGSEL;
			diag_message (DIAG_DART_CFG, "DART Channel %d WR0: Select register %d", ch, channel[ch]);
			switch ( value & WR0_CMD )
				{
				case WR0_CMD_NULL:
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d Command: Null Code", ch);
				break;
				}
				case WR0_CMD_NIL:
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d Command: Not used", ch);
				break;
				}
				case WR0_CMD_RST_STI:
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d Command: Reset Ext/Status Interrupts", ch);
				channel[ch].stchg	=  FALSE;
				clear_int (ch, INT_SPECIAL);
				break;
				}
				case WR0_CMD_RESET:
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d Command: Channel reset", ch);
				channel_reset (ch);
				break;
				}
				case WR0_CMD_INT_NEXT:
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d Command: Enable Int on next Rx character", ch);
				channel[ch].rxint	=  TRUE;
				break;
				}
				case WR0_CMD_RST_TXI:
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d Command: Reset Tx Int Pending", ch);
				clear_int (ch, INT_TX_EMPTY);
				break;
				}
				case WR0_CMD_RST_ERR:
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d Command: Error Reset", ch);
				channel[ch].rr1	 &=	~ ( RR1_PARITY_ERROR | RR1_RX_OVERRUN | RR1_FRAMING_ERROR );
				break;
				}
				case WR0_CMD_RETI:
				{
				if ( ch == 0 )
					{
					diag_message (DIAG_DART_CFG, "DART Channel %d Command: Return from Int", ch);
					dart_reti ();
					}
				else
					{
					diag_message (DIAG_DART_CFG, "DART Channel %d Command: Return from Int"
						" - Ignored on channel B", ch);
					}
				break;
				}
				}
			break;
			}
			case  1:
			{
			channel[ch].wr1	  =	 value;
			if ( channel[ch].wr1 & WR1_EXT_INT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Ext Int Enable", ch);
			if ( channel[ch].wr1 & WR1_TX_INT )
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Tx Int Enable", ch);
				}
			if ( channel[ch].wr1 & WR1_WR_ON_RT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Wait/Ready on R/T", ch);
			if ( channel[ch].wr1 & WR1_WR_FUNC )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Wait/Ready Function", ch);
			if ( channel[ch].wr1 & WR1_WR_ENABLE )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Wait/Ready Enable", ch);
			if ( ( channel[ch].wr1 & WR1_RX_INT ) == WR1_RXI_NONE )
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Rx Int Disable", ch);
				}
			else if ( ( channel[ch].wr1 & WR1_RX_INT ) == WR1_RXI_FST )
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Rx Int on first character", ch);
				}
			else if ( ( channel[ch].wr1 & WR1_RX_INT ) == WR1_RXI_ALL_PAR )
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Int on all Rx characters (parity affects vector)", ch);
				}
			else if ( ( channel[ch].wr1 & WR1_RX_INT ) == WR1_RXI_ALL_NP )
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d WR1: Int on all Rx characters (parity does not affect vector)", ch);
				}
			channel[ch].reg	  =	 0;
			break;
			}
			case  2:
			{
			if ( ch == 1 )
				{
				ivec  =	 value;
				diag_message (DIAG_DART_CFG, "DART Channel %d WR2: Interrupt vector = 0x%02x", ch, ivec);
				}
			else
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d WR2: Interrupt vector = 0x%02x"
					" - Ignored on channel A", ch, ivec);
				}
			channel[ch].reg	  =	 0;
			break;
			}
			case  3:
			{
			channel[ch].wr3	  =	 value;
			if ( channel[ch].wr3 & WR3_RX_ENABLE )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR3: Rx Enable", ch);
			if ( channel[ch].wr3 & WR3_AUTO_ENABLE )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR3: Auto enables", ch);
			if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_5BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR3: Rx 5 bits/character", ch);
			else if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_7BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR3: Rx 7 bits/character", ch);
			else if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_6BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR3: Rx 6 bits/character", ch);
			else if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_8BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR3: Rx 8 bits/character", ch);
			if ( channel[ch].hw )
				{
				channel[ch].config.c_cflag	 &=	~ CSIZE;
				if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_5BIT )
					channel[ch].config.c_cflag	  |= CS5;
				else if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_7BIT )
					channel[ch].config.c_cflag	  |= CS7;
				else if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_6BIT )
					channel[ch].config.c_cflag	  |= CS6;
				else if ( ( channel[ch].wr3 & WR3_RX_SIZE ) == WR3_RX_8BIT )
					channel[ch].config.c_cflag	  |= CS8;
				dart_config (ch);
				}
			channel[ch].reg	  =	 0;
			break;
			}
			case  4:
			{
			channel[ch].wr4	  =	 value;
			if ( channel[ch].wr4 & WR4_PARITY_ENABLE )
				{
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: Parity Enable", ch);
				if ( channel[ch].wr4 & WR4_PARITY_EVEN )
					diag_message (DIAG_DART_CFG, "DART Channel %d WR4: Parity Even", ch);
				else
					diag_message (DIAG_DART_CFG, "DART Channel %d WR4: Parity Odd", ch);
				}
			if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_NIL )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: Invalid stop bits", ch);
			else if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_1_BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: 1 stop bit/character", ch);
			else if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_1P5_BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: 1 1/2 stop bits/character", ch);
			else if ( ( channel[ch].wr4 & WR4_STOP_BITS ) == WR4_STOP_2_BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: 2 stop bits/character", ch);
			if ( ( channel[ch].wr4 & WR4_CLOCK ) == WR4_CLOCK_X1 )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: x1 clock mode", ch);
			else if ( ( channel[ch].wr4 & WR4_CLOCK ) == WR4_CLOCK_X16 )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: x16 clock mode", ch);
			else if ( ( channel[ch].wr4 & WR4_CLOCK ) == WR4_CLOCK_X32 )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: x32 clock mode", ch);
			else if ( ( channel[ch].wr4 & WR4_CLOCK ) == WR4_CLOCK_X64 )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR4: x64 clock mode", ch);
			dart_baud (ch);
			if ( channel[ch].hw )	dart_config (ch);
			channel[ch].reg	  =	 0;
			break;
			}
			case  5:
			{
			channel[ch].wr5	  =	 value;
			if ( channel[ch].wr5 & WR5_RTS )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR5: RTS", ch);
			if ( channel[ch].wr5 & WR5_TX_ENABLE )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR5: Tx Enable", ch);
			if ( channel[ch].wr5 & WR5_SEND_BREAK )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR5: Send Break", ch);
			if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_5BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR5: Tx 5 bits/character", ch);
			else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_7BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR5: Tx 7 bits/character", ch);
			else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_6BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR5: Tx 6 bits/character", ch);
			else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_8BIT )
				diag_message (DIAG_DART_CFG, "DART Channel %d WR5: Tx 8 bits/character", ch);
			if ( channel[ch].wr5 & WR5_DTR ) diag_message (DIAG_DART_CFG, "DART Channel %d WR5: DTR", ch);
			dart_baud (ch);
			if ( channel[ch].hw )
				{
				int	 iset  =  0;
				int	 iclr  =  0;
				if ( channel[ch].wr5 & WR5_RTS )   iset  |= TIOCM_RTS;
				else							   iclr  |= TIOCM_RTS;
				if ( channel[ch].wr5 & WR5_DTR )   iset  |= TIOCM_DTR;
				else							   iclr  |= TIOCM_DTR;
				channel[ch].config.c_cflag	 &=	~ CSIZE;
				if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_5BIT )
					channel[ch].config.c_cflag	  |= CS5;
				else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_7BIT )
					channel[ch].config.c_cflag	  |= CS7;
				else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_6BIT )
					channel[ch].config.c_cflag	  |= CS6;
				else if ( ( channel[ch].wr5 & WR5_TX_SIZE ) == WR5_TX_8BIT )
					channel[ch].config.c_cflag	  |= CS8;
				dart_config (ch);
				if ( ! channel[ch].nomodem )
					{
					if ( iset )
						{
						if ( ioctl (channel[ch].fdIn, TIOCMBIS, &iset) == -1 )
							{
							if ( errno == EINVAL )
								{
								diag_message (DIAG_DART_HW,
									"Serial device on channel %d does not support modem status",
									ch);
								channel[ch].nomodem = TRUE;
								}
							else
								{
								fatal ("Error %d setting modem signals on DART channel %d.", errno, ch);
								}
							}
						}
					if ( iclr )
						{
						if ( ioctl (channel[ch].fdIn, TIOCMBIC, &iset) == -1 )
							{
							if ( errno == EINVAL )
								{
								diag_message (DIAG_DART_HW,
									"Serial device on channel %d does not support modem status",
									ch);
								channel[ch].nomodem = TRUE;
								}
							else
								{
								fatal ("Error %d clearing modem signals on DART channel %d.", errno, ch);
								}
							}
						}
					}
				}
			channel[ch].reg	  =	 0;
			break;
			}
			default:
			{
			fatal ("Write to invalid DART register");
			break;
			}
			}
		}
	else
		{
		if ( channel[ch].wr5 & WR5_TX_ENABLE )
			{
			diag_message (DIAG_DART_DATA, "DART Channel %d: Transmit 0x%02x", ch, value);
			if ( ( ch == 0 ) && ( diag_flags[DIAG_NODE_PKT_TX] ) )
				pkt_log (&pkt_tx, diag_flags[DIAG_NODE_PKT_RX] ? &pkt_rx : NULL, value);
			if ( channel[ch].txfull ) fatal ("Transmit overrun on DART channel %d", ch);
			channel[ch].txfull	 =	TRUE;
			channel[ch].txbuf	 =	value;
			channel[ch].rr0	&= ~RR0_TX_EMPTY;
			channel[ch].rr1	&= ~RR1_ALL_SENT;
			clear_int (ch, INT_TX_EMPTY);
			if ( ch == 0 ) ++nCha0_Wr;
			}
		else
			{
			diag_message (DIAG_DART_DATA, "DART Channel %d: Transmit 0x%02x - Transmitter not enabled",
				ch, value);
			}
		}
	dart_pump (0);
	}

byte dart_in (word port)
	{
	int	ch	=  port & 0x01;
	byte value =  0;
	diag_message (DIAG_DART_PORTS, "Read from DART port 0x%02x", port);
	dart_pump (0);
	if ( port & 0x02 )
		{
		switch ( channel[ch].reg )
			{
			case  0:
			{
			value =	 channel[ch].rr0;
			if ( value & RR0_RX_AVAILABLE )	 diag_message (DIAG_DART_CFG, "DART Channel %d RR0: Rx Character Available", ch);
			if ( value & RR0_INT_PENDING )	 diag_message (DIAG_DART_CFG, "DART Channel %d RR0: Interrupt Pending", ch);
			if ( value & RR0_TX_EMPTY )		 diag_message (DIAG_DART_CFG, "DART Channel %d RR0: Transmit Empty", ch);
			if ( value & RR0_DCD )			 diag_message (DIAG_DART_CFG, "DART Channel %d RR0: Data Carrier Detect", ch);
			if ( value & RR0_RI )		  	 diag_message (DIAG_DART_CFG, "DART Channel %d RR0: Ring Indicator", ch);
			if ( value & RR0_CTS )			 diag_message (DIAG_DART_CFG, "DART Channel %d RR0: Clear to Send", ch);
			if ( value & RR0_BREAK )		 diag_message (DIAG_DART_CFG, "DART Channel %d RR0: Break", ch);
			break;
			}
			case  1:
			{
			value =	 channel[ch].rr1;
			if ( value & RR1_ALL_SENT )		 diag_message (DIAG_DART_CFG, "DART Channel %d RR1: All Sent", ch);
			if ( value & RR1_PARITY_ERROR )	 diag_message (DIAG_DART_CFG, "DART Channel %d RR1: Parity Error", ch);
			if ( value & RR1_RX_OVERRUN )	 diag_message (DIAG_DART_CFG, "DART Channel %d RR1: RX Overrun", ch);
			if ( value & RR1_FRAMING_ERROR ) diag_message (DIAG_DART_CFG, "DART Channel %d RR1: Framing Error", ch);
			break;
			}
			case  2:
			{
			if ( ch == 1 )
				{
				value =	 dart_get_vector ();
				diag_message (DIAG_DART_CFG, "DART Channel %d RR2: Interrupt Vector = 0x%02x", ch, value);
				}
			else
				{
				value	=  0;
				diag_message (DIAG_DART_CFG, "DART Channel %d RR2: Not implemented", ch);
				}
			break;
			}
			default:
			{
			fatal ("Read from invalid DART register");
			break;
			}
			}
		}
	else
		{
		value  =  channel[ch].rx;
		channel[ch].rr0	&= ~RR0_RX_AVAILABLE;
		clear_int (ch, INT_RX_AVAIL);
		clear_int (ch, INT_SPECIAL);
		diag_message (DIAG_DART_DATA, "DART Channel %d: Read data = 0x%02x", ch, value);
		if ( ch == 0 )	++nCha0_Rd;
		}
	return	 value;
	}

int dart_int_pending (int cycles)
	{
	int	 ipend =  -1;
	// diag_message (DIAG_DART_CFG, "Entered dart_int_pending");
	dart_pump (cycles);
	if ( ( ius ) && ( high_bit (iflags) <= ius ) )
		{
		++nIUS;
		if ( ( iflags != iflags_old ) || ( ius != ius_old ) )
			{
			diag_message (DIAG_DART_CFG,
				"DART Interrupts: Flags = 0x%02x, ius = 0x%02x: Interrupt under service", iflags, ius);
			iflags_old = iflags;
			ius_old = ius;
			}
		return -3;
		}
	if ( iflags )
		{
		int ifact = high_bit (iflags);
		if ( ifact & CH0_SPECIAL )		  ++nInt0_Spec;
		else if ( ifact & CH0_RX_AVAIL ) ++nInt0_RX;
		else if ( ifact & CH0_TX_EMPTY ) ++nInt0_TX;
		else if ( ifact & CH0_EXT_CHG )  ++nInt0_Chg;
		ipend  =  dart_get_vector ();
		ius	   |= ifact;
		diag_message (DIAG_DART_CFG,
			"DART Interrupts: Flags = 0x%02x, ius = 0x%02x, base = 0x%02x, vector = 0x%02x",
			iflags, ius, ivec, ipend);
		}
	iflags_old = -1;
	ius_old = -1;
	return	ipend;
	}

void dart_init (void)
	{
	memset (&channel, 0, sizeof (channel));
	channel[0].fdIn	 =	-1;
	channel[0].fdOut =	-1;
	channel[1].fdIn	 =	-1;
	channel[1].fdOut =	-1;
	channel_reset (0);
	channel_reset (1);
	pkt_init (&pkt_tx, "Transmit");
	pkt_init (&pkt_rx, "Receive");
	}

void dart_read (int ch, const char *psFile)
	{
    psFile = PMapPath (psFile);
	if ( channel[ch].fdIn >= 0 )
		{
		close (channel[ch].fdIn);
		channel[ch].fdIn	=  -1;
		if ( channel[ch].hw ) channel[ch].fdOut	=  -1;
		}
	channel[ch].hw	 =	FALSE;
	if ( ( psFile != NULL ) && ( psFile[0] != '\0' ) )
		{
		channel[ch].fdIn   =  open (psFile, O_RDONLY | O_NONBLOCK);
		if ( channel[ch].fdIn == -1 )
			fatal ("Error %d when opening file \"%s\" for read on DART channel %d.", errno, psFile, ch);
		}
	}

void dart_write (int ch, const char *psFile)
	{
    psFile = PMapPath (psFile);
	if ( channel[ch].fdOut >= 0 )
		{
		close (channel[ch].fdOut);
		channel[ch].fdOut	=  -1;
		if ( channel[ch].hw ) channel[ch].fdIn	=  -1;
		}
	channel[ch].hw	 =	FALSE;
	if ( ( psFile != NULL ) && ( psFile[0] != '\0' ) )
		{
		channel[ch].fdOut   =  creat (psFile, 0777);
		if ( channel[ch].fdOut == -1 )
			fatal ("Error %d when opening file \"%s\" for write on DART channel %d.", errno, psFile, ch);
		}
	}

void dart_term (void)
	{
	if ( diag_flags[DIAG_NODE_PKT_TX] || diag_flags[DIAG_NODE_PKT_RX] )	 pkt_ints ();
	if ( diag_flags[DIAG_NODE_PKT_TX] )	 pkt_stat (&pkt_tx);
	if ( diag_flags[DIAG_NODE_PKT_RX] )	 pkt_stat (&pkt_rx);
	dart_read (0, NULL);
	dart_write (0, NULL);
	dart_read (1, NULL);
	dart_write (1, NULL);
	}

void dart_serial (int ch, const char *psDev)
	{
	dart_read (ch, NULL);
	dart_write (ch, NULL);
	if ( ( psDev != NULL ) && ( psDev[0] != '\0' ) )
		{
		int fd;
		int ios;
		fd	=  open (psDev, O_RDWR | O_NOCTTY | O_NDELAY);
		if ( fd == -1 )
			fatal ("Error %d when opening serial device \"%s\" on DART channel %d.", errno, psDev, ch);
		channel[ch].fdOut  =  fd;
		channel[ch].fdIn   =  fd;
		channel[ch].hw	   =  TRUE;
		if ( !isatty (fd) )
			fatal ("Device \"%s\" on DART channel %d is not a serial device.", psDev, ch);
		diag_message (DIAG_DART_HW, "Opened device \"%s\" on file no %d.", psDev, fd);
		ios = tcgetattr (fd, &channel[ch].config);
		if ( ios < 0 )
			fatal ("Error %d when getting serial device \"%s\" configuration on DART channel %d.",
				errno, psDev, ch);
		diag_message (DIAG_DART_HW, "Got serial configuration. fd = %d", fd);
	    /*
	    Input flags - Turn off input processing
	    convert break to null byte, no CR to NL translation,
	    no NL to CR translation, don't mark parity errors or breaks
	    no input parity check, don't strip high bit off,
	    no XON/XOFF software flow control
	    */
		channel[ch].config.c_iflag &= ~(IGNBRK | BRKINT | ICRNL |
			INLCR | PARMRK | INPCK | ISTRIP | IXON);
        /*
        Output flags - Turn off output processing
        no CR to NL translation, no NL to CR-NL translation,
        no NL to CR translation, no column 0 CR suppression,
        no Ctrl-D suppression, no fill characters, no case mapping,
        no local output processing
        */
		channel[ch].config.c_oflag &= ~(OCRNL | ONLCR | ONLRET |
		ONOCR | OFILL | OLCUC | OPOST);
        /*
        No line processing:
        echo off, echo newline off, canonical mode off, 
        extended input processing off, signal chars off
        */
		channel[ch].config.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
        /*
        Turn off character processing
        clear current char size mask, no parity checking,
        no output processing, force 8 bit input
        */
		channel[ch].config.c_cflag &= ~(CSIZE | PARENB);
		channel[ch].config.c_cflag |= CS8;
        /*
        Return from read() immediately if no characters available
        Inter-character timer off
        */
		channel[ch].config.c_cc[VMIN]  = 0;
		channel[ch].config.c_cc[VTIME] = 0;
        /*
        Finally, apply the configuration
        */
		if ( tcsetattr (fd, TCSAFLUSH, &channel[ch].config) < 0)
			fatal ("Error %d when setting serial device \"%s\" configuration on DART channel %d.",
				errno, psDev, ch);
		diag_message (DIAG_DART_HW, "Set serial configuration for device \"%s\" on file no %d.",
			psDev, fd);
		/* Node packet logging */
		diag_message (DIAG_NODE_PKT_RX, "Log packets received on %s", psDev);
		diag_message (DIAG_NODE_PKT_TX, "Log packets transmitted on %s", psDev);
		}
	}
