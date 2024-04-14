/* nfx.c - Emulation of the NFX Wiznet Interface */

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <SDL2/SDL_net.h>
#include "nfx.h"
#include "diag.h"
#include "common.h"
#include "types.h"

#define NFX_MEM             0x8000
#define NFX_NSOCK           4

#define R_MR                0x00    // Common mode register
#define R_GWR               0x01    // Gateway address register (4 bytes)
#define R_SUBR              0x05    // Subnet mask register (4 bytes)
#define R_SHAR              0x09    // Source hardware (MAC) address register (6 bytes)
#define R_SIPR              0x0F    // Source IP address register (4 bytes)
#define R_IR                0x15    // Interrupt register
#define R_IMR               0x16    // Interrupt mask register
#define R_RTR               0x17    // Retry time register (2 bytes)
#define R_RCR               0x19    // Retry count register
#define R_RMSR              0x1A    // Receive memory size register
#define R_TMSR              0x1B    // Transmit memory size register

#define S_MR                0x00    // Socket mode register
#define S_CR                0x01    // Socket command register
#define S_IR                0x02    // Socket interrupt register
#define S_SR                0x03    // Socket status register
#define S_PORT              0x04    // Socket source port number (2 bytes)
#define S_DHAR              0x06    // Socket destination hardware (MAC) address register (6 bytes)
#define S_DIPR              0x0C    // Socket destination IP address register (4 bytes)
#define S_DPORT             0x10    // Socket destination port number (2 bytes)
#define S_MSSR              0x12    // Socket maximum segment size register (2 bytes)
#define S_PROTO             0x14    // IP Protocol number for raw mode
#define S_TOS               0x15    // TCP type of service
#define S_TTL               0x16    // TCP time to live
#define S_TX_FSR            0x20    // Socket transmit free size register (2 bytes)
#define S_TX_RD             0x22    // Socket transmit read pointer (2 bytes)
#define S_TX_WR             0x24    // Socket transmit write pointer (2 bytes)
#define S_RX_RSR            0x26    // Socket received size register (2 bytes)
#define S_RX_RD             0x28    // Socket receive read pointer (2 bytes)

// Protocol definitions for mode register
#define PROTO_MASK          0x0F
#define PROTO_CLOSED        0x00
#define PROTO_TCP           0x01
#define PROTO_UDP           0x02
#define PROTO_IPRAW         0x03
#define PROTO_MACRAW        0x04
#define PROTO_PPPoE         0x05

static const char *psProto[] = { "Closed", "TCP", "UDP", "Raw IP", "Raw MAC", "PPPoE" };

// Socket command register verbs
#define SCR_OPEN            0x01
#define SCR_LISTEN          0x02
#define SCR_CONNECT         0x04
#define SCR_DISCON          0x08
#define SCR_CLOSE           0x10
#define SCR_SEND            0x20
#define SCR_SEND_MAC        0x21
#define SCR_SEND_KEEP       0x22
#define SCR_RECV            0x40

// Socket Interrupt Register bits
#define SIR_CON             0x01
#define SIR_DISCON          0x02
#define SIR_RECV            0x04
#define SIR_TIMEOUT         0x08
#define SIR_SEND_OK         0x10

// Socket status register values
#define SOCK_CLOSED         0x00
#define SOCK_ARP            0x01
#define SOCK_INIT           0x13
#define SOCK_LISTEN         0x14
#define SOCK_SYNSENT        0x15
#define SOCK_SYNRECV        0x16
#define SOCK_ESTABLISHED    0x17
#define SOCK_FIN_WAIT       0x18
#define SOCK_CLOSING        0x1A
#define SOCK_TIME_WAIT      0x1B
#define SOCK_CLOSE_WAIT     0x1C
#define SOCK_LAST_ACK       0x1D
#define SOCK_UDP            0x22
#define SOCK_IPRAW          0x32
#define SOCK_MACRAW         0x42
#define SOCK_PPPoE          0x5F

static BOOLEAN nfx_init = FALSE;
static byte nfx_reg[NFX_MEM];
static word nfx_addr;
static int  nfx_offset = 0;
static byte nfx_data[0x2000];

#define nfx_sock_reg(skt, addr) nfx_reg[0x400 + 0x100 * skt + addr]

static struct   // Linux connection per TCP port
    {
    int ncon;       // Number of NFX sockets for this port
    int proto;      // Protocol for the connection
    int port;       // Port number
    TCPsocket iskt; // Socket stream number
    }
    nfx_conn[NFX_NSOCK];

