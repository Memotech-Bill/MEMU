/*  tape.c  -   Cassette tape hardware emulation */

#include "common.h"
#include "tape.h"
#include "ctc.h"
#include "diag.h"
#include "memu.h"
#include "mem.h"
#include <stdio.h>
#include <string.h>
#ifdef WIN32
#define strcasecmp _stricmp
#endif

#define MTX_TAPE_ZERO   0x348           /* Duration of a zero bit half cycle (in Z80 clocks) */
#define MTX_TAPE_ONE    0x690           /* Duration of a one bit half cycle (in Z80 clocks) */
#define MTX_TAPE_HALF   ( ( MTX_TAPE_ZERO + MTX_TAPE_ONE ) / 2 )

typedef enum { fmtUnk, fmtMTX, fmtCAS, fmtWAV, fmtW08, fmtW16, fmtW32 } TapeFmt;
typedef enum { stgPre, stgWait, stgData } MTXStages;

static BOOLEAN bTapeRun = FALSE;        /* Tape running */

static const char *psTapeIn = NULL;     /* Input file name */
static TapeFmt fmtIn = fmtUnk;          /* Input file format */
static FILE *pfTapeIn = NULL;           /* Input file stream */
static long long cTapeNext = 0;             /* Z80 clocks until next interrupt */
static MTXStages stgIn = stgPre;        /* Stages of loading an MTX tape file */
static int nCycleIn;                    /* Number of half cycles in tape preamble */
static BOOLEAN bMTXFirst;               /* First half cycle of an MTX tape bit */
static int nMTXBitsIn;                  /* Number of bits remaining in MTX tape byte */
static int nMTXBytesIn;                 /* Number of bytes loaded in a stage */
static byte byMTXIn;                    /* MTX tape byte */
static unsigned long long cRes = 0;         /* Z80 clocks remaining from last tape advance */
static word nChan;                      /* Number of channels in WAV file */
static unsigned int nRateIn;            /* WAV file sample rate */
static float fTapeLast;                 /* Last value read from tape */
static int nPulse;                      /* Number of CTC trigger pulses since tape last started */

static const char *psTapeOut = NULL;    /* Output filename */
static TapeFmt fmtOut = fmtUnk;         /* Output file format */
static FILE *pfTapeOut = NULL;          /* Output file stream */
static byte byTapeOut = 0;              /* Last value output to tape */
static unsigned long long cTapeLast = 0;        /* Z80 clock at last tape output */
static MTXStages stgOut = stgPre;       /* Stages of saving an MTX tape file */
static int nMTXBitsOut;                 /* Number of bits in MTX tape byte */
static int nMTXBytesOut;                /* Number of bytes saved to an MTX tape file */
static int nMTXBlock;                   /* Number of blocks saved to an MTX tape file */
static byte byMTXOut;                   /* MTX tape output byte */
static byte nCycleOut;                  /* Number of preamble cycles output */
static int nRateOut = 4800;             /* WAV output file sample rate */

extern CFG cfg;

static TapeFmt tape_identify (const char *psFile)
    {
    const char *psExt;
    int nCh = (int) strlen (psFile);
    if ( nCh < 3 ) return fmtUnk;
    psExt = &psFile[nCh-3];
    if ( ! strcasecmp (psExt, "mtx") ) return fmtMTX;
    if ( ! strcasecmp (psExt, "cas") ) return fmtCAS;
    if ( ! strcasecmp (psExt, "wav") ) return fmtWAV;
    return fmtUnk;
    }

void tape_set_input (const char *psFile)
    {
    if ( pfTapeIn != NULL )
        {
        fclose (pfTapeIn);
        pfTapeIn = NULL;
        }
    if ( psTapeIn != NULL )
        {
        free ((void *) psTapeIn);
        psTapeIn = NULL;
        }
    if ( psFile != NULL )
        {
        diag_message (DIAG_TAPE, "Set cassette input file %s", psFile);
        psTapeIn = estrdup (psFile);
        fmtIn = tape_identify (psTapeIn);
        if ( fmtIn == fmtUnk ) fatal ("Unknown tape in format");
        }
    }

