/* main.c - Initialise MEMU on Pico */

#define VID_CORE    1

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "tusb.h"
#include "display_pico.h"
#include "ff_stdio.h"
#include "memu.h"

void memu_main (void)
    {
    printf ("In memu_main\n");
    sleep_ms(500);
    printf ("Calling mount\n");
    fio_mount ();
#ifdef INIT_DIAG
    static const char *sArg[] = { "memu", "-diag-file", INIT_DIAG, "-config-file", "/memu0.cfg",
                                  "-config-file", "/memu.cfg" };
    remove ("/memu.log");
#else
    static const char *sArg[] = { "memu", "-config-file", "/memu0.cfg", "-config-file", "/memu.cfg" };
#endif
    printf ("Initialising USB\n");
    tusb_init();
    printf ("Starting MEMU\n");
    memu (sizeof (sArg) / sizeof (sArg[0]), sArg);
    }

int main (void)
    {
    set_sys_clock_khz (250000, true);
    stdio_init_all();
#ifdef DEBUG
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    for (int i = 10; i > 0; --i )
        {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
        printf ("%d seconds to start\n", i);
        }
#endif
#if VID_CORE == 0
    multicore_launch_core1 (memu_main);
    printf ("Starting display loop\n");
    display_loop ();
#else
    printf ("Starting display loop\n");
    multicore_launch_core1 (display_loop);
    memu_main ();
#endif
    }
