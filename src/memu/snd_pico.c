/* snd-pico.c - MEMU sound routines for Pico - Re-implement using integer arithmatic

The SN76489A chip is driven at 4MHz.
This clock is initially divided by 16 to give a 250KHz chip clock
For reasonable audio output, require samples at appox 40KHz
Now 250KHz / 40KHz = 6.25
This suggests one sample every 6 chip clocks, which gives an audio rate of 41.667KHz
 */

#include "types.h"
#include "snd.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "sound.pio.h"
#include <string.h>
#include <stdio.h>

// #define DEBUG

#define CLKSTP  6
static byte val_in = 0;
static int reg_addr = 0;
static struct st_tone
    {
    int     reload;
    int     dcount;
    int     amp;
    int     val;
    } snd_tone[3];

static int  noise_dcount = 0;
static byte noise_mode = 0;
static word noise_sr = 0x8000;
static int  noise_amp = 0;
static int  noise_val = 0;
static int  snd_val = 0;
static int  snd_emu = 0;

static PIO  pio_snd = pio1;
static int  sm_snd_out = -1;
static int  sm_snd_pace = -1;
static uint offset_out = 0;

static absolute_time_t t1;
static absolute_time_t t2;
static int iPer;
static int nInt = 0;
static int nFill = 0;

static void snd_cfg1_tone (struct st_tone *ptone, byte val)
    {
#ifdef DEBUG
    printf ("snd_cfg1_tone (%p, 0x%02X)\n", ptone, val);
#endif
    if ( val & 0x10 )
        {
        ptone->amp = ( 15 - val & 0x0F ) << 25;
        }
    else
        {
        ptone->reload = ( ptone->reload & 0x3F0 ) | ( val & 0x0F );
        }
    if ( ptone->reload == 0 ) ptone->reload = 0x400;
#ifdef DEBUG
    printf ("amp = 0x%08X, reload = 0x%03X\n", ptone->amp, ptone->reload);
#endif
    }

static void snd_cfg2_tone (struct st_tone *ptone, byte val)
    {
#ifdef DEBUG
    printf ("snd_cfg2_tone (%p, 0x%02X)\n", ptone, val);
#endif
    ptone->reload = ( ( val & 0x3F ) << 4 ) | ( ptone->reload & 0x0f );
    if ( ptone->reload == 0 ) ptone->reload = 0x400;
#ifdef DEBUG
    printf ("amp = 0x%08X, reload = 0x%03X\n", ptone->amp, ptone->reload);
#endif
    }

static void snd_cfg_noise (byte val)
    {
#ifdef DEBUG
    printf ("snd_cfg_noise (0x%02X)\n", val);
#endif
    if ( val & 0x10 )
        {
        noise_amp = ( 15 - val & 0x0F ) << 25;
        }
    else
        {
        noise_mode = val & 0x07;
        }
#ifdef DEBUG
    printf ("noise_mode = 0x%02X, noise_amp = 0x%08X\n", noise_mode, noise_amp);
#endif
    }

void snd_out6(byte val)
    {
#ifdef DEBUG
    printf ("snd_out6 (0x%02X)\n", val);
#endif
    val_in = val;
    }

byte snd_in3(void)
    {
#ifdef DEBUG
    printf ("snd_in3\n");
#endif
    if ( val_in & 0x80 )
        {
        reg_addr = ( val_in >> 5 ) & 0x03;
        if ( reg_addr == 3 ) snd_cfg_noise (val_in);
        else snd_cfg1_tone (&snd_tone[reg_addr], val_in);
        }
    else if ( reg_addr != 3 )
        {
        snd_cfg2_tone (&snd_tone[reg_addr], val_in);
        }
    // printf ("act_buff = %d, DMA busy %d\n", act_buff, dma_channel_is_busy (dma_audio));
    return 0x03;
    }