const char *tape_get_input (void)
    {
    return psTapeIn;
    }

static float tape_read_wav (void)
    {
    if ( pfTapeIn == NULL ) return 0.0;
    float fVal = 0.0;
    int iChan;
    int nRead;
    byte by;
    short wd;
    float f;
    for ( iChan = 0; iChan < nChan; ++iChan)
        {
        switch (fmtIn)
            {
            case fmtW08:
                nRead = (int) fread (&by, sizeof (by), 1, pfTapeIn);
                // diag_message (DIAG_TAPE, "Channel %d byte = %d", iChan, by);
                fVal += (float) (by / 128.0 - 1.0);
                break;
            case fmtW16:
                nRead = (int) fread (&wd, sizeof (wd), 1, pfTapeIn);
                // diag_message (DIAG_TAPE, "Channel %d word = %d", iChan, wd);
                fVal += (float) (wd / 32768.0);
                break;
            case fmtW32:
                nRead = (int) fread (&f, sizeof (f), 1, pfTapeIn);
                fVal += f;
                // diag_message (DIAG_TAPE, "Channel %d float = %f", iChan, by);
                break;
            default:
                nRead = 0;  // Never reached but keep compiler happy
                // diag_message (DIAG_TAPE, "Invalid format read");
                break;
            }
        if ( nRead == 0 )
            {
            fclose (pfTapeIn);
            pfTapeIn = NULL;
            diag_message (DIAG_TAPE, "End of tape: CTC Pulses = %d.", nPulse);
            return 0.0;
            }
        }
    return fVal / nChan;
    }

static void tape_open_cas (const char *psFile)
    {
    pfTapeIn = fopen (psFile, "rb");
    cTapeNext = MTX_TAPE_ONE;
    diag_message (DIAG_TAPE, "Open CAS format tape input file %s", psFile);
    }

static void tape_open_mtx (const char *psFile)
    {
    pfTapeIn = fopen (psFile, "rb");
    cTapeNext = MTX_TAPE_ONE;
    stgIn = stgPre;
    diag_message (DIAG_TAPE, "Open MTX format tape input file %s", psFile);
    }

static void tape_open_wav (const char *psFile)
    {
    unsigned int nLen;
    char sID[5];
    pfTapeIn = fopen (psFile, "rb");
    if ( pfTapeIn == NULL ) return;
    diag_message (DIAG_TAPE, "Open WAV format tape input file %s", psFile);
    sID[4] = '\0';
    fread (sID, sizeof (char), 4, pfTapeIn);
    if ( strcmp (sID, "RIFF") ) fatal ("Tape WAV file does not have a RIFF header");
    fread (&nLen, sizeof (nLen), 1, pfTapeIn);
    fread (sID, sizeof (char), 4, pfTapeIn);
    if ( strcmp (sID, "WAVE") ) fatal ("Tape WAV file is not WAVE format");
    while ( fread (sID, sizeof (char), 4, pfTapeIn) == 4 )
        {
        fread (&nLen, sizeof (nLen), 1, pfTapeIn);
        if ( ! strcmp (sID, "fmt ") )
            {
            word wFmt;
            unsigned int nBPS;
            word wAlign;
            unsigned int nBits;
            word wBits;
            fread (&wFmt, sizeof (wFmt), 1, pfTapeIn);
            if ( wFmt != 1 ) fatal ("Tape WAV file is compressed");
            fread (&nChan, sizeof (nChan), 1, pfTapeIn);
            fread (&nRateIn, sizeof (nRateIn), 1, pfTapeIn);
            fread (&nBPS, sizeof (nBPS), 1, pfTapeIn);
            fread (&wAlign, sizeof (wAlign), 1, pfTapeIn);
            if ( nLen == 16 )
                {
                fread (&wBits, sizeof (wBits), 1, pfTapeIn);
                nBits = wBits;
                }
            else if ( nLen == 18 )
                {
                fread (&nBits, sizeof (nBits), 1, pfTapeIn);
                }
            else
                {
                fatal ("Tape WAV file has unexpected fmt chunk length");
                }
            if ( nBits == 8 )       fmtIn = fmtW08;
            else if ( nBits == 16 ) fmtIn = fmtW16;
            else if ( nBits == 32 ) fmtIn = fmtW32;
            else fatal ("Tape WAV file has unsupported sample length");
            }
        else if ( ! strcmp (sID, "data") )
            {
            if ( fmtIn == fmtWAV ) fatal ("No fmt chunk in WAV file before data");
            fTapeLast = tape_read_wav ();
            return;
            }
        else
            {
            fseek (pfTapeIn, nLen, SEEK_CUR);
            }
        }
    fatal ("No data chunk found in Tape WAV file");
    }

