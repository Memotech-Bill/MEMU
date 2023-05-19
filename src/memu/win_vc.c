/*

  win_vc.c - Derived from win_fb, but use VideoCore IV DISPMANX API to do GPU accelerated
             palette mapping and window scaling.

			 The VideoCore code is based upon that produced by Andrew Duncan, with am
			 MIT License.
			 https://github.com/AndrewFromMelbourne/raspidmx

*/

#define VC_DISPLAY 0

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <termios.h>
#include <bcm_host.h>
#include <interface/vmcs_host/vc_vchi_gencmd.h>

#include "types.h"
#include "diag.h"
#include "common.h"
#include "win.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vwin\46\h:0:*/
/*...e*/

typedef struct st_winpriv WIN_PRIV;

typedef struct
	{
	int	iDisplay;						//	VideoCore display number
	int	nWin;							//	Number of MEMU windows using this display
	WIN_PRIV *winAct;					//  Active window on this display
    DISPMANX_DISPLAY_HANDLE_T hDisp;	//	Display handle
    DISPMANX_MODEINFO_T modeInfo;		//	Screen dimensions
    DISPMANX_RESOURCE_HANDLE_T resBkGd;	//	Background resource
    DISPMANX_ELEMENT_HANDLE_T eleBkGd;	//	Background display element
	} VCD;

struct st_winpriv
	{
	int width, height;
	int width_scale, height_scale;
	int n_cols;
	byte *data;
	void (*keypress)(int);
	void (*keyrelease)(int);
	/* Private window data below - Above must match definition of WIN in win.h */
	byte *image;						// 	Pointer to image data (possibly scaled up)
	int iWin;							// 	Window number
	int img_w, img_h;					//	Scaled image dimensions
	int left, top;						//	Position of top corner
	int iDisplay;						//	VideoCore display number to use
	VCD	*vcd;							//	Pointer to VideoCore display details
	DISPMANX_RESOURCE_HANDLE_T resImg;	//	Image resource
    DISPMANX_ELEMENT_HANDLE_T eleImg;	//	Image display element
	__u32 *cols;
	};

#define   MAX_WINS 10					//	Maximum number of separate windows
static int n_wins = 0; 					//	Actual number of windows
static WIN_PRIV *wins[MAX_WINS];		//	Pointer to each window structure
static int iActiveWin = 0;				//	Currently active window
#define	GPU_DEFAULT		3				//	Default GPU mode
static int gpu_mode = GPU_DEFAULT;		//	GPU scaling mode
//												1 = ARM upscaling
//												2 = GPU upscaling - default interpolation
//												3 = GPU upscaling - no interpolation

#define	NPAL		256					//	Size of palette
#define	MAX_VCD 	2					//	Maximum number of VideoCore displays
static BOOLEAN bcm_init =  FALSE;		//	Broadcom API initialised
static int n_vcd = 0;					//	Number of active VideoCore displays
static VCD *vcds[MAX_VCD];				//	Pointer to each display structure
static char sSclKer[1024];				//	Remember previous scaling kernel

static BOOLEAN tty_init =  FALSE;		//	Keyboard interface initialised
static int ttyfd = 0;					//	File descriptor for keyboard
static struct termios tty_attr_old;		//	Remember original keyboard state to restore
static int old_keyboard_mode;			//	Also remember old mode

static struct
	{
	int	scale;							//	Scaling of DISPMANX windows for overscan
	int	left;							//	Left margin
	int	right;							//	Right margin
	int	top;							//	Top margin
	int bottom;							//	Bottom margin
	} oscan;							//	Values obtained from GPU

#define MKY_LSHIFT   0x01
#define MKY_RSHIFT   0x02
#define MKY_LCTRL    0x04
#define MKY_RCTRL    0x08
#define MKY_LALT     0x10
#define MKY_RALT     0x20
#define MKY_CAPSLK   0x40

static int mod_keys  =  0;

void set_gpu_mode (int mode)
	{
	if ( ( mode < 1 ) || ( mode > 3 ) )
		fatal ("gpu_mode must be 1, 2, or 3");
	gpu_mode = mode;
	}

int get_gpu_mode (void)
	{
	if ( gpu_mode == GPU_DEFAULT )	return 0;
	return	gpu_mode;
	}

