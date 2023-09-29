/*

  mem.c - MTX memory

  We have a 8KB monitor ROM and 8 other 8KB ROMs.
  By default, ROM 2 is subpaged into 16 subpages.
  We have a number of 16KB RAM pages.

  Memory is never paged in less than 8KB chunks.
  8 chunks comprise the whole 64KB visible to the Z80.
  So we keep two maps, one for reading, one for writing.

  This version has changes by Bill Brendling, allowing larger memory sizes,
  and selective enabling of ROMs.

  It implements the memory map rules, as documented in the manual,
  as implemented correctly by MTX500 and MTX512, and REMEMOTECH.
  However, the 512KB extra memory on certain SDXs is known to fail to
  implement the rules properly, and it is expected that MTX S2 and MTX 2000
  could have problems also.

*/

/*...sincludes:0:*/
#include "ff_stdio.h"
#include <string.h>

#include "types.h"
#include "diag.h"
#include "common.h"
#include "roms.h"
#include "mem.h"
#ifdef HAVE_VDEB
#include "vdeb.h"
#endif
#include "dirmap.h"

#include "Z80.h"
extern Z80 *get_Z80_regs (void);
extern void DebugZ80Instruction(Z80 *r, const char *instruction);
extern BOOLEAN dis_instruction(word *a, char *s);

/*...vtypes\46\h:0:*/
/*...vdiag\46\h:0:*/
/*...vcommon\46\h:0:*/
/*...vroms\46\h:0:*/
/*...vmem\46\h:0:*/
/*...e*/

#ifdef SMALL_MEM
#define MAX_SUBPAGES    1
static byte *mem_high = NULL;
static byte *mem_vapour = NULL;
static const byte *mem_rom_os;
static const byte *mem_subpages[8][MAX_SUBPAGES];
#else
#define MAX_SUBPAGES    256
static byte mem_high[ROM_SIZE]; /* Read this when no chip selected (all 1's) */
static byte mem_vapour[ROM_SIZE]; /* Write here when no chip, or ROM selected */
static byte mem_rom_os[ROM_SIZE]; /* Monitor ROM */
static byte *mem_subpages[8][MAX_SUBPAGES];
#endif
static int mem_n_subpages[8] = { 0,0,0,0,0,0,0,0 };
static byte *mem_ram[MAX_BLOCKS];
static const byte *mem_read[8]; /* Read through these */
static byte *mem_write[8]; /* Write through these */
static byte *mem_update[8]; /* Allow emulator to update ROMS */
static byte mem_iobyte; /* IOBYTE */
static byte mem_subpage = 0x00;
static int mem_blocks;
#ifdef HAVE_VDEB
static BOOLEAN bWrChk = FALSE;
#endif

/* If enabled, you can snapshot RAM, and query from it */
static byte *mem_ram_snapshot[MAX_BLOCKS];
static int mem_blocks_snapshot;

#ifdef DYNAMIC_ROMS
static int rom_enable = 0xff;

/*...smem_get_rom_enable:0:*/
int mem_get_rom_enable (void)
    {
    // printf ("mem_get_rom_enable() = 0x%02X\n", rom_enable);
    return  rom_enable;
    }
/*...e*/
/*...smem_set_rom_enable:0:*/
void mem_set_rom_enable (int ienable)
    {
    // printf ("mem_set_rom_enable (0x%02X)\n", ienable);
    rom_enable  =  ienable;
    }
/*...e*/
/*...smem_get_alloc:0:*/
int mem_get_alloc (void)
    {
    return  mem_blocks;
    }
/*...e*/
#endif

#ifdef HAVE_VDEB
void mem_wrchk (BOOLEAN bChk)
    {
    bWrChk = bChk;
    }
#endif

#ifndef SMALL_MEM
/*...smem_set_n_subpages:0:*/
void mem_set_n_subpages(int rom, int n_subpages)
    {
    int i;
    for ( i = mem_n_subpages[rom]; i < n_subpages; i++ )
        {
        mem_subpages[rom][i] = (byte *) emalloc(ROM_SIZE);
        memset(mem_subpages[rom][i], 0xff, ROM_SIZE);
        }
    for ( ; i < mem_n_subpages[rom]; i++ )
        free(mem_subpages[rom][i]);
    mem_n_subpages[rom] = n_subpages;
    }
