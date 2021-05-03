/*

ctc.c - Z80 CTC

The most recent changes here are to respect interrupt priority.
And in addition, to clear interrupts not just at RETI.

*/

/*...sincludes:0:*/
#include "types.h"
#include "diag.h"
#include "common.h"
#include "ctc.h"
#include "memu.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vctc\46\h:0:*/
/*...e*/

// #define DIAG_CHAN   0
#ifdef  DIAG_CHAN
#define IF_CHAN(c)    if ( c == DIAG_CHAN )
#else
#define IF_CHAN(c)
#endif

/*...svars:0:*/
#define	CC_INTERRUPT     0x80	/* Interrupts enabled */
#define	CC_COUNTER_MODE  0x40	/* Counter (not timer) mode */
#define	CC_PRESCALER_256 0x20	/* Timer prescale factor is 256, not 16 */
#define	CC_RISING_EDGE   0x10	/* Rising (not falling edge) */
#define	CC_TIMER_TRIGGER 0x08	/* Timer trigger */
#define	CC_CONSTANT      0x04	/* Load of constant is pending */
#define	CC_RESET         0x02	/* Channel has been reset */
#define	CC_CONTROL       0x01	/* Write control register (not int vector) */

typedef struct
	{
	byte control;
	byte prescaler;
	byte constant;
	byte counter;
	BOOLEAN run;
    enum { isNone, isPending, isRaised, isInService } is;
	} CHANNEL;

#define	N_CHANNELS 4

static CHANNEL ctc_channels[N_CHANNELS];
static byte ctc_int_vector; /* bits 7-3 inclusive */

static int	nInt = 0;
static int  nIUS = 0;
static int	nReti = 0;

void ctc_stats (void)
	{
	diag_message (DIAG_ALWAYS, "CTC: %d Interrupts, %d IUS, %d RETI", nInt, nIUS, nReti);
	nInt = 0;
	nIUS = 0;
	nReti = 0;
	}
/*...e*/

/*...sctc_init:0:*/
void ctc_init(void)
	{
	int i;
	ctc_int_vector = 0x00;
	for ( i = 0; i < N_CHANNELS; i++ )
		{
		CHANNEL *c = &(ctc_channels[i]);
		c->control   = (CC_RESET|CC_CONTROL);
		c->prescaler = 0;
		c->constant  = 0; /* treated as 0x100 */
		c->counter   = 0;
		c->run       = FALSE;
        c->is        = isNone;
		}
	}
/*...e*/

// Raise an interupt on Z80 unless there is already a higher priority one in progress.
void ctc_int (void)
    {
	int i;
	for ( i = 0; i < N_CHANNELS; i++ )
		{
		CHANNEL *c = &(ctc_channels[i]);
        if ( c->is >= isRaised )
            {
            diag_message(DIAG_CTC_INTERRUPT, "CTC interrupt blocked by higher priority for channel %d", i);
            break;
            }
        if ( c->is == isPending )
            {
			IF_CHAN(i)
            diag_message(DIAG_CTC_INTERRUPT, "CTC raised interrupt on CPU for channel %d", i);
            RaiseInt ("CTC");
            c->is = isRaised;
            }
        }
    }

/*...sctc_int_pending:0:*/
// Equivalent to CTC IEO low.
BOOLEAN ctc_int_pending(void)
	{
	int i;
	for ( i = 0; i < N_CHANNELS; i++ )
		{
        if ( ctc_channels[i].is != isNone ) return TRUE;
		}
	return FALSE;
	}
/*...e*/
/*...sctc_int_ack:0:*/
BOOLEAN ctc_int_ack (byte *vector)
	{
	int i;
	for ( i = 0; i < N_CHANNELS; i++ )
		{
		CHANNEL *c = &(ctc_channels[i]);
		if ( c->is == isRaised )
			{
			++nInt;
			c->is = isInService;
			*vector = ctc_int_vector|(i<<1);
			IF_CHAN(i)
            diag_message(DIAG_CTC_INTERRUPT,
                "CTC interrupt acknowledged by CPU on channel %d: vector = 0x%02X",
                i, *vector);
            return TRUE;
			}
		}
	return FALSE;
	}
/*...e*/
/*...sctc_get_int_vector:0:*/
/* This is provided to enable to PANEL hack to work. */
byte ctc_get_int_vector(void)
	{
	return ctc_int_vector;
	}
