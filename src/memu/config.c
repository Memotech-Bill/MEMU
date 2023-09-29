/*      config.c  -  Display a user interface for setting configuration options */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "ff_stdio.h"
#include <stdlib.h>
#ifdef WIN32
#define BOOLEAN BOOLEANx
#include <windows.h>
#undef BOOLEAN
#define strdup _strdup
#else
#include <unistd.h>
#endif
#include "config.h"
#include "diag.h"
#define DYNAMIC_ROMS 1
#include "memu.h"
#include "monprom.h"
#include "win.h"
#include "kbd.h"
#include "mem.h"
#include "vid.h"
#include "mon.h"
#include "snd.h"
#include "sdxfdc.h"
#ifdef HAVE_SID
#include "sid.h"
#else
#define N_SIDISC    0
#endif
#ifndef SMALL_MEM
#include "cpm.h"
#endif
#ifdef HAVE_CFX2
#include "cfx2.h"
#endif
#ifdef HAVE_VGA
#include "vga.h"
#endif
#include "tape.h"
#ifdef HAVE_JOY
#include "joy.h"
#endif
#include "common.h"
#include "dirt.h"
#include "dirmap.h"

#ifdef WIN32
#define strcasecmp _stricmp
#endif

#define STY_NORMAL          0x06    // Cyan
#define STY_HIGHLIGHT       0x03    // Yellow
#define STY_DISABLED        0x04    // Blue
#define STY_HELP            0x02    // Green

#define CFG_MTX500          0
#define CFG_MTX512          1
#define CFG_SDX             2
#define CFG_CPM_MONO        3
#define CFG_CPM_COLOUR      4
#ifdef HAVE_CFX2
#define CFG_CFX2            5
#define CFG_COUNT           6
#else
#define CFG_COUNT           5
#endif

#ifdef SMALL_MEM
#define SDX_MEM_BLOCKS      4
#define CPM_MEM_BLOCKS      4
#else
#define SDX_MEM_BLOCKS      ( 512 / 16 )
#define CPM_MEM_BLOCKS      ( ( 512 + 64 ) / 16 )
#endif

#define EXIT_APPLY          0
#define EXIT_CANCEL         1
#define EXIT_RESTART        2
#define EXIT_EXIT           3
#define EXIT_COUNT          4

#define STATE_FOCUS         2
#define STATE_NORMAL        1
#define STATE_DISABLED      0

#define NUM_FLOPPY          2
#ifdef HAVE_CFX2
#define NUM_DRIVES          ( NUM_FLOPPY + N_SIDISC + NCF_CARD * NCF_PART )
#else
#define NUM_DRIVES          ( NUM_FLOPPY + N_SIDISC )
#endif

#define ROW_CFG             0                                       //  0
#define ROW_KEYMAP          ( ROW_CFG + 1 )                         //  1
#define ROW_TAPEOPT         ( ROW_KEYMAP + 1 )                      //  2
#define ROW_TAPE            ( ROW_TAPEOPT + 1 )                     //  3
#define ROW_TAPEOUT         ( ROW_TAPE + 1 )                        //  4
#define ROW_DRIVEOPT        ( ROW_TAPEOUT + 1 )                     //  5
#define ROW_DRIVE           ( ROW_DRIVEOPT + 1 )                    //  6
#define ROW_EXIT            ( ROW_DRIVE + NUM_FLOPPY + N_SIDISC )   // 12
#define ROW_HELP            ( ROW_EXIT + 1 )                        // 13
#define ROW_CHOOSE          ( ROW_HELP + 1 )                        // 14 - Rows available for selection = 10

/* The following defines initial allocation for directory listing.
   This is then doubled as often as necessary, c.f STDC++ Vector container */

#define INIT_NFILE          20
#define MAX_PATH            260

static WIN * cfg_win    =   NULL;
static const char *  config_fn  =   NULL;
static const char *  disk_dir   =   NULL;
static int     rom_enable   =   0xff;
static BOOLEAN bCfgRedraw   =   TRUE;
static int     iCfgOld      =   0;
static int     iCfgCur      =   0;
static int     iCfgSel      =   0;
static BOOLEAN bCfgRemap    =   FALSE;
static BOOLEAN bCfgNode     =   FALSE;
static BOOLEAN bCfgSound    =   FALSE;
static int     iSelKbdN     =   0;
static int     iSelTopt     =   0;
static BOOLEAN bTapeAudio   =   FALSE;
static BOOLEAN bTapeOver    =   FALSE;
static const char *  psCfgTape    =   NULL;
static const char *  psCurTape    =   NULL;
static const char *  psTapeOut    =   NULL;
static int     iSelDropt    =   0;
#ifdef HAVE_SID
static int     iSidEmu      =   0;
#endif
static BOOLEAN bFstInit     =   TRUE;
static const char *  psCfgDir[NUM_DRIVES];
static const char *  psCfgDrive[NUM_DRIVES];
// Used to track names allocated with strdup. NULL for names from argv
static const char *  psCurDrive[NUM_DRIVES];
static BOOLEAN bNoApply     =   FALSE;
static int     iCfgExit     =   0;
static BOOLEAN bCfgExit     =   FALSE;
static int     iRowHelp     =   ROW_HELP;

//  Test for entering config mode
BOOLEAN test_cfg_key (int wk)
    {
    if ( config_fn == NULL ) return FALSE;
    // diag_message (DIAG_INIT, "Test key 0x%02X for Config key", wk);
    switch (wk)
        {
//        case WK_Scroll_Lock:
        case WK_Sys_Req:
        case WK_PC_Windows_L:
        case WK_PC_Windows_R:
        case WK_PC_Menu:
        case WK_F11:
            // diag_message (DIAG_INIT, "Config Key found");
            config ();
            // diag_message (DIAG_INIT, "Config exit");
            return TRUE;
        }
    return FALSE;
    }

static void config_term (void)
    {
    if ( cfg_win != NULL )
        {
        win_delete (cfg_win);
        cfg_win  =  NULL;
        }
    }

//  Set configuration file name

void config_set_file (const char *psFile)
    {
    config_fn   =   psFile;
    }

//  Display a help message
static void cfg_help (const char *psHelp)
    {
    twin_print (cfg_win, iRowHelp, 0, STY_HELP, psHelp, WINCFG_WTH);
    }

