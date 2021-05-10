/*

  win.c - Microsoft Windows Window

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>

#define BOOLEAN BOOLEANx
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellscalingapi.h>
#undef BOOLEAN

#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...e*/

/*...sWIN_PRIV:0:*/
typedef struct
    {
    int width, height;
    int width_scale, height_scale;
    int n_cols;
    byte *data;
    void (*keypress)(int);
    void (*keyrelease)(int);
    /* Private window data below - Above must match definition of WIN in win.h */
    HANDLE hmutex;        /* Control access to an instance of this class */
    HANDLE heventCreated; /* Signalled when window initialised etc. */
    HANDLE heventUpdated; /* Signalled when update window done */
    HANDLE heventDeleted; /* Signalled when window destroyed */
    HWND hwnd;
    BITMAPINFOHEADER bmih;
    RGBQUAD rgbqPalette[N_COLS_MAX];
    HBITMAP hbitmap;
    HPALETTE hpalette;
    char title[200+1];
    COL cols[N_COLS_MAX];
    int iXFrame, iYFrame;
    } WIN_PRIV;
/*...e*/

#define LEN_KEYBUF 128
#define WK_Release 0x8000

static int iXBorder = 0;
static int iYBorder = 0;

typedef struct
    {
    WIN_PRIV *win;
    int wk;
    } KEYEVT;
static KEYEVT keybuf[LEN_KEYBUF];
static int iKBIn = 0;
static int iKBOut = 0;
static HANDLE hKBmutex = NULL;

static BOOLEAN win_inited = FALSE;
static HINSTANCE hinst;
// static int cxScreen, cyScreen, cyCaption, cxFixedFrame, cyFixedFrame;
static int fPalettised;

static BOOLEAN terminated = FALSE;

/*...swin_map_key:0:*/
typedef struct
    {
    WORD vk;
    int wk;
    } MAPVK;

static MAPVK win_mapvk[] =
    {
    {VK_BACK        ,WK_BackSpace},
    {VK_TAB         ,WK_Tab},
#if 0
    {VK_Linefeed        ,WK_Linefeed},
#endif
    {VK_RETURN      ,WK_Return},
    {VK_ESCAPE      ,WK_Escape},
    {VK_LEFT        ,WK_Left},
    {VK_RIGHT       ,WK_Right},
    {VK_UP          ,WK_Up},
    {VK_DOWN        ,WK_Down},
    {VK_PRIOR       ,WK_Page_Up},
    {VK_NEXT        ,WK_Page_Down},
    {VK_HOME        ,WK_Home},
    {VK_END         ,WK_End},
    {VK_INSERT      ,WK_Insert},
    {VK_DELETE      ,WK_Delete},
    {VK_PAUSE       ,WK_Pause},
    {VK_SCROLL      ,WK_Scroll_Lock},
    {VK_SNAPSHOT        ,WK_Sys_Req},
    {VK_SHIFT       ,WK_Shift_L},
#if 0       
    {VK_APPS        ,WK_Shift_R},
#else       
    {VK_APPS        ,WK_PC_Menu},
#endif      
    {VK_CONTROL     ,WK_Control_L},
    {VK_CAPITAL     ,WK_Caps_Lock},
#if 0
    {VK_Shift_Lock      ,WK_Shift_Lock},
#endif
    {VK_NUMLOCK     ,WK_Num_Lock},
    {VK_F1          ,WK_F1},
    {VK_F2          ,WK_F2},
    {VK_F3          ,WK_F3},
    {VK_F4          ,WK_F4},
    {VK_F5          ,WK_F5},
    {VK_F6          ,WK_F6},
    {VK_F7          ,WK_F7},
    {VK_F8          ,WK_F8},
    {VK_F9          ,WK_F9},
    {VK_F10         ,WK_F10},
    {VK_F11         ,WK_F11},
    {VK_F12         ,WK_F12},
    {VK_NUMPAD4     ,WK_KP_Left},
    {VK_NUMPAD6     ,WK_KP_Right},
    {VK_NUMPAD8     ,WK_KP_Up},
    {VK_NUMPAD2     ,WK_KP_Down},
    {VK_NUMPAD9     ,WK_KP_Page_Up},
    {VK_NUMPAD3     ,WK_KP_Page_Down},
    {VK_NUMPAD7     ,WK_KP_Home},
    {VK_NUMPAD1     ,WK_KP_End},
    {VK_ADD         ,WK_KP_Add},
    {VK_SUBTRACT        ,WK_KP_Subtract},
    {VK_MULTIPLY        ,WK_KP_Multiply},
    {VK_DIVIDE      ,WK_KP_Divide},
#if 0
    {VK_KP_Enter        ,WK_KP_Enter},
#endif
    {VK_NUMPAD5     ,WK_KP_Middle},

    {VK_OEM_1       ,';'},      /* ;: */
    {VK_OEM_PLUS        ,'='},      /* =+ */
    {VK_OEM_COMMA       ,','},      /* ,< */
    {VK_OEM_MINUS       ,'-'},      /* -_ */
    {VK_OEM_PERIOD      ,'.'},      /* .> */
    {VK_OEM_2       ,'/'},      /* /? */
    {VK_OEM_3       ,'\''},     /* '@ */
    {VK_OEM_4       ,'['},      /* [{ */
    {VK_OEM_5       ,'\\'},     /* \| */
    {VK_OEM_6       ,']'},      /* ]} */
    {VK_OEM_7       ,'#'},      /* #~ */
    {VK_OEM_8       ,'`'},      /* `not */
    {VK_MENU        ,WK_Menu},
    };

