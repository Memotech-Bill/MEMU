/*

  snd.c - Sound

  In this code, the tone generators are numbered 0 to 2.
  In the documentation, they're referred to as 1 to 3.

  See http://www.smspower.org/Development/SN76489.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "types.h"
#include "diag.h"
#include "common.h"
#include "snd.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vsnd\46\h:0:*/
/*...e*/

/*...svars:0:*/
#define FREQ 44100

static int snd_emu = 0;

typedef struct
    {
    word freq;
    byte atten;
    float phase;
    } CHANNEL;

static CHANNEL snd_channels[3];
static int snd_channel;
static byte snd_noise_ctrl;
static byte snd_noise_atten;
static float snd_noise_phase;
static word snd_noise_shifter;
static int snd_noise_bit;
static float snd_lastvol;

#ifdef __circle__
#include "kfuncs.h"
static float aMin = 0.0;
static float aMax = 0.0;
#else
#include "portaudio.h"
static PaStream *snd_paStream;
#endif
/*...e*/

/*...ssnd_out6:0:*/
/*...ssnd_set_tone_freq_low:0:*/
static void snd_set_tone_freq_low(int channel, byte val)
    {
    snd_channel = channel; /* for subsequent high update */
    snd_channels[channel].freq = (snd_channels[channel].freq & 0x3f0)
        | (val & 0x0f);
    diag_message(DIAG_SND_REGISTERS, "Tone %d frequency low changed, freq=%d", channel, snd_channels[channel].freq);
    }
/*...e*/
/*...ssnd_set_tone_freq_high:0:*/
static void snd_set_tone_freq_high(byte val)
    {
    snd_channels[snd_channel].freq = (snd_channels[snd_channel].freq & 0x00f)
        | (((word)val<<4) & 0x3f0);
    diag_message(DIAG_SND_REGISTERS, "Tone %d frequency high changed, freq=%d", snd_channel, snd_channels[snd_channel].freq);
    }
/*...e*/
/*...ssnd_set_tone_atten:0:*/
static void snd_set_tone_atten(int channel, byte val)
    {
    snd_channels[channel].atten = (val&0x0f);
    diag_message(DIAG_SND_REGISTERS, "Tone %d attenuation changed, atten=%d", channel, snd_channels[channel].atten);
    }
/*...e*/
/*...ssnd_set_noise_ctrl:0:*/
static void snd_set_noise_ctrl(byte val)
    {
    static const char *shift_rates[] =
        { "N/512", "N/1024", "N/2048", "tone generator 2 output" };
    snd_noise_ctrl = (val & 0x07);
	snd_noise_shifter = 0x8000;
    diag_message(DIAG_SND_REGISTERS, "Noise control changed, %s noise, %s shift rate",
        (snd_noise_ctrl & 0x04) ? "white" : "periodic",
        shift_rates[snd_noise_ctrl&0x03]);
    }
/*...e*/
/*...ssnd_set_noise_atten:0:*/
static void snd_set_noise_atten(byte val)
    {
    snd_noise_atten = (val&0x0f);
    diag_message(DIAG_SND_REGISTERS, "Noise attenuation changed, atten=%d", snd_noise_atten);
    }
/*...e*/

void snd_out6(byte val)
    {
    if ( val & 0x80 )
        /* Write to register */
        switch ( val & 0x70 )
            {
            case 0x00:  snd_set_tone_freq_low(0, val);  break;
            case 0x10:  snd_set_tone_atten(0, val); break;
            case 0x20:  snd_set_tone_freq_low(1, val);  break;
            case 0x30:  snd_set_tone_atten(1, val); break;
            case 0x40:  snd_set_tone_freq_low(2, val);  break;
            case 0x50:  snd_set_tone_atten(2, val); break;
            case 0x60:  snd_set_noise_ctrl(val);    break;
            case 0x70:  snd_set_noise_atten(val);   break;
            }
    else
        /* High 6 bits of frequency */
        snd_set_tone_freq_high(val);
    }
/*...e*/
/*...ssnd_in3:0:*/
/* There is no data returned by sound hardware when inputing from port 3.
   So we return the most recent value fetched over the bus.
   If the code did in a,(3), this will be 3, and Pothole Pete relies on this.
   Not sure how we'd cope with in a,(c). */

byte snd_in3(void)
    {
    return (byte) 0x03;
    }
/*...e*/

/*...ssnd_callback:0:*/
/*...ssnd_step:0:*/
/* Its important we cope with a frequency value of 0.
   Kilopede doesn't set a frequency initially, and yet varies the volume.
   We should get a low note, rather than no sound. */

