/*  rtc.h - Emulates the DS3231 Real Time Clock for MFX. */

#ifndef RTC_H
#define RTC_H

#include "types.h"

extern void rtc_out70 (byte value);
extern void rtc_out71 (byte value);
extern void rtc_out72 (byte value);
extern byte rtc_in71 (void);
extern byte rtc_in72 (void);

#endif
