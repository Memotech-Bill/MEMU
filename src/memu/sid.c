/*

sid.c - Silicon Disc

Its not clear whether setting up the low or high (or both) parts of the
sector address resets the 7 bit byte-within-sector counter, so we do both.

Its not clear whether the sector address could be read back or not,
but in this emulation we allow it.

Its also not clear whether the byte-within-sector counter carries
into the sector address, as it does in this emulation.

We mustn't let the user change the Silicon Disc content as the emulation runs
because the DPBs for the Silicon Disc type codes are set up assuming the
disks are not removable.
Changing the content would not be detected, and corruption would result.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "types.h"
#include "diag.h"
#include "common.h"
#include "sid.h"

#ifdef __circle__
#include "console.h"
#endif

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vsid\46\h:0:*/
/*...e*/

/*...svars:0:*/
static int sid_emu;

#define	SIDISC_SIZE (8*1024*1024)
#define SIBUF_SIZE  1024

static byte *memory[N_SIDISC] =
	{
	NULL,
	NULL,
	NULL,
	NULL
	};

static const char *fns[N_SIDISC] =
	{
	NULL,
	NULL,
	NULL,
	NULL
	};

static unsigned ptr[N_SIDISC] =
	{
	0,
	0,
	0,
	0
	};

static unsigned size[N_SIDISC] =
	{
	SIDISC_SIZE,
	SIDISC_SIZE,
	SIDISC_SIZE,
	SIDISC_SIZE
	};

static unsigned base[N_SIDISC] =
    {
    0,
    0,
    0,
    0
    };
/*
static unsigned active[N_SIDISC] =
    {
    0,
    0,
    0,
    0
    };
*/
static FILE *fp[N_SIDISC] =
    {
    NULL,
    NULL,
    NULL,
    NULL
    };
/*...e*/

/*...ssid_out:0:*/
void sid_out(word port, byte value)
	{
    int drive = (port-0x50)/4;
    if ( fns[drive] == NULL ) return;
	switch ( port & 0x03 )
		{
		case 0x00:
			/* Set low byte of sector address,
			   reset byte within sector */
			ptr[drive] = ( (ptr[drive]&~0x00007fff) | (unsigned)value <<  7 );
			diag_message(DIAG_SIDISC_ADDRESS, "set low byte of drive %d sector address, now 0x%06x",
                drive, ptr[drive]>>7);
//            diag_message(DIAG_SIDISC_FILE, "Top of drive %d = 0x%08X", drive, active[drive]);
			break;
		case 0x01:
			/* Set high byte of sector address,
			   reset byte within sector */
			ptr[drive] = ( (ptr[drive]&~0x007f807f) | (unsigned)value << 15 );
			diag_message(DIAG_SIDISC_ADDRESS, "set high byte of drive %d sector address, now 0x%06x",
                drive, ptr[drive]>>7);
//            diag_message(DIAG_SIDISC_FILE, "Top of drive %d = 0x%08X", drive, active[drive]);
            break;
		case 0x02:
			if ( sid_emu & SIDEMU_HUGE )
				{
				/* Set high byte of sector address,
				   reset byte within sector */
				ptr[drive] = ( (ptr[drive]&~0x7f80007f) | (unsigned)value << 23 );
				diag_message(DIAG_SIDISC_ADDRESS, "set huge byte of drive %d sector address, now 0x%06x",
                    drive, ptr[drive]>>7);
//                diag_message(DIAG_SIDISC_FILE, "Top of drive %d = 0x%08X", drive, active[drive]);
				}
			break;
		case 0x03:
            if ( ptr[drive] >= size[drive] )
                {
//                diag_message(DIAG_SIDISC_ADDRESS,
//                    "drive pointer wrapped: ptr = 0x%08X, size = 0x%08X, active = 0x%0xX",
//                    drive, ptr[drive], size[drive], active[drive]);
				ptr[drive] = 0;
                }
			if ( memory[drive] == NULL )
				{
                unsigned int nalloc = ( sid_emu & SIDEMU_HUGE ) ? SIBUF_SIZE : SIDISC_SIZE;
				memory[drive] = emalloc(nalloc);
                memset(memory[drive], 0, nalloc);
				}
            if ( sid_emu & SIDEMU_HUGE )
                {
                if ( ( ptr[drive] < base[drive] ) || ( ptr[drive] >= base[drive] + SIBUF_SIZE ) )
                    {
                    size_t n;
                    if ( ! (sid_emu & SIDEMU_NO_SAVE) )
                        {
                        fseek (fp[drive], base[drive], SEEK_SET);
                        n = fwrite (memory[drive], 1, SIBUF_SIZE, fp[drive]);
                        diag_message(DIAG_SIDISC_FILE, "saved %d bytes of Silicon Disc %d content to %s",
                            (int) n, drive, fns[drive]);
                        }
                    fseek (fp[drive], ptr[drive], SEEK_SET);
                    n = fread (memory[drive], 1, SIBUF_SIZE, fp[drive]);
                    diag_message(DIAG_SIDISC_FILE, "loaded %d bytes of Silicon Disc %d content from %s",
                        (int) n, drive, fns[drive]);
                    base[drive] = ptr[drive];
                    }
                }
			memory[drive][ptr[drive] - base[drive]] = value;
			diag_message(DIAG_SIDISC_DATA, "write to drive %d byte 0x%02x", drive, value);
			++ptr[drive];
//            if ( ptr[drive] > active[drive] ) active[drive] = ptr[drive];
			break;
		}
	}
