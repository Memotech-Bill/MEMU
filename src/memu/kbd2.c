/* kbd2.c - Alternate keyboard emulation providing features similar to propeller add-on */

#include "types.h"
#include "diag.h"
#include "common.h"
#include "memu.h"
#include "mem.h"
#include "roms.h"
#include "win.h"
#include "kbd.h"
#include "vdeb.h"
#ifndef WIN32
#include <unistd.h>
#endif

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
    int iKey;
    byte    press[NKEYMODE];
    byte    release;
    };

/* The following entries must be in iKey order - bisection search is used */

static struct s_keyinfo keyinfo[] =
    {
    {WK_BackSpace,      {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x18},NKEY}, // 0x08 - Backspace
    {WK_Tab,            {0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28},NKEY}, // 0x09 - Keyboard Tab
    {WK_Linefeed,       {0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36},NKEY}, // 0x0A - Line Feed
    {WK_Return,         {0x56,0x56,0x56,0x56,0x56,0x56,0x56,0x56},NKEY}, // 0x0D - Keyboard Return (ENTER)
    {WK_Escape,         {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},NKEY}, // 0x1B - Keyboard ESCAPE
    {' ',               {0x78,0x78,0x78,0x78,0x78,0x78,0x78,0x78},NKEY}, // 0x20 - Keyboard Spacebar
    {'!',               {NKEY,0x00,NKEY,0x00,NKEY,0x00,NKEY,0x00},NKEY}, // 0x21 - Keyboard 1 and !
    {'"',               {NKEY,0x11,NKEY,0x11,NKEY,0x11,NKEY,0x11},NKEY}, // 0x22 - Keyboard 2 and "
    {'#',               {0x55,0x16,0x68,0x16,0x55,0x16,0x68,0x16},NKEY}, // 0x23 - Keyboard # and ~
    {'$',               {NKEY,0x12,NKEY,0x12,NKEY,0x12,NKEY,0x12},NKEY}, // 0x24 - Keyboard 4 and $
    {'%',               {NKEY,0x02,NKEY,0x02,NKEY,0x02,NKEY,0x02},NKEY}, // 0x25 - Keyboard 5 and %
    {'&',               {NKEY,0x13,NKEY,0x13,NKEY,0x13,NKEY,0x13},NKEY}, // 0x26 - Keyboard 7 and &
    {'\'',              {0x35,0x03,0x58,0x58,0x35,0x03,0x58,0x58},NKEY}, // 0x27 - Keyboard ' and @
    {'(',               {NKEY,0x14,NKEY,0x14,NKEY,0x14,NKEY,0x14},NKEY}, // 0x28 - Keyboard 9 and (
    {')',               {NKEY,0x04,NKEY,0x04,NKEY,0x04,NKEY,0x04},NKEY}, // 0x29 - Keyboard 0 and )
    {'*',               {NKEY,0x55,NKEY,0x55,NKEY,0x55,NKEY,0x55},NKEY}, // 0x2A - Keyboard 8 and *
    {'+',               {NKEY,0x45,NKEY,0x45,NKEY,0x45,NKEY,0x45},NKEY}, // 0x2B - Keyboard = and +
    {',',               {0x64,0x64,0x64,0x64,0x64,0x64,0x64,0x64},NKEY}, // 0x2C - Keyboard , and <
    {'-',               {0x05,0x75,0x05,0x75,0x05,0x75,0x05,0x75},NKEY}, // 0x2D - Keyboard - and _
    {'.',               {0x74,0x74,0x74,0x74,0x74,0x74,0x74,0x74},NKEY}, // 0x2E - Keyboard . and >
    {'/',               {0x65,0x65,0x65,0x65,0x65,0x65,0x65,0x65},NKEY}, // 0x2F - Keyboard / and ?
    {'0',               {0x15,0x04,0x15,0x04,0x15,0x04,0x15,0x04},NKEY}, // 0x30 - Keyboard 0 and )
    {'1',               {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},NKEY}, // 0x31 - Keyboard 1 and !
    {'2',               {0x11,0x11,0x11,0x11,0x11,0x11,0x11,0x11},NKEY}, // 0x32 - Keyboard 2 and "
    {'3',               {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01},NKEY}, // 0x33 - Keyboard 3 and £
    {'4',               {0x12,0x12,0x12,0x12,0x12,0x12,0x12,0x12},NKEY}, // 0x34 - Keyboard 4 and ,0x
    {'5',               {0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02},NKEY}, // 0x35 - Keyboard 5 and %
    {'6',               {0x13,0x05,0x13,0x48,0x13,0x05,0x13,0x48},NKEY}, // 0x36 - Keyboard 6 and ^
    {'7',               {0x03,0x13,0x03,0x13,0x03,0x13,0x03,0x13},NKEY}, // 0x37 - Keyboard 7 and &
    {'8',               {0x14,0x55,0x14,0x55,0x14,0x55,0x14,0x55},NKEY}, // 0x38 - Keyboard 8 and *
    {'9',               {0x04,0x14,0x04,0x14,0x04,0x14,0x04,0x14},NKEY}, // 0x39 - Keyboard 9 and (
    {':',               {NKEY,NKEY,NKEY,0x68,NKEY,NKEY,NKEY,0x68},NKEY}, // 0x3A - Keyboard ; and :
    {';',               {0x45,NKEY,0x45,0x68,0x45,NKEY,0x45,0x68},NKEY}, // 0x3B - Keyboard ; and :
    {'<',               {NKEY,0x64,NKEY,0x64,NKEY,0x64,NKEY,0x64},NKEY}, // 0x3C - Keyboard , and <
    {'=',               {0x16,0x45,0x48,0x45,0x16,0x45,0x48,0x45},NKEY}, // 0x3D - Keyboard = and +
    {'>',               {NKEY,0x74,NKEY,0x74,NKEY,0x74,NKEY,0x74},NKEY}, // 0x3E - Keyboard . and >
    {'?',               {NKEY,0x65,NKEY,0x65,NKEY,0x65,NKEY,0x65},NKEY}, // 0x3F - Keyboard / and ?
    {'@',               {NKEY,0x35,NKEY,0x58,NKEY,0x35,NKEY,0x58},NKEY}, // 0x40 - Keyboard ' and @
    {'A',               {0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50},NKEY}, // 0x41 - Keyboard a and A
    {'B',               {0x72,0x72,0x72,0x72,0x72,0x72,0x72,0x72},NKEY}, // 0x42 - Keyboard b and B
    {'C',               {0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71},NKEY}, // 0x43 - Keyboard c and C
    {'D',               {0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51},NKEY}, // 0x44 - Keyboard d and D
    {'E',               {0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31},NKEY}, // 0x45 - Keyboard e and E
    {'F',               {0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42},NKEY}, // 0x46 - Keyboard f and F
    {'G',               {0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52},NKEY}, // 0x47 - Keyboard g and G
    {'H',               {0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43},NKEY}, // 0x48 - Keyboard h and H
    {'I',               {0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24},NKEY}, // 0x49 - Keyboard i and I
    {'J',               {0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x53},NKEY}, // 0x4A - Keyboard j and J
    {'K',               {0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44},NKEY}, // 0x4B - Keyboard k and K
    {'L',               {0x54,0x54,0x54,0x54,0x54,0x54,0x54,0x54},NKEY}, // 0x4C - Keyboard l and L
    {'M',               {0x73,0x73,0x73,0x73,0x73,0x73,0x73,0x73},NKEY}, // 0x4D - Keyboard m and M
    {'N',               {0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63},NKEY}, // 0x4E - Keyboard n and N
    {'O',               {0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34},NKEY}, // 0x4F - Keyboard o and O
    {'P',               {0x25,0x25,0x25,0x25,0x25,0x25,0x25,0x25},NKEY}, // 0x50 - Keyboard p and P
    {'Q',               {0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30},NKEY}, // 0x51 - Keyboard q and Q
    {'R',               {0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22},NKEY}, // 0x52 - Keyboard r and R
    {'S',               {0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41},NKEY}, // 0x53 - Keyboard s and S
    {'T',               {0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32},NKEY}, // 0x54 - Keyboard t and T
    {'U',               {0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33},NKEY}, // 0x55 - Keyboard u and U
    {'V',               {0x62,0x62,0x62,0x62,0x62,0x62,0x62,0x62},NKEY}, // 0x56 - Keyboard v and V
    {'W',               {0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21},NKEY}, // 0x57 - Keyboard w and W
    {'X',               {0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61},NKEY}, // 0x58 - Keyboard x and X
    {'Y',               {0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23},NKEY}, // 0x59 - Keyboard y and Y
    {'Z',               {0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70},NKEY}, // 0x5A - Keyboard z and Z
    {'[',               {0x26,0x26,0x26,0x26,0x26,0x26,0x26,0x26},NKEY}, // 0x5B - Keyboard [ and {
    {'\\',              {0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06},NKEY}, // 0x5C - Keyboard \ and |
    {']',               {0x46,0x46,0x46,0x46,0x46,0x46,0x46,0x46},NKEY}, // 0x5D - Keyboard ] and }
    {'^',               {NKEY,0x05,NKEY,0x48,NKEY,0x05,NKEY,0x48},NKEY}, // 0x5E - Keyboard 6 and ^
    {'_',               {NKEY,0x75,NKEY,0x75,NKEY,0x75,NKEY,0x75},NKEY}, // 0x5F - Keyboard - and _
    {'`',               {NKEY,0x35,NKEY,0x35,NKEY,0x35,NKEY,0x35},NKEY}, // 0x60 - Keyboard ` and ~
    {'a',               {0x50,0x50,0x50,0x50,0x50,0x50,0x50,0x50},NKEY}, // 0x61 - Keyboard a and A
    {'b',               {0x72,0x72,0x72,0x72,0x72,0x72,0x72,0x72},NKEY}, // 0x62 - Keyboard b and B
    {'c',               {0x71,0x71,0x71,0x71,0x71,0x71,0x71,0x71},NKEY}, // 0x63 - Keyboard c and C
    {'d',               {0x51,0x51,0x51,0x51,0x51,0x51,0x51,0x51},NKEY}, // 0x64 - Keyboard d and D
    {'e',               {0x31,0x31,0x31,0x31,0x31,0x31,0x31,0x31},NKEY}, // 0x65 - Keyboard e and E
    {'f',               {0x42,0x42,0x42,0x42,0x42,0x42,0x42,0x42},NKEY}, // 0x66 - Keyboard f and F
    {'g',               {0x52,0x52,0x52,0x52,0x52,0x52,0x52,0x52},NKEY}, // 0x67 - Keyboard g and G
    {'h',               {0x43,0x43,0x43,0x43,0x43,0x43,0x43,0x43},NKEY}, // 0x68 - Keyboard h and H
    {'i',               {0x24,0x24,0x24,0x24,0x24,0x24,0x24,0x24},NKEY}, // 0x69 - Keyboard i and I
    {'j',               {0x53,0x53,0x53,0x53,0x53,0x53,0x53,0x53},NKEY}, // 0x6A - Keyboard j and J
    {'k',               {0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44},NKEY}, // 0x6B - Keyboard k and K
    {'l',               {0x54,0x54,0x54,0x54,0x54,0x54,0x54,0x54},NKEY}, // 0x6C - Keyboard l and L
    {'m',               {0x73,0x73,0x73,0x73,0x73,0x73,0x73,0x73},NKEY}, // 0x6D - Keyboard m and M
    {'n',               {0x63,0x63,0x63,0x63,0x63,0x63,0x63,0x63},NKEY}, // 0x6E - Keyboard n and N
    {'o',               {0x34,0x34,0x34,0x34,0x34,0x34,0x34,0x34},NKEY}, // 0x6F - Keyboard o and O
    {'p',               {0x25,0x25,0x25,0x25,0x25,0x25,0x25,0x25},NKEY}, // 0x70 - Keyboard p and P
    {'q',               {0x30,0x30,0x30,0x30,0x30,0x30,0x30,0x30},NKEY}, // 0x71 - Keyboard q and Q
    {'r',               {0x22,0x22,0x22,0x22,0x22,0x22,0x22,0x22},NKEY}, // 0x72 - Keyboard r and R
    {'s',               {0x41,0x41,0x41,0x41,0x41,0x41,0x41,0x41},NKEY}, // 0x73 - Keyboard s and S
    {'t',               {0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32},NKEY}, // 0x74 - Keyboard t and T
    {'u',               {0x33,0x33,0x33,0x33,0x33,0x33,0x33,0x33},NKEY}, // 0x75 - Keyboard u and U
    {'v',               {0x62,0x62,0x62,0x62,0x62,0x62,0x62,0x62},NKEY}, // 0x76 - Keyboard v and V
    {'w',               {0x21,0x21,0x21,0x21,0x21,0x21,0x21,0x21},NKEY}, // 0x77 - Keyboard w and W
    {'x',               {0x61,0x61,0x61,0x61,0x61,0x61,0x61,0x61},NKEY}, // 0x78 - Keyboard x and X
    {'y',               {0x23,0x23,0x23,0x23,0x23,0x23,0x23,0x23},NKEY}, // 0x79 - Keyboard y and Y
    {'z',               {0x70,0x70,0x70,0x70,0x70,0x70,0x70,0x70},NKEY}, // 0x7A - Keyboard z and Z
    {'{',               {NKEY,0x26,NKEY,0x26,NKEY,0x26,NKEY,0x26},NKEY}, // 0x7B - Keyboard [ and {
    {'|',               {NKEY,0x06,NKEY,0x06,NKEY,0x06,NKEY,0x06},NKEY}, // 0x7C - Keyboard \ and |
    {'}',               {NKEY,0x46,NKEY,0x46,NKEY,0x46,NKEY,0x46},NKEY}, // 0x7D - Keyboard ] and }
    {'~',               {NKEY,0x16,NKEY,0x16,NKEY,0x16,NKEY,0x16},NKEY}, // 0x7E - Keyboard # and ~
    {WK_Left,           {0x37,0x37,0x37,0x37,0x37,0x37,0x37,0x37},NKEY}, // 0x100 - Keyboard Left Arrow
    {WK_Right,          {0x47,0x47,0x47,0x47,0x47,0x47,0x47,0x47},NKEY}, // 0x101 - Keyboard Right Arrow
    {WK_Up,             {0x27,0x27,0x27,0x27,0x27,0x27,0x27,0x27},NKEY}, // 0x102 - Keyboard Up Arrow
    {WK_Down,           {0x67,0x67,0x67,0x67,0x67,0x67,0x67,0x67},NKEY}, // 0x103 - Keyboard Down Arrow
    {WK_Page_Up,        {0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07},NKEY}, // 0x104 - Keyboard Page Up
    {WK_Page_Down,      {0x77,0x77,0x77,0x77,0x77,0x77,0x77,0x77},NKEY}, // 0x105 - Keyboard Page Down
    {WK_Home,           {0x57,0x57,0x57,0x57,0x57,0x57,0x57,0x57},NKEY}, // 0x106 - Keyboard Home
    {WK_End,            {0x17,0x17,0x17,0x17,0x17,0x17,0x17,0x17},NKEY}, // 0x107 - Keyboard End
    {WK_Insert,         {0x76,0x76,0x76,0x76,0x76,0x76,0x76,0x76},NKEY}, // 0x108 - Keyboard Insert
    {WK_Delete,         {0x38,0x38,0x38,0x38,0x38,0x38,0x38,0x38},NKEY}, // 0x109 - Keyboard Delete Forward
    {WK_Pause,          {0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08},NKEY}, // 0x10A - Keyboard Pause
    {WK_Scroll_Lock,    {SCRL,SCRL,SCRL,SCRL,SCRL,SCRL,SCRL,SCRL},NKEY}, // 0x10B - Keyboard Scroll Lock
    {WK_Shift_L,        {0x60,0x60,0x60,0x60,0x60,0x60,0x60,0x60},NKEY}, // 0x10D - Left Shift
    {WK_Shift_R,        {0x66,0x66,0x66,0x66,0x66,0x66,0x66,0x66},NKEY}, // 0x10E - Right Shift
    {WK_Control_L,      {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20},NKEY}, // 0x10F - Left Control
    {WK_Control_R,      {0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20},NKEY}, // 0x110 - Right Control
    {WK_Caps_Lock,      {0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40},NKEY}, // 0x111 - Keyboard Caps Lock
    {WK_Num_Lock,       {0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07},NKEY}, // 0x113 - Keyboard Num Lock and Clear
    {WK_F1,             {0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09},NKEY}, // 0x120 - Keyboard F1
    {WK_F2,             {0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29},NKEY}, // 0x121 - Keyboard F2
    {WK_F3,             {0x59,0x59,0x59,0x59,0x59,0x59,0x59,0x59},NKEY}, // 0x122 - Keyboard F3
    {WK_F4,             {0x79,0x79,0x79,0x79,0x79,0x79,0x79,0x79},NKEY}, // 0x123 - Keyboard F4
    {WK_F5,             {0x19,0x19,0x19,0x19,0x19,0x19,0x19,0x19},NKEY}, // 0x124 - Keyboard F5
    {WK_F6,             {0x39,0x39,0x39,0x39,0x39,0x39,0x39,0x39},NKEY}, // 0x125 - Keyboard F6
    {WK_F7,             {0x49,0x49,0x49,0x49,0x49,0x49,0x49,0x49},NKEY}, // 0x126 - Keyboard F7
    {WK_F8,             {0x69,0x69,0x69,0x69,0x69,0x69,0x69,0x69},NKEY}, // 0x127 - Keyboard F8
    {WK_F9 ,            {DIAK,DIAK,DIAK,DIAK,DIAK,DIAK,DIAK,DIAK},NKEY}, // 0x129 - Keyboard F9 - Diagnostics toggle
    {WK_F10,            {0x36,0x36,0x36,0x36,0x36,0x36,0x36,0x36},NKEY}, // 0x128 - Keyboard F10 - MTX Line Feed
    {WK_F11,            {NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},NKEY}, // 0x12A - Keyboard F11
    {WK_F12,            {NUML,NUML,NUML,NUML,NUML,NUML,NUML,NUML},NKEY}, // 0x12B - Keyboard F12 - NumLock replacement
    {WK_KP_Left,        {0x37,0x37,0x37,0x37,0x37,0x28,0x37,0x28},NKEY}, // 0x130 - Keypad 4 and Left Arrow
    {WK_KP_Right,       {0x47,0x47,0x47,0x47,0x47,0x38,0x47,0x38},NKEY}, // 0x131 - Keypad 6 and Right Arrow
    {WK_KP_Up,          {0x27,0x27,0x27,0x27,0x27,0x17,0x27,0x17},NKEY}, // 0x132 - Keypad 8 and Up Arrow
    {WK_KP_Down,        {0x67,0x67,0x67,0x67,0x67,0x57,0x67,0x57},NKEY}, // 0x133 - Keypad 2 and Down Arrow
    {WK_KP_Page_Up,     {0x38,0x38,0x38,0x38,0x07,0x08,0x07,0x08},NKEY}, // 0x134 - Keypad 9 and Page Up
    {WK_KP_Page_Down,   {0x77,0x77,0x77,0x77,0x77,0x47,0x77,0x47},NKEY}, // 0x135 - Keypad 3 and PageDn
    {WK_KP_Home,        {0x28,0x28,0x28,0x28,0x57,0x07,0x57,0x07},NKEY}, // 0x136 - Keypad 7 and Home
    {WK_KP_End,         {0x76,0x76,0x76,0x76,0x17,0x37,0x17,0x37},NKEY}, // 0x137 - Keypad 1 and End
    {WK_KP_Add,         {NKEY,NKEY,NKEY,NKEY,NKEY,0x45,NKEY,0x45},NKEY}, // 0x138 - Keypad +
    {WK_KP_Subtract,    {NKEY,NKEY,NKEY,NKEY,0x05,NKEY,0x05,NKEY},NKEY}, // 0x139 - Keypad -
    {WK_KP_Multiply,    {0x08,0x08,0x08,0x08,NKEY,0x55,NKEY,0x55},NKEY}, // 0x13A - Keypad *
    {WK_KP_Divide,      {0x17,0x17,0x17,0x17,0x65,NKEY,0x65,NKEY},NKEY}, // 0x13B - Keypad /
    {WK_KP_Enter,       {NKEY,NKEY,NKEY,NKEY,0x56,0x56,0x56,0x56},NKEY}, // 0x13C - Keypad ENTER
    {WK_KP_Middle,      {0x57,0x57,0x57,0x57,0x57,0x27,0x57,0x27},NKEY}, // 0x13D - Keypad 5
    {WK_PC_Alt_L,       {RST1,RST1,RST1,RST1,RST1,RST1,RST1,RST1},NKEY}, // 0x140 - Left Alt
    {WK_PC_Alt_R,       {RST2,RST2,RST2,RST2,RST2,RST2,RST2,RST2},NKEY}, // 0x141 - Right Alt
    {WK_Mac_Cmd_L,      {NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},NKEY}, // 0x143 - Left Meta
    {WK_Mac_Cmd_R,      {NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY,NKEY},NKEY}  // 0x144 - Right Meta
    };

