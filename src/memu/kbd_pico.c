/* kbd_pico.c - Keyboard interface routines for Pico */

#include "tusb.h"
#include "class/hid/hid.h"
#include "types.h"
#include "win.h"
#include "diag.h"
#include "vid.h"
#include "mon.h"
#include "memu.h"
#include "kbd.h"

#define NKEYMODE    8
#define KMD_SHIFT   0x01
#define KMD_SCRLK   0x02
#define KMD_NUMLK   0x04

#define SHIFT_ROW       6
#define SHIFT_BITPOS_L  0
#define SHIFT_BITPOS_R  6
#define SHIFT_BIT_L     ( 1 << SHIFT_BITPOS_L )
#define SHIFT_BIT_R     ( 1 << SHIFT_BITPOS_R )
#define SHIFT_BITS      ( SHIFT_BIT_L | SHIFT_BIT_R )

static KeyHandler handle_press = NULL;
static KeyHandler handle_release = NULL;

/*
  MTX matrix positions corresponding to key presses.
  High nibble (0-7) is drive line
  Low nibble (0-9) is sense line
  Special codes:
*/
#define NKEY    0xFF    // Null key
#define RST1    0xE1    // First reset key
#define RST2    0xE2    // Second reset key
#define NUML    0xD1    // Num lock
#define SCRL    0xD4    // Scroll lock
#define DIAK    0xD8    // Diagnostics key

struct s_keyinfo
    {
    byte    press[NKEYMODE];
    word    wk[2];
    };

