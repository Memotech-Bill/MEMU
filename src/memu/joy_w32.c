/*

joy.c - Windows Joystick

Uses DirectInput8.
See http://www.yaldex.com/games-programming/0672323699_ch09lev1sec2.html

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#ifndef NO_JOY
#define DIRECTINPUT_VERSION 0x0800
#define	BOOLEAN BOOLEANx
#define	INITGUID
#include <objbase.h>
#include <dinput.h>
#undef BOOLEAN
#endif

#include "types.h"
#include "diag.h"
#include "common.h"
#include "kbd.h"
#include "joy.h"

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vkbd\46\h:0:*/
/*...vjoy\46\h:0:*/
/*...e*/

/*...svars:0:*/
static int joy_emu = 0;
static const char *joy_buttons = "<LEFT><RIGHT><UP><DOWN><HOME> ";
static int joy_central = 100; /* 1% dead-zone */

#ifndef NO_JOY

static LPDIRECTINPUT8       joy_lpdi = NULL;
static LPDIRECTINPUTDEVICE8 joy_lpdid = NULL;

static int joy_left_row , joy_left_bitpos;
static int joy_right_row, joy_right_bitpos;
static int joy_up_row   , joy_up_bitpos;
static int joy_down_row , joy_down_bitpos;
#define	MAX_BUTTONS 32 /* matches limit in DIJOYSTATE */
static int joy_n_buttons;
static int joy_buttons_row[MAX_BUTTONS], joy_buttons_bitpos[MAX_BUTTONS];

#endif
/*...e*/

/*...sjoy_set_buttons:0:*/
void joy_set_buttons(const char *buttons)
	{
	joy_buttons = buttons;
	}
/*...e*/
/*...sjoy_set_central:0:*/
/* Passed in to this function as a percentage.
   Passed to DirectInput in 1/100th of a percent. */

void joy_set_central(int central)
	{
	joy_central = central * 10000 / 100;
	}
/*...e*/

/*...sjoy_periodic:0:*/
#ifndef NO_JOY
/*...sjoy_button:0:*/
static void joy_button(int row, int bitpos, BOOLEAN press)
	{
	if ( press )
		kbd_grid_press(row, bitpos);
	else
		kbd_grid_release(row, bitpos);
	}
/*...e*/
#endif

void joy_periodic(void)
	{
#ifndef NO_JOY
	if ( joy_emu & JOYEMU_JOY )
		{
		DIJOYSTATE js;
		HRESULT hr;
		while ( (hr = IDirectInputDevice8_GetDeviceState(
				joy_lpdid,
				sizeof(js),
				&js)) == DIERR_INPUTLOST )
			{
			if ( FAILED(hr = IDirectInputDevice8_Acquire(joy_lpdid)) )
				break;
			diag_message(DIAG_JOY_INIT, "reacquired joystick");
			}
		if ( FAILED(hr) )
			diag_message(DIAG_JOY_USAGE, "can't get device state, hr=0x%08x", (unsigned) hr);
		else
			{
			int i;
			diag_message(DIAG_JOY_USAGE, "got joystick device state: lX=%ld, lY=%ld, rgbButton[0]=0x%02x", js.lX, js.lY, js.rgbButtons[0]);
			joy_button(joy_left_row , joy_left_bitpos , js.lX < 0);
			joy_button(joy_right_row, joy_right_bitpos, js.lX > 0);
			joy_button(joy_up_row   , joy_up_bitpos   , js.lY < 0);
			joy_button(joy_down_row , joy_down_bitpos , js.lY > 0);
			for ( i = 0; i < joy_n_buttons; i++ )
				joy_button(joy_buttons_row[i], joy_buttons_bitpos[i], (js.rgbButtons[i]&0x80) != 0);
			}
		}
#endif
	}
/*...e*/

/*...sjoy_init:0:*/
#ifndef NO_JOY
static BOOLEAN joy_found = FALSE;
static GUID joy_guid;
static char joy_product_name[MAX_PATH];

static BOOL CALLBACK joy_enum_callback(LPCDIDEVICEINSTANCE lpddi, LPVOID *pv)
	{
	joy_found = TRUE;
	joy_guid = lpddi->guidInstance;
	strcpy(joy_product_name, lpddi->tszProductName);
	return DIENUM_STOP;
	}
#endif

void joy_init(int emu)
	{
#ifndef NO_JOY
	if ( emu & JOYEMU_JOY )
		{
		HINSTANCE hInst = GetModuleHandle(NULL);
		DIDEVCAPS caps;
		DIPROPRANGE axis_range;
		DIPROPDWORD dead_zone;
		const char *p;
		BOOLEAN shifted;
		HRESULT hr;
		if ( FAILED(hr = DirectInput8Create(
				hInst,
				DIRECTINPUT_VERSION,
				&IID_IDirectInput8, /* & as its a ref */
				(void **) &joy_lpdi,
				NULL)) )
			fatal("can't create interface to DirectInput, hr=0x%08x", (unsigned) hr); 
		if ( FAILED(hr = IDirectInput8_EnumDevices(
				joy_lpdi,
				DI8DEVCLASS_GAMECTRL, /* supersedes DIDEVTYPE_JOYSTICK */
				joy_enum_callback,
				NULL, /* pv */
				DIEDFL_ATTACHEDONLY)) )
			{
			IDirectInput8_Release(joy_lpdi);
			fatal("can't enumerate attached joysticks, hr=0x%08x", (unsigned) hr);
			}
		if ( !joy_found )
			{
			IDirectInput8_Release(joy_lpdi);
			fatal("couldn't find an attached joystick");
			}
		if ( FAILED(hr = IDirectInput8_CreateDevice(
				joy_lpdi,
				&joy_guid, /* & as its a ref */
				&joy_lpdid,
				NULL)) )
			{
			IDirectInput8_Release(joy_lpdi);
			fatal("can't access joystick, hr=0x%08x", (unsigned) hr);
			}
		caps.dwSize = sizeof(DIDEVCAPS);
		if ( FAILED(hr = IDirectInputDevice8_GetCapabilities(
				joy_lpdid,
				&caps)) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't get joystick device capabilities, hr=0x%08x", (unsigned) hr);
			}
		if ( caps.dwAxes < 2 )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("joystick needs to support at least 2 axes");
			}
		if ( caps.dwButtons < 1 )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("joystick needs to support at least 1 button");
			}
		diag_message(DIAG_JOY_INIT, "found \"%s\" joystick with %d axes and %d buttons", joy_product_name, (int) caps.dwAxes, (int) caps.dwButtons);