static void vc_configure (void)
	{
	static const char sNoInt[] = "scaling_kernel 0 0 0 0 0 0 0 0 1 1 1 1 255 255 255 255 255 255 255 255 1 1 1 1 0 0 0 0 0 0 0 0   1";
	int instNum = 0;
	VCHI_INSTANCE_T vchi_instance;
	VCHI_CONNECTION_T *vchi_connection = NULL;
	char buffer[1024];

	memset (&oscan, 0, sizeof (oscan));

	vcos_init ();

    if ( vchi_initialise (&vchi_instance) != 0 )
        fatal ("VCHI initialization failed");

    //create a vchi connection
    if ( vchi_connect (NULL, 0, vchi_instance) != 0 )
        fatal ("VCHI connection failed");

    vc_vchi_gencmd_init (vchi_instance, &vchi_connection, 1);

	if ( vc_gencmd (buffer, sizeof (buffer), "get_config int") != 0 )
		fatal ("GENCMD failed");
	//	Returned string overflows buffer in diag_message
	// diag_message (DIAG_WIN_HW, "Configuration string:\n%s", buffer);

	if ( ! vc_gencmd_number_property (buffer, "overscan_scale", &oscan.scale) )	oscan.scale	= 0;
	if ( ! oscan.scale )
		{
		vc_gencmd_number_property (buffer, "overscan_left", &oscan.left);
		vc_gencmd_number_property (buffer, "overscan_right", &oscan.right);
		vc_gencmd_number_property (buffer, "overscan_top", &oscan.top);
		vc_gencmd_number_property (buffer, "overscan_bottom", &oscan.bottom);
		}
	diag_message (DIAG_WIN_HW, "Overscan: left = %d, right = %d, top = %d, bottom = %d",
		oscan.left, oscan.right, oscan.top, oscan.bottom);

	// Turn of DISPMANX interpolation
	if ( gpu_mode == 3 )
		{
		//	Get previous scaling kernel
		if ( vc_gencmd (sSclKer, sizeof (sSclKer), "scaling_kernel") != 0 )
			fatal ("Failed to fetch old scaling kernel");
		diag_message (DIAG_WIN_HW, sSclKer);

		if ( vc_gencmd (buffer, sizeof (buffer), sNoInt) != 0 )
			fatal ("Failed to set non-interpolation scaling kernel");
		}

    vc_gencmd_stop ();

    //close the vchi connection
    if ( vchi_disconnect (vchi_instance) != 0 )
        fatal ("VCHI disconnect failed");
	}

static void vc_restore (void)
	{
	int instNum = 0;
	VCHI_INSTANCE_T vchi_instance;
	VCHI_CONNECTION_T *vchi_connection = NULL;
	char buffer[1024];
	char *ps;

    if ( vchi_initialise (&vchi_instance) != 0 )
        fatal ("VCHI initialization failed");

    //create a vchi connection
    if ( vchi_connect (NULL, 0, vchi_instance) != 0 )
        fatal ("VCHI connection failed");

    vc_vchi_gencmd_init (vchi_instance, &vchi_connection, 1);

	if ( ( ps = strchr (sSclKer, '=') ) != NULL )	*ps = ' ';
	if ( vc_gencmd (buffer, sizeof (buffer), sSclKer) != 0 )
		fatal ("Failed to restore scaling kernel");
	diag_message (DIAG_WIN_HW, "Restored original scaling kernel");

    vc_gencmd_stop ();

    //close the vchi connection
    if ( vchi_disconnect (vchi_instance) != 0 )
        fatal ("VCHI disconnect failed");
	}

