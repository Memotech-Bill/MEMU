/* cfx2.c - Emulation of CFX-2 CompactFlash Interface */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include "types.h"
#include "common.h"
#include "diag.h"
#include "cfx2.h"
#include "memu.h"

#define CPM_PART_SIZE   ( 8 * 1024 * 1024 ) // Size of CPM partitions on CF card

#define LTOP_CARD       0x10                // Card selection bit in top address
#define LTOP_LBA        0x40                // LBA mode selection bit in top address

#define CMD_RECALIBRATE 0x10
#define CMD_RD_SECTOR   0x20
#define CMD_WR_SECTOR   0x30
#define CMD_INITIALISE  0x91
#define CMD_SPIN_UP     0xE0
#define CMD_SPIN_DOWN   0xE1
#define CMD_IDENTIFY    0xEC
#define CMD_SET_FEATURE 0xEF

#define FTR_8BIT        0x01
#define FTR_NOCACHE     0x82

#define STA_BUSY        0x80    // Device is busy
#define STA_RDY         0x40    // Device is ready to process commands
#define STA_DF          0x20    // Device fault has occurred
#define STA_DSC         0x10    // Device seek complete
#define STA_DRQ         0x08    // Data Request - ready to read / write data
#define STA_CORR        0x04    // Corrected data error
#define STA_IDX         0x02    // Vendor specific
#define STA_ERR         0x01    // Error in previous command.

#define ERR_UNC         0x40    // Uncorrectable data error
#define ERR_IDNF        0x10    // Requested sector identifier not found
#define ERR_ABRT        0x04    // Aborted due to invalid command or other error
#define ERR_AMNF        0x01    // Address mark not found

static char *psImage[NCF_CARD * NCF_PART] =
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static FILE *pfImage[NCF_CARD * NCF_PART] =
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static unsigned int lba = 0;
static unsigned int part = 0;
static unsigned int addr = 0;
static unsigned int count = 0;
static byte sector[LEN_SECTOR];
static byte feature = 0;
static byte command = 0;
static byte status = 0;
static byte cferr = 0;
static byte lbatop = 0;
static unsigned int nCFPart[NCF_CARD] = { NCF_PART, NCF_PART };
static long long nCFSize[NCF_CARD];

const char *cfx_cmd_name (int iCmd)
    {
    if ( iCmd == 0x00 )
        {
        static const char sCmd00[] = "IDLE";
        return sCmd00;
        }
    else if ( iCmd == 0x10 )
        {
        static const char sCmd10[] = "RECALIBRATE";
        return sCmd10;
        }
    else if ( iCmd == 0x20 )
        {
        static const char sCmd20[] = "RD_SECTOR";
        return sCmd20;
        }
    else if ( iCmd == 0x30 )
        {
        static const char sCmd30[] = "WR_SECTOR";
        return sCmd30;
        }
    else if ( iCmd == 0x91 )
        {
        static const char sCmd91[] = "INITIALISE";
        return sCmd91;
        }
    else if ( iCmd == 0xE0 )
        {
        static const char sCmdE0[] = "SPIN_UP";
        return sCmdE0;
        }
    else if ( iCmd == 0xE1 )
        {
        static const char sCmdE1[] = "SPIN_DOWN";
        return sCmdE1;
        }
    else if ( iCmd == 0xEC )
        {
        static const char sCmdEC[] = "IDENTELSE IFY";
        return sCmdEC;
        }
    else if ( iCmd == 0xEF )
        {
        static const char sCmdEF[] = "SET_FEATURE";
        return sCmdEF;
        }
    static const char sCmdUnk[] = "UNKNOWN";
    return sCmdUnk;
    }

void cfx_dump (void)
    {
    char sLine[77];
    unsigned int ad;
    unsigned int i;
    if ( ! diag_flags[DIAG_SDXFDC_DATA] ) return;
    memset (sLine, ' ', 76);
    sLine[76] = '\0';
    for ( ad = 0; ad < 512; ad += 16 )
        {
        sprintf (&sLine[0], "%04X", ad);
        sLine[4] = ' ';
        for ( i = 0; i < 16; ++i )
            {
            byte b = sector[ad+i];
            sprintf (&sLine[3*i+8], "%02X", b);
            sLine[3*i+10] = ' ';
            if ( ( b >= 0x20 ) && ( b < 0x7F ) ) sLine[i+60] = (char) b;
            else sLine[i+60] = '.';
            }
        diag_message (DIAG_ALWAYS, "%s", sLine);
        }
    }

