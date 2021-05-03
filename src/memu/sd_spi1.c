/*  sd_spi.c - Routines for accessing SD card using SPI routines */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "sd_spi.pio.h"
#include "sd_spi.h"

#define SD_CS_PIN       ( PICO_SD_DAT0_PIN + 3 )
#define SD_CLK_PIN      PICO_SD_CLK_PIN
#define SD_MOSI_PIN     PICO_SD_CMD_PIN
// #define SD_MOSI_PIN     PICO_SD_DAT0_PIN
#define SD_MISO_PIN     PICO_SD_DAT0_PIN

PIO pio_sd = pio1;
uint sd_sm;
SD_TYPE sd_type = sdtpUnk;

void sd_spi_load (void)
    {
    gpio_init (SD_CS_PIN);
    gpio_set_dir (SD_CS_PIN, GPIO_OUT);
    gpio_pull_up (SD_MISO_PIN);
    // gpio_pull_down (SD_CLK_PIN);
    gpio_put (SD_CS_PIN, 1);
    uint offset = pio_add_program (pio_sd, &sd_spi_program);
    printf ("offset = %d\n", offset);
    sd_sm = pio_claim_unused_sm (pio_sd, true);
    printf ("sd_sm = %d\n", sd_sm);
    pio_sm_config c = sd_spi_program_get_default_config (offset);
    sm_config_set_out_pins (&c, SD_MOSI_PIN, 1);
    sm_config_set_in_pins (&c, SD_MISO_PIN);
    sm_config_set_sideset_pins (&c, SD_CLK_PIN);
    sm_config_set_out_shift (&c, false, true, 8);
    sm_config_set_in_shift (&c, false, true, 8);
    pio_sm_set_pins_with_mask(pio_sd, sd_sm, (1u << SD_MOSI_PIN),
        (1u << SD_CLK_PIN) | (1u << SD_MOSI_PIN));
    pio_sm_set_pindirs_with_mask(pio_sd, sd_sm,  (1u << SD_CLK_PIN) | (1u << SD_MOSI_PIN),
        (1u << SD_CLK_PIN) | (1u << SD_MOSI_PIN) | (1u << SD_MISO_PIN));
    pio_gpio_init (pio_sd, SD_CLK_PIN);
    pio_gpio_init (pio_sd, SD_MOSI_PIN);
    pio_gpio_init (pio_sd, SD_MISO_PIN);
    hw_set_bits (&pio_sd->input_sync_bypass, 1u << SD_MISO_PIN);
    pio_sm_init (pio_sd, sd_sm, offset, &c);
    pio_sm_set_enabled (pio_sd, sd_sm, true);
    }

void sd_spi_freq (float freq)
    {
    printf ("freq = %f, clock = %d\n", freq, clock_get_hz (clk_sys));
    float div = clock_get_hz (clk_sys) / ( 4000.0 * freq );
    printf ("div = %f\n", div);
    pio_sm_set_clkdiv (pio_sd, sd_sm, div);
    }

void sd_spi_chpsel (bool sel)
    {
    gpio_put (SD_CS_PIN, ! sel);
    }