static VCD * win_vc_init (int iDisplay)
	{
	DISPMANX_UPDATE_HANDLE_T update;
    VC_DISPMANX_ALPHA_T alpha;
    VC_RECT_T rect;
    VC_RECT_T src_rect;
    VC_RECT_T dst_rect;
	VCD *vcd = NULL;
    uint32_t vc_image_ptr;
    uint32_t background = 0;
	int	i;

	//	Initialise Broadcom APIs.
	if ( ! bcm_init )
		{
		bcm_host_init();
		diag_message (DIAG_WIN_HW, "Initialised Broadcom Interface");
		vc_configure ();
		bcm_init = TRUE;
		}

	//	Search for existing display structure
	for ( i = 0; i < n_vcd; ++i )
		{
		if ( vcds[i]->iDisplay == iDisplay )
			{
			++vcds[i]->nWin;
			diag_message (DIAG_WIN_HW, "Reuse existing display %d", iDisplay);
			return	vcds[i];
			}
		}

	//	Allocate data structure
	if ( n_vcd == MAX_VCD )
		fatal("Too many VideoCore displays");
	vcd = (VCD *) emalloc (sizeof (VCD));
	memset (vcd, 0, sizeof (VCD));
	vcds[n_vcd]	=	vcd;
	++n_vcd;
	vcd->iDisplay = iDisplay;
	vcd->nWin = 1;

	//	Open display and get dimensions
    vcd->hDisp	=	vc_dispmanx_display_open (iDisplay);
    if ( vcd->hDisp == 0 ) fatal ("Failed to open VideoCore display %d", iDisplay);

    if ( vc_dispmanx_display_get_info (vcd->hDisp, &vcd->modeInfo) != 0 )
		fatal ("Failed to get dimensions of VideoCore display %d", iDisplay);
	diag_message (DIAG_WIN_HW, "Opened new VC display %d (%dx%d)",
		iDisplay, vcd->modeInfo.width, vcd->modeInfo.height);

	//	Create background layer (hides Linux Framebuffer)
    vcd->resBkGd	=	vc_dispmanx_resource_create	(VC_IMAGE_RGB565,
		                                             1,
		                                             1,
                                                     &vc_image_ptr);
    if ( vcd->resBkGd == 0 )
		fatal ("Failed to create background resource for VideoCore display %d", iDisplay);

    vc_dispmanx_rect_set (&rect, 0, 0, 1, 1);
    if ( vc_dispmanx_resource_write_data (vcd->resBkGd,
                                          VC_IMAGE_RGB565,
                                          sizeof (background),
                                          &background,
	                                      &rect) != 0 )
		fatal ("Failed to set background data for VideoCore display %d", iDisplay);
	diag_message (DIAG_WIN_HW, "Created background layer for display %d", iDisplay);

	//	Output background layer
	update = vc_dispmanx_update_start (0);
	if ( update == 0 ) fatal ("Failed to start display update");

    vc_dispmanx_rect_set (&src_rect, 0, 0, 1, 1);
    vc_dispmanx_rect_set (&dst_rect, 0, 0, 0, 0);

    alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    alpha.opacity = 255;
    alpha.mask = 0;

    vcd->eleBkGd	=	vc_dispmanx_element_add (update,
                                vcd->hDisp,
                                1,
                                &dst_rect,
                                vcd->resBkGd,
                                &src_rect,
                                DISPMANX_PROTECTION_NONE,
                                &alpha,
                                NULL,
                                DISPMANX_NO_ROTATE);

	if ( vc_dispmanx_update_submit_sync (update) != 0 )
		fatal ("Failed to complete window update");
	diag_message (DIAG_WIN_HW, "Displayed background layer on display %d", iDisplay);
	return vcd;
	}

