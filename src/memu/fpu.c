/*  fpu.c - Emulation of the Arithmetic accelerator in v4 of the MFX FPGA */

#include "fpu.h"

#define N_STACK 10

typedef struct
    {
    unsigned int    m;
    unsigned char   e;
    } Float5;

static Float5 fconst[] = {
    {0x00000000, 0x81},    // 0x40 - 1.0
    {0x00000000, 0x82},    // 0x41 - 2.0
    {0x20000000, 0x84},    // 0x42 - 10.0
    {0x490FDAA2, 0x82},    // 0x43 - pi
    {0x490FDAA2, 0x81},    // 0x44 - pi/2
    {0x490FDAA2, 0x83},    // 0x45 - 2pi
    {0x00000000, 0x80},    // 0x46 - 1/2
    {0x2AAAAAAA, 0x7F},    // 0x47 - 1/3 
    {0x4CCCCCCD, 0x7E},    // 0x48 - 1/5 
    {0x12492492, 0x7E},    // 0x49 - 1/7 
    {0x638E38E4, 0x7D},    // 0x4A - 1/9 
    {0x3A2E8BA3, 0x7D},    // 0x4B - 1/11
    {0x1D89D89E, 0x7D},    // 0x4C - 1/13
    {0x08888889, 0x7D},    // 0x4D - 1/15
    {0x70F0F0F1, 0x7D},    // 0x4E - 1/17
    {0x2AAAAAAB, 0x7E},    // 0x4F - 1/3!
    {0x2AAAAAAB, 0x7C},    // 0x50 - 1/4!
    {0x08888889, 0x7A},    // 0x51 - 1/5!
    {0x360B60B6, 0x77},    // 0x52 - 1/6!
    {0x300D00D0, 0x74},    // 0x53 - 1/7!
    {0x500D00D0, 0x71},    // 0x54 - 1/8!
    {0x38EF1D2B, 0x6E},    // 0x55 - 1/9!
    {0x37322B40, 0x67},    // 0x56 - 1/11!
    {0x3092309D, 0x60},    // 0x57 - 1/13!
    {0x38AA3B29, 0x81},    // 0x58 - log2(e)
    {0x317217F8, 0x80}     // 0x59 - loge(2)
    };

static Float5 stack[N_STACK];
static Float5 *tos = &stack[0];
static Float5 *nos = &stack[N_STACK - 1];
static unsigned long long mulres = 0;
static byte iResult = 0;

static inline Float5* fpu_stack (int iPos)
    {
    Float5 *pf = tos - iPos;
    if (pf < stack) pf += N_STACK;
    return pf;
    }

static void fpu_copy (Float5 *pdst, const Float5 *psrc)
    {
    pdst->m = psrc->m;
    pdst->e = psrc->e;
    }

static void fpu_push (void)
    {
    nos = tos;
    ++tos;
    if (tos == stack + N_STACK) tos = stack;
    }

static void fpu_pop (void)
    {
    tos = nos;
    nos = fpu_stack (1);
    }

static void fpu_fadd (void)
    {
    BOOLEAN tneg = tos->m >= 0x80000000;
    BOOLEAN nneg = nos->m >= 0x80000000;
    long long tosm = (tos->m | 0x8000000) << 1;
    long long nosm = (nos->m | 0x8000000) << 1;
    if (tos->e >= nos->e)
        {
        nosm >>= tos->e - nos->e;
        nos->e = tos->e;
        }
    else
        {
        tosm >>= nos->e - tos->e;
        }
    if (tneg == nneg)
        {
        nosm += tosm;
        if (nosm & 1) ++nosm;
        if (nosm & 0x200000000)
            {
            if (nos->e == 0xFF) iResult = R_OVER;
            ++nos->e;
            nosm >>= 1;
            }
        nos->m = (unsigned int)(nosm >> 1);
        }
    else if (nosm == tosm)
        {
        nos->m = 0;
        nos->e = 0;
        nneg = FALSE;
        }
    else
        {
        nosm -= tosm;
        if (nosm < 0)
            {
            nosm = -nosm;
            nneg = ! nneg;
            }
        if (nosm & 1) ++nosm;
        nos->m = (unsigned int)(nosm >> 1);
        while (!(nos->m & 0x80000000))
            {
            if (nos->e == 0)
                {
                iResult = R_UNDR;
                break;
                }
            --nos->e;
            nos->m <<= 1;
            }
        }
    if (!nneg) nos->m &= 0x7FFFFFF;
    }

