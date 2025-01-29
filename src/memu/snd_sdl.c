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

#include <SDL3/SDL.h>

static SDL_AudioStream *astream;
static SDL_AudioSpec aspec;
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

void snd_callback (void *user, SDL_AudioStream *stream, int len_add, int len_tot)
    {
    float *buffer = (float *) emalloc (len_add * sizeof (float));
    float *out = buffer;
    int i;
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
    for ( i = 0; i < len_add; ++i )
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
        *out++ = snd_lastvol = val;
        }
//    diag_message (DIAG_INIT, "Exit snd_callback: aMin = %6.3f aMax = %6.3f", aMin, aMax);
    SDL_PutAudioStreamData (stream, buffer, len_add);
    free (buffer);
    }
/*...e*/

/*...ssnd_init:0:*/
void snd_init(int emu, double latency)
    {
    if ( emu & SNDEMU_PORTAUDIO )
        {
        SDL_AudioSpec areq;
        snd_emu = emu;
        memset (&areq, 0, sizeof (areq));
        areq.freq = FREQ;
        areq.format = SDL_AUDIO_F32;
        areq.channels = 1;
        SDL_InitSubSystem (SDL_INIT_AUDIO);
        astream = SDL_OpenAudioDeviceStream (SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &areq, snd_callback, NULL);
        SDL_ResumeAudioStreamDevice (astream);
        }
    }
/*...e*/
/*...ssnd_term:0:*/
void snd_term(void)
    {
    if ( snd_emu & SNDEMU_PORTAUDIO )
        {
        SDL_DestroyAudioStream (astream);
        }
    snd_emu = 0;
    }
/*...e*/