static struct   // Data for each NFX socket
    {
    int icon;       // Linux connection number for this NFX socket
    TCPsocket iskt; // Socket stream number for established connection
    int rxbase;     // Base of RX buffer
    int rxsize;     // Size of RX buffer
    int rxmask;     // Mask for RX address bits
    int rxrd;       // Read location for received data
    int rxwr;       // Write location for received data
    int txbase;     // Base of TX buffer
    int txsize;     // Size of TX buffer
    int txmask;     // Mask for TX address bits
    int txrd;       // Read location for data to transmit
    int txwr;       // Write location for data to transmit
    }
    nfx_sock[NFX_NSOCK];

typedef struct
    {
    word        addr;
    const char *ps;
    } reg_desc;

reg_desc comm_desc[] = {
    { 0x00, "Mode" },
    { 0x01, "Gateway address %d" },
    { 0x05, "Subnet mask %d" },
    { 0x09, "Source MAC address %d" },
    { 0x0F, "Source IP address %d" },
    { 0x13, "Reserved A %d" },
    { 0x15, "Interrupt" },
    { 0x16, "Interrupt mask" },
    { 0x17, "Retry time %d" },
    { 0x19, "Retry count" },
    { 0x1A, "RX memory size" },
    { 0x1B, "TX memory size" },
    { 0x1C, "PPPoE authentication type %d" },
    { 0x1E, "Reserved B %d" },
    { 0x28, "LCP request timer" },
    { 0x29, "LCP magic number" },
    { 0x2A, "Unreachable IP address %d" },
    { 0x2E, "Unreachable port %d" },
    { 0x30, "" }};

reg_desc sock_desc[] = {
    { S_MR, "mode" },
    { S_CR, "command" },
    { S_IR, "interrupt" },
    { S_SR, "status" },
    { S_PORT, "source port %d" },
    { 0x06, "destination MAC address %d" },
    { 0x0C, "destination IP address %d" },
    { 0x10, "destination port %d" },
    { 0x12, "maximum segment size %d" },
    { 0x14, "raw IP protocol" },
    { 0x15, "TCP TOS" },
    { 0x16, "TCP TTL" },
    { 0x17, "Reserved D %d" },
    { 0x20, "TX free size %d" },
    { 0x22, "TX read pointer %d" },
    { 0x24, "TX write pointer %d" },
    { 0x26, "RX received size %d" },
    { 0x28, "RX read pointer %d" },
    { 0x2A, "Reserved E %d" },
    { 0xFF, "" }};

static void nfx_initialise (void);
    
int info_index (reg_desc *desc, word addr)
    {
    int i = 0;
    while (desc[i].addr <= addr) ++i;
    return i - 1;
    }

const char *reg_info (word addr)
    {
    static char sDesc[256];
    if ( addr < 0x30 )
        {
        int idx = info_index (comm_desc, addr);
        sprintf (sDesc, comm_desc[idx].ps, addr - comm_desc[idx].addr);
        }
    else if ( addr < 0x400 )
        {
        sprintf (sDesc, "Reserved C %d", addr - 0x400);
        }
    else if ( addr < 0x800 )
        {
        int sock = ( addr - 0x400 ) >> 8;
        sprintf (sDesc, "Socket %d ", sock);
        addr &= 0xFF;
        int idx = info_index (sock_desc, addr);
        sprintf (&sDesc[9], sock_desc[idx].ps, addr - sock_desc[idx].addr);
        }
    else if ( addr < 0x4000 )
        {
        sprintf (sDesc, "Reserved F %d", addr - 0x800);
        }
    else if ( addr < 0x6000 )
        {
        sprintf (sDesc, "TX memory %d", addr - 0x4000);
        }
    else if (addr < 0x8000 )
        {
        sprintf (sDesc, "RX memory %d", addr - 0x6000);
        }
    else
        {
        strcpy (sDesc, "Invalid");
        }
    return sDesc;
    }

static void nfx_show_data (byte *pdata, int ndata)
    {
    static char sLine[80];
    int addr = 0;
    while ( ndata > 0 )
        {
        sprintf (sLine, "%04X:", addr);
        for (int i = 0; i < 16; ++i )
            {
            if ( ndata > 0 )
                {
                sprintf (&sLine[3*i+5], " %02X", *pdata);
                if (( *pdata >= 0x20 ) && ( *pdata < 0x7F ))
                    sLine[i+55] = (char) *pdata;
                else
                    sLine[i+55] = '.';
                }
            else
                {
                strcpy (&sLine[3*i+5], "   ");
                sLine[i+55] = ' ';
                }
            ++pdata;
            --ndata;
            }
        sLine[53] = ' ';
        sLine[54] = ' ';
        sLine[71] = '\0';
        diag_message (DIAG_NFX_DATA, "%s", sLine);
        addr += 16;
        }
    }

