//
// keyeventbuffer.cpp - Modified from keyboardbuffer.cpp
//
// Buffers key press and key release events.
// Note: Is effectivly a soliton class.
//
// MEMU version based upon:
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
#include <assert.h>
#include <circle/util.h>
#include "keyeventbuffer.h"

CKeyEventBuffer *CKeyEventBuffer::s_pThis = 0;

CKeyEventBuffer::CKeyEventBuffer (CUSBKeyboardDevice *pKeyboard)
    :	m_pKeyboard (pKeyboard),
        m_nInPtr (0),
        m_nOutPtr (0),
        m_LastMods (0)
    {
	assert (s_pThis == 0);
	s_pThis = this;

    for (int i = 0; i < USBKEYB_REPORT_SIZE - 2; ++i)
        m_LastKeys[i] = 0;

	assert (m_pKeyboard != 0);
	m_pKeyboard->RegisterKeyStatusHandlerRaw (KeyEventStub);
    }

CKeyEventBuffer::~CKeyEventBuffer (void)
    {
	m_pKeyboard = 0;

	s_pThis = 0;
    }

void CKeyEventBuffer::InBuf (u8 uKey)
    {
	if (((m_nInPtr+1) & KEYB_BUF_MASK) != m_nOutPtr)
        {
		m_Buffer[m_nInPtr] = uKey;

		m_nInPtr = (m_nInPtr+1) & KEYB_BUF_MASK;
        }
    }

boolean CKeyEventBuffer::BufStat (void) const
    {
	return m_nInPtr != m_nOutPtr ? TRUE : FALSE;
    }

u8 CKeyEventBuffer::OutBuf (void)
    {
	if (m_nInPtr == m_nOutPtr)
        {
		return 0;
        }

	u8 uKey = m_Buffer[m_nOutPtr];

	m_nOutPtr = (m_nOutPtr+1) & KEYB_BUF_MASK;

	return uKey;
    }

void CKeyEventBuffer::KeyEventHandler (u8 uMods, const u8 *pKeys)
    {
	// report modifier keys
	for (unsigned i = 0; i < 8; i++)
        {
		unsigned nMask = 1 << i;

		if (    (uMods & nMask)
		    && !(m_LastMods & nMask))
            {
			InBuf (KEY_MODIFIERS + i);
            }
		else if ( ! ( uMods & nMask ) && ( m_LastMods & nMask ) )
            {
			InBuf (KEY_RELEASE + KEY_MODIFIERS + i);
            }
        }
    m_LastMods = uMods;

	// report released keys
	for (unsigned i = 0; i < USBKEYB_REPORT_SIZE - 2; i++)
        {
		u8 ucKeyCode = m_LastKeys[i];
		if ( ( ucKeyCode != 0 ) && ( ucKeyCode < KEY_MODIFIERS )
		    && ( ! FindByte (pKeys, ucKeyCode, USBKEYB_REPORT_SIZE-2) ) )
            {
			InBuf (KEY_RELEASE + ucKeyCode);
            }
        }

	// report pressed keys
	for (unsigned i = 0; i < USBKEYB_REPORT_SIZE - 2; i++)
        {
		u8 ucKeyCode = pKeys[i];
		if ( ( ucKeyCode != 0 ) && ( ucKeyCode < KEY_MODIFIERS )
		    && ( ! FindByte (m_LastKeys, ucKeyCode, USBKEYB_REPORT_SIZE-2) ) )
            {
			InBuf (ucKeyCode);
            }
        }

	memcpy (m_LastKeys, pKeys, sizeof (m_LastKeys));
    }

boolean CKeyEventBuffer::FindByte (const u8 *pBuffer, u8 ucByte, unsigned nLength)
    {
	while (nLength-- > 0)
        {
		if (*pBuffer++ == ucByte)
            {
			return TRUE;
            }
        }

	return FALSE;
    }

void CKeyEventBuffer::KeyEventStub (u8 uMods, const u8 *pKeys)
    {
	assert (s_pThis != 0);
	s_pThis->KeyEventHandler (uMods, pKeys);
    }

u8 CKeyEventBuffer::GetEvent (void)
    {
	assert (s_pThis != 0);
	return s_pThis->OutBuf ();
    }

void CKeyEventBuffer::SetLeds (u8 uLeds)
    {
	assert (s_pThis != 0);
    s_pThis->m_pKeyboard->SetLEDs (uLeds);
    }
