/*  fpu.c - Emulation of the Arithmetic accelerator in v4 of the MFX FPGA */

#include "fpu.h"
#include "diag.h"
#include <string.h>
#include <math.h>

#define N_STACK 8

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
static Float5 tos;
static Float5 nos;
static int iStk = N_STACK - 1;
static int nDepth = 0;
static unsigned long long mulres = 0;
static byte iResult = 0;
static byte enable = 0;
static const char *psStatus[] = {"Busy", "OK", "Zero Divide", "Overflow", "Underflow"};
static const struct
    {
    byte        cmd;
    const char *psDesc;
    } cmd_desc[] =
    {
    {0x00, "Initialise"},
    {0x01, "Push zero onto top of stack"},
    {0x02, "Push a second copy of top of stack"},
    {0x03, "Pop value from top of stack"},
    {0x04, "Exchange top of stack and next of stack"},
    {0x05, "Push a copy of next of stack"},
    {0x06, "Set OK status"},
    {0x10, "Load integer 1 on top of stack (replaces previous value)"},
    {0x20, "32 bit integer negation"},
    {0x21, "32 bit integer bitwise not"},
    {0x22, "32 bit integer logical shift left"},
    {0x23, "32 bit integer logical shift right"},
    {0x24, "32 bit integer arithmetic shift right"},
    {0x25, "32 bit integer absolute value"},
    {0x26, "32 bit sign (result is -1, 0 or 1)"},
    {0x30, "32 bit integer addition (40 bit result)"},
    {0x31, "32 bit integer subtraction (40 bit result)"},
    {0x32, "32 bit integer unsigned multiplication"},
    {0x33, "32 bit integer signed multiplication"},
    {0x34, "32 bit integer unsigned division"},
    {0x35, "32 bit integer signed division"},
    {0x36, "32 bit integer unsigned modulus"},
    {0x37, "32 bit integer signed modulus"},
    {0x38, "Push bits 32 to 63 of multiply result to top of stack"},
    {0x40, "Load 1.0 on top of stack (replaces previous value)"},
    {0x41, "Load 2.0 on top of stack"},
    {0x42, "Load 10.0 on top of stack"},
    {0x43, "Load pi on top of stack"},
    {0x44, "Load pi/2 on top of stack"},
    {0x45, "Load 2pi on top of stack"},
    {0x46, "Load 1/2 on top of stack"},
    {0x47, "Load 1/3 on top of stack"},
    {0x48, "Load 1/5 on top of stack"},
    {0x49, "Load 1/7 on top of stack"},
    {0x4A, "Load 1/9 on top of stack"},
    {0x4B, "Load 1/11 on top of stack"},
    {0x4C, "Load 1/13 on top of stack"},
    {0x4D, "Load 1/15 on top of stack"},
    {0x4E, "Load 1/17 on top of stack"},
    {0x4F, "Load 1/3! on top of stack"},
    {0x50, "Load 1/4! on top of stack"},
    {0x51, "Load 1/5! on top of stack"},
    {0x52, "Load 1/6! on top of stack"},
    {0x53, "Load 1/7! on top of stack"},
    {0x54, "Load 1/8! on top of stack"},
    {0x55, "Load 1/9! on top of stack"},
    {0x56, "Load 1/11! on top of stack"},
    {0x57, "Load 1/13! on top of stack"},
    {0x58, "Load log_2(e) on top of stack"},
    {0x59, "Load log_e(2) on top of stack"},
    {0x60, "Floating point negation"},
    {0x61, "Floating point absolute value"},
    {0x62, "Floating point sign"},
    {0x63, "Round to integer value (towards zero)"},
    {0x70, "Floating point addition"},
    {0x71, "Floating point subtraction"},
    {0x72, "Floating point multiplication"},
    {0x73, "Floating point division"},
    {0x80, "32 bit unsigned integer to floating point"},
    {0x81, "Floating point to 32-bit unsigned integer"}
    };

static double f5val (const Float5 *pf5)
    {
    if (pf5->e == 0) return 0.0;
    BOOLEAN bNeg = FALSE;
    unsigned int m = pf5->m;
    if (m & 0x80000000) bNeg = TRUE;
    m |= 0x80000000;
    double v = m;
    v *= pow(2.0, pf5->e - 129 - 31);
    if (bNeg) v = -v;
    return v;
    }