//  Display mode selection
static void row_cfg_draw (int info, int iState)
    {
    static const char *psHelp[CFG_COUNT]   =
        {
        "<Space> to select: Memotech MTX500 with 32k byte of RAM",
        "<Space> to select: Memotech MTX512 with 64k byte of RAM",
        "<Space> to select: Memotech with 512k bytes of RAM and two type 07 disk drives",
        "<Space> to select: 576k bytes of RAM, two type 07 drives, CP/M, mono monitor",
        "<Space> to select: 576k bytes of RAM, two type 07 drives, CP/M, colour monitor",
#ifdef HAVE_CFX2
        "<Space> to select: 576k bytes of RAM, two CF cards, Propeller VGA display",
#endif
        };
/*                                     1         2         3         4         5         6         7
                                       01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
#ifdef HAVE_CFX2
    static char sCfg[]   =  " [ ] MTX500   [ ] MTX512   [ ] SDX      [ ] CPM MONO [ ] CPM COLR [ ] CFX-II ";
    int nSelWth = 13;
    sCfg[ 2] =  ( iCfgCur == CFG_MTX500     ) ? '*' : ' ';
    sCfg[15] =  ( iCfgCur == CFG_MTX512     ) ? '*' : ' ';
    sCfg[28] =  ( iCfgCur == CFG_SDX        ) ? '*' : ' ';
    sCfg[41] =  ( iCfgCur == CFG_CPM_MONO   ) ? '*' : ' ';
    sCfg[54] =  ( iCfgCur == CFG_CPM_COLOUR ) ? '*' : ' ';
    sCfg[67] =  ( iCfgCur == CFG_CFX2       ) ? '*' : ' ';
#else
    static char sCfg[]   =  " [ ] MTX500     [ ] MTX512     [ ] SDX        [ ] CPM MONO   [ ] CPM COLOUR ";
    int nSelWth = 15;
    sCfg[ 2] =  ( iCfgCur == CFG_MTX500     ) ? '*' : ' ';
    sCfg[17] =  ( iCfgCur == CFG_MTX512     ) ? '*' : ' ';
    sCfg[32] =  ( iCfgCur == CFG_SDX        ) ? '*' : ' ';
    sCfg[47] =  ( iCfgCur == CFG_CPM_MONO   ) ? '*' : ' ';
    sCfg[62] =  ( iCfgCur == CFG_CPM_COLOUR ) ? '*' : ' ';
#endif
    twin_print (cfg_win, ROW_CFG, 0, STY_NORMAL, sCfg, 0);
    if ( iState == STATE_FOCUS )
        {
        twin_print (cfg_win, ROW_CFG, nSelWth*iCfgSel, STY_HIGHLIGHT, &sCfg[nSelWth*iCfgSel], nSelWth);
        cfg_help (psHelp[iCfgSel]);
        }
    }

//  Key processing for major mode selection
static BOOLEAN row_cfg_key (int info, int wk)
    {
    switch (wk)
        {
        case  WK_Left:
        {
        if ( iCfgSel == 0 )  iCfgSel  =  CFG_COUNT;
        --iCfgSel;
        break;
        }
        case  WK_Right:
        {
        ++iCfgSel;
        if ( iCfgSel == CFG_COUNT )  iCfgSel  =  0;
        break;
        }
        case  WK_Return:
        case  ' ':
        {
        iCfgCur  =  iCfgSel;
        bCfgRedraw  =  TRUE;
        break;
        }
        default:
        {
        return   FALSE;
        }
        }
    return   TRUE;
    }

//  Display keyboard map selection
static void row_kbd_draw (int info, int iState)
    {
    static char sNomap[] =  " [ ] Normal   ";
    static char sRemap[] =  " [ ] Remapped ";
    static char sNoSnd[] =  " [ ] No  ";
    static char sSound[] =  " [ ] Yes ";
    static const char *psHelp[] = {
        "<Space> to select: Keys match MTX keyboard - Not always PC key symbols",
        "<Space> to select: Multiple keyboard modes - More closely match PC keys symbols",
        "<Space> to select: Disable Sound",
        "<Space> to select: Enable Sound"
        };
    int iSel;
    if ( iState == STATE_FOCUS ) iSel = iSelKbdN;
    else iSel = -1;
    if ( bCfgRemap )
        {
        sNomap[2]   =  ' ';
        sRemap[2]   =  '*';
        }
    else
        {
        sNomap[2]   =  '*';
        sRemap[2]   =  ' ';
        }
    if ( bCfgSound )
        {
        sNoSnd[2]   =  ' ';
        sSound[2]   =  '*';
        }
    else
        {
        sNoSnd[2]   =  '*';
        sSound[2]   =  ' ';
        }
    twin_print (cfg_win, ROW_KEYMAP, 0, STY_NORMAL, "Keyboard: ", 0);
    twin_print (cfg_win, ROW_KEYMAP, 10, ( iSel == 0 ) ? STY_HIGHLIGHT : STY_NORMAL, sNomap, 0);
    twin_print (cfg_win, ROW_KEYMAP, 24, ( iSel == 1 ) ? STY_HIGHLIGHT : STY_NORMAL, sRemap, 0);
    twin_print (cfg_win, ROW_KEYMAP, 40, STY_NORMAL, "Sound: ", 0);
    twin_print (cfg_win, ROW_KEYMAP, 50, ( iSel == 2 ) ? STY_HIGHLIGHT : STY_NORMAL, sNoSnd, 0);
    twin_print (cfg_win, ROW_KEYMAP, 59, ( iSel == 3 ) ? STY_HIGHLIGHT : STY_NORMAL, sSound, 0);
    if ( iState == STATE_FOCUS ) cfg_help (psHelp[iSelKbdN]);
    }

//  Key processing for keyboard map selection
static BOOLEAN row_kbd_key (int info, int wk)
    {
    int nSel = 4;
    switch (wk)
        {
        case  WK_Left:
        {
        if ( iSelKbdN == 0 ) iSelKbdN = nSel;
        --iSelKbdN;
        break;
        }
        case  WK_Right:
        {
        ++iSelKbdN;
        if ( iSelKbdN == nSel ) iSelKbdN = 0;
        break;
        }
        case  WK_Return:
        case  ' ':
        {
        if ( iSelKbdN == 0 )      bCfgRemap = FALSE;
        else if ( iSelKbdN == 1 ) bCfgRemap = TRUE;
        else if ( ( iSelKbdN == 2 ) && ( bCfgSound ) )
            {
            bCfgSound = FALSE;
            bNoApply = TRUE;
            bCfgRedraw = TRUE;
            }
        else if ( ( iSelKbdN == 3 ) && ( ! bCfgSound ) )
            {
            bCfgSound = TRUE;
            bNoApply = TRUE;
            bCfgRedraw = TRUE;
            }
        break;
        }
        default:
        {
        return   FALSE;
        }
        }
    return   TRUE;
    }

//  Display tape option selection
static void row_topt_draw (int info, int iState)
    {
    static char sNo[]   =  " [ ] No  ";
    static char sYes[]  =  " [ ] Yes ";
    static const char *psHelp[] = {   "<Space> to select: Fast. \".mtx\" files only",
                                      "<Space> to select: Slow. \".mtx\" and \".wav\" files",
                                      "<Space> to select: Protects existing tape files",
                                      "<space> to select: Allow overwriting existing files"};
    int iSel;
    if ( iState == STATE_FOCUS ) iSel = iSelTopt;
    else iSel = -1;
    if ( bTapeAudio )
        {
        sNo[2]  =  ' ';
        sYes[2] =  '*';
        }
    else
        {
        sNo[2]  =  '*';
        sYes[2] =  ' ';
        }
    twin_print (cfg_win, ROW_TAPEOPT, 0, STY_NORMAL, "Audio tapes: ", 0);
    twin_print (cfg_win, ROW_TAPEOPT, 14, ( iSel == 0 ) ? STY_HIGHLIGHT : STY_NORMAL, sNo, 0);
    twin_print (cfg_win, ROW_TAPEOPT, 24, ( iSel == 1 ) ? STY_HIGHLIGHT : STY_NORMAL, sYes, 0);
    if ( bTapeOver )
        {
        sNo[2]  =  ' ';
        sYes[2] =  '*';
        }
    else
        {
        sNo[2]  =  '*';
        sYes[2] =  ' ';
        }
    twin_print (cfg_win, ROW_TAPEOPT, 40, STY_NORMAL, "Overwrite tapes: ", 0);
    twin_print (cfg_win, ROW_TAPEOPT, 57, ( iSel == 2 ) ? STY_HIGHLIGHT : STY_NORMAL, sNo, 0);
    twin_print (cfg_win, ROW_TAPEOPT, 67, ( iSel == 3 ) ? STY_HIGHLIGHT : STY_NORMAL, sYes, 0);
    if ( iState == STATE_FOCUS ) cfg_help (psHelp[iSelTopt]);
    }

//  Key processing for keyboard map selection
static BOOLEAN row_topt_key (int info, int wk)
    {
    int nSel = 4;
    switch (wk)
        {
        case  WK_Left:
        {
        if ( iSelTopt == 0 ) iSelTopt = nSel;
        --iSelTopt;
        break;
        }
        case  WK_Right:
        {
        ++iSelTopt;
        if ( iSelTopt == nSel ) iSelTopt = 0;
        break;
        }
        case  WK_Return:
        case  ' ':
        {
        if ( ( iSelTopt == 0 ) && ( bTapeAudio ) )
            {
            bTapeAudio = FALSE;
            // bNoApply = TRUE;
            bCfgRedraw = TRUE;
            }
        else if ( ( iSelTopt == 1 ) && ( ! bTapeAudio ) )
            {
            bTapeAudio = TRUE;
            // bNoApply = TRUE;
            bCfgRedraw = TRUE;
            }
        else if ( iSelTopt == 2 ) bTapeOver = FALSE;
        else if ( iSelTopt == 3 ) bTapeOver = TRUE;
        break;
        }
        default:
        {
        return   FALSE;
        }
        }
    return   TRUE;
    }

//  Select an item from a displayed list
static int cfg_choose (int iRowChoose, int nItem, char **ppsItem)
    {
    int  nLenMax   =  0;
    int  nLen, i;
    int  nCol, nWth;
    int  iChoose, iScroll, iItem;
    int  iRow, iCol;
    int  nVis   =  WINCFG_HGT - iRowChoose;
    cfg_help ("Cursor keys to move highlight, <Space> to select, <Esc> to cancel");
    for ( i = 0; i < nItem; ++i )
        {
        nLen  =  (int) strlen (ppsItem[i]);
        if ( nLen > nLenMax )   nLenMax  =  nLen;
        }
    if ( nLenMax + 2 < WINCFG_WTH )  nCol  =  WINCFG_WTH / ( nLenMax + 2 );
    else                             nCol  =  1;
    nWth =   WINCFG_WTH / nCol;
    iChoose  =  0;
    iScroll  =  0;
    while (TRUE)
        {
        iItem    =  iScroll;
        for ( iRow = 0; ( iRow < nVis ) && ( iItem < nItem ); ++iRow )
            {
            for ( iCol = 0; ( iCol < nCol ) && ( iItem < nItem ); ++iCol, ++iItem )
                {
                if ( iItem == iChoose )
                    twin_print (cfg_win, iRow + iRowChoose, iCol * nWth, STY_HIGHLIGHT, ppsItem[iItem], nWth);
                else
                    twin_print (cfg_win, iRow + iRowChoose, iCol * nWth, STY_NORMAL, ppsItem[iItem], nWth);
                }
            }
        switch (twin_kbd_in (cfg_win))
            {
            case  WK_Right:
            {
            if ( iChoose < nItem - 1 )
                {
                ++iChoose;
                if ( ( iChoose - iScroll ) / nCol >= nVis )  iScroll  += nCol;
                }
            break;
            }
            case  WK_Left:
            {
            if ( iChoose > 0 )
                {
                --iChoose;
                if ( iChoose < iScroll )   iScroll  -= nCol;
                }
            break;
            }
            case  WK_Down:
            {
            if ( iChoose < nItem - nCol )
                {
                iChoose  += nCol;
                if ( ( iChoose - iScroll ) / nCol >= nVis )  iScroll  += nCol;
                }
            break;
            }
            case  WK_Up:
            {
            if ( iChoose >= nCol )
                {
                iChoose  -= nCol;
                if ( iChoose < iScroll )   iScroll  -= nCol;
                }
            break;
            }
            case  WK_Return:
            case  ' ':
            {
            twin_clear_rows (cfg_win, iRowChoose, WINCFG_HGT);
            return   iChoose;
            }
            case  WK_Escape:
            {
            twin_clear_rows (cfg_win, iRowChoose, WINCFG_HGT);
            return   -1;
            }
            }
        }
    }

static BOOLEAN cfg_match_ext (int nExt, const char *psExt, const char *psFile)
    {
    BOOLEAN bMatch = FALSE;
    int iExt;
    if ( nExt == 0 ) return TRUE;
    for ( iExt = 0; iExt < nExt; ++iExt )
        {
        int nExt =  (int) strlen (psExt);
        int nCh  =  (int) strlen (psFile);
        if ( ( nCh > nExt ) && ( strcasecmp (&psFile[nCh-nExt], psExt) == 0 ) )
            {
            bMatch = TRUE;
            break;
            }
        psExt += nExt + 1;
        }
    return bMatch;
    }

//  Enter name of new file
static char *cfg_new_file (int iRowChoose, const char *psDir, int nExt, const char *psExt)
    {
    int wk;
    char *psPath;
    char sFile[MAX_PATH];
    strncpy (sFile, psExt, MAX_PATH);
    sFile[MAX_PATH-1]   =  '\0';
    cfg_help ("Enter new file name below, <Return> to finish, <Esc> to cancel");
    twin_print (cfg_win, iRowChoose, 0, STY_NORMAL, "New file:", 0);
    wk = twin_edit (cfg_win, iRowChoose, 10, WINCFG_WTH - 10, STY_NORMAL, MAX_PATH, sFile, NULL);
    twin_clear_rows (cfg_win, iRowChoose, iRowChoose + 1);
    if ( wk == WK_Return )
        {
        FILE  *pfil;
        int   nFile =  (int) strlen (sFile);
        int   nExt  =  (int) strlen (psExt);
        if ( ! cfg_match_ext (nExt, psExt, sFile) )
            {
            if ( nFile + nExt > MAX_PATH - 1 )  strcpy (&sFile[MAX_PATH - 1 - nExt], psExt);
            else                                strcpy (&sFile[nFile], psExt);
            nFile =  (int) strlen (sFile);
            }
        psPath  = make_path (psDir, sFile);
        pfil    = fopen (psPath, "wb");
        if ( pfil == NULL )  { config_term (); fatal ("Failed to create file %s", psPath); }
        fclose (pfil);
        free (psPath);
        return   estrdup (sFile);
        }
    return   NULL;
    }

//  Display a list of files to choose from
static const char *cfg_choose_file (int iRowChoose, const char *psDir, int nExt, const char *psExt,
    BOOLEAN bDisk)
    {
    char *psFile;
    const char *psFile2;
    char **ppsFiles   =  (char **) malloc (INIT_NFILE * sizeof (char *));
    int  nAlloc       =  INIT_NFILE;
    int  nFiles       =  1;
    int  nFirst;
    int  i;
    int  iErr;
    int  iChoose;
    DIRT *dirt;
    BOOLEAN bSorted;
    if ( ( psDir == NULL ) || ( psDir[0] == '\0' ) )
        {
        config_term ();
        fatal ("No directory specified for %s files", bDisk ? "disk image" : "tape");
        }
    dirt =  dirt_open (PMapPath (psDir), &iErr);
    if ( iErr != DIRTE_OK ) { config_term (); fatal (dirt_error (iErr)); }
    ppsFiles[0] =  estrdup ("<None>");
    if ( bDisk )
        {
        nFiles   =  2;
        ppsFiles[1] =  estrdup ("<New>");
        }
    nFirst = nFiles;
    while ( ( psFile2 = dirt_next (dirt) ) != NULL )
        {
        if ( cfg_match_ext (nExt, psExt, psFile2) )
            {
            if ( nFiles >= nAlloc )
                {
                nAlloc   *= 2;
                ppsFiles =  (char **) realloc (ppsFiles, nAlloc * sizeof (char *));
                if ( ppsFiles == NULL ) { config_term (); fatal ("Memory allocation failure"); }
                }
            ppsFiles[nFiles]  =  estrdup (psFile2);
            ++nFiles;
            }
        }
    dirt_close (dirt);
    bSorted = FALSE;
    while ( ! bSorted )
        {
        bSorted = TRUE;
        for ( i = nFirst; i < nFiles - 1; ++i )
            {
            if ( strcasecmp (ppsFiles[i], ppsFiles[i+1]) > 0 )
                {
                psFile = ppsFiles[i+1];
                ppsFiles[i+1] = ppsFiles[i];
                ppsFiles[i] = psFile;
                bSorted = FALSE;
                }
            }
        }
    iChoose  =  cfg_choose (iRowChoose, nFiles, ppsFiles);
    if ( iChoose >= 0 )  psFile   =  ppsFiles[iChoose];
    else                 psFile   =  NULL;
    for ( i = 0; i < nFiles; ++i )
        {
        if ( ppsFiles[i] != psFile )  free ((void *) ppsFiles[i]);
        }
    free ((void *) ppsFiles);
    if ( ( bDisk ) && ( iChoose == 1 ) )
        {
        free ((void *) psFile);
        psFile   =  cfg_new_file (iRowChoose, psDir, nExt, psExt);
        }
    return   psFile;
    }

//  Display a file selection configuration line (tape or disk emulation file)
static void file_draw (int iRow, const char *psTitle, const char *psFile, int iState)
    {
    char sLine[WINCFG_WTH+1];
    int  nCh =  (int) strlen (psTitle);
    strcpy (sLine, psTitle);
    if ( psFile != NULL )
        {
        strncpy (&sLine[nCh], psFile, WINCFG_WTH - nCh);
        sLine[WINCFG_WTH] =  '\0';
        }
    if ( iState == STATE_FOCUS )
        {
        twin_print (cfg_win, iRow, 0, STY_HIGHLIGHT, sLine, WINCFG_WTH);
        cfg_help ("<Space> to change selected file");
        }
    else if ( iState == STATE_NORMAL )
        {
        twin_print (cfg_win, iRow, 0, STY_NORMAL, sLine, WINCFG_WTH);
        }
    else
        {
        twin_print (cfg_win, iRow, 0, STY_DISABLED, sLine, WINCFG_WTH);
        }
    }

//  File selection key processing
static BOOLEAN file_key (const char *psDir, int nExt, const char *psExt, BOOLEAN bDisk,
    const char **ppsFile, int wk)
    {
    if ( ( wk == WK_Return ) || ( wk == ' ' ) )
        {
        const char *psFile   =  cfg_choose_file (ROW_CHOOSE, psDir, nExt, psExt, bDisk);
        if ( psFile != NULL )
            {
            if ( *ppsFile != NULL ) free ((void *) *ppsFile);
            *ppsFile =  psFile;
            }
        return   TRUE;
        }
    return   FALSE;
    }

//  Display tape emulation file
static void row_tape_draw (int info, int iState)
    {
    file_draw (ROW_TAPE, "Tape: ", psCfgTape, iState);
    }

//  Process tape emulation key press
static BOOLEAN row_tape_key (int info, int wk)
    {
    return   file_key (cfg.tape_name_prefix, bTapeAudio ? 2 : 1, ".mtx\0.wav", FALSE, &psCfgTape, wk);
    }

//  Display tape output file
static void row_tout_draw (int info, int iState)
    {
    file_draw (ROW_TAPEOUT, "Save Tape: ", psTapeOut, iState);
    }

//  Process tape emulation key press
static BOOLEAN row_tout_key (int info, int wk)
    {
    return   file_key (cfg.tape_name_prefix, bTapeAudio ? 2 : 1, ".mtx\0.wav", TRUE, &psTapeOut, wk);
    }

#ifdef HAVE_SID
static BOOLEAN cfg_match_fn (const char *psPath1, const char *psPath2)
    {
    const char *psFN1;
    const char *psFN2;
#ifdef WIN32
    const char *psTmp;
#endif
    if ( ( psPath1 == NULL ) || ( psPath2 == NULL ) ) return FALSE;
    psFN1 = strrchr (psPath1, '/');
    if ( psFN1 == NULL ) psFN1 = psPath1;
    else ++psFN1;
    psFN2 = strrchr (psPath2, '/');
    if ( psFN2 == NULL ) psFN2 = psPath2;
    else ++psFN2;
#ifdef WIN32
    psTmp = strrchr (psFN1, '\\');
    if ( psTmp != NULL ) psFN1 = psTmp + 1;
    psTmp = strrchr (psFN2, '\\');
    if ( psTmp != NULL ) psFN2 = psTmp + 1;
#endif
    return ( strcmp (psFN1, psFN2) == 0 );
    }

//  Display drive option selection
static void cfg_clear_sid (void)
    {
    int iDrv;
    for ( iDrv = NUM_FLOPPY; iDrv < NUM_FLOPPY + N_SIDISC; ++iDrv )
        {
        /*
        diag_message (DIAG_ALWAYS, "clear_sid: %d, Cur = %s, Cfg = %s", iDrv,
            cfg.sid_fn[iDrv-NUM_FLOPPY] ? cfg.sid_fn[iDrv-NUM_FLOPPY] : "NULL",
            psCfgDrive[iDrv] ? psCfgDrive[iDrv] : "NULL");
        */
        if ( cfg_match_fn (psCfgDrive[iDrv], cfg.sid_fn[iDrv-NUM_FLOPPY]) )
            {
            // diag_message (DIAG_ALWAYS, "Cleared drive %d: %s", iDrv, psCfgDrive[iDrv]);
            free ((void *)psCfgDrive[iDrv]);
            psCfgDrive[iDrv] = estrdup ("<None>");
            }
        }
    }

