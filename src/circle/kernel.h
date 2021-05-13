//
// kernel.h
//
// MEMU version based upon:
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014  R. Stange <rsta2@o2online.de>
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
#ifndef _kernel_h
#define _kernel_h

#include <circle/memory.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/bcmframebuffer.h>
// #include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
// #include <circle/usb/dwhcidevice.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/sched/scheduler.h>
#include <circle/soundbasedevice.h>
#ifdef USE_VCHIQ_SOUND
	#include <vc4/vchiq/vchiqdevice.h>
#endif
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <circle/types.h>

#include "kfuncs.h"
#include "keyeventbuffer.h"
#include "console.h"

enum TShutdownMode
    {
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
    };

class CKernel
    {
    public:
    CKernel (void);
    ~CKernel (void);
    boolean Initialize (void);
    TShutdownMode Run (void);
    void GetFBInfo (struct FBInfo *pfinfo);
    void SetFBPalette (int iCol, u32 iRGB);
    void UpdateFBPalette (void);
    int InitSound (void);
    void WriteSoundData (unsigned int nFrames);
    void SoundCallback (void);
    static void SoundCallbackStub (void *pv);
    void TermSound (void);
    void Yield (void);
    void Quit (void);
    
    private:
    // do not change this order
    CMemorySystem       m_Memory;
    CActLED             m_ActLED;
    CKernelOptions      m_Options;
    CDeviceNameService  m_DeviceNameService;
//	CSerialDevice		m_Serial;
    CConsole            m_Console;
    CExceptionHandler   m_ExceptionHandler;
    CInterruptSystem    m_Interrupt;
    CTimer              m_Timer;
	CLogger 			m_Logger;
//    CDWHCIDevice        m_DWHCI;
	CUSBHCIDevice		m_USBHCI;
	CScheduler		    m_Scheduler;
#ifdef USE_VCHIQ_SOUND
	CVCHIQDevice		m_VCHIQ;
#endif
	CEMMCDevice		    m_EMMC;
	FATFS			    m_FileSystem;

    CBcmFrameBuffer *   m_pFrameBuffer;
    CKeyEventBuffer *   m_pKeyEvents;
	CSoundBaseDevice *  m_pSound;
    unsigned int        m_nSoundQueueSize;
    unsigned int        m_nSoundQueueMin;
    bool                m_bSoundOn;
    bool                m_bInCallback;
    };

#endif