#define SH_LEFT   0x01
#define SH_RIGHT  0x02

static int win_map_key(WPARAM vk, LPARAM lParam, BOOLEAN bPress)
    {
    static int iSSNow = 0;
    int i;
    if ( vk >= 'A' && vk <= 'Z' )
        return (int) (vk-'A'+'a');
    if ( vk == VK_MENU )
        {
        if ( lParam & ( 1 << 24 ) ) return WK_PC_Alt_R;
        return WK_PC_Alt_L;
        }
    if ( vk == VK_CONTROL )
        {
        if ( lParam & ( 1 << 24 ) ) return WK_Control_R;
        return WK_Control_L;
        }
    if ( vk == VK_SHIFT )
        {
        int iScan = ( lParam >> 16 ) & 0xFF;
        int iSSPrev = iSSNow;
        int iSSChg = 0;
        iSSNow = 0;
        // Current state of shift keys
        if ( GetKeyState (VK_LSHIFT) & 0x80 ) iSSNow |= SH_LEFT;
        if ( GetKeyState (VK_RSHIFT) & 0x80 ) iSSNow |= SH_RIGHT;
        diag_message (DIAG_KBD_WIN_KEY, "Shift key: iScan = 0x%02X, iSSNow = 0x%X", iScan, iSSNow);
        // Try using PS/2 scan code set 1 - Seems to work for USB keyboards as well
        if ( iScan == 0x2A ) return WK_Shift_L;
        if ( iScan == 0x36 ) return WK_Shift_R;
        // Change in shift status
        iSSChg = iSSPrev ^ iSSNow;
        if ( iSSChg & SH_LEFT )
            {
            if ( bPress )
                {
                if ( iSSNow & SH_LEFT ) return WK_Shift_L;
                }
            else
                {
                if ( ( iSSNow & SH_LEFT ) == 0 ) return WK_Shift_L;
                }
            }
        if ( iSSChg & SH_RIGHT )
            {
            if ( bPress )
                {
                if ( iSSNow & SH_RIGHT ) return WK_Shift_R;
                }
            else
                {
                if ( ( iSSNow & SH_RIGHT ) == 0 ) return WK_Shift_R;
                }
            }
        }
    for ( i = 0; i < sizeof(win_mapvk)/sizeof(win_mapvk[0]); i++ )
        if ( vk == win_mapvk[i].vk )
            return win_mapvk[i].wk;
    if ( vk >= ' ' && vk <= '~' )
        return (int) vk;
    diag_message(DIAG_WIN_UNKNOWN_KEY, "win_map_key can't map vk=0x%04x", vk);
    return -1;
    }
/*...e*/

static void win_add_key_event (WIN_PRIV *win, int wk)
    {
    int iPtr;
    WaitForSingleObject(hKBmutex, INFINITE);
    iPtr = iKBIn + 1;
    if ( iPtr >= LEN_KEYBUF ) iPtr = 0;
    if ( iPtr != iKBOut )
        {
        keybuf[iKBIn].win = win;
        keybuf[iKBIn].wk = wk;
        iKBIn = iPtr;
        }
    ReleaseMutex(hKBmutex);
    }

#define WC_MEMUWIN "MemuWindow"

/*...sMemuWindowProc:0:*/
#define WM_UPDATEWINDOW  WM_USER
#define WM_DELETEWINDOW (WM_USER+1)