void tape_play (void)
    {
    if ( ( pfTapeIn == NULL ) && ( psTapeIn != NULL ) )
        {
        diag_message (DIAG_TAPE, "tape_play: psTapeIn = \"%s\".", psTapeIn);
        const char *psTapePath = make_path (cfg.tape_name_prefix, psTapeIn);
        switch (fmtIn)
            {
            case fmtMTX:
                tape_open_mtx (psTapePath);
                break;
            case fmtCAS:
                tape_open_cas (psTapePath);
                break;
            default:
                tape_open_wav (psTapePath);
                break;
            }
        if ( pfTapeIn == NULL ) fatal ("Failed to open input tape file");
        free ((void *) psTapePath);
        }
    }

void tape_advance_cas (int clks)
    {
    // diag_message (DIAG_TAPE, "Tape Advance: clks = %d, cTapeNext = %d", clks, cTapeNext);
    cTapeNext -= clks;
    if ( cTapeNext <= 0 )
        {
        word cNext;
        // diag_message (DIAG_TAPE, "Tape interrupt.");
        ++nPulse;
        ctc_trigger (3);
        if ( fread (&cNext, sizeof (cNext), 1, pfTapeIn) == 1 )
            {
            cTapeNext += cNext;
            }
        else
            {
            fclose (pfTapeIn);
            pfTapeIn = NULL;
            diag_message (DIAG_TAPE, "End of tape.");
            }
        }
    }

void tape_advance_mtx (int clks)
    {
    // diag_message (DIAG_TAPE, "Tape Advance: clks = %d, cTapeNext = %d", clks, cTapeNext);
    cTapeNext -= clks;
    if ( cTapeNext <= 0 )
        {
        // diag_message (DIAG_TAPE, "Tape interrupt.");
        ++nPulse;
        ctc_trigger (3);
        switch (stgIn)
            {
            case stgPre:
                nCycleIn = 1500;
                ++stgIn;
                diag_message (DIAG_TAPE, "Starting preamble.");
                /* Fall through to next case */
            case stgWait:
                cTapeNext = MTX_TAPE_ZERO;
                --nCycleIn;
                if ( nCycleIn == 0 )
                    {
                    cTapeNext = MTX_TAPE_ONE;
                    diag_message (DIAG_TAPE, "Starting data.");
                    ++stgIn;
                    nMTXBytesIn = 0;
                    nMTXBitsIn = 0;
                    bMTXFirst = TRUE;
                    }
                break;
            case stgData:
                if ( bMTXFirst )
                    {
                    bMTXFirst = FALSE;
                    }
                else
                    {
                    bMTXFirst = TRUE;
                    byMTXIn >>= 1;
                    --nMTXBitsIn;
                    if ( nMTXBitsIn <= 0 )
                        {
                        if ( fread (&byMTXIn, sizeof (byMTXIn), 1, pfTapeIn) != 1 )
                            {
                            fclose (pfTapeIn);
                            pfTapeIn = NULL;
                            diag_message (DIAG_TAPE, "End of tape.");
                            return;
                            }
                        // diag_message (DIAG_TAPE, "Tape byte = 0x%02x", byMTXIn);
                        ++nMTXBytesIn;
                        nMTXBitsIn = 8;
                        }
                    }
                if ( byMTXIn & 0x01 )   cTapeNext = MTX_TAPE_ONE;
                else                    cTapeNext = MTX_TAPE_ZERO;
                // diag_message (DIAG_TAPE, "Set cTapeNext = %d", cTapeNext);
            }
        }
    }