// Do 8 bit accesses on FIFO, so that write data is byte-replicated. This
// gets us the left-justification for free (for MSB-first shift-out)
uint8_t sd_spi_put (const uint8_t *src, size_t len)
    {
    size_t tx_remain = len;
    size_t rx_remain = len;
    uint8_t resp;
    io_rw_8 *txfifo = (io_rw_8 *) &pio_sd->txf[sd_sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &pio_sd->rxf[sd_sm];
    while (tx_remain || rx_remain)
        {
        if (tx_remain && !pio_sm_is_tx_fifo_full (pio_sd, sd_sm))
            {
            *txfifo = *src;
            ++src;
            --tx_remain;
            }
        if (rx_remain && !pio_sm_is_rx_fifo_empty (pio_sd, sd_sm))
            {
            resp = *rxfifo;
            --rx_remain;
            }
        }
    return resp;
    }

void sd_spi_get (uint8_t *dst, size_t len)
    {
    size_t tx_remain = len;
    size_t rx_remain = len;
    uint8_t resp;
    io_rw_8 *txfifo = (io_rw_8 *) &pio_sd->txf[sd_sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &pio_sd->rxf[sd_sm];
    while (tx_remain || rx_remain)
        {
        if (tx_remain && !pio_sm_is_tx_fifo_full (pio_sd, sd_sm))
            {
            *txfifo = 0xFF;
            --tx_remain;
            }
        if (rx_remain && !pio_sm_is_rx_fifo_empty (pio_sd, sd_sm))
            {
            *dst = *rxfifo;
            ++dst;
            --rx_remain;
            }
        }
    }

uint8_t sd_spi_clk (size_t len)
    {
    size_t tx_remain = len;
    size_t rx_remain = len;
    uint8_t resp;
    io_rw_8 *txfifo = (io_rw_8 *) &pio_sd->txf[sd_sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &pio_sd->rxf[sd_sm];
    while (tx_remain || rx_remain)
        {
        if (tx_remain && !pio_sm_is_tx_fifo_full (pio_sd, sd_sm))
            {
            *txfifo = 0xFF;
            --tx_remain;
            }
        if (rx_remain && !pio_sm_is_rx_fifo_empty (pio_sd, sd_sm))
            {
            resp = *rxfifo;
            --rx_remain;
            }
        }
    }

#define SD_R1_OK        0x00
#define SD_R1_IDLE      0x01
#define SD_R1_ILLEGAL   0x04

#define SDBT_START	    0xFE	// Start of data token
#define SDBT_ERRMSK	    0xF0	// Mask to select zero bits in error token
#define SDBT_ERANGE	    0x08	// Out of range error flag
#define SDBT_EECC	    0x04	// Card ECC failed
#define SDBT_ECC	    0x02	// CC error
#define SDBT_ERROR	    0x01	// Error
#define SDBT_ECLIP	    0x10	// Value above all error bits

static uint8_t cmd0[]   = {  0, 0x00, 0x00, 0x00, 0x00, 0x95 }; // Go Idle
static uint8_t cmd8[]   = {  8, 0x00, 0x00, 0x01, 0xAA, 0x87 }; // Set interface condition
static uint8_t cmd17[]  = { 17, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Read single block
static uint8_t cmd24[]  = { 24, 0x00, 0x00, 0x00, 0x00, 0x00 }; // Write single block
static uint8_t cmd55[]  = { 55, 0x00, 0x00, 0x01, 0xAA, 0x65 }; // Application command follows
static uint8_t cmd58[]  = { 58, 0x00, 0x00, 0x00, 0x00, 0xFD }; // Read Operating Condition Register
static uint8_t acmd41[] = { 41, 0x40, 0x00, 0x00, 0x00, 0x77 }; // Set operation condition

uint8_t sd_spi_cmd (uint8_t *src)
    {
    size_t tx_remain = 6;
    size_t rx_remain = 6;
    uint8_t resp;
    io_rw_8 *txfifo = (io_rw_8 *) &pio_sd->txf[sd_sm];
    io_rw_8 *rxfifo = (io_rw_8 *) &pio_sd->rxf[sd_sm];
    *txfifo = 0xFF;
    ++rx_remain;
    *txfifo = *src | 0x40;
    ++src;
    --tx_remain;
    while (tx_remain || rx_remain)
        {
        while (rx_remain && !pio_sm_is_rx_fifo_empty (pio_sd, sd_sm))
            {
            resp = *rxfifo;
            printf (" 0x%02X", resp);
            --rx_remain;
            }
        if (tx_remain && !pio_sm_is_tx_fifo_full (pio_sd, sd_sm))
            {
            *txfifo = *src;
            ++src;
            --tx_remain;
            }
        }
    printf ("\n");
    int nTry = 16;
    while ( ( resp & 0x80 ) && ( nTry > 0 ) )
        {
        *txfifo = 0xFF;
        while ( pio_sm_is_rx_fifo_empty (pio_sd, sd_sm) )
            {
            tight_loop_contents();
            }
        resp = *rxfifo;
        printf (" 0x%02X", resp);
        --nTry;
        }
    printf ("\n");
    return resp;
    }

bool sd_spi_init (void)
    {
    uint8_t chk[4];
    uint8_t resp;
    printf ("sd_spi_init\n");
    sd_type = sdtpUnk;
    sd_spi_freq (100);
    sd_spi_load ();
    sd_spi_chpsel (false);
    sd_spi_clk (10);
    for (int i = 0; i < 8; ++i)
        {
        sd_spi_chpsel (true);
        printf ("Go idle\n");
        resp = sd_spi_cmd (cmd0);
        printf ("   Response 0x%02X\n", resp);
        if ( resp == SD_R1_IDLE ) break;
        sd_spi_chpsel (false);
        sleep_ms (1);
        }
    if ( resp != SD_R1_IDLE )
        {
        printf ("Failed @1\n");
        return false;
        }
    for (int i = 0; i < 10; ++i)
        {
        printf ("Set interface condition\n");
        resp = sd_spi_cmd (cmd8);
        printf ("   Response 0x%02X\n", resp);
        if ( resp == SD_R1_IDLE )
            {
            sd_spi_get (chk, 4);
            printf ("   Data 0x%02X 0x%02X 0x%02X 0x%02X\n", chk[0], chk[1], chk[2], chk[3]);
            if ( chk[3] == cmd8[4] ) break;
            }
        else if ( resp & SD_R1_ILLEGAL )
            {
            break;
            }
        }
    if ( resp & SD_R1_ILLEGAL )
        {
        printf ("Version 1 SD Card\n");
        sd_type = sdtpVer1;
        }
    else if ( resp != SD_R1_IDLE )
        {
        printf ("Failed @2\n");
        sd_spi_chpsel (false);
        return false;
        }
    for (int i = 0; i < 8; ++i)
        {
        printf ("Set operation condition\n");
        resp = sd_spi_cmd (cmd55);
        resp = sd_spi_cmd (acmd41);
        printf ("   Response 0x%02X\n", resp);
        if ( resp == SD_R1_OK ) break;
        }
    if ( resp != SD_R1_OK )
        {
        printf ("Failed @3\n");
        sd_spi_chpsel (false);
        return false;
        }
    if ( sd_type == sdtpUnk )
        {
        printf ("Read Operating Condition Register\n");
        resp = sd_spi_cmd (cmd58);
        printf ("   Response 0x%02X\n", resp);
        if ( resp != SD_R1_OK )
            {
            printf ("Failed @3\n");
            sd_spi_chpsel (false);
            return false;
            }
        sd_spi_get (chk, 4);
        printf ("   Data 0x%02X 0x%02X 0x%02X 0x%02X\n", chk[0], chk[1], chk[2], chk[3]);
        if ( chk[0] & 0x40 )
            {
            printf ("High capacity SD card\n");
            sd_type = sdtpHigh;
            }
        else
            {
            printf ("Version 2 SD card\n");
            sd_type = sdtpVer2;
            }
        }
    printf ("SD Card initialised\n");
    sd_spi_freq (25000);
    return true;
    }

void sd_spi_term (void)
    {
    printf ("SD Card terminate\n");
    sd_type = sdtpUnk;
    sd_spi_chpsel (false);
    sd_spi_freq (400);
    }

void sd_spi_set_crc7 (uint8_t *pcmd)
    {
    uint8_t crc = 0;
    for (int i = 0; i < 5; ++i)
        {
        uint8_t v = *pcmd;
        for (int j = 0; j < 8; ++j)
            {
            if ( ( v ^ crc ) & 0x80 ) crc ^= 0x09;
            crc <<= 1;
            v <<= 1;
            }
        ++pcmd;
        }
    *pcmd = crc + 1;
    }

void sd_spi_set_lba (uint lba, uint8_t *pcmd)
    {
    if ( sd_type != sdtpHigh ) lba <<= 9;
    pcmd += 4;
    for (int i = 0; i < 4; ++i)
        {
        *pcmd = lba & 0xFF;
        lba >>= 8;
        --pcmd;
        }
    sd_spi_set_crc7 (pcmd);
    }

uint16_t sd_spi_crc16 (const uint8_t *buff)
    {
    uint8_t out;
    int dma = dma_claim_unused_channel (true);
    dma_channel_config c = dma_channel_get_default_config (dma);
    channel_config_set_transfer_data_size (&c, DMA_SIZE_8);
    channel_config_set_enable (&c, true);
    channel_config_set_read_increment (&c, true);
    channel_config_set_write_increment (&c, false);
    channel_config_set_sniff_enable (&c, true);
    dma_sniffer_enable (dma, DMA_SNIFF_CTRL_CALC_VALUE_CRC16, true);
    dma_hw->sniff_data = 0;
    dma_channel_configure (dma, &c, &out, buff, 512, true);
    dma_channel_wait_for_finish_blocking (dma);
    dma_channel_unclaim (dma);
    uint16_t crc = dma_hw->sniff_data;
    printf ("CRC = 0x%04X\n", crc);
    return crc;
    }

bool sd_spi_read (uint lba, uint8_t *buff)
    {
    uint8_t chk[2];
    sd_spi_set_lba (lba, cmd17);
    printf ("Read command 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
        cmd17[0], cmd17[1], cmd17[2], cmd17[3], cmd17[4], cmd17[5]);
    uint8_t resp = sd_spi_cmd (cmd17);
    printf ("   Resp 0x%02X", resp);
    if ( resp != SD_R1_OK )
        {
        printf ("\nFailed\n");
        return false;
        }
    while (true)
        {
        sd_spi_get (&resp, 1);
        printf (" 0x%02X", resp);
        if ( resp == SDBT_START ) break;
        if ( resp < SDBT_ECLIP )
            {
            printf ("\nError\n");
            return false;
            }
        }
    printf ("\n");
    sd_spi_get (buff, 512);
    sd_spi_get (chk, 2);
    printf ("Check bytes 0x%02X 0x%02X\n", chk[0], chk[1]);
    uint16_t crc = sd_spi_crc16 (buff);
    if (( chk[0] != ( crc >> 8 )) || (chk[1] != ( crc & 0xFF )))
        {
        printf ("CRC mismatch\n");
        return false;
        }
    return true;
    }

bool sd_spi_write (uint lba, const uint8_t *buff)
    {
    uint8_t chk[2];
    printf ("Write block\n");
    uint16_t crc = sd_spi_crc16 (buff);
    chk[0] = crc >> 8;
    chk[1] = crc & 0xFF;
    sd_spi_set_lba (lba, cmd24);
    printf ("Write command 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
        cmd24[0], cmd24[1], cmd24[2], cmd24[3], cmd24[4], cmd24[5]);
    uint8_t resp = sd_spi_cmd (cmd24);
    printf ("   Resp 0x%02X", resp);
    if ( resp != SD_R1_OK )
        {
        printf ("\nFailed\n");
        return false;
        }
    printf ("Write data\n");
    resp = SDBT_START;
    sd_spi_put (&resp, 1);
    sd_spi_put (buff, 512);
    sd_spi_put (chk, 2);
    printf ("   Resp");
    while (true)
        {
        sd_spi_get (&resp, 1);
        printf (" 0x%02X", resp);
        if ( resp != 0xFF ) break;
        }
    printf ("\n");
    return true;
    }
    
