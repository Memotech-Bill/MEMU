//
// kernel.cpp
//
// MEMU version based upon:
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2018  R. Stange <rsta2@o2online.de>
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
//  #define BOOT_WAIT   // Wait for key press before switching to MEMU screens
// #define INIT_DIAG   "-diag-init"   // Force MEMU initial diagnostics

#include <circle/startup.h>
#include <circle/util.h>
#include <circle/machineinfo.h>
#include <circle/pwmsoundbasedevice.h>
#include <circle/i2ssoundbasedevice.h>
#ifdef USE_VCHIQ_SOUND
#include <vc4/sound/vchiqsoundbasedevice.h>
#endif
#include <fatfs/ff.h>
#include <stdio.h>
#include "kernel.h"
#include "memu.h"
#include "snd.h"

#define SAMPLE_RATE         44100       // overall system clock
#define WRITE_CHANNELS      1           // 1: Mono, 2: Stereo
#define QUEUE_SIZE_MSECS    100         // size of the sound queue in milliseconds duration
#define CHUNK_SIZE          4000        // number of samples, written to sound device at once

#define DRIVE       "SD:"
//#define DRIVE     "USB:"

static CKernel* s_pThis = 0;

static const char FromKernel[] = "kernel";

CKernel::CKernel (void)
    :   m_Timer (&m_Interrupt),
        m_Logger (m_Options.GetLogLevel (), &m_Timer),
        // m_DWHCI (&m_Interrupt, &m_Timer),
        m_USBHCI (&m_Interrupt, &m_Timer, FALSE),		// TRUE: enable plug-and-play
#ifdef USE_VCHIQ_SOUND
        m_VCHIQ (&m_Memory, &m_Interrupt),
#endif
        m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
        m_pFrameBuffer (0),
        m_pKeyEvents (0),
        m_pSound (0),
        m_bSoundOn (FALSE),
        m_bInCallback (FALSE)
    {
    s_pThis = this;
    }

CKernel::~CKernel (void)
    {
    if ( m_pKeyEvents ) delete m_pKeyEvents;
    m_pKeyEvents = 0;
    if ( m_pFrameBuffer ) delete m_pFrameBuffer;
    m_pFrameBuffer = 0;
    }