static int kbd_emu = 0;
static int kbd_mode = 0;

static int kbd_drive;
static word kbd_sense[8];
#ifdef HAVE_JOY
#include "joy.h"
static word kbd_sense_joy[8];
#endif

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

static struct s_keyinfo *kbd_find (int iKey)
    {
    unsigned int iki1 = 0;
    unsigned int iki2 = sizeof (keyinfo) / sizeof (keyinfo[0]) - 1;
    unsigned int iki3;
    if ( iKey < keyinfo[iki1].iKey ) return NULL;
    if ( iKey == keyinfo[iki1].iKey ) return &keyinfo[iki1];
    if ( iKey > keyinfo[iki2].iKey ) return NULL;
    if ( iKey == keyinfo[iki2].iKey ) return &keyinfo[iki2];
    while ( iki2 - iki1 > 1 )
        {
        iki3 = ( iki1 + iki2 ) / 2;
        if ( iKey == keyinfo[iki3].iKey ) return &keyinfo[iki3];
        if ( iKey < keyinfo[iki3].iKey ) iki2 = iki3;
        else iki1 = iki3;
        }
    return NULL;
    }

void kbd_chk_leds (int *mode)
    {
#ifdef WIN32
    BOOLEAN bCaps, bNum, bScrl;
    if ( kbd_emu & KBDEMU_REMAP )
        {
        win_leds_state (&bCaps, &bNum, &bScrl);
        if ( bScrl ) *mode |= KMD_SCRLK;
        else         *mode &= ~KMD_SCRLK;
        }
#endif
    }

