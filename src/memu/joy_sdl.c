/*

joy_sdl.c - Joystick via SDL

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>

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
static int joy_central = 0; /* Its as if Linux driver is already doing this */

#ifdef HAVE_JOY

#define MAX_JOY     4
static int n_joy = 0;
SDL_Joystick *joy_dev[MAX_JOY];
SDL_JoystickID joy_id[MAX_JOY];
static int joy_fst_axis[MAX_JOY];
static int joy_fst_button[MAX_JOY];
#define MAX_AXES    8
static int joy_n_axes = 0;
static int joy_up_row[MAX_AXES]   , joy_up_bitpos[MAX_AXES];
static int joy_down_row[MAX_AXES] , joy_down_bitpos[MAX_AXES];
#define	MAX_BUTTONS 16
static int joy_n_buttons = 0;
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
/* Expressed as a percentage */

void joy_set_central(int central)
	{
	joy_central = central * 32000 / 100;
	}
/*...e*/

/*...sjoy_periodic:0:*/
#ifdef HAVE_JOY
/*...sjoy_button:0:*/
#ifdef ALT_KEYPRESS
BOOLEAN ALT_KEYPRESS (int wk);
#endif

static void joy_button(int row, int bitpos, BOOLEAN press)
	{
    diag_message(DIAG_JOY_USAGE, "joy_button (%d, %d, %d)", row, bitpos, press);
    if ( row >= 0 )
        {
        if ( press )
            kbd_grid_press(row, bitpos);
        else
            kbd_grid_release(row, bitpos);
        }
#ifdef ALT_KEYPRESS
    else if ( press && ( row == -2 ) )
        {
        ALT_KEYPRESS (bitpos);
        }
#endif
	}
/*...e*/
#endif

void joy_periodic(void)
	{
    }

static int joy_index (SDL_JoystickID id)
    {
    for (int i = 0; i < n_joy; ++i)
        {
        if (id == joy_id[i]) return i;
        }
    return -1;
    }

void joy_handle_events (SDL_Event *e)
    {
#ifdef HAVE_JOY
    int idx;
	if ( joy_emu & JOYEMU_JOY )
		{
        switch (e->type)
            {
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                idx = joy_index (e->jaxis.which);
                if (idx < 0) break;
                int axis = joy_fst_axis[idx] + e->jaxis.axis;
                if (axis >= joy_n_axes) break;
                joy_button(joy_up_row[axis]   , joy_up_bitpos[axis],
                    e->jaxis.value < -joy_central);
                joy_button(joy_down_row[axis] , joy_down_bitpos[axis],
                    e->jaxis.value >  joy_central);
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                idx = joy_index (e->jbutton.which);
                if (idx < 0) break;
                int button = joy_fst_button[idx] + e->jbutton.button;
                if (button >= joy_n_buttons) break;
                joy_button(joy_buttons_row[button], joy_buttons_bitpos[button],
                    e->jbutton.down);
                break;
			}
		}
#endif
	}
/*...e*/

int joy_int (const char **pps)
    {
    int iVal = 0;
    while ( (**pps >= '0' ) && (**pps <= '9') )
        {
        iVal = 10 * iVal + (**pps - '0');
        ++(*pps);
        }
    return iVal;
    }