/*...e*/
/*...smem_get_n_subpages:0:*/
int mem_get_n_subpages(int rom)
    {
    return mem_n_subpages[rom];
    }
/*...e*/
#endif

/*...sRdZ80:0:*/
byte RdZ80(word addr)
    {
    // printf ("RdZ80 (%04X)\n", addr);
#ifdef SMALL_MEM
    if ( mem_read[addr>>13] == NULL ) return 0xFF;
#endif
    return mem_read[addr>>13][addr&0x1fff];
/*
    byte b = 0xFF;
    const byte *p = mem_read[addr>>13];
    printf ("RdZ80 (%04X) p = %p", addr, p);
    if ( p != NULL ) b = p[addr&0x1fff];
    printf (", b = %02X\n", b);
    return b;
*/
    }
/*...e*/
/*...sWrZ80:0:*/
void WrZ80(word addr, byte value)
    {
    // printf ("WrZ80 (%04X, %02X)\n", addr, value);
#ifdef SMALL_MEM
    if ( mem_write[addr>>13] == NULL ) return;
#endif
    if ( (addr>>13) != 0 || (mem_iobyte&0x80) != 0 )
        {
        /* Normal write */
        mem_write[addr>>13][addr&0x1fff] = value;
#ifdef HAVE_VDEB
        if ( bWrChk ) vdeb_mwrite (mem_iobyte, addr);
#endif
        }
    else
        /* Write to first 8KB in RELCPMH=0 mode, sets sub-page */
        {
#ifdef SMALL_MEM
        Z80 *z80 = get_Z80_regs ();
        diag_message(DIAG_MEM_SUBPAGE, "ROM sub-page set 0x%02x - NOT SUPPORTED, PC = 0x%04X", value, z80->PC.W);
#else
        Z80 *z80 = get_Z80_regs ();
        diag_message(DIAG_MEM_SUBPAGE, "ROM sub-page set to 0x%02x, PC = 0x%04X", value, z80->PC.W);
        mem_set_rom_subpage(value);
#endif
        }
    }
/*...e*/

/*...smem_read_byte:0:*/
byte mem_read_byte(word addr)
    {
#ifdef SMALL_MEM
    if ( mem_read[addr>>13] == NULL ) return 0xFF;
#endif
    return mem_read[addr>>13][addr&0x1fff];
    }
/*...e*/
/*...smem_write_byte:0:*/
/* Notice this deliberately uses mem_update (previously used mem_read).
   In other words, you can write ROM using this call.
   This call is used internally within MEMU, but not by the Z80.
   The Z80 will use WrZ80, which can't. */

void mem_write_byte(word addr, byte value)
    {
#ifdef SMALL_MEM
    if ( mem_update[addr>>13] == NULL ) return;
#endif
    mem_update[addr>>13][addr&0x1fff] = value;
    }
/*...e*/
/*...smem_read_block:0:*/
void mem_read_block(word addr, word len, byte *buf)
    {
    while ( len-- )
        *buf++ = mem_read_byte(addr++);
    }
/*...e*/
/*...smem_write_block:0:*/
void mem_write_block(word addr, word len, const byte *buf)
    {
    // diag_message (DIAG_INIT, "mem_write_block: addr = 0x%04X, ptr = %p", addr, mem_read[addr>>13]);
    while ( len-- )
        mem_write_byte(addr++, *buf++);
    }
/*...e*/

/*...smem_set_iobyte:0:*/
/* This happens infrequently, so we do lots of hard work here,
   to ensure that RdZ80 and WrZ80 can be very fast.
   Note that mem_ram[3] is both
   RELCPMH=1 P=0 0x0000-0x3fff
   RELCPMH=0 P=1 0x8000-0xbfff
   Looking at the PALASM source we can work out what happens in RELCPMH=1
   mode when there is only 32KB of RAM. p247 doesn't show it.
   Note also that there is a magic jumper on the board called I2H4L which is
   wired high on MTX500, preventing 16KB of the RAM appearing more than once.
   Its wired low on MTX512, otherwise lots of RAM would vanish.
*/


