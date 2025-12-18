/* cfx2.h - Emulation of CFX-II CompactFlash Interface */

#ifndef H_CFX2
#define H_CFX2

#if defined(HAVE_CFX) && ! defined(HAVE_CFX2)
#define HAVE_CFX2
#endif

#define ADDR_BITS       9                   // Number of bits in an address within a sector
#define LEN_SECTOR      ( 1 << ADDR_BITS )  // 512B sector
#define SECT_BITS       14                  // Number of bits in a sector address
#define LEN_PARTITION   ( 1 << SECT_BITS )  // 16K sectors = 8MB partitions
#define PART_BITS       3                   // Number of bits in partition selection
#define NCF_PART        ( 1 << PART_BITS )  // 8 partitions per card
#define CARD_BITS       1                   // Number of bits in card selection
#define NCF_CARD        ( 1 << CARD_BITS )  // Two cards (primary and secondary)

#ifdef __cplusplus
extern "C"
    {
#endif

    const char * cfx2_get_image (int iCard, int iPartition);
    void cfx2_set_image (int iCard, int iPartition, const char *psFile);
    void cfx2_out_high (byte value);
    void cfx2_out (word port, byte value);
    byte cfx2_in_high (void);
    byte cfx2_in (word port);
    void cfx2_init (void);
    void cfx2_term (void);

#ifdef __cplusplus
    }
#endif

#endif