void nfx_common_out (word addr, byte value)
    {
    switch (addr)
        {
        case R_MR:
            nfx_reg[addr] = value;
            if ( !(nfx_reg[0] & 0x01) ) fatal ("NFX direct bus mode not implemented");
            if ( ! nfx_init ) nfx_initialise ();
            nfx_reg[0] &= 0x7F;
            break;
        case R_IR:
            nfx_reg[addr] &= ~ ( value & 0xE0 );
            break;
        case R_RMSR:
            nfx_reg[addr] = value;
            nfx_sock[0].rxbase = 0x6000;
            nfx_sock[0].rxsize = 0x400 << ( value & 0x03 );
            nfx_sock[0].rxmask = nfx_sock[0].rxsize - 1;
            nfx_sock[1].rxbase = nfx_sock[0].rxbase + nfx_sock[0].rxsize;
            nfx_sock[1].rxsize = 0x400 << ( ( value >> 2 ) & 0x03 );
            if ( nfx_sock[1].rxbase + nfx_sock[1].rxsize > 0x8000 )
                nfx_sock[1].rxsize = 0x8000 - nfx_sock[1].rxbase;
            nfx_sock[1].rxmask = nfx_sock[1].rxsize - 1;
            nfx_sock[2].rxbase = nfx_sock[1].rxbase + nfx_sock[1].rxsize;
            nfx_sock[2].rxsize = 0x400 << ( ( value >> 4 ) & 0x03 );
            if ( nfx_sock[2].rxbase + nfx_sock[2].rxsize > 0x8000 )
                nfx_sock[2].rxsize = 0x8000 - nfx_sock[2].rxbase;
            nfx_sock[2].rxmask = nfx_sock[2].rxsize - 1;
            nfx_sock[3].rxbase = nfx_sock[2].rxbase + nfx_sock[2].rxsize;
            nfx_sock[3].rxsize = 0x400 << ( ( value >> 6 ) & 0x03 );
            if ( nfx_sock[3].rxbase + nfx_sock[3].rxsize > 0x8000 )
                nfx_sock[3].rxsize = 0x8000 - nfx_sock[3].rxbase;
            nfx_sock[3].rxmask = nfx_sock[3].rxsize - 1;
            break;
        case R_TMSR:
            nfx_reg[addr] = value;
            nfx_sock[0].txbase = 0x4000;
            nfx_sock[0].txsize = 0x400 << ( value & 0x03 );
            nfx_sock[0].txmask = nfx_sock[0].txsize - 1;
            nfx_sock[1].txbase = nfx_sock[0].txbase + nfx_sock[0].txsize;
            nfx_sock[1].txsize = 0x400 << ( ( value >> 2 ) & 0x03 );
            if ( nfx_sock[1].txbase + nfx_sock[1].txsize > 0x6000 )
                nfx_sock[1].txsize = 0x6000 - nfx_sock[1].txbase;
            nfx_sock[1].txmask = nfx_sock[1].txsize - 1;
            nfx_sock[2].txbase = nfx_sock[1].txbase + nfx_sock[1].txsize;
            nfx_sock[2].txsize = 0x400 << ( ( value >> 4 ) & 0x03 );
            if ( nfx_sock[2].txbase + nfx_sock[2].txsize > 0x6000 )
                nfx_sock[2].txsize = 0x6000 - nfx_sock[2].txbase;
            nfx_sock[2].txmask = nfx_sock[2].txsize - 1;
            nfx_sock[3].txbase = nfx_sock[2].txbase + nfx_sock[2].txsize;
            nfx_sock[3].txsize = 0x400 << ( ( value >> 6 ) & 0x03 );
            if ( nfx_sock[3].txbase + nfx_sock[3].txsize > 0x6000 )
                nfx_sock[3].txsize = 0x6000 - nfx_sock[3].txbase;
            nfx_sock[3].txmask = nfx_sock[3].txsize - 1;
            break;
        default:
            nfx_reg[addr] = value;
            break;
        }
    }

void nfx_socket_close (int skt)
    {
    if ( nfx_sock[skt].iskt != NULL )
        {
        SDLNet_TCP_Close (nfx_sock[skt].iskt);
        nfx_sock[skt].iskt = NULL;
        }
    int icon = nfx_sock[skt].icon;
    if ( icon >= 0 )
        {
        // diag_message (DIAG_NFX_EVENT, "Close socket %d: icon = %d ncon = %d", skt, icon, nfx_conn[icon].ncon);
        if ( --nfx_conn[icon].ncon == 0 )
            {
            SDLNet_TCP_Close (nfx_conn[icon].iskt);
            // diag_message (DIAG_NFX_EVENT, "Close connection %d", icon);
            nfx_conn[icon].iskt = NULL;
            }
        nfx_sock[skt].icon = -1;
        }
    if ( nfx_sock_reg(skt, S_SR) != SOCK_CLOSED )
        {
        nfx_sock_reg(skt, S_SR) = SOCK_CLOSED;
        if ( !(nfx_sock_reg(skt, S_IR) & SIR_DISCON) )
            diag_message (DIAG_NFX_EVENT, "Socket %d: Raise DISCONNECT interrupt", skt);
        nfx_sock_reg(skt, S_IR) |= SIR_DISCON;
        nfx_reg[R_IR] |= 1 << skt;
        }
    }