/*...sMakePalette:0:*/
static HPALETTE MakePalette(COL *cols, int n_cols)
    {
    LOGPALETTE *pal = (LOGPALETTE *) emalloc(sizeof(LOGPALETTE)+n_cols*sizeof(PALETTEENTRY));
    int i;
    HPALETTE hpal;
    pal->palNumEntries = n_cols;
    pal->palVersion = 0x300;
    for ( i = 0; i < n_cols; i++ )
        {
        pal->palPalEntry[i].peRed   = (BYTE) cols[i].r;
        pal->palPalEntry[i].peGreen = (BYTE) cols[i].g;
        pal->palPalEntry[i].peBlue  = (BYTE) cols[i].b;
        pal->palPalEntry[i].peFlags = (BYTE) 0;
        }
    hpal = CreatePalette(pal);
    free(pal);
    return hpal;
    }
/*...e*/
/*...sRealizeOurPalette:0:*/
static void RealizeOurPalette(HWND hwnd, HPALETTE hpalette, BOOL bForceBackground)
    {
    HDC hdc = GetWindowDC(hwnd);
    HPALETTE hpaletteOld = SelectPalette(hdc, hpalette, FALSE);
    UINT nChanged = RealizePalette(hdc);
    SelectPalette(hdc, hpaletteOld, TRUE);
    ReleaseDC(hwnd, hdc);
    if ( nChanged != 0 )
        /* Palette changes occured, we should repaint all */
        /* so as to get the best rendition to the new palette */
        InvalidateRect(hwnd, NULL, TRUE);
    }
/*...e*/

void GuessBorders (void)
    {
    if ( iXBorder == 0 )
        {
        // These values appear to be an under-estimate on Windows 10
        iXBorder = 2 * GetSystemMetrics (SM_CXFIXEDFRAME);
        iYBorder = 2 * GetSystemMetrics (SM_CYFIXEDFRAME) + GetSystemMetrics (SM_CYCAPTION);
        }
    }