const char * cfx2_get_image (int iCard, int iPartition)
    {
    if ( ( iCard < 0 ) || ( iCard >= NCF_CARD ) )
        {
        if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Invalid CF card number");
        }
    else if ( ( iPartition < 0 ) || ( iPartition >= NCF_PART ) )
        {
        if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Invalid CF partition number");
        }
    else
        {
        iPartition += NCF_PART * iCard;
        return psImage[iPartition];
        }
    return NULL;
    }

void cfx2_set_image (int iCard, int iPartition, const char *psFile)
    {
    if ( ( iCard < 0 ) || ( iCard >= NCF_CARD ) )
        {
        if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Invalid CF card number");
        }
    else if ( ( iPartition < 0 ) || ( iPartition >= NCF_PART ) )
        {
        if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Invalid CF partition number");
        }
    else
        {
        iPartition += NCF_PART * iCard;
        if ( psImage[iPartition] != NULL )
            {
            free (psImage[iPartition]);
            psImage[iPartition] = NULL;
            }
        if ( psFile != NULL ) psImage[iPartition] = estrdup (psFile);
        }
    }

static void cfx2_seek (void)
    {
    unsigned int iAddr;
    int card = 0;
    if ( lbatop & LTOP_CARD ) card = 1;
    if ( nCFPart[card] == 1 )
        {
        iAddr = lba << ADDR_BITS;
        part = 0;
        }
    else
        {
        iAddr = ( lba & (LEN_PARTITION - 1) ) << ADDR_BITS;
        part = lba >> SECT_BITS;
        }
    diag_message (DIAG_SDXFDC_HW, "CFX2 seek: lba = 0x%08X part = 0x%02X", lba, part);
    if ( part >= nCFPart[card] )
        {
        diag_message (DIAG_SDXFDC_HW, "CFX2 seek: lba = 0x%08X part = 0x%02X", lba, part);
        cferr |= ERR_IDNF;
        status = STA_ERR;
        command = 0;
        count = 0;
        return;
        }
    if ( lbatop & LTOP_CARD )
        {
        part += NCF_PART;
        diag_message (DIAG_SDXFDC_HW, "CFX2 seek on second CF card");
        }
    if ( pfImage[part] == NULL )
        {
        diag_message (DIAG_SDXFDC_HW, "CFX2 attempt to seek to undefined partition %d", part);
        cferr |= ERR_IDNF;
        status |= STA_ERR;
        command = 0;
        count = 0;
        }
    else if ( fseek (pfImage[part], iAddr, SEEK_SET) != 0 )
        {
        diag_message (DIAG_SDXFDC_HW, "CFX2 error seeking to sector %d", lba);
        cferr |= ERR_IDNF;
        status |= STA_ERR;
        command = 0;
        count = 0;
        }
    addr = 0;
    }

static void cfx2_read (void)
    {
    unsigned int nbyte = (unsigned int) fread (sector, 1, LEN_SECTOR, pfImage[part]);
    diag_message (DIAG_SDXFDC_HW, "CFX2 read %d bytes from sector %d", nbyte, lba);
    cfx_dump ();
    /* When formatting a new partition, need to be able to "read" not yet initialised sectors - so no error
       if ( nbyte != LEN_SECTOR )
       {
       cferr |= ERR_UNC;
       status = STA_ERR;
       command = 0;
       count = 0;
       }
    */
    addr = 0;
    }