/* The following entries must be in iKey order - binary search is used */
static const struct s_keyinfo keyinfo[] =
    {
    // 0x04 - Keyboard a and A                                                 
    {{0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50},{'a', 'A'}},
    // 0x05 - Keyboard b and B                                                 
    {{0x72,0x72,0x72,0x72,0x72,0x72,0x72,0x72},{'b', 'B'}},
    // 0x06 - Keyboard c and C                                                 
    {{0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71},{'c', 'C'}},
    // 0x07 - Keyboard d and D                                                 
    {{0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51},{'d', 'D'}},
    // 0x08 - Keyboard e and E                                                 
    {{0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31},{'e', 'E'}},
    // 0x09 - Keyboard f and F                                                 
    {{0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42},{'f', 'F'}},
    // 0x0A - Keyboard g and G                                                 
    {{0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52},{'g', 'G'}},
    // 0x0B - Keyboard h and H                                                 
    {{0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43},{'h', 'H'}},
    // 0x0C - Keyboard i and I                                                 
    {{0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24},{'i', 'I'}},
    // 0x0D - Keyboard j and J                                                 
    {{0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x53},{'j', 'J'}},
    // 0x0E - Keyboard k and K                                                 
    {{0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44},{'k', 'K'}},
    // 0x0F - Keyboard l and L                                                 
    {{0x54,0x54,0x54,0x54,0x54,0x54,0x54,0x54},{'l', 'L'}},
    // 0x10 - Keyboard m and M                                                 
    {{0x73,0x73,0x73,0x73,0x73,0x73,0x73,0x73},{'m', 'M'}},
    // 0x11 - Keyboard n and N                                                 
    {{0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63},{'n', 'N'}},
    // 0x12 - Keyboard o and O                                                 
    {{0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34},{'o', 'O'}},
    // 0x13 - Keyboard p and P                                                 
    {{0x25,0x25,0x25,0x25,0x25,0x25,0x25,0x25},{'p', 'P'}},
    // 0x14 - Keyboard q and Q                                                 
    {{0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30},{'q', 'Q'}},
    // 0x15 - Keyboard r and R                                                 
    {{0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22},{'r', 'R'}},
    // 0x16 - Keyboard s and S                                                 
    {{0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41},{'s', 'S'}},
    // 0x17 - Keyboard t and T                                                 
    {{0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32},{'t', 'T'}},
    // 0x18 - Keyboard u and U                                                 
    {{0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33},{'u', 'U'}},
    // 0x19 - Keyboard v and V                                                 
    {{0x62,0x62,0x62,0x62,0x62,0x62,0x62,0x62},{'v', 'V'}},
    // 0x1A - Keyboard w and W                                                 
    {{0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21},{'w', 'W'}},
    // 0x1B - Keyboard x and X                                                 
    {{0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61},{'x', 'X'}},
    // 0x1C - Keyboard y and Y                                                 
    {{0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23},{'y', 'Y'}},
    // 0x1D - Keyboard z and Z                                                 
    {{0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70},{'z', 'Z'}},
    // 0x1E - Keyboard 1 and !                                                 
    {{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{'1', '!'}},
    // 0x1F - Keyboard 2 and "                                                 
    {{0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11},{'2', '"'}},
    // 0x20 - Keyboard 3 and £                                                 
    {{0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},{'3', '#'}},
    // 0x21 - Keyboard 4 and $                                                 
    {{0x12,0x12,0x12,0x12,0x12,0x12,0x12,0x12},{'4', '$'}},
    // 0x22 - Keyboard 5 and %                                                 
    {{0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02},{'5', '%'}},
    // 0x23 - Keyboard 6 and ^                                                 
    {{0x13,0x05,0x13,0x48,0x13,0x05,0x13,0x48},{'6', '^'}},
    // 0x24 - Keyboard 7 and &                                                 
    {{0x03,0x13,0x03,0x13,0x03,0x13,0x03,0x13},{'7', '&'}},
    // 0x25 - Keyboard 8 and *                                                 
    {{0x14,0x55,0x14,0x55,0x14,0x55,0x14,0x55},{'8', '*'}},
    // 0x26 - Keyboard 9 and (                                                 
    {{0x04,0x14,0x04,0x14,0x04,0x14,0x04,0x14},{'9', '('}},
    // 0x27 - Keyboard 0 and )                                                 
    {{0x15,0x04,0x15,0x04,0x15,0x04,0x15,0x04},{'0', ')'}},
    // 0x28 - Keyboard Return (ENTER)                                          
    {{0x56,0x56,0x56,0x56,0x56,0x56,0x56,0x56},{WK_Return, WK_Return}},
    // 0x29 - Keyboard ESCAPE                                                  
    {{0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},{WK_Escape, WK_Escape}},
    // 0x2A - Backspace                                                        
    {{0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},{WK_BackSpace, WK_BackSpace}},
    // 0x2B - Keyboard Tab                                                     
    {{0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28},{WK_Tab, WK_Tab}},
    // 0x2C - Keyboard Spacebar                                                
    {{0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78},{' ', ' '}},
    // 0x2D - Keyboard - and _                                                 
    {{0x05,0x75,0x05,0x75,0x05,0x75,0x05,0x75},{'-', '_'}},
    // 0x2E - Keyboard = and +                                                 
    {{0x16,0x45,0x48,0x45,0x16,0x45,0x48,0x45},{'=', '+'}},
    // 0x2F - Keyboard [ and {                                                 
    {{0x26,0x26,0x26,0x26,0x26,0x26,0x26,0x26},{'[', '{'}},
    // 0x30 - Keyboard ] and }                                                 
    {{0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46},{']', '}'}},
    // 0x31 - Keyboard \ and |                                                 
    {{0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06},{'\\', '|'}},
    // 0x32 - Keyboard # and ~                                                 
    {{0x55,0x16,0x68,0x16,0x55,0x16,0x68,0x16},{'#', '~'}},
    // 0x33 - Keyboard ; and :                                                 
    {{0x45,NKEY,0x45,0x68,0x45,NKEY,0x45,0x68},{';', ':'}},
    // 0x34 - Keyboard ' and @                                                 
    {{0x35,0x03,0x58,0x58,0x35,0x03,0x58,0x58},{'\'', '@'}},
    // 0x35 - Keyboard ` and ~                                                 
    {{NKEY,0x35,NKEY,0x35,NKEY,0x35,NKEY,0x35},{'`', '~'}},
    // 0x36 - Keyboard , and <                                                 
    {{0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64},{',', '<'}},
    // 0x37 - Keyboard . and >                                                 
    {{0x74,0x74,0x74,0x74,0x74,0x74,0x74,0x74},{'.', '>'}},
    // 0x38 - Keyboard / and ?                                                 
    {{0x65,0x65,0x65,0x65,0x65,0x65,0x65,0x65},{'/', '?'}},
    // 0x39 - Keyboard Caps Lock                                               
    {{0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40},{WK_Caps_Lock, WK_Caps_Lock}},
    // 0x3A - Keyboard F1                                                      
    {{0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09},{WK_F1, WK_F1}},
    // 0x3B - Keyboard F2                                                      
    {{0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29},{WK_F2, WK_F2}},
    // 0x3C - Keyboard F3                                                      
    {{0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59},{WK_F3, WK_F3}},
    // 0x3D - Keyboard F4                                                      
    {{0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79},{WK_F4, WK_F4}},
    // 0x3E - Keyboard F5                                                      
    {{0x19,0x19,0x19,0x19,0x19,0x19,0x19,0x19},{WK_F5, WK_F5}},
    // 0x3F - Keyboard F6                                                      
    {{0x39,0x39,0x39,0x39,0x39,0x39,0x39,0x39},{WK_F6, WK_F6}},
    // 0x40 - Keyboard F7                                                      
    {{0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49},{WK_F7, WK_F7}},
    // 0x41 - Keyboard F8                                                      
    {{0x69,0x69,0x69,0x69,0x69,0x69,0x69,0x69},{WK_F8, WK_F8}},
    // 0x42 - Keyboard F9 - MTX Line Feed                                      
    {{0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36},{WK_F9, WK_F9}},
    // 0x43 - Keyboard F10 - Diagnostics toggle                                
    {{DIAK,DIAK,DIAK,DIAK,DIAK,DIAK,DIAK,DIAK},{WK_F10, WK_F10}},
    // 0x44 - Keyboard F11 - Open VDEB                                         
    {{NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},{WK_F11, WK_F11}},
    // 0x45 - Keyboard F12 - NumLock rep                                       
    {{NUML,NUML,NUML,NUML,NUML,NUML,NUML,NUML},{WK_F12, WK_F12}},
    // 0x46 - Keyboard Print Screen and Sys Request
    {{NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},{WK_Sys_Req, WK_Sys_Req}},
    // 0x47 - Keyboard Scroll Lock                                             
    {{SCRL,SCRL,SCRL,SCRL,SCRL,SCRL,SCRL,SCRL},{WK_Scroll_Lock, WK_Scroll_Lock}},
    // 0x48 - Keyboard Pause                                                   
    {{0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08},{WK_Pause, WK_Pause}},
    // 0x49 - Keyboard Insert                                                  
    {{0x76,0x76,0x76,0x76,0x76,0x76,0x76,0x76},{WK_Insert, WK_Insert}},
    // 0x4A - Keyboard Home                                                    
    {{0x57,0x57,0x57,0x57,0x57,0x57,0x57,0x57},{WK_Home, WK_Home}},
    // 0x4B - Keyboard Page Up                                                 
    {{0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07},{WK_Page_Up, WK_Page_Up}},
    // 0x4C - Keyboard Delete Forward                                          
    {{0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38},{WK_Delete, WK_Delete}},
    // 0x4D - Keyboard End                                                     
    {{0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17},{WK_End, WK_End}},
    // 0x4E - Keyboard Page Down                                               
    {{0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77},{WK_Page_Down, WK_Page_Down}},
    // 0x4F - Keyboard Right Arrow                                             
    {{0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47},{WK_Right, WK_Right}},
    // 0x50 - Keyboard Left Arrow                                              
    {{0x37,0x37,0x37,0x37,0x37,0x37,0x37,0x37},{WK_Left, WK_Left}},
    // 0x51 - Keyboard Down Arrow                                              
    {{0x67,0x67,0x67,0x67,0x67,0x67,0x67,0x67},{WK_Down, WK_Down}},
    // 0x52 - Keyboard Up Arrow                                                
    {{0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27},{WK_Up, WK_Up}},
    // 0x53 - Keyboard Num Lock and Clear                                      
    {{0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07},{WK_Num_Lock, WK_Num_Lock}},
    // 0x54 - Keypad /                                                         
    {{0x17,0x17,0x17,0x17,0x65,NKEY,0x65,NKEY},{WK_KP_Divide, WK_KP_Divide}},
    // 0x55 - Keypad *                                                         
    {{0x08,0x08,0x08,0x08,NKEY,0x55,NKEY,0x55},{WK_KP_Multiply, WK_KP_Multiply}},
    // 0x56 - Keypad -                                                         
    {{NKEY,NKEY,NKEY,NKEY,0x05,NKEY,0x05,NKEY},{WK_KP_Subtract, WK_KP_Subtract}},
    // 0x57 - Keypad +                                                         
    {{NKEY,NKEY,NKEY,NKEY,NKEY,0x45,NKEY,0x45},{WK_KP_Add, WK_KP_Add}},
    // 0x58 - Keypad Enter                                                     
    {{NKEY,NKEY,NKEY,NKEY,0x56,0x56,0x56,0x56},{WK_KP_Enter, WK_KP_Enter}},
    // 0x59 - Keypad 1 and End                                                 
    {{0x76,0x76,0x76,0x76,0x17,0x37,0x17,0x37},{WK_KP_End, '1'}},
    // 0x5A - Keypad 2 and Down Arrow                                          
    {{0x67,0x67,0x67,0x67,0x67,0x57,0x67,0x57},{WK_KP_Down, '2'}},
    // 0x5B - Keypad 3 and PageDn                                              
    {{0x77,0x77,0x77,0x77,0x77,0x47,0x77,0x47},{WK_KP_Page_Down, '3'}},
    // 0x5C - Keypad 4 and Left Arrow                                          
    {{0x37,0x37,0x37,0x37,0x37,0x28,0x37,0x28},{WK_KP_Left, '4'}},
    // 0x5D - Keypad 5                                                         
    {{0x57,0x57,0x57,0x57,0x57,0x27,0x57,0x27},{WK_KP_Middle, '5'}},
    // 0x5E - Keypad 6 and Right Arrow                                         
    {{0x47,0x47,0x47,0x47,0x47,0x38,0x47,0x38},{WK_KP_Right, '6'}},
    // 0x5F - Keypad 7 and Home                                                
    {{0x28,0x28,0x28,0x28,0x57,0x07,0x57,0x07},{WK_KP_Home, '7'}},
    // 0x60 - Keypad 8 and Up Arrow                                            
    {{0x27,0x27,0x27,0x27,0x27,0x17,0x27,0x17},{WK_KP_Up, '8'}},
    // 0x61 - Keypad 9 and Page Up                                             
    {{0x38,0x38,0x38,0x38,0x07,0x08,0x07,0x08},{WK_KP_Page_Up, '9'}},
    // 0x62 - Keypad 0 and Insert                                              
    {{0x76,0x76,0x76,0x76,0x76,0x76,0x76,0x76},{WK_Insert, '0'}},
    // 0x63 - Keypad . and Delete                                              
    {{0x38,0x38,0x38,0x38,0x38,0x67,0x38,0x67},{WK_Delete, '.'}},
    // 0x64 - Keyboard Non-US \ and |                                          
    {{0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06},{'\\', '|'}},
    // 0x65 - Keyboard Application                                             
    {{NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},{WK_PC_Menu, WK_PC_Menu}}
    };

static const struct s_keyinfo metainfo[] =
    {    
    // 0xE0 - Left Control
    {{0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20},{WK_Control_L, WK_Control_L}},
    // 0xE1 - Left Shift
    {{0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60},{WK_Shift_L, WK_Shift_L}},
    // 0xE2 - Left Alt
    {{RST1,RST1,RST1,RST1,RST1,RST1,RST1,RST1},{WK_PC_Alt_L, WK_PC_Alt_L}},
    // 0xE3 - Left Meta
    {{NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},{WK_PC_Windows_L, WK_PC_Windows_L}},
    // 0xE4 - Right Control
    {{0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20},{WK_Control_R, WK_Control_R}},
    // 0xE5 - Right Shift
    {{0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66},{WK_Shift_R, WK_Shift_R}},
    // 0xE6 - Right Alt
    {{RST2,RST2,RST2,RST2,RST2,RST2,RST2,RST2},{WK_PC_Alt_R, WK_PC_Alt_R}},
    // 0xE7 - Right Meta
    {{NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},{WK_PC_Windows_R, WK_PC_Windows_R}}
    };

#define countof(x)  (sizeof(x) / sizeof(x[0]))

static byte keyrel[countof(keyinfo)] = {NKEY};
static byte metarel[countof(metainfo)] = {NKEY};

static uint8_t kbd_addr = 0;

static struct st_keyevt
    {
    int     hk;
    BOOLEAN bPress;
    } key_queue[20];    // 8 modifier keys + 6 keys pressed and 6 released

static int n_key_queue = 0;

static int kbd_emu = 0;
static int kbd_mode = 0;

static int kbd_drive;
static word kbd_sense[8];

static byte rst_keys = 0;
BOOLEAN kbd_diag = FALSE;

#define LEN_LOG   80
static char sLogType[LEN_LOG+1];
static int nLogType = 0;

static void kbd_log (int key)
    {
    if ( diag_flags[DIAG_LOG_TYPE] )
        {
        if ( ( key >= 0x20 ) && ( key <= 0x7E ) )  sLogType[nLogType++] = (char) key;
        if ( ( nLogType >= LEN_LOG ) || ( key == WK_Return ) )
            {
            sLogType[nLogType] = '\0';
            diag_message (DIAG_ALWAYS, "Type in: %s", sLogType);
            nLogType = 0;
            }
        }
    }

static uint8_t led_flags = 0;

static void set_leds (uint8_t leds)
    {
    led_flags = leds;
    diag_message (DIAG_KBD_WIN_KEY, "set_leds: kbd_addr = %d, leds = 0x%02X", kbd_addr, leds);

    if ( kbd_addr > 0 )
        {
        tusb_control_request_t ledreq = {
            .bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE,
            .bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS,
            .bmRequestType_bit.direction = TUSB_DIR_OUT,
            .bRequest = HID_REQ_CONTROL_SET_REPORT,
            .wValue = HID_REPORT_TYPE_OUTPUT << 8,
            .wIndex = 0,    // Interface number
            .wLength = sizeof (led_flags)
            };
    
        bool bRes = tuh_control_xfer (kbd_addr, &ledreq, &led_flags, NULL);
        }
    }

void kbd_chk_leds (int *mode)
    {
    }

void win_kbd_leds (BOOLEAN bCaps, BOOLEAN bNum, BOOLEAN bScroll)
    {
    uint8_t leds = 0;
    if ( bNum ) leds |= KEYBOARD_LED_NUMLOCK;
    if ( bCaps ) leds |= KEYBOARD_LED_CAPSLOCK;
    if ( bScroll ) leds |= KEYBOARD_LED_SCROLLLOCK;
    set_leds (leds);
    }

#ifdef ALT_KEYPRESS
extern BOOLEAN ALT_KEYPRESS (int wk);
#endif

void kbd_find (int hk, const struct s_keyinfo **ppki, int *pwk, byte **pprel)
    {
    *ppki = NULL;
    *pwk = -1;
    *pprel = NULL;
    
    if ( hk < 4 )
        {
        return;
        }
    else if ( hk < sizeof (keyinfo) / sizeof (keyinfo[0]) + 4 )
        {
        *ppki = &keyinfo[hk-4];
        *pwk = (*ppki)->wk[kbd_mode & KMD_SHIFT];
        *pprel = &keyrel[hk-4];
        }
    else if ( hk < HID_KEY_CONTROL_LEFT )
        {
        return;
        }
    else if ( hk <= HID_KEY_GUI_RIGHT )
        {
        *ppki = &metainfo[hk-HID_KEY_CONTROL_LEFT];
        *pwk = (*ppki)->wk[kbd_mode & KMD_SHIFT];
        *pprel = &metarel[hk-HID_KEY_CONTROL_LEFT];
        }
    return;
    }

void kbd_win_keypress (int hk)
    {
    const struct s_keyinfo *pki;
    int wk;
    byte *prel;
    kbd_find (hk, &pki, &wk, &prel);
#ifdef ALT_KEYPRESS
    if ( ALT_KEYPRESS(wk) )
        {
        diag_message (DIAG_INIT, "Config completed");
        return;
        }
#endif
    if ( handle_press )
        {
        handle_press (wk);
        return;
        }
    if ( kbd_diag )
        {
        diag_message (DIAG_KBD_WIN_KEY, "Diagnostics key pressed 0x%02X '%c'", wk,
            (( wk >= 0x20 ) && ( wk < 0x7F )) ? wk : '.');
        diag_control(wk);
        return;
        }
    diag_message (DIAG_KBD_WIN_KEY, "Key pressed 0x%02X '%c' pki = %p", wk,
        (( wk >= 0x20 ) && ( wk < 0x7F )) ? wk : '.', pki);
    if ( ( kbd_sense[SHIFT_ROW] & SHIFT_BITS ) != SHIFT_BITS )  kbd_mode |= KMD_SHIFT;
    else    kbd_mode &= ~ KMD_SHIFT;
    if ( rst_keys )
        {
        // At least one <Alt> key down
        if ( wk == WK_F1 )
            {
            vid_init (0, 1, 1);
            return;
            }
        else if ( wk == WK_F2 )
            {
            mon_show ();
            return;
            }
        }
    if ( pki != NULL )
        {
        byte kscan = pki->press[kbd_mode];
        *prel = kscan;
        diag_message (DIAG_KBD_WIN_KEY, "kbd_mode = %d kscan = 0x%02X", kbd_mode, kscan);
        switch ( kscan )
            {
            case NKEY:
                break;
            case RST1:
            case RST2:
                rst_keys |= kscan & 0x03;
                diag_message (DIAG_KBD_WIN_KEY, "rst_keys = 0x%02X", rst_keys);
                break;
            case NUML:
                if ( kbd_emu & KBDEMU_REMAP )
                    {
                    kbd_mode ^= KMD_NUMLK;
                    kbd_chk_leds (&kbd_mode);
                    win_kbd_leds (FALSE, kbd_mode & KMD_NUMLK, kbd_mode & KMD_SCRLK);
                    }
                break;
            case SCRL:
                if ( kbd_emu & KBDEMU_REMAP )
                    {
                    kbd_mode ^= KMD_SCRLK;
                    kbd_chk_leds (&kbd_mode);
                    win_kbd_leds (FALSE, kbd_mode & KMD_NUMLK, kbd_mode & KMD_SCRLK);
                    }
                break;
            case DIAK:
                kbd_diag = TRUE;
                break;
            default:
            {
            word sense = 1 << ( kscan & 0x0F );
            int drive = kscan >> 4;
            kbd_sense[drive] &= ~ sense;
            diag_message (DIAG_KBD_WIN_KEY, "kbd_sense[%d] = 0x%03X", drive, kbd_sense[drive]);
            kbd_log (wk);
            break;
            }
            }
        }
    }

#ifdef ALT_KEYRELEASE
extern BOOLEAN ALT_KEYRELEASE(int wk);
#endif

void kbd_win_keyrelease(int hk)
    {
    const struct s_keyinfo *pki;
    int wk;
    byte *prel;
    kbd_find (hk, &pki, &wk, &prel);
#ifdef ALT_KEYRELEASE
    if ( ALT_KEYRELEASE (wk) ) return;
#endif
    if ( handle_release )
        {
        handle_release (wk);
        return;
        }
    diag_message (DIAG_KBD_WIN_KEY, "Key released 0x%02X '%c' pki = %p", wk,
        (( wk >= 0x20 ) && ( wk < 0x7F )) ? wk : '.', pki);
    if ( pki != NULL )
        {
        byte kscan = *prel;
        *prel = NKEY;
        diag_message (DIAG_KBD_WIN_KEY, "kscan = 0x%02X", kscan);
        switch ( kscan )
            {
            case NKEY:
            case NUML:
            case SCRL:
                break;
            case RST1:
            case RST2:
                if ( rst_keys == 0x03 )
                    {
                    // Allow time to press keys for CFX mode selection
                    memu_reset ();
                    // sleep (2);
                    }
                rst_keys &= ~ (kscan & 0x03);
                diag_message (DIAG_KBD_WIN_KEY, "rst_keys = 0x%02X", rst_keys);
                break;
            case DIAK:
                kbd_diag = FALSE;
                break;
            default:
            {
            word sense = 1 << ( kscan & 0x0F );
            int drive = kscan >> 4;
            kbd_sense[drive] |= sense;
            diag_message (DIAG_KBD_WIN_KEY, "kbd_sense[%d] = 0x%03X", drive, kbd_sense[drive]);
            break;
            }
            }
        }
    }

/*...skbd_out5:0:*/
/* "Drive" */
void kbd_out5(byte val)
    {
    kbd_drive = val;
    }
/*...e*/
/*...skbd_in5:0:*/
/* "Sense1" */
#ifdef ALT_KBD_SENSE1
extern word ALT_KBD_SENSE1 (word drive);
#endif

byte kbd_in5(void)
    {
    int i;
    word result = 0x00ff;
    for ( i = 0; i < 8; i++ )
        if ( (kbd_drive&(1<<i)) == 0 )
            {
            result &= kbd_sense    [i];
            }
#ifdef ALT_KBD_SENSE1
    result &= ALT_KBD_SENSE1 (kbd_drive);
#endif
    diag_message(DIAG_KBD_SENSE, "kbd_in5 0x%02x returns 0x%02x", kbd_drive, result);
    return (byte) result;
    }
/*...e*/
/*...skbd_in6:0:*/
/* "Sense2", the two extra rows.
   D3 and D2 are two bits of the country code
   (00=English, 01=France, 02=German, 03=Swedish).
   Interestingly, the BASIC ROM has tables for English, then France,
   then German, then Swedish, then Denmark (commented out), then Spain!
   D7 to D4 are all zeros (per Claus Baekkel observation, but I suspect that
   on real hardware you get is what ever happens to be last on the bus. */
#ifdef ALT_KBD_SENSE2
extern word ALT_KBD_SENSE2(word drive);
#endif

byte kbd_in6(void)
    {
    int i;
    word result = 0x03ff;
    for ( i = 0; i < 8; i++ )
        if ( (kbd_drive&(1<<i)) == 0 )
            {
            result &= kbd_sense[i];
            }
    result >>= 8;
    result   |= ( kbd_emu & KBDEMU_COUNTRY );
#ifdef ALT_KBD_SENSE2
    result &= ALT_KBD_SENSE2 (kbd_drive);
#endif
    diag_message(DIAG_KBD_SENSE, "kbd_in6 0x%02x returns 0x%02x", kbd_drive, result);
    return (byte) result;
    }
/*...e*/

// look up new key in previous keys
static inline int find_key_in_report(hid_keyboard_report_t const *p_report, uint8_t keycode)
    {
    for (int i = 0; i < 6; i++)
        {
        if (p_report->keycode[i] == keycode) return i;
        }
    return -1;
    }

static inline void process_kbd_report(hid_keyboard_report_t const *p_new_report)
    {
    static hid_keyboard_report_t prev_report = {0, 0, {0}}; // previous report to check key released
    bool held[6];
    for (int i = 0; i < 6; ++i) held[i] = false;
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = p_new_report->keycode[i];
        if ( key )
            {
            int kr = find_key_in_report(&prev_report, key);
            if ( kr >= 0 )
                {
                held[kr] = true;
                }
            else
                {
                key_queue[n_key_queue].hk = key;
                key_queue[n_key_queue].bPress = TRUE;
                ++n_key_queue;
                }
            }
        }
    int new_mod = p_new_report->modifier;
    int old_mod = prev_report.modifier;
    int bit = 0x01;
    for (int i = 0; i < 8; ++i)
        {
        if ((new_mod & bit) && !(old_mod & bit))
            {
            key_queue[n_key_queue].hk = HID_KEY_CONTROL_LEFT + i;
            key_queue[n_key_queue].bPress = TRUE;
            ++n_key_queue;
            }
        bit <<= 1;
        }
    bit = 0x01;
    for (int i = 0; i < 8; ++i)
        {
        if (!(new_mod & bit) && (old_mod & bit))
            {
            key_queue[n_key_queue].hk = HID_KEY_CONTROL_LEFT + i;
            key_queue[n_key_queue].bPress = FALSE;
            ++n_key_queue;
            }
        bit <<= 1;
        }
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = prev_report.keycode[i];
        if (( ! held[i] ) && ( key ))
            {
            key_queue[n_key_queue].hk = key;
            key_queue[n_key_queue].bPress = FALSE;
            ++n_key_queue;
            }
        }
    prev_report = *p_new_report;
    }