static void win_kbd_init (void)
	{
	struct termios tty_attr;
	int flags;
	int  i;

	/* save old keyboard mode */
#ifndef	NOKBD		// Defining NOKBD allows debugging using gdb.
	if (ioctl (ttyfd, KDGKBMODE, &old_keyboard_mode) < 0)
		{
		fatal ("Unable to get existing keyboard mode.");
		}
    diag_message (DIAG_WIN_HW, "old_keyboard_mode = %d", old_keyboard_mode);

	tcgetattr (ttyfd, &tty_attr_old);
	tty_init   =  TRUE;

	/* make stdin non-blocking */
	flags = fcntl (ttyfd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl (ttyfd, F_SETFL, flags);

	/* turn off buffering, echo and key processing */
	tty_attr = tty_attr_old;
	tty_attr.c_lflag &= ~(ICANON | ECHO | ISIG);
	tty_attr.c_iflag &= ~(ISTRIP | INLCR | ICRNL | IGNCR | IXON | IXOFF);
	tcsetattr (ttyfd, TCSANOW, &tty_attr);

	ioctl (ttyfd, KDSKBMODE, K_MEDIUMRAW);
#endif
	}

static void win_scale (WIN_PRIV *win)
	{
	int	iRow, jCol, iPix, jPix;
	byte *pdat	=	win->data;
	byte *pimg	=	win->image;
	if ( pimg == pdat ) return;
	for ( iRow = 0; iRow < win->height; ++iRow )
		{
		for ( iPix = 0; iPix < win->height_scale; ++iPix )
			{
			for ( jCol = 0; jCol < win->width; ++jCol )
				{
				for ( jPix = 0; jPix < win->width_scale; ++jPix )
					{
					*pimg	=	*pdat;
					++pimg;
					}
				++pdat;
				}
			pdat	-=	win->width;
			}
		pdat	+=	win->width;
		}
	}

/*...swin_refresh:0:*/
void win_refresh (WIN *win_pub)
	{
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
    VC_RECT_T rect;
	DISPMANX_UPDATE_HANDLE_T update;
	int iWth16	= ( win->img_w + 15 ) & 0xFFF0;

	if ( gpu_mode == 1 ) win_scale (win);

	//	Update pixel map
	vc_dispmanx_rect_set (&rect, 0, 0, win->img_w, win->img_h);
	if ( vc_dispmanx_resource_write_data (win->resImg,
                                          VC_IMAGE_8BPP,
                                          iWth16,
                                          win->image,
                                          &rect) != 0 )
		fatal ("Failed to transfer data for window %d", win->iWin);

	//	Refresh display
//   diag_message (DIAG_WIN_HW, "Refresh window %d, Active window = %d", win->iWin, iActiveWin);
	if ( win != win->vcd->winAct ) return;
	update = vc_dispmanx_update_start (0);
	if ( update == 0 ) fatal ("Failed to start display update");

	if ( vc_dispmanx_element_change_source (update,
		                                    win->eleImg,
	                                        win->resImg) != 0 )
		fatal ("Failed to update the displayed window element");
	if ( vc_dispmanx_update_submit_sync (update) != 0 )
		fatal ("Failed to complete window update");

//   diag_message (DIAG_WIN_HW, "Refresh complete");
	}

static void win_swap (WIN_PRIV *win)
	{
	VCD *vcd = win->vcd;
	DISPMANX_UPDATE_HANDLE_T update;
    VC_DISPMANX_ALPHA_T alpha;
    VC_RECT_T src_rect;
    VC_RECT_T dst_rect;
	int	iXPos, iYPos, iXScl, iYScl;
	diag_message (DIAG_WIN_HW, "Swap to window %d", win->iWin);

	//	Test for window already active
	if ( win == vcd->winAct ) return;

	//	Set palette colours
	if ( vc_dispmanx_resource_set_palette (win->resImg,
                                           win->cols,
                                           0,
			                               NPAL * sizeof (__u32)) != 0 )
		fatal ("Failed to initialise palette for window %d", win->iWin);
	diag_message (DIAG_WIN_HW, "Updated palette for display %d", win->iDisplay);

	//	Refresh display
	update = vc_dispmanx_update_start (0);
	if ( update == 0 ) fatal ("Failed to start display update");

	if ( ( vcd->winAct != NULL ) && ( vcd->winAct->eleImg != 0 ) )
		{
		if ( vc_dispmanx_element_remove (update, vcd->winAct->eleImg) != 0 )
			fatal ("Failed to remove previous active window element");
		vcd->winAct->eleImg = 0;
		diag_message (DIAG_WIN_HW, "Removed previous window (%d) element", vcd->winAct->iWin);
		}

	vc_dispmanx_rect_set (&src_rect,
		0,
		0,
		win->img_w << 16,
		win->img_h << 16);

    vc_dispmanx_rect_set (&dst_rect,
                          win->left,
                          win->top,
                          win->width * win->width_scale,
                          win->height * win->height_scale);

    alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    alpha.opacity = 255;
    alpha.mask = 0;

    win->eleImg	=	vc_dispmanx_element_add (update,
                                win->vcd->hDisp,
                                2,
                                &dst_rect,
                                win->resImg,
                                &src_rect,
                                DISPMANX_PROTECTION_NONE,
                                &alpha,
                                NULL,
                                DISPMANX_NO_ROTATE);
	diag_message (DIAG_WIN_HW, "Added new window element (%d)", win->iWin);
	vcd->winAct = win;
	iActiveWin = win->iWin;

	if ( vc_dispmanx_update_submit_sync (update) != 0 )
		fatal ("Failed to complete window update");
	}

void win_next (void)
	{
	if ( ++iActiveWin >= n_wins )	iActiveWin	=  0;
	win_swap (wins[iActiveWin]);
	}

void win_prev (void)
	{
	if ( --iActiveWin < 0 )	iActiveWin	=  n_wins - 1;
	win_swap (wins[iActiveWin]);
	}

BOOLEAN win_active (WIN *win_pub)
    {
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
    return ( win->iWin == iActiveWin );
    }

/*...e*/

/*...swin_create:0:*/
static __u32 win_vc_colour (COL *col)
	{
	__u32 uCol = ( col->r << 16 ) | ( col->g << 8 ) | ( col->b );
	return uCol;
	}

void win_max_size (const char *display, int *pWth, int *pHgt)
    {
    // Dummy values, not used anyway
    *pWth = 640;
    *pHgt = 480;
    }

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
	WIN_PRIV *win;
	VC_RECT_T rect;
	int  i;

	diag_message (DIAG_WIN_HW, "Create window %d, size = %d x %d",
		n_wins, width, height);

	if ( n_wins == MAX_WINS )
		fatal("too many windows");

	if ( ! tty_init )  win_kbd_init ();

	win = (WIN_PRIV *) emalloc(sizeof(WIN_PRIV));
	win->iWin            = n_wins;
	win->width           = width;
	win->height          = height;
	win->data            = emalloc (width * height);
	memset (win->data, 0, width * height);
	win->keypress        = keypress;
	win->keyrelease      = keyrelease;
	win->n_cols          = n_cols;
	win->cols            = (__u32 *) emalloc (NPAL * sizeof (__u32));
	for ( i = 0; i < n_cols; ++i )
	  {
	    win->cols[i]   =  win_vc_colour (&cols[i]);
	    diag_message (DIAG_WIN_HW, "Palette colour %d: r = %d, g = %d, b = %d, col = 0x%06x",
			  i, cols[i].r, cols[i].g, cols[i].b, win->cols[i]);
	  }
	wins[n_wins++] =  win;

	//	Create display layer
	if ( display )	win->iDisplay = atoi (display);
	else			win->iDisplay = VC_DISPLAY;
	win->vcd = win_vc_init (win->iDisplay);
	if ( win->iDisplay == VC_DISPLAY )
		{
		win->width_scale = ( win->vcd->modeInfo.width - oscan.left - oscan.right ) / win->width;
		win->height_scale = ( win->vcd->modeInfo.height - oscan.top - oscan.bottom ) / win->height;
		if ( win->width_scale > win->height_scale )	win->width_scale = win->height_scale;
		win->left = ( win->vcd->modeInfo.width + oscan.left - oscan.right
			- win->width * win->width_scale ) / 2;
		win->top = ( win->vcd->modeInfo.height + oscan.top - oscan.bottom
			- win->height * win->height_scale ) / 2;
		}
	else
		{
		win->width_scale = win->vcd->modeInfo.width / win->width;
		win->height_scale = win->vcd->modeInfo.height / win->height;
		if ( win->width_scale > win->height_scale )	win->width_scale = win->height_scale;
		win->left = ( win->vcd->modeInfo.width - win->width * win->width_scale ) / 2;
		win->top = ( win->vcd->modeInfo.height - win->height * win->height_scale ) / 2;
		}
	diag_message (DIAG_WIN_HW, "New window: size = (%d, %d), scale = (%d, %d), pos = (%d, %d)",
		win->width, win->height, win->width_scale, win->height_scale, win->left, win->top);
	if ( ( gpu_mode == 1 ) && ( win->height_scale > 1 ) )
		{
		win->img_w = win->width * win->width_scale;
		win->img_h = win->height * win->height_scale;
		win->image = emalloc (win->img_w * win->img_h);
		diag_message (DIAG_WIN_HW, "Allocated upscaling buffer for window");
		}
	else
		{
		win->img_w = win->width;
		win->img_h = win->height;
		win->image = win->data;
		}
    uint32_t vcImagePtr = 0;
	int iWth16	= ( win->img_w + 15 ) & 0xFFF0;
	int iHgt16	= ( win->img_h + 15 ) & 0xFFF0;
	if ( win->img_w != iWth16 )
		fatal ("Width of window is not a multiple of 16 pixels");
    win->resImg = vc_dispmanx_resource_create (
            VC_IMAGE_8BPP,
            win->img_w << 16 | iWth16,
            win->img_h << 16 | iHgt16,
            &vcImagePtr);
    if ( win->resImg == 0 )
		fatal ("Failed to create VideoCore image resource for window %d", win->iWin);

    vc_dispmanx_rect_set (&rect, 0, 0, win->img_w, win->img_h);
	if ( vc_dispmanx_resource_write_data (win->resImg,
                                          VC_IMAGE_8BPP,
                                          iWth16,
                                          win->image,
                                          &rect) != 0 )
		fatal ("Failed to transfer data for window %d", win->iWin);
	diag_message (DIAG_WIN_HW, "Created VC resource for window %d", win->iWin);
	win_swap (win);

	return (WIN *) win;
	}
