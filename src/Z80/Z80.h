/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.h                         **/
/**                                                         **/
/** This file contains declarations relevant to emulation   **/
/** of Z80 CPU.                                             **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994,1995,1996,1997       **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/**                                                         **/
/** WJB 28/ 8/16 Revisions for cycle accurate interrupts    **/
/*************************************************************/
#ifndef Z80_H
#define Z80_H



                               /* Compilation options:       */
//#define DEBUG	          		/* Compile debugging version  */
/* @@@AK, specified in makefile */
/* #define LSB_FIRST */        /* Compile for low-endian CPU */
/* #define MSB_FIRST */		/* Compile for hi-endian CPU  */

#if ( ! defined(LSB_FIRST) ) && ( ! defined(MSB_FIRST) )
#error Processor endian not defined
#endif

/* @@@AK, a feature added so I can track time */
#define	SUPPORT_ELAPSED

                               /* LoopZ80() may return:      */
#define INT_IRQ     0x0038     /* Standard RST 38h interrupt */
#define INT_NMI     0x0066     /* Non-maskable interrupt     */
#define INT_NONE    0xFFFF     /* No interrupt required      */
#define INT_QUIT    0xFFFE     /* Exit the emulation         */

                               /* Bits in Z80 F register:    */
#define S_FLAG      0x80       /* 1: Result negative         */
#define Z_FLAG      0x40       /* 1: Result is zero          */
#define H_FLAG      0x10       /* 1: Halfcarry/Halfborrow    */
#define P_FLAG      0x04       /* 1: Result is even          */
#define V_FLAG      0x04       /* 1: Overflow occured        */
#define N_FLAG      0x02       /* 1: Subtraction occured     */
#define C_FLAG      0x01       /* 1: Carry/Borrow occured    */

/* WJB - Bits in IFF:

Bit 0	0x01	Interrupts enabled
Bit 1	0x02	Interrupt mode 1
Bit 2	0x04	Interrupt mode 2
Bit 3	0x08	Not used ?
Bit 4	0x10	Not used ?
Bit 5	0x20	Interrupt after next instruction
Bit 6	0x40	Saved state of interrupts enabled flag - Copied to Bit 0 on RETN.
Bit 7	0x80	Halt

*/

#define	IFF_IEN		0x01		/* Interrupts enabled */
#define	IFF_IMODE	0x06		/* Interrupt modes */
#define	IFF_IM1		0x02		/* Interrupt mode 1 */
#define	IFF_IM2		0x04		/* Interrupt mode 2 */
#define	IFF_IENX	0x20		/* Enable interrupts after next instruction */
#define	IFF_IEN2	0x40		/* Saved state of interrupt enable flag */
#define	IFF_HALT	0x80		/* Halt */

#define	ICF_NMI		0x01		/* NMI Requested */
#define ICF_INT		0x02		/* INT Requested */
#define ICF_LOOP	0x04		/* LoopZ80 requested interrupt */

/** Simple Datatypes *****************************************/
/** NOTICE: sizeof(byte)=1 and sizeof(word)=2               **/
/*************************************************************/
#ifndef BYTE_TYPE
#define BYTE_TYPE
typedef unsigned char byte;
#endif
#ifndef WORD_TYPE
#define	WORD_TYPE
typedef unsigned short word;
#endif
typedef signed char offset;

typedef int BOOLEAN;
#ifndef TRUE
#define	TRUE 1
#define	FALSE 0
#endif

/** Structured Datatypes *************************************/
/** NOTICE: #define LSB_FIRST for machines where least      **/
/**         signifcant byte goes first.                     **/
/*************************************************************/
typedef union
{
#ifdef LSB_FIRST
  struct { byte l,h; } B;
#else
  struct { byte h,l; } B;
#endif
  word W;
} pair;

typedef struct
{
  pair AF,BC,DE,HL,IX,IY,PC,SP;       /* Main registers      */
  pair AF1,BC1,DE1,HL1;               /* Shadow registers    */
  byte IFF,I;                         /* Interrupt registers */

  int IPeriod,ICount; /* Set IPeriod to number of CPU cycles */
                      /* between calls to LoopZ80()          */
  int IBackup;        /* Private, don't touch                */
  int ICntLast;       /* Save ICount for updating clock      */
  word IRequest;      /* Set to address of pending IRQ       */
  void *User;         /* Arbitrary user data (ID,RAM*,etc.)  */
  byte TrapBadOps;    /* Set to 1 to warn of illegal opcodes */
  word Trap;          /* Set Trap to address to trace from   */
  byte Trace;         /* Set Trace=1 to start tracing        */
  byte IntCont;       /* Interrupt control flags             */
#ifdef SUPPORT_ELAPSED
  unsigned long long IElapsed;
#endif
} Z80;