void nfx_socket_cmnd_tcp (int skt, byte value)
    {
    uint16_t port = ( nfx_sock_reg(skt, S_PORT) << 8 ) | nfx_sock_reg(skt, S_PORT + 1);
    switch (value)
        {
        case SCR_OPEN:
        {
        // OPEN Command
        diag_message (DIAG_NFX_EVENT, "Socket %d Open TCP", skt);
        nfx_sock_reg(skt, S_SR) = SOCK_INIT;
        nfx_sock[skt].txrd = 0;
        nfx_sock[skt].txwr = 0;
        nfx_sock_reg(skt, S_TX_RD) = 0;
        nfx_sock_reg(skt, S_TX_RD + 1) = 0;
        nfx_sock_reg(skt, S_TX_WR) = 0;
        nfx_sock_reg(skt, S_TX_WR + 1) = 0;
        nfx_sock_reg(skt, S_TX_FSR) = nfx_sock[skt].txsize >> 8;
        nfx_sock_reg(skt, S_TX_FSR + 1) = nfx_sock[skt].txsize & 0xFF;
        nfx_sock[skt].rxrd = 0;
        nfx_sock[skt].rxwr = 0;
        nfx_sock_reg(skt, S_RX_RD) = 0;
        nfx_sock_reg(skt, S_RX_RD + 1) = 0;
        nfx_sock_reg(skt, S_RX_RSR) = 0;
        nfx_sock_reg(skt, S_RX_RSR + 1) = 0;
        break;
        }
        case SCR_LISTEN:
        {
        // Listen command
        diag_message (DIAG_NFX_EVENT, "Socket %d Listen TCP port %d", skt, port);
        int icon = -1;
        for (int i = 0; i < NFX_NSOCK; ++i)
            {
            // diag_message (DIAG_NFX_EVENT, "Connection %d: ncon = %d proto = %d, port = %d",
            //     i, nfx_conn[i].ncon, nfx_conn[i].proto, nfx_conn[i].port);
            if (( nfx_conn[i].ncon > 0 ) && ( nfx_conn[i].proto == PROTO_TCP )
                && ( nfx_conn[i].port == port ))
                {
                ++nfx_conn[i].ncon;
                nfx_sock[skt].icon = i;
                // diag_message (DIAG_NFX_EVENT, "Socket %d added to connection %d", skt, i);
                nfx_sock_reg(skt, S_SR) = SOCK_LISTEN;
                return;
                }
            else if ( nfx_conn[i].ncon == 0 ) icon = i;
            }
        if ( icon < 0 ) fatal ("No free connections for socket %d", skt);
        // diag_message (DIAG_NFX_EVENT, "Using connection %d for socket %d", icon, skt);
        IPaddress ip;
        ip.host = INADDR_ANY;
        ip.port = port;
        nfx_conn[icon].ncon = 1;
        nfx_conn[icon].proto = PROTO_TCP;
        nfx_conn[icon].port = port;
        nfx_sock[skt].icon = icon;
        nfx_conn[icon].iskt = SDLNet_TCP_Open (&ip);
        if ( nfx_conn[icon].iskt == NULL ) fatal ("Could not create socket %d", skt);
        nfx_sock_reg(skt, S_SR) = SOCK_LISTEN;
        break;
        }
        case SCR_CONNECT:
        {
        IPaddress ip;
        ip.host = ( nfx_sock_reg(skt, S_DIPR) << 24 ) | ( nfx_sock_reg(skt, S_DIPR + 1) << 16 )
            | ( nfx_sock_reg(skt, S_DIPR + 2) << 8 ) | nfx_sock_reg(skt, S_DIPR + 3);
        ip.port = ( nfx_sock_reg(skt, S_DPORT) << 8 ) | nfx_sock_reg(skt, S_DPORT + 1);
        diag_message (DIAG_NFX_EVENT, "Socket %d Connect to %d.%d.%d.%d:%d", skt,
            nfx_sock_reg(skt, S_DIPR), nfx_sock_reg(skt, S_DIPR + 1),
            nfx_sock_reg(skt, S_DIPR + 2), nfx_sock_reg(skt, S_DIPR + 3), port);
        nfx_sock[skt].iskt = SDLNet_TCP_Open (&ip);
        if ( nfx_sock[skt].iskt == NULL ) fatal ("Could not create socket %d", skt);
        if ( !(nfx_sock_reg(skt, S_IR) & SIR_CON) )
            diag_message (DIAG_NFX_EVENT, "Socket %d: Raise CONNECT interrupt", skt);
        nfx_sock_reg(skt, S_IR) |= SIR_CON;
        nfx_reg[R_IR] |= 1 << skt;
        nfx_sock_reg(skt, S_SR) = SOCK_ESTABLISHED;
        break;
        }
        case SCR_DISCON:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Disconnect", skt);
        nfx_socket_close (skt);
        if ( !(nfx_sock_reg(skt, S_IR) & SIR_DISCON) )
            diag_message (DIAG_NFX_EVENT, "Socket %d: Raise DISCONNECT interrupt", skt);
        nfx_sock_reg(skt, S_IR) |= SIR_DISCON;
        nfx_reg[R_IR] |= 1 << skt;
        break;
        }
        case SCR_CLOSE:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Close", skt);
        nfx_socket_close (skt);
        break;
        }
        case SCR_SEND:
        {
        nfx_sock[skt].txwr = ( nfx_sock_reg(skt, S_TX_WR) << 8 ) | nfx_sock_reg(skt, S_TX_WR + 1);
        int nsend = nfx_sock[skt].txwr - nfx_sock[skt].txrd;
        diag_message (DIAG_NFX_EVENT, "Socket %d Send %d bytes", skt, nsend);
        if ( nsend > 0 )
            {
            int nbyte;
            if ( nfx_sock[skt].txwr <= nfx_sock[skt].txsize )
                {
                nbyte = SDLNet_TCP_Send (nfx_sock[skt].iskt, &nfx_reg[nfx_sock[skt].txbase + nfx_sock[skt].txrd], nsend);
                if ( diag_flags[DIAG_NFX_DATA] )
                    nfx_show_data (&nfx_reg[nfx_sock[skt].txbase + nfx_sock[skt].txrd], nsend);
                }
            else
                {
                int ncopy = nfx_sock[skt].txsize - nfx_sock[skt].txrd;
                // diag_message (DIAG_NFX_EVENT, "ncopy = %d", ncopy);
                memcpy (nfx_data, &nfx_reg[nfx_sock[skt].txbase + nfx_sock[skt].txrd], ncopy);
                memcpy (&nfx_data[ncopy],  &nfx_reg[nfx_sock[skt].txbase], nsend - ncopy);
                nbyte = SDLNet_TCP_Send (nfx_sock[skt].iskt, nfx_data, nsend);
                if ( diag_flags[DIAG_NFX_DATA] ) nfx_show_data (nfx_data, nsend);
                }
            if ( nbyte >= 0 )
                {
                nfx_sock[skt].txrd += nbyte;
                if ( nfx_sock[skt].txrd >= nfx_sock[skt].txsize )
                    {
                    nfx_sock[skt].txrd -= nfx_sock[skt].txsize;
                    nfx_sock[skt].txwr -= nfx_sock[skt].txsize;
                    }
                int nfree = nfx_sock[skt].txrd - nfx_sock[skt].txwr;
                if ( nfree <= 0 ) nfree += nfx_sock[skt].txsize;
                nfx_sock_reg(skt, S_TX_RD) = nfx_sock[skt].txrd >> 8;
                nfx_sock_reg(skt, S_TX_RD + 1) = nfx_sock[skt].txrd & 0xFF;
                nfx_sock_reg(skt, S_TX_WR) = nfx_sock[skt].txwr >> 8;
                nfx_sock_reg(skt, S_TX_WR + 1) = nfx_sock[skt].txwr & 0xFF;
                nfx_sock_reg(skt, S_TX_FSR) = nfree >> 8;
                nfx_sock_reg(skt, S_TX_FSR + 1) = nfree & 0xFF;
                if ( !(nfx_sock_reg(skt, S_IR) & SIR_SEND_OK) )
                    diag_message (DIAG_NFX_EVENT, "Socket %d: Raise SEND_OK interrupt", skt);
                nfx_sock_reg(skt, S_IR) |= SIR_SEND_OK;
                nfx_reg[R_IR] |= 1 << skt;
                }
            else
                {
                diag_message (DIAG_NFX_EVENT, "Error sending data on socket %d: %s", skt, SDL_GetError ());
                }
            }
        break;
        }
        case SCR_SEND_KEEP:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Send Keep", skt);
        break;
        }
        case SCR_RECV:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Receive", skt);
        nfx_sock[skt].rxrd = ( nfx_sock_reg(skt, S_RX_RD) << 8 ) | nfx_sock_reg(skt, S_RX_RD + 1);
        int ndata = nfx_sock[skt].rxwr - nfx_sock[skt].rxrd;
        if ( ndata < 0 ) ndata = 0;
        // if ( ndata < 0 ) ndata  += nfx_sock[skt].rxsize;
        nfx_sock_reg(skt, S_RX_RSR) = ndata >> 8;
        nfx_sock_reg(skt, S_RX_RSR + 1) = ndata & 0xFF;
        if ( nfx_sock[skt].rxrd > nfx_sock[skt].rxmask )
            {
            nfx_sock[skt].rxrd &= nfx_sock[skt].rxmask;
            nfx_sock[skt].rxwr = nfx_sock[skt].rxrd + ndata;
            nfx_sock_reg(skt, S_RX_RD) = nfx_sock[skt].rxrd >> 8;
            nfx_sock_reg(skt, S_RX_RD + 1) = nfx_sock[skt].rxrd & 0xFF;
            }
        break;
        }
        default:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Undefined command", skt);
        break;
        }
        }
    }