/*...e*/

/*...swin_delete:0:*/
void win_term (void)
	{
	diag_message (DIAG_WIN_HW, "win_term");
#ifndef	NOKBD
	if ( tty_init )
		{
		tcsetattr (ttyfd, TCSAFLUSH, &tty_attr_old);
        diag_message (DIAG_WIN_HW, "old_keyboard_mode = %d", old_keyboard_mode);
		ioctl (ttyfd, KDSKBMODE, old_keyboard_mode);
		tty_init = FALSE;
        diag_message (DIAG_WIN_HW, "Restored keyboard mode");
        tty_init = 0;
		}
#endif
	if ( gpu_mode == 3 )	vc_restore ();
	}

void vcd_delete (VCD *vcd)
	{
	WIN_PRIV *win = vcd->winAct;
	DISPMANX_UPDATE_HANDLE_T update;
	diag_message (DIAG_WIN_HW, "Delete display %d", vcd->iDisplay);

	//	Refresh display
	update = vc_dispmanx_update_start (0);
	if ( update == 0 ) fatal ("Failed to start display update");

	if ( vc_dispmanx_element_remove (update, win->eleImg) != 0 )
		fatal ("Failed to remove window image element");
	win->eleImg = 0;
	diag_message (DIAG_WIN_HW, "Removed VC element for window %d", win->iWin);

	if ( vc_dispmanx_element_remove (update, vcd->eleBkGd) != 0 )
		fatal ("Failed to remove display background element");
	vcd->eleBkGd = 0;
	diag_message (DIAG_WIN_HW, "Removed VC background element for display %d", vcd->iDisplay);

	if ( vc_dispmanx_update_submit_sync (update) != 0 )
		fatal ("Failed to complete window update");

    if ( vc_dispmanx_resource_delete (vcd->resBkGd) != 0 )
		fatal ("Failed to delete background resource for display %d", vcd->iDisplay);
	vcd->resBkGd = 0;
	diag_message (DIAG_WIN_HW, "Deleted background resource for display %d", vcd->iDisplay);

	free (vcd);
	}