/*...smem_set_iobyte_ram:0:*/
static void mem_set_iobyte_ram(int ipage, int iblock)
    {
    if ( iblock < mem_blocks )
        {
        mem_read  [2*ipage  ] = mem_ram[iblock]         ;
        mem_write [2*ipage  ] = mem_ram[iblock]         ;
        mem_update[2*ipage  ] = mem_ram[iblock]         ;
        mem_read  [2*ipage+1] = mem_ram[iblock]+ROM_SIZE;
        mem_write [2*ipage+1] = mem_ram[iblock]+ROM_SIZE;
        mem_update[2*ipage+1] = mem_ram[iblock]+ROM_SIZE;
        }
    else
        {
        mem_read  [2*ipage  ] = mem_high ;
        mem_write [2*ipage  ] = mem_vapour;
        mem_update[2*ipage  ] = mem_vapour;
        mem_read  [2*ipage+1] = mem_high ;
        mem_write [2*ipage+1] = mem_vapour;
        mem_update[2*ipage+1] = mem_vapour;
        }
    }
/*...e*/

void mem_set_iobyte(byte val)
    {
    int iblock, ipage;

    // printf ("mem_set_iobyte (%02X)\n", val);
    if ( val != mem_iobyte )
        {
        diag_message(DIAG_MEM_IOBYTE, "memory IOBYTE set to 0x%02x", val);
        }
    mem_iobyte = val;
#ifdef HAVE_VDEB
    vdeb_iobyte (mem_iobyte);
#endif
    mem_read[7] = mem_write[7] = mem_update[7] = mem_ram[0] + ROM_SIZE;
    mem_read[6] = mem_write[6] = mem_update[6] = mem_ram[0];
    if ( mem_iobyte & 0x80 )
        {
        iblock = 3 * ( mem_iobyte & 0x0f ) + 1;
        if ( mem_iobyte & 0x0f )
            for ( ipage = 0; ipage <= 2; ++ipage, ++iblock )
                mem_set_iobyte_ram(ipage, iblock);
        else
            for ( ipage = 2; ipage >= 0; --ipage, ++iblock )
                mem_set_iobyte_ram(ipage, iblock);
        }
    else
        {
        mem_read[0] = mem_rom_os;
        mem_write[0] = mem_vapour;
#ifdef SMALL_MEM
        mem_update[0] = NULL;
#else
        mem_update[0] = mem_rom_os;
#endif
        int irom = ( mem_iobyte >> 4 ) & 7;
#ifdef DYNAMIC_ROMS
        if ( ( ( rom_enable >> irom ) & 0x01 ) == 0 )
            {
            mem_read[1] = mem_high;
#ifdef SMALL_MEM
            mem_update[1] = NULL;
#else
            mem_update[1] = mem_vapour;
#endif
            /*
              diag_message (DIAG_INIT, "iobyte = 0x%02X, rom_enable = 0x%02X,  ROM Disabled, ptr = %p",
              mem_iobyte, rom_enable, mem_read[1]);
            */
            }
        else
#endif
            {
            int mask = mem_n_subpages[irom]-1;
#ifdef SMALL_MEM
            mem_read[1] = mem_subpages[irom][mem_subpage&mask];
            mem_update[1] = NULL;
#else
            mem_update[1] = mem_subpages[irom][mem_subpage&mask];
            mem_read[1] = mem_update[1];
#endif
            /*
              diag_message (DIAG_INIT, "iobyte = 0x%02X, subpage = 0x%02X, mask = 0x%02X, ptr = %p",
              mem_iobyte, mem_subpage, mask, mem_read[1]);
            */
            }
        mem_write[1] = mem_vapour;
        iblock = 2 * ( mem_iobyte & 0x0f ) + 1;
        for ( ipage = 2; ipage >= 1; --ipage, ++iblock )
            mem_set_iobyte_ram(ipage, iblock);
        // WJB - ROM routine for sizing memory has bug (missing EX AF,AF') for >512K RAM
        if ( ( mem_iobyte & 0x8f ) == 0x0f )
            {
            mem_read[2] = mem_high;
            mem_read[3] = mem_high;
            mem_write[2] = mem_vapour;
            mem_write[3] = mem_vapour;
            mem_update[2] = mem_vapour;
            mem_update[3] = mem_vapour;
            }
        }
    /*
    for ( int i = 0; i < 8; ++i )
        {
        printf ("mem_read[%d] = %p, mem_write[%d] = %p, mem_update[%d] = %p\n",
            i, mem_read[i], i, mem_write[i], i, mem_update[1]);
        }
    */
    }
