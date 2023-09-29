/*

sdxfdc.c -  Partial emulation of the SDX floppy disc controller.

WJB   17/ 7/12 First draft.
AK    01/04/13 Support 40 track media in 80 track drive.
               Handle single sided and single density media.

*/

#include "ff_stdio.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#if defined (UNIX)
#include <unistd.h>
#endif

#include "common.h"
#include "sdxfdc.h"
#include "memu.h"
#include "diag.h"
#include "dirmap.h"

#define  FDCS_NOT_READY    0x80
#define  FDCS_PROTECTED    0x40
#define  FDCS_HEAD_LOADED  0x20
#define  FDCS_DELETED_DATA 0x20
#define  FDCS_SEEK_ERROR   0x10
#define  FDCS_CRC_ERROR    0x08
#define  FDCS_TRACK_00     0x04
#define  FDCS_LOST_DATA    0x04
#define  FDCS_DATA         0x02
#define  FDCS_BUSY         0x01
static byte fdc_status = 0;
#define  FDCC_SIDE         0x02
#define  FDCC_MULTI        0x10
#define  FDCC_MASK         0xe0
#define  FDCC_MASK2        0xf0
#define  FDCC_NONE         0x00
#define  FDCC_READ         0x80
#define  FDCC_WRITE        0xa0
#define  FDCC_WRITE_TRACK  0xf0
static byte fdc_command = 0;
static byte fdc_track = 0;
static byte fdc_sector = 0;
static byte fdc_data = 0;

#define  SDX_DRIVES              2
static int  drv_no   =  0;

#define  DRVS_CFG          0x1f
#define  DRVS_READY        0x20
#define  DRVS_INT          0x40
#define  DRVS_DRQ          0x80
static byte drv_cfg[SDX_DRIVES]  =
   {
   DRVS_DOUBLE_SIDED | DRVS_80TRACK | DRVS_1_DRIVE,
   DRVS_DOUBLE_SIDED | DRVS_80TRACK | DRVS_1_DRIVE
   };
static byte drv_status  =  0;
#define  DRVC_DRIVE           0x01
#define  DRVC_SIDE            0x02
#define  DRVC_MOTOR_ON        0x04
#define  DRVC_MOTOR_READY     0x08
#define  DRVC_DOUBLE_DENSITY  0x10
static byte drv_control             =  0;
static byte drv_track[SDX_DRIVES]   =  { 0, 0 };
static BOOLEAN drv_stout            =  FALSE;

static FILE *fdc_fd[SDX_DRIVES]     =  {NULL, NULL};
static long media_len[SDX_DRIVES];

#define  SDX_SECTOR_SD           128
#define  SDX_SECTOR_DD           256
#define  SDX_SECTORS_PER_TRACK   16
#define  SDX_HEADS               2
static byte drv_data[SDX_SECTOR_DD];
static int sect_len[SDX_DRIVES]  =  { SDX_SECTOR_DD, SDX_SECTOR_DD };
static int sect_pos  =  0;

static enum { wtk_init, wtk_addr, wtk_skip, wtk_data } wtk_state  =  wtk_init;
static int wtk_numsec;

void sdxfdc_fdcsta (void)
   {
   if ( diag_flags[DIAG_SDXFDC_HW] )
      {
      char  s[256];
      strcpy (s, "SDX FDC Status:");
      if ( ! fdc_status )        strcat (s, " None");
      if ( fdc_status & 0x01 )   strcat (s, " Busy");
      if ( fdc_status & 0x02 )   strcat (s, " Index/DRQ");
      if ( fdc_status & 0x04 )   strcat (s, " Track_00/Lost_data");
      if ( fdc_status & 0x08 )   strcat (s, " CRC_error");
      if ( fdc_status & 0x10 )   strcat (s, " Seek_error/Not_found");
      if ( fdc_status & 0x20 )   strcat (s, " Head_loaded/Deleted_data");
      if ( fdc_status & 0x40 )   strcat (s, " Write_protect");
      if ( fdc_status & 0x80 )   strcat (s, " Not_ready");
      diag_message (DIAG_SDXFDC_HW, s);
      }
   }
   
