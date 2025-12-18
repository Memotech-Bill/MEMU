/* cfx.c - Emulation of the CFX interface,

   This code emulates the 82C55 interface. Only emulates mode 0 for A & B ports.
   The IDE / ATA emulation is provided by cfx2.c */

#include "cfx.h"
#include "cfx2.h"
#include "diag.h"
#include "common.h"
#include <string.h>

// Bit definitions for port C
#define PC_ADDR     0x07
#define PC_CS0      0x08
#define PC_CS1      0x10
#define PC_WR       0x20
#define PC_RD       0x40
#define PC_RST      0x80

// Bit definitions for 82C55 control register
#define CTL_CLIN    0x01
#define CTL_BIN     0x02
#define CTL_BMODE   0x04
#define CTL_CHIN    0x08
#define CTL_AIN     0x10
#define CTL_AMODE   0x60
#define CTL_ACTIVE  0x80

static byte data_a;
static byte data_b;
static byte data_c;
static byte last_c;
static byte ctl;

void cfx_out (word port, byte value)
    {
    char sDiag[128];
    diag_message (DIAG_CFX_PORT, "CFX output to port 0x%02x: 0x%02x", port & 0xFF, value);
    switch (port & 0x03)
        {
        case 0:
            if (!(ctl & CTL_AIN)) data_a = value;
            break;
        case 1:
            if (!(ctl & CTL_BIN)) data_b = value;
            break;
        case 2:
            switch (ctl & (CTL_CLIN | CTL_CHIN))
                {
                case 0: data_c = value; break;
                case CTL_CHIN: data_c = (data_c & 0xF0) | (value & 0x0f); break;
                case CTL_CLIN: data_c = (data_c & 0x0F) | (value & 0xF0); break;
                default: break;
                }
            sprintf (sDiag, "Addr = %d", data_c & PC_ADDR);
            if (data_c & PC_CS0) strcat (sDiag, ", CS0");
            if (data_c & PC_CS1) strcat (sDiag, ", CS1");
            if (data_c & PC_WR) strcat (sDiag, ", WR");
            if (data_c & PC_RD) strcat (sDiag, ", RD");
            if (data_c & PC_RST) strcat (sDiag, ", RST");
            diag_message (DIAG_CFX_PORT, sDiag);
            if ((data_c & PC_CS0) && (data_c & PC_WR) && (!(last_c & PC_WR)))
                {
                diag_message (DIAG_CFX_PORT, "Write to ATA address %d: 0x%02X%02X", data_c & PC_ADDR, data_b, data_a);
                cfx2_out_high (data_b);
                cfx2_out (data_c & PC_ADDR, data_a);
                }
            if ((data_c & PC_CS0) && (data_c & PC_RD) && (!(last_c & PC_RD)))
                {
                data_a = cfx2_in (data_c & PC_ADDR);
                data_b = cfx2_in_high ();
                diag_message (DIAG_CFX_PORT, "Read from ATA address %d: 0x%02X%02X", data_c & PC_ADDR, data_b, data_a);
                }
            last_c = data_c;
            break;
        case 3:
            if (value & CTL_ACTIVE)
                {
                ctl = value;
                if (ctl & CTL_AMODE) fatal ("CFX 82C55 A mode %d not supported", (ctl & CTL_AMODE) >> 5);
                if (ctl & CTL_BMODE) fatal ("CFX 82C55 B mode %d not supported", (ctl & CTL_BMODE) >> 2);
                if (ctl & CTL_AIN) strcpy (sDiag, "A In");
                else strcpy (sDiag, "A Out");
                if (ctl & CTL_BIN) strcat (sDiag, ", B In");
                else strcat (sDiag, ", B Out");
                if (ctl & CTL_CLIN) strcat (sDiag, ", Low C In");
                else strcat (sDiag, ", Low C Out");
                if (ctl & CTL_CHIN) strcat (sDiag, ", High C In");
                else strcat (sDiag, ", High C Out");
                diag_message (DIAG_CFX_PORT, sDiag);
                }
        }
    }

byte cfx_in (word port)
    {
    byte b = 0;
    switch (port & 0x03 )
        {
        case 0:
            if (ctl & CTL_AIN) b = data_a;
            break;
        case 1:
            if (ctl & CTL_BIN) b = data_b;
            break;
        case 2:
            if (ctl & CTL_CLIN) b |= data_a & 0x0F;
            if (ctl & CTL_CHIN) b |= data_a & 0xF0;
            break;
        case 3:
            b = ctl;
        }
    diag_message (DIAG_CFX_PORT, "CFX input from port 0x%02x: 0x%02x", port & 0xFF, b);
    return b;
    }