/*...e*/
/*...smem_get_iobyte:0:*/
byte mem_get_iobyte(void)
    {
    return mem_iobyte;
    }
/*...e*/

/*...smem_get_rom_subpage:0:*/
byte mem_get_rom_subpage(void)
    {
    return mem_subpage;
    }
/*...e*/
/*...smem_set_rom_subpage:0:*/
void mem_set_rom_subpage(byte subpage)
    {
    mem_subpage = subpage;
    mem_set_iobyte(mem_iobyte);
    }
/*...e*/

/*...smem_out0:0:*/
void mem_out0(byte val)
    {
    // if ( val != mem_iobyte )
    //     diag_message(DIAG_MEM_IOBYTE, "memory IOBYTE set to 0x%02x (previously 0x%02x)", val, mem_iobyte);
    mem_set_iobyte(val);
    }
/*...e*/

/*...smem_alloc:0:*/
void mem_alloc(int nblocks)
    {
    int i;
    if ( ( nblocks < 2 ) || ( nblocks > MAX_BLOCKS ) )
        fatal("invalid amount of memory");
    diag_message (DIAG_INIT, "allocated %d blocks (%d KB) memory", nblocks, 16 * nblocks);
    mem_blocks = nblocks;
    for ( i = 0; i < nblocks; ++i )
        if ( mem_ram[i] == NULL )
            mem_ram[i] = (byte *) emalloc(0x4000);
    mem_set_iobyte(mem_iobyte); /* Recalculate page visibility */
    }
/*...e*/

#ifndef SMALL_MEM
/*...smem_alloc_snapshot:0:*/
void mem_alloc_snapshot(int nblocks)
    {
    int i;
    if ( ( nblocks < 0 ) || ( nblocks > MAX_BLOCKS ) )
        fatal("invalid amount of snapshot memory");
    mem_blocks_snapshot = nblocks;
    for ( i = 0; i < nblocks; ++i )
        if ( mem_ram_snapshot[i] == NULL )
            mem_ram_snapshot[i] = (byte *) emalloc(0x4000);
    }
/*...e*/
/*...smem_snapshot:0:*/
void mem_snapshot(void)
    {
    int i;
    for ( i = 0; i < mem_blocks && i < mem_blocks_snapshot; i++ )
        memcpy(mem_ram_snapshot[i], mem_ram[i], 0x4000);
    }
/*...e*/
/*...smem_read_byte_snapshot:0:*/
byte mem_read_byte_snapshot(word addr)
    {
    const byte *p = mem_read[addr>>13];
    int i;
    for ( i = 0; i < mem_blocks && i < mem_blocks_snapshot; i++ )
        if ( p >= mem_ram[i] && p < mem_ram[i]+0x4000 )
            {
            p = p - mem_ram[i] + mem_ram_snapshot[i];
            break;
            }
    return p[addr&0x1fff];
    }
/*...e*/

/*...smem_type_at_address:0:*/
int mem_type_at_address(word addr)
    {
    const byte *p = mem_read[addr>>13];
    int i, j;
    for ( i = 0; i < mem_blocks && i < mem_blocks_snapshot; i++ )
        if ( p >= mem_ram[i] && p < mem_ram[i]+0x4000 )
            return MEMT_RAM_SNAPSHOT;
    for ( ; i < mem_blocks ; i++ )
        if ( p >= mem_ram[i] && p < mem_ram[i]+0x4000 )
            return MEMT_RAM_NO_SNAPSHOT;
    if ( p >= mem_rom_os && p < mem_rom_os+ROM_SIZE )
        return MEMT_ROM;
    for ( i = 0; i < 8; i++ )	
        for ( j = 0; j < mem_n_subpages[i]; j++ )
            if ( p >= mem_subpages[i][j] && p < mem_subpages[i][j]+ROM_SIZE )
                return MEMT_ROM;
    return MEMT_VOID;
    }
/*...e*/
#endif