static void fpu_fmul (void)
    {
    if ((tos->e == 0) || (nos->e == 0))
        {
        nos->m = 0;
        nos->e = 0;
        }
    else
        {
        BOOLEAN tneg = tos->m >= 0x80000000;
        BOOLEAN nneg = nos->m >= 0x80000000;
        unsigned long long tosm = tos->m | 0x8000000;
        unsigned long long nosm = nos->m | 0x8000000;
        mulres = nosm * tosm;
        int nexp = (int) nos->e + (int) tos->e - 0x81;
        if (mulres & 0x8000000000000000)
            {
            ++nexp;
            mulres >>= 1;
            }
        if (mulres & 0x40000000)
            {
            mulres += 0x4000000;
            if (mulres & 0x8000000000000000)
                {
                ++nexp;
                mulres >>= 1;
                }
            }
        if (nexp > 0xFF) iResult = R_OVER;
        else if (nexp <= 0) iResult = R_UNDR;
        nos->e = nexp & 0xFF;
        nos->m = (unsigned int)(mulres >> 31);
        if (tneg == nneg) nos->m &= 0x7FFFFFFF;
        }
    }

static void fpu_fdiv (void)
    {
    if (tos->e == 0)
        {
        iResult = R_DIV0;
        }
    else if (nos->e != 0)
        {
        BOOLEAN tneg = tos->m >= 0x80000000;
        BOOLEAN nneg = nos->m >= 0x80000000;
        unsigned long long tosm = tos->m | 0x8000000;
        unsigned long long nosm = nos->m | 0x8000000;
        nosm <<= 32;
        unsigned long long divres = nosm / tosm;
        int nexp = (int) nos->e - (int) tos->e + 0x81;
        if (divres & 0x8000000000000000)
            {
            divres >>= 1;
            }
        else
            {
            --nexp;
            }
        if (divres & 0x40000000)
            {
            divres += 0x4000000;
            if (divres & 0x8000000000000000)
                {
                ++nexp;
                divres >>= 1;
                }
            }
        if (nexp > 0xFF) iResult = R_OVER;
        else if (nexp <= 0) iResult = R_UNDR;
        nos->e = nexp & 0xFF;
        nos->m = (unsigned int)(divres >> 31);
        if (tneg == nneg) nos->m &= 0x7FFFFFFF;
        }
    }