static float snd_step(word freq)
    {
    float hz;
    if ( freq == 0 )
        freq = 0x400;
    hz = (float) ( 4000000.0 / ( 32.0 * (float) freq ) );
    return (float) ( hz * 2.0 ) / (float) FREQ;
    }
/*...e*/
/*...ssnd_scale:0:*/
static float snd_scale(byte atten)
    {
    return (float) ( 0.25 * ( (float) (15-atten) / 15.0 ) );
    }
/*...e*/
/*...ssnd_next_noise_bit:0:*/
static int snd_next_noise_bit(void)
    {
    word input = ( snd_noise_ctrl & 0x04 )
        /* White noise */
        ? ((snd_noise_shifter&0x0008)<<12) ^ ((snd_noise_shifter&0x0001)<<15)
        /* Periodic noise */
        :                                    ((snd_noise_shifter&0x0001)<<15);
    snd_noise_shifter = input | ( snd_noise_shifter >> 1 );
    if ( snd_noise_shifter == 0 )
        snd_noise_shifter = 0x8000;
    return (int) ( snd_noise_shifter & 1 );
    }
/*...e*/

#ifdef __circle__
int snd_callback (short *outputBuffer, unsigned long framesPerBuffer)
    {
    short *out = outputBuffer;
#else    
    static int snd_callback(
        const void *inputBuffer,
        void *outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData
        )
        {
        float *out = (float *) outputBuffer;
#endif
        unsigned long i;
        int c;
        float steps[3];
        float scales[3];
        float step_noise;
        float scale_noise;
//    diag_message (DIAG_INIT, "snd_callback (0x%08X, %d)", outputBuffer, framesPerBuffer);
        for ( c = 0; c < 3; c++ )
            {
            word freq = snd_channels[c].freq;
            steps[c] = snd_step(freq);
            scales[c] = snd_scale(snd_channels[c].atten);
            snd_channels[c].phase = (float) fmod(snd_channels[c].phase, 2.0);
            }
        switch ( snd_noise_ctrl & 0x03 )
            {
            case 0x00:  step_noise = snd_step(16);  break;
            case 0x01:  step_noise = snd_step(32);  break;
            case 0x02:  step_noise = snd_step(64);  break;
            case 0x03:  step_noise = steps[2];      break;
            }
        scale_noise = snd_scale(snd_noise_atten);
        snd_noise_phase = (float) fmod(snd_noise_phase, 2.0);
        for ( i = 0; i < framesPerBuffer; i++ )
            {
            float val = 0.0;
            for ( c = 0; c < 3; c++ )
                {
                if ( (unsigned) snd_channels[c].phase & 1 )
                    val += scales[c];
                else
                    val -= scales[c];
                snd_channels[c].phase += steps[c];
                }
            if ( ( (unsigned) (snd_noise_phase + step_noise) & 1 ) !=
                ( (unsigned) (snd_noise_phase             ) & 1 ) )
                snd_noise_bit = snd_next_noise_bit();
            if ( snd_noise_bit )
                val += scale_noise;
            else
                val -= scale_noise;
            snd_noise_phase += step_noise;
            /* This bit is to try to remove the sharp edges of the
               square wave which we are generating */
            val = (float) ( val * 0.1 + snd_lastvol * 0.9 );
#ifdef __circle__
            snd_lastvol = val;
            *out = (short)( 32760 * val );
            ++out;
            if ( val < aMin ) aMin = val;
            else if ( val > aMax ) aMax = val;
#else
            *out++ = snd_lastvol = val;
#endif
            }
//    diag_message (DIAG_INIT, "Exit snd_callback: aMin = %6.3f aMax = %6.3f", aMin, aMax);
        return 0;
        }
/*...e*/

/*...ssnd_init:0:*/
#ifdef __circle__
    void snd_init(int emu, double latency)
        {
        int i;
        snd_emu = emu;
        for ( i = 0; i < 3; i++ )
            {
            snd_channels[i].freq  = 0;
            snd_channels[i].atten = 15;
            snd_channels[i].phase = 0.0;
            }
        snd_channel = 0;
        snd_noise_ctrl    = 0;
        snd_noise_atten   = 15;
        snd_noise_phase   = 0;
        snd_noise_shifter = 0x8000;
        snd_noise_bit     = 0;
        snd_lastvol = 0.0;
        if ( snd_emu & SNDEMU_PORTAUDIO )
            {
            if ( ! InitSound () ) snd_emu = 0;
            }
        }
#else
    void snd_init(int emu, double latency)
        {
        int i;
        PaStreamParameters paParamsOut;
        PaDeviceIndex paDevice;
        const PaDeviceInfo *paDeviceInfo;
        PaTime paLatency;
        snd_emu = emu;
        for ( i = 0; i < 3; i++ )
            {
            snd_channels[i].freq  = 0;
            snd_channels[i].atten = 15;
            snd_channels[i].phase = 0.0;
            }
        snd_channel = 0;
        snd_noise_ctrl    = 0;
        snd_noise_atten   = 15;
        snd_noise_phase   = 0;
        snd_noise_shifter = 0x8000;
        snd_noise_bit     = 0;
        snd_lastvol = 0.0;
        if ( snd_emu & SNDEMU_PORTAUDIO )
            {
            PaError paErr;
            PaTime paLatencyLow, paLatencyHigh;
#ifdef UNIX
            int fd_stderr, fd_null;
            if ( ! diag_flags[DIAG_SND_INIT] )
                {
                fd_stderr = dup(2);
                fd_null = open("/dev/null", O_WRONLY);
                dup2(fd_null, 2);
                }
#endif
            paErr = Pa_Initialize();
#ifdef UNIX
            if ( ! diag_flags[DIAG_SND_INIT] )
                {
                dup2(fd_stderr, 2);
                close(fd_null);
                }
#endif
            if ( paErr != paNoError )
                {
                if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal("cannot initialise portaudio: %s", Pa_GetErrorText(paErr));
                snd_emu = 0;
                return;
                }
            paDevice = Pa_GetDefaultOutputDevice();
            if ( paDevice == paNoDevice )
                {
                Pa_Terminate();
                if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal("no default portaudio output device");
                snd_emu = 0;
                return;
                }
            paDeviceInfo = Pa_GetDeviceInfo(paDevice);
            paLatencyLow  = paDeviceInfo->defaultLowOutputLatency;
            paLatencyHigh = paDeviceInfo->defaultHighOutputLatency;
            diag_message(DIAG_SND_LATENCY, "Device low latency value of %8.6lfs and high latency value of %8.6lfs",
                (double) paLatencyLow, (double) paLatencyHigh);
            /* At the moment I observe that I need the shortest latency
               I can get on Windows, but on Linux, if I do this, I get lots
               of ALSA lib underrun errors. So I pick a different value. */
#if defined(UNIX)
            paLatency = ( paLatencyLow + paLatencyHigh ) / 2;
#elif defined(WIN32)
            paLatency = paLatencyLow;
#else
#error Dont know what default latency value to use for this platform
#endif
            diag_message(DIAG_SND_LATENCY, "Chosen latency %8.6lfs",
                (double) paLatency);
            if ( latency != 0.0 )
                {
                if ( latency < paLatencyLow )
                    paLatency = paLatencyLow;
                else if ( latency > paLatencyHigh )
                    paLatency = paLatencyHigh;
                else
                    paLatency = (PaTime) latency;
                diag_message(DIAG_SND_LATENCY, "User overrides latency to %8.6lfs",
                    (double) paLatency);
                }
            paParamsOut.device                    = paDevice;
            paParamsOut.channelCount              = 1; /* mono output */
            paParamsOut.sampleFormat              = paFloat32;
            paParamsOut.suggestedLatency          = paLatency; 
            paParamsOut.hostApiSpecificStreamInfo = NULL;
            paErr = Pa_OpenStream(
                &snd_paStream,
                NULL,
                &paParamsOut,
                FREQ,
                paFramesPerBufferUnspecified,
                paNoFlag,
                snd_callback,
                NULL
                );
            if ( paErr != paNoError )
                {
                Pa_Terminate();
                if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] )
                    fatal("cannot open default audio stream: %s", Pa_GetErrorText(paErr));
                snd_emu = 0;
                return;
                }
            paErr = Pa_StartStream(snd_paStream);
            if ( paErr != paNoError )
                {
                Pa_CloseStream(snd_paStream);
                Pa_Terminate();
                if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] )
                    fatal("cannot start audio stream: %s", Pa_GetErrorText(paErr));
                snd_emu = 0;
                return;
                }
            }
        }
#endif
/*...e*/
/*...ssnd_term:0:*/
    void snd_term(void)
        {
        if ( snd_emu & SNDEMU_PORTAUDIO )
            {
#ifdef __circle__
            TermSound ();
            diag_message (DIAG_SND_REGISTERS, "Sound range: %6.3f to %6.3f", aMin, aMax);
#else
            Pa_StopStream(snd_paStream);
            Pa_CloseStream(snd_paStream);
            Pa_Terminate();
#endif
            }
        snd_emu = 0;
        }
/*...e*/