/*...e*/

// Respond to RETI instruction. Returns true if CTC was being serviced.
BOOLEAN ctc_reti (void)
	{
	int i;
	for ( i = 0; i < N_CHANNELS; i++ )
		{
		CHANNEL *c = &(ctc_channels[i]);
		if ( c->is == isInService )
			{
#ifdef DIAG_CHAN
			if ( ( i == DIAG_CHAN ) || ( ctc_channels[DIAG_CHAN].is == isPending ) )
#endif
            diag_message(DIAG_CTC_INTERRUPT, "CTC isr ended by CPU RETI on channel %d", i);
			++nReti;
			c->is = isNone;
            ctc_int ();
            // if ( i == 2 ) diag_flags[DIAG_Z80_INSTRUCTIONS] = FALSE;
			return TRUE;
			}
		}
	return FALSE;
	}
/*...sctc_in:0:*/
byte ctc_in(int channel)
	{
	byte value = ctc_channels[channel].counter;
	IF_CHAN(channel)
    diag_message(DIAG_CTC_REGISTERS, "CTC counter register for channel %d returned as 0x%02x",
        channel, value);
	return value;
	}
/*...e*/
/*...sctc_out:0:*/
void ctc_out(int channel, byte value)
	{
	CHANNEL *c = &(ctc_channels[channel]);
	if ( c->control & CC_CONSTANT )
		{
		c->control &= ~(CC_CONSTANT|CC_RESET);
		c->constant = value;
			/* 0 is treated as 0x100 */
		c->counter = value;
			/* Note that the Z80 CTC documentation says
			   "This constant is is automatically loaded into the
			    down-counter when the counter/time channel is
			    initialized, and subsequently after each zero count",
			   and on the other hand it appears to contradict with
			   "if updated control and time constant words are
			    written during the count operation, the count
			    continues to zero before the time constant is loaded
			    into the counter",
			   I find that the former gives results matching
			   what I see on the real hardware.
			   Without the fix, some games have to count down 256
			   video frames before the Z80 sees the first
			   interrupt, resulting in a 5s startup delay.
			   For many years, this code used to not set the
			   counter, and the only reason the bug wasn't spotted
			   was another bug in the way video interrupts were
			   handled in memu.c which masked the symptom. */
		IF_CHAN(channel)
        diag_message(DIAG_CTC_REGISTERS, "CTC constant for channel %d set to 0x%02x", channel, value);
		}
	else if ( value & CC_CONTROL )
		{
		c->control = value;
		IF_CHAN(channel)
        diag_message(DIAG_CTC_REGISTERS, "CTC control register for channel %d set to 0x%02x", channel, value);
		if ( value & CC_RESET )
            {
            c->run = FALSE;
            c->prescaler = ( c->control & CC_PRESCALER_256 ) ? 0 : 16; /* Reload prescaler */
            if ( c->is != isNone )
                {
                /* A bit unusual to see this,
                   but there is code out there that doesn't
                   always end interrupt routines with RETI */
                c->is = isNone;
                diag_message(DIAG_CTC_INTERRUPT, "CTC interrupt cleared by reset on channel %d", channel);
                }
			}
		}
	else
		{
		if ( channel == 0 )
			{
			ctc_int_vector = (value & 0xf8);
			diag_message(DIAG_CTC_REGISTERS, "CTC interrupt vector set to 0x%02x", value);
            if ( c->is != isNone )
                {
				/* I have seen code in F1 Simulator
				   that relies of this happening */
                c->is = isNone;
				diag_message(DIAG_CTC_INTERRUPT, "CTC interrupt cleared by writing interrupt vector");
                }
			}
		else
			diag_message(DIAG_CTC_REGISTERS, "CTC unexpected write of 0x%02x to channel %d !!!",
                value, channel);
		}
	}
/*...e*/
/*...sctc_reload:0:*/
void ctc_reload(int channel)
	{
	CHANNEL *c = &(ctc_channels[channel]);
	c->counter = c->constant;
	c->prescaler = ( c->control & CC_PRESCALER_256 ) ? 0 : 16; /* Reload prescaler */
	}