void win_delete (WIN *win_pub)
	{
	if ( win_pub == NULL ) return;
	WIN_PRIV *win = (WIN_PRIV *) win_pub;
	VCD *vcd = win->vcd;
	int iWin = win->iWin;
	diag_message (DIAG_WIN_HW, "Delete window %d", iWin);
	if ( --vcd->nWin > 0 )
		{
		if ( vcd->winAct == win )
			{
			int i;
			for ( i = 0; i < n_wins; ++i )
				{
				if ( ( wins[i] != win ) && ( wins[i]->vcd == vcd ) )
					{
					win_swap (wins[i]);
					break;
					}
				}
			}
		}
	else
		{
		vcd_delete (vcd);
		}

    if ( vc_dispmanx_resource_delete (win->resImg) != 0 )
		fatal ("Failed to delete image resource for window %d", win->iWin);
	win->resImg = 0;
	diag_message (DIAG_WIN_HW, "Deleted image resource for window %d", win->iWin);

	wins[iWin] = wins[--n_wins];
	wins[iWin]->iWin  =  iWin;
	if ( win->image != win->data )
		{
		free (win->image);
		diag_message (DIAG_WIN_HW, "Freed upscaling buffer");
		}
	free (win->data);
	free (win->cols);
	free (win);
	if ( ! n_wins )
		{
		win_term ();
		}
	}
/*...e*/

/*...swin_map_key:0:*/
typedef struct
	{
	unsigned char ks;
	int wk;
	const char *ps;
	} MAPKS;

static MAPKS win_mapks[] =
	{
	{KEY_ENTER,         WK_Return,               "Enter"},
	{KEY_BACKSPACE,     WK_BackSpace,            "Back Space"},
	{KEY_LEFT,          WK_Left,                 "Left Arrow"},
	{KEY_RIGHT,         WK_Right,                "Right Arrow"},
	{KEY_UP,            WK_Up,                   "Up Arrow"},
	{KEY_DOWN,          WK_Down,                 "Down Arrow"},
	{KEY_PAGEUP,        WK_Page_Up,              "Page Up"},
	{KEY_PAGEDOWN,      WK_Page_Down,            "Page Down"},
	{KEY_HOME,          WK_Home,                 "Home"},
	{KEY_END,           WK_End,                  "End"},
	{KEY_INSERT,        WK_Insert,               "Insert"},
	{KEY_DELETE,        WK_Delete,               "Delete"},
	{KEY_PAUSE,         WK_Pause,                "Pause"},
	{KEY_SCROLLLOCK,    WK_Scroll_Lock,          "Scroll Lock"},
	{KEY_SYSRQ,         WK_Sys_Req,              "Sys Req"},
	{KEY_LEFTSHIFT,     WK_Shift_L,              "Left Shift"},
	{KEY_RIGHTSHIFT,    WK_Shift_R,              "Right Shift"},
	{KEY_LEFTCTRL,      WK_Control_L,            "Left Control"},
	{KEY_RIGHTCTRL,     WK_Control_R,            "Right Control"},
	{KEY_CAPSLOCK,      WK_Caps_Lock,            "Caps Lock"},
	{KEY_NUMLOCK,       WK_Num_Lock,             "Num Lock"},
	{KEY_F1,            WK_F1,                   "F1"},
	{KEY_F2,            WK_F2,                   "F2"},
	{KEY_F3,            WK_F3,                   "F3"},
	{KEY_F4,            WK_F4,                   "F4"},
	{KEY_F5,            WK_F5,                   "F5"},
	{KEY_F6,            WK_F6,                   "F6"},
	{KEY_F7,            WK_F7,                   "F7"},
	{KEY_F8,            WK_F8,                   "F8"},
	{KEY_F9,            WK_F9,                   "F9"},
	{KEY_F10,           WK_F10,                  "F10"},
	{KEY_F11,           WK_F11,                  "F11"},
	{KEY_F12,           WK_F12,                  "F12"},
	{KEY_KP4,           WK_KP_Left,              "Keypad 4 (Left)"},
	{KEY_KP6,           WK_KP_Right,             "Keypad 6 (Right)"},
	{KEY_KP8,           WK_KP_Up,                "Keypad 8 (Up)"},
	{KEY_KP2,           WK_KP_Down,              "Keypad 2 (Down)"},
	{KEY_KP9,           WK_KP_Page_Up,           "Keypad 9 (Page Up)"},
	{KEY_KP3,           WK_KP_Page_Down,         "Keypad 3 (Page Down)"},
	{KEY_KP7,           WK_KP_Home,              "Keypad 7 (Home)"},
	{KEY_KP1,           WK_KP_End,               "Keypad 1 (End)"},
	{KEY_KPPLUS,        WK_KP_Add,               "Keypad +"},
	{KEY_KPMINUS,       WK_KP_Subtract,          "Keypad -"},
	{KEY_KPASTERISK,    WK_KP_Multiply,          "Keypad *"},
	{KEY_KPSLASH,       WK_KP_Divide,            "Keypad /"},
	{KEY_KPENTER,       WK_KP_Enter,             "Keypad Enter"},
	{KEY_KP5,           WK_KP_Middle,            "Keypad 5 (Middle)"},
	{KEY_LEFTMETA,      WK_PC_Windows_L,         "Windows Left"}, /* WK_Mac_Cmd_L */
	{KEY_RIGHTMETA,     WK_PC_Windows_R,         "Windows Right"}, /* WK_Mac_Cmd_R */
	{KEY_LEFTALT,       WK_PC_Alt_L,             "Left Alt"},
	{KEY_RIGHTALT,      WK_PC_Alt_R,             "Right Alt"},
	{KEY_MENU,          WK_PC_Menu,              "Menu"},
/*      {KEY_MODE,          WK_Mac_Alt},     /* Alt Gr */
	};

