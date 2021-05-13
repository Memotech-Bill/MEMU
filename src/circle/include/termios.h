/*  Dummy termios.h for compiling MEMU */

#ifdef SHOW_HDR
#warning Circle version of termios.h loaded
#endif

#ifndef H_TERMIOS
#define H_TERMIOS

#define TCSADRAIN   1
#define TCSAFLUSH   2

#define CS5         0x0000
#define CS6         0x0001
#define CS7         0x0002
#define CS8         0x0003
#define CSIZE       0x0003
#define CSTOPB      0x0008
#define PARENB      0x0010

#define ICRNL       0x0001
#define INLCR       0x0002
#define INLRET      0x0004
#define INOCR       0x0008
#define IGNBRK      0x0010
#define BRKINS      0x0020
#define PARMRK      0x0040
#define INPCH       0x0080
#define ISTRIP      0x0100
#define IXON        0x0200

#define OCRNL       0x0001
#define ONLCR       0x0002
#define ONLRET      0x0004
#define ONOCR       0x0008
#define OFILL       0x0010
#define OLCUC       0x0020
#define OPOST       0x0040

#define ECHO        0x0001
#define ECHONL      0x0002
#define ICANON      0x0004
#define IEXTN       0x0008
#define ISIG        0x0010

#define VMIN        0
#define VTIME       1
#define NCCS        2

#define B50         50
#define B75         75
#define B110        110
#define B134        134
#define B150        150
#define B200        200
#define B300        300
#define B600        600
#define B1200       1200
#define B1800       1800
#define B2400       2400
#define B4800       4800
#define B9600       9600
#define B19200      19200
#define B38400      38400
#define B57600      57600
#define B115200     115200
#define B230400     230400

struct termios
    {
    int c_iflag;    /* input modes */
    int c_oflag;    /* output modes */
    int c_cflag;    /* control modes */
    int c_lflag;    /* local modes */
    int c_cc[NCCS]; /* special characters */
    int baud;       /* Baud rate (not standard) */
    };

#ifdef __cplusplus
extern "C"
    {
#endif

int tcgetattr (int fd, struct termios *termios_p);
int tcsetattr (int fd, int optional_actions, const struct termios *termios_p);
int tcsendbreak (int fd, int duration);
int tcdrain (int fd);
int tcflush (int fd, int queue_selector);
int cfsetspeed (struct termios *termios_p, speed_t speed);    
#ifdef __cplusplus
    }
#endif

#endif
