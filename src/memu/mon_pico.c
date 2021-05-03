/* mon_pico.c - Raspberry Pi Pico emulation of Memotech 80 column display */

#include <string.h>
#include "display_pico.h"
#include "80col_pico.h"
#include "diag.h"
#include "mon.h"

static EightyColumn mon_ec;

/* The hardware level emulation state */

static byte mon_adr_lo;
static byte mon_adr_hi;
static byte mon_ascd;
static byte mon_atrd;
static byte mon_crtc_address;
static int mon_emu;

/* Address low.
   Latches the data onto the screen. */
void mon_out30(byte value)
	{
	mon_adr_lo = value;
	if ( mon_adr_hi & 0x80 )
		/* Its a write */
		{
		word adr = ( (((word)(mon_adr_hi&0x07))<<8) | mon_adr_lo );
		if ( mon_adr_hi & 0x40 )
			mon_ec.ram[adr].ch = mon_ascd;
		if ( mon_adr_hi & 0x20 )
			mon_ec.ram[adr].at = mon_atrd;
		}
	}

/* Address high */
void mon_out31(byte value)
	{
	mon_adr_hi = value;
	}

/* ASCII data */
void mon_out32(byte value)
	{
	mon_ascd = value;
	}

/* Attribute data */
void mon_out33(byte value)
	{
	mon_atrd = value;
	}

/* CRTC address */
void mon_out38(byte value)
	{
	value &= 0x1f; /* Only low 5 bits are significant */
	mon_crtc_address = value;
	}

/* CRTC data */
void mon_out39(byte value)
	{
    if ( mon_crtc_address < NREG80C ) mon_ec.reg[mon_crtc_address] = value;
	}

/* Address low.
   According to ACRT.MAC, this will ring the bell! */
byte mon_in30(void)
	{
	return (byte) 0xff;
	}

/* ASCII data */
byte mon_in32(void)
	{
	word adr = ( (((word)(mon_adr_hi&0x07))<<8) | mon_adr_lo );
	if ( (mon_adr_hi&0x80) == 0x00 )
		{
		byte value = mon_ec.ram[adr].ch;
		return value;
		}
	return 0xff;
	}

/* Attribute data */
byte mon_in33(void)
	{
	word adr = ( (((word)(mon_adr_hi&0x07))<<8) | mon_adr_lo );
	if ( (mon_adr_hi&0x80) == 0x00 )
		{
		byte value = mon_ec.ram[adr].at;
		return value;
		}
	return 0xff;
	}

/* CRTC address.
   Not sure if this is readable on real hardware. */
byte mon_in38(void)
	{
	diag_message(DIAG_MON_HW, "return selected CRTC register returned %d", mon_crtc_address);
	return mon_crtc_address;
	}

/* CRTC data.
   Many of the registers are in fact write-only,
   but we allow them all to be read. */
byte mon_in39(void)
	{
    return mon_ec.reg[mon_crtc_address & 0x0F];
	}

void mon_init(int emu, int width_scale, int height_scale)
    {
    diag_message (DIAG_ALWAYS, "mon_init (0x%02X)", emu);
    mon_emu = emu;
	if ( mon_emu & MONEMU_WIN )
        {
        memset (&mon_ec, 0, sizeof (mon_ec));
        mon_ec.mode = mon_emu & MONEMU_WIN_MONO;
        diag_message (DIAG_ALWAYS, "mon_ec.mode = 0x%02X", mon_ec.mode);
        display_80column (&mon_ec);
        }
    }

void mon_show (void)
    {
    display_80column (&mon_ec);
    }

void mon_term(void)
    {
    diag_message (DIAG_ALWAYS, "mon_term");
    display_vdp ();
    }
