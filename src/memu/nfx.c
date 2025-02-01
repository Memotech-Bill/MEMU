/* nfx.c - Emulation of the NFX Wiznet Interface

   The first cross-platform version of this (nfx_sdl.c) used SDL2_net.
   Sadly SDL3_net is broken for this application, no way of specifying
   numeric IP address (only convert them to string and then do DNS
   lookup).

   Instead, use ideas to make this code cross-platform, from:
   https://gist.github.com/DanielGibson/bf6bd299c50c1ac1aff4cd063472cbe4
*/

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#include <winsock2.h>
#define BOOLEAN NFX_BOOL
#else
#include <sys/socket.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#endif
#include "nfx.h"
#include "diag.h"
#include "common.h"
#include "types.h"

// Cross-platform network mappings

#if defined(_WIN32)

typedef int             socklen_t;
typedef SSIZE_T         ssize_t;
#define net_errno       WSAGetLastError ()
#define SOCK_NONBLOCK   0x4000
#define NET_EINPROGRESS WSAEINPROGRESS
#define NET_EAGAIN      WSAEWOULDBLOCK
#define NET_EWOULDBLOCK WSAEWOULDBLOCK

static inline SOCKET socket3 (int af, int type, int protocol)
    {
    SOCKET sock = socket (af, type & 0xFF, protocol);
    if ((sock != INVALID_SOCKET) && (type & SOCK_NONBLOCK))
        {
        unsigned long mode = 1;
        ioctlsocket (sock, FIONBIO, &mode);
        }
    return sock;
    }