// was FAR PASCAL
LRESULT CALLBACK PASCAL MemuWindowProc(
    HWND hwnd,
    unsigned message,
    WPARAM wParam,
    LPARAM lParam
    )
    {
    switch ( message )
        {
        case WM_SIZE:
        {
        // Adjust window size for actual borders if necessary
        WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
        // printf ("WM_SIZE = %d x %d\n", LOWORD (lParam), HIWORD (lParam));
        if ( ( LOWORD (lParam) != win->width * win->width_scale )
            || ( HIWORD (lParam) != win->height * win->height_scale ) )
            {
            iXBorder = win->iXFrame - LOWORD (lParam);
            iYBorder = win->iYFrame - HIWORD (lParam);
            win->iXFrame = win->width * win->width_scale + iXBorder;
            win->iYFrame = win->height * win->height_scale + iYBorder;
            SetWindowPos (hwnd, HWND_TOP, 0, 0, win->iXFrame, win->iYFrame,
                SWP_NOMOVE | SWP_NOZORDER);
            }
        break;
        }
/*...sWM_CREATE          \45\ create:16:*/
        case WM_CREATE:
        {
        CREATESTRUCT *cs = (CREATESTRUCT *) lParam;
        WIN_PRIV *win = (WIN_PRIV *) cs->lpCreateParams;
        HDC hdcScreen;
        int i;
        SetWindowLongPtr(hwnd, 0, (LONG_PTR) win);
        memset(&(win->bmih), 0, sizeof(win->bmih));
        win->bmih.biSize         = sizeof(win->bmih);
        win->bmih.biWidth        = win->width;
        win->bmih.biHeight       = -(win->height); /* -ve => Top down */
        win->bmih.biPlanes       = 1;
        win->bmih.biBitCount     = 8;
        win->bmih.biCompression  = BI_RGB;
        win->bmih.biClrUsed      = win->n_cols;
        win->bmih.biClrImportant = win->n_cols;
        for ( i = 0; i < win->n_cols; i++ )
            {
            win->rgbqPalette[i].rgbRed      = (BYTE) win->cols[i].r;
            win->rgbqPalette[i].rgbGreen    = (BYTE) win->cols[i].g;
            win->rgbqPalette[i].rgbBlue     = (BYTE) win->cols[i].b;
            win->rgbqPalette[i].rgbReserved = 0;
            }
        if ( fPalettised )
            win->hpalette = MakePalette(win->cols, win->n_cols);
        else
            win->hpalette = (HPALETTE) NULL;
        hdcScreen = GetWindowDC((HWND) NULL);
        win->hbitmap = CreateDIBSection(
            hdcScreen,
            (BITMAPINFO *) &(win->bmih),
            DIB_RGB_COLORS,
            NULL,
            (HANDLE) NULL,
            0);
        ReleaseDC((HWND) NULL, hdcScreen);
        }
        return 0;
/*...e*/
/*...sWM_PAINT           \45\ paint client:16:*/
        case WM_PAINT:
        {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcClient;
        WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
        HDC hdcCompatible;
        HBITMAP hbitmapOld;
        HPALETTE hpaletteOld;
        int cxClient, cyClient;

        GetClientRect(hwnd, &rcClient);
        cxClient = rcClient.right  - rcClient.left;
        cyClient = rcClient.bottom - rcClient.top ;

        WaitForSingleObject(win->hmutex, INFINITE);

        hdcCompatible = CreateCompatibleDC(hdc);
        hbitmapOld = (HBITMAP) SelectObject(hdcCompatible, win->hbitmap);

        if ( win->hpalette != (HPALETTE) NULL )
            hpaletteOld = SelectPalette(hdc, win->hpalette, FALSE);

        win->bmih.biHeight = -(win->height); /* -ve => Top down */
        SetDIBits(hdcCompatible, win->hbitmap,
            0, win->height, win->data,
            (CONST BITMAPINFO *) &(win->bmih), DIB_RGB_COLORS);

        if ( win->width_scale == 1 && win->height_scale == 1 )
            BitBlt(hdc, 0, 0, win->width, win->height,
                hdcCompatible, 0, 0, SRCCOPY);
        else
            StretchBlt(hdc, 0, 0, win->width*win->width_scale, win->height*win->height_scale,
                hdcCompatible, 0, 0, win->width, win->height, SRCCOPY);

        if ( win->hpalette != (HPALETTE) NULL )
            SelectPalette(hdc, hpaletteOld, TRUE);

        SelectObject(hdcCompatible, hbitmapOld);
        DeleteDC(hdcCompatible);

        ReleaseMutex(win->hmutex);

        EndPaint(hwnd, &ps);
        }
        return 0;
/*...e*/
/*...sWM_QUERYNEWPALETTE \45\ got focus\44\ set our palette:16:*/
        case WM_QUERYNEWPALETTE:
            /* We've got input focus, set our choice of palette */
        {
        WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
        if ( win->hpalette != (HPALETTE) NULL )
            RealizeOurPalette(hwnd, win->hpalette, FALSE);
        }
        return 0;
/*...e*/
/*...sWM_PALETTECHANGED  \45\ other changed palette:16:*/
        case WM_PALETTECHANGED:
            if ( (HWND) wParam != hwnd )
                /* Someone else got palette
                   Aquire what entries we can */
                {
                WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
                if ( win->hpalette != (HPALETTE) NULL )
                    RealizeOurPalette(hwnd, win->hpalette, FALSE);
                }
            return 0;
/*...e*/
/*...sWM_CLOSE           \45\ close:16:*/
        case WM_CLOSE:
        {
        /*
          WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
          WaitForSingleObject(win->hmutex, INFINITE);
          DestroyWindow(hwnd);
          ReleaseMutex(win->hmutex);
        */
        terminated = TRUE;
        }
        return 0;
/*...e*/
/*...sWM_DESTROY         \45\ destroy:16:*/
        case WM_DESTROY:
        {
        WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
        diag_message (DIAG_INIT, "WM_DESTROY Wait for Mutex");
        WaitForSingleObject(win->hmutex, INFINITE);
        win->hwnd = (HWND) NULL;
        diag_message (DIAG_INIT, "Delete palette");
        if ( win->hpalette != (HPALETTE) NULL )
            DeleteObject(win->hpalette);
        win->hpalette = (HPALETTE) NULL;
        diag_message (DIAG_INIT, "Delete bitmap");
        DeleteObject(win->hbitmap);
        win->hbitmap = (HBITMAP) NULL;
        diag_message (DIAG_INIT, "Release mutex");
        ReleaseMutex(win->hmutex);
        diag_message (DIAG_INIT, "Set events");
        SetEvent(win->heventUpdated);
        SetEvent(win->heventDeleted);
        diag_message (DIAG_INIT, "Quit message posted");
        PostQuitMessage(0);
        diag_message (DIAG_INIT, "Window destroyed");
        }
        return 0;
/*...e*/
/*...sWM_KEYDOWN         \45\ keypress:16:*/
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        {
        WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
        int wk = win_map_key(wParam, lParam, TRUE);
        diag_message (DIAG_KBD_WIN_KEY, "keydown: wParam = 0x%02X, lParam = 0x%04X, wk = 0x%02X", wParam, lParam, wk);
        if ( wk != -1 )
            {
            // (*win->keypress)(wk);
            win_add_key_event (win, wk);
            return 0L;
            }
        }
        break;
/*...e*/
/*...sWM_KEYUP           \45\ keyrelease:16:*/
        case WM_SYSKEYUP:
        case WM_KEYUP:
        {
        WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
        int wk = win_map_key(wParam, lParam, FALSE);
        diag_message (DIAG_KBD_WIN_KEY, "keyup: wParam = 0x%02X, wk = 0x%02X", wParam, wk);
        if ( wk != -1 )
            {
            // (*win->keyrelease)(wk);
            win_add_key_event (win, wk | WK_Release);
            if ( wk == WK_Menu )
                /* Strange problem whereby if you press
                   the menu key, you get VK_CONTROL then VK_MENU,
                   but when you release it, you only get VK_MENU. */
                // (*win->keyrelease)(WK_Control_L);
                win_add_key_event (win, WK_Control_L | WK_Release);
            return 0L;
            }
        }
        break;
/*...e*/
/*...sWM_UPDATEWINDOW    \45\ update window:16:*/
        case WM_UPDATEWINDOW:
        {
        WIN_PRIV *win = (WIN_PRIV *) GetWindowLongPtr(hwnd, 0);
        UpdateWindow(hwnd);
        SetEvent(win->heventUpdated);
        }
        return 0;
/*...e*/
/*...sWM_DELETEWINDOW    \45\ delete window:16:*/
        case WM_DELETEWINDOW:
            diag_message (DIAG_INIT, "Call DestroyWindow: thread = 0x%X", GetCurrentThreadId ());
            DestroyWindow(hwnd);
            return 0;
/*...e*/
        }
    return DefWindowProc(hwnd, message, wParam, lParam);
    }