#if 0
		/* Sample code on the net talks about doing this
		   but MEMU is a console application, not particularly tied
		   to a single window, so I don't (yet) have a hWnd. */
		if ( FAILED(hr = IDirectInputDevice8_SetCooperativeLevel(
				joy_lpdid,
				hWnd,
				DISCL_BACKGROUND|DISCL_NONEXCLUSIVE)) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't set joystick cooperation level, hr=0x%08x", (unsigned) hr);
			}
#endif
		if ( FAILED(hr = IDirectInputDevice8_SetDataFormat(
				joy_lpdid,
				&c_dfDIJoystick)) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't set joystick data format, hr=0x%08x", (unsigned) hr);
			}
		axis_range.lMin              = -1024;
		axis_range.lMax              =  1024;
		axis_range.diph.dwSize       = sizeof(DIPROPRANGE);
		axis_range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		axis_range.diph.dwHow        = DIPH_BYOFFSET;
		axis_range.diph.dwObj        = DIJOFS_X;
		if ( FAILED(hr = IDirectInputDevice8_SetProperty(
				joy_lpdid,
				DIPROP_RANGE,
				&(axis_range.diph))) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't set joystick X axis range, hr=0x%08x", (unsigned) hr);
			}
		axis_range.diph.dwObj        = DIJOFS_Y;
		if ( FAILED(hr = IDirectInputDevice8_SetProperty(
				joy_lpdid,
				DIPROP_RANGE,
				&(axis_range.diph))) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't set joystick Y axis range, hr=0x%08x", (unsigned) hr);
			}
		dead_zone.dwData            = joy_central;
		dead_zone.diph.dwSize       = sizeof(DIPROPDWORD);
		dead_zone.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		dead_zone.diph.dwHow        = DIPH_BYOFFSET;
		dead_zone.diph.dwObj        = DIJOFS_X;
		if ( FAILED(hr = IDirectInputDevice8_SetProperty(
				joy_lpdid,
				DIPROP_DEADZONE,
				&(dead_zone.diph))) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't set joystick X dead zone, hr=0x%08x", (unsigned) hr);
			}
		dead_zone.diph.dwObj        = DIJOFS_Y;
		if ( FAILED(hr = IDirectInputDevice8_SetProperty(
				joy_lpdid,
				DIPROP_DEADZONE,
				&(dead_zone.diph))) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't set joystick Y dead zone, hr=0x%08x", (unsigned) hr);
			}
		if ( FAILED(hr = IDirectInputDevice8_Acquire(joy_lpdid)) )
			{
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't acquire the joystick, hr=0x%08x", (unsigned) hr);
			}
		p = joy_buttons;
		if ( kbd_find_grid(&p, &joy_left_row      , &joy_left_bitpos      , &shifted) &&
		     kbd_find_grid(&p, &joy_right_row     , &joy_right_bitpos     , &shifted) &&
		     kbd_find_grid(&p, &joy_up_row        , &joy_up_bitpos        , &shifted) &&
		     kbd_find_grid(&p, &joy_down_row      , &joy_down_bitpos      , &shifted) &&
		     kbd_find_grid(&p, &joy_buttons_row[0], &joy_buttons_bitpos[0], &shifted) )
			;
		else
			{
			IDirectInputDevice8_Unacquire(joy_lpdid);
			IDirectInputDevice8_Release(joy_lpdid);
			IDirectInput8_Release(joy_lpdi);
			fatal("can't parse joystick buttons: %s", joy_buttons);
			}
		joy_n_buttons = 1;
		while ( *p != '\0' && joy_n_buttons < MAX_BUTTONS && joy_n_buttons < (int) caps.dwButtons )
			{
			if ( ! kbd_find_grid(&p, &joy_buttons_row[joy_n_buttons], &joy_buttons_bitpos[joy_n_buttons], &shifted) )
				{
				IDirectInputDevice8_Unacquire(joy_lpdid);
				IDirectInputDevice8_Release(joy_lpdid);
				IDirectInput8_Release(joy_lpdi);
				fatal("can't parse joystick extra buttons: \"%s\"", joy_buttons);
				}
			++joy_n_buttons;
			}
		diag_message(DIAG_JOY_INIT, "%d axis central region, %d buttons configured to press keys", joy_central, joy_n_buttons);
		}
	joy_emu = emu;
#endif
	}
/*...e*/
/*...sjoy_term:0:*/
void joy_term(void)
	{
#ifndef NO_JOY
	if ( joy_emu & JOYEMU_JOY )
		{
		IDirectInputDevice8_Unacquire(joy_lpdid);
		IDirectInputDevice8_Release(joy_lpdid);
		IDirectInput8_Release(joy_lpdi);
		}
	joy_emu = 0;
#endif
	}
/*...e*/