static inline SOCKET accept4 (SOCKET sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
    {
    SOCKET sock = accept (sockfd, addr, addrlen);
    if ((sock != INVALID_SOCKET) && (flags == SOCK_NONBLOCK))
        {
        unsigned long mode = 1;
        ioctlsocket (sock, FIONBIO, &mode);
        }
    return sock;
    }

static inline int readsocket (SOCKET sock, char * buff, int len)
    {
    return recv (sock, buff, len, 0);
    }

static inline int writesocket (SOCKET sock, const char * buff, int len)
    {
    return send (sock, buff, len, 0);
    }

const char* net_strerror( int errorCode )
    {
    // Descriptions from http://tangentsoft.net/wskfaq/examples/basics/ws-util.cpp
    // which is placed in the Public Domain

    switch( errorCode )
	{
	case 0:	return "NO ERROR";
	case WSAEINTR: return "Interrupted system call";
	case WSAEBADF: return "Bad file number";
	case WSAEACCES: return "Permission denied";
	case WSAEFAULT: return "Bad address";
	case WSAEINVAL: return "Invalid argument";
	case WSAEMFILE: return "Too many open sockets";
	case WSAEWOULDBLOCK: return "Operation would block";
	case WSAEINPROGRESS: return "Operation now in progress";
	case WSAEALREADY: return "Operation already in progress";
	case WSAENOTSOCK: return "Socket operation on non-socket";
	case WSAEDESTADDRREQ: return "Destination address required";
	case WSAEMSGSIZE: return "Message too long";
	case WSAEPROTOTYPE: return "Protocol wrong type for socket";
	case WSAENOPROTOOPT: return "Bad protocol option";
	case WSAEPROTONOSUPPORT: return "Protocol not supported";
	case WSAESOCKTNOSUPPORT: return "Socket type not supported";
	case WSAEOPNOTSUPP: return "Operation not supported on socket";
	case WSAEPFNOSUPPORT: return "Protocol family not supported";
	case WSAEAFNOSUPPORT: return "Address family not supported";
	case WSAEADDRINUSE: return "Address already in use";
	case WSAEADDRNOTAVAIL: return "Can't assign requested address";
	case WSAENETDOWN: return "Network is down";
	case WSAENETUNREACH: return "Network is unreachable";
	case WSAENETRESET: return "Net connection reset";
	case WSAECONNABORTED: return "Software caused connection abort";
	case WSAECONNRESET: return "Connection reset by peer";
	case WSAENOBUFS: return "No buffer space available";
	case WSAEISCONN: return "Socket is already connected";
	case WSAENOTCONN: return "Socket is not connected";
	case WSAESHUTDOWN: return "Can't send after socket shutdown";
	case WSAETOOMANYREFS: return "Too many references, can't splice";
	case WSAETIMEDOUT: return "Connection timed out";
	case WSAECONNREFUSED: return "Connection refused";
	case WSAELOOP: return "Too many levels of symbolic links";
	case WSAENAMETOOLONG: return "File name too long";
	case WSAEHOSTDOWN: return "Host is down";
	case WSAEHOSTUNREACH: return "No route to host";
	case WSAENOTEMPTY: return "Directory not empty";
	case WSAEUSERS: return "Too many users";
	case WSAEDQUOT: return "Disc quota exceeded";
	case WSAESTALE: return "Stale NFS file handle";
	case WSAEREMOTE: return "Too many levels of remote in path";
	case WSAEPROCLIM: return "Too many processes using WinSock";
	case WSAEDISCON: return "Remote host has shut the connection down";
	case WSAENOMORE: return "No more results";
	case WSAECANCELLED: return "Call has been canceled";
	case WSAEINVALIDPROCTABLE: return "Service provider procedure table is invalid";
	case WSAEINVALIDPROVIDER: return "Service provider is invalid";
	case WSAEPROVIDERFAILEDINIT: return "Service provider failed to initialize";
	case WSASYSCALLFAILURE: return "System call has failed, even though it shouldn't";
	case WSASERVICE_NOT_FOUND: return "Service not found";
	case WSATYPE_NOT_FOUND: return "Class type not found";
	case WSAEREFUSED: return "Database query was refused"; // whatever this means
	case WSANO_DATA: return "The requested name is valid, but no data of the requested type was found";
	default: return "An errorcode that's not supported/used on the current platform (or so I thought..)";
	}
    }

#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#define INVALID_SOCKET  -1
#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK   0x4000
#endif
#define NET_EINPROGRESS EINPROGRESS
#define NET_EAGAIN      EAGAIN
#define NET_EWOULDBLOCK EWOULDBLOCK

#define readsocket      read
#define writesocket     write
#define closesocket     close
#define net_strerror    strerror
#define net_errno       errno

static inline SOCKET socket3 (int af, int type, int protocol)
    {
    SOCKET sock = socket (af, type & 0xFF, protocol);
    if ((sock != INVALID_SOCKET) && (type & SOCK_NONBLOCK))
        {
        int iflg = fcntl (sock, F_GETFL);
        fcntl (sock, F_SETFL, iflg | O_NONBLOCK);
        }
    return sock;
    }

static inline SOCKET accept4 (SOCKET sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags)
    {
    SOCKET sock = accept (sockfd, addr, addrlen);
    if ((sock != INVALID_SOCKET) && (flags == SOCK_NONBLOCK))
        {
        int iflg = fcntl (sock, F_GETFL);
        fcntl (sock, F_SETFL, iflg | O_NONBLOCK);
        }
    return sock;
    }

#else
#error Unsupported Apple device
#endif

#else
#define INVALID_SOCKET  -1
#define NET_EINPROGRESS EINPROGRESS
#define NET_EAGAIN      EAGAIN
#define NET_EWOULDBLOCK EWOULDBLOCK
typedef int SOCKET;
#define socket3         socket
#define readsocket      read
#define writesocket     write
#define closesocket     close
#define net_strerror    strerror
#define net_errno       errno
#endif

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
    SOCKET iskt;    // Socket stream number
    }
    nfx_conn[NFX_NSOCK];