/*...e*/
/*...ssid_in:0:*/
byte sid_in(word port)
	{
	int drive = (port-0x50)/4;
    if ( fns[drive] == NULL ) return (byte) 0xFF;
	switch ( port & 0x03 )
		{
		case 0x00:
			diag_message(DIAG_SIDISC_ADDRESS, "read low byte of drive %d sector address, return 0x%02x", drive, (ptr[drive]>>7)&0xff);
			return (byte) (ptr[drive]>> 7);
		case 0x01:
			diag_message(DIAG_SIDISC_ADDRESS, "read high byte of drive %d sector address, return 0x%02x", drive, (ptr[drive]>>15)&0xff);
			return (byte) (ptr[drive]>>15);
		case 0x02:
			if ( sid_emu & SIDEMU_HUGE )
				{
				diag_message(DIAG_SIDISC_ADDRESS, "read huge byte of drive %d sector address, return 0x%02x", drive, (ptr[drive]>>23)&0xff);
				return (byte) (ptr[drive]>>23);
				}
			break;
		case 0x03:
			{
			byte b;
			if ( ptr[drive] >= size[drive] )
				ptr[drive] = 0;
			if ( memory[drive] != NULL )
                {
                if ( sid_emu & SIDEMU_HUGE )
                    {
                    if ( ( ptr[drive] < base[drive] ) || ( ptr[drive] >= base[drive] + SIBUF_SIZE ) )
                        {
                        size_t n;
                        if ( ! (sid_emu & SIDEMU_NO_SAVE) )
                            {
                            fseek (fp[drive], base[drive], SEEK_SET);
                            n = fwrite (memory[drive], 1, SIBUF_SIZE, fp[drive]);
                            diag_message(DIAG_SIDISC_FILE, "saved %d bytes of Silicon Disc %d content to %s",
                                (int) n, drive, fns[drive]);
                            }
                        fseek (fp[drive], ptr[drive], SEEK_SET);
                        n = fread (memory[drive], 1, SIBUF_SIZE, fp[drive]);
                        diag_message(DIAG_SIDISC_FILE, "loaded %d bytes of Silicon Disc %d content from %s",
                            (int) n, drive, fns[drive]);
                        base[drive] = ptr[drive];
                        }
                    }
				b = memory[drive][ptr[drive] - base[drive]];
                }
			else
				b = 0;
			++ptr[drive];
			diag_message(DIAG_SIDISC_DATA, "read from drive %d byte 0x%02x", drive, b);
			return b;
			}
		}
	return (byte) 0xff; /* Keep compiler happy */
	}