static const char *show_cmd (byte cmd)
    {
    static const char *psUnkn = "Unknown";
    for (int i = 0; i < sizeof (cmd_desc) / sizeof (cmd_desc[0]); ++i)
        {
        if (cmd == cmd_desc[i].cmd) return cmd_desc[i].psDesc;
        }
    return psUnkn;
    }

static void show_stack (byte cmd)
    {
    int nShow = 0;
    switch (cmd)
        {
        case 0x03:    // C_DROP
        case 0x04:    // C_SWAP
        case 0x05:    // C_OVER
        case 0x30:    // C_IADD
        case 0x31:    // C_ISUB
        case 0x32:    // C_UMUL
        case 0x33:    // C_SMUL
        case 0x34:    // C_UDIV
        case 0x35:    // C_SDIV
        case 0x36:    // C_UMOD
        case 0x37:    // C_SMOD
        case 0x70:    // C_FADD
        case 0x71:    // C_FSUB
        case 0x72:    // C_FMUL
        case 0x73:    // C_FDIV
            nShow = 1;
            break;
        }
    if (nDepth < nShow)
        {
        diag_message (DIAG_FPU_STACK, "Stack depth = %d, Insufficient, %d required", nDepth + 1, nShow + 1);
        }
    else
        {
        diag_message (DIAG_FPU_STACK, "Stack depth = %d", nDepth + 1);
        nShow = nDepth;
        }
    diag_message (DIAG_FPU_STACK, "tos: %02X%08X = %16.10E", tos.e, tos.m, f5val (&tos));
    if (nShow > 0) diag_message (DIAG_FPU_STACK, "nos: %02X%08X = %16.10E", nos.e, nos.m, f5val (&nos));
    for (int i = 2; i <= nShow; ++i)
        {
        Float5 *pf5 = &stack[(iStk - i + 2) & (N_STACK - 1)];
        diag_message (DIAG_FPU_STACK, "%3d: %02X%08X = %16.10E", i, pf5->e, pf5->m, f5val (pf5));
        }
    }

static void fpu_copy (Float5 *pdst, const Float5 *psrc)
    {
    pdst->m = psrc->m;
    pdst->e = psrc->e;
    }

static void fpu_push (void)
    {
    if (++iStk >= N_STACK) iStk = 0;
    fpu_copy (&stack[iStk], &nos);
    fpu_copy (&nos, &tos);
    if (nDepth < N_STACK + 1) ++nDepth;
    else diag_message (DIAG_FPU_STACK, "Overflowed FPU stack");
    }

static void fpu_pop (void)
    {
    fpu_copy (&tos, &nos);
    fpu_copy (&nos, &stack[iStk]);
    if (--iStk < 0) iStk = N_STACK - 1;
    if (nDepth > 0) --nDepth;
    else diag_message (DIAG_FPU_STACK, "Pop from empty FPU stack");
    }

static void fpu_fadd (void)
    {
    BOOLEAN tneg = tos.m >= 0x80000000;
    BOOLEAN nneg = nos.m >= 0x80000000;
    long long tosm = ((long long)(tos.m | 0x80000000)) << 1;
    long long nosm = ((long long)(nos.m | 0x80000000)) << 1;
    diag_message (DIAG_FPU_CALC, "Original: tosm = %c%09llX, nosm = %c%09llX",
        tneg ? '-' : '+', tosm, nneg ? '-' : '+', nosm);
    if (tos.e >= nos.e)
        {
        nosm >>= tos.e - nos.e;
        nos.e = tos.e;
        }
    else
        {
        tosm >>= nos.e - tos.e;
        }
    diag_message (DIAG_FPU_CALC, "Shifted: tosm = %09llX, nosm = %09llX", tosm, nosm);
    if (tneg == nneg)
        {
        nosm += tosm;
        if (nosm & 1) ++nosm;
        diag_message (DIAG_FPU_CALC, "Added: nosm = %09llX, nos.e = %02X", nosm, nos.e);
        if (nosm & 0x200000000ULL)
            {
            if (nos.e == 0xFF) iResult = R_OVER;
            ++nos.e;
            nosm >>= 1;
            }
        nos.m = (unsigned int)(nosm >> 1);
        }
    else if (nosm == tosm)
        {
        nos.m = 0;
        nos.e = 0;
        nneg = FALSE;
        diag_message (DIAG_FPU_CALC, "Subtract equal values");
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
        nos.m = (unsigned int)(nosm >> 1);
        diag_message (DIAG_FPU_CALC, "Subtracted: nosm = %09llX, nos.e = %d", nosm, nos.e);
        while (!(nos.m & 0x80000000))
            {
            if (nos.e == 0)
                {
                iResult = R_UNDR;
                break;
                }
            --nos.e;
            nos.m <<= 1;
            }
        }
    if (!nneg) nos.m &= 0x7FFFFFFF;
    }