#ifdef SUPPORT_ELAPSED
#define	ELAPSE(t) do { R->ICount-=(t); R->IElapsed+=(t); }while(0)
#else
#define	ELAPSE(t) R->ICount-=(t)
#endif

/** ResetZ80() ***********************************************/
/** This function can be used to reset the registers before **/
/** starting execution with RunZ80(). It sets registers to  **/
/** their initial values.                                   **/
/*************************************************************/
void ResetZ80( Z80 *R);

/** ExecZ80() ************************************************/
/** This function will execute a single Z80 opcode. It will **/
/** then return next PC, and current register values in R.  **/
/*************************************************************/
word ExecZ80( Z80 *R);

/** IntZ80() *************************************************/
/** This function will generate interrupt of given vector.  **/
/*************************************************************/
void IntZ80( Z80 *R, word Vector);

/** RunZ80() *************************************************/
/** This function will run Z80 code until an LoopZ80() call **/
/** returns INT_QUIT. It will return the PC at which        **/
/** emulation stopped, and current register values in R.    **/
/*************************************************************/
word RunZ80( Z80 *R);

/** RdZ80()/WrZ80() ******************************************/
/** These functions are called when access to RAM occurs.   **/
/** They allow to control memory access.                    **/
/************************************ TO BE WRITTEN BY USER **/
/* @@@AK, removed inlines */
void WrZ80( word Addr, byte Value);
byte RdZ80( word Addr);

/** InZ80()/OutZ80() *****************************************/
/** Z80 emulation calls these functions to read/write from  **/
/** I/O ports. There can be 65536 I/O ports, but only first **/
/** 256 are usually used                                    **/
/************************************ TO BE WRITTEN BY USER **/
void OutZ80( word Port, byte Value);
byte InZ80( word Port);

/** PatchZ80() ***********************************************/
/** Z80 emulation calls this function when it encounters a  **/
/** special patch command (ED FE) provided for user needs.  **/
/** For example, it can be called to emulate BIOS calls,    **/
/** such as disk and tape access. Replace it with an empty  **/
/** macro for no patching.                                  **/
/************************************ TO BE WRITTEN BY USER **/
void PatchZ80( Z80 *R);

/** DebugZ80() ***********************************************/
/** This function should exist if DEBUG is #defined. When   **/
/** Trace!=0, it is called after each command executed by   **/
/** the CPU, and given the Z80 registers. Emulation exits   **/
/** if DebugZ80() returns 0.                                **/
/*************************************************************/
byte DebugZ80( Z80 *R);

/** LoopZ80() ************************************************/
/** Z80 emulation calls this function periodically to check **/
/** if the system hardware requires any interrupts. This    **/
/** function must return an address of the interrupt vector **/
/** (0x0038, 0x0066, etc.) or INT_NONE for no interrupt.    **/
/** Return INT_QUIT to exit the emulation loop.             **/
/************************************ TO BE WRITTEN BY USER **/
word LoopZ80( Z80 *R);

/* @@@AK, add RETI support */
void RetiZ80( Z80 *R);

#endif /* Z80_H */

/*** CYCLE ACCURATE INTERRUPT ROUTINES ***********************/

/** Z80NMI ***************************************************/
/** Trigger a Non-Maskable interrupt at the end of the      **/
/** current instruction.                                    **/
/*************************************************************/
void Z80NMI (Z80 *R);

/** Z80Int ***************************************************/
/** Trigger an interrupt at the end of the current          **/
/** instruction or after interrupts are enabled.            **/
/*************************************************************/
void Z80Int (Z80 *R);

/** Z80IntAck() **********************************************/
/** Emulates the Z80 Interrupt Acknowledge cycle. This      **/
/** routine should return the value of the byte placed on   **/
/** the data bus by the interrupting device.                **/
/** Does not currently support multi-byte responses.        **/
/************************************ TO BE WRITTEN BY USER **/
BOOLEAN Z80IntAck (Z80 *R, word *pvec);

/** Z80Step() ************************************************/
/** Called at the end of every Z80 instruction to update    **/
/** hardware emulation.                                     **/
/** Second parameter is the number of clock cycles since    **/
/** the previous call.                                      **/
/************************************ TO BE WRITTEN BY USER **/
void Z80Step (Z80 *R, unsigned int uStep);

/** Z80Run() *************************************************/
/** Equivalent of the RunZ80 routine, but implementing      **/
/** cycle accurate interrupts.                              **/
/** This still calls LoopZ80 for hardware for which high    **/
/** latency is acceptable.                                  **/
/*************************************************************/
word Z80Run (Z80 *R);