void tape_advance_wav (int clks)
    {
    BOOLEAN bInt = FALSE;
    cRes += clks;
    int nSamp = (int) (nRateIn * cRes / 4000000);
    int iSamp;
    cRes -= 4000000 * nSamp / nRateIn;
    // diag_message (DIAG_TAPE, "Tape Advance: clks = %d, nSamp = %d, cRes = %llu", clks, nSamp, cRes);
    for ( iSamp = 0; iSamp < nSamp; ++iSamp )
        {
        float fTapeIn = tape_read_wav ();
        if ( ( ( fTapeIn > 0.0 ) && ( fTapeLast <= 0.0 ) )
            || ( ( fTapeIn < 0.0 ) && ( fTapeLast >= 0.0 ) ) ) bInt = TRUE;
        fTapeLast = fTapeIn;
        }
    if ( bInt )
        {
        diag_message (DIAG_TAPE, "Tape interrupt.");
        ++nPulse;
        ctc_trigger (3);
        }
    }

void tape_advance (int clks)
    {
    if ( ( bTapeRun ) && ( pfTapeIn != NULL ) )
        {
        switch (fmtIn)
            {
            case fmtMTX:
                tape_advance_mtx (clks);
                break;
            case fmtCAS:
                tape_advance_cas (clks);
                break;
            default:
                tape_advance_wav (clks);
                break;
            }
        }
    }

void tape_out1F (byte value)
    {
    static int  isave;
    Z80 *r = get_Z80_regs ();
    switch ( value )
        {
        case 0xaa:
            diag_message (DIAG_TAPE, "Tape start: HL = 0x%04X, DE = 0x%04X", r->HL.W, r->DE.W);
            nPulse = 0;
            tape_play ();
            bTapeRun = TRUE;
            isave = cfg.iperiod;
            if ( fmtIn >= fmtWAV )  cfg.iperiod = 105;
            else                    cfg.iperiod = 840;
            if ( fmtOut == fmtMTX ) stgOut = stgPre;
            if ( fmtIn == fmtMTX )
                {
                stgIn = stgPre;
                cTapeNext = MTX_TAPE_ZERO;
                }
            break;
        case 0x55:
            diag_message (DIAG_TAPE, "Tape stop: HL = 0x%04X, CTC Pulses = %d.", r->HL.W, nPulse);
            bTapeRun = FALSE;
            cfg.iperiod = isave;
            if ( fmtIn == fmtMTX )
                {
                diag_message (DIAG_TAPE, "%d Bytes loaded, %d Bits remaining.", nMTXBytesIn, nMTXBitsIn);
                }
            if ( fmtOut == fmtMTX )
                {
                ++nMTXBlock;
                if (( nMTXBlock == 1 ) && ( nMTXBytesOut == 20 ))
                    {
                    // Discard last two bytes of name block.
                    diag_message (DIAG_TAPE, "Discard last two bytes of name block.");
                    nMTXBytesOut -= 2;
                    fseek (pfTapeOut, -2, SEEK_CUR);
                    }
                diag_message (DIAG_TAPE, "MTX block %d: %d data bytes saved.", nMTXBlock, nMTXBytesOut);
                }
            if ( pfTapeOut != NULL ) fflush (pfTapeOut);
            break;
        }
    }