static void row_dropt_draw (int info, int iState)
    {
    static char sNo[]   =  " [ ] No  ";
    static char sYes[]  =  " [ ] Yes ";
    static const char *psHelp[] = {   "<Space> to select: SiDiscs limited to 8MB",
                                      "<Space> to select: Huge SiDiscs for HexTrain",
                                      "<Space> to select: SiDisc contents lost on exit",
                                      "<space> to select: SiDisc contents saved. Slow for huge disks"};
    int iSty = ( iState == STATE_DISABLED ) ? STY_DISABLED : STY_NORMAL;
    int iSel;
    if ( iState == STATE_FOCUS ) iSel = iSelDropt;
    else iSel = -1;
    if ( iSidEmu & SIDEMU_HUGE )
        {
        sNo[2]  =  ' ';
        sYes[2] =  '*';
        }
    else
        {
        sNo[2]  =  '*';
        sYes[2] =  ' ';
        }
    twin_print (cfg_win, ROW_DRIVEOPT, 0, iSty, "Huge SiDiscs: ", 0);
    twin_print (cfg_win, ROW_DRIVEOPT, 14, ( iSel == 0 ) ? STY_HIGHLIGHT : iSty, sNo, 0);
    twin_print (cfg_win, ROW_DRIVEOPT, 24, ( iSel == 1 ) ? STY_HIGHLIGHT : iSty, sYes, 0);
    if ( iSidEmu & SIDEMU_NO_SAVE )
        {
        sNo[2]  =  '*';
        sYes[2] =  ' ';
        }
    else
        {
        sNo[2]  =  ' ';
        sYes[2] =  '*';
        }
    twin_print (cfg_win, ROW_DRIVEOPT, 40, iSty, "Save silicon drives: ", 0);
    twin_print (cfg_win, ROW_DRIVEOPT, 61, ( iSel == 2 ) ? STY_HIGHLIGHT : iSty, sNo, 0);
    twin_print (cfg_win, ROW_DRIVEOPT, 71, ( iSel == 3 ) ? STY_HIGHLIGHT : iSty, sYes, 0);
    if ( iState == STATE_FOCUS ) cfg_help (psHelp[iSelDropt]);
    }

