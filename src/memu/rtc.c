/*  rtc.c - Emulates the DS3231 Real Time Clock for MFX.

    Initial value is set according to operating system.
    Subsequent setting of the clock is emulated without changing operating system clock
*/

#include "rtc.h"
#include "types.h"
#include <time.h>

#define N_RTC_REG   19
#define N_TIM_REG   7
#define NSPD        (24 * 60 * 60)

#ifdef WIN32
#define timegm _mkgmtime
#endif

static time_t   tOSSet;
static time_t   tRTCSet;

static byte rtc_regs[N_RTC_REG];
static byte rtc_addr = 0;
static byte rtc_data = 0;
static byte dweek = 0;

static void rtc_set_time (void)
    {
    struct tm tm;
    tm.tm_sec = 10 * (rtc_regs[0] >> 4) + (rtc_regs[0] & 0x0F);
    tm.tm_min = 10 * (rtc_regs[1] >> 4) + (rtc_regs[1] & 0x0F);
    tm.tm_hour = 10 * ((rtc_regs[2] & 0x30) >> 4) + (rtc_regs[2] & 0x0F);
    if ((rtc_regs[2] & 0x60) == 0x60) tm.tm_hour -= 8;
    tm.tm_mday = 10 * ((rtc_regs[4] & 0x30) >> 4) + (rtc_regs[4] & 0x0F);
    tm.tm_mon = 10 * ((rtc_regs[5] & 0x10) >> 4) + (rtc_regs[5] & 0x0F) - 1;
    tm.tm_year = 10 * (rtc_regs[6] >> 4) + (rtc_regs[6] & 0x0F) + 100;
    tm.tm_isdst = 0;
    dweek = rtc_regs[3] - 1;
    tOSSet = time (NULL);
    tRTCSet = timegm (&tm);
    }

static void rtc_get_time (void)
    {
    time_t tNow = time (NULL);
    time_t tRTC = tRTCSet + (tNow - tOSSet);
    struct tm *ptm = gmtime (&tRTC);
    rtc_regs[0] = ((ptm->tm_sec / 10) << 4) + (ptm->tm_sec % 10);
    rtc_regs[1] = ((ptm->tm_min / 10) << 4) + (ptm->tm_min % 10);
    if (rtc_regs[2] & 0x40)
        {
        byte bpm = (ptm->tm_hour >= 12) ? 0x20 : 0x00;
        if (bpm) ptm->tm_hour -= 12;
        if (ptm->tm_hour == 0) ptm->tm_hour = 12;
        rtc_regs[2] = 0x40 + bpm + ((ptm->tm_hour / 10) << 4) + (ptm->tm_hour % 10);
        }
    else
        {
        rtc_regs[2] = ((ptm->tm_hour / 10) << 4) + (ptm->tm_hour % 10);
        }
    rtc_regs[3] = (dweek + ((tNow / NSPD) - (tOSSet / NSPD))) % 7 + 1;
    rtc_regs[4] = ((ptm->tm_mday / 10) << 4) + (ptm->tm_mday % 10);
    byte bCent = rtc_regs[5] & 0x80;
    ptm->tm_year -= 100;
    if (ptm->tm_year >= 100) bCent ^= 0x80;
    ++ptm->tm_mon;
    rtc_regs[5] = bCent + ((ptm->tm_mon / 10) << 4) + (ptm->tm_mon % 10);
    ptm->tm_year %= 100;
    rtc_regs[6] =  ((ptm->tm_year / 10) << 4) + (ptm->tm_year % 10);
    }

void rtc_out70 (byte value)
    {
    rtc_addr = value;
    }

void rtc_out71 (byte value)
    {
    rtc_data = value;
    }

void rtc_out72 (byte value)
    {
    switch (value)
        {
        case 1:
            if (rtc_addr == 0) rtc_get_time ();
            rtc_data = rtc_regs[rtc_addr];
            break;
        case 2:
            if (rtc_addr < N_TIM_REG)
                {
                rtc_get_time ();
                rtc_regs[rtc_addr] = rtc_data;
                rtc_set_time ();
                }
            else
                {
                rtc_regs[rtc_addr] = rtc_data;
                }
            break;
        default:
            break;
        }
    ++rtc_addr;
    if (rtc_addr == N_RTC_REG) rtc_addr = 0;
    }

byte rtc_in71 (void)
    {
    return rtc_data;
    }

byte rtc_in72 (void)
    {
    return 0;
    }