/*...smem_init_mtx:0:*/
void mem_init_mtx(void)
    {
    // printf ("mem_init_mtx\n");
    int i;
    for ( i = 0; i < MAX_BLOCKS; ++i )
        {
        mem_ram         [i] = NULL;
        mem_ram_snapshot[i] = NULL;
        }
#ifndef SMALL_MEM
    memset(mem_high, 0xff, ROM_SIZE);
    mem_set_n_subpages(0,  1);
    mem_set_n_subpages(1,  1);
    mem_set_n_subpages(2, 16);
    mem_set_n_subpages(3,  1);
    mem_set_n_subpages(4,  1);
    mem_set_n_subpages(5,  1);
    mem_set_n_subpages(6,  1);
    mem_set_n_subpages(7,  1);
    memcpy(mem_rom_os        , rom_os   , ROM_SIZE);
    memcpy(mem_subpages[0][0], rom_basic, ROM_SIZE);
    memcpy(mem_subpages[1][0], rom_assem, ROM_SIZE);
    mem_alloc_snapshot(0);
#else
    /*
    printf ("rom_os = %p\nrom_basic = %p\nrom_assem = %p\nrom_cpm = %p\nrom_sdx = %p\n",
        rom_os, rom_basic, rom_assem, rom_cpm, rom_sdx);
    printf ("rom_os[0] = %02X\n", rom_os[2]);
    */
    mem_rom_os = rom_os;
    mem_subpages[0][0] = rom_basic;
    mem_subpages[1][0] = rom_assem;
    mem_subpages[4][0] = rom_cpm;
    mem_subpages[5][0] = rom_sdx;
    for ( int i = 0; i < 8; ++i )
        mem_n_subpages[i] = 1;
#endif

    mem_alloc(4);

    mem_set_iobyte(0x00);
    }
/*...e*/
/*...smem_dump:0:*/
/* Just a chance to dump the Z80 address space on exit */

void mem_dump(void)
    {
#ifndef SMALL_MEM
    if ( diag_flags[DIAG_MEM_DUMP] )
        {
        FILE *fp;
        byte buf[0x10000];
        /* Can't read 0x10000 in one go */
        mem_read_block(0x0000,0xffff,buf);
        buf[0xffff] = mem_read_byte(0xffff);
        if ( (fp = fopen("memu.mem", "wb")) != NULL )
            {
            fwrite(buf, 1, sizeof(buf), fp);
            fclose(fp);
            }
        }
#endif
    }
/*...e*/

#ifndef SMALL_MEM
/*...smem_rom_ptr:0:*/
/* Backdoor access to memory map implementation.
   Used by keyboard remapping logic, to patch ROM. */

byte *mem_rom_ptr(int rom)
    {
    int mask = mem_n_subpages[rom]-1;
    return mem_subpages[rom][mem_subpage&mask];
    }
/*...e*/

void load_rom (int rom, const char *fname)
    {
    fname = PMapPath (fname);
    if ( rom < 0 || rom > 7 )
        fatal("ROM must be between 0 and 7");
#ifdef SMALL_MEM
    FILE *fp = efopen(fname, "rb");;
    size_t size;
    byte subpage_write = 0;
    while ( subpage_write <= mem_n_subpages[rom] )
        {
        size = fread(mem_subpages[rom][subpage_write], 1, 0x2000, fp);
        diag_message (DIAG_INIT, "load_rom: Loaded 0x%04X bytes from %s into ROM %d, sub-page %d",
            size, fname, rom, subpage_write);
        if ( size < 0x2000 ) break;
        ++subpage_write;
        }
#else
    byte *buf = emalloc(0x2000);
    byte iob, subpage, subpage_write = 0;
    FILE *fp;
    size_t size;
#ifdef DYNAMIC_ROMS
    int ren = mem_get_rom_enable ();
    mem_set_rom_enable (0xFF);
#endif
    fp = efopen(fname, "rb");
    iob = mem_get_iobyte();
    mem_set_iobyte((byte)(rom<<4));
    subpage = mem_get_rom_subpage();
    while ( (size = fread(buf, 1, 0x2000, fp)) > 0 )
        {
        mem_set_rom_subpage(subpage_write);
        mem_write_block(0x2000, (word) size, buf);
        diag_message (DIAG_INIT, "load_rom: Loaded 0x%04X bytes from %s into ROM %d, sub-page %d",
            size, fname, rom, subpage_write);
        ++subpage_write;
        }
    mem_set_rom_subpage(subpage);
#ifdef DYNAMIC_ROMS
    mem_set_rom_enable (ren);
#endif    
    mem_set_iobyte(iob);
    free(buf);
#endif // SMALL_MEM
    }