//  Key processing for keyboard map selection
static BOOLEAN row_dropt_key (int info, int wk)
    {
    int nSel = 4;
    switch (wk)
        {
        case  WK_Left:
        {
        if ( iSelDropt == 0 ) iSelDropt = nSel;
        --iSelDropt;
        break;
        }
        case  WK_Right:
        {
        ++iSelDropt;
        if ( iSelDropt == nSel ) iSelDropt = 0;
        break;
        }
        case  WK_Return:
        case  ' ':
        {
        if ( ( iSelDropt == 0 ) && ( iSidEmu & SIDEMU_HUGE ) )
            {
            cfg_clear_sid ();
            iSidEmu &= ~ SIDEMU_HUGE;
            bNoApply = TRUE;
            bCfgRedraw = TRUE;
            }
        else if ( ( iSelDropt == 1 ) && ( ! ( iSidEmu & SIDEMU_HUGE ) ) )
            {
            iSidEmu |= SIDEMU_HUGE;
            bNoApply = TRUE;
            bCfgRedraw = TRUE;
            }
        else if ( iSelDropt == 2 )
            {
            iSidEmu |= SIDEMU_NO_SAVE;
            }
        else if ( ( iSelDropt == 3 ) && ( iSidEmu & SIDEMU_NO_SAVE ) )
            {
            cfg_clear_sid ();
            iSidEmu &= ~ SIDEMU_NO_SAVE;
            bCfgRedraw = TRUE;
            }
        break;
        }
        default:
        {
        return   FALSE;
        }
        }
    return   TRUE;
    }
#endif

// Display a row from a CF Card configuration
#ifdef HAVE_CFX2
static void cfg_cf_card_row (const char *psPart[NCF_PART], int iRow, int iState)
    {
    char sPart[4];
    if ( iRow < NCF_PART )
        {
        sprintf (sPart, "%d: ", iRow);
        file_draw (iRow + 1, sPart, psPart[iRow], iState);
        }
    else
        {
        twin_print (cfg_win, 9, 0, ( iState == STATE_FOCUS ) ? STY_HIGHLIGHT : STY_NORMAL, "Exit", 0);
        }
    }

// Edit the list of partitions making up a CF card
static BOOLEAN cfg_cf_card (int iCard, const char *psDir, const char *psPart[NCF_PART])
    {
    char sLine[WINCFG_WTH+1];
    const char *psFile;
    BOOLEAN bChg = FALSE;
    int iSel = 0;
    int wk;
    int i;
    twin_clear_rows (cfg_win, 0, WINCFG_HGT);
    iRowHelp = NCF_PART + 2;
    sprintf (sLine, "Partitions for Compact Flash Card %d", iCard);
    twin_print (cfg_win, 0, 0, STY_HELP, sLine, 0);
    for ( i = 0; i <= NCF_PART; ++i )
        {
        cfg_cf_card_row (psPart, i, STATE_NORMAL);
        }
    while (TRUE)
        {
        cfg_cf_card_row (psPart, iSel, STATE_FOCUS);
        wk =  twin_kbd_in (cfg_win);
        cfg_cf_card_row (psPart, iSel, STATE_NORMAL);
        switch (wk)
            {
            case WK_Down:
                if ( ++iSel > NCF_PART ) iSel = 0;
                break;
            case WK_Up:
                if ( --iSel < 0 ) iSel = NCF_PART;
                break;
            case WK_Return:
            case ' ':
                if ( iSel == NCF_PART )
                    {
                    twin_clear_rows (cfg_win, 0, WINCFG_HGT);
                    return bChg;
                    }
                psFile = cfg_choose_file (NCF_PART + 3, psDir, ( iSel == 0 ? 3 : 2),
                    ".img\0.sid\0.drive", TRUE);
                if ( psFile != NULL )
                    {
                    bChg = TRUE;
                    free ((void *) psPart[iSel]);
                    psPart[iSel] = psFile;
                    }
                break;
            case WK_Escape:
                twin_clear_rows (cfg_win, 0, WINCFG_HGT);
                return bChg;
                break;
            }
        }
    }
#endif

//  Display disk emulation file
static void row_drive_draw (int iDrive, int iState)
    {
    char sName[] = "Drive X:";
    char chDrive[] = { 'B', 'C', 'F', 'G', 'H', 'I'};
#ifdef HAVE_CFX2
    if ( ( iDrive < NCF_CARD ) && ( iCfgCur == CFG_CFX2 ) )
        {
        char sLine[42];
        int nPart = 0;
        int i;
        for ( i = 0; i < NCF_PART; ++i )
            {
            const char *psPart = psCfgDrive[NUM_FLOPPY + N_SIDISC + NCF_PART * iDrive + i];
            if ( ( psPart != NULL ) && ( psPart[0] != '\0' )
                && ( strcmp (psPart, "<None>") != 0 ) ) ++nPart;
            }
        sprintf (sLine, "Compact Flash Card %d: %d Active Partitions", iDrive, nPart);
        twin_print (cfg_win, ROW_DRIVE + iDrive, 0, ( iState == STATE_FOCUS ) ? STY_HIGHLIGHT : STY_NORMAL,
            sLine, 0);
        cfg_help ("<Space> to edit CF partitions");
        return;
        }
#endif
    sName[6] = chDrive[iDrive];
    file_draw (ROW_DRIVE + iDrive, sName, psCfgDrive[iDrive], iState);
    }

//  Process drive configuration key presses
static BOOLEAN row_drive_key (int iDrive, int wk)
    {
    int nExt = 2;
    const char *psExt = ".mfloppy\0.mfloppy-07";
    BOOLEAN bChg = FALSE;
#ifdef HAVE_CFX2
    if ( ( iDrive < NCF_CARD ) && ( iCfgCur == CFG_CFX2 ) )
        {
        if ( ( wk == WK_Return ) || ( wk == ' ' ) )
            {
            int iPart = NUM_FLOPPY + N_SIDISC + NCF_PART * iDrive;
            bChg = cfg_cf_card (iDrive, psCfgDir[iPart], &psCfgDrive[iPart]);
            if ( bChg ) bNoApply = TRUE;
            iRowHelp = ROW_HELP;
            bCfgRedraw = TRUE;
            }
        return bChg;
        }
#endif
    if ( iDrive >= NUM_FLOPPY )
        {
        psExt = ".img\0.bin\0.sid";
        nExt = 3;
        // if ( iSidEmu & SIDEMU_HUGE ) nExt = 3;
        }
    bChg = file_key (psCfgDir[iDrive], nExt, psExt, TRUE, &psCfgDrive[iDrive], wk);
    if ( bChg && ( ! bNoApply ) && ( iDrive >= NUM_FLOPPY ) )
        {
        bCfgRedraw  =  TRUE;
        bNoApply = TRUE;
        }
    return bChg;
    }

// Identify all forms of no file
const char *cfg_def_file (const char *psFile)
    {
    if ( ( psFile == NULL ) || ( psFile[0] == '\0' ) || ( strcmp (psFile, "<None>") == 0 ) ) return NULL;
    return psFile;
    }

const char *cfg_def_path (const char *psDir, const char *psFile)
    {
    if ( ( psFile == NULL ) || ( psFile[0] == '\0' ) || ( strcmp (psFile, "<None>") == 0 ) ) return NULL;
    return make_path (psDir, psFile);
    }

//  Set default directory for emulated disks
void cfg_set_disk_dir (const char *psDir)
    {
    if ( disk_dir != NULL )  free ((void *)disk_dir);
    disk_dir =  NULL;
    if ( psDir != NULL ) disk_dir  =  estrdup (psDir);
    }

//  Get directory and file for emulated disk drive
static void cfg_get_drive (int iDrive, const char **ppsDir, const char **ppsFile)
    {
    const char *ps1;
    if ( iDrive < NUM_FLOPPY ) ps1 = cfg.fn_sdxfdc[iDrive];
#ifdef HAVE_CFX2
    else if ( iDrive >= NUM_FLOPPY + N_SIDISC + NCF_PART )
        ps1 = cfg.fn_cfx2[1][iDrive - (NUM_FLOPPY + N_SIDISC + NCF_PART)];
    else if ( iDrive >= NUM_FLOPPY + N_SIDISC )
        ps1 = cfg.fn_cfx2[0][iDrive - (NUM_FLOPPY + N_SIDISC)];
#endif
#ifdef HAVE_SID
    else ps1 = cfg.sid_fn[iDrive - NUM_FLOPPY];
#endif
    if ( *ppsFile != NULL ) free ((void *) *ppsFile);
    if ( ( ps1 == NULL ) || ( ps1[0] == '\0' ) )
        {
        *ppsFile =  estrdup ("<None>");
        }
    else
        {
        const char *ps2   =  strrchr (ps1, '/');
        if ( ps2 != NULL )
            {
            char *ps3;
            int   nDir  =  (int) (ps2 - ps1);
            if ( *ppsDir != NULL ) free ((void *) *ppsDir);
            ps3  =  (char *) emalloc (nDir + 1);
            strncpy (ps3, ps1, nDir);
            ps3[nDir]   =  '\0';
            *ppsDir     =  ps3;
            ps1   =  ps2 + 1;
            if ( disk_dir == NULL )   cfg_set_disk_dir (*ppsDir);
            }
        else if ( *ppsDir == NULL )
            {
            const char *ps2   =  disk_dir;
            if ( ps2 != NULL )   *ppsDir  =  estrdup (ps2);
            }
        *ppsFile =  estrdup (ps1);
        }
    }

