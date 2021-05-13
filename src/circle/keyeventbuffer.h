//
// keyeventbuffer.h - Modified from keyboardbuffer.h
//
// MEMU version based upon:
//
// Buffers key press and key release events.
// Note: Is effectivly a soliton class.
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2017-2018  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#ifndef keyeventbuffer_h
#define keyeventbuffer_h
//
//   USB Scan codes.
//
#define KEY_NULL        0x00    // Invalid key
#define KEY_A           0x04    // Keyboard a and A
#define KEY_B           0x05    // Keyboard b and B
#define KEY_C           0x06    // Keyboard c and C
#define KEY_D           0x07    // Keyboard d and D
#define KEY_E           0x08    // Keyboard e and E
#define KEY_F           0x09    // Keyboard f and F
#define KEY_G           0x0a    // Keyboard g and G
#define KEY_H           0x0b    // Keyboard h and H
#define KEY_I           0x0c    // Keyboard i and I
#define KEY_J           0x0d    // Keyboard j and J
#define KEY_K           0x0e    // Keyboard k and K
#define KEY_L           0x0f    // Keyboard l and L
#define KEY_M           0x10    // Keyboard m and M
#define KEY_N           0x11    // Keyboard n and N
#define KEY_O           0x12    // Keyboard o and O
#define KEY_P           0x13    // Keyboard p and P
#define KEY_Q           0x14    // Keyboard q and Q
#define KEY_R           0x15    // Keyboard r and R
#define KEY_S           0x16    // Keyboard s and S
#define KEY_T           0x17    // Keyboard t and T
#define KEY_U           0x18    // Keyboard u and U
#define KEY_V           0x19    // Keyboard v and V
#define KEY_W           0x1a    // Keyboard w and W
#define KEY_X           0x1b    // Keyboard x and X
#define KEY_Y           0x1c    // Keyboard y and Y
#define KEY_Z           0x1d    // Keyboard z and Z
#define KEY_1           0x1e    // Keyboard 1 and !
#define KEY_2           0x1f    // Keyboard 2 and "
#define KEY_3           0x20    // Keyboard 3 and £
#define KEY_4           0x21    // Keyboard 4 and $
#define KEY_5           0x22    // Keyboard 5 and %
#define KEY_6           0x23    // Keyboard 6 and ^
#define KEY_7           0x24    // Keyboard 7 and &
#define KEY_8           0x25    // Keyboard 8 and *
#define KEY_9           0x26    // Keyboard 9 and (
#define KEY_ZERO        0x27    // Keyboard 0 and )
#define KEY_ENTER       0x28    // Keyboard Return (ENTER)
#define KEY_ESC         0x29    // Keyboard ESCAPE
#define KEY_BACKSP      0x2a    // Keyboard DELETE (Backspace)
#define KEY_TAB         0x2b    // Keyboard Tab
#define KEY_SPACE       0x2c    // Keyboard Spacebar
#define KEY_MINUS       0x2d    // Keyboard - and _
#define KEY_EQUAL       0x2e    // Keyboard = and +
#define KEY_LSQBRK      0x2f    // Keyboard [ and {
#define KEY_RSQBRK      0x30    // Keyboard ] and }
#define KEY_BSLASH      0x31    // Keyboard \ and |
#define KEY_HASH        0x32    // Keyboard Non-US # and ~
#define KEY_SCOLON      0x33    // Keyboard ; and :
#define KEY_QUOTE       0x34    // Keyboard // and "
#define KEY_BQUOTE      0x35    // Keyboard ` and ~
#define KEY_COMMA       0x36    // Keyboard , and <
#define KEY_STOP        0x37    // Keyboard . and >
#define KEY_SLASH       0x38    // Keyboard / and ?
#define KEY_CAPLK       0x39    // Keyboard Caps Lock
#define KEY_F1          0x3a    // Keyboard F1
#define KEY_F2          0x3b    // Keyboard F2
#define KEY_F3          0x3c    // Keyboard F3
#define KEY_F4          0x3d    // Keyboard F4
#define KEY_F5          0x3e    // Keyboard F5
#define KEY_F6          0x3f    // Keyboard F6
#define KEY_F7          0x40    // Keyboard F7
#define KEY_F8          0x41    // Keyboard F8
#define KEY_F9          0x42    // Keyboard F9 - MTX Line Feed
#define KEY_F10         0x43    // Keyboard F10
#define KEY_F11         0x44    // Keyboard F11
#define KEY_F12         0x45    // Keyboard F12
#define KEY_PRNSCR      0x46    // Keyboard Print Screen
#define KEY_SCRLK       0x47    // Keyboard Scroll Lock
#define KEY_PAUSE       0x48    // Keyboard Pause
#define KEY_INS         0x49    // Keyboard Insert
#define KEY_HOME        0x4a    // Keyboard Home
#define KEY_PGUP        0x4b    // Keyboard Page Up
#define KEY_DEL         0x4c    // Keyboard Delete Forward
#define KEY_END         0x4d    // Keyboard End
#define KEY_PGDN        0x4e    // Keyboard Page Down
#define KEY_RIGHT       0x4f    // Keyboard Right Arrow
#define KEY_LEFT        0x50    // Keyboard Left Arrow
#define KEY_DOWN        0x51    // Keyboard Down Arrow
#define KEY_UP          0x52    // Keyboard Up Arrow
#define KEY_NUMLK       0x53    // Keyboard Num Lock and Clear
#define KEY_KPDIV       0x54    // Keypad /
#define KEY_KPMULT      0x55    // Keypad *
#define KEY_KPMINUS     0x56    // Keypad -
#define KEY_KPPLUS      0x57    // Keypad +
#define KEY_KPENTER     0x58    // Keypad ENTER
#define KEY_KP1         0x59    // Keypad 1 and End
#define KEY_KP2         0x5a    // Keypad 2 and Down Arrow
#define KEY_KP3         0x5b    // Keypad 3 and PageDn
#define KEY_KP4         0x5c    // Keypad 4 and Left Arrow
#define KEY_KP5         0x5d    // Keypad 5
#define KEY_KP6         0x5e    // Keypad 6 and Right Arrow
#define KEY_KP7         0x5f    // Keypad 7 and Home
#define KEY_KP8         0x60    // Keypad 8 and Up Arrow
#define KEY_KP9         0x61    // Keypad 9 and Page Up
#define KEY_KP0         0x62    // Keypad 0 and Insert
#define KEY_KPSTOP      0x63    // Keypad . and Delete
//   The following are not true USB scan codes but are used for modifier keys
#define KEY_MODIFIERS   0x64
#define KEY_LCTRL       KEY_MODIFIERS + 0   // Left Control
#define KEY_LSHIFT      KEY_MODIFIERS + 1   // Left Shift
#define KEY_LALT        KEY_MODIFIERS + 2   // Left Alt
#define KEY_LMETA       KEY_MODIFIERS + 3   // Left Meta
#define KEY_RCTRL       KEY_MODIFIERS + 4   // Right Control
#define KEY_RSHIFT      KEY_MODIFIERS + 5   // Right Shift
#define KEY_RALT        KEY_MODIFIERS + 6   // Right Alt
#define KEY_RMETA       KEY_MODIFIERS + 7   // Right Meta
#define KEY_COUNT       KEY_MODIFIERS + 8   // Number of key codes defined.
#define KEY_RELEASE     0x80    // Key release flag

