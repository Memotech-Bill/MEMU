/* vga.h - Emulation of the Propeller VGA interface */

#ifndef H_VGA
#define H_VGA

#include "types.h"

#ifdef __cplusplus
extern "C"
    {
#endif

    void vga_init (void);
    void vga_term (void);
    void vga_reset (void);
    void vga_out1 (byte chr);
    void vga_out2 (byte chr);
    void vga_out60 (byte chr);
    byte vga_in60 (void);
    void vga_out61 (byte chr);
    byte vga_in61 (void);
    void vga_refresh (void);
    void vga_show (void);

#ifdef __cplusplus
    }
#endif

#endif