//  Save emulated disk drive
static void cfg_save_drive (int iDrive, const char *psDir, const char *psFile)
    {
#ifdef HAVE_CFX2
    int iCard;
    int iPart;
#endif
    int nMaxDrv = 0;
    int nMinDrv = 0;
    const char *psPath = cfg_def_path (psDir, psFile);
    if ( iCfgOld == CFG_SDX ) nMaxDrv = NUM_FLOPPY;
#ifdef HAVE_CFX2
    else if ( iCfgOld == CFG_CFX2 )
        {
        nMinDrv = NUM_FLOPPY;
        nMaxDrv = NUM_FLOPPY + N_SIDISC + NCF_CARD * NCF_PART;
        }
#endif
    else if ( iCfgOld > CFG_SDX ) nMaxDrv = NUM_FLOPPY + N_SIDISC;
    if ( ( iDrive >= nMinDrv ) && ( iDrive < nMaxDrv ) )
        {
        const char *psOld;
        if ( iDrive < NUM_FLOPPY )
            {
            psOld = cfg.fn_sdxfdc[iDrive];
            }
#ifdef HAVE_CFX2
        else if ( iDrive >= NUM_FLOPPY + N_SIDISC )
            {
            iCard = ( iDrive - NUM_FLOPPY - N_SIDISC ) / NCF_PART;
            iPart = iDrive - NUM_FLOPPY - N_SIDISC - NCF_PART * iCard;
            psOld = cfx2_get_image (iCard, iPart);
            }
#endif
#ifdef HAVE_SID
        else
            {
            psOld = sid_get_file (iDrive - NUM_FLOPPY);
            }
#endif
        diag_message (DIAG_INIT, "cfg_save_drive: drive = %d psOld = %s psPath = %s",
            iDrive, psOld, psPath);
        if ( ( psOld != NULL ) && ( ( psPath == NULL ) || ( strcmp (psPath, psOld) != 0 ) ) )
            {
            if ( iDrive < NUM_FLOPPY )
                {
                diag_message (DIAG_INIT, "sdxfdc_init (%d, NULL)", iDrive);
                sdxfdc_init (iDrive, NULL);
                cfg.fn_sdxfdc[iDrive] = NULL;
                }
#ifdef HAVE_CFX2
            else if ( iDrive >= NUM_FLOPPY + N_SIDISC )
                {
                diag_message (DIAG_INIT, "cfx2_set_image (%d, %d, NULL)", iCard, iPart);
                cfx2_set_image (iCard, iPart, NULL);
                cfg.fn_cfx2[iCard][iPart] = NULL;
                }
#endif
#ifdef HAVE_SID
            else
                {
                diag_message (DIAG_INIT, "sid_set_file (%d, NULL)", iDrive - NUM_FLOPPY);
                if ( ! ( cfg.sid_emu & SIDEMU_NO_SAVE ) ) sid_save (iDrive - NUM_FLOPPY, TRUE);
                sid_set_file (iDrive - NUM_FLOPPY, NULL);
                cfg.sid_fn[iDrive - NUM_FLOPPY] = NULL;
                }
#endif
            }
        if ( psPath != NULL ) free ((void *) psPath);
        }
    }

//  Set directory and file for emulated disk drive
static void cfg_set_drive (int iDrive, const char *psDir, const char *psFile)
    {
#ifdef HAVE_CFX2
    int iCard = 0;
    int iPart = 0;
#endif
    int nMaxDrv = 0;
    int nMinDrv = 0;
    const char *psPath = cfg_def_path (psDir, psFile);
    if ( iCfgCur == CFG_SDX ) nMaxDrv = NUM_FLOPPY;
#ifdef HAVE_CFX2
    else if ( iCfgCur == CFG_CFX2 )
        {
        nMinDrv = NUM_FLOPPY;
        nMaxDrv = NUM_FLOPPY + N_SIDISC + NCF_CARD * NCF_PART;
        }
#endif
    else if ( iCfgCur > CFG_SDX ) nMaxDrv = NUM_FLOPPY + N_SIDISC;
    if ( ( iDrive >= nMinDrv ) && ( iDrive < nMaxDrv ) )
        {
        const char *psOld;
        if ( iDrive < NUM_FLOPPY )
            {
            psOld = cfg.fn_sdxfdc[iDrive];
            }
#ifdef HAVE_CFX2
        else if ( iDrive >= NUM_FLOPPY + N_SIDISC )
            {
            iCard = ( iDrive - NUM_FLOPPY - N_SIDISC ) / NCF_PART;
            iPart = iDrive - NUM_FLOPPY - N_SIDISC - NCF_PART * iCard;
            psOld = cfx2_get_image (iCard, iPart);
            }
#endif
#ifdef HAVE_SID
        else
            {
            psOld = sid_get_file (iDrive - NUM_FLOPPY);
            }
#endif
        diag_message (DIAG_INIT, "cfg_set_drive: drive = %d psOld = %s psPath = %s",
            iDrive, psOld, psPath);
        if ( ( psPath != NULL ) && ( ( psOld == NULL ) || ( strcmp (psPath, psOld) != 0 ) ) )
            {
            if ( iDrive < NUM_FLOPPY )
                {
                diag_message (DIAG_INIT, "sdxfdc_init (%d, %s)", iDrive, psPath);
                sdxfdc_init (iDrive, psPath);
                cfg.fn_sdxfdc[iDrive] = psPath;
                }
#ifdef HAVE_CFX2
            else if ( iDrive >= NUM_FLOPPY + N_SIDISC )
                {
                diag_message (DIAG_INIT, "cfx2_set_image (%d, %d, %s)", iCard, iPart, psPath);
                cfx2_set_image (iCard, iPart, psPath);
                cfg.fn_cfx2[iCard][iPart] = psPath;
                }
#endif
#ifdef HAVE_SID
            else
                {
                diag_message (DIAG_INIT, "sid_set_file (%d, %s)", iDrive - NUM_FLOPPY, psPath);
                sid_set_file (iDrive - NUM_FLOPPY, psPath);
                cfg.sid_fn[iDrive - NUM_FLOPPY] = psPath;
                sid_load (iDrive - NUM_FLOPPY);
                }
#endif
            if ( psCurDrive[iDrive] != NULL ) free ((void *) psCurDrive[iDrive]);
            psCurDrive[iDrive] = psPath;
            }
        }
    }

//  Set tape emulation file
static void cfg_set_tape (const char *psTape)
    {
    if ( psCurTape != NULL ) free ((void *) psCurTape);
    psCurTape = NULL;
    if ( psTape != NULL ) psCurTape = estrdup (psTape);
    cfg.tape_fn = psCurTape;
    }

//  Determine current configuration and create configuration window
static void cfg_init (void)
    {
    int iDrive;
    const char *psTmp;
    if ( bFstInit )
        {
        for ( iDrive = 0; iDrive < NUM_DRIVES; ++iDrive )
            {
            psCfgDir[iDrive] = NULL;
            psCfgDrive[iDrive] = NULL;
            psCurDrive[iDrive] = NULL;
            }
        bFstInit = FALSE;
        }
#ifdef SMALL_MEM
    if ( rom_enable & ROMEN_CPM )
        {
        if ( cfg.mon_emu & MONEMU_WIN_MONO ) iCfgOld  =  CFG_CPM_MONO;
        else                                 iCfgOld  =  CFG_CPM_COLOUR;
        }
    else if ( rom_enable & ROMEN_SDX2 )
        {
        iCfgOld  =  CFG_SDX;
        }
#else
    if ( ( rom_enable & ROMEN_CPM ) && ( cfg.rom_fn[ROM_CPM] != NULL ) )
        {
        if ( cfg.mon_emu & MONEMU_WIN_MONO ) iCfgOld  =  CFG_CPM_MONO;
        else                                 iCfgOld  =  CFG_CPM_COLOUR;
        }
    else if ( ( rom_enable & ROMEN_SDX2 ) && ( cfg.rom_fn[ROM_SDX2] != NULL ) )
        {
        iCfgOld  =  CFG_SDX;
        }
#endif
    else if ( mem_get_alloc () >= 4 )
        {
        iCfgOld  =  CFG_MTX512;
        }
    else
        {
        iCfgOld  =  CFG_MTX500;
        }
#ifdef HAVE_CFX2
    if ( cfg.bCFX2 ) iCfgOld = CFG_CFX2;
#endif
    iCfgCur  =  iCfgOld;
    iCfgSel  =  0;

    bCfgRemap = cfg.kbd_emu & KBDEMU_REMAP;
    bCfgNode = ( ( rom_enable & ROMEN_NODE ) && ( cfg.rom_fn[ROM_NODE] != NULL ) );
    bCfgSound = cfg.snd_emu & SNDEMU_PORTAUDIO;
    iSelKbdN = 0;

    bTapeOver = cfg.tape_overwrite;
    bTapeAudio = cfg.tape_disable;

    if ( psCfgTape != NULL )   free ((void *) psCfgTape);
    psCfgTape = NULL;
    if ( psTapeOut != NULL )   free ((void *) psTapeOut);
    psTapeOut = NULL;
    if ( bTapeAudio )
        {
        if (psTapeOut != NULL ) free ((void *) psTapeOut);
        psTapeOut = NULL;
        psTmp = tape_get_input ();
        if ( psTmp != NULL ) psCfgTape = estrdup (psTmp);
        else psCfgTape = estrdup ("<None>");
        psTmp = tape_get_output ();
        if ( psTmp != NULL ) psTapeOut = estrdup (psTmp);
        else psTapeOut = estrdup ("<None>");
        }
    else
        {
        if ( cfg.tape_fn != NULL )   psCfgTape   =  estrdup (cfg.tape_fn);
        else psCfgTape = estrdup ("<None>");
        }

#ifdef HAVE_SID
    iSidEmu = cfg.sid_emu;
#endif
    for ( iDrive = 0; iDrive < NUM_DRIVES; ++iDrive )
        {
        cfg_get_drive (iDrive, &psCfgDir[iDrive], &psCfgDrive[iDrive]);
        }
    if ( disk_dir != NULL )
        {
        for ( iDrive = 0; iDrive < NUM_DRIVES; ++iDrive )
            {
            if ( psCfgDir[iDrive] == NULL )   psCfgDir[iDrive]   =  estrdup (disk_dir);
            }
        }
    iCfgExit =  0; 
    bNoApply    =   FALSE;

    cfg_win  =  twin_create (cfg.mon_width_scale, cfg.mon_height_scale,
        "MEMU Configuration", NULL, NULL, twin_keypress, twin_keyrelease, FALSE);
    twin_csr_style (cfg_win, 0x20, 9);
    }

#ifdef ALT_SAVE_CFG
void ALT_SAVE_CFG (FILE *);
#endif