#define KEYB_LED_NUM_LOCK       (1 << 0)
#define KEYB_LED_CAPS_LOCK      (1 << 1)
#define KEYB_LED_SCROLL_LOCK    (1 << 2)

#ifdef __cplusplus
#include <circle/device.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/types.h>

#define KEYB_BUF_SIZE		64			// must be a power of 2
#define KEYB_BUF_MASK		(KEYB_BUF_SIZE-1)

class CKeyEventBuffer : public CDevice
{
public:
	CKeyEventBuffer (CUSBKeyboardDevice *pKeyboard);
	~CKeyEventBuffer (void);

	u8 OutBuf (void);				// returns '\0' if no key is waiting
    static u8 GetEvent (void);      // Static version of above
    static void SetLeds (u8 uLeds); // Set keyboard status LEDs

private:
	void InBuf (u8 uKey);
	boolean BufStat (void) const;

	void KeyEventHandler (u8 uMods, const u8 *pKeys);
    boolean FindByte (const u8 *pBuffer, u8 ucByte, unsigned nLength);
	static void KeyEventStub (u8 uMods, const u8 *pKeys);

private:
	CUSBKeyboardDevice *m_pKeyboard;

	u8   	    m_Buffer[KEYB_BUF_SIZE];
	unsigned    m_nInPtr;
	unsigned    m_nOutPtr;
    u8          m_LastMods;
	u8          m_LastKeys[USBKEYB_REPORT_SIZE - 2];

	static CKeyEventBuffer *s_pThis;
};
#endif

#endif