/*...e*/

/*...ssid_set_file:0:*/
void sid_set_file(int drive, const char *fn)
	{
	fns[drive] = fn;
	}

const char *sid_get_file (int drive)
	{
	return	fns[drive];
	}
/*...e*/

/*...ssid_init:0:*/
void sid_load (int drive)
	{
	if ( fns[drive] != NULL )
		{
        ALERT_ON();
        fprintf (stderr, "Loading contents of silicon drive %d from file \"%s\"\n", drive, fns[drive]);
        if ( memory[drive] )
            {
            free (memory[drive]);
            memory[drive] = NULL;
            }
        if ( fp[drive] ) fclose (fp[drive]);
        base[drive] = 0;
//        active[drive] = 0;
		if ( (fp[drive] = fopen(fns[drive], "rb+")) != NULL )
			{
            unsigned int nalloc = ( sid_emu & SIDEMU_HUGE ) ? SIBUF_SIZE : SIDISC_SIZE;
			size_t file_size;
			size_t n;
			fseek(fp[drive], 0L, SEEK_END);
			file_size = (unsigned) ftell(fp[drive]);
			fseek(fp[drive], 0L, SEEK_SET);
			size[drive] = ( (sid_emu & SIDEMU_HUGE) != 0 && file_size > SIDISC_SIZE ) ? file_size : SIDISC_SIZE;
//            active[drive] = file_size;
			memory[drive] = emalloc(nalloc);
			n = fread(memory[drive], 1, nalloc, fp[drive]);
            if ( ! ( sid_emu & SIDEMU_HUGE ) )
                {
                fclose(fp[drive]);
                fp[drive] = NULL;
                }
			diag_message(DIAG_SIDISC_FILE, "loaded %d bytes of Silicon Disc %d content from %s", (int) n, drive, fns[drive]);
			}
		else
			diag_message(DIAG_SIDISC_FILE, "can't load Silicon Disc %d content from %s", drive, fns[drive]);
        ALERT_OFF();
		}
	}

void sid_init(int emu)
	{
	int i;
	sid_emu = emu;
    ALERT_ON();
	for ( i = 0; i < N_SIDISC; i++ ) sid_load (i);
    ALERT_OFF();
	}
/*...e*/
/*...ssid_term:0:*/
void sid_mode (int emu)
    {
    sid_emu = emu;
    }

void sid_save (int drive, BOOLEAN bFree)
	{
	if ( ( fns[drive] != NULL ) && ( memory[drive] != NULL ) && ( (sid_emu & SIDEMU_NO_SAVE) == 0 ) )
		{
        ALERT_ON();
        fprintf (stderr, "Saving contents of silicon drive %d to file \"%s\"\n", drive, fns[drive]);
        if ( fp[drive] == NULL ) fp[drive] = fopen(fns[drive], "wb+");
		if ( fp[drive] != NULL )
			{
			fseek(fp[drive], base[drive], SEEK_SET);
//            size_t nSave = ( sid_emu & SIDEMU_HUGE ) ? SIBUF_SIZE : active[drive];
            size_t nSave = ( sid_emu & SIDEMU_HUGE ) ? SIBUF_SIZE : SIDISC_SIZE;
			size_t n = fwrite(memory[drive], 1, nSave, fp[drive]);
			fclose(fp[drive]);
            fp[drive] = NULL;
			diag_message(DIAG_SIDISC_FILE, "saved %d bytes of Silicon Disc %d content to %s",
                (int) n, drive, fns[drive]);
			}
		else
			diag_message(DIAG_SIDISC_FILE, "can't save Silicon Disc %d content to %s", drive, fns[drive]);
        ALERT_OFF();
		}
    if ( bFree && memory[drive] )
        {
        free (memory[drive]);
        memory[drive] = NULL;
        }
	}

void sid_term (void)
    {
    int i;
    ALERT_ON();
    for ( i = 0; i < N_SIDISC; i++ ) sid_save (i, TRUE);
    ALERT_OFF();
    }
/*...e*/
