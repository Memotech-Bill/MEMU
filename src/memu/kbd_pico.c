/* kbd_pico.c - Keyboard interface routines for Pico */

#include <pico.h>
#include <tusb.h>
#include "win.h"
#include "diag.h"
#include "kbd.h"
#include "common.h"

#if PICO_SDK_VERSION_MAJOR == 1
#if PICO_SDK_VERSION_MINOR < 2
#define KBD_VERSION     1
#elif PICO_SDK_VERSION_MINOR == 2
#define KBD_VERSION     2
#elif PICO_SDK_VERSION_MINOR == 3
#define KBD_VERSION     3
#endif  // PICO_SDK_VERSION_MINOR
#endif  // PICO_SDK_VERSION_MAJOR
#ifndef KBD_VERSION
#if TUSB_VERSION_MAJOR == 0
#if TUSB_VERSION_MINOR == 12
#define KBD_VERSION     3
#elif (TUSB_VERSION_MINOR == 14) || (TUSB_VERSION_MINOR == 15)
#define KBD_VERSION     4
#elif (TUSB_VERSION_MINOR == 17) || (TUSB_VERSION_MINOR == 18)
#define KBD_VERSION     5
#endif  // TUSB_VERSION_MINOR
#endif  // TUSB_VERSION_MAJOR
#endif  // KBD_VERSION
#ifndef KBD_VERSION
#error Unknown USB Version for keyboard
#endif  // KBD_VERSION

#if KBD_VERSION == 5
#include "class/hid/hid_host.h"
#else
#include "class/hid/hid.h"
#endif