/*...e*/
/*...sUiThread:0:*/
/* The main thread of the program isn't necessarily a windows thread.
   So we create a second thread to create and watch the bitmap viewer window. */

static void _cdecl UiThread(void *p)
    {
    WIN_PRIV *win = (WIN_PRIV *) p;
    MSG msg;
    BOOL bRet;
    /*
    int iWth =    cxFixedFrame + win->width*win->width_scale + cxFixedFrame;
    int iHgt =    cyFixedFrame + cyCaption + win->height*win->height_scale + cyFixedFrame;
    RECT rcSize;
    rcSize.left = 0;
    rcSize.right = win->width*win->width_scale;
    rcSize.top = 0;
    rcSize.bottom = win->height*win->height_scale;
    AdjustWindowRectEx (&rcSize, WS_BORDER|WS_SYSMENU|WS_MINIMIZEBOX|WS_OVERLAPPED, 0, 0);
    int iWth = rcSize.right - rcSize.left;
    int iHgt = rcSize.bottom - rcSize.top;
    */
    win->iXFrame = win->width * win->width_scale + iXBorder;
    win->iYFrame = win->height * win->height_scale + iYBorder;
    // printf ("Requested window size = %d x %d\n", win->width*win->width_scale, win->height*win->height_scale);
    // printf ("Create window size = %d x %d\n", win->iXFrame, win->iYFrame);
    win->hwnd = CreateWindow(
        WC_MEMUWIN,     /* Window class name */
        win->title,     /* Window title */
        WS_BORDER|WS_SYSMENU|WS_MINIMIZEBOX|WS_OVERLAPPED,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        /*
        cxFixedFrame + win->width*win->width_scale + cxFixedFrame,
        cyFixedFrame + cyCaption + win->height*win->height_scale + cyFixedFrame,
        */
        win->iXFrame,
        win->iYFrame,
        NULL,           /* No parent for this window */
        NULL,           /* Use the class menu (not our own) */
        hinst,          /* Who created the window */
        win);           /* Parameters to pass on */
    diag_message (DIAG_INIT, "Create window: win = 0x%X, hwnd = 0x%X", win, win->hwnd);
    ShowWindow(win->hwnd, SW_SHOWNORMAL);
    SetWindowPos(win->hwnd, HWND_TOP, 0, 0, 0, 0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_SHOWWINDOW);
    SetEvent(win->heventCreated);
    while ( ( bRet = GetMessage(&msg, NULL, 0, 0) ) != 0 )
        {
        // diag_message (DIAG_INIT, "MSG: hwnd = 0x%X, message = 0x%X", msg.hwnd, msg.message);
        if ( bRet == -1 )
            {
            diag_message (DIAG_INIT, "Message loop error");
            break;
            }
        // TranslateMessage(&msg); /* So that WM_CHAR works! */
        DispatchMessage(&msg);
        // diag_message (DIAG_INIT, "DispatchMessage returned: hwnd = 0x%X, message = 0x%X", msg.hwnd, msg.message);
        }
    diag_message (DIAG_INIT, "Message loop terminated");
    }