void sdxfdc_drvsta (void)
   {
   if ( diag_flags[DIAG_SDXFDC_STATUS] )
      {
      char  s[256];
      sprintf (s, "SDX Drive %d Status:", drv_no);
      if ( drv_cfg[drv_no] & 0x01 ) strcat (s, " No_Head_Load");
      else                          strcat (s, " Head_load");
      if ( drv_cfg[drv_no] & 0x02 ) strcat (s, " Single_sided");
      else                          strcat (s, " Double_sided");
      if ( drv_cfg[drv_no] & 0x04 ) strcat (s, " 40_Tracks");
      else                          strcat (s, " 80_Tracks");
      if ( drv_cfg[drv_no] & 0x08 ) strcat (s, " 1_Drive");
      else                          strcat (s, " 2_Drives");
      if ( drv_cfg[drv_no] & 0x10 ) strcat (s, " Link");
      if ( drv_status & 0x20 )      strcat (s, " Ready");
      if ( drv_status & 0x40 )      strcat (s, " INT");
      if ( drv_status & 0x80 )      strcat (s, " DRQ");
      diag_message (DIAG_SDXFDC_STATUS, s);
      }
   }

void sdxfdc_type1 (byte cmd)
   {
   fdc_status  =  0;
   
   if ( fdc_fd[drv_no] == NULL )
      {
      fdc_status  |= FDCS_NOT_READY;
      drv_status  &= ~ DRVS_READY;
      }
   
   if ( cmd & 0x08 )    /* Head load */
      {
      fdc_status  |= FDCS_HEAD_LOADED;
      }
      
   if ( cmd & 0x04 )    /* Verify */
      {
      if ( fdc_track !=  drv_track[drv_no] ) fdc_status  |= FDCS_SEEK_ERROR; 
      }
      
   if ( drv_track[drv_no] == 0 )
      {
      fdc_status  |= FDCS_TRACK_00;
      }

   drv_status  |= DRVS_INT;
   }

BOOLEAN sdxfdc_setsec (byte cmd, byte sector)
   {
   int   iSide;
   int   nSides;
   int   iTrack;
   int   nTracks;
   int   iSect;
   if ( fdc_fd[drv_no] == NULL )
      {
      if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Attempt to read or write an undefined floppy drive");
      fdc_command =  FDCC_NONE;
      fdc_status  =  FDCS_SEEK_ERROR;
      return FALSE;
      }
   iSide =  ( drv_control & DRVC_SIDE ) ? 1 : 0;
   diag_message (DIAG_SDXFDC_HW, "SDX FDC Seek emulated sector: Track = %d, Side = %d, Sector = %d",
      fdc_track, iSide, sector);

   nTracks = (drv_cfg[drv_no]&DRVS_40TRACK     ) ? 40 : 80;
   nSides  = (drv_cfg[drv_no]&DRVS_SINGLE_SIDED) ?  1 :  2;
   iTrack  = drv_track[drv_no];

   /* Spot 40 track media in 80 track drive.
      In this case, the SDX disc driver will have doubled the track number.
      So we'll halve it again, and thus access the right part of the file.
      Also, the sector not found check will work correctly. */
   if ( nTracks == 80 &&
        media_len[drv_no] == 40*nSides*SDX_SECTORS_PER_TRACK*sect_len[drv_no] )
      iTrack /= 2;

   if ( ( sector < 1 ) || ( sector > SDX_SECTORS_PER_TRACK ) || ( fdc_track != iTrack ) )
      {
      diag_message (DIAG_SDXFDC_HW, "SDX FDC Sector not found");
      fdc_command =  FDCC_NONE;
      fdc_status  =  FDCS_SEEK_ERROR;
      return FALSE;
      }

   iSect = ( nSides * iTrack + iSide ) * SDX_SECTORS_PER_TRACK + sector - 1;
   if ( fseek (fdc_fd[drv_no], iSect * sect_len[drv_no], SEEK_SET) != 0 )
      {
      if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Error positioning in MFLOPPY file");
      fdc_command =  FDCC_NONE;
      fdc_status  =  FDCS_SEEK_ERROR;
      return FALSE;
      }
   fdc_command =  cmd;
   fdc_status  &= ~ FDCS_SEEK_ERROR;
   fdc_status  |= ( FDCS_DATA | FDCS_BUSY );
   drv_status  &= ~ DRVS_INT;
   drv_status  |= DRVS_DRQ;
   sect_pos =  0;
   return   TRUE;
   }