static const int usb_map[] =
    {
    'a',                // 0x04 - Keyboard a and A                                                 
    'b',                // 0x05 - Keyboard b and B                                                 
    'c',                // 0x06 - Keyboard c and C                                                 
    'd',                // 0x07 - Keyboard d and D                                                 
    'e',                // 0x08 - Keyboard e and E                                                 
    'f',                // 0x09 - Keyboard f and F                                                 
    'g',                // 0x0A - Keyboard g and G                                                 
    'h',                // 0x0B - Keyboard h and H                                                 
    'i',                // 0x0C - Keyboard i and I                                                 
    'j',                // 0x0D - Keyboard j and J                                                 
    'k',                // 0x0E - Keyboard k and K                                                 
    'l',                // 0x0F - Keyboard l and L                                                 
    'm',                // 0x10 - Keyboard m and M                                                 
    'n',                // 0x11 - Keyboard n and N                                                 
    'o',                // 0x12 - Keyboard o and O                                                 
    'p',                // 0x13 - Keyboard p and P                                                 
    'q',                // 0x14 - Keyboard q and Q                                                 
    'r',                // 0x15 - Keyboard r and R                                                 
    's',                // 0x16 - Keyboard s and S                                                 
    't',                // 0x17 - Keyboard t and T                                                 
    'u',                // 0x18 - Keyboard u and U                                                 
    'v',                // 0x19 - Keyboard v and V                                                 
    'w',                // 0x1A - Keyboard w and W                                                 
    'x',                // 0x1B - Keyboard x and X                                                 
    'y',                // 0x1C - Keyboard y and Y                                                 
    'z',                // 0x1D - Keyboard z and Z                                                 
    '1',                // 0x1E - Keyboard 1 and !                                                 
    '2',                // 0x1F - Keyboard 2 and "                                                 
    '3',                // 0x20 - Keyboard 3 and £                                                 
    '4',                // 0x21 - Keyboard 4 and $                                                 
    '5',                // 0x22 - Keyboard 5 and %                                                 
    '6',                // 0x23 - Keyboard 6 and ^                                                 
    '7',                // 0x24 - Keyboard 7 and &                                                 
    '8',                // 0x25 - Keyboard 8 and *                                                 
    '9',                // 0x26 - Keyboard 9 and (                                                 
    '0',                // 0x27 - Keyboard 0 and )                                                 
    WK_Return,          // 0x28 - Keyboard Return (ENTER)                                          
    WK_Escape,          // 0x29 - Keyboard ESCAPE                                                  
    WK_BackSpace,       // 0x2A - Backspace                                                        
    WK_Tab,             // 0x2B - Keyboard Tab                                                     
    ' ',                // 0x2C - Keyboard Spacebar                                                
    '-',                // 0x2D - Keyboard - and _                                                 
    '=',                // 0x2E - Keyboard = and +                                                 
    '[',                // 0x2F - Keyboard [ and {                                                 
    ']',                // 0x30 - Keyboard ] and }                                                 
    '\\',               // 0x31 - Keyboard \ and |                                                 
    '#',                // 0x32 - Keyboard # and ~                                                 
    ';',                // 0x33 - Keyboard ; and :                                                 
    '\'',               // 0x34 - Keyboard ' and @                                                 
    '`',                // 0x35 - Keyboard ` and ~                                                 
    ',',                // 0x36 - Keyboard , and <                                                 
    '.',                // 0x37 - Keyboard . and >                                                 
    '/',                // 0x38 - Keyboard / and ?                                                 
    WK_Caps_Lock,       // 0x39 - Keyboard Caps Lock                                               
    WK_F1,              // 0x3A - Keyboard F1                                                      
    WK_F2,              // 0x3B - Keyboard F2                                                      
    WK_F3,              // 0x3C - Keyboard F3                                                      
    WK_F4,              // 0x3D - Keyboard F4                                                      
    WK_F5,              // 0x3E - Keyboard F5                                                      
    WK_F6,              // 0x3F - Keyboard F6                                                      
    WK_F7,              // 0x40 - Keyboard F7                                                      
    WK_F8,              // 0x41 - Keyboard F8                                                      
    WK_F9,              // 0x42 - Keyboard F9 - MTX Line Feed                                      
    WK_F10,             // 0x43 - Keyboard F10 - Diagnostics toggle                                
    WK_F11,             // 0x44 - Keyboard F11 - Open VDEB                                         
    WK_F12,             // 0x45 - Keyboard F12 - NumLock rep                                       
    WK_Sys_Req,         // 0x46 - Keyboard Print Screen and Sys Request
    WK_Scroll_Lock,     // 0x47 - Keyboard Scroll Lock                                             
    WK_Pause,           // 0x48 - Keyboard Pause                                                   
    WK_Insert,          // 0x49 - Keyboard Insert                                                  
    WK_Home,            // 0x4A - Keyboard Home                                                    
    WK_Page_Up,         // 0x4B - Keyboard Page Up                                                 
    WK_Delete,          // 0x4C - Keyboard Delete Forward                                          
    WK_End,             // 0x4D - Keyboard End                                                     
    WK_Page_Down,       // 0x4E - Keyboard Page Down                                               
    WK_Right,           // 0x4F - Keyboard Right Arrow                                             
    WK_Left,            // 0x50 - Keyboard Left Arrow                                              
    WK_Down,            // 0x51 - Keyboard Down Arrow                                              
    WK_Up,              // 0x52 - Keyboard Up Arrow                                                
    WK_Num_Lock,        // 0x53 - Keyboard Num Lock and Clear                                      
    WK_KP_Divide,       // 0x54 - Keypad /                                                         
    WK_KP_Multiply,     // 0x55 - Keypad *                                                         
    WK_KP_Subtract,     // 0x56 - Keypad -                                                         
    WK_KP_Add,          // 0x57 - Keypad +                                                         
    WK_KP_Enter,        // 0x58 - Keypad Enter                                                     
    WK_KP_End,          // 0x59 - Keypad 1 and End                                                 
    WK_KP_Down,         // 0x5A - Keypad 2 and Down Arrow                                          
    WK_KP_Page_Down,    // 0x5B - Keypad 3 and PageDn                                              
    WK_KP_Left,         // 0x5C - Keypad 4 and Left Arrow                                          
    WK_KP_Middle,       // 0x5D - Keypad 5                                                         
    WK_KP_Right,        // 0x5E - Keypad 6 and Right Arrow                                         
    WK_KP_Home,         // 0x5F - Keypad 7 and Home                                                
    WK_KP_Up,           // 0x60 - Keypad 8 and Up Arrow                                            
    WK_KP_Page_Up,      // 0x61 - Keypad 9 and Page Up                                             
    WK_Insert,          // 0x62 - Keypad 0 and Insert                                              
    WK_Delete,          // 0x63 - Keypad . and Delete                                              
    '\\',               // 0x64 - Keyboard Non-US \ and |                                          
    WK_PC_Menu,         // 0x65 - Keyboard Application                                             
    };