/*...e*/

/*...swin_init:0:*/
static void win_init()
    {
    WNDCLASS wc;
    HDC hdcScreen;
    LONG lRasterCaps;
    LONG lPlanes;
    LONG lBitCount;

    hinst = (HINSTANCE) GetModuleHandle((LPCSTR) NULL);

    /* Register the window class */
    wc.lpszClassName = WC_MEMUWIN;
    wc.hInstance     = hinst;
    wc.lpfnWndProc   = MemuWindowProc;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = (HICON) NULL;
    wc.lpszMenuName  = NULL;
    wc.hbrBackground = (HBRUSH) GetStockObject(NULL_BRUSH);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = sizeof(void *);
    RegisterClass(&wc);

    /* Learn some system metrics */
    GuessBorders ();
    /*
    cxScreen     = GetSystemMetrics(SM_CXSCREEN);
    cyScreen     = GetSystemMetrics(SM_CYSCREEN);
    cyCaption    = GetSystemMetrics(SM_CYCAPTION);
    cxFixedFrame = GetSystemMetrics(SM_CXFIXEDFRAME);
    cyFixedFrame = GetSystemMetrics(SM_CYFIXEDFRAME);
    printf ("cxScreen = %d, cyScreen = %d\n", cxScreen, cyScreen);
    printf ("cxFixedFrame = %d, cyFixedFrame = %d, cyCaption = %d\n", cxFixedFrame, cyFixedFrame, cyCaption);
    */

    /* Work out the screen characteristics */
    hdcScreen   = GetWindowDC((HWND) NULL);
    lRasterCaps = GetDeviceCaps( hdcScreen, RASTERCAPS );
    lPlanes     = GetDeviceCaps( hdcScreen, PLANES     );
    lBitCount   = GetDeviceCaps( hdcScreen, BITSPIXEL  );
    ReleaseDC((HWND) NULL, hdcScreen);
    if ( (lRasterCaps & RC_PALETTE) != 0 && lPlanes == 1 && lBitCount == 8 )
        fPalettised = TRUE;
    else
        fPalettised = FALSE;
    /* Mutex to ensure only one window at a time adds key events to the buffer */
    hKBmutex = CreateMutex(NULL, FALSE, NULL);
    }
/*...e*/

/*...swin_create:0:*/
WIN *win_create(
    int width, int height,
    int width_scale, int height_scale,
    const char *title,
    const char *display,
    const char *geometry,
    COL *cols, int n_cols,
    void (*keypress)(int k),
    void (*keyrelease)(int k)
    )
    {
    WIN_PRIV *win = (WIN_PRIV *) emalloc(sizeof(WIN_PRIV));
    int i;

    if ( ! win_inited )
        {
        win_init();
        win_inited = TRUE;
        }

    win->width        = width;
    win->height       = height;
    win->width_scale  = width_scale;
    win->height_scale = height_scale;
    strcpy(win->title, title);

    /* Mutex used to ensure either main tracing thread or windows
       thread can only access win at any one time. */
    win->hmutex = CreateMutex(NULL, FALSE, NULL);

    /* Event which gets signalled when initialisation completes */
    win->heventCreated = CreateEvent(NULL, TRUE, FALSE, NULL);

    /* Event which gets signalled when update window occurs */
    win->heventUpdated = CreateEvent(NULL, TRUE, FALSE, NULL);

    /* Event which gets signalled when window destroyed */
    win->heventDeleted = CreateEvent(NULL, TRUE, FALSE, NULL);

    /* Space to record the pixel data */
    win->data = emalloc(width*height);
    memset(win->data, 0, width*height);

    /* Record the palette */
    for ( i = 0; i < n_cols; i++ )
        {
        win->cols[i].r = cols[i].r;
        win->cols[i].g = cols[i].g;
        win->cols[i].b = cols[i].b;
        }
    win->n_cols = n_cols;

    win->keypress   = keypress;
    win->keyrelease = keyrelease;

    /* Start a thread to ensure event processing happens */
    _beginthread(UiThread, 0x10000, win);
    WaitForSingleObject(win->heventCreated, INFINITE);

    return (WIN *) win;
    }