//  Save new configuration
static void cfg_save (const char *psConfig)
    {
    if ( psConfig != NULL )
        {
        const char *ps;
        int   rom;
        int iDisk;
        FILE *pfil  =  fopen (psConfig, "w");
        if ( pfil == NULL )  { config_term (); fatal ("Unable to write configuration file"); }
        fprintf (pfil, "-mem-blocks %d\n", mem_get_alloc ());
        for ( rom = 0; rom < 8; ++rom )
            {
            if ( cfg.rom_fn[rom] != NULL ) fprintf (pfil, "-rom%d \"%s\"\n", rom, PMapMapped (cfg.rom_fn[rom]));
            }
#ifdef HAVE_CFX2
        if ( cfg.rom_cfx2 != NULL )
            fprintf (pfil, "%s \"%s\"\n", ( cfg.bCFX2 ? "-cfx2" : "-no-cfx2" ), PMapMapped (cfg.rom_cfx2));
#endif
        if ( cfg.kbd_emu & KBDEMU_REMAP ) fprintf (pfil, "-kbd-remap\n");
        fprintf (pfil, "-kbd-country %d\n", ( cfg.kbd_emu & KBDEMU_COUNTRY ) >> 2);
        if ( cfg.vid_emu & VIDEMU_WIN )
            {
            fprintf (pfil, "-vid-win\n");
            if ( cfg.vid_emu & VIDEMU_WIN_MAX )
                {
                fprintf (pfil, "-vid-win-max\n");
                }
            else
                {
                int   scale = cfg.vid_height_scale;
                while ( --scale > 0 )
                    {
                    fprintf (pfil, "-vid-win-big\n");
                    }
                }
            if ( cfg.vid_emu & VIDEMU_WIN_HW_PALETTE ) fprintf (pfil, "-vid-win-hw-palette\n");
            if ( cfg.screen_refresh == 60 ) fprintf (pfil, "-vid-ntsc\n");
            }
        if ( cfg.snd_emu & SNDEMU_PORTAUDIO )
            {
            fprintf (pfil, "-snd-portaudio\n");
            // if ( cfg.latency != 0.0 )   fprintf (pfil, "-snd-latency %f\n", cfg.latency);
            }
        if ( cfg.mon_emu & MONEMU_WIN )
            {
            fprintf (pfil, "-mon-win\n");
            if ( cfg.mon_emu & MONEMU_WIN_MAX )
                {
                fprintf (pfil, "-mon-win-max\n");
                }
            else
                {
                int   scale = cfg.mon_height_scale;
                while ( --scale > 0 )
                    {
                    fprintf (pfil, "-mon-win-big\n");
                    }
                }
            if ( cfg.mon_emu & MONEMU_WIN_MONO ) fprintf (pfil, "-mon-win-mono\n");
            if ( ! (cfg.mon_emu & MONEMU_IGNORE_INIT) ) fprintf (pfil, "-mon-no-ignore-init\n");
            }
        else if ( cfg.mon_emu & MONEMU_WIN_MAX )
            {
            fprintf (pfil, "-mon-win-max\n");
            }
        else if ( cfg.mon_height_scale > 1 )
            {
            fprintf (pfil, "-mon-size %d\n", cfg.mon_height_scale);
            }
        if ( disk_dir != NULL ) fprintf (pfil, "-disk-dir \"%s\"\n", disk_dir);
        if ( cfg.fn_sdxfdc[0] != NULL ) fprintf (pfil, "-sdx-mfloppy \"%s\"\n", PMapMapped (cfg.fn_sdxfdc[0]));
        if ( cfg.fn_sdxfdc[1] != NULL ) fprintf (pfil, "-sdx-mfloppy2 \"%s\"\n", PMapMapped (cfg.fn_sdxfdc[1]));
#ifdef HAVE_SID
        if ( cfg.sid_emu & SIDEMU_HUGE ) fprintf (pfil, "-sidisc-huge\n");
        if ( cfg.sid_emu & SIDEMU_NO_SAVE ) fprintf (pfil, "-sidisc-no-save\n");
        for ( iDisk = 0; iDisk < N_SIDISC; ++iDisk )
            if ( cfg.sid_fn[iDisk] != NULL )
                fprintf (pfil, "-sidisc-file %d \"%s\"\n", iDisk, PMapMapped (cfg.sid_fn[iDisk]));
#endif
#ifdef HAVE_CFX2
        for ( iDisk = 0; iDisk < NCF_CARD; ++iDisk )
            {
            int iPart;
            for ( iPart = 0; iPart < NCF_PART; ++iPart )
                {
                if ( cfg.fn_cfx2[iDisk][iPart] != NULL )
                    fprintf (pfil, "-cf-image %d:%d \"%s\"\n", iDisk, iPart, PMapMapped (cfg.fn_cfx2[iDisk][iPart]));
                }
            }
#endif
        if ( cfg.tape_name_prefix != NULL ) fprintf (pfil, "-tape-dir \"%s\"\n", cfg.tape_name_prefix);
        if ( cfg.tape_overwrite ) fprintf (pfil, "-tape-overwrite\n");
        if ( cfg.tape_disable )
            {
            fprintf (pfil, "-tape-disable\n");
            if ( ( ps = tape_get_input () ) ) fprintf (pfil, "-cassette-in \"%s\"\n", ps);
            if ( ( ps = tape_get_output () ) ) fprintf (pfil, "-cassette-out \"%s\"\n", ps);
            }
        rom_enable   =  mem_get_rom_enable ();
        if ( rom_enable != 0xff )   fprintf (pfil, "-rom-enable 0x%02x\n", rom_enable);
        
#ifdef ALT_SAVE_CFG
        ALT_SAVE_CFG (pfil);
#endif
#if 0
        if ( cfg.tracks_sdxfdc[0] != 0 ) fprintf (pfil, "-sdx-tracks %d\n", cfg.tracks_sdxfdc[0]);
        if ( cfg.tracks_sdxfdc[1] != 0 ) fprintf (pfil, "-sdx-tracks2 %d\n", cfg.tracks_sdxfdc[1]);
#ifdef HAVE_JOY
        if ( cfg.joy_emu & JOYEMU_JOY )
            {
            fprintf (pfil, "-joy\n");
            if ( cfg.joy_central != 0 ) fprintf (pfil, "-joy-central %d\n", cfg.joy_central);
            if ( cfg.joy_buttons != NULL ) fprintf (pfil, "-joy-buttons \"%s\"\n", cfg.joy_buttons);
            }
#endif
        if ( cfg.bSerialDev[0] ) fprintf (pfil, "-serial1-dev \"%s\"\n", cfg.fn_serial_in[0]);
        else
            {
            if ( cfg.fn_serial_in[0] != NULL ) fprintf (pfil, "-serial1-in \"%s\"\n",
                cfg.fn_serial_in[0]);
            if ( cfg.fn_serial_out[0] != NULL ) fprintf (pfil, "-serial1-out \"%s\"\n",
                cfg.fn_serial_out[0]);
            }
        if ( cfg.bSerialDev[1] ) fprintf (pfil, "-serial2-dev \"%s\"\n", cfg.fn_serial_in[1]);
        else
            {
            if ( cfg.fn_serial_in[1] != NULL ) fprintf (pfil, "-serial2-in \"%s\"\n",
                cfg.fn_serial_in[1]);
            if ( cfg.fn_serial_out[1] != NULL ) fprintf (pfil, "-serial2-out \"%s\"\n",
                cfg.fn_serial_out[1]);
            }
#ifdef HAVE_GUI
        if ( ( ps = vid_get_title () ) )    fprintf (pfil, "-vid-win-title \"%s\"\n", ps);
        if ( ( ps = vid_get_display () ) )  fprintf (pfil, "-vid-win-display \"%s\"\n", ps);
        if ( ( ps = mon_get_title () ) )    fprintf (pfil, "-mon-win-title \"%s\"\n", ps);
        if ( ( ps = mon_get_display () ) )  fprintf (pfil, "-mon-win-display \"%s\"\n", ps);
#endif
#endif
        if ( ! cfg.tape_disable )
            {
            if ( ( ps = tape_get_input () ) ) fprintf (pfil, "\"%s\"\n", ps);
            }
        fclose (pfil);
        }
    }