static const int usb_meta[] =
    {    
    WK_Control_L,       // 0xE0 - Left Control
    WK_Shift_L,         // 0xE1 - Left Shift
    WK_PC_Alt_L,        // 0xE2 - Left Alt
    WK_PC_Windows_L,    // 0xE3 - Left Meta
    WK_Control_R,       // 0xE4 - Right Control
    WK_Shift_R,         // 0xE5 - Right Shift
    WK_PC_Alt_R,        // 0xE6 - Right Alt
    WK_PC_Windows_R,    // 0xE7 - Right Meta
    };

#define countof(x)  (sizeof(x) / sizeof(x[0]))

static struct st_keyevt
    {
    int     hk;
    BOOLEAN bPress;
    } key_queue[20];    // 8 modifier keys + 6 keys pressed and 6 released

static int n_key_queue = 0;

static int kbd_addr = 0;
static uint8_t led_flags = 0;

static void set_leds (uint8_t leds)
    {
    led_flags = leds;
    diag_message (DIAG_KBD_WIN_KEY, "set_leds: kbd_addr = %d, leds = 0x%02X", kbd_addr, leds);

    if ( kbd_addr > 0 )
        {
        tusb_control_request_t ledreq = {
            .bmRequestType_bit.recipient = TUSB_REQ_RCPT_INTERFACE,
            .bmRequestType_bit.type = TUSB_REQ_TYPE_CLASS,
            .bmRequestType_bit.direction = TUSB_DIR_OUT,
            .bRequest = HID_REQ_CONTROL_SET_REPORT,
            .wValue = HID_REPORT_TYPE_OUTPUT << 8,
            .wIndex = 0,    // Interface number
            .wLength = sizeof (led_flags)
            };
#if KBD_VERSION == 3
        bool bRes = tuh_control_xfer (kbd_addr, &ledreq, &led_flags, NULL);
#elif KBD_VERSION >= 4
        tuh_xfer_t ledxfer = {
            .daddr = kbd_addr,
            .setup = &ledreq,
            .buffer = &led_flags,
            .complete_cb = NULL
            };
        bool bRes = tuh_control_xfer (&ledxfer);
#endif
        }
    }

void kbd_chk_leds (int *mods)
    {
    static uint8_t prev = 0;
    uint8_t leds = 0;
    if ( *mods & MKY_NUMLK )  leds |= KEYBOARD_LED_NUMLOCK;
    if ( *mods & MKY_CAPSLK ) leds |= KEYBOARD_LED_CAPSLOCK;
    if ( *mods & MKY_SCRLLK ) leds |= KEYBOARD_LED_SCROLLLOCK;
    if ( leds != prev )
        {
        set_leds (leds);
        prev = leds;
        }
    }

int usb_find (int hk)
    {
    if ( hk < 4 )
        {
        return -1;
        }
    else if ( hk < countof(usb_map) + 4 )
        {
        return usb_map[hk-4];
        }
    else if ( hk < HID_KEY_CONTROL_LEFT )
        {
        return -1;
        }
    else if ( hk <= HID_KEY_GUI_RIGHT )
        {
        return usb_meta[hk-HID_KEY_CONTROL_LEFT];
        }
    return -1;
    }

static void usb_keypress (int hk)
    {
    int wk = usb_find (hk);
    active_win->keypress (active_win, wk);
    }

static void usb_keyrelease (int hk)
    {
    int wk = usb_find (hk);
    active_win->keyrelease (active_win, wk);
    }

// look up new key in previous keys
static inline int find_key_in_report(hid_keyboard_report_t const *p_report, uint8_t keycode)
    {
    for (int i = 0; i < 6; i++)
        {
        if (p_report->keycode[i] == keycode) return i;
        }
    return -1;
    }