void tape_out_close (void)
    {
    if ( fmtOut == fmtWAV )
        {
        /* Output final bit */
        int nSamp = ( MTX_TAPE_ONE * nRateOut + 2000000 ) / 4000000;
        int iSamp;
        byte byOut = byTapeOut ? 255 : 0;
        for ( iSamp = 0; iSamp < nSamp; ++iSamp ) fwrite (&byOut, sizeof (byOut), 1, pfTapeOut);
        /* Size of data chunk */
        fseek (pfTapeOut, 42, SEEK_SET);
        fwrite (&nMTXBytesOut, sizeof (nMTXBytesOut), 1, pfTapeOut);
        /* Size of WAVE file */
        nMTXBytesOut += 34;
        fseek (pfTapeOut, 4, SEEK_SET);
        fwrite (&nMTXBytesOut, sizeof (nMTXBytesOut), 1, pfTapeOut);
        fseek (pfTapeOut, 0, SEEK_END);
        }
    fclose (pfTapeOut);
    pfTapeOut = NULL;
    diag_message (DIAG_TAPE, "Closed tape output file");
    }

void tape_set_output (const char *psFile)
    {
    if ( pfTapeOut != NULL )
        {
        tape_out_close ();
        pfTapeOut = NULL;
        }
    if ( psTapeOut != NULL )
        {
        free ((void *) psTapeOut);
        psTapeOut = NULL;
        }
    if ( psFile != NULL )
        {
        diag_message (DIAG_TAPE, "Set cassette output file %s", psFile);
        psTapeOut = estrdup (psFile);
        fmtOut = tape_identify (psTapeOut);
        if ( fmtOut == fmtUnk ) fatal ("Unknown tape out format");
        }
    }

const char *tape_get_output (void)
    {
    return psTapeOut;
    }

void tape_save_cas (byte value)
    {
    unsigned long long  cNow = get_Z80_clocks ();
    word cTime;
    if ( pfTapeOut == NULL )
        {
        const char *psTapePath;
        if ( psTapeOut == NULL ) OutZ80_bad("cassette tape", 0x03, value, FALSE);
        diag_message (DIAG_TAPE, "Open CAS format tape output file %s", psTapeOut);
        psTapePath = make_path (cfg.tape_name_prefix, psTapeOut);
        pfTapeOut = fopen (psTapePath, "wb");
        free ((void *) psTapePath);
        }
    cTime = (word) (cNow - cTapeLast);
    fwrite (&cTime, sizeof (cTime), 1, pfTapeOut);
    cTapeLast = cNow;
    }

void tape_save_mtx (byte value)
    {
    unsigned long long  cNow = get_Z80_clocks ();
    word cTime = (word) (cNow - cTapeLast);
    cTapeLast = cNow;
    switch (stgOut)
        {
        case stgPre:
            nCycleOut = 0;
            ++stgOut;
            break;
        case stgWait:
            ++nCycleOut;
            if ( ( value ) && ( cTime > MTX_TAPE_HALF ) )
                {
                if ( pfTapeOut == NULL )
                    {
                    const char *psTapePath;
                    if ( psTapeOut == NULL ) OutZ80_bad("cassette tape", 0x03, value, FALSE);
                    diag_message (DIAG_TAPE, "Open MTX format tape output file %s", psTapeOut);
                    psTapePath = make_path (cfg.tape_name_prefix, psTapeOut);
                    pfTapeOut = fopen (psTapePath, "wb");
                    free ((void *) psTapePath);
                    nMTXBlock = 0;
                    }
                diag_message (DIAG_TAPE, "%d Preamble cycles", nCycleOut);
                nMTXBytesOut = 0;
                nMTXBitsOut = 0;
                byMTXOut = 0x00;
                bMTXFirst = TRUE;
                ++stgOut;
                }
            break;
        case stgData:
            if ( value )
                {
                byMTXOut >>= 1;
                if ( cTime > MTX_TAPE_HALF ) byMTXOut |= 0x80;
                if ( ++ nMTXBitsOut >= 8 )
                    {
                    Z80 *r = get_Z80_regs ();
                    diag_message (DIAG_TAPE, "Byte %d = 0x%02X, PC = 0x%04X, HL = 0x%04X, (HL) = 0x%02X",
                        nMTXBytesOut, byMTXOut, r->PC.W, r->HL.W, mem_read_byte (r->HL.W));
                    fwrite (&byMTXOut, sizeof (byMTXOut), 1, pfTapeOut);
                    ++nMTXBytesOut;
                    nMTXBitsOut = 0;
                    byMTXOut = 0x00;
                    }
                }
        }
    cTapeLast = cNow;
    }