static int win_map_key (unsigned char ks)
	{
#ifndef	NOKBD
	struct kbentry kbe;
	int  i;

	// Special keys.
	for ( i = 0; i < sizeof(win_mapks)/sizeof(win_mapks[0]); i++ )
		{
		if ( ks == win_mapks[i].ks )
			{
			diag_message (DIAG_KBD_HW, "Mapped special key 0x%02x to %s", (int) ks, win_mapks[i].ps);
			return win_mapks[i].wk;
			}
		}

	// Have to deal with shift 3 (Pound sign) as a special case on UK keyboards.
	if ( ( ks == KEY_3 ) && ( mod_keys & ( MKY_LSHIFT | MKY_RSHIFT ) )
		&& ( ( mod_keys & ( MKY_LCTRL | MKY_RCTRL | MKY_LALT | MKY_RALT ) ) == 0 ) )
		{
		diag_message (DIAG_KBD_HW, "Mapped UK keyboard key 0x%02x to '#'", (int) ks);
		int   key   =  '#';
		return   key;
		}

	// Use keyboard mapping.
	kbe.kb_table   =  K_NORMTAB;
	if ( mod_keys & ( MKY_LSHIFT | MKY_RSHIFT ) )   kbe.kb_table   |= K_SHIFTTAB;
	if ( mod_keys & ( MKY_LALT | MKY_RALT ) )       kbe.kb_table   |= K_ALTTAB;
	kbe.kb_index   =  ks;
	kbe.kb_value   =  0;
	if ( ioctl (ttyfd, KDGKBENT, &kbe) >= 0 )
		{
		int   type  =  kbe.kb_value >> 8;
		int   key   =  kbe.kb_value & 0xff;
		if ( ( type == 0x00 ) || ( type == 0x0b ) )
			{
			// if ( ( type == 0x0b ) && ( mod_keys & MKY_CAPSLK ) )  key   ^= 0x20;
			diag_message (DIAG_KBD_HW, "Mapped key 0x%02x to '%c' 0x%02x", (int) ks,
				( ( key >= 0x20 ) && ( key < 0x7f ) ) ? ((char) key) : '.',
				key);
			return   key;
			}
		diag_message (DIAG_KBD_HW, "Key 0x%02x is type 0x%02x code 0x%02x", (int) ks, type, key);
		}
	diag_message (DIAG_KBD_HW, "Can't map ks = 0x%02x", ks);
#endif
	return   -1;
	}

/*...e*/

/*...swin_shifted_wk:0:*/
/* Keys of the host keyboard have an unshifted label and a shifted label
   written on them, eg: unshifted "1", shifted "!". Alphabetic keys typically
   omit the unshifted lowercase letter, but notionally it is there.
   This module returns WK_ values with names which reflect unshifted label.
   Sometimes the module user will want to know the equivelent shifted label.

   The problem with this code is that it assumes the UK keyboard layout. */

int win_shifted_wk(int wk)
	{
	// This version of the code actually returns the shifted codes. No further conversion is necessary.
	return   wk;
#if (FALSE)
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
		case '\'':   return '@';
		case '#':   return '~';
		case '\\':   return '|';
		case ',':   return '<';
		case '.':   return '>';
		case '/':   return '?';
		default:   return ( wk >= 0 && wk < 0x100 ) ? wk : -1;
		}
#endif
	}
