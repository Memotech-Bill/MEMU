/*  circleio.c  -   Use FatFS library to emulate stdio.h routines */

#include <circle/alloc.h>
#include <circle/string.h>
#include <circle/util.h>
#include <fatfs/ff.h>
#include <stdio.h>
#include "console.h"
#include "kfuncs.h"
// #include "diag.h"

static CString sText;
static const char * FormatString (const char *fmt, va_list va)
    {
    sText.FormatV (fmt, va);
    return (const char *) sText;
    }

int vsprintf (char *ps, const char *fmt, va_list va)
    {
    const char *sOut = FormatString (fmt, va);
    strcpy (ps, sOut);
    return strlen (sOut);
    }

#if 0
int sprintf (char *ps, const char *fmt, ...)
    {
    int nByte;
    va_list va;
    va_start (va, fmt);
    nByte = vsprintf (ps, fmt, va);
    va_end (va);
    return nByte;
    }

int vsnprintf (char *ps, int n, const char *fmt, va_list va)
    {
    const char *sOut = FormatString (fmt, va);
    strncpy (ps, sOut, n);
    return strlen (sOut);
    }
#endif

int sscanf (const char *psStr, const char *psFmt, ...)
    {
    /* A very incomplete implementation */
    // diag_message (DIAG_INIT, "sscanf (\"%s\", \"%s\",...)", psStr, psFmt);
    int nArg = 0;
    va_list va;
    va_start (va, *psFmt);
    while ( *psFmt && *psStr )
        {
        if ( *psFmt == '%' )
            {
			int nLong = 0;
            // diag_message (DIAG_INIT, "Start of format specifier");
            ++psFmt;
			if ( *psFmt == '%' )
				{
				if ( *psStr != '%' ) break;
				++psStr;
				++psFmt;
				continue;
				}
			while ( *psFmt == 'l' )
				{
				++nLong;
				++psFmt;
				}
            if ( ( *psFmt == 'd' ) || ( *psFmt == 'i' ) || ( *psFmt == 'u' ) )
                {
                // diag_message (DIAG_INIT, "Integer format");
                unsigned long long uVal = 0;
				int base = 10;
                int bNeg = 0;
				while ( ( *psStr ) && ( *psStr <= ' ' ) ) ++psStr;
                // diag_message (DIAG_INIT, "Start of number");
				if ( *psFmt != 'u' )
					{
					if ( *psStr == '-' )
						{
						bNeg = 1;
						++psStr;
						}
					else if ( *psStr == '+' )
						{
						++psStr;
						}
					}
				if ( *psFmt == 'i' )
					{
					if ( *psStr == '0' )
						{
						base = 8;
						++psStr;
						if ( ( *psStr == 'x' ) || ( *psStr == 'X' ) )
							{
							base = 16;
							++psStr;
							}
						}
                    }
                // diag_message (DIAG_INIT, "Start of digits");
                while ( *psStr )
                    {
                    if ( ( *psStr >= '0' ) && ( *psStr < '0' + base ) )
                        {
                        uVal = base * uVal + ( *psStr - '0' );
                        ++psStr;
                        }
                    else if ( ( *psStr >= 'A' ) && ( *psStr < 'A' + base - 10 ) )
                        {
                        uVal = base * uVal + ( *psStr - 'A' + 10 );
                        ++psStr;
                        }
                    else if ( ( *psStr >= 'a' ) && ( *psStr < 'a' + base - 10 ) )
                        {
                        uVal = base * uVal + ( *psStr - 'a' + 10 );
                        ++psStr;
                        }
                    else
                        {
                        break;
                        }
                    }
				if ( *psFmt =='u' )
					{
					if ( nLong == 0 )
						{
						unsigned int *pu = va_arg (va, unsigned int *);
						*pu = (unsigned int) uVal;
						}
					else if ( nLong == 1 )
						{
						unsigned long *plu = va_arg (va, unsigned long *);
						*plu = (unsigned long) uVal;
						}
					else
						{
						unsigned long long *pllu = va_arg (va, unsigned long long *);
						*pllu = uVal;
						}
					}
				else
					{
					long long iVal = (long long) uVal;
					if ( bNeg ) iVal = - iVal;
					if ( nLong == 0 )
						{
						int *pi = va_arg (va, int *);
						*pi = (int) iVal;
						}
					else if ( nLong == 1 )
						{
						long *pli = va_arg (va, long *);
						*pli = (long) iVal;
						}
					else
						{
						long long *plli = va_arg (va, long long *);
						*plli = iVal;
						}
					}
                ++nArg;
                ++psFmt;
                }
            else if ( *psFmt == 'f' )
                {
                // diag_message (DIAG_INIT, "Integer format");
				double dVal = 0.0;
                int bNeg = 0;
				while ( ( *psStr ) && ( *psStr <= ' ' ) ) ++psStr;
                // diag_message (DIAG_INIT, "Start of number");
				if ( *psStr == '-' )
					{
					bNeg = 1;
					++psStr;
					}
				else if ( *psStr == '+' )
					{
					++psStr;
					}
                // diag_message (DIAG_INIT, "Start of digits");
                while ( *psStr )
                    {
                    if ( ( *psStr >= '0' ) && ( *psStr <= '9' ) )
                        {
                        dVal = 10.0 * dVal + ( *psStr - '0' );
                        ++psStr;
                        }
                    else
                        {
                        break;
                        }
                    }
				if ( *psStr == '.' )
					{
					double dFrac = 0.0;
					double dDen = 1.0;
					while ( *psStr )
						{
						if ( ( *psStr >= '0' ) && ( *psStr <= '9' ) )
							{
							dFrac = 10.0 * dFrac + ( *psStr - '0' );
							dDen *= 10.0;
							++psStr;
							}
						else
							{
							break;
							}
						}
					dVal += dFrac / dDen;
					}
				if ( bNeg ) dVal = - dVal;
				if ( nLong == 0 )
					{
					float *pf = va_arg (va, float *);
					*pf = (float) dVal;
					}
				else
					{
					double *pd = va_arg (va, double *);
					*pd = dVal;
					}
                ++nArg;
                ++psFmt;
                }
			else if ( *psFmt == '[' )
				{
				int bMatch = 1;
				char *psOut = va_arg (va, char *);
				++psFmt;
				if ( *psFmt == '^' )
					{
					bMatch = 0;
					++psFmt;
					}
				while ( *psStr )
					{
					const char *psTst = psFmt;
					int bFind = 0;
					while ( *psTst )
						{
						if ( ( *psTst == ']' ) && ( psTst != psFmt ) )
							{
							break;
							}
						else if ( *psTst == '-' )
							{
							if ( ( psTst == psFmt ) || ( *(psTst+1) == ']' ) )
								{
								if ( *psStr == '-' ) bFind = 1;
								break;
								}
							else if ( ( *psStr >= *(psTst-1) ) && ( *psStr <= *(psTst+1) ) )
								{
								bFind = 1;
								break;
								}
							}
						else if ( *psStr == *psTst )
							{
							bFind = 1;
							break;
							}
						++psTst;
						}
					if ( bFind == bMatch )
						{
						*psOut = *psStr;
						++psOut;
						++psStr;
						}
					else
						{
						*psOut = '\0';
						break;
						}
					}
                ++nArg;
				if ( *psFmt == ']' ) ++psFmt;
				while ( ( *psFmt ) && ( *psFmt != ']' ) ) ++psFmt;
				if ( *psFmt == ']' ) ++psFmt;
				}
            else
                {
                // diag_message (DIAG_INIT, "Invalid format");
                break;
                }
            }
		else if ( *psFmt <= ' ' )
			{
			while ( ( *psStr ) && ( *psStr <= ' ' ) ) ++psStr;
			++psFmt;
			}
        else
            {
			if ( *psStr != *psFmt ) break;
			++psStr;
            ++psFmt;
            }
        }
    va_end (va);
    // diag_message (DIAG_INIT, "%d arguments parsed.", nArg);
    return nArg;
    }

double atof (const char *ps)
    {
    int dVal = 0.0;
    sscanf (ps, "%lf", &dVal);
    return dVal;
    }

#if 0
int atoi (const char *ps)
    {
    int iVal = 0;
    sscanf (ps, "%d", &iVal);
    return iVal;
    }

static int iUngetCh = -1;

int getc (FILE *pf)
    {
    int iCh = iUngetCh;
    if ( iCh >= 0 )
        {
        iUngetCh = -1;
        }
    else
        {
        size_t in;
        iCh = 0;
        f_read ((FIL *) pf, &iCh, 1, &in);
        if ( in != 1 ) iCh = EOF;
        }
    return iCh;
    }

int ungetc (int iCh, FILE *pf)
    {
    int iRes = iCh;
    if ( iUngetCh != -1 ) iRes = EOF;
    iUngetCh = iCh;
    return iRes;
    }
#endif