CFG_TUSB_MEM_SECTION static hid_keyboard_report_t usb_keyboard_report;

void hid_task(void)
	{
    if ((kbd_addr > 0) && tuh_hid_keyboard_is_mounted(kbd_addr))
		{
        if (!tuh_hid_keyboard_is_busy(kbd_addr))
			{
            process_kbd_report(&usb_keyboard_report);
            tuh_hid_keyboard_get_report(kbd_addr, &usb_keyboard_report);
			}
		}
	}

void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr)
    {
    // application set-up
    kbd_addr = dev_addr;
    diag_message (DIAG_KBD_WIN_KEY, "A Keyboard device (address %d) is mounted", dev_addr);
    tuh_hid_keyboard_get_report(dev_addr, &usb_keyboard_report);
    kbd_chk_leds (&kbd_mode);
    win_kbd_leds (FALSE, kbd_mode & KMD_NUMLK, kbd_mode & KMD_SCRLK);
    }

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr)
    {
    // application tear-down
    diag_message (DIAG_KBD_WIN_KEY, "A Keyboard device (address %d) is unmounted", dev_addr);
    kbd_addr = 0;
    }

// invoked ISR context
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event)
    {
    (void) dev_addr;
    (void) event;
    }

void kbd_periodic (void)
    {
    if ( n_key_queue == 0 )
        {
        tuh_task();
        hid_task();
        }
    if ( n_key_queue > 0 )
        {
        --n_key_queue;
        if ( key_queue[n_key_queue].bPress ) kbd_win_keypress (key_queue[n_key_queue].hk);
        else kbd_win_keyrelease (key_queue[n_key_queue].hk);
        }
    }

void kbd_init(int emu)
    {
    kbd_emu = emu;
    kbd_mode = 0;
    // diag_flags[DIAG_KBD_WIN_KEY] = TRUE;

    for ( int i = 0; i < 8; i++ )
        {
        kbd_sense    [i] = 0xffff;
        }

    if ( kbd_emu & KBDEMU_REMAP )
        {
        kbd_mode = KMD_SCRLK;
        }
    else
        {
        kbd_mode = 0;
        }
    kbd_chk_leds (&kbd_mode);
    win_kbd_leds (FALSE, kbd_mode & KMD_NUMLK, kbd_mode & KMD_SCRLK);
    }

int kbd_get_emu (void)
    {
    return kbd_emu;
    }

void kbd_term(void)
    {
    }

void kbd_set_handlers (KeyHandler press, KeyHandler release)
    {
    handle_press = press;
    handle_release = release;
    }

int win_shifted_wk(int wk)
	{
    // Pico already returns shifted state
    return wk;
    }