/*...e*/
/*...swin_delete:0:*/
void win_delete(WIN *win_pub)
    {
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
    diag_message (DIAG_INIT, "win_delete: Wait for mutex");
    WaitForSingleObject(win->hmutex, INFINITE);
    diag_message (DIAG_INIT, "win_delete: hwnd = 0x%X, thread = 0x%X", win->hwnd, GetCurrentThreadId ());
    if ( win->hwnd != (HWND) NULL )
        {
        diag_message (DIAG_INIT, "Post delete message");
        if ( ! PostMessage(win->hwnd, WM_DELETEWINDOW, 0, 0) )
            {
            diag_message (DIAG_INIT, "PostMessage failed: err = %d", GetLastError ());
            }
        diag_message (DIAG_INIT, "Release mutex");
        ReleaseMutex(win->hmutex);
        diag_message (DIAG_INIT, "Wait for signal");
        WaitForSingleObject(win->heventDeleted, INFINITE);
        win->hwnd = NULL;
        }
    else
        {
        diag_message (DIAG_INIT, "No window: Release mutex");
        ReleaseMutex(win->hmutex);
        }
    if ( win->heventCreated != NULL )
        {
        CloseHandle (win->heventCreated);
        win->heventCreated = NULL;
        }
    if ( win->heventUpdated != NULL )
        {
        CloseHandle (win->heventUpdated);
        win->heventUpdated = NULL;
        }
    if ( win->heventDeleted != NULL )
        {
        CloseHandle (win->heventDeleted);
        win->heventDeleted = NULL;
        }
    if ( win->hmutex != NULL )
        {
        CloseHandle (win->hmutex);
        win->hmutex = NULL;
        }
    /*
    if ( win->hbitmap != NULL )
        {
        CloseHandle (win->hbitmap);
        win->hbitmap = NULL;
        }
    if ( win->hpalette != NULL )
        {
        CloseHandle (win->hpalette);
        win->hpalette = NULL;
        }
    */
    if ( win->data != NULL )
        {
        free(win->data);
        win->data = NULL;
        }
    free(win);
    }
/*...e*/

/*...swin_refresh:0:*/
void win_refresh(WIN *win_pub)
    {
    WIN_PRIV *win = (WIN_PRIV *) win_pub;
    WaitForSingleObject(win->hmutex, INFINITE);
    if ( win->hwnd != (HWND) NULL )
        {
        ResetEvent(win->heventUpdated);
        InvalidateRect(win->hwnd, NULL, TRUE);
        PostMessage(win->hwnd, WM_UPDATEWINDOW, 0, 0); /* delayed redraw */
        ReleaseMutex(win->hmutex);
        WaitForSingleObject(win->heventUpdated, INFINITE);
        }
    else
        ReleaseMutex(win->hmutex);
    }
/*...e*/

BOOLEAN win_active (WIN *win)
    {
    return TRUE;
    }

void win_kbd_leds (BOOLEAN bCaps, BOOLEAN bNum, BOOLEAN bScroll)
    {
    }

void win_leds_state (BOOLEAN *bCaps, BOOLEAN *bNum, BOOLEAN *bScroll)
    {
    *bCaps = GetKeyState (VK_CAPITAL) & 1;
    *bNum = GetKeyState (VK_NUMLOCK) & 1;
    *bScroll = GetKeyState (VK_SCROLL) & 1;
    }

/*...swin_shifted_wk:0:*/
/* Keys of the host keyboard have an unshifted label and a shifted label
   written on them, eg: unshifted "1", shifted "!". Alphabetic keys typically
   omit the unshifted lowercase letter, but notionally it is there.
   This module returns WK_ values with names which reflect unshifted label.
   Sometimes the module user will want to know the equivelent shifted label.

   The problem with this code is that it assumes the UK keyboard layout. */

int win_shifted_wk(int wk)
    {
    if ( wk >= 'a' && wk <= 'z' )
        return wk-'a'+'A';
    switch ( wk )
        {
        case '1':   return '!';
        case '2':   return '"';
        case '3':   return '#'; /* pound */
        case '4':   return '$';
        case '5':   return '%';
        case '6':   return '^';
        case '7':   return '&';
        case '8':   return '*';
        case '9':   return '(';
        case '0':   return ')';
        case '-':   return '_';
        case '=':   return '+';
        case '[':   return '{';
        case ']':   return '}';
        case ';':   return ':';
        case '\'':  return '@';
        case '#':   return '~';
        case '\\':  return '|';
        case ',':   return '<';
        case '.':   return '>';
        case '/':   return '?';
        default:    return ( wk >= 0 && wk < 0x100 ) ? wk : -1;
        }
    }