void load_rompair (int rom, const char *fname)
    {
    fname = PMapPath (fname);
    if ( rom != 2 && rom != 4 && rom != 6 )
        fatal("ROM-pair base must be 2, 4 or 6");
#ifdef SMALL_MEM
    FILE *fp = efopen(fname, "rb");;
    size_t size;
    byte subpage_write = 0;
    while ( subpage_write <= mem_n_subpages[rom] )
        {
        size = fread(mem_subpages[rom][subpage_write], 1, 0x2000, fp);
        diag_message (DIAG_INIT, "load_rompair: Loaded 0x%04X bytes from %s into ROM %d, sub-page %d",
            size, fname, rom, subpage_write);
        if ( size < 0x2000 ) break;
        rom ^= 1;
        if ( (rom & 1) == 0 )
            subpage_write++;
        }
#else
    byte *buf = emalloc(0x2000);
    byte iob, subpage, subpage_write = 0;
    FILE *fp;
    size_t size;
#ifdef DYNAMIC_ROMS
    int ren = mem_get_rom_enable ();
    mem_set_rom_enable (0xFF);
#endif
    fp = efopen(fname, "rb");
    iob = mem_get_iobyte();
    subpage = mem_get_rom_subpage();
    while ( (size = fread(buf, 1, 0x2000, fp)) > 0 )
        {
        mem_set_iobyte((byte)(rom<<4));
        mem_set_rom_subpage(subpage_write);
        mem_write_block(0x2000, (word) size, buf);
        diag_message (DIAG_INIT, "load_rompair: Loaded 0x%04X bytes from %s into ROM %d, sub-page %d",
            size, fname, rom, subpage_write);
        rom ^= 1;
        if ( (rom & 1) == 0 )
            subpage_write++;
        }
    mem_set_rom_subpage(subpage);
#ifdef DYNAMIC_ROMS
    mem_set_rom_enable (ren);
#endif    
    mem_set_iobyte(iob);
    free(buf);
#endif // SMALL_MEM
    }

void load_largerom (const char *psFlags, const char *psFile)
    {
    FILE *pf = efopen (psFile, "r");
    while ( *psFlags )
        {
        int rom = *psFlags;
        if (( rom == 'S' ) || ( rom == 's' ))
            {
            fseek (pf, 0, SEEK_SET);
            if ( fread (mem_rom_os, 1, ROM_SIZE, pf) != ROM_SIZE ) fatal ("Error loading system ROM");
            }
        else if (( rom >= '0' ) && ( rom <= '7' ))
            {
            rom -= '0';
            fseek (pf, ROM_SIZE * ( 2 * rom + 1 ), SEEK_SET);
            if ( fread (mem_subpages[rom][0], 1, ROM_SIZE, pf) != ROM_SIZE ) fatal ("Error loading ROM");
            }
        else
            {
            fatal ("Invalid ROM selector");
            }
        ++psFlags;
        }
    }
#endif

#ifdef SMALL_MEM
byte *mem_ram_ptr (word addr, word *psize)
    {
    byte *ptr = mem_update[addr>>13];
    if ( ptr == NULL ) *psize = 0;
    else
        {
        int ofs = addr & (ROM_SIZE - 1);
        ptr += ofs;
        *psize = ROM_SIZE - ofs;
        }
    // printf ("mem_ram_ptr(0x%04X): ptr = %p, size = %d\n", addr, ptr, *psize);
    if ( ptr == NULL ) fatal ("No memory");
    return ptr;
    }

int mem_file_load (FILE *f, word addr, word len)
    {
    int nrd = 0;
    while ( len > 0 )
        {
        byte *ptr = mem_update[addr>>13];
        if ( ptr == NULL ) fatal ("attempt to load file into read-only memory");
        int ofs = addr & (ROM_SIZE - 1);
        ptr += ofs;
        int blen = ROM_SIZE - ofs;
        if ( blen > len ) blen = len;
        int nin = fread (ptr, 1, blen, f);
        if ( nin < 0 ) return nin;
        nrd += nin;
        if ( nin < blen ) break;
        addr += blen;
        len -= blen;
        }
    return nrd;
    }
#endif