static void fpu_fmul (void)
    {
    if ((tos.e == 0) || (nos.e == 0))
        {
        nos.m = 0;
        nos.e = 0;
        }
    else
        {
        BOOLEAN tneg = tos.m >= 0x80000000;
        BOOLEAN nneg = nos.m >= 0x80000000;
        unsigned long long tosm = tos.m | 0x80000000;
        unsigned long long nosm = nos.m | 0x80000000;
        mulres = nosm * tosm;
        int nexp = (int) nos.e + (int) tos.e - 0x81;
        diag_message (DIAG_FPU_CALC, "tosm = %c%08llX, nosm = %c%08llX, mulres = %016llX, nexp = %d",
            tneg ? '-' : '+', tosm, nneg ? '-' : '+', nosm, mulres, nexp);
        if (mulres & 0x8000000000000000ULL)
            {
            ++nexp;
            mulres >>= 1;
            }
        if (mulres & 0x40000000)
            {
            mulres += 0x4000000;
            if (mulres & 0x8000000000000000ULL)
                {
                ++nexp;
                mulres >>= 1;
                }
            }
        if (nexp > 0xFF) iResult = R_OVER;
        else if (nexp <= 0) iResult = R_UNDR;
        nos.e = nexp & 0xFF;
        nos.m = (unsigned int)(mulres >> 31);
        if (tneg == nneg) nos.m &= 0x7FFFFFFF;
        }
    }

static void fpu_fdiv (void)
    {
    if (tos.e == 0)
        {
        diag_message (DIAG_FPU_CALC, "Floating divide by zero");
        iResult = R_DIV0;
        }
    else if (nos.e != 0)
        {
        BOOLEAN tneg = tos.m >= 0x80000000;
        BOOLEAN nneg = nos.m >= 0x80000000;
        unsigned long long tosm = tos.m | 0x80000000;
        unsigned long long nosm = nos.m | 0x80000000;
        diag_message (DIAG_FPU_CALC, "tosm = %c%08llX, nosm = %c%08llX",
            tneg ? '-' : '+', tosm, nneg ? '-' : '+', nosm);
        nosm <<= 32;
        nosm = (nosm + tosm / 2) / tosm;
        int nexp = (int) nos.e - (int) tos.e + 0x81;
        diag_message (DIAG_FPU_CALC, "nosm = %016llX, nexp = %02X", nosm, nexp);
        if (nosm & 0x100000000ULL)
            {
            nosm >>= 1;
            }
        else
            {
            --nexp;
            }
        if (nexp > 0xFF) iResult = R_OVER;
        else if (nexp <= 0) iResult = R_UNDR;
        nos.e = nexp & 0xFF;
        nos.m = nosm;
        if (tneg == nneg) nos.m &= 0x7FFFFFFF;
        }
    }