static struct   // Data for each NFX socket
    {
    int icon;       // Linux connection number for this NFX socket
    SOCKET iskt;    // Socket stream number for established connection
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
    if ( nfx_sock[skt].iskt != INVALID_SOCKET )
        {
        closesocket (nfx_sock[skt].iskt);
        nfx_sock[skt].iskt = INVALID_SOCKET;
        }
    int icon = nfx_sock[skt].icon;
    if ( icon >= 0 )
        {
        // diag_message (DIAG_NFX_EVENT, "Close socket %d: icon = %d ncon = %d", skt, icon, nfx_conn[icon].ncon);
        if ( --nfx_conn[icon].ncon == 0 )
            {
            closesocket (nfx_conn[icon].iskt);
            // diag_message (DIAG_NFX_EVENT, "Close connection %d", icon);
            nfx_conn[icon].iskt = INVALID_SOCKET;
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
        diag_message (DIAG_NFX_EVENT, "Socket %d Open TCP: iskt = %d", skt, nfx_sock[skt].iskt);
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
                diag_message (DIAG_NFX_EVENT, "Socket %d added to connection %d: nfx_conn.iskt = %d", skt, i, nfx_conn[i].iskt);
                nfx_sock_reg(skt, S_SR) = SOCK_LISTEN;
                return;
                }
            else if ( nfx_conn[i].ncon == 0 )
                {
                icon = i;
                break;
                }
            }
        if ( icon < 0 ) fatal ("No free connections for socket %d", skt);
        nfx_conn[icon].ncon = 1;
        nfx_conn[icon].proto = PROTO_TCP;
        nfx_conn[icon].port = port;
        nfx_sock[skt].icon = icon;
        nfx_conn[icon].iskt = socket3 (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_IP);
        if ( nfx_conn[icon].iskt == INVALID_SOCKET )
            {
            int iErr = net_errno;
            fatal ("Could not create LISTEN socket %d: %s", skt, net_strerror (iErr));
            }
        diag_message (DIAG_NFX_EVENT, "Using connection %d for socket %d: nfx_conn.iskt = %d", icon, skt, nfx_conn[icon].iskt);
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = INADDR_ANY;
        uint16_t oport = port;
        if (oport < 1024) oport += nfx_offset;
        sa.sin_port = htons (oport);
        if ( bind (nfx_conn[icon].iskt, (struct sockaddr *) &sa, sizeof (sa)) < 0 )
            {
            int iErr = net_errno;
            fatal ("Failed to bind socket for connection %d: %s", skt, net_strerror (iErr));
            }
        if ( listen (nfx_conn[icon].iskt, NFX_NSOCK) < 0 )
            {
            int iErr = net_errno;
            fatal ("Failed to listen on socket for connection %d: %s", skt, net_strerror (iErr));
            }
        nfx_sock_reg(skt, S_SR) = SOCK_LISTEN;
        break;
        }
        case SCR_CONNECT:
        {
        uint32_t addr = ( nfx_sock_reg(skt, S_DIPR) << 24 ) | ( nfx_sock_reg(skt, S_DIPR + 1) << 16 )
            | ( nfx_sock_reg(skt, S_DIPR + 2) << 8 ) | nfx_sock_reg(skt, S_DIPR + 3);
        uint16_t port = ( nfx_sock_reg(skt, S_DPORT) << 8 ) | nfx_sock_reg(skt, S_DPORT + 1);
        nfx_sock[skt].iskt = socket3 (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_IP);
        diag_message (DIAG_NFX_EVENT, "Socket %d Connect to %d.%d.%d.%d:%d: iskt = %d", skt,
            nfx_sock_reg(skt, S_DIPR), nfx_sock_reg(skt, S_DIPR + 1),
            nfx_sock_reg(skt, S_DIPR + 2), nfx_sock_reg(skt, S_DIPR + 3), port, nfx_sock[skt].iskt);
        if ( nfx_sock[skt].iskt == INVALID_SOCKET )
            {
            int iErr = net_errno;
            fatal ("Could not create CONNECT socket %d: %s", skt, net_strerror (iErr));
            }
        struct sockaddr_in sa;
        sa.sin_family = AF_INET;
        sa.sin_addr.s_addr = htonl (addr);
        sa.sin_port = htons (port);
        if ( connect (nfx_sock[skt].iskt, (struct sockaddr *) &sa, sizeof (sa)) < 0 )
            {
            int iErr = net_errno;
            if ( iErr != NET_EINPROGRESS )
                fatal ("Failed to connect socket %d: Error %d: %s", skt, iErr, net_strerror (iErr));
            }
        if ( !(nfx_sock_reg(skt, S_IR) & SIR_CON) )
            diag_message (DIAG_NFX_EVENT, "Socket %d: Raise CONNECT interrupt @1: iskt = %d", skt, nfx_sock[skt].iskt);
        nfx_sock_reg(skt, S_IR) |= SIR_CON;
        nfx_reg[R_IR] |= 1 << skt;
        nfx_sock_reg(skt, S_SR) = SOCK_ESTABLISHED;
        break;
        }
        case SCR_DISCON:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Disconnect: iskt = %d", skt, nfx_sock[skt].iskt);
        nfx_socket_close (skt);
        if ( !(nfx_sock_reg(skt, S_IR) & SIR_DISCON) )
            diag_message (DIAG_NFX_EVENT, "Socket %d: Raise DISCONNECT interrupt", skt);
        nfx_sock_reg(skt, S_IR) |= SIR_DISCON;
        nfx_reg[R_IR] |= 1 << skt;
        break;
        }
        case SCR_CLOSE:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Close: iskt = %d", skt, nfx_sock[skt].iskt);
        nfx_socket_close (skt);
        break;
        }
        case SCR_SEND:
        {
        nfx_sock[skt].txwr = ( nfx_sock_reg(skt, S_TX_WR) << 8 ) | nfx_sock_reg(skt, S_TX_WR + 1);
        int nsend = nfx_sock[skt].txwr - nfx_sock[skt].txrd;
        diag_message (DIAG_NFX_EVENT, "Socket %d Send %d bytes: iskt = %d", skt, nsend, nfx_sock[skt].iskt);
        if ( nsend > 0 )
            {
            int nbyte;
            if ( nfx_sock[skt].txwr <= nfx_sock[skt].txsize )
                {
                nbyte = writesocket (nfx_sock[skt].iskt, &nfx_reg[nfx_sock[skt].txbase + nfx_sock[skt].txrd], nsend);
                if ( diag_flags[DIAG_NFX_DATA] )
                    nfx_show_data (&nfx_reg[nfx_sock[skt].txbase + nfx_sock[skt].txrd], nsend);
                }
            else
                {
                int ncopy = nfx_sock[skt].txsize - nfx_sock[skt].txrd;
                // diag_message (DIAG_NFX_EVENT, "ncopy = %d", ncopy);
                memcpy (nfx_data, &nfx_reg[nfx_sock[skt].txbase + nfx_sock[skt].txrd], ncopy);
                memcpy (&nfx_data[ncopy],  &nfx_reg[nfx_sock[skt].txbase], nsend - ncopy);
                nbyte = writesocket (nfx_sock[skt].iskt, nfx_data, nsend);
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
                    diag_message (DIAG_NFX_EVENT, "Socket %d: Raise SEND_OK interrupt: iskt = %d", skt, nfx_sock[skt].iskt);
                nfx_sock_reg(skt, S_IR) |= SIR_SEND_OK;
                nfx_reg[R_IR] |= 1 << skt;
                }
            else
                {
		int iErr = net_errno;
                diag_message (DIAG_NFX_EVENT, "Error %d sending data on socket %d: iskt = %d %s",
		    iErr, skt, nfx_sock[skt].iskt, net_strerror (iErr));
                }
            }
        break;
        }
        case SCR_SEND_KEEP:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Send Keep: iskt = %d", skt, nfx_sock[skt].iskt);
        break;
        }
        case SCR_RECV:
        {
        diag_message (DIAG_NFX_EVENT, "Socket %d Receive: iskt = %d", skt, nfx_sock[skt].iskt);
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
                struct sockaddr_in sa;
                socklen_t slen = sizeof (sa);
                nfx_sock[skt].iskt = accept4 (nfx_conn[icon].iskt, (struct sockaddr *) &sa, &slen, SOCK_NONBLOCK);
                int iErr = net_errno;
                if ( nfx_sock[skt].iskt != INVALID_SOCKET )
                    {
                    uint32_t addr = ntohl (sa.sin_addr.s_addr);
                    nfx_sock_reg(skt, S_DIPR)     = ( addr >> 24 ) & 0xFF;
                    nfx_sock_reg(skt, S_DIPR + 1) = ( addr >> 16 ) & 0xFF;
                    nfx_sock_reg(skt, S_DIPR + 2) = ( addr >> 8 ) & 0xFF;
                    nfx_sock_reg(skt, S_DIPR + 3) = addr & 0xFF;
                    uint16_t port = ntohs (sa.sin_port);
                    nfx_sock_reg(skt, S_DPORT)     = ( port >> 8 ) & 0xFF;
                    nfx_sock_reg(skt, S_DPORT + 1) = port & 0xFF;
                    if ( !(nfx_sock_reg(skt, S_IR) & SIR_CON) )
                        diag_message (DIAG_NFX_EVENT, "Socket %d: Raise CONNECT interrupt @2: iskt = %d", skt, nfx_sock[skt].iskt);
                    nfx_sock_reg(skt, S_IR) |= SIR_CON;
                    nfx_reg[R_IR] |= 1 << skt;
                    nfx_sock_reg(skt, S_SR) = SOCK_ESTABLISHED;
                    slen = sizeof (sa);
                    getsockname (nfx_sock[skt].iskt, (struct sockaddr *) &sa, &slen);
                    addr = ntohl (sa.sin_addr.s_addr);
                    nfx_reg[R_SIPR]     = ( addr >> 24 ) & 0xFF;
                    nfx_reg[R_SIPR + 1] = ( addr >> 16 ) & 0xFF;
                    nfx_reg[R_SIPR + 2] = ( addr >> 8 ) & 0xFF;
                    nfx_reg[R_SIPR + 3] = addr & 0xFF;
                    diag_message (DIAG_NFX_EVENT,
                        "Socket %d connection from %d.%d.%d.%d port %d to %d.%d.%d.%d: iskt = %d",
                        skt, nfx_sock_reg(skt, S_DIPR), nfx_sock_reg(skt, S_DIPR + 1),
                        nfx_sock_reg(skt, S_DIPR + 2), nfx_sock_reg(skt, S_DIPR + 3), port,
                        nfx_reg[R_SIPR], nfx_reg[R_SIPR + 1], nfx_reg[R_SIPR + 2], nfx_reg[R_SIPR + 3],
			nfx_sock[skt].iskt);
                    }
                else if (( iErr != NET_EAGAIN ) && ( iErr != NET_EWOULDBLOCK ))
                    {
                    diag_message (DIAG_NFX_EVENT, "Error %d testing for connection on socket %d: nfx_conn.iskt = %d %s",
                        iErr, skt, nfx_conn[icon].iskt, net_strerror (iErr));
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
                    int nbyte = (int) readsocket (nfx_sock[skt].iskt, nfx_data, nfree);
		    int iErr = net_errno;
                    if ( nbyte > 0 )
                        {
                        diag_message (DIAG_NFX_EVENT, "Socket %d: Received %d bytes: iskt = %d", skt, nbyte, nfx_sock[skt].iskt);
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
                            diag_message (DIAG_NFX_EVENT, "Socket %d: Raise RECEIVED interrupt: iskt = %d", skt, nfx_sock[skt].iskt);
                        nfx_sock_reg(skt, S_IR) |= SIR_RECV;
                        nfx_reg[R_IR] |= 1 << skt;
                        }
                    else if ( nbyte == 0 )
                        {
                        diag_message (DIAG_NFX_EVENT, "Closed socket %d due to zero byte read: iskt = %d", skt, nfx_sock[skt].iskt);
                        // nfx_sock_reg(skt, S_SR) = SOCK_CLOSE_WAIT;
                        // nfx_sock_reg(skt, S_IR) |= SIR_DISCON;
                        // nfx_reg[R_IR] |= 1 << skt;
                        nfx_socket_close (skt);
                        }
                    else if (( iErr != NET_EAGAIN ) && ( iErr != NET_EWOULDBLOCK ))
                        {
                        diag_message (DIAG_NFX_EVENT, "Error %d testing for data received on socket %d: iskt = %d, %s",
                            iErr, skt, nfx_sock[skt].iskt, net_strerror (iErr));
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
#ifdef _WIN32
    WSADATA wsaData;
    int iErr = WSAStartup (0x202, &wsaData);
    if (iErr != 0) fatal ("Failed to initialise networking");
#endif
    diag_message (DIAG_NFX_EVENT, "nfx_initialise");
    memset (nfx_sock, 0, sizeof (nfx_sock));
    memset (nfx_conn, 0, sizeof (nfx_conn));
    for (int i = 0; i < NFX_NSOCK; ++i)
        {
        nfx_sock[i].icon = -1;
        nfx_sock[i].iskt = INVALID_SOCKET;
        nfx_conn[i].iskt = INVALID_SOCKET;
        }
    nfx_common_out (R_RMSR, 0x55);
    nfx_common_out (R_TMSR, 0x55);
    nfx_init = TRUE;
    }