void tape_save_wav (byte value)
    {
    unsigned long long  cNow = get_Z80_clocks ();
    word cTime = (word) (cNow - cTapeLast);
    cTapeLast = cNow;
    if ( cTime > 2 * MTX_TAPE_ONE ) cTime = 2 * MTX_TAPE_ONE;
    if ( pfTapeOut == NULL )
        {
        int iVal;
        word wVal;
        const char *psTapePath;
        if ( psTapeOut == NULL ) OutZ80_bad("cassette tape", 0x03, value, FALSE);
        diag_message (DIAG_TAPE, "Open WAV format tape output file %s", psTapeOut);
        psTapePath = make_path (cfg.tape_name_prefix, psTapeOut);
        pfTapeOut = fopen (psTapePath, "wb");
        free ((void *) psTapePath);
        fwrite ("RIFF", sizeof (char), 4, pfTapeOut);
        nMTXBytesOut = 0;
        fwrite (&nMTXBytesOut, sizeof (nMTXBytesOut), 1, pfTapeOut);
        fwrite ("WAVE", sizeof (char), 4, pfTapeOut);
        fwrite ("fmt ", sizeof (char), 4, pfTapeOut);
        iVal = 18;  /* Chunk size */
        fwrite (&iVal, sizeof (iVal), 1, pfTapeOut);
        wVal = 1;   /* Format */
        fwrite (&wVal, sizeof (wVal), 1, pfTapeOut);
        wVal = 1;   /* Number of channels */
        fwrite (&wVal, sizeof (wVal), 1, pfTapeOut);
        fwrite (&nRateOut, sizeof (nRateOut), 1, pfTapeOut);
        fwrite (&nRateOut, sizeof (nRateOut), 1, pfTapeOut);
        wVal = 1;   /* Block align */
        fwrite (&wVal, sizeof (wVal), 1, pfTapeOut);
        iVal = 8;   /* Bits per sample */
        fwrite (&iVal, sizeof (iVal), 1, pfTapeOut);
        fwrite ("data", sizeof (char), 4, pfTapeOut);
        fwrite (&nMTXBytesOut, sizeof (nMTXBytesOut), 1, pfTapeOut);
        }
    else
        {
        int nSamp = ( cTime * nRateOut + 2000000 ) / 4000000;
        int iSamp;
        byte byOut = byTapeOut ? 255 : 0;
        for ( iSamp = 0; iSamp < nSamp; ++iSamp ) fwrite (&byOut, sizeof (byOut), 1, pfTapeOut);
        nMTXBytesOut += nSamp;
        }
    }

void tape_out3 (byte value)
    {
    value &= 0x01;
    //  diag_message (DIAG_TAPE, "Cassette output 0x%02x", value);
    if ( ( bTapeRun ) && ( value != byTapeOut ) )
        {
        // diag_message (DIAG_TAPE, "Tape output: %d", value);
        switch (fmtOut)
            {
            case fmtMTX:
                tape_save_mtx (value);
                break;
            case fmtCAS:
                tape_save_cas (value);
                break;
            default:
                tape_save_wav (value);
                break;
            }
        }
    byTapeOut = value;
    }

void tape_term (void)
    {
    if ( pfTapeOut != NULL ) tape_out_close ();
    if ( pfTapeIn != NULL )
        {
        fclose (pfTapeIn);
        pfTapeIn = NULL;
        }
    }