boolean CKernel::Initialize (void)
    {
   // m_ActLED.Blink (3, 500, 500);
    // if ( ! m_Serial.Initialize (115200) ) return FALSE;
    // m_ActLED.Blink (1);
    // if ( ! m_Logger.Initialize (&m_Serial) ) return FALSE;
    // m_ActLED.Blink (1);
    m_pFrameBuffer = new CBcmFrameBuffer (0, 0, 8);
    if ( ! m_pFrameBuffer->Initialize () ) return FALSE;
    // m_ActLED.Blink (1);
    if ( m_pFrameBuffer->GetDepth () != 8 ) return FALSE;
   // m_ActLED.Blink (1);
    // Ensure that each row is word-aligned so that we can safely use memcpyblk()
    if ( m_pFrameBuffer->GetPitch () % sizeof (u32) != 0 ) return FALSE;
   // m_ActLED.Blink (1);
    if ( ! m_Console.Initialize () ) return FALSE;
   // m_ActLED.Blink (1);
    if ( ! m_Logger.Initialize (&m_Console) ) return FALSE;
    m_Logger.Write (FromKernel, LogNotice, "Kernel version: %d Compile time: " __DATE__ " " __TIME__, RASPPI);
   // m_ActLED.Blink (1);
    m_Logger.Write (FromKernel, LogNotice, "FrameBuffer: %d x %d, Stride = %d, Depth = %d",
        m_pFrameBuffer->GetWidth (),
        m_pFrameBuffer->GetHeight (),
        m_pFrameBuffer->GetPitch (),
        m_pFrameBuffer->GetDepth ());

    m_Logger.Write (FromKernel, LogNotice, "Initialising Interrupts");
    if ( ! m_Interrupt.Initialize () )
        {
        m_Logger.Write (FromKernel, LogPanic, "Failed to initialise Interrupts");
        return FALSE;
        }
   // m_ActLED.Blink (1);
    m_Logger.Write (FromKernel, LogNotice, "Initialising Timer");
    if ( ! m_Timer.Initialize () )
        {
        m_Logger.Write (FromKernel, LogPanic, "Failed to initialise Timer");
        return FALSE;
        }
   // m_ActLED.Blink (1);
    m_Logger.Write (FromKernel, LogNotice, "Initialising USB");
    if ( ! m_USBHCI.Initialize () )
        {
        m_Logger.Write (FromKernel, LogPanic, "Failed to initialise USB");
        return FALSE;
        }
   // m_ActLED.Blink (1);
#ifdef USE_VCHIQ_SOUND
    m_Logger.Write (FromKernel, LogNotice, "Initialising VCHIQ");
    if ( ! m_VCHIQ.Initialize () )
        {
        m_Logger.Write (FromKernel, LogPanic, "Failed to initialise VCHIQ");
        return FALSE;
        }
   // m_ActLED.Blink (1);
#endif
    m_Logger.Write (FromKernel, LogNotice, "Initialising SD Card Device");
    if ( ! m_EMMC.Initialize () )
        {
        m_Logger.Write (FromKernel, LogPanic, "Failed to initialise SD Card Device");
        return FALSE;
        }
   // m_ActLED.Blink (1);
    m_Logger.Write (FromKernel, LogNotice, "Mounting Filesystem");
    if ( f_mount (&m_FileSystem, DRIVE, 1) != FR_OK )
        {
        m_Logger.Write (FromKernel, LogPanic, "Failed to Mount Filesystem");
        return FALSE;
        }
   // m_ActLED.Blink (1);
    if ( f_chdrive (DRIVE) != FR_OK )
        {
        m_Logger.Write (FromKernel, LogPanic, "Failed to set Default Drive");
        return FALSE;
        }
   // m_ActLED.Blink (1);
    
    CUSBKeyboardDevice *pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
    if ( pKeyboard == 0 )
        {
        m_Logger.Write (FromKernel, LogNotice, "The following devices have been found:");
        m_DeviceNameService.ListDevices (&m_Console);
        m_Logger.Write (FromKernel, LogPanic, "No keyboard found");
#ifdef BOOT_DUMP
        m_Console.Dump ();
#endif
        return FALSE;
        }
   // m_ActLED.Blink (1);
    m_pKeyEvents = new CKeyEventBuffer (pKeyboard);
    // select the sound device
    const char *pSoundDevice = m_Options.GetSoundDevice ();
    m_Logger.Write (FromKernel, LogNotice, "Requested %s Sound Device", pSoundDevice);
    if (strcmp (pSoundDevice, "sndi2s") == 0)
        {
        m_Logger.Write (FromKernel, LogNotice, "Loading I2S Sound Device");
        m_pSound = new CI2SSoundBaseDevice (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
        }
#ifdef USE_VCHIQ_SOUND
    else if (strcmp (pSoundDevice, "sndvchiq") == 0)
        {
        m_Logger.Write (FromKernel, LogNotice, "Loading HDMI Sound Device");
        m_pSound = new CVCHIQSoundBaseDevice (&m_VCHIQ, SAMPLE_RATE, CHUNK_SIZE,
            (TVCHIQSoundDestination) m_Options.GetSoundOption ());
        }
#endif
    else
        {
        m_Logger.Write (FromKernel, LogNotice, "Loading PWM Sound Device");
        m_pSound = new CPWMSoundBaseDevice (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
        }
   // m_ActLED.Blink (1);
    
    return TRUE;
    }

TShutdownMode CKernel::Run (void)
    {
   // m_ActLED.Blink (3, 2000, 500);
#ifdef INIT_DIAG
    static const char *sArg[] = { "memu", "-diag-file", INIT_DIAG, "-config-file", "/memu0.cfg",
                                  "-config-file", "/memu.cfg" };
    remove ("/memu.log");
#else
    static const char *sArg[] = { "memu", "-config-file", "/memu0.cfg", "-config-file", "/memu.cfg" };
#endif
#ifdef BOOT_DUMP
    m_Console.Dump ();
#endif
#ifdef BOOT_WAIT
    m_Console.Write ("Press a key to continue\n", 0);
    while ( CKeyEventBuffer::GetEvent () == 0 )
        {
        // Wait for key press
        }
#endif
    m_Logger.Write (FromKernel, LogNotice, "Starting MEMU");
    memu (sizeof (sArg) / sizeof (sArg[0]), sArg);
    Quit ();

    return ShutdownHalt;
    }

void CKernel::Quit (void)
    {
    m_Console.Show ();
    m_ActLED.Blink (3, 500, 200);
    m_Logger.Write (FromKernel, LogPanic, "MEMU Terminated");
    halt ();
    }

void CKernel::GetFBInfo (struct FBInfo *pfinfo)
    {
    pfinfo->xres = m_pFrameBuffer->GetWidth ();
    pfinfo->yres = m_pFrameBuffer->GetHeight ();
    pfinfo->xstride = m_pFrameBuffer->GetPitch ();
    pfinfo->pfb = (byte *) m_pFrameBuffer->GetBuffer ();
    }

void CKernel::SetFBPalette (int iCol, u32 iRGB)
    {
    m_pFrameBuffer->SetPalette32 (iCol, iRGB);
    }

void CKernel::UpdateFBPalette (void)
    {
    m_pFrameBuffer->UpdatePalette ();
    }

int CKernel::InitSound (void)
    {
    m_Logger.Write (FromKernel, LogNotice, "InitSound called");
    // configure sound device
    if ( ! m_pSound->AllocateQueue (QUEUE_SIZE_MSECS) )
        {
        m_Logger.Write (FromKernel, LogError, "Failed to allocate sound queue");
        return 0;
        }
    m_pSound->SetWriteFormat (SoundFormatSigned16, WRITE_CHANNELS);

    // initially fill the whole queue with data
    m_nSoundQueueSize = m_pSound->GetQueueSizeFrames ();
    m_Logger.Write (FromKernel, LogNotice, "Sound Queue Size = %d", m_nSoundQueueSize);
    WriteSoundData (m_nSoundQueueSize);
    m_nSoundQueueMin = m_nSoundQueueSize;

    // Register data callback
    // m_pSound->RegisterNeedDataCallback (SoundCallbackStub, 0);

    // start sound device
    if ( ! m_pSound->Start () )
        {
        m_Logger.Write (FromKernel, LogError, "Failed to start sound playback");
        return 0;
        }
    m_Logger.Write (FromKernel, LogNotice, "Sound Output Started");
    m_bSoundOn = TRUE;

    return 1;
    }

void CKernel::WriteSoundData (unsigned int nFrames)
    {
    const unsigned int nFramesPerWrite = 1000;
    u8 Buffer[nFramesPerWrite * WRITE_CHANNELS * sizeof (s16)];
    // m_Logger.Write (FromKernel, LogNotice,
    //     "WriteSoundData: this = 0x%08X, s_pThis = 0x%08X, nFrames = %d",
    //     this, s_pThis, nFrames);

    while (nFrames > 0)
        {
        unsigned int nWriteFrames = nFrames < nFramesPerWrite ? nFrames : nFramesPerWrite;
        // m_Logger.Write (FromKernel, LogNotice, "Request %d Frames", nWriteFrames);
        snd_callback ((short *) Buffer, nWriteFrames);

        unsigned int nWriteBytes = nWriteFrames * WRITE_CHANNELS * sizeof (s16);
        // m_Logger.Write (FromKernel, LogNotice, "Write %d bytes", nWriteBytes);
        unsigned int nWritten = m_pSound->Write (Buffer, nWriteBytes);
        // m_Logger.Write (FromKernel, LogNotice, "%d bytes written", nWritten);
        nFrames -= nWritten / ( WRITE_CHANNELS * sizeof (s16) );

#ifdef USE_VCHIQ_SOUND
        m_Scheduler.Yield ();       // ensure the VCHIQ tasks can run
#endif
        }
    }

void CKernel::SoundCallback (void)
    {
    m_bInCallback = TRUE;
    unsigned int nSoundRemaining = m_pSound->GetQueueFramesAvail ();
    if ( nSoundRemaining == m_nSoundQueueSize ) return;
//    if ( nSoundRemaining == 0 )
//        {
//        m_Logger.Write (FromKernel, LogNotice, "Sound Buffer Exhausted");
//        m_Logger.Write (FromKernel, LogPanic, "Sound Buffer Exhausted");
//        halt ();
//        }
//    else
    if ( nSoundRemaining < m_nSoundQueueMin )
        m_nSoundQueueMin = nSoundRemaining;
    WriteSoundData (m_nSoundQueueSize - nSoundRemaining);
    m_bInCallback = FALSE;
    }

void CKernel::SoundCallbackStub (void *pv)
    {
    if ( ! s_pThis->m_bInCallback )
        {
        s_pThis->SoundCallback ();
        }
    }

void CKernel::TermSound (void)
    {
    m_Logger.Write (FromKernel, LogNotice, "TermSound called");
    m_pSound->Cancel ();
    m_Logger.Write (FromKernel, LogNotice, "Minimum buffer level = %5.1f%%",
        ( 100.0 * m_nSoundQueueMin ) / m_nSoundQueueSize);
    m_bSoundOn = FALSE;
    }

void CKernel::Yield (void)
    {
#ifdef USE_VCHIQ_SOUND
    m_Scheduler.Yield ();       // Ensure the VCHIQ tasks can run
#endif
    if ( m_bSoundOn ) SoundCallback ();           // Regularly top up sound buffers
    }