static bool __time_critical_func(tone_step) (struct st_tone *ptone)
    {
    ptone->dcount -= CLKSTP;
    if ( ptone->dcount < 0 )
        {
        ptone->dcount = ptone->reload;
        if ( ptone->val > 0 ) ptone->val = - ptone->amp;
        else ptone->val = ptone->amp;
        return true;
        }
    return false;
    }

static void __time_critical_func(noise_shift) (void)
    {
    if ( noise_sr == 0 ) noise_sr = 0x8000;
    word hibit = noise_sr << 15;
    if ( noise_mode & 0x04 ) hibit ^= ( noise_sr & 0x0008 ) << 12;
    noise_sr = hibit | ( noise_sr >> 1 );
    if ( noise_sr & 1 ) noise_val = noise_amp;
    else noise_val = - noise_amp;
    }

static void __time_critical_func(noise_step) (bool bOut3)
    {
    if ( ( noise_mode & 0x03 ) == 0x03 )
        {
        if ( bOut3 ) noise_shift ();
        }
    else
        {
        noise_dcount -= CLKSTP;
        if ( noise_dcount < 0 )
            {
            noise_shift ();
            noise_dcount = 2 << ( noise_mode & 0x03 );
            }
        }
    }

sword __time_critical_func(snd_step) (void)
    {
    bool bOut3;
    tone_step (&snd_tone[0]);
    tone_step (&snd_tone[1]);
    bOut3 = tone_step (&snd_tone[2]);
    noise_step (bOut3);
    int val = snd_tone[0].val + snd_tone[1].val + snd_tone[2].val + noise_val;
    snd_val += ( val - snd_val ) >> 3;
    return (sword) (snd_val >> 16);
    }

void __time_critical_func(snd_fill) (void)
    {
    while ( ! pio_sm_is_tx_fifo_full (pio_snd, sm_snd_out) )
        {
        io_rw_16 *txfifo = (io_rw_16 *) &pio_snd->txf[sm_snd_out];
        *txfifo = snd_step ();
        ++nFill;
        }
    }

void __time_critical_func(snd_isr) (void)
    {
    if ( pio_snd->ints0 & ( PIO_INTR_SM0_TXNFULL_BITS << sm_snd_out ) )
        {
        t2 = get_absolute_time ();
        int iPer = absolute_time_diff_us (t1, t2);
        t1 = t2;
        snd_fill ();
        ++nInt;
        // pio_snd->ints0 = ( PIO_INTR_SM0_TXNFULL_BITS << sm_snd_out );
        }
    }