void sdxfdc_rsec (byte cmd)
   {
   if ( ! sdxfdc_setsec (cmd, fdc_sector) )  return;
   if ( fread (drv_data, sect_len[drv_no], 1, fdc_fd[drv_no]) != 1 )
      {
      if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Error reading MFLOPPY file");
      fdc_command =  FDCC_NONE;
      fdc_status  =  FDCS_CRC_ERROR;
      }
   diag_message (DIAG_SDXFDC_HW, "SDX FDC Read sector");
   }

byte sdxfdc_rdata (void)
   {
   byte  value =  0;
   if ( ( fdc_command & FDCC_MASK ) == FDCC_READ )
      {
      value =  drv_data[sect_pos];
      diag_message (DIAG_SDXFDC_DATA, "SDX FDC Byte %d from sector: 0x%02x", sect_pos, value);
      if ( ++sect_pos >= sect_len[drv_no] )
         {
         if ( fdc_command & FDCC_MULTI )
            {
            diag_message (DIAG_SDXFDC_HW, "SDX FDC Continue multi-sector read");
            ++fdc_sector;
            sdxfdc_rsec (fdc_command);
            }
         else
            {
            fdc_command =  FDCC_NONE;
            fdc_status  =  0;
            drv_status  &= ~ DRVS_DRQ;
            drv_status  |= DRVS_INT;
            }
         }
      }
   else
      {
      value =  fdc_data;
      diag_message (DIAG_SDXFDC_DATA, "SDX FDC Read data register: 0x%02x", fdc_data);
      }
   return   value;
   }

void sdxfdc_wsec (byte cmd)
   {
   sdxfdc_setsec (cmd, fdc_sector);
   }