//  Restart MEMU in new configuration
static void cfg_restart (void)
    {
    diag_message (DIAG_INIT, "cfg_restart: iCfgCur = %d, iCfgOld = %d", iCfgCur, iCfgOld);
    if ( iCfgCur != iCfgOld )
        {
#ifdef HAVE_CFX2
        if ( iCfgOld == CFG_CFX2 )
            {
            if ( cfg.rom_fn[4] != NULL )
                {
                diag_message (DIAG_INIT, "Installing SDX ROM");
                load_rom (4, cfg.rom_fn[4]);
                }
            if ( cfg.rom_fn[5] != NULL )
                {
                diag_message (DIAG_INIT, "Installing CP/M ROM");
                load_rom (5, cfg.rom_fn[5]);
                }
            cfg.bCFX2 = FALSE;
            cfx2_term ();
#ifdef HAVE_VGA
            cfg.bVGA = FALSE;
            vga_term ();
#endif
            }
#endif
        switch ( iCfgCur )
            {
            case  CFG_MTX500:
            {
            if ( cfg.mon_emu & ( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE ) )
                {
                mon_term ();
                cfg.mon_emu &= ~( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE );
                // cfg_set_display ();
                }
            mem_alloc (2);
            rom_enable = ROMEN_ASSEM | ROMEN_BASIC;
            mem_set_rom_enable (rom_enable);
            break;
            }
            case  CFG_MTX512:
            {
            if ( cfg.mon_emu & ( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE ) )
                {
                mon_term ();
                cfg.mon_emu &= ~( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE );
                // cfg_set_display ();
                }
            mem_alloc (4);
            rom_enable = ROMEN_ASSEM | ROMEN_BASIC;
            mem_set_rom_enable (rom_enable);
            break;
            }
            case  CFG_SDX:
            {
            if ( cfg.mon_emu & ( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE ) )
                {
                mon_term ();
                cfg.mon_emu &= ~( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE );
                // cfg_set_display ();
                }
            mem_alloc (SDX_MEM_BLOCKS);
            rom_enable = ROMEN_ASSEM | ROMEN_BASIC | ROMEN_SDX2;
#ifndef SMALL_MEM
            if ( cfg.rom_fn[ROM_SDX2] == NULL ) { config_term (); fatal ("SDX ROM not installed"); }
#endif
            mem_set_rom_enable (rom_enable);
            break;
            }
            case  CFG_CPM_MONO:
            {
            mem_alloc (CPM_MEM_BLOCKS);
            rom_enable = ROMEN_ASSEM | ROMEN_BASIC | ROMEN_SDX2 | ROMEN_CPM;
#ifndef SMALL_MEM
            if ( cfg.rom_fn[ROM_SDX2] == NULL ) { config_term (); fatal ("SDX ROM not installed"); }
            if ( cfg.rom_fn[ROM_CPM] == NULL ) { config_term (); fatal ("CP/M ROM not installed"); }
#endif
            mem_set_rom_enable (rom_enable);
            if ( cfg.mon_emu != ( MONEMU_WIN | MONEMU_IGNORE_INIT | MONEMU_WIN_MONO ) )
                {
                if ( cfg.mon_emu & ( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE ) ) mon_term ();
                cfg.mon_emu = MONEMU_WIN | MONEMU_IGNORE_INIT | MONEMU_WIN_MONO;
                mon_init (cfg.mon_emu, cfg.mon_width_scale, cfg.mon_height_scale);
                // cfg_set_display ();
                }
            break;
            }
            case  CFG_CPM_COLOUR:
            {
            mem_alloc (CPM_MEM_BLOCKS);
            rom_enable = ROMEN_ASSEM | ROMEN_BASIC | ROMEN_SDX2 | ROMEN_CPM;
#ifndef SMALL_MEM
            if ( cfg.rom_fn[ROM_SDX2] == NULL ) { config_term (); fatal ("SDX ROM not installed"); }
            if ( cfg.rom_fn[ROM_CPM] == NULL ) { config_term (); fatal ("CP/M ROM not installed"); }
#endif
            mem_set_rom_enable (rom_enable);
            if ( cfg.mon_emu != ( MONEMU_WIN | MONEMU_IGNORE_INIT ) )
                {
                if ( cfg.mon_emu & ( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE ) ) mon_term ();
                cfg.mon_emu = MONEMU_WIN | MONEMU_IGNORE_INIT;
                mon_init (cfg.mon_emu, cfg.mon_width_scale, cfg.mon_height_scale);
                // cfg_set_display ();
                }
            break;
            }
#ifdef HAVE_CFX2
            case CFG_CFX2:
            {
            if ( cfg.mon_emu & ( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE ) )
                {
                mon_term ();
                cfg.mon_emu &= ~( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE );
                // cfg_set_display ();
                }
            /*
            if ( cfg.mon_emu != ( MONEMU_WIN | MONEMU_IGNORE_INIT ) )
                {
                if ( cfg.mon_emu & ( MONEMU_WIN | MONEMU_TH | MONEMU_CONSOLE ) ) mon_term ();
                cfg.mon_emu = MONEMU_WIN | MONEMU_IGNORE_INIT;
                mon_init (cfg.mon_emu, cfg.mon_width_scale, cfg.mon_height_scale);
                }
            */
            mem_alloc (CPM_MEM_BLOCKS);
            if ( cfg.rom_cfx2 == NULL ) { config_term (); fatal ("CFX-II ROM not installed"); }
            diag_message (DIAG_INIT, "Installing CFX ROM");
            load_rompair (4, cfg.rom_cfx2);
            rom_enable = ROMEN_ASSEM | ROMEN_BASIC | ROMEN_SDX2 | ROMEN_CPM;
            mem_set_rom_enable (rom_enable);
            cfg.bCFX2 = TRUE;
#ifdef HAVE_VGA
            cfg.bVGA = TRUE;
            vga_init ();
#endif
            break;
            }
#endif
            }
        }
    if ( ( bCfgSound ) && ( ! ( cfg.snd_emu & SNDEMU_PORTAUDIO ) ) )
        {
        cfg.snd_emu |= SNDEMU_PORTAUDIO;
        snd_init (cfg.snd_emu, cfg.latency);
        }
    else if ( ( ! bCfgSound ) && ( cfg.snd_emu & SNDEMU_PORTAUDIO ) )
        {
        snd_term ();
        cfg.snd_emu &= ~SNDEMU_PORTAUDIO;
        }
    if ( bCfgNode ) rom_enable |= ROMEN_NODE;
    else            rom_enable &= ~ROMEN_NODE;
    mem_set_rom_enable (rom_enable);
    memu_reset ();
    }

//  Display configuration exit options
static void row_exit_draw (int info, int iState)
    {
    static char *psExit   =  "  Apply   Cancel   Restart   Exit   ";
    static const char *psHelp[EXIT_COUNT]   =
        {
        "<Space> to select: Change disk and/or tape and continue MEMU",
        "<Space> to select: Continue MEMU with no changes",
        "<Space> to select: Make changes and restart MEMU",
        "<Space> to select: Exit MEMU"
        };
    twin_print (cfg_win, ROW_EXIT, 0, STY_NORMAL, psExit, 0);
    if ( ( iCfgCur != iCfgOld ) || bNoApply )
        {
        twin_print (cfg_win, ROW_EXIT, 0, STY_DISABLED, psExit, 9);
        }
    if ( iState == STATE_FOCUS )
        {
        if ( ( ( iCfgCur != iCfgOld ) || bNoApply ) && ( iCfgExit == EXIT_APPLY ) )
            iCfgExit =  EXIT_RESTART;
        twin_print (cfg_win, ROW_EXIT, 9*iCfgExit, STY_HIGHLIGHT, &psExit[9*iCfgExit], 9);
        cfg_help (psHelp[iCfgExit]);
        }
    }

//  Exit configuration key processing
static BOOLEAN row_exit_key (int info, int wk)
    {
    switch (wk)
        {
        case  WK_Left:
        {
        --iCfgExit;
        if ( ( ( iCfgCur != iCfgOld ) || bNoApply ) && ( iCfgExit == EXIT_APPLY ) )
            --iCfgExit;
        if ( iCfgExit < 0 )  iCfgExit =  EXIT_COUNT - 1;
        break;
        }
        case  WK_Right:
        {
        ++iCfgExit;
        if ( iCfgExit >= EXIT_COUNT ) iCfgExit =  0;
        if ( ( ( iCfgCur != iCfgOld ) || bNoApply ) && ( iCfgExit == EXIT_APPLY ) )
            ++iCfgExit;
        break;
        }
        case  WK_Return:
        case  ' ':
        {
        if ( iCfgExit == EXIT_RESTART )
            {
            cfg_restart ();
            }
        if ( iCfgExit != EXIT_CANCEL )
            {
            int iDrive;
            cfg.tape_disable = bTapeAudio;
            tape_patch (! bTapeAudio);
            if ( bTapeAudio )
                {
                tape_set_input (cfg_def_file (psCfgTape));
                tape_set_output (cfg_def_file (psTapeOut));
                }
            else
                {
                cfg_set_tape (cfg_def_file (psCfgTape));
                }
            ALERT_ON();
            for ( iDrive = 0; iDrive < NUM_DRIVES; ++iDrive )
                {
                cfg_save_drive (iDrive, psCfgDir[iDrive], psCfgDrive[iDrive]);
                }
#ifdef HAVE_SID
            cfg.sid_emu = iSidEmu;
            sid_mode (iSidEmu);
#endif
            for ( iDrive = 0; iDrive < NUM_DRIVES; ++iDrive )
                {
                cfg_set_drive (iDrive, psCfgDir[iDrive], psCfgDrive[iDrive]);
                }
            ALERT_OFF();
#ifdef HAVE_CFX2
            if ( cfg.bCFX2 ) cfx2_init ();
#endif
            diag_message (DIAG_INIT, "After cfx2_init");
            if ( bCfgRemap )
                {
                cfg.kbd_emu   |= KBDEMU_REMAP;
                kbd_init (cfg.kbd_emu);
                }
            else
                {
                cfg.kbd_emu   &= ~KBDEMU_REMAP;
                kbd_init (cfg.kbd_emu);
                }
            diag_message (DIAG_INIT, "After keyboard remap");
            cfg_save (config_fn);
            diag_message (DIAG_INIT, "After save config");
            }
        if ( iCfgExit == EXIT_EXIT )
            {
            diag_message (DIAG_INIT, "Close config window");
            config_term ();
            diag_message (DIAG_INIT, "Terminate");
            terminate ("User exit");
            }
        bCfgExit =  TRUE;
        break;
        }
        default:
        {
        return   FALSE;
        }
        }
    return   TRUE;
    }

static BOOLEAN row_focus_true (int info)
    {
    return   TRUE;
    }

static BOOLEAN row_focus_drive (int iDrive)
    {
//    if ( ( iCfgCur == CFG_CPM_MONO ) || ( iCfgCur == CFG_CPM_COLOUR ) ) return  TRUE;
    if ( iCfgCur >= CFG_CPM_MONO ) return  TRUE;
    if ( ( iCfgCur == CFG_SDX ) && ( iDrive < NUM_FLOPPY ) )    return  TRUE;
    return  FALSE;
    }

static BOOLEAN row_focus_tout (int info)
    {
    return bTapeAudio;
    }

struct  s_cfg_ui
    {
    void (*draw) (int,int);
    BOOLEAN (*key) (int,int);
    BOOLEAN (*focus) (int);
    int info;
    }
    cfg_ui[] =
        {
        { row_cfg_draw, row_cfg_key, row_focus_true, 0 },
        { row_kbd_draw, row_kbd_key, row_focus_true, 0 },
        { row_topt_draw, row_topt_key, row_focus_true, 0 },
        { row_tape_draw, row_tape_key, row_focus_true, 0 },
        { row_tout_draw, row_tout_key, row_focus_tout, 0 },
#ifdef HAVE_SID
        { row_dropt_draw, row_dropt_key, row_focus_drive, 2 },
#endif
        { row_drive_draw, row_drive_key, row_focus_drive, 0 },
#if NUM_DRIVES > 1
        { row_drive_draw, row_drive_key, row_focus_drive, 1 },
#endif
#if NUM_DRIVES > 2
        { row_drive_draw, row_drive_key, row_focus_drive, 2 },
#endif
#if NUM_DRIVES > 3
        { row_drive_draw, row_drive_key, row_focus_drive, 3 },
#endif
#if NUM_DRIVES > 4
        { row_drive_draw, row_drive_key, row_focus_drive, 4 },
#endif
#if NUM_DRIVES > 5
        { row_drive_draw, row_drive_key, row_focus_drive, 5 },
#endif
        { row_exit_draw, row_exit_key, row_focus_true, 0 }
        };

