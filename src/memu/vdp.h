// vdp.h - Emulation of the VDP wrapped within a struct for use in multiple display drivers

#define	VDP_MEMORY_SIZE 0x4000
#define	VBORDER 8
#define	HBORDER256 8
#define	HBORDER240 16
#define	VDP_WIDTH  (HBORDER256+256+HBORDER256)
#define	VDP_HEIGHT (VBORDER   +192+   VBORDER)

typedef struct
    {
    BOOLEAN changed;
    byte    regs[8];
    byte    spr_lines[192];
    word    addr;
    BOOLEAN read_mode;
    int     last_mode;
    BOOLEAN latched;
    byte    latch;
    byte *  ram;
    byte *  pix;
    } VDP;

void vdp_init (VDP *vdp);
void vdp_reset (VDP *vdp);
void vdp_refresh (VDP *vdp);
void vdp_out1 (VDP *, byte val);
void vdp_out2 (VDP *vdp, byte val);