/*...sjoy_init:0:*/
void joy_init(int emu)
	{
#ifdef HAVE_JOY
	if ( emu & JOYEMU_JOY )
		{
		unsigned char axes = 0;
		unsigned char buttons = 0;
		const char *p;
		BOOLEAN shifted;
        SDL_InitSubSystem (SDL_INIT_JOYSTICK);
        int n_joy;
        SDL_JoystickID *pid = SDL_GetJoysticks (&n_joy);
        if ( n_joy < 1 ) fatal ("No joystick found");
        if (n_joy > MAX_JOY) n_joy = MAX_JOY;
        for (int i = 0; i < n_joy; ++i)
            {
            joy_id[i] = pid[i];
            joy_dev[i] = SDL_OpenJoystick (joy_id[i]);
            joy_fst_axis[i] = axes;
            joy_fst_button[i] = buttons;
            axes += SDL_GetNumJoystickAxes (joy_dev[i]);
            buttons += SDL_GetNumJoystickButtons (joy_dev[i]);
            }
        SDL_free (pid);
		if ( axes < 2 )
			{
            joy_term ();
			fatal("joystick needs to support at least 2 axes");
			}
        else if ( axes > MAX_AXES )
            {
            axes = MAX_AXES;
            }
		if ( buttons < 1 )
			{
            joy_term ();
			fatal("joystick needs to support at least 1 button");
			}
		diag_message(DIAG_JOY_INIT, "found %d joystick(s) with %d axes and %d buttons",
            n_joy, axes, buttons);
        for (int i = 0; i < MAX_AXES; ++i)
            {
            joy_up_row[i] = -1;
            joy_down_row[i] = -1;
            }
        for (int i = 0; i < MAX_BUTTONS; ++i) joy_buttons_row[i] = -1;
        p = joy_buttons;
        char *buf = NULL;
        if ( joy_buttons[0] == '@' )
            {
            // Configuration from a file
            FILE *fp = efopen(&joy_buttons[1], "rb");
            fseek(fp, 0, SEEK_END);
            size_t length = ftell(fp);
            buf = emalloc (length + 1);
            fseek(fp, 0, SEEK_SET);
            fread(buf, 1, length, fp);
            fclose(fp);
            buf[length] = '\0';
            p = buf;
            }
        if ( strchr (p, '=') == NULL )
            {
            // Old style configuration
            if ( kbd_find_grid(&p, &joy_up_row[0]    , &joy_up_bitpos[0]     , &shifted) &&
                kbd_find_grid(&p, &joy_down_row[0]   , &joy_down_bitpos[0]   , &shifted) &&
                kbd_find_grid(&p, &joy_up_row[1]     , &joy_up_bitpos[1]     , &shifted) &&
                kbd_find_grid(&p, &joy_down_row[1]   , &joy_down_bitpos[1]   , &shifted) &&
                kbd_find_grid(&p, &joy_buttons_row[0], &joy_buttons_bitpos[0], &shifted) )
                ;
            else
                {
                joy_term ();
                fatal("can't parse joystick buttons: %s", joy_buttons);
                }
            joy_n_buttons = 1;
            while ( *p != '\0' && joy_n_buttons < MAX_BUTTONS && joy_n_buttons < buttons )
                {
                if ( ! kbd_find_grid(&p, &joy_buttons_row[joy_n_buttons], &joy_buttons_bitpos[joy_n_buttons], &shifted) )
                    fatal("can't parse joystick extra buttons: \"%s\"", joy_buttons);
                ++joy_n_buttons;
                }
            diag_message(DIAG_JOY_INIT, "%d axis central region, %d buttons configured to press keys",
                joy_central, joy_n_buttons);
            }
        else
            {
            // New style configuration
            joy_n_buttons = 1;
            while ( *p )
                {
                if ( *p < ' ' )
                    {
                    ++p;
                    }
                else if ( *p == 'A' )
                    {
                    ++p;
                    int axis = joy_int (&p);
                    if ( axis >= MAX_AXES ) fatal ("Joystick axis %d more than %d", axis, MAX_AXES);
                    if ( *p != '=' ) fatal ("Invalid joystick configuration syntax");
                    ++p;
                    if ( (! kbd_find_grid(&p, &joy_up_row[axis],   &joy_up_bitpos[axis],   &shifted) ) ||
                         (! kbd_find_grid(&p, &joy_down_row[axis], &joy_down_bitpos[axis], &shifted) ) )
                        fatal ("can't parse keys for joystick axis %d", axis);
                    diag_message(DIAG_JOY_INIT, "Joystick axis %d up:   row = %d, bitpos = %d",
                        axis, joy_up_row[axis], joy_up_bitpos[axis]);
                    diag_message(DIAG_JOY_INIT, "Joystick axis %d down: row = %d, bitpos = %d",
                        axis, joy_down_row[axis], joy_down_bitpos[axis]);
                    if (axis >= joy_n_axes) joy_n_axes = axis + 1;
                    }
                else if ( *p == 'B' )
                    {
                    ++p;
                    int btn = joy_int (&p);
                    if ( btn >= MAX_BUTTONS ) fatal ("Joystick button %d more than %d", btn, MAX_BUTTONS);
                    if ( *p != '=' ) fatal ("Invalid joystick configuration syntax");
                    ++p;
                    if ( ! kbd_find_grid(&p, &joy_buttons_row[btn], &joy_buttons_bitpos[btn], &shifted) )
                        fatal ("can't parse key for joystick button %d", btn);
                    diag_message(DIAG_JOY_INIT, "Joystick button %d:   row = %d, bitpos = %d",
                        btn, joy_buttons_row[btn], joy_buttons_bitpos[btn]);
                    if ( btn >= joy_n_buttons ) joy_n_buttons = btn + 1;
                    }
                }
            }
		}
	joy_emu = emu;
#endif
	}
/*...e*/
/*...sjoy_term:0:*/
void joy_term(void)
	{
#ifdef HAVE_JOY
	if ( joy_emu & JOYEMU_JOY )
        {
        for (int i = 0; i < n_joy; ++i)
            {
            SDL_CloseJoystick (joy_dev[i]);
            joy_dev[i] = NULL;
            joy_id[i] = -1;
            }
        n_joy = 0;
        }
	joy_emu = 0;
#endif
	}
/*...e*/