static inline void process_kbd_report(hid_keyboard_report_t const *p_new_report)
    {
    static hid_keyboard_report_t prev_report = {0, 0, {0}}; // previous report to check key released
    bool held[6];
    for (int i = 0; i < 6; ++i) held[i] = false;
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = p_new_report->keycode[i];
        if ( key )
            {
            int kr = find_key_in_report(&prev_report, key);
            if ( kr >= 0 )
                {
                held[kr] = true;
                }
            else
                {
                key_queue[n_key_queue].hk = key;
                key_queue[n_key_queue].bPress = TRUE;
                ++n_key_queue;
                }
            }
        }
    int new_mod = p_new_report->modifier;
    int old_mod = prev_report.modifier;
    int bit = 0x01;
    for (int i = 0; i < 8; ++i)
        {
        if ((new_mod & bit) && !(old_mod & bit))
            {
            key_queue[n_key_queue].hk = HID_KEY_CONTROL_LEFT + i;
            key_queue[n_key_queue].bPress = TRUE;
            ++n_key_queue;
            }
        bit <<= 1;
        }
    bit = 0x01;
    for (int i = 0; i < 8; ++i)
        {
        if (!(new_mod & bit) && (old_mod & bit))
            {
            key_queue[n_key_queue].hk = HID_KEY_CONTROL_LEFT + i;
            key_queue[n_key_queue].bPress = FALSE;
            ++n_key_queue;
            }
        bit <<= 1;
        }
    for (int i = 0; i < 6; ++i)
        {
        uint8_t key = prev_report.keycode[i];
        if (( ! held[i] ) && ( key ))
            {
            key_queue[n_key_queue].hk = key;
            key_queue[n_key_queue].bPress = FALSE;
            ++n_key_queue;
            }
        }
    prev_report = *p_new_report;
    }

CFG_TUSB_MEM_SECTION static hid_keyboard_report_t usb_keyboard_report;

#if KBD_VERSION == 1
void hid_task(void)
	{
    if ((kbd_addr > 0) && tuh_hid_keyboard_is_mounted(kbd_addr))
		{
        if (!tuh_hid_keyboard_is_busy(kbd_addr))
			{
            process_kbd_report(&usb_keyboard_report);
            tuh_hid_keyboard_get_report(kbd_addr, &usb_keyboard_report);
			}
		}
	}

void tuh_hid_keyboard_mounted_cb(uint8_t dev_addr)
    {
    // application set-up
    kbd_addr = dev_addr;
    diag_message (DIAG_KBD_WIN_KEY, "A Keyboard device (address %d) is mounted", dev_addr);
    tuh_hid_keyboard_get_report(dev_addr, &usb_keyboard_report);
    kbd_chk_leds (&kbd_mode);
    win_kbd_leds (FALSE, kbd_mode & KMD_NUMLK, kbd_mode & KMD_SCRLK);
    }

void tuh_hid_keyboard_unmounted_cb(uint8_t dev_addr)
    {
    // application tear-down
    diag_message (DIAG_KBD_WIN_KEY, "A Keyboard device (address %d) is unmounted", dev_addr);
    kbd_addr = 0;
    }

// invoked ISR context
void tuh_hid_keyboard_isr(uint8_t dev_addr, xfer_result_t event)
    {
    (void) dev_addr;
    (void) event;
    }

#elif KBD_VERSION == 2
void hid_task (void)
    {
    static int n = 0;
    if ( kbd_addr == 0 )
        {
        if ( ++n > 1000 ) fatal ("No keyboard mounted.");
        }
    }

// Each HID instance can has multiple reports