/*...e*/
/*...sctc_trigger:0:*/
void ctc_trigger(int channel)
	{
	CHANNEL *c = &(ctc_channels[channel]);
	if ( (c->control & (CC_CONSTANT|CC_RESET)) == 0 )
		{
		if ( c->control & CC_COUNTER_MODE )
			{
			if ( --(c->counter) == 0 )
				{
				c->counter = c->constant; /* Reload */
				if ( ( c->control & CC_INTERRUPT ) && ( c->is == isNone ) )
					{
					c->is = isPending;
					IF_CHAN(channel)
                    diag_message(DIAG_CTC_PENDING, "CTC interrupt pending on channel %d (counter)", channel);
					ctc_int ();
					}
				}
			}
		else if ( c->control & CC_TIMER_TRIGGER )
			{
			c->run = TRUE;
			}
		}
	}
/*...e*/
/*...sctc_advance:0:*/
/* Important: This advances in terms of the system clock, as fed to
   the whole of the CTC, not in terms of the input to the channel. */

void ctc_advance (int adv)
	{
    static int cnt13;
    int channel;
    cnt13 += adv;
    for ( channel = 0; channel < N_CHANNELS; ++channel )
        {
        CHANNEL *c = &(ctc_channels[channel]);
        IF_CHAN(channel)
            if ( c->run ) diag_message(DIAG_CTC_COUNT, "CTC advance channel %d by %d clocks",
                channel, adv);
        if ( (c->control & (CC_CONSTANT|CC_RESET)) == 0 )
            {
            if ( (c->control & CC_COUNTER_MODE) == 0 )
                {
                if ( c->run )
                    {
                    int clks = adv;
                    while ( clks > 0 )
                        {
                        int prescaler = c->prescaler == 0 ? 0x100 : c->prescaler;
                        if ( clks < prescaler )
                            {
                            c->prescaler -= (byte) clks;
                            break;
                            }
                        else
                            {
                            clks -= prescaler;
                            c->prescaler = ( c->control & CC_PRESCALER_256 ) ? 0 : 16; /* Reload prescaler */
                            if ( --(c->counter) == 0 )
                                {
                                c->counter = c->constant; /* Reload */
                                if ( ( c->control & CC_INTERRUPT ) && ( c->is == isNone ) )
                                    {
                                    c->is = isPending;
                                    IF_CHAN(channel)
                                        diag_message(DIAG_CTC_PENDING,
                                            "CTC interrupt pending on channel %d (timer)",
                                            channel);
                                    ctc_int ();
                                    }
                                }
                            }
                        }
                    IF_CHAN(channel)
                        diag_message(DIAG_CTC_COUNT, "CTC channel %d: prescaler = %d, counter = %d",
                            channel, c->prescaler, c->counter);
                    }
                else
                    {
                    IF_CHAN(channel)
                        diag_message(DIAG_CTC_COUNT,
                            "CTC channel %d: counter started: prescaler = %d, counter = %d",
                            channel, c->prescaler, c->counter);
                    // if ( channel == 2 ) diag_flags[DIAG_Z80_INSTRUCTIONS] = TRUE;
                    c->run = TRUE;
                    }
                }
            else if ( ( channel == 1 ) || ( channel == 2 ) )
                {
                int clks = cnt13 / 13;
                int cntr = ( c->counter == 0 ) ? 0x100 : c->counter;
                int cons = ( c->constant == 0 ) ? 0x100 : c->constant;
                while ( clks > 0 )
                    {
                    if ( cntr <= clks )
                        {
                        clks -= cntr;
                        cntr = cons; /* Reload */
                        if ( ( c->control & CC_INTERRUPT ) && ( c->is == isNone ) )
                            {
                            c->is = isPending;
                            IF_CHAN(channel)
                                diag_message(DIAG_CTC_PENDING,
                                    "CTC interrupt pending on channel %d (counter)", channel);
                            ctc_int ();
                            }
                        }
                    else
                        {
                        cntr -= clks;
                        break;
                        }
                    }
                c->counter = (byte) cntr;
                }
            }
        }
    cnt13 %= 13;
	}
/*...e*/

double ctc_freq (int channel)
	{
	CHANNEL *c = &(ctc_channels[channel]);
	double	freq;
	if ( c->control & CC_COUNTER_MODE )
		{
		static double fIn[] = { 50.0, 4e6 / 13.0, 4e6 / 13.0, 0.0 };
		freq = fIn[channel];
		}
	else if ( c->control & CC_PRESCALER_256 )
		{
		freq = 4e6 / 256.0;
		}
	else
		{
		freq = 4e6 / 16.0;
		}
	return freq;
	}