void sdxfdc_wdata (byte value)
   {
   if ( ( fdc_command & FDCC_MASK ) == FDCC_WRITE )
      {
      diag_message (DIAG_SDXFDC_DATA, "SDX FDC Write byte %d to sector: 0x%02x", sect_pos, value);
      drv_data[sect_pos]   =  value;
      if ( ++sect_pos >= sect_len[drv_no] )
         {
         if ( fwrite (drv_data, sect_len[drv_no], 1, fdc_fd[drv_no]) != 1 )
            {
            if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Write error on MFLOPPY file");
            fdc_command =  FDCC_NONE;
            fdc_status  =  FDCS_CRC_ERROR;
            }
         diag_message (DIAG_SDXFDC_HW, "SDX FDC Write sector");
         if ( fdc_command & FDCC_MULTI )
            {
            diag_message (DIAG_SDXFDC_HW, "SDX FDC Continue multi-sector write");
            ++fdc_sector;
            sdxfdc_wsec (fdc_command);
            }
         else
            {
            fdc_command =  FDCC_NONE;
            fdc_status  =  0;
            drv_status  &= ~ DRVS_DRQ;
            drv_status  |= DRVS_INT;
            }
         }
      }
   else if ( ( fdc_command & FDCC_MASK2 ) == FDCC_WRITE_TRACK )
      {
      diag_message (DIAG_SDXFDC_DATA, "SDX FDC Write track byte: 0x%02x", value);
      switch (wtk_state)
         {
         case  wtk_init:
            {
            if ( value == 0xfe )
               {
               diag_message (DIAG_SDXFDC_HW, "SDX FDC Start of address mark");
               wtk_state   =  wtk_addr;
               sect_pos    =  0;
               }
            break;
            }
         case  wtk_addr:
            {
            if ( value == 0xf7 )
               {
               if ( sect_pos < 4 )
                  {
                  if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Address mark too short");
                  fdc_status  =  FDCS_LOST_DATA;
                  wtk_state   =  wtk_skip;
                  break;
                  }
               diag_message (DIAG_SDXFDC_HW,
                  "SDX FDC Address mark: Track = %d, Side = %d, Sector = %d, Length = %d",
                  drv_data[0], drv_data[1], drv_data[2], drv_data[3]);
               if ( ( drv_data[0] != drv_track[drv_no] )
                  || ( ( drv_data[1] * DRVC_SIDE ) != ( drv_control & DRVC_SIDE ) )
                  || ( drv_data[2] > SDX_SECTORS_PER_TRACK )
                  || ( ( drv_data[3] ? SDX_SECTOR_DD : SDX_SECTOR_SD ) != sect_len[drv_no] ) )
                  {
                  if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Incorrect address mark data");
                  fdc_status  =  FDCS_LOST_DATA;
                  wtk_state   =  wtk_skip;
                  break;
                  }
               sdxfdc_setsec (fdc_command, drv_data[2]);
               wtk_state   =  wtk_skip;
               // diag_flags[DIAG_SDXFDC_DATA]  =  TRUE;
               }
            else if ( sect_pos < 4 )
               {
               drv_data[sect_pos]   =  value;
               ++sect_pos;
               }
            else
               {
               if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Address mark too long");
               fdc_status  =  FDCS_LOST_DATA;
               wtk_state   =  wtk_skip;
               }
            break;
            }
         case  wtk_skip:
            {
            if ( value == 0xfb )
               {
               diag_message (DIAG_SDXFDC_HW, "SDX FDC Start of data mark");
               wtk_state   =  wtk_data;
               }
            break;
            }
         case  wtk_data:
            {
            if ( value == 0xf7 )
               {
               if ( sect_pos < sect_len[drv_no] )
                  {
                  if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Sector mark too short");
                  fdc_status  =  FDCS_LOST_DATA;
                  }
               diag_message (DIAG_SDXFDC_HW, "SDX FDC Format sector");
               if ( fwrite (drv_data, sect_len[drv_no], 1, fdc_fd[drv_no]) != 1 ) /* @@@AK length in blocks */
                  {
                  if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Write error on MFLOPPY file");
                  fdc_status  =  FDCS_LOST_DATA;
                  }
               if ( ++wtk_numsec == SDX_SECTORS_PER_TRACK )
                  {
                  diag_message (DIAG_SDXFDC_HW, "SDX FDC End of write track");
                  fdc_command =  FDCC_NONE;
                  fdc_status  =  0;
                  drv_status  &= ~ DRVS_DRQ;
                  drv_status  |= DRVS_INT;
                  }
               wtk_state   =  wtk_init;
               }
            else if ( sect_pos < sect_len[drv_no] )
               {
               drv_data[sect_pos]   =  value;
               ++sect_pos;
               }
            else
               {
               if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Sector mark too long");
               fdc_status  =  FDCS_LOST_DATA;
               }
            break;
            }
         }
      }
   else
      {
      fdc_data =  value;
      diag_message (DIAG_SDXFDC_HW, "SDX FDC data register set to: 0x%02x", value);
      }
   }
   