static void fpu_cmd (byte cmd)
    {
    Float5 fTmp;
    switch (cmd)
        {
        case 0x00:    // C_INIT
            tos = stack;
            nos = fpu_stack (1);
            iResult = R_OK;
            break;
        case 0x01:    // C_LIT
            fpu_push ();
            tos->m = 0;
            tos->e = 0;
            break;
        case 0x02:    // C_DUP
            fpu_push ();
            fpu_copy (tos, nos);
            break;
        case 0x03:    // C_DROP
            fpu_pop ();
            break;
        case 0x04:    // C_SWAP
            fpu_copy (&fTmp, nos);
            fpu_copy (nos, tos);
            fpu_copy (tos, &fTmp);
            break;
        case 0x05:    // C_OVER
            fpu_push ();
            fpu_copy (tos, fpu_stack (2));
            break;
        case 0x06:    // C_OK
            iResult = R_OK;
            break;
        case 0x10:    // C_1
            tos->m = 1;
            tos->e = 0;
            break;
        case 0x20:    // C_INEG
            tos->m = (unsigned int)(- (int)tos->m);
            break;
        case 0x21:    // C_INOT
            tos->m = ~ tos->m;
            break;
        case 0x22:    // C_ILSL
            tos->m <<= 1;
            break;
        case 0x23:    // C_ILSR
            tos->m >>= 1;
            break;
        case 0x24:    // C_IASR
            tos->m = (tos->m & 0x80000000) | (tos->m >> 1);
            break;
        case 0x25:    // C_IABS
            if ((int) tos->m < 0) tos->m = (unsigned int)(-((int) tos->m));
            break;
        case 0x26:    // C_ISGN
            if ((int) tos->m < 0) tos->m = (unsigned int) -1;
            else if (tos->m > 0) tos->m = 1;
            break;
        case 0x30:    // C_IADD
            nos->m = nos->m + tos->m;
            fpu_pop ();
            break;
        case 0x31:    // C_ISUB
            nos->m = nos->m - tos->m;
            fpu_pop ();
            break;
        case 0x32:    // C_UMUL
            mulres = ((unsigned long long) nos->m) * ((unsigned long long) tos->m);
            nos->m = ((unsigned int *) &mulres)[0];
            fpu_pop ();
            break;
        case 0x33:    // C_SMUL
            mulres = (unsigned long long)((long long) nos->m) * ((long long) tos->m);
            nos->m = ((unsigned int *) &mulres)[0];
            fpu_pop ();
            break;
        case 0x34:    // C_UDIV
            if (tos->m == 0) iResult = R_DIV0;
            else nos->m = nos->m / tos->m;
            fpu_pop ();
            break;
        case 0x35:    // C_SDIV
            if (tos->m == 0) iResult = R_DIV0;
            else nos->m = (unsigned int)((int) nos->m) / ((int) tos->m);
            fpu_pop ();
            break;
        case 0x36:    // C_UMOD
            if (tos->m == 0) iResult = R_DIV0;
            else nos->m = nos->m % tos->m;
            fpu_pop ();
            break;
        case 0x37:    // C_SMOD
            if (tos->m == 0) iResult = R_DIV0;
            else nos->m = (unsigned int)((int) nos->m) % ((int) tos->m);
            fpu_pop ();
            break;
        case 0x38:    // C_HMUL
            fpu_push ();
            tos->m = ((unsigned int *) &mulres)[1];
            break;
        case 0x60:    // C_FNEG
            if (tos->e != 0) tos->m ^= 0x80000000;
            break;
        case 0x61:    // C_FABS
            tos->m &= 0x7FFFFFFF;
            break;
        case 0x62:    // C_FSGN
            if (tos->e != 0)
                {
                tos->e = 0x81;
                tos->m &= 0x80000000;
                }
            break;
        case 0x63:    // C_FINT
            if (tos->e < 0x81)
                {
                tos->m = 0;
                tos->e = 0;
                }
            else if (tos->e < 0xA0)
                {
                unsigned int mask[] = {
                    0x80000000, 0xC0000000, 0xE0000000, 0xF0000000,
                    0xF8000000, 0xFC000000, 0xFE000000, 0xFF000000,
                    0xFF800000, 0xFFC00000, 0xFFE00000, 0xFFF00000,
                    0xFFF80000, 0xFFFC0000, 0xFFFE0000, 0xFFFF0000,
                    0xFFFF8000, 0xFFFFC000, 0xFFFFE000, 0xFFFFF000,
                    0xFFFFF800, 0xFFFFFC00, 0xFFFFFE00, 0xFFFFFFF0,
                    0xFFFFFF80, 0xFFFFFFC0, 0xFFFFFFE0, 0xFFFFFFF0,
                    0xFFFFFFF8, 0xFFFFFFFC, 0xFFFFFFFE, 0xFFFFFFFF};
                tos->m &= mask[tos->e - 0x81];
                }
        case 0x71:    // C_FSUB
            tos->m ^= 0x80000000;
        case 0x70:    // C_FADD
            fpu_fadd ();
            fpu_pop ();
            break;
        case 0x72:    // C_FMUL
            fpu_fmul ();
            fpu_pop ();
            break;
        case 0x73:    // C_FDIV
            fpu_fdiv ();
            fpu_pop ();
            break;
        case 0x80:    // C_UTOF
            if (tos->m == 0)
                {
                tos->e = 0;
                }
            else
                {
                tos->e = 31;
                while (!(tos->m & 0x80000000))
                    {
                    tos->m <<= 1;
                    --tos->e;
                    }
                tos->m &= 0x7FFFFFFF;
                }
            break;
        case 0x81:    // C_FTOU
            if (tos->e < 0x81)
                {
                tos->m = 0;
                }
            else if (tos->e > 0xA0)
                {
                iResult = R_OVER;
                }
            else if (tos->m & 0x80000000)
                {
                iResult = R_UNDR;
                }
            else
                {
                tos->m |= 0x80000000;
                tos->m >>= 0xA0 - tos->e;
                }
            break;
        default:
            if ((cmd >= 0x40) && (cmd <= 0x59))
                {
                fpu_copy (tos, &fconst[cmd - 0x40]);
                }
            break;
        }
    }

void fpu_out (word port, byte value)
    {
    switch (port & 0xFF)
        {
        case 0xA0:
            tos->m = (tos->m & 0xFFFFFF00) | value;
            break;
        case 0xA1:
            tos->m = (tos->m & 0xFFFF00FF) | (((unsigned int) value) << 8);
            break;
        case 0xA2:
            tos->m = (tos->m & 0xFF00FFFF) | (((unsigned int) value) << 16);
            break;
        case 0xA3:
            tos->m = (tos->m & 0x00FFFFFF) | (((unsigned int) value) << 14);
            break;
        case 0xA4:
            tos->e = value;
            break;
        case 0xA5:
            fpu_cmd (value);
            break;
        }
    }

byte fpu_in (word port)
    {
    byte value;
    switch (port & 0xFF)
        {
        case 0xA0:
            value = tos->m & 0xFF;
            break;
        case 0xA1:
            value = (tos->m >> 8) & 0xFF;
            break;
        case 0xA2:
            value = (tos->m >> 16) & 0xFF;
            break;
        case 0xA3:
            value = (tos->m >> 24) & 0xFF;
            break;
        case 0xA4:
            value = tos->e;
            break;
        case 0xA5:
            value = iResult;
            break;
        }
    return value;
    }