void cfx2_out (word port, byte value)
    {
    diag_message (DIAG_SDXFDC_PORT, "CFX2 output to port 0x%02x: 0x%02x", port & 0xFF, value);
    switch (port & 0x07 )
        {
        case 0: // Data register
            if ( command == CMD_WR_SECTOR )
                {
                sector[addr] = value;
                // diag_message (DIAG_SDXFDC_DATA, "CFX2 write byte %d to sector: 0x%02x", addr, value);
                status |= STA_BUSY;
                if ( ++addr == LEN_SECTOR )
                    {
                    unsigned int nbyte = (unsigned int) fwrite (sector, 1, LEN_SECTOR, pfImage[part]);
                    diag_message (DIAG_SDXFDC_HW, "CFX2 write %d bytes to sector %d", nbyte, lba);
                    cfx_dump ();
                    if ( nbyte != LEN_SECTOR )
                        {
                        cferr |= ERR_UNC;
                        status |= STA_DF | STA_ERR;
                        command = 0;
                        count = 0;
                        }
                    ++lba;
                    if ( --count == 0 )
                        {
                        command = 0;
                        }
                    else
                        {
                        status |= STA_BUSY;
                        ++lba;
                        }
                    }
                }
            else
                {
                diag_message (DIAG_SDXFDC_HW,
                    "CFX2 unexpected write to data register: command = %s (0x%02X), value = %d",
                    cfx_cmd_name (command), command, value);
#ifdef DEBUG
                show_instruction ();
#endif
                cferr |= ERR_ABRT;
                status |= STA_ERR;
                }
                
            break;
        case 1: // Features register
            feature = value;
            break;
        case 2: // Sector count register
            if ( value == 0 ) count = 256;
            else count = value;
            break;
        case 3: // LBA address bits 0-7
            lba = ( lba & 0xFFFFFF00 ) | ((unsigned long) value );
            break;
        case 4: // LBA address bits 8-15
            lba = ( lba & 0xFFFF00FF ) | ((unsigned long) value << 8);
            break;
        case 5: // LBA address bits 16-23
            lba = ( lba & 0xFF00FFFF ) | ((unsigned long) value << 16);
            break;
        case 6: // LBA address bits 24-27
            lbatop = value;
            if ( ! ( value & LTOP_LBA ) )
                {
                if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] )
                    fatal ("CHS addressing of CompactFlash drives is not supported.");
                diag_message (DIAG_SDXFDC_HW, "CFX2 error: CHS addressing selected");
                status = STA_ERR;
                }
            lba = ( lba & 0x00FFFFFF ) | (((unsigned long) value & 0x0F ) << 24);
            break;
        case 7: // Command register
            if ( status & STA_RDY )
                {
                status |= STA_BUSY;
                cferr = 0;
                switch (value)
                    {
                    case CMD_RECALIBRATE:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 command: Recalibrate");
                        break;
                    case CMD_RD_SECTOR:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 command: Read %d blocks starting at %d",
                            count, lba);
                        command = value;
                        cfx2_seek ();
                        if ( cferr == 0 )
                            {
                            cfx2_read ();
                            if ( cferr == 0 ) status |= STA_DRQ;
                            }
                        break;
                    case CMD_WR_SECTOR:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 command: Write %d blocks starting at %d",
                            count, lba);
                        command = value;
                        cfx2_seek ();
                        if ( cferr == 0 ) status |= STA_DRQ;
                        break;
                    case CMD_INITIALISE:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 command: Initialise");
                        break;
                    case CMD_SPIN_UP:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 command: Spin Up");
                        break;
                    case CMD_SPIN_DOWN:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 command: Spin Down");
                        break;
                    case CMD_IDENTIFY:
                    {
                    int nlb;
                    int card = 0;
                    if ( lbatop & LTOP_CARD ) card = 1;
                    nlb = (int) ( nCFSize[card] / LEN_SECTOR );
                    diag_message (DIAG_SDXFDC_HW, "CFX2 command: Identify");
                    command = value;
                    memset (sector, 0, LEN_SECTOR);
                    sector[99] = 0x02;
                    sector[120] = nlb & 0xFF;
                    sector[121] = ( nlb >> 8 ) & 0xFF;
                    sector[122] = ( nlb >> 16 ) & 0xFF;
                    sector[123] = ( nlb >> 24 ) & 0xFF;
                    count = 1;
                    addr = 0;
                    status |= STA_DRQ;
                    break;
                    }
                    case CMD_SET_FEATURE:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 command: Set feature 0x%02X", feature);
                        if ( ( feature != FTR_8BIT ) && ( feature != FTR_NOCACHE ) )
                            {
                            diag_message (DIAG_SDXFDC_HW, "CFX2 Unsupported feature");
                            cferr |= ERR_AMNF;
                            status |= STA_ERR;
                            }
                        break;
                    default:
                        diag_message (DIAG_SDXFDC_HW, "CFX2 unsupported command: 0x%02X", value);
                        cferr |= ERR_AMNF;
                        status |= STA_ERR;
                        break;
                    }
                }
            else
                {
                diag_message (DIAG_SDXFDC_HW, "CFX2 Error: Write to command register while not ready");
                cferr |= ERR_ABRT;
                status |= STA_ERR;
                }
            break;
        }
    }