void snd_init (int emu, double latency)
    {
#ifdef DEBUG
    printf ("snd_init (%d, %f)\n", emu, latency);
#endif
    snd_emu = emu;
    memset (snd_tone, 0, sizeof (snd_tone));
    noise_dcount = 0;
    noise_mode = 0;
    noise_sr = 0x8000;
    noise_amp = 0;
    noise_val = 0;
    snd_val = 0;

    if ( snd_emu & SNDEMU_PORTAUDIO )
        {
        pio_gpio_init (pio_snd, PICO_AUDIO_I2S_DATA_PIN);
        pio_gpio_init (pio_snd, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
        pio_gpio_init (pio_snd, PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1);
    
        sm_snd_out = pio_claim_unused_sm (pio_snd, true);
        pio_sm_set_pindirs_with_mask (pio_snd, sm_snd_out,
            ( 1u << PICO_AUDIO_I2S_DATA_PIN ) | ( 3u << PICO_AUDIO_I2S_CLOCK_PIN_BASE ),
            ( 1u << PICO_AUDIO_I2S_DATA_PIN ) | ( 3u << PICO_AUDIO_I2S_CLOCK_PIN_BASE ));
        offset_out = pio_add_program (pio_snd, &sound_out_program);
#ifdef DEBUG
        printf ("sm_snd_out = %d, offset_out = %d\n", sm_snd_out, offset_out);
#endif
        pio_sm_config c_out = sound_out_program_get_default_config (offset_out);
        sm_config_set_out_pins (&c_out, PICO_AUDIO_I2S_DATA_PIN, 1);
        sm_config_set_sideset_pins (&c_out, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
        float div = 6 * clock_get_hz (clk_sys) / ( 64 * 250000 );
#ifdef DEBUG
        printf ("div = %f\n", div);
#endif
        sm_config_set_clkdiv (&c_out, div);
        sm_config_set_out_shift (&c_out, false, true, 32);
        pio_sm_init (pio_snd, sm_snd_out, offset_out, &c_out);
        /*
          sm_snd_pace = pio_claim_unused_sm (pio_snd, true);
        uint offset_pace = pio_add_program (pio_snd, &snd_pace_program);
        pio_sm_config c_pace = snd_pace_get_default_config (offset_pace);
        sm_config_set_clkdiv (&c_pace, div);
        pio_sm_init (pio_snd, sm_snd_pace, offset_pace, &c_pace);
        */

        snd_fill ();
        irq_set_exclusive_handler (PIO1_IRQ_0, snd_isr);
        irq_set_enabled (PIO1_IRQ_0, true);
        pio_snd->inte0 = ( PIO_INTR_SM0_TXNFULL_BITS << sm_snd_out );
        pio_sm_set_enabled (pio_snd, sm_snd_out, true);
        }
    }

void snd_term (void)
    {
#ifdef DEBUG
    printf ("snd_term\n");
#endif
    if ( snd_emu & SNDEMU_PORTAUDIO )
        {
        pio_sm_set_enabled (pio_snd, sm_snd_out, false);
        pio_snd->inte0 = 0;
        irq_remove_handler (PIO1_IRQ_0, snd_isr);
        pio_remove_program (pio_snd, &sound_out_program, offset_out);
        pio_sm_unclaim (pio_snd, sm_snd_out);
        snd_emu = 0;
        }
    }

void snd_query (void)
    {
    printf ("Period = %d us, Frequency = %f KHz\n", iPer, 1000.0 / iPer);
    printf ("nInt = %d, nFill = %d\n", nInt, nFill);
    printf ("sm_pc = %d, fifo = %d, stalled = %d\n",
        pio_sm_get_pc (pio_snd, sm_snd_out),
        pio_sm_get_tx_fifo_level (pio_snd, sm_snd_out),
        pio_sm_is_exec_stalled (pio_snd, sm_snd_out));
    }

#if 0
#include "audio_i2s.pio.h"
static int  act_buff = 0;

static PIO  pio_audio = pio1;
static int  sm_audio = -1;
static int  dma_audio = -1;

static int nsnd = 0;
static int smin = 0;
static int smax = 0;
static int smin2 = 0;
static int smax2 = 0;
static volatile BOOLEAN bShow = FALSE;

// To have audio interrupts at ~100Hz, this requires audio buffers of 400 samples
#define NSBUF   2
#define SBLEN   400

static sword snd_buff[NSBUF][SBLEN];

void snd_query (void)
    {
    printf ("nsnd = %d, smin = 0x%08X, smax = 0x%08X, smin2 = 0x%08X, smax2 = 0x%08X\n",
        nsnd, smin, smax, smin2, smax2);
    nsnd = 0;
    smin = 0;
    smax = 0;
    smin2 = 0;
    smax2 = 0;
    bShow = TRUE;
    }

void __time_critical_func(snd_fill_buffer) (int iBuff)
    {
    smin = 0;
    smax = 0;
    smin2 = 0;
    smax2 = 0;
    if ( bShow ) printf ("snd_fill_buffer (%d)\n", iBuff);
    sword *ptr = snd_buff[iBuff];
    for (int i = 0; i < SBLEN; ++i)
        {
        bool bOut3;
#if 0
        tone_step (&snd_tone[0]);
        tone_step (&snd_tone[1]);
        bOut3 = tone_step (&snd_tone[2]);
#endif
        noise_step (bOut3);
        int val = snd_tone[0].val + snd_tone[1].val + snd_tone[2].val + noise_val;
        snd_val += ( val - snd_val ) >> 3;
        *ptr = (sword) (snd_val >> 16);
        /*
        if ( bShow )
            {
            printf ("i = %3d, tone 0 = 0x%08X, val = 0x%08X, snd_val = 0x%08X, *ptr = 0x%08X, ptr = %p\n",
                i, snd_tone[0].val, val, snd_val, *ptr, ptr);
            }
        */
        if ( val < 0 ) ++smin;
        else if ( val > 0 ) ++smax;
        if ( snd_val < 0 ) ++smin2;
        else if ( snd_val > 0 ) ++smax2;
        /*
        if ( snd_val > smax ) smax = snd_val;
        else if ( snd_val < smin ) smin = snd_val;
        if ( *ptr > smax2 ) smax2 = *ptr;
        else if ( *ptr < smin2 ) smin2 = *ptr;
        ++nsnd;
        */
        ++ptr;
        }
    // bShow = FALSE;
    }

static absolute_time_t t1;
static absolute_time_t t2;

void __time_critical_func(snd_queue) (void)
    {
    // printf ("snd_queue\n");
    // dma_channel_transfer_from_buffer_now (dma_audio, snd_buff[act_buff], SBLEN);
    io_rw_16 *txfifo = (io_rw_16 *) &pio_audio->txf[sm_audio];
    // dma_channel_set_read_addr (dma_audio, snd_buff[act_buff], false);
    // dma_channel_set_write_addr (dma_audio, txfifo, false);
    // dma_channel_set_trans_count (dma_audio, SBLEN, false);
    // dma_channel_start (dma_audio);
    dma_channel_config c = dma_channel_get_default_config (dma_audio);
    channel_config_set_transfer_data_size (&c, DMA_SIZE_16);
    channel_config_set_enable (&c, true);
    channel_config_set_read_increment (&c, true);
    channel_config_set_write_increment (&c, false);
    channel_config_set_dreq (&c, DREQ_PIO1_TX0 + sm_audio);
    dma_channel_set_irq1_enabled (dma_audio, true);
    dma_channel_configure (dma_audio, &c, txfifo, snd_buff[act_buff], SBLEN, true);
    t1 = get_absolute_time ();
    /*
    printf ("snd_queue: act_buff = %d, DMA busy %d, int = %X\n", act_buff, dma_channel_is_busy (dma_audio),
        dma_hw->ints1 & ( 1u << dma_audio ));
    smin = 0;
    smax = 0;
    smin2 = 0;
    smax2 = 0;
    for (int i = 0; i < SBLEN; ++i)
        {
        if ( snd_buff[0][i] < 0 ) smin += 1; // snd_buff[0][i];
        else if ( snd_buff[0][i] > 0 ) smax += 1; // snd_buff[0][i];
        if ( snd_buff[1][i] < 0 ) smin2 += 1; // snd_buff[1][i];
        else if ( snd_buff[1][i] > 0 ) smax2 += 1; // snd_buff[1][i];
        }
    */
    act_buff = 1 - act_buff;
    snd_fill_buffer (act_buff);
    if ( bShow )
        {
        /*
        for (int i = 0; i < SBLEN; ++i)
            {
            printf ("act_buff = %d, snd_buff[0][%3d] = 0x%08X, snd_buff[1][%3d] = 0x%08X\n",
                act_buff, i, snd_buff[0][i], i, snd_buff[1][i]);
            }
        */
        t2 = get_absolute_time ();
        int64_t iPer = absolute_time_diff_us (t1, t2);
        printf ("Period = %ld us, freq = %f kHz\n", iPer, 1000.0 / iPer);
        printf ("sm_pc = %d, fifo = %d, stalled = %d\n",
            pio_sm_get_pc (pio_audio, sm_audio),
            pio_sm_get_tx_fifo_level (pio_audio, sm_audio),
            pio_sm_is_exec_stalled (pio_audio, sm_audio));
        }
    // t1 = get_absolute_time ();
    if ( bShow ) printf ("snd_queue done\n");
    bShow = FALSE;
    // memset (snd_buff[act_buff], 0x7000 * (1 - 2 * act_buff), sizeof (snd_buff[0]));
    }

void __time_critical_func(snd_isr) (void)
    {
    if ( dma_hw->ints1 & ( 1u << dma_audio ) )
        {
        dma_hw->ints1 = ( 1u << dma_audio );
        t2 = get_absolute_time ();
        // snd_queue ();
        }
    }

void snd_init(int emu, double latency)
    {
    memset (snd_buff, 0, sizeof (snd_buff));
    reg_addr = 0;
    for (int i = 0; i < 3; ++i)
        {
        snd_tone[i].reload = 0x400;
        snd_tone[i].dcount = 0;
        snd_tone[i].amp = 0;
        snd_tone[i].val = 0;
        }
    noise_mode = 0;
    noise_sr = 0x8000;
    noise_amp = 0;
    noise_val = 0;
    snd_val = 0;
    sm_audio = pio_claim_unused_sm (pio_audio, true);
    dma_audio = dma_claim_unused_channel (true);
    uint offset = pio_add_program (pio_audio, &audio_i2s_program);
    printf ("sm_audio = %d, dma_audio = %d, offset = %d\n", sm_audio, dma_audio, offset);
    float div = 6 * clock_get_hz (clk_sys) / ( 64 * 250000 );
    printf ("div = %f\n", div);
    pio_sm_set_clkdiv (pio_audio, sm_audio, div);
    printf ("Configure audio gpio pins\n");
    pio_gpio_init (pio_audio, PICO_AUDIO_I2S_DATA_PIN);
    pio_gpio_init (pio_audio, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    pio_gpio_init (pio_audio, PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1);
    printf ("Initialise audio PIO program\n");
    audio_i2s_program_init (pio_audio, sm_audio, offset, PICO_AUDIO_I2S_DATA_PIN,
        PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    pio_sm_set_enabled (pio_audio, sm_audio, true);
    printf ("Configure audio DMA\n");
    /*
    io_rw_16 *txfifo = (io_rw_16 *) &pio_audio->txf[sm_audio];
    dma_channel_config c = dma_channel_get_default_config (dma_audio);
    channel_config_set_transfer_data_size (&c, DMA_SIZE_16);
    channel_config_set_enable (&c, true);
    channel_config_set_read_increment (&c, true);
    channel_config_set_write_increment (&c, false);
    channel_config_set_dreq (&c, DREQ_PIO1_TX0 + sm_audio);
    dma_channel_configure (dma_audio, &c, txfifo, NULL, SBLEN, false);
    dma_channel_set_irq1_enabled (dma_audio, true);
    */
    irq_set_exclusive_handler (DMA_IRQ_1, snd_isr);
    dma_hw->ints1 = ( 1u << dma_audio );
    irq_set_enabled (DMA_IRQ_1, true);
    printf ("snd_queue\n");
    // bShow = TRUE;
    snd_queue ();
    absolute_time_t t3 = get_absolute_time ();
    // printf ("act_buff = %d, DMA busy %d\n", act_buff, dma_channel_is_busy (dma_audio));
    int iPer = absolute_time_diff_us (t1, t2);
    int iPer2 = absolute_time_diff_us (t1, t3);
    int iBusy = dma_channel_is_busy (dma_audio);
    printf ("Period = %d us, Period 2 = %d us, DMA busy %d\n", iPer, iPer2, iBusy);
    printf ("Returned from snd_queue\n");
    printf ("snd_init completed\n");
    }

void snd_term(void)
    {
    irq_set_enabled (DMA_IRQ_1, false);
    dma_channel_abort (dma_audio);
    dma_channel_unclaim (dma_audio);
    pio_sm_unclaim (pio_audio, sm_audio);
    }
#endif