void config (void)
    {
    int  nRow   =  sizeof (cfg_ui) / sizeof (cfg_ui[0]);
    int  iFocus =  0;
    int  wk, i;
    cfg_init ();
    bCfgExit =  FALSE;
    bCfgRedraw  =  TRUE;
    while ( ! bCfgExit )
        {
        if ( bCfgRedraw )
            {
            // printf ("Redraw config screen\n");
            for ( i = 0; i < nRow; ++i )
                {
                cfg_ui[i].draw (cfg_ui[i].info,
                    cfg_ui[i].focus (cfg_ui[i].info) ? STATE_NORMAL : STATE_DISABLED);
                }
            bCfgRedraw  =  FALSE;
            }
        cfg_ui[iFocus].draw (cfg_ui[iFocus].info, STATE_FOCUS);
        wk =  twin_kbd_in (cfg_win);
        // printf ("cfg_key = %d\n", wk);
        cfg_ui[iFocus].draw (cfg_ui[iFocus].info, STATE_NORMAL);
        if ( ! cfg_ui[iFocus].key (cfg_ui[iFocus].info, wk) )
            {
            if ( wk == WK_Down )
                {
                do
                    {
                    ++iFocus;
                    if ( iFocus >= nRow )   iFocus   =  0;
                    }
                while ( ! cfg_ui[iFocus].focus (cfg_ui[iFocus].info) );
                }
            else if ( wk == WK_Up )
                {
                do
                    {
                    --iFocus;
                    if ( iFocus < 0 ) iFocus   =  nRow - 1;
                    }
                while ( ! cfg_ui[iFocus].focus (cfg_ui[iFocus].info) );
                }
            }
        }
    diag_message (DIAG_INIT, "Before config_term");
    config_term ();
    diag_message (DIAG_INIT, "After config_term");
    }

//  Read configuration file and generate argument list
BOOLEAN read_config (const char *psFile, int *pargc, const char ***pargv, int *pi)
    {
    FILE *pfil;
    char **argv;
    char *psArg;
    char chQuo;
    int  iarg, narg;
    int  ich, nch;
    BOOLEAN  bFirst;
    BOOLEAN  bQuote;
    /*
    printf ("Before read_config: argc = %d, i = %d\n", *pargc, *pi);
    for (int i = 0; i < *pargc; ++i)
        {
        printf ("%s\n", (*pargv)[i]);
        }
    */
    psFile = PMapPath (psFile);
    char *psDir = strdup (psFile);
    if ( psDir != NULL )
        {
        char *psDEnd = strrchr (psDir, '/');
#ifdef WIN32
        char *psDEn2 = strrchr (psDir, '\\');
        if ( psDEn2 >= psDEnd ) psDEnd = psDEn2;
#endif
        if ( psDEnd != NULL )
            {
            *psDEnd = '\0';
            PMapRootDir (pmapCfg, psDir, TRUE);
            }
        else
            {
            PMapRootDir (pmapCfg, ".", TRUE);
            }
        free (psDir);
        }
    
    diag_message (DIAG_INIT, "Configuration file: %s", psFile);
    pfil  =  fopen (psFile, "rb");
    if ( pfil == NULL )
        {
        diag_message (DIAG_INIT, "Failed to open Configuration file: %s", psFile);
        return   FALSE;
        }
    fseek (pfil, 0, SEEK_END);
    nch  =   ftell (pfil);
    fseek (pfil, 0, SEEK_SET);
    diag_message (DIAG_INIT, "Configuration file size: %d bytes.", nch);
    psArg =  malloc (nch+1);
    if ( psArg == NULL )
        {
        fclose (pfil);
        diag_message (DIAG_INIT, "Failed to allocate memory for configuration string");
        return   FALSE;
        }
    if ( fread (psArg, 1, nch, pfil) != nch )
        {
        free ((void *) psArg);
        fclose (pfil);
        diag_message (DIAG_INIT, "Failed to read expected number of bytes from configuration file");
        return   FALSE;
        }
    fclose (pfil);
    psArg[nch]  =  '\0';
    narg     =  0;
    bFirst   =  TRUE;
    bQuote   =  FALSE;
    for ( ich = 0; ich < nch; ++ich )
        {
        if ( ( ! bQuote ) && ( psArg[ich] <= ' ' ) )
            {
            bFirst   =  TRUE;
            }
        else if ( ( bQuote ) && ( psArg[ich] == chQuo ) )
            {
            bQuote   =  FALSE;
            bFirst   =  TRUE;
            }
        else if ( bFirst )
            {
            ++narg;
            bFirst   =  FALSE;
            if ( ( psArg[ich] == '"' ) || ( psArg[ich] == '\'' ) )
                {
                bQuote   =  TRUE;
                chQuo    =  psArg[ich];
                }
            }
        }
    diag_message (DIAG_INIT, "Number of new arguments = %d", narg);
    if ( narg == 0 )  return   FALSE;
    argv   =  (char **) malloc ((narg+*pargc)*sizeof (char *));
    if ( argv == NULL )
        {
        free ((void *) psArg);
        return   FALSE;
        }
    memcpy (argv, *pargv, (*pi + 1) * sizeof (char *));
    iarg =   *pi + 1;
    bFirst   =  TRUE;
    bQuote   =  FALSE;
    for ( ich = 0; ich < nch; ++ich )
        {
        if ( ( ! bQuote ) && ( psArg[ich] <= ' ' ) )
            {
            psArg[ich]  =  '\0';
            bFirst   =  TRUE;
            }
        else if ( ( bQuote ) && ( psArg[ich] == chQuo ) )
            {
            psArg[ich]  =  '\0';
            bQuote   =  FALSE;
            bFirst   =  TRUE;
            }
        else if ( bFirst )
            {
            argv[iarg]   =  &psArg[ich];
            bFirst   =  FALSE;
            if ( ( psArg[ich] == '"' ) || ( psArg[ich] == '\'' ) )
                {
                argv[iarg]   =  &psArg[ich+1];
                bQuote   =  TRUE;
                chQuo    =  psArg[ich];
                }
            ++iarg;
            }
        }
    if ( *pi < *pargc - 1 ) memcpy (&argv[iarg], *pargv + *pi + 1, (*pargc - *pi - 1) * sizeof (char *));
    *pargc   += narg;
    *pargv   =  (const char ** ) argv;
    diag_message (DIAG_INIT, "Total arguments = %d", *pargc);
    /*
    printf ("After read_config: argc = %d, i = %d\n", *pargc, *pi);
    for (int i = 0; i < *pargc; ++i)
        {
        printf ("%s\n", (*pargv)[i]);
        }
    */
    return   TRUE;
    }

//  Process additional command line options
BOOLEAN cfg_options (int *pargc, const char ***pargv, int *pi)
    {
    if ( !strcmp((*pargv)[*pi], "-config-file") )
        {
#ifdef HAVE_OSFS
        cpm_allow_open_hack(TRUE);
#endif
        diag_flags[DIAG_BAD_PORT_IGNORE] = TRUE;
        if ( ++(*pi) == (*pargc) )
            usage((*pargv)[*pi-1]);
        config_fn   =  (*pargv)[*pi];
        read_config (config_fn, pargc, pargv, pi);
        }
    else if ( !strcmp((*pargv)[*pi], "-no-ignore-faults") )
        {
#ifdef HAVE_OSFS
        cpm_allow_open_hack(FALSE);
#endif
        diag_flags[DIAG_BAD_PORT_IGNORE] = FALSE;
        }
    else if ( !strcmp((*pargv)[*pi], "-rom-enable") )
        {
        if ( ++(*pi) == (*pargc) )
            usage((*pargv)[*pi-1]);
        sscanf ((*pargv)[*pi], "%i", &rom_enable);
        mem_set_rom_enable (rom_enable);
        }
    else if ( !strcmp ((*pargv)[*pi], "-no-cfx2") )
        {
#ifdef HAVE_CFX2
        if ( ++(*pi) == (*pargc) )
            usage((*pargv)[*pi-1]);
        cfg.rom_cfx2 = (*pargv)[*pi];
#else
        unimplemented ((*pargv)[*pi]);
        ++(*pi);
#endif
        }
    else if ( !strcmp((*pargv)[*pi], "-mon-size") )
        {
        if ( ++(*pi) == (*pargc) )
            usage((*pargv)[*pi-1]);
        sscanf((*pargv)[*pi], "%i", &cfg.mon_height_scale);
        cfg.mon_width_scale = cfg.mon_height_scale/2;
        }
    else if ( !strcmp((*pargv)[*pi], "-disk-dir") )
        {
        if ( ++(*pi) == (*pargc) )
            usage((*pargv)[*pi-1]);
        disk_dir =  (*pargv)[*pi];
        }
    else
        {
        return FALSE;
        }
    return TRUE;
    }

//  Display additional command line options
void cfg_usage (void)
    {
    fprintf(stderr, "       -config-file file    read configuration options from file\n");
    fprintf(stderr, "       -no-ignore-faults    turn off permissive options enabled by -config-file\n");
    fprintf(stderr, "       -rom-enable rom_bits bit flags to enable (1) or disable (0) a rom\n");
#ifdef HAVE_CFX2
    fprintf(stderr, "       -no-cfx2 rom_file    disable CFX-II emulation but specify ROM image file\n");
#endif
    fprintf(stderr, "       -mon-size            sets 80 col size (but does not enable it)\n");
    fprintf(stderr, "       -disk-dir dir        directory containg disk images\n");
    }

#ifndef __Pico__
WIN * get_cfg_win (void)
    {
    return cfg_win;
    }
#endif