void nfx_socket_cmnd_other (int skt, byte value)
    {
    int proto = nfx_sock_reg(skt, S_MR) & PROTO_MASK;
    switch (value)
        {
        case SCR_OPEN:
        {
        // OPEN Command
        switch (proto)
            {
            case PROTO_UDP:
            {
            diag_message (DIAG_NFX_EVENT, "Socket %d Open UDP", skt);
            nfx_sock_reg(skt, S_SR) = SOCK_UDP;
            break;
            }
            case PROTO_IPRAW:
            {
            diag_message (DIAG_NFX_EVENT, "Socket %d Open Raw IP", skt);
            nfx_sock_reg(skt, S_SR) = SOCK_IPRAW;
            break;
            }
            case PROTO_MACRAW:
            {
            diag_message (DIAG_NFX_EVENT, "Socket %d Open Raw MAC", skt);
            nfx_sock_reg(skt, S_SR) = SOCK_MACRAW;
            break;
            }
            case PROTO_PPPoE:
            {
            diag_message (DIAG_NFX_EVENT, "Socket %d Open PPPoE", skt);
            nfx_sock_reg(skt, S_SR) = SOCK_PPPoE;
            break;
            }
            }
        break;
        }
        case SCR_LISTEN:
        {
        // Listen command
        fatal ("Socket %d protocol %s not implemented", skt, psProto[proto]);
        break;
        }
        case SCR_CONNECT:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Connect", skt);
        break;
        }
        case SCR_DISCON:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Disconnect", skt);
        break;
        }
        case SCR_CLOSE:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Close", skt);
        break;
        }
        case SCR_SEND:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Send", skt);
        break;
        }
        case SCR_SEND_MAC:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Send MAC", skt);
        break;
        }
        case SCR_SEND_KEEP:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Send Keep", skt);
        break;
        }
        case SCR_RECV:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Receive", skt);
        break;
        }
        default:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Undefined command", skt);
        break;
        }
        }
    }