static void fpu_cmd (byte cmd)
    {
    Float5 fTmp;
    long long itmp;
    if (diag_flags[DIAG_FPU_STACK])
        {
        const char *psCmd = show_cmd (cmd);
        diag_message (DIAG_FPU_STACK, "-----\nBefore FPU command 0x%02X: %s", cmd, psCmd);
        show_stack (cmd);
        diag_message (DIAG_FPU_STACK, "After FPU command 0x%02X: %s", cmd, psCmd);
        }
    switch (cmd)
        {
        case 0x00:    // C_INIT
            iStk = 0;
            nDepth = 0;
            iResult = R_OK;
            break;
        case 0x01:    // C_LIT
            fpu_push ();
            tos.m = 0;
            tos.e = 0;
            break;
        case 0x02:    // C_DUP
            fpu_push ();
            break;
        case 0x03:    // C_DROP
            fpu_pop ();
            break;
        case 0x04:    // C_SWAP
            fpu_copy (&fTmp, &nos);
            fpu_copy (&nos, &tos);
            fpu_copy (&tos, &fTmp);
            break;
        case 0x05:    // C_OVER
            fpu_push ();
            fpu_copy (&tos, &stack[iStk]);
            break;
        case 0x06:    // C_OK
            iResult = R_OK;
            break;
        case 0x10:    // C_1
            tos.m = 1;
            tos.e = 0;
            break;
        case 0x20:    // C_INEG
            tos.m = (unsigned int)(- (int)tos.m);
            break;
        case 0x21:    // C_INOT
            tos.m = ~ tos.m;
            break;
        case 0x22:    // C_ILSL
            tos.m <<= 1;
            break;
        case 0x23:    // C_ILSR
            tos.m >>= 1;
            break;
        case 0x24:    // C_IASR
            tos.m = (tos.m & 0x80000000) | (tos.m >> 1);
            break;
        case 0x25:    // C_IABS
            if ((int) tos.m < 0) tos.m = (unsigned int)(-((int) tos.m));
            break;
        case 0x26:    // C_ISGN
            if ((int) tos.m < 0) tos.m = (unsigned int) -1;
            else if (tos.m > 0) tos.m = 1;
            break;
        case 0x30:    // C_IADD
            itmp = (long long) nos.m + tos.m;
            memcpy (&nos, &itmp, 5);
            fpu_pop ();
            break;
        case 0x31:    // C_ISUB
            itmp = (long long) nos.m - tos.m;
            memcpy (&nos, &itmp, 5);
            fpu_pop ();
            break;
        case 0x32:    // C_UMUL
            mulres = ((unsigned long long) nos.m) * ((unsigned long long) tos.m);
            nos.m = ((unsigned int *) &mulres)[0];
            nos.e = 0;
            fpu_pop ();
            if (diag_flags[DIAG_FPU_STACK]) diag_message (DIAG_FPU_CALC, "mulres = %016llX", mulres);
            break;
        case 0x33:    // C_SMUL
            mulres = (unsigned long long)((long long)(int) nos.m) * ((long long)(int) tos.m);
            nos.m = ((unsigned int *) &mulres)[0];
            nos.e = 0;
            fpu_pop ();
            if (diag_flags[DIAG_FPU_STACK]) diag_message (DIAG_FPU_CALC, "mulres = %016llX", mulres);
            break;
        case 0x34:    // C_UDIV
            if (tos.m == 0)
                {
                iResult = R_DIV0;
                nos.m = 0;
                }
            else
                {
                nos.m = nos.m / tos.m;
                }
            nos.e = 0;
            fpu_pop ();
            break;
        case 0x35:    // C_SDIV
            if (tos.m == 0)
                {
                iResult = R_DIV0;
                nos.m = 0;
                }
            else
                {
                nos.m = (unsigned int)(((int) nos.m) / ((int) tos.m));
                }
            nos.e = 0;
            fpu_pop ();
            break;
        case 0x36:    // C_UMOD
            if (tos.m == 0)
                {
                iResult = R_DIV0;
                nos.m = 0;
                }
            else
                {
                nos.m = nos.m % tos.m;
                }
            nos.e = 0;
            fpu_pop ();
            break;
        case 0x37:    // C_SMOD
            if (tos.m == 0)
                {
                iResult = R_DIV0;
                nos.m = 0;
                }
            else
                {
                nos.m = (unsigned int)(((int) nos.m) % ((int) tos.m));
                }
            nos.e = 0;
            fpu_pop ();
            break;
        case 0x38:    // C_HMUL
            fpu_push ();
            tos.m = ((unsigned int *) &mulres)[1];
            break;
        case 0x60:    // C_FNEG
            if (tos.e != 0) tos.m ^= 0x80000000;
            break;
        case 0x61:    // C_FABS
            tos.m &= 0x7FFFFFFF;
            break;
        case 0x62:    // C_FSGN
            if (tos.e != 0)
                {
                tos.e = 0x81;
                tos.m &= 0x80000000;
                }
            break;
        case 0x63:    // C_FINT
            if (tos.e < 0x81)
                {
                tos.m = 0;
                tos.e = 0;
                }
            else if (tos.e < 0xA0)
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
                tos.m &= mask[tos.e - 0x81];
                }
            break;
        case 0x71:    // C_FSUB
            tos.m ^= 0x80000000;
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
            if (tos.m == 0)
                {
                tos.e = 0;
                }
            else
                {
                tos.e = 0xA0;
                while (!(tos.m & 0x80000000))
                    {
                    tos.m <<= 1;
                    --tos.e;
                    }
                tos.m &= 0x7FFFFFFF;
                }
            break;
        case 0x81:    // C_FTOU
            if (tos.e < 0x81)
                {
                tos.m = 0;
                }
            else if (tos.e > 0xA0)
                {
                iResult = R_OVER;
                }
            else if (tos.m & 0x80000000)
                {
                iResult = R_UNDR;
                }
            else
                {
                tos.m |= 0x80000000;
                tos.m >>= 0xA0 - tos.e;
                }
            break;
        default:
            if ((cmd >= 0x40) && (cmd <= 0x59))
                {
                fpu_copy (&tos, &fconst[cmd - 0x40]);
                }
            else diag_message (DIAG_FPU_CALC, "Invalid FPU operation 0x%02X", cmd);
            break;
        }
    if (diag_flags[DIAG_FPU_STACK])
        {
        diag_message (DIAG_FPU_STACK, "Status = 0x%02X: %s", iResult, psStatus[iResult]);
        show_stack (0);
        }
    }

