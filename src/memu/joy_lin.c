/*

joy.c - Linux Joystick

When compiling for Linux, we can use the Linux Joystick Driver, see
http://atrey.karlin.mff.cuni.cz/~vojtech/joystick/

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#ifdef HAVE_JOY
#include <linux/joystick.h>
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
static int joy_central = 0; /* Its as if Linux driver is already doing this */

#ifdef HAVE_JOY

static int joy_fd;
#define MAX_AXES    8
// static int joy_left_row , joy_left_bitpos;
// static int joy_right_row, joy_right_bitpos;
static int joy_up_row[MAX_AXES]   , joy_up_bitpos[MAX_AXES];
static int joy_down_row[MAX_AXES] , joy_down_bitpos[MAX_AXES];
#define	MAX_BUTTONS 16
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
#ifdef HAVE_JOY
	if ( joy_emu & JOYEMU_JOY )
		{
		struct js_event ev;
		while ( read(joy_fd, &ev, sizeof(ev)) > 0 )
			{
			diag_message(DIAG_JOY_USAGE, "joystick event type=%d, number=%d, value=%d",
                ev.type, ev.number, ev.value);
			if ( ev.type & JS_EVENT_INIT )
				; /* Ignore this */
			else
				switch ( ev.type )
					{
					case JS_EVENT_AXIS:
                        /*
						switch( ev.number )
							{
							case 0:
								joy_button(joy_left_row , joy_left_bitpos , ev.value < -joy_central);
								joy_button(joy_right_row, joy_right_bitpos, ev.value >  joy_central);
								break;
							case 1:
								joy_button(joy_up_row   , joy_up_bitpos   , ev.value < -joy_central);
								joy_button(joy_down_row , joy_down_bitpos , ev.value >  joy_central);
								break;
							}
                        */
                        joy_button(joy_up_row[ev.number]   , joy_up_bitpos[ev.number],
                            ev.value < -joy_central);
                        joy_button(joy_down_row[ev.number] , joy_down_bitpos[ev.number],
                            ev.value >  joy_central);
						break;
					case JS_EVENT_BUTTON:
						if ( ev.number < joy_n_buttons )
							joy_button(joy_buttons_row[ev.number], joy_buttons_bitpos[ev.number],
                                ev.value != 0);
						break;
					}
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
		unsigned char axes;
		unsigned char buttons;
		char name[100];
		const char *p;
		BOOLEAN shifted;
		if ( (joy_fd = open("/dev/input/js0", O_RDONLY|O_NONBLOCK)) == -1 )
			fatal("can't open Linux joystick /dev/input/js0");
		ioctl(joy_fd, JSIOCGAXES, &axes);
		if ( axes < 2 )
			{
			close(joy_fd);
			fatal("joystick needs to support at least 2 axes");
			}
		ioctl(joy_fd, JSIOCGBUTTONS, &buttons);
		if ( buttons < 1 )
			{
			close(joy_fd);
			fatal("joystick needs to support at least 1 button");
			}
		ioctl(joy_fd, JSIOCGNAME(sizeof(name)), name);
		diag_message(DIAG_JOY_INIT, "found Linux \"%s\" joystick with %d axes and %d buttons",
            name, axes, buttons);
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
            /*
            if ( kbd_find_grid(&p, &joy_left_row      , &joy_left_bitpos      , &shifted) &&
                kbd_find_grid(&p, &joy_right_row     , &joy_right_bitpos     , &shifted) &&
                kbd_find_grid(&p, &joy_up_row        , &joy_up_bitpos        , &shifted) &&
                kbd_find_grid(&p, &joy_down_row      , &joy_down_bitpos      , &shifted) &&
                kbd_find_grid(&p, &joy_buttons_row[0], &joy_buttons_bitpos[0], &shifted) )
            */
            if ( kbd_find_grid(&p, &joy_up_row[0]    , &joy_up_bitpos[0]     , &shifted) &&
                kbd_find_grid(&p, &joy_down_row[0]   , &joy_down_bitpos[0]   , &shifted) &&
                kbd_find_grid(&p, &joy_up_row[1]     , &joy_up_bitpos[1]     , &shifted) &&
                kbd_find_grid(&p, &joy_down_row[1]   , &joy_down_bitpos[1]   , &shifted) &&
                kbd_find_grid(&p, &joy_buttons_row[0], &joy_buttons_bitpos[0], &shifted) )
                ;
            else
                {
                close(joy_fd);
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
		close(joy_fd);
	joy_emu = 0;
#endif
	}
/*...e*/