void nfx_socket_cmnd (int skt, byte value)
    {
    int proto = nfx_sock_reg(skt, S_MR) & PROTO_MASK;
    switch (proto)
        {
        case PROTO_TCP:
            nfx_socket_cmnd_tcp (skt, value);
            break;
        case PROTO_UDP:
            nfx_socket_cmnd_other (skt, value);
            break;
        case PROTO_IPRAW:
            nfx_socket_cmnd_other (skt, value);
            break;
        case PROTO_MACRAW:
            nfx_socket_cmnd_other (skt, value);
            break;
        case PROTO_PPPoE:
            nfx_socket_cmnd_other (skt, value);
            break;
        default:
            fatal ("Socket %d protocol 0x%02X not defined", skt, proto);
            break;
        }
    }

void nfx_socket_out (int skt, word addr, byte value)
    {
    switch (addr)
        {
        case S_CR:
            // Socket command
            nfx_socket_cmnd (skt, value);
            break;
        case S_IR:
            {
            byte ir_clr = nfx_sock_reg(skt, S_IR) & value;
            nfx_sock_reg(skt, S_IR) &= ~value;
            if ( ir_clr & 0x01 ) diag_message (DIAG_NFX_EVENT, "Socket %d: Clear CONNECT interrupt", skt);
            if ( ir_clr & 0x02 ) diag_message (DIAG_NFX_EVENT, "Socket %d: Clear DISCONNECT interrupt", skt);
            if ( ir_clr & 0x04 ) diag_message (DIAG_NFX_EVENT, "Socket %d: Clear RECEIVED interrupt", skt);
            if ( ir_clr & 0x08 ) diag_message (DIAG_NFX_EVENT, "Socket %d: Clear TIMEOUT interrupt", skt);
            if ( ir_clr & 0x10 ) diag_message (DIAG_NFX_EVENT, "Socket %d: Clear SEND_OK interrupt", skt);
            if ( nfx_sock_reg(skt, S_IR) == 0 ) nfx_reg[R_IR] &= ~( 1 << skt );
            }
            break;
        default:
            nfx_sock_reg(skt, addr) = value;
            break;
        }
    }

