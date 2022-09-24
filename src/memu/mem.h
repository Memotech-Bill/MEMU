/*

mem.h - MTX memory

*/

#ifndef MEM_H
#define	MEM_H

#ifdef SMALL_MEM
#define MAX_BLOCKS 4
#else
#define MAX_BLOCKS (3*16+1)
#endif

/*...sincludes:0:*/
#include "types.h"

/*...vtypes\46\h:0:*/
/*...e*/

#define	MEMT_VOID            0
#define	MEMT_ROM             1
#define	MEMT_RAM_SNAPSHOT    2
#define	MEMT_RAM_NO_SNAPSHOT 3

extern void mem_set_n_subpages(int rom, int n_subpages);
extern int  mem_get_n_subpages(int rom);

#ifdef DYNAMIC_ROMS

#define ROM_BASIC    0
#define ROM_ASSEM    1
#define ROM_SDX1     3
#define ROM_CPM      4
#define ROM_SDX2     5
#define ROM_NODE     6	/* According to at least one piece of documentation - WJB */
#define ROM_GAME     7

#define ROMEN_BASIC  ( 1 << ROM_BASIC )
#define ROMEN_ASSEM  ( 1 << ROM_ASSEM )
#define ROMEN_SDX1   ( 1 << ROM_SDX1 )
#define ROMEN_CPM    ( 1 << ROM_CPM )
#define ROMEN_SDX2   ( 1 << ROM_SDX2 )
#define ROMEN_NODE   ( 1 << ROM_NODE )
#define ROMEN_GAME   ( 1 << ROM_GAME )

extern int mem_get_rom_enable (void);
extern void mem_set_rom_enable (int ienable);
extern int mem_get_alloc (void);
#endif

extern byte RdZ80(word addr);
extern void WrZ80(word addr, byte value);

extern byte mem_read_byte(word addr);
extern void mem_write_byte(word addr, byte value);
extern void mem_read_block(word addr, word len, byte *buf);
extern void mem_write_block(word addr, word len, const byte *buf);
extern void mem_set_iobyte(byte val);
extern byte mem_get_iobyte(void);
extern byte mem_get_rom_subpage(void);
extern void mem_set_rom_subpage(byte subpage);
extern void mem_out0(byte val);
extern void mem_wrchk (BOOLEAN bChk);

extern void mem_alloc(int nblocks);

extern void mem_alloc_snapshot(int nblocks);
extern void mem_snapshot();
extern byte mem_read_byte_snapshot(word addr);

/* Return MEMT_ value for address */
extern int mem_type_at_address(word addr);

extern void mem_init_mtx(void);
extern void mem_dump(void);

extern byte *mem_rom_ptr(int rom);
extern void load_rom (int rom, const char *fname);
extern void load_rompair (int rom, const char *fname);
extern void load_largerom (const char *psFlags, const char *psFile);

#ifdef SMALL_MEM
extern byte *mem_ram_ptr (word addr, word *psize);
#endif

#endif