#define MAX_REPORT  4
static uint8_t _report_count;
static tuh_hid_report_info_t _report_info_arr[MAX_REPORT];

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
    {
    // Interface protocol
    uint8_t const interface_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    if ( interface_protocol == HID_ITF_PROTOCOL_KEYBOARD )
        {
        kbd_addr = dev_addr;
    
        _report_count = tuh_hid_parse_report_descriptor(_report_info_arr, MAX_REPORT,
            desc_report, desc_len);
        }
    }

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
    {
    if ( dev_addr == kbd_addr ) kbd_addr = 0;
    }

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
    {
    if ( dev_addr != kbd_addr ) return;

    uint8_t const rpt_count = _report_count;
    tuh_hid_report_info_t* rpt_info_arr = _report_info_arr;
    tuh_hid_report_info_t* rpt_info = NULL;

    if ( rpt_count == 1 && rpt_info_arr[0].report_id == 0)
        {
        // Simple report without report ID as 1st byte
        rpt_info = &rpt_info_arr[0];
        }
    else
        {
        // Composite report, 1st byte is report ID, data starts from 2nd byte
        uint8_t const rpt_id = report[0];

        // Find report id in the arrray
        for(uint8_t i=0; i<rpt_count; i++)
            {
            if (rpt_id == rpt_info_arr[i].report_id )
                {
                rpt_info = &rpt_info_arr[i];
                break;
                }
            }

        report++;
        len--;
        }

    if (!rpt_info)
        {
#if USBDBG > 0
        printf("Couldn't find the report info for this report !\r\n");
#endif
        return;
        }

    if ( rpt_info->usage_page == HID_USAGE_PAGE_DESKTOP )
        {
        switch (rpt_info->usage)
            {
            case HID_USAGE_DESKTOP_KEYBOARD:
                // Assume keyboard follow boot report layout
                process_kbd_report( (hid_keyboard_report_t const*) report );
                break;

            default:
                break;
            }
        }
    }

#elif ( KBD_VERSION >= 3 )
void hid_task (void)
    {
    static int n = 0;
    if ( kbd_addr == 0 )
        {
        if ( ++n > 1000 ) fatal ("No keyboard mounted.");
        }
    }

//--------------------------------------------------------------------+
// TinyUSB Callbacks
//--------------------------------------------------------------------+

// Each HID instance can has multiple reports
#define MAX_REPORT  4
static struct
    {
    uint8_t report_count;
    tuh_hid_report_info_t report_info[MAX_REPORT];
    } hid_info[CFG_TUH_HID];

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
    {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
#if USBDBG > 0
    printf("HID device address = %d, instance = %d is mounted\r\n", dev_addr, instance);

    // Interface protocol (hid_interface_protocol_enum_t)
    const char* protocol_str[] = { "None", "Keyboard", "Mouse" };
    printf("HID Interface Protocol = %s\r\n", protocol_str[itf_protocol]);
#endif
    if ( itf_protocol == HID_ITF_PROTOCOL_KEYBOARD )
        kbd_addr = dev_addr;

    // By default host stack will use activate boot protocol on supported interface.
    // Therefore for this simple example, we only need to parse generic report descriptor
    // (with built-in parser)
    if ( itf_protocol == HID_ITF_PROTOCOL_NONE )
        {
        hid_info[instance].report_count = tuh_hid_parse_report_descriptor(hid_info[instance].report_info,
            MAX_REPORT, desc_report, desc_len);
#if USBDBG > 0
        printf("HID has %u reports \r\n", hid_info[instance].report_count);
#endif
        }

    // request to receive report
    // tuh_hid_report_received_cb() will be invoked when report is available
    if ( !tuh_hid_receive_report(dev_addr, instance) )
        {
#if USBDBG > 0
        printf("Error: cannot request to receive report\r\n");
#endif
        }
    }

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
    {
#if USBDBG > 0
    printf("HID device address = %d, instance = %d is unmounted\r\n", dev_addr, instance);
#endif
    if ( dev_addr == kbd_addr ) kbd_addr = 0;
    }

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
    {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD)
        {
#if USBDBG > 1
        printf("HID receive boot keyboard report\r\n");
#endif
        process_kbd_report( (hid_keyboard_report_t const*) report );
        }

    // continue to request to receive report
    if ( !tuh_hid_receive_report(dev_addr, instance) )
        {
#if USBDBG > 0
        printf("Error: cannot request to receive report\r\n");
#endif
        }
    }
#else
#error Unknown USB Version for Keyboard
#endif  // KBD_VERSION

void win_handle_events (void)
    {
    if ( n_key_queue == 0 )
        {
        tuh_task();
        hid_task();
        }
    if ( n_key_queue > 0 )
        {
        --n_key_queue;
        if ( key_queue[n_key_queue].bPress ) usb_keypress (key_queue[n_key_queue].hk);
        else usb_keyrelease (key_queue[n_key_queue].hk);
        }
    }