void nfx_port_offset (int offset)
    {
    nfx_offset = offset;
    diag_message (DIAG_NFX_REG, "NFX ports will be offset by %d", offset);
    }

void nfx_out (byte port, byte value)
    {
    diag_message (DIAG_NFX_PORT, "NFX write: port = %d, value = 0x%02X", port, value);
    switch (port)
        {
        case 0:
            nfx_reg[0] = value;
            diag_message (DIAG_NFX_REG, "Write nfx_reg[0000] = 0x%02X - Mode", value);
            nfx_common_out (0, value);
            break;
        case 1:
            nfx_addr = ( ((word) value) << 8 ) | ( nfx_addr & 0xFF );
            break;
        case 2:
            nfx_addr = ( nfx_addr & 0xFF00 ) | value;
            break;
        case 3:
            diag_message (DIAG_NFX_REG, "Write nfx_reg[%04X] = 0x%02X - %s", nfx_addr, value,
                reg_info(nfx_addr));
            if ( nfx_addr < 0x30 )
                {
                nfx_common_out (nfx_addr, value);
                }
            else if (( nfx_addr >= 0x400 ) && ( nfx_addr < 0x800 ))
                {
                nfx_socket_out ((nfx_addr - 0x400) >> 8, nfx_addr & 0xFF, value);
                }
            else
                {
                nfx_reg[nfx_addr] = value;
                }
            if ( nfx_reg[0] & 0x02 ) ++nfx_addr;
            break;
        }
    }

