/*  sdcard.c - Emulation of the MFX SD Card Interface

    A very minimal emulation of a real SD Card,
    just enough to satisfy the MFX ROM (and hextrain).

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "sdcard.h"
#include "crc7.h"
#include "common.h"
#include "diag.h"

#define NSDPART         8                   // Maximum number of SD Card partitions
#define SD_PART_SIZE   ( 8 * 1024 * 1024 )  // Size of CPM partitions on SD card
#define LEN_CMD         6                   // Length of commands
#define LEN_RESP        6                   // Length of responses
#define LEN_BLK         512                 // Length of data block

static int nImage = 0;
static const char *psImage[NSDPART] = { NULL };
static FILE *pfImage[NSDPART] = { NULL };
static int ncbt = 0;
static byte cmdbuf[LEN_CMD];
static byte crc = 0;
static bool bAppCmd = false;
static byte respbuf[LEN_RESP];
static int nrbt = 0;
static enum {ctSDv1, ctSDv2, ctSDHC} cdtyp = ctSDHC;
static enum {st_idle, st_r1, st_r3, st_r7, st_rd_ack, st_rm_ack, st_wr_ack,
    st_read, st_rmrd, st_rmnxt, st_wr_wait, st_write} sd_state = st_idle;
static bool bSDHC = false;
static byte databuf[LEN_BLK];
static FILE *pf = NULL;

int sdcard_set_type (const char *psType)
    {
    int iValid = 1;
    if ( strcasecmp (psType, "SDv1") == 0 )         cdtyp = ctSDv1;
    else if ( strcasecmp (psType, "SDv2") == 0 )    cdtyp = ctSDv2;
    else if ( strcasecmp (psType, "SDHC") == 0 )    cdtyp = ctSDHC;
    else                                            iValid = 0;
    return iValid;
    }

static byte calc_crc (const byte *pdata, int nLen)
    {
    byte crc = 0;
    for ( int i = 0; i < nLen; ++i )
        {
        crc = crc7_table[crc ^ *pdata];
        ++pdata;
        }
    return crc;
    }

static FILE * sd_seek (int iPos)
    {
    diag_message (DIAG_SDXFDC_HW, "SD Card seek: Address %d Sector %d", iPos, iPos >> 9);
    int nFile = 0;
    if ( nImage > 1 )
        {
        nFile = iPos / SD_PART_SIZE;
        iPos %= SD_PART_SIZE;
        }
    if ( psImage[nFile] == NULL )
        {
        diag_message (DIAG_SDXFDC_HW, "Seek to non-existant partition %d", nFile);
        return NULL;
        }
    FILE *pf = pfImage[nFile];
    if ( pf == NULL )
        {
        pf = fopen (psImage[nFile], "r+");
        pfImage[nFile] = pf;
        }
    if ( pf != NULL ) fseek (pf, iPos, SEEK_SET);
    return pf;
    }

static void sd_data_out (byte b)
    {
    if ( sd_state == st_wr_wait )
        {
        diag_message (DIAG_SDXFDC_HW, "SD wait data: 0x%02X", b);
        if ( b == 0xFE )
            {
            diag_message (DIAG_SDXFDC_HW, "SD card: State Write");
            sd_state = st_write;
            ncbt = 0;
            }
        return;
        }
    else if ( sd_state == st_write )
        {
        if ( ncbt < LEN_BLK )
            {
            diag_message (DIAG_SDXFDC_DATA, "SD write byte %d: 0x%02X", ncbt, b);
            databuf[ncbt] = b;
            }
        else
            {
            diag_message (DIAG_SDXFDC_HW, "SD checksum: 0x%02X", b);
            }
        if ( ++ncbt == LEN_BLK + 2 )
            {
            if ( fwrite (databuf, 1, LEN_BLK, pf) == LEN_BLK )
                {
                diag_message (DIAG_SDXFDC_HW, "Saved sector");
                respbuf[0] = 0x05;
                }
            else
                {
                diag_message (DIAG_SDXFDC_HW, "Failed to save sector");
                respbuf[0] = 0x0D;
                }
            nrbt = -2;
            sd_state = st_r1;
            ncbt = 0;
            }
        return;
        }
    if (( ncbt == 0 ) && ( b == 0xFF )) return;
    cmdbuf[ncbt] = b;
    if ( ncbt < LEN_CMD - 1 ) crc = crc7_table[crc ^ b];
    diag_message (DIAG_SDXFDC_HW, "Command byte %d: 0x%02X, crc = 0x%02X", ncbt, b, crc);
    if ( ++ncbt < LEN_CMD ) return;
    if ( ! bAppCmd ) cmdbuf[0] &= 0x3F;
    bAppCmd = false;
    switch (cmdbuf[0])
        {
        case 0:         // Go idle
            diag_message (DIAG_SDXFDC_HW, "SD CMD0 - Go Idle");
                respbuf[0] = 0x01;
                nrbt = -2;
                sd_state = st_r1;
            break;
        case 8:         // Send interface condition
            diag_message (DIAG_SDXFDC_HW, "SD CMD8 - Send interface condition");
            if ( cdtyp == ctSDv1 )
                {
                diag_message (DIAG_SDXFDC_HW, "SD CMD8 - Invalid for SDv1");
                respbuf[0] = 0x05;
                nrbt = -2;
                sd_state = st_r1;
                }
            else if ( crc + 1 != cmdbuf[5] )
                {
                diag_message (DIAG_SDXFDC_HW, "SD CMD8 - Invalid CRC");
                respbuf[0] = 0x09;
                nrbt = -2;
                sd_state = st_r1;
                }
            else
                {
                respbuf[0] = 0x01;
                respbuf[1] = 0x00;
                respbuf[2] = 0x00;
                respbuf[3] = 0x01;
                respbuf[4] = cmdbuf[4];
                respbuf[5] = calc_crc (respbuf, 5);
                nrbt = -2;
                sd_state = st_r7;
                }
            break;

        case 12:        // Stop transmission
            diag_message (DIAG_SDXFDC_HW, "SD CMD12 - Stop transmission");
            sd_state = st_idle;
            break;

        case 17:        // Read single block
        case 18:        // Read multiple blocks
            if ( cmdbuf[0] == 17 )
                diag_message (DIAG_SDXFDC_HW, "SD CMD17 - Read single block");
            else
                diag_message (DIAG_SDXFDC_HW, "SD CMD18 - Read multiple blocks");
            respbuf[0] = 0x20;
            nrbt = -2;
            sd_state = st_r1;
            int iPos = ( cmdbuf[1] << 24 ) | ( cmdbuf[2] << 16 ) | ( cmdbuf[3] << 8 ) | cmdbuf[4];
            if ( bSDHC ) iPos <<= 9;
            pf = sd_seek (iPos);
            if ( pf != NULL )
                {
                if ( fread (databuf, 1, LEN_BLK, pf) == LEN_BLK )
                    {
                    respbuf[0] = 0x00;
                    respbuf[1] = 0xFE;
                    if ( cmdbuf[0] == 17 ) sd_state = st_rd_ack;
                    else sd_state = st_rm_ack;
                    }
                }
            break;

        case 24:        // Write single block
            diag_message (DIAG_SDXFDC_HW, "SD CMD24 - Write single block");
            respbuf[0] = 0x20;
            nrbt = -2;
            sd_state = st_r1;
            iPos = ( cmdbuf[1] << 24 ) | ( cmdbuf[2] << 16 ) | ( cmdbuf[3] << 8 ) | cmdbuf[4];
            if ( bSDHC ) iPos <<= 9;
            pf = sd_seek (iPos);
            if ( pf != NULL )
                {
                respbuf[0] = 0x00;
                sd_state = st_wr_ack;
                }
            break;
            
        case 55:        // App command follows
            diag_message (DIAG_SDXFDC_HW, "SD CMD55 - App command follows");
            bAppCmd = true;
            respbuf[0] = 0x00;
            nrbt = -2;
            sd_state = st_r1;
            break;

        case 58:        // Read OCR
            diag_message (DIAG_SDXFDC_HW, "SD CMD58 - Read OCR");
            respbuf[0] = 0x00;
            if ( bSDHC ) respbuf[1] = 0x40;
            else respbuf[1] = 0x00;
            respbuf[2] = 0xFF;
            respbuf[3] = 0x80;
            respbuf[4] = 0x00;
            nrbt = -2;
            sd_state = st_r3;
            break;

        case 41 + 0x40: // Send Operating Condition
            diag_message (DIAG_SDXFDC_HW, "SD ACMD41 - Send Operating Condition");
            if (( cdtyp == ctSDHC ) && ( cmdbuf[1] & 0x40 )) bSDHC = true;
            respbuf[0] = 0x00;
            nrbt = -2;
            sd_state = st_r1;
            break;

        default:
            diag_message (DIAG_SDXFDC_HW, "SDCMD%d - Unimplemeted");
            break;
        }
    ncbt = 0;
    crc = 0;
    }

static byte sd_data_in (void)
    {
    byte b = 0xFF;
    if ( nrbt < 0 ) return b;
    switch (sd_state)
        {
        case st_r1:
        case st_r3:
        case st_r7:
        case st_rd_ack:
        case st_rm_ack:
        case st_wr_ack:
            b = respbuf[nrbt];
            diag_message (DIAG_SDXFDC_HW, "SD response: 0x%02X", b);
            break;
        case st_read:
        case st_rmrd:
            b = databuf[nrbt];
            diag_message (DIAG_SDXFDC_DATA, "SD read byte %d: 0x%02X", nrbt, b);
            break;
        default:
            diag_message (DIAG_SDXFDC_HW, "SD idle response: 0x%02X", b);
            break;
        }
    return b;
    }

static void sd_data_adv (void)
    {
    ++nrbt;
    switch (sd_state)
        {
        case st_idle:
            nrbt = 0;
            break;
        case st_r1:
            if ( nrbt == 1 ) sd_state = st_idle;
            break;
        case st_r3:
            if ( nrbt == 5 ) sd_state = st_idle;
            break;
        case st_r7:
            if ( nrbt == 6 ) sd_state = st_idle;
            break;
        case st_rd_ack:
            if ( nrbt == 2 )
                {
                diag_message (DIAG_SDXFDC_HW, "SD card: State Read");
                sd_state = st_read;
                nrbt = 0;
                }
            break;
        case st_read:
            if ( nrbt == LEN_BLK )
                {
                diag_message (DIAG_SDXFDC_HW, "SD card: State Idle");
                sd_state = st_idle;
                }
            break;
        case st_rm_ack:
            if ( nrbt == 2 )
                {
                diag_message (DIAG_SDXFDC_HW, "SD card: State Read Multiple");
                sd_state = st_rmrd;
                nrbt = 0;
                }
            break;
        case st_rmrd:
            if ( nrbt == LEN_BLK )
                {
                diag_message (DIAG_SDXFDC_HW, "SD card: State Read Next");
                sd_state = st_rmnxt;
                nrbt = -6;
                }
            break;
        case st_rmnxt:
            if ( nrbt == 0 )
                {
                if ( fread (databuf, 1, LEN_BLK, pf) == LEN_BLK )
                    {
                    diag_message (DIAG_SDXFDC_HW, "Read next sector");
                    sd_state = st_rm_ack;
                    }
                else
                    {
                    diag_message (DIAG_SDXFDC_HW, "Error reading next sector");
                    respbuf[0] = 0x09;
                    sd_state = st_r1;
                    }
                }
            break;
        case st_wr_ack:
            if ( nrbt == 1 )
                {
                diag_message (DIAG_SDXFDC_HW, "SD card: State Write Wait");
                sd_state = st_wr_wait;
                }
            break;
        default:
            break;
        }
    }

void sdcard_out (byte port, byte value)
    {
    diag_message (DIAG_SDXFDC_PORT, "SD card output to port 0x%02x: 0x%02x", port & 0xFF, value);
    switch (port)
        {
        case 0xD6:
            sd_data_adv ();
            sd_data_out (value);
            break;
        default:
            break;
        }
    }

byte sdcard_in (byte port)
    {
    byte b = port;
    switch (port)
        {
        case 0xD4:
            // Bit 7 - Data available
            // Bit 6 - Card not changed
            b = 0xC0;
            break;
        case 0xD6:
            b = sd_data_in ();
            break;
        case 0xD7:
            b = sd_data_in ();
            sd_data_adv ();
            break;
        default:
            break;
        }
    diag_message (DIAG_SDXFDC_PORT, "SD card input from port 0x%02x: 0x%02x", port & 0xFF, b);
    return b;
    }

void sdcard_set_image (int iImage, const char *psFile)
    {
    if (( iImage < 0 ) || ( iImage >= NSDPART ))
        {
        if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Invalid SD partition number");
        }
    if ( pfImage[iImage] != NULL )
        {
        fclose (pfImage[iImage]);
        pfImage[iImage] = NULL;
        }
    if ( psImage[iImage] != NULL )
        {
        free ((void *) psImage[iImage]);
        psImage[iImage] = NULL;
        }
    if ( psFile != NULL )
        {
        psImage[iImage] = estrdup (psFile);
        if ( iImage >= nImage ) nImage = iImage + 1;
        }
    else
        {
        if ( nImage == iImage + 1 ) --nImage;
        }
    }