void fpu_out (word port, byte value)
    {
    port &= 0xFF;
    if (port == 0xDA)
        {
        enable = value & 0x40;
        diag_message (DIAG_FPU_PORT, "FPU port 0x%02X write 0x%02X", port, value);
        }
    else if (enable)
        {
        diag_message (DIAG_FPU_PORT, "FPU port 0x%02X write 0x%02X", port, value);
        switch (port)
            {
            case 0xA0:
                tos.m = (tos.m & 0xFFFFFF00) | value;
                break;
            case 0xA1:
                tos.m = (tos.m & 0xFFFF00FF) | (((unsigned int) value) << 8);
                break;
            case 0xA2:
                tos.m = (tos.m & 0xFF00FFFF) | (((unsigned int) value) << 16);
                break;
            case 0xA3:
                tos.m = (tos.m & 0x00FFFFFF) | (((unsigned int) value) << 24);
                break;
            case 0xA4:
                tos.e = value;
                break;
            case 0xA5:
                fpu_cmd (value);
                break;
            }
        }
    else
        {
        diag_message (DIAG_FPU_PORT, "Disabled FPU port 0x%02X write 0x%02X", port, value);
        }
    }

byte fpu_in (word port)
    {
    byte value = 0x78;
    port &= 0xFF;
    if (port == 0xDA)
        {
        value = enable;
        diag_message (DIAG_FPU_PORT, "FPU port 0x%02X read 0x%02X", port, value);
        }
    else if (enable)
        {
        switch (port)
            {
            case 0xA0:
                value = tos.m & 0xFF;
                break;
            case 0xA1:
                value = (tos.m >> 8) & 0xFF;
                break;
            case 0xA2:
                value = (tos.m >> 16) & 0xFF;
                break;
            case 0xA3:
                value = (tos.m >> 24) & 0xFF;
                break;
            case 0xA4:
                value = tos.e;
                break;
            case 0xA5:
                value = iResult;
                break;
            }
        diag_message (DIAG_FPU_PORT, "FPU port 0x%02X read 0x%02X", port, value);
        }
    else
        {
        diag_message (DIAG_FPU_PORT, "Disabled FPU port 0x%02X read 0x%02X", port, value);
        }
    return value;
    }