void nfx_poll (void)
    {
    for (int skt = 0; skt < NFX_NSOCK; ++skt)
        {
//        if ( nfx_sock_reg(skt, S_IR) == 0 )
            {
            if ( nfx_sock_reg(skt, S_SR) == SOCK_LISTEN )
                {
                // Test for new connection
                int icon = nfx_sock[skt].icon;
                nfx_sock[skt].iskt = SDLNet_TCP_Accept (nfx_conn[icon].iskt);
                if ( nfx_sock[skt].iskt != NULL )
                    {
                    IPaddress *ip = SDLNet_TCP_GetPeerAddress (nfx_sock[skt].iskt);
                    uint32_t addr = ntohl (ip->host);
                    nfx_sock_reg(skt, S_DIPR)     = ( addr >> 24 ) & 0xFF;
                    nfx_sock_reg(skt, S_DIPR + 1) = ( addr >> 16 ) & 0xFF;
                    nfx_sock_reg(skt, S_DIPR + 2) = ( addr >> 8 ) & 0xFF;
                    nfx_sock_reg(skt, S_DIPR + 3) = addr & 0xFF;
                    uint16_t port = ntohs (ip->port);
                    nfx_sock_reg(skt, S_DPORT)     = ( port >> 8 ) & 0xFF;
                    nfx_sock_reg(skt, S_DPORT + 1) = port & 0xFF;
                    if ( !(nfx_sock_reg(skt, S_IR) & SIR_CON) )
                        diag_message (DIAG_NFX_EVENT, "Socket %d: Raise CONNECT interrupt", skt);
                    nfx_sock_reg(skt, S_IR) |= SIR_CON;
                    nfx_reg[R_IR] |= 1 << skt;
                    nfx_sock_reg(skt, S_SR) = SOCK_ESTABLISHED;
                    IPaddress local_ip;
                    SDLNet_GetLocalAddresses (&local_ip, 1);
                    addr = ntohl (local_ip.host);
                    nfx_reg[R_SIPR]     = ( addr >> 24 ) & 0xFF;
                    nfx_reg[R_SIPR + 1] = ( addr >> 16 ) & 0xFF;
                    nfx_reg[R_SIPR + 2] = ( addr >> 8 ) & 0xFF;
                    nfx_reg[R_SIPR + 3] = addr & 0xFF;
                    diag_message (DIAG_NFX_EVENT,
                        "Socket %d connection from %d.%d.%d.%d port %d to %d.%d.%d.%d.",
                        skt, nfx_sock_reg(skt, S_DIPR), nfx_sock_reg(skt, S_DIPR + 1),
                        nfx_sock_reg(skt, S_DIPR + 2), nfx_sock_reg(skt, S_DIPR + 3), port,
                        nfx_reg[R_SIPR], nfx_reg[R_SIPR + 1], nfx_reg[R_SIPR + 2], nfx_reg[R_SIPR + 3]);
                    }
                else
                    {
                    diag_message (DIAG_NFX_EVENT, "Error testing for connection on socket %d: %s",
                        skt, SDL_GetError ());
                    }
                }
            else if ( nfx_sock_reg(skt, S_SR) == SOCK_ESTABLISHED )
                {
                // Test for data received
                // int nfree = nfx_sock[skt].rxrd - nfx_sock[skt].rxwr;
                // if ( nfree <= 0 ) nfree += nfx_sock[skt].rxsize;
                int nfree = nfx_sock[skt].rxsize - (nfx_sock[skt].rxwr - nfx_sock[skt].rxrd);
                if ( nfree > 0 )
                    {
                    // diag_message (DIAG_NFX_EVENT, "nfx_poll: rxrd = %d rxwr = %d nfree = %d",
                    //     nfx_sock[skt].rxrd, nfx_sock[skt].rxwr, nfree);
                    ssize_t nbyte = SDLNet_TCP_Recv (nfx_sock[skt].iskt, nfx_data, nfree);
                    if ( nbyte > 0 )
                        {
                        diag_message (DIAG_NFX_EVENT, "Socket %d: Received %d bytes", skt, nbyte);
                        if ( diag_flags[DIAG_NFX_DATA] ) nfx_show_data (nfx_data, nbyte);
                        int rxwr = nfx_sock[skt].rxwr & nfx_sock[skt].rxmask;
                        if ( rxwr + nbyte > nfx_sock[skt].rxmask )
                            {
                            int ncopy = nfx_sock[skt].rxsize - rxwr;
                            memcpy (&nfx_reg[nfx_sock[skt].rxbase + rxwr], nfx_data, ncopy);
                            memcpy (&nfx_reg[nfx_sock[skt].rxbase], &nfx_data[ncopy], nbyte - ncopy);
                            }
                        else
                            {
                            memcpy (&nfx_reg[nfx_sock[skt].rxbase + rxwr], nfx_data, nbyte);
                            }
                        nfx_sock[skt].rxwr += nbyte;
                        int ndata = nfx_sock[skt].rxwr - nfx_sock[skt].rxrd;
                        // if ( ndata < 0 ) ndata  += nfx_sock[skt].rxsize;
                        nfx_sock_reg(skt, S_RX_RSR) = ndata >> 8;
                        nfx_sock_reg(skt, S_RX_RSR + 1) = ndata & 0xFF;
                        if ( !(nfx_sock_reg(skt, S_IR) & SIR_RECV) )
                            diag_message (DIAG_NFX_EVENT, "Socket %d: Raise RECEIVED interrupt", skt);
                        nfx_sock_reg(skt, S_IR) |= SIR_RECV;
                        nfx_reg[R_IR] |= 1 << skt;
                        }
                    else if ( nbyte == 0 )
                        {
                        diag_message (DIAG_NFX_EVENT, "Closed socket %d due to zero byte read", skt);
                        // nfx_sock_reg(skt, S_SR) = SOCK_CLOSE_WAIT;
                        // nfx_sock_reg(skt, S_IR) |= SIR_DISCON;
                        // nfx_reg[R_IR] |= 1 << skt;
                        nfx_socket_close (skt);
                        }
                    else
                        {
                        diag_message (DIAG_NFX_EVENT, "Error testing for data received on socket %d: %s",
                            skt, SDL_GetError ());
                        }
                    }
                }
            }
        }
    }

byte nfx_in (byte port)
    {
    byte b;
    nfx_poll ();
    switch (port)
        {
        case 0:
            b = nfx_reg[0];
            diag_message (DIAG_NFX_REG, "Read  nfx_reg[0000] = 0x%02X - Mode", b);
            break;
        case 1:
            b = nfx_addr >> 8;
            break;
        case 2:
            b = nfx_addr & 0xFF;
            break;
        case 3:
            b = nfx_reg[nfx_addr];
            if (( b != 0 ) || ( nfx_addr != R_IR ))
                diag_message (DIAG_NFX_REG, "Read  nfx_reg[%04X] = 0x%02X - %s", nfx_addr, b,
                    reg_info(nfx_addr));
            if ( nfx_reg[0] & 0x02 ) ++nfx_addr;
            break;
        }
    diag_message (DIAG_NFX_PORT, "NFX read: port = %d, value = 0x%02X", port, b);
    return b;
    }

static void nfx_initialise (void)
    {
    diag_message (DIAG_NFX_EVENT, "nfx_initialise");
    memset (nfx_sock, 0, sizeof (nfx_sock));
    memset (nfx_conn, 0, sizeof (nfx_conn));
    for (int i = 0; i < NFX_NSOCK; ++i)
        {
        nfx_sock[i].icon = -1;
        nfx_sock[i].iskt = NULL;
        nfx_conn[i].iskt = NULL;
        }
    nfx_common_out (R_RMSR, 0x55);
    nfx_common_out (R_TMSR, 0x55);
    nfx_init = TRUE;
    }