byte cfx2_in (word port)
    {
    byte b = 0;
    switch (port & 0x07 )
        {
        case 0: // Data register
            if ( ( command == CMD_RD_SECTOR ) || ( command == CMD_IDENTIFY ) )
                {
                b = sector[addr];
                // diag_message (DIAG_SDXFDC_DATA, "CFX2 byte %d from sector: 0x%02x", addr, b);
                if ( ++addr == LEN_SECTOR )
                    {
                    if ( --count > 0 )
                        {
                        ++lba;
                        cfx2_seek ();
                        cfx2_read ();
                        status |= STA_BUSY;
                        }
                    else
                        {
                        command = 0;
                        break;
                        }
                    }
                }
            else
                {
                diag_message (DIAG_SDXFDC_HW,
                    "CFX2 Unexpected read of data register: command = %s (0x%02X)",
                    cfx_cmd_name (command), command);
                cferr |= ERR_ABRT;
                status |= STA_ERR;
                }
            break;
        case 1: // Error register
            b = cferr;
            diag_message (DIAG_SDXFDC_HW, "CFX2 error register = 0x%02X", cferr);
            break;
        case 2: // Sector count register
            b = (byte) count;
            break;
        case 3: // LBA address bits 0-7
            b = (byte) (lba & 0xFF);
            break;
        case 4: // LBA address bits 8-15
            b = (byte) ((lba >> 8) & 0xFF);
            break;
        case 5: // LBA address bits 16-23
            b = (byte) ((lba >> 16) & 0xFF);
            break;
        case 6: // LBA address bits 24-27
            b = (byte) ((lba >> 24) & 0x0F) | 0xF0;
            break;
        case 7: // Status register
            b = status;
            diag_message (DIAG_SDXFDC_HW, "CFX2 status register = 0x%02X", status);
            if ( status & STA_BUSY ) status &= ~ STA_BUSY;
            else if ( command == 0 ) status |= STA_RDY;
        }
    diag_message (DIAG_SDXFDC_PORT, "CFX2 input from port 0x%02x: 0x%02x", port & 0xFF, b);
    return b;
    }

void cfx2_init (void)
    {
    int card;
    struct stat st;
    cfx2_term ();
    for ( card = 0; card < NCF_CARD; ++card )
        {
        nCFPart[card] = 0;
        nCFSize[card] = 0;
        }
    for ( part = 0; part < NCF_CARD * NCF_PART; ++part )
        {
        if ( psImage[part] != NULL )
            {
            card = part / NCF_PART;
            diag_message (DIAG_SDXFDC_HW, "CFX2 opening image file \"%s\" for partition %d",
                psImage[part], part);
            pfImage[part] = fopen (psImage[part], "r+b");
            // if ( pfImage[part] == NULL ) fatal ("Error opening CF partition image");
            if ( pfImage[part] != NULL )
                {
                nCFPart[card] = ( part % NCF_PART ) + 1;
                stat (psImage[part], &st);
                if ( st.st_size < CPM_PART_SIZE ) st.st_size = CPM_PART_SIZE;
                nCFSize[card] += st.st_size;
                }
            }
        }
    lba = 0;
    part = 0;
    addr = 0;
    count = 0;
    feature = 0;
    command = 0;
    status = STA_RDY;
    cferr = 0;
    lbatop = 0;
    }

void cfx2_term (void)
    {
    for ( part = 0; part < NCF_CARD * NCF_PART; ++part )
        {
        if ( pfImage[part] != NULL )
            {
            diag_message (DIAG_SDXFDC_HW, "CFX2 closing image file \"%s\" for partition %d",
                psImage[part], part);
            fclose (pfImage[part]);
            pfImage[part] = NULL;
            }
        }
    }
