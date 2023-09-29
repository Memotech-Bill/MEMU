/* An on-screen display of log messages compatible with MEMU/circle window switching */

#include <circle/device.h>
#include <circle/util.h>
#include "console.h"
#include "monprom.h"
#include "win.h"
#include "kbd.h"
#include "common.h"
#include "kfuncs.h"

#define WINCON_WTH			80
#define WINCON_HGT			24

#define CLR_BACKGROUND		0
#define CLR_NORMAL			1
#define CLR_HIGHLIGHT		2
#define CLR_COUNT			3

#define STY_NORMAL			( 16 * CLR_NORMAL    + CLR_BACKGROUND )
#define STY_HIGHLIGHT		( 16 * CLR_HIGHLIGHT + CLR_BACKGROUND )

static WIN *con_win  =  NULL;
static COL con_clr[CLR_COUNT] = {
	{   0,   0,   0 },   /* Backgound */
	{   0, 255, 255 },   /* Normal text */
	{ 255,   0,   0 },   /* Highlighted text */
	};
static int	nRow	=	0;
static int	nCol	=	0;
static int	iRow	=	0;
static int	iCol	=	0;
static int	iSty	=	STY_NORMAL;
static int  con_wk  =  	-1;

#ifdef BOOT_DUMP
#include <circle/alloc.h>
#include <circle/util.h>
struct LogMsg
    {
    LogMsg *    pPrev;
    LogMsg *    pNext;
    char        sMsg[1];
    };

static LogMsg * pFirst;
static LogMsg * pLast;
static bool     bFile = false;
#endif

CConsole::CConsole (void)
   {
   }

CConsole::~CConsole (void)
   {
   con_term ();
   }

boolean CConsole::Initialize (void)
    {
    con_init ();
    return TRUE;
    }

int CConsole::Write (const void *pBuffer, size_t nCount)
   {
   con_print ((const char *) pBuffer, nCount);
   return nCount;
   }

void CConsole::Show (void)
    {
    con_show ();
    }

#ifdef BOOT_DUMP
void CConsole::Dump (void)
    {
    FILE *pfil = fopen (BOOT_DUMP, "w");
    if ( pfil != NULL )
        {
        char sMsg[256];
        int nLine = 0;
        LogMsg *plm = pFirst;
        while ( plm )
            {
            ++nLine;
            fputs (plm->sMsg, pfil);
            pFirst = plm->pNext;
            free (plm);
            plm = pFirst;
            }
        pLast = NULL;
        bFile = true;
        fclose (pfil);
        sprintf (sMsg, "%d boot messages saved to %s\n", nLine, BOOT_DUMP);
        con_print (sMsg, 0);
        }
    }
#endif

//	Key press event handler - record key pressed.
static void con_keypress (WIN *win, int wk)
	{
	con_wk =  wk;
	}

//	Key release event handler - does nothing.
static void con_keyrelease (WIN *win, int wk)
	{
    con_wk = -1;
	}

void con_init (void)
	{
	struct FBInfo fbi;
	GetFBInfo (&fbi);
	nCol = fbi.xres / GLYPH_WIDTH;
	nRow = fbi.yres / GLYPH_HEIGHT;
	if ( nRow >= 50 )
		{
		nRow /= 2;
		if ( nCol >= 160 ) nCol /= 2;
		}
	con_win = win_create (GLYPH_WIDTH * nCol, GLYPH_HEIGHT * nRow,
		1, 1,
		"MEMU Console",
		NULL, NULL,
		con_clr, CLR_COUNT,
		con_keypress, con_keyrelease);
	}

//	Exit console screen.
void con_term (void)
	{
	if ( con_win != NULL )
		{
		win_delete (con_win);
		con_win  =  NULL;
		}
	}

void con_show (void)
    {
    win_show (con_win);
    }

static int nAlert = 0;
static WIN *win_alert = NULL;

void con_alert_on (void)
    {
    if ( nAlert == 0 ) win_alert = win_current ();
    ++nAlert;
    }

void con_alert_off (void)
    {
    if ( nAlert > 0 ) --nAlert;
    if ( ( nAlert == 0 ) && ( win_alert != NULL ) )
        {
        win_show (win_alert);
        win_alert = NULL;
        }
    }

static void con_scroll (void)
	{
    int one_line = GLYPH_HEIGHT * GLYPH_WIDTH * nCol;
	memcpy (con_win->data, con_win->data + one_line, one_line * (nRow - 1));
    memset (con_win->data + one_line * (nRow - 1), 0, one_line);
	if ( iRow > 0 ) --iRow;
	}

static void con_putch (int iSty, char ch)
	{
	int  iFG =  iSty >> 4;
	int  iBG =  iSty & 0x0f;
	byte *pby   =  con_win->data + GLYPH_HEIGHT * con_win->width * iRow + GLYPH_WIDTH * iCol;
	byte by;
	int  iScan, iPix;
	if ( iCol >= nCol )
		{
		++iRow;
		iCol = 0;
		}
	if ( iRow >= nRow )
		{
		con_scroll ();
		iRow = nRow - 1;
		}
	for ( iScan = 0; iScan < GLYPH_HEIGHT; ++iScan )
		{
		by =  mon_alpha_prom[(int) ch][iScan];
		for ( iPix = 0; iPix < GLYPH_WIDTH; ++iPix )
			{
			*pby  =  ( by & 0x80 ) ? iFG : iBG;
			++pby;
			by = by << 1;
			}
		pby   += con_win->width - GLYPH_WIDTH;
		}
    ++iCol;
	}

void con_print (const char *ps, int nCh)
	{
	if ( con_win == NULL ) return;
	if ( nCh <= 0 ) nCh = strlen (ps);
#ifdef BOOT_DUMP
   if ( bFile )
       {
       FILE *pfil = fopen (BOOT_DUMP, "a");
       if ( pfil != NULL )
           {
           fwrite (ps, 1, nCh, pfil);
           fclose (pfil);
           }
       }
   else
       {
       LogMsg *plm = (LogMsg *) malloc (sizeof (LogMsg) + nCh);
       if ( plm != NULL )
           {
           plm->pPrev = pLast;
           plm->pNext = NULL;
           strncpy (plm->sMsg, ps, nCh);
           plm->sMsg[nCh] = '\0';
           if ( pFirst == NULL ) pFirst = plm;
           else pLast->pNext = plm;
           pLast = plm;
           }
       }
#endif
	while ( nCh > 0 )
		{
		char ch = *ps;
		if ( ch == '\0' )
			{
			break;
			}
		else if ( ( ch >= ' ' ) && ( ch <= '~' ) )
			{
			con_putch (iSty, ch);
			}
		else if ( ch == '\n' )
			{
			iCol = 0;
			++iRow;
			if ( iRow >= nRow ) con_scroll ();
			}
		else if ( ch == 0x1B )
			{
			if ( ( nCh > 3 ) && ( *(ps+1) == '[' ) && ( *(ps+3) == 'm' ) )
				{
				if ( *(ps+2) == '1' )
                    {
                    iSty = STY_HIGHLIGHT;
                    con_show ();
                    }
				else
                    {
                    iSty = STY_NORMAL;
                    }
				nCh -= 3;
				ps += 3;
				}
			}
		++ps;
		--nCh;
		}
    if ( ( nAlert > 0 ) && ( win_current () != con_win ) ) win_show (con_win);
	else win_refresh (con_win);
	}

void printf_circle (const char *ps)
    {
    con_print (ps, 0);
    }