void sdxfdc_cmd (byte cmd)
   {
   switch ( cmd & 0xf0 )
      {
      case 0x00:  /* Restore */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC RESTORE Command");
         fdc_track   =  0;
         drv_track[drv_no]   =  0;
         drv_stout   =  FALSE;
         sdxfdc_type1 (cmd);
         break;
         }
      case 0x10:  /* Seek */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC SEEK Command: Seek track %d", fdc_data);
         drv_stout   =  ( fdc_track > drv_track[drv_no] );
         drv_track[drv_no]   =  fdc_data;
         fdc_track   =  fdc_data;
         sdxfdc_type1 (cmd);
         break;
         }
      case 0x20:
      case 0x30:  /* Step */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC STEP Command");
         if ( drv_stout )  ++drv_track[drv_no];
         else if ( drv_track[drv_no] )   --drv_track[drv_no];
         sdxfdc_type1 (cmd);
         break;
         }
      case 0x40:
      case 0x50:  /* Step-in */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC STEP-IN Command");
         drv_stout   =  FALSE;
         if ( drv_track[drv_no] )   --drv_track[drv_no];
         sdxfdc_type1 (cmd);
         break;
         }
      case 0x60:
      case 0x70:  /* Step-out */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC STEP-OUT Command");
         drv_stout   =  TRUE;
         ++drv_track[drv_no];
         sdxfdc_type1 (cmd);
         break;
         }
      case 0x80:
      case 0x90:  /* Read sector */
         {
         // diag_message (DIAG_SDXFDC_HW, "SDX FDC READ Command: Side %d", ( cmd & FDCC_SIDE ) ? 1 : 0);
         diag_message (DIAG_SDXFDC_HW, "SDX FDC READ Command");
         sdxfdc_rsec (cmd);
         break;
         }
      case 0xa0:
      case 0xb0:  /* Write sector */
         {
         // diag_message (DIAG_SDXFDC_HW, "SDX FDC WRITE Command: Side %d", ( cmd & FDCC_SIDE ) ? 1 : 0);
         diag_message (DIAG_SDXFDC_HW, "SDX FDC WRITE Command");
         sdxfdc_wsec (cmd);
         break;
         }
      case 0xc0:  /* Read address */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC READ-ADDRESS Command");
         if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("FDC READ ADDRESS command not implemented");
         fdc_status  =  FDCS_LOST_DATA;
         break;
         }
      case 0xd0:  /* Force interrupt */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC FORCE-INTERRUPT Command");
         fdc_command =  FDCC_NONE;
         sdxfdc_type1 (cmd);
         break;
         }
      case 0xe0:  /* Read track */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC READ-TRACK Command");
         if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("FDC READ TRACK command not implemented");
         fdc_status  =  FDCS_LOST_DATA;
         break;
         }
      case 0xf0:  /* Write Track */
         {
         diag_message (DIAG_SDXFDC_HW, "SDX FDC WRITE-TRACK Command");
         fdc_command =  cmd;
         wtk_state   =  wtk_init;
         wtk_numsec  =  0;
         fdc_status  &= ~ FDCS_SEEK_ERROR;
         fdc_status  |= ( FDCS_DATA | FDCS_BUSY );
         drv_status  &= ~ DRVS_INT;
         drv_status  |= DRVS_DRQ;
         sect_pos =  0;
         break;
         }
      }
   diag_message (DIAG_SDXFDC_HW, "SDX FDC Track = %d, Sector = %d, Drive Track = %d",
       fdc_track, fdc_sector, drv_track[drv_no]);
   sdxfdc_fdcsta ();
   sdxfdc_drvsta ();
   }

void sdxfdc_out (word port, byte value)
   {
   // diag_flags[DIAG_Z80_INSTRUCTIONS]   =  TRUE;
   diag_message (DIAG_SDXFDC_PORT, "SDX FDC Output to port 0x%04x: 0x%02x", port, value);
   if ( ( ! fdc_fd[drv_no] ) && ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) )
       fatal ("No emulated floppy disk defined");
   switch (port & 0x00ff)
      {
      case 0x10:
         {
         sdxfdc_cmd (value);
         break;
         }
      case 0x11:
         {
         fdc_track  =  value;
         diag_message (DIAG_SDXFDC_HW, "SDX FDC Set track register to %d", fdc_track);
         // if ( fdc_track == 1 )  diag_flags[DIAG_Z80_INSTRUCTIONS]   =  TRUE;
         break;
         }
      case 0x12:
         {
         fdc_sector =  value;
         diag_message (DIAG_SDXFDC_HW, "SDX FDC Set sector register to %d", fdc_sector);
         break;
         }
      case 0x13:
         {
         sdxfdc_wdata (value);
         break;
         }
      case 0x14:
         {
         drv_control =  value;
         drv_no   =  ( drv_control & DRVC_DRIVE ) ? 1 : 0;
         if ( ( fdc_fd[drv_no] != NULL ) &&
               ( ( drv_control & ( DRVC_MOTOR_ON | DRVC_MOTOR_READY ) ) == ( DRVC_MOTOR_ON | DRVC_MOTOR_READY ) ) )
            drv_status  |= DRVS_READY;
         else
            drv_status  &= ~ DRVS_READY;
         sect_len[drv_no]  =  ( drv_control & DRVC_DOUBLE_DENSITY ) ? SDX_SECTOR_DD : SDX_SECTOR_SD;
         if ( diag_flags[DIAG_SDXFDC_HW] )
            {
            char  s[256];
            strcpy (s, "SDX Drive Control:");
            if ( drv_control & 0x01 )  strcat (s, " Drive_0");
            else                       strcat (s, " Drive_1");
            if ( drv_control & 0x02 )  strcat (s, " Side_1");
            else                       strcat (s, " Side_0");
            if ( drv_control & 0x04 )  strcat (s, " Motor_On");
            if ( drv_control & 0x08 )  strcat (s, " Motor_Ready");
            if ( drv_control & 0x10 )  strcat (s, " Double_Density");
            diag_message (DIAG_SDXFDC_HW, s);
            }
         break;
         }
      default:
         {
         // fatal ("Invalid SDX FDX output port: 0x%04x", port);
         OutZ80_bad ("SDX FDX", port, value, TRUE);
         }
      }
   }
   