/*...e*/

/*...swin_handle_events:0:*/
void win_handle_events()
    {
    int iPtr;
    while ( iKBOut != iKBIn )
        {
        iPtr = iKBOut;
        if ( ++iKBOut >= LEN_KEYBUF ) iKBOut = 0;
        if ( keybuf[iPtr].wk & WK_Release )
            keybuf[iPtr].win->keyrelease (keybuf[iPtr].wk & (~WK_Release));
        else
            keybuf[iPtr].win->keypress (keybuf[iPtr].wk);
        }
    /* It seems important to some versions of Windows at least,
       that we only shut everything down from the main thread. */
    if ( terminated )
        terminate("user closed window");
    }
/*...e*/
void win_max_size (const char *display, int *pWth, int *pHgt)
    {
    /*
    static int bFirst = 1;
    if ( bFirst )
        {
        int iXDPI, iYDPI;
        SetProcessDpiAwareness (PROCESS_SYSTEM_DPI_AWARE);
        HMONITOR hMon = MonitorFromWindow(GetDesktopWindow(),
                          MONITOR_DEFAULTTOPRIMARY);
        GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &iXDPI, &iYDPI);
        printf ("DPI = %d x %d\n", iXDPI, iYDPI);
        bFirst = 0;
        }
    RECT rcSize;
    if ( SystemParametersInfo (SPI_GETWORKAREA, 0, &rcSize, 0) )
        {
        *pWth = rcSize.right - rcSize.left - 2 * GetSystemMetrics (SM_CXBORDER);
        *pHgt = rcSize.bottom - rcSize.top - GetSystemMetrics (SM_CYCAPTION) - 2 * GetSystemMetrics (SM_CYBORDER);
        printf ("WORKAREA: left = %d, right = %d, top = %d, bottom = %d\n",
            rcSize.left, rcSize.right, rcSize.top, rcSize.bottom);
        }
    else
        {
        *pWth = GetSystemMetrics (SM_CXMAXTRACK) - 2 * GetSystemMetrics (SM_CXBORDER);
        *pHgt = GetSystemMetrics (SM_CYMAXTRACK) - GetSystemMetrics (SM_CYCAPTION) - 2 * GetSystemMetrics (SM_CYBORDER);
        }
    printf ("cxMaxTrack = %d, cyMaxTrack = %d\n", GetSystemMetrics (SM_CXMAXTRACK),
        GetSystemMetrics (SM_CYMAXTRACK));
    printf ("cxBorder = %d, cyBorder = %d, cyCaption = %d\n", GetSystemMetrics (SM_CXBORDER),
        GetSystemMetrics (SM_CYBORDER), GetSystemMetrics (SM_CYCAPTION));
    printf ("cxFixedFrame = %d, cyFixedFrame = %d\n",
        GetSystemMetrics (SM_CXFIXEDFRAME), GetSystemMetrics (SM_CYFIXEDFRAME));
    printf ("cxEdge = %d, cyEdge = %d\n",
        GetSystemMetrics (SM_CXEDGE), GetSystemMetrics (SM_CYEDGE));
    printf ("cxFrame = %d, cyFrame = %d\n",
        GetSystemMetrics (SM_CXFRAME), GetSystemMetrics (SM_CYFRAME));
    printf ("cxFullscreen = %d, cyFullscreen = %d\n",
        GetSystemMetrics (SM_CXFULLSCREEN), GetSystemMetrics (SM_CYFULLSCREEN));
    printf ("cxPaddedborder = %d\n",
        GetSystemMetrics (SM_CXPADDEDBORDER));
    printf ("cxScreen = %d, cyScreen = %d\n",
        GetSystemMetrics (SM_CXSCREEN), GetSystemMetrics (SM_CYSCREEN));
    printf ("win_max_size: width = %d, height = %d\n", *pWth, *pHgt);
    */
    GuessBorders ();
    *pWth = GetSystemMetrics (SM_CXFULLSCREEN) - iXBorder;
    *pHgt = GetSystemMetrics (SM_CYFULLSCREEN) - iYBorder;
    }