/*...e*/

static char * ListModifiers (void)
	{
	static char sMods[19];
	sMods[0] =  '\0';
	if ( mod_keys == 0 ) strcpy (sMods, " None");
	if ( mod_keys & MKY_LSHIFT )  strcat (sMods, " LS");
	if ( mod_keys & MKY_RSHIFT )  strcat (sMods, " RS");
	if ( mod_keys & MKY_LCTRL )   strcat (sMods, " LC");
	if ( mod_keys & MKY_RCTRL )   strcat (sMods, " RC");
	if ( mod_keys & MKY_LALT )    strcat (sMods, " LA");
	if ( mod_keys & MKY_RALT )    strcat (sMods, " RA");
	if ( mod_keys & MKY_CAPSLK )  strcat (sMods, " CL");
	return   sMods;
	}

/*...swin_handle_events:0:*/

#ifdef	 ALT_HANDLE_EVENTS
extern BOOLEAN ALT_HANDLE_EVENTS (WIN *);
#endif

void win_handle_events()
	{
	unsigned char  key = 0;

#ifdef	 ALT_HANDLE_EVENTS
	if ( ALT_HANDLE_EVENTS ((WIN *) wins[iActiveWin]) )	 return;
#endif
#ifndef NOKBD
	while ( read (ttyfd, &key, 1) > 0 )
		{
		if ( ( key & 0x80 ) == 0 )
			{
			// Key press.
			diag_message (DIAG_KBD_HW, "Key down event: 0x%02x, Modifiers:%s", key, ListModifiers ());

			// Modifier keys.
			if ( key == KEY_LEFTSHIFT )         mod_keys |= MKY_LSHIFT;
			else if ( key == KEY_RIGHTSHIFT )   mod_keys |= MKY_RSHIFT;
			else if ( key == KEY_LEFTCTRL )     mod_keys |= MKY_LCTRL;
			else if ( key == KEY_RIGHTCTRL )    mod_keys |= MKY_RCTRL;
			else if ( key == KEY_LEFTALT )      mod_keys |= MKY_LALT;
			else if ( key == KEY_RIGHTALT )     mod_keys |= MKY_RALT;
			else if ( key == KEY_CAPSLOCK )     mod_keys |= MKY_CAPSLK;

			// Select window.
			if ( ( mod_keys & MKY_LCTRL ) && ( key >= KEY_F1 ) && ( key <= KEY_F8 ) )
				{
				if ( ( key - KEY_F1 ) < n_wins )  win_swap (wins[key - KEY_F1]);
				return;
				}

			// Exit on ctrl+break.

			if ( ( key == KEY_PAUSE ) && ( mod_keys & MKY_LCTRL ) )
				{
				diag_message (DIAG_KBD_HW, "Ctrl+Break keys pressed.");
				terminate ("Ctrl+Break keys pressed.");
				}

			// Process key press.
			wins[iActiveWin]->keypress (win_map_key (key));
			}
		else
			{
			// Key release.
			key &= 0x7f;
			diag_message (DIAG_KBD_HW, "Key up event: 0x%x, Modifiers:%02s", key, ListModifiers ());

			// Modifier keys.
			if ( key == KEY_LEFTSHIFT )         mod_keys &= ~MKY_LSHIFT;
			else if ( key == KEY_RIGHTSHIFT )   mod_keys &= ~MKY_RSHIFT;
			else if ( key == KEY_LEFTCTRL )     mod_keys &= ~MKY_LCTRL;
			else if ( key == KEY_RIGHTCTRL )    mod_keys &= ~MKY_RCTRL;
			else if ( key == KEY_LEFTALT )      mod_keys &= ~MKY_LALT;
			else if ( key == KEY_RIGHTALT )     mod_keys &= ~MKY_RALT;
			else if ( key == KEY_CAPSLOCK )     mod_keys &= ~MKY_CAPSLK;

			// Process key release.
			wins[iActiveWin]->keyrelease (win_map_key (key));
			}
		}
#endif
	}   
/*...e*/

#define KEYB_LED_CAPS_LOCK      0x04
#define KEYB_LED_NUM_LOCK       0x02
#define KEYB_LED_SCROLL_LOCK    0x01

void win_kbd_leds (BOOLEAN bCaps, BOOLEAN bNum, BOOLEAN bScroll)
    {
    static unsigned int uLast = 0xFF;
    unsigned int uLeds = 0;
    if ( bCaps )    uLeds |= KEYB_LED_CAPS_LOCK;
    if ( bNum )     uLeds |= KEYB_LED_NUM_LOCK;
    if ( bScroll )  uLeds |= KEYB_LED_SCROLL_LOCK;
    if ( uLeds != uLast ) ioctl (ttyfd, KDSETLED, &uLeds);
    uLast = uLeds;
    }