byte sdxfdc_in (word port)
   {
   byte  value;
   if ( ( fdc_fd[drv_no] == NULL ) && ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) )
       fatal ("No emulated floppy disk defined");
   // diag_flags[DIAG_Z80_INSTRUCTIONS]   =  TRUE;
   switch (port & 0x00ff)
      {
      case 0x10:
         {
         value =  fdc_status;
         sdxfdc_fdcsta ();
         break;
         }
      case 0x11:
         {
         value =  fdc_track;
         diag_message (DIAG_SDXFDC_HW, "SDX FDC Read track register: %d", fdc_track);
         break;
         }
      case 0x12:
         {
         value =  fdc_sector;
         diag_message (DIAG_SDXFDC_HW, "SDX FDC Read sector register to %d", fdc_sector);
         break;
         }
      case 0x13:
         {
         value =  sdxfdc_rdata ();
         break;
         }
      case 0x14:
         {
         value =  drv_status | drv_cfg[drv_no];
         sdxfdc_drvsta ();
         break;
         }
      default:
         {
         value =  0;    // Initialisation to keep compiler happy.
         // fatal ("Invalid SDX FDX input port: 0x%04x", port);
         InZ80_bad ("SDX FDC", port, TRUE);
         }
      }
   diag_message (DIAG_SDXFDC_PORT, "SDX FDC Input from port 0x%04x: 0x%02x", port, value);
   return   value;
   }
   
void sdxfdc_init (int drive, const char *psFile)
   {
   if ( ( drive < 0 ) || ( drive >= SDX_DRIVES ) )
      {
      if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Attempt to configure an invalid drive");
      return;
      }
   if ( fdc_fd[drive] != NULL )
      {
      fclose (fdc_fd[drive]);
      fdc_fd[drive]   =  NULL;
      }
   if ( psFile && psFile[0] )
      {
      fdc_fd[drive]     =  fopen (PMapPath (psFile), "r+b");
      if ( fdc_fd[drive] == NULL )
         {
         if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Failed to open MFLOPPY file: %s", psFile);
         return;
         }
      fseek(fdc_fd[drive], 0L, SEEK_END);
      media_len[drive] = ftell(fdc_fd[drive]);
      }
   }

void sdxfdc_term (void)
   {
   if ( fdc_fd[0] != NULL )
      {
      fclose (fdc_fd[0]);
      fdc_fd[0]   =  NULL;
      }
   if ( fdc_fd[1] != NULL )
      {
      fclose (fdc_fd[1]);
      fdc_fd[1]   =  NULL;
      }
   }

void sdxfdc_drvcfg (int drive, byte cfg)
   {
   if ( ( drive < 0 ) || ( drive >= SDX_DRIVES ) )
      {
      if ( ! diag_flags[DIAG_BAD_PORT_IGNORE] ) fatal ("Attempt to configure an invalid drive");
      return;
      }
   drv_cfg[drive] =  cfg & DRVS_CFG;
   }