#ifdef ALT_KEYPRESS
extern BOOLEAN ALT_KEYPRESS (int wk);
#endif

void kbd_win_keypress (int wk)
    {
#ifdef ALT_KEYPRESS
    if ( ALT_KEYPRESS(wk) )
        {
        diag_message (DIAG_INIT, "Config completed");
        return;
        }
#endif
    struct s_keyinfo *pki;
    if ( kbd_diag )
        {
        diag_message (DIAG_KBD_WIN_KEY, "Diagnostics key pressed 0x%02X '%c'", wk,
            (( wk >= 0x20 ) && ( wk < 0x7F )) ? wk : '.');
        diag_control(wk);
        return;
        }
    pki = kbd_find (wk);
    diag_message (DIAG_KBD_WIN_KEY, "Key pressed 0x%02X '%c' pki = %p", wk,
        (( wk >= 0x20 ) && ( wk < 0x7F )) ? wk : '.', pki);
    if ( pki != NULL )
        {
        byte kscan;
        if ( ( kbd_sense[SHIFT_ROW] & SHIFT_BITS ) != SHIFT_BITS )  kbd_mode |= KMD_SHIFT;
        else    kbd_mode &= ~ KMD_SHIFT;
        kscan = pki->press[kbd_mode];
        pki->release = kscan;
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

void kbd_win_keyrelease(int wk)
    {
#ifdef ALT_KEYRELEASE
    if ( ALT_KEYRELEASE (wk) ) return;
#endif
    struct s_keyinfo *pki = kbd_find (wk);
    diag_message (DIAG_KBD_WIN_KEY, "Key released 0x%02X '%c' pki = %p", wk,
        (( wk >= 0x20 ) && ( wk < 0x7F )) ? wk : '.', pki);
    if ( pki != NULL )
        {
        byte kscan = pki->release;
        pki->release = NKEY;
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

#ifdef HAVE_JOY
/*...skbd_grid_press:0:*/
void kbd_grid_press(int row, int bitpos)
    {
    // kbd_sense_joy[row] &= ~(1<<bitpos);
    kbd_sense_joy[row] &= ~bitpos;
    }
/*...e*/
/*...skbd_grid_release:0:*/
void kbd_grid_release(int row, int bitpos)
    {
    // kbd_sense_joy[row] |= (1<<bitpos);
    kbd_sense_joy[row] |= bitpos;
    }
/*...e*/
BOOLEAN kbd_grid_test(int row, int bitpos)
    {
    return (kbd_sense_joy[row] & bitpos) == 0;
    }

static struct st_joykey
    {
    int     wk;
    int     row;
    int     bitpos;
    }
    joykey[] =
        {
        {WK_Left,   3, 0x80},
        {WK_Right,  4, 0x80},
        {WK_Up,     2, 0x80},
        {WK_Down,   6, 0x80},
        {WK_Return, 5, 0x40},
        {WK_Escape, 1, 0x01}
        };
static int jk_last = -1;
static long long ms_last = 0;
#define JOY_REPEAT  250     // Repeat interval for joystick buttons in ms.

int joy_key (void)
    {
    int jk = -1;
    joy_periodic ();
    for (int i = 0; i < sizeof (joykey) / sizeof (joykey[0]); ++i)
        {
        if (kbd_grid_test (joykey[i].row, joykey[i].bitpos))
            {
            jk = joykey[i].wk;
            break;
            }
        }
    if ( jk > 0 )
        {
        long long ms_now = get_millis ();
        if ( ( jk != jk_last ) || ( ms_now - ms_last > JOY_REPEAT ) )
            {
            jk_last = jk;
            ms_last = ms_now;
            }
        }
    else
        {
        jk_last = -1;
        }
    return jk;
    }
#endif

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
#ifdef HAVE_JOY
            result &= kbd_sense_joy[i];
#endif
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
#ifdef HAVE_JOY
            result &= kbd_sense_joy[i];
#endif
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

/* Key Name and Grid position */
typedef struct
    {
    const char *name;
    int key;
    } KNAME;

static KNAME kbd_knames[] =
    {
    { "<LT>",           '<' },
    { "<ESC>",          WK_Escape },
    { "<BS>",           WK_BackSpace },
    { "<PAGE>",         WK_Page_Up },
    { "<EOL>",          WK_End },
    { "<BRK>",          WK_Pause },
    { "<F1>",           WK_F1 },
    { "<F5>",           WK_F5 },
    { "<CTRL>",         WK_Control_L },
    { "<LINEFEED>",     WK_F9 },
    { "<TAB>",          WK_Tab },
    { "<UP>",           WK_Up },
    { "<DEL>",          WK_Delete },
    { "<F2>",           WK_F2 },
    { "<F6>",           WK_F6 },
    { "<ALPHALOCK>",    WK_Caps_Lock },
    { "<RET>",          WK_Return },
    { "<LEFT>",         WK_Left },
    { "<HOME>",         WK_Home },
    { "<RIGHT>",        WK_Right },
    { "<F3>",           WK_F3 },
    { "<F7>",           WK_F7 },
    { "<LSHIFT>",       WK_Shift_L },
    { "<RSHIFT>",       WK_Shift_R },
    { "<INS>",          WK_Insert },
    { "<DOWN>",         WK_Down },
    { "<ENTCLS>",       WK_Page_Down },
    { "<F4>",           WK_F4 },
    { "<F8>",           WK_F8 },
    { "<SPACE>",        ' '},
    { "<MENU>",         WK_PC_Menu }
    };

/* Lookup keyname and return key code and shift */
BOOLEAN kbd_find_key (const char **p, int *key, BOOLEAN *shifted)
    {
    struct s_keyinfo *kinfo;
    if ( **p == '\0' )
        return FALSE;
    /*
    if ( !strncmp(*p, "<LT>", 4) )
        {
        *key = '<';
        *shifted = TRUE;
        (*p) += 4;
        return TRUE;
        }
    */
    if ( **p == '<' )
        {
        int i;
        for ( i = 0; i < sizeof(kbd_knames)/sizeof(kbd_knames[0]); i++ )
            {
            KNAME *kname = &(kbd_knames[i]);
            if ( !strncmp(*p, kname->name, strlen(kname->name)) )
                {
                *key     = kname->key;
                *shifted = FALSE;
                (*p)    += strlen(kname->name);
                return TRUE;
                }
            }
        return FALSE;
        }
    kinfo = kbd_find (**p);
    if ( kinfo != NULL )
        {
        *key = **p;
        if ( ( *key >= 'A' ) && ( *key <= 'Z' ) ) *shifted = TRUE;
        else *shifted = ( kinfo->press[0] == NKEY );
        ++(*p);
        return TRUE;
        }
    return FALSE;
    }


/* Lookup keyname and return grid pos and shift */
BOOLEAN kbd_find_grid (const char **p, int *row, int *bitpos, BOOLEAN *shifted)
    {
    int key;
    if ( kbd_find_key (p, &key, shifted) )
        {
        struct s_keyinfo *kinfo = kbd_find (key);
        if ( kinfo != NULL )
            {
            byte pos = kinfo->press[*shifted ? 1: 0];
            *row = pos >> 4;
            *bitpos = 1 << ( pos & 0x0F );
            }
        else
            {
            *row = -2;
            *bitpos = key;
            *shifted = FALSE;
            }
        return TRUE;
        }
    return FALSE;
    }
/*...squeue of key events:0:*/
#define QKE_DELAY   0
#define QKE_PRESS   1
#define QKE_RELEASE 2
#define QKE_STRING  3
#define QKE_FILE    4
#define QKE_DONE    5

typedef struct _QKE QKE;

struct _QKE
    {
    QKE *next;
    int action;
    int value;
    void *ptr;
    };

static QKE *kbd_qke_first = NULL;
static QKE *kbd_qke_last  = NULL;

static void kbd_qke_enqueue (BOOLEAN bFront, int action, int value, void *ptr)
    {
    QKE *qke = (QKE *) emalloc(sizeof(QKE));
    qke->action = action;
    qke->value = value;
    qke->ptr = ptr;
    qke->next   = NULL;
    if ( kbd_qke_first == NULL )
        {
        kbd_qke_first = qke;
        kbd_qke_last  = qke;
        }
    else if ( bFront )
        {
        qke->next     = kbd_qke_first;
        kbd_qke_first = qke;
        }
    else
        {
        kbd_qke_last->next = qke;
        kbd_qke_last       = qke;
        }
    }

static void kbd_qke_dequeue (void)
    {
    QKE *qke = kbd_qke_first;
    kbd_qke_first = qke->next;
    if ( kbd_qke_first == NULL ) kbd_qke_last = NULL;
    free (qke);
    }
/*...e*/
/*...sauto typing:0:*/
/* Cope with things like this
   <Wait20>LOAD ""<RET>
   <Wait20><ALPHALOCK><AutoShift>10 PRINT "Hello"
   <Press><HOME><LEFT><RIGHT><Release><LEFT><RIGHT><HOME><PressAndRelease>
   <Wait200><AutoShift>dir A:<RET>
*/

static BOOLEAN kbd_do_press      = TRUE;
static BOOLEAN kbd_do_release    = TRUE;
static int     kbd_time_press    = 1;
static int     kbd_time_release  = 1;
static BOOLEAN kbd_do_auto_shift = TRUE;
#define SS_UNKNOWN   0
#define SS_SHIFTED   1
#define SS_UNSHIFTED 2
static int     kbd_shift_state = SS_UNKNOWN;
static int     kbd_time_return = 25;

static void kbd_string_events (const char **p)
    {
    diag_message(DIAG_KBD_AUTO_TYPE, "auto-type %s", *p);
    while ( **p != '\0' )
        {
        char buf[50+1];
        /* Simple delay */
        if ( sscanf (*p, "<Wait%[0-9]>", buf) == 1 )
            {
            int wait;
            sscanf (buf, "%d", &wait);
            *p += ( 6 + strlen(buf) );
            kbd_qke_enqueue (TRUE, QKE_DELAY, wait, NULL);
            break;
            }
        /* Controlling of button pushing behavior */
        else if ( !strncmp (*p, "<Press>", 7) )
            {
            kbd_do_press = TRUE;
            kbd_do_release = FALSE;
            *p += 7;
            }
        else if ( !strncmp (*p, "<Release>", 9) )
            {
            kbd_do_press = FALSE;
            kbd_do_release = TRUE;
            *p += 9;
            }
        else if ( !strncmp (*p, "<PressAndRelease>", 17) )
            {
            kbd_do_press = TRUE;
            kbd_do_release = TRUE;
            *p += 17;
            }
        else if ( sscanf (*p, "<PressTime%[0-9]>", buf) == 1 )
            {
            sscanf (buf, "%d", &kbd_time_press);
            *p += ( 11 + strlen(buf) );
            }
        else if ( sscanf (*p, "<ReleaseTime%[0-9]>", buf) == 1 )
            {
            sscanf (buf, "%d", &kbd_time_release);
            *p += ( 13 + strlen(buf) );
            }
        else if ( sscanf (*p, "<ReturnTime%[0-9]>", buf) == 1 )
            {
            sscanf (buf, "%d", &kbd_time_return);
            *p += ( 12 + strlen(buf) );
            }
        else if ( !strncmp (*p, "<AutoShift>", 11) )
            {
            kbd_do_auto_shift = TRUE;
            *p += 11;
            }
        else if ( !strncmp (*p, "<NoAutoShift>", 13) )
            {
            if ( kbd_do_auto_shift )
                {
                kbd_do_auto_shift = FALSE;
                *p += 13;
                if ( kbd_shift_state == SS_SHIFTED )
                    {
                    kbd_qke_enqueue (TRUE, QKE_DELAY, kbd_time_release, NULL);
                    kbd_qke_enqueue (TRUE, QKE_RELEASE, WK_Shift_L, NULL);
                    kbd_shift_state = SS_UNKNOWN;
                    break;
                    }
                }
            }
        /* Button push */
        else
            {
            int key;
            BOOLEAN shifted;
            if ( kbd_find_key (p, &key, &shifted) )
                {
                if ( kbd_do_release )
                    {
                    kbd_qke_enqueue (TRUE, QKE_DELAY,
                        ( key == WK_Return ? kbd_time_return : kbd_time_release ),
                        NULL);
                    kbd_qke_enqueue (TRUE, QKE_RELEASE, key, NULL);
                    }
                if ( kbd_do_press )
                    {
                    kbd_qke_enqueue (TRUE, QKE_DELAY, kbd_time_press, NULL);
                    kbd_qke_enqueue (TRUE, QKE_PRESS, key, NULL);
                    if ( kbd_do_auto_shift )
                        {
                        if ( shifted && kbd_shift_state != SS_SHIFTED )
                            {
                            kbd_qke_enqueue (TRUE, QKE_DELAY, kbd_time_press, NULL);
                            kbd_qke_enqueue (TRUE, QKE_PRESS, WK_Shift_L, NULL);
                            kbd_shift_state = SS_SHIFTED;
                            }
                        else if ( ! shifted && kbd_shift_state != SS_UNSHIFTED )
                            {
                            kbd_qke_enqueue (TRUE, QKE_DELAY, kbd_time_release, NULL);
                            kbd_qke_enqueue (TRUE, QKE_RELEASE, WK_Shift_L, NULL);
                            kbd_shift_state = SS_UNSHIFTED;
                            }
                        }
                    }
                break;
                }
            else
                ++(*p); /* Skip the junk */
            }
        }
    }

#define L_LINE 300
static char autotype_line[L_LINE+1];

/* Called here every 50th of a second */
void kbd_periodic (void)
    {
    diag_message (DIAG_KBD_AUTO_TYPE, "kbd_periodic");
    while ( kbd_qke_first != NULL )
        {
        diag_message (DIAG_KBD_AUTO_TYPE, "kbd_qke_first = %p, action = %d",
            kbd_qke_first, kbd_qke_first->action);
        switch ( kbd_qke_first->action )
            {
            case QKE_DELAY:
            {
            diag_message (DIAG_KBD_AUTO_TYPE, "auto-type delay %d", kbd_qke_first->value);
            if ( --kbd_qke_first->value == 0 ) kbd_qke_dequeue ();
            return;
            }
            case QKE_PRESS:
            {
            int key = kbd_qke_first->value;
            diag_message (DIAG_KBD_AUTO_TYPE, "auto-type press 0x%02X %c", key,
                ( ( key > 0x20 ) && ( key < 0x7F ) ) ? key : '.');
            kbd_win_keypress (key);
            kbd_qke_dequeue ();
            break;
            }
            case QKE_RELEASE:
            {
            int key = kbd_qke_first->value;
            diag_message (DIAG_KBD_AUTO_TYPE, "auto-type release 0x%02X %c", key,
                ( ( key > 0x20 ) && ( key < 0x7F ) ) ? key : '.');
            kbd_win_keyrelease (key);
            kbd_qke_dequeue ();
            }
            break;
            case QKE_STRING:
            {
            if ( *((const char *) (kbd_qke_first->ptr)) == '\0' ) kbd_qke_dequeue ();
            else kbd_string_events ((const char **) &kbd_qke_first->ptr);
            }
            break;
            case QKE_FILE:
            {
            FILE *fp = (FILE *) kbd_qke_first->ptr;
            diag_message (DIAG_KBD_AUTO_TYPE, "auto-type file");
            if ( fgets (autotype_line, L_LINE, fp) != NULL )
                {
                int len = strlen (autotype_line);
                if ( len > 0 )
                    {
                    if ( autotype_line[len-1] == '\n' ) autotype_line[len-1] = '\r';
                    kbd_qke_enqueue (TRUE, QKE_STRING, 0, autotype_line);
                    }
                }
            else
                {
                fclose (fp);
                kbd_qke_dequeue ();
                }
            }
            break;
            case QKE_DONE:
            {
            diag_message (DIAG_KBD_AUTO_TYPE, "auto-type done");
            if ( kbd_do_auto_shift )
                {
                if ( kbd_shift_state == SS_SHIFTED )
                    {
                    kbd_qke_enqueue (TRUE, QKE_DELAY, kbd_time_release, NULL);
                    kbd_qke_enqueue (TRUE, QKE_RELEASE, WK_Shift_L, NULL);
                    }
                kbd_shift_state = SS_UNKNOWN;
                }
            kbd_qke_dequeue ();
            }
            break;
            }
        }
    }

void kbd_add_events (const char *p)
    {
    kbd_qke_enqueue (FALSE, QKE_STRING, 0, (void *) p);
    }

void kbd_add_events_file (const char *fn)
    {
    FILE *fp = efopen (fn, "r");
    diag_message (DIAG_KBD_AUTO_TYPE, "auto-type from file %s", fn);
    if ( fp == NULL ) fatal ("can't open %s", fn);
    kbd_qke_enqueue (FALSE, QKE_FILE, 0, fp);
    }

void kbd_add_events_done(void)
    {
    kbd_qke_enqueue (FALSE, QKE_DONE, 0, NULL);
    }
/* Normally we use the unmapped tables.
   However, if keyboard remapping is enabled,
   we use different bits for the keys,
   and we have to patch the ROMs using data below. */

static const byte *mtx_base   = (const byte *)
    "z\x00" "a\xa0q\x00\x1b" "1"
    "cxdsew23"
    "bvgftr45"
    "mnjhuy67"
    ".,lkoi89"
    "_/:;@p0-"
    "\x15\x00\x0d]\x0a[^\\"
    "\x0c\x0a\x1a\x19\x08\x0b\x05\x1d"
    "\x83\x87\x82\x86\x85\x81\x84\x80"
    " \x00\x00\x00\x7f\x09\x08\x03";

static const byte *mtx_upper  = (const byte *)
    "Z\x00" "A\xa0Q\x00\x1b!"
    "CXDSEW\"#"
    "BVGFTR$%"
    "MNJHUY&'"
    "><LKOI()"
    "_?*+`P0="
    "0\x00\x0d}\x0a{~|"
    "\x0d.231587"
    "\x8b\x8f\x8a\x8e\x8d\x89\x8c\x88"
    " \x00\x00\x00" "64\x08" "9";

static const byte *patch_base   = (const byte *)
    "z\x00" "a\xa0q\x00\x1b" "1"
    "cxdsew23"
    "bvgftr45"
    "mnjhuy67"
    ".,lkoi89"
    "_/:;@p0-"
    "\x15\x00\x0d]\x0a[^\\"
    "\x0c\x0a\x1a\x19\x08\x0b\x05\x1d"
    "\x83\x87\x82\x86\x85\x81\x84\x80"
    " #'=\x7f\x09\x08\x03";

static const byte *patch_upper  = (const byte *)
    "Z\x00" "A\xa0Q\x00\x1b!"
    "CXDSEW\"#"
    "BVGFTR$%"
    "MNJHUY&'"
    "><LKOI()"
    "_?*+`P0="
    "0\x00\x0d}\x0a{~|"
    "\x0d.231587"
    "\x8b\x8f\x8a\x8e\x8d\x89\x8c\x88"
    " :@^64\x08" "9";

/*...skbd_apply_remap:0:*/
static void kbd_patch_roms (const byte *old_lower, const byte *old_upper, const byte *new_lower, const byte *new_upper)
    {
    int rom;
    byte *ptr;
    for ( rom = 0; rom < 8; ++rom )
        {
        /* diag_message (DIAG_KBD_REMAP, "Searching ROM%d for keyboard mappings", rom); */
        ptr = mem_rom_ptr(rom);
        if ( memcmp(ptr, "\xff\xff\xff\xff\xff\xff\xff\xff", 8) )
            {
            int addr;
            for ( addr = ROM_SIZE; addr < ( 2 * ROM_SIZE - 80 ); ++addr )
                {
                if ( !memcmp(ptr, old_lower, 80) )
                    {
                    memcpy(ptr, new_lower, 80);
                    diag_message (DIAG_KBD_REMAP, "Remapped unshifted keys at %d:0x%04x", rom, addr);
                    }
                else if ( !memcmp (ptr, old_upper, 80) )
                    {
                    memcpy(ptr, new_upper, 80);
                    diag_message(DIAG_KBD_REMAP, "Remapped shifted keys at %d:0x%04x", rom, addr);
                    }
                ++ptr;
                }
            }
        }
    }

void kbd_apply_remap (void)
    {
    kbd_patch_roms (mtx_base, mtx_upper, patch_base, patch_upper);
    kbd_emu |= KBDEMU_REMAP;
    }

void kbd_apply_unmap (void)
    {
    kbd_patch_roms (patch_base, patch_upper, mtx_base, mtx_upper);
    kbd_emu &= ~KBDEMU_REMAP;
    }
/*...e*/

/*...skbd_init:0:*/
void kbd_init(int emu)
    {
    int i;

    kbd_emu = emu;
    kbd_mode = 0;

    for ( i = 0; i < 8; i++ )
        {
        kbd_sense    [i] = 0xffff;
#ifdef HAVE_JOY
        kbd_sense_joy[i] = 0xffff;
#endif
        }

    if ( kbd_emu & KBDEMU_REMAP )
        {
        kbd_apply_remap ();
        kbd_mode = KMD_SCRLK;
        }
    else
        {
        kbd_apply_unmap ();
        }
    kbd_chk_leds (&kbd_mode);
    win_kbd_leds (FALSE, kbd_mode & KMD_NUMLK, kbd_mode & KMD_SCRLK);
    }

int kbd_get_emu (void)
    {
    return kbd_emu;
    }
/*...e*/
/*...skbd_term:0:*/
void kbd_term(void)
    {
    kbd_emu = 0;
    }
/*...e*/
