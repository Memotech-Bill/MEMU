/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                         CodesED.h                       **/
/**                                                         **/
/** This file contains implementation for the ED table of   **/
/** Z80 commands. It is included from Z80.c.                **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994,1995,1996,1997       **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

/** This is a special patch for emulating BIOS calls: ********/
case DB_FE:     PatchZ80(R);break;
/*************************************************************/

case ADC_HL_BC: M_ADCW(BC);break;
case ADC_HL_DE: M_ADCW(DE);break;
case ADC_HL_HL: M_ADCW(HL);break;
case ADC_HL_SP: M_ADCW(SP);break;

case SBC_HL_BC: M_SBCW(BC);break;
case SBC_HL_DE: M_SBCW(DE);break;
case SBC_HL_HL: M_SBCW(HL);break;
case SBC_HL_SP: M_SBCW(SP);break;

case LD_xWORDe_HL:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  WrZ80(J.W++,R->HL.B.l);
  WrZ80(J.W,R->HL.B.h);
  break;
case LD_xWORDe_DE:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  WrZ80(J.W++,R->DE.B.l);
  WrZ80(J.W,R->DE.B.h);
  break;
case LD_xWORDe_BC:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  WrZ80(J.W++,R->BC.B.l);
  WrZ80(J.W,R->BC.B.h);
  break;
case LD_xWORDe_SP:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  WrZ80(J.W++,R->SP.B.l);
  WrZ80(J.W,R->SP.B.h);
  break;

case LD_HL_xWORDe:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  R->HL.B.l=RdZ80(J.W++);
  R->HL.B.h=RdZ80(J.W);
  break;
case LD_DE_xWORDe:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  R->DE.B.l=RdZ80(J.W++);
  R->DE.B.h=RdZ80(J.W);
  break;
case LD_BC_xWORDe:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  R->BC.B.l=RdZ80(J.W++);
  R->BC.B.h=RdZ80(J.W);
  break;
case LD_SP_xWORDe:
  J.B.l=RdZ80(R->PC.W++);
  J.B.h=RdZ80(R->PC.W++);
  R->SP.B.l=RdZ80(J.W++);
  R->SP.B.h=RdZ80(J.W);
  break;

case RRD:
  I=RdZ80(R->HL.W);
  J.B.l=(I>>4)|(R->AF.B.h<<4);
  WrZ80(R->HL.W,J.B.l);
  R->AF.B.h=(I&0x0F)|(R->AF.B.h&0xF0);
  R->AF.B.l=PZSTable[R->AF.B.h]|(R->AF.B.l&C_FLAG);
  break;
case RLD:
  I=RdZ80(R->HL.W);
  J.B.l=(I<<4)|(R->AF.B.h&0x0F);
  WrZ80(R->HL.W,J.B.l);
  R->AF.B.h=(I>>4)|(R->AF.B.h&0xF0);
  R->AF.B.l=PZSTable[R->AF.B.h]|(R->AF.B.l&C_FLAG);
  break;

case LD_A_I:
  R->AF.B.h=R->I;
  R->AF.B.l=(R->AF.B.l&C_FLAG)|(R->IFF&1? P_FLAG:0)|ZSTable[R->AF.B.h];
  break;

case LD_A_R:
  /* @@@AK, &0x7F added in response to observation about real Z80 by Claus Baekkel */
  R->AF.B.h=(byte)( (-R->ICount&0xFF) &0x7F );
  R->AF.B.l=(R->AF.B.l&C_FLAG)|(R->IFF&1? P_FLAG:0)|ZSTable[R->AF.B.h];
  break;

case LD_I_A:   R->I=R->AF.B.h;break;
case LD_R_A:   break;

case IM_0:     R->IFF&=0xF9;break;
case IM_1:     R->IFF=(R->IFF&0xF9)|2;break;
case IM_2:     R->IFF=(R->IFF&0xF9)|4;break;

/* @@@AK, give visibility of RETI */
case RETI:     M_RET;RetiZ80(R);break;
case RETN:     if(R->IFF&0x40) R->IFF|=0x01; else R->IFF&=0xFE;
               M_RET;break;

case NEG:      I=R->AF.B.h;R->AF.B.h=0;M_SUB(I);break;

case IN_B_xC:  M_IN(R->BC.B.h);break;
case IN_C_xC:  M_IN(R->BC.B.l);break;
case IN_D_xC:  M_IN(R->DE.B.h);break;
case IN_E_xC:  M_IN(R->DE.B.l);break;
case IN_H_xC:  M_IN(R->HL.B.h);break;
case IN_L_xC:  M_IN(R->HL.B.l);break;
case IN_A_xC:  M_IN(R->AF.B.h);break;
case IN_F_xC:  M_IN(J.B.l);break;

/* @@@AK, full word IO address */
case OUT_xC_B: OutZ80(R->BC.W,R->BC.B.h);break;
/* @@@AK, full word IO address */
case OUT_xC_C: OutZ80(R->BC.W,R->BC.B.l);break;
/* @@@AK, full word IO address */
case OUT_xC_D: OutZ80(R->BC.W,R->DE.B.h);break;
/* @@@AK, full word IO address */
case OUT_xC_E: OutZ80(R->BC.W,R->DE.B.l);break;
/* @@@AK, full word IO address */
case OUT_xC_H: OutZ80(R->BC.W,R->HL.B.h);break;
/* @@@AK, full word IO address */
case OUT_xC_L: OutZ80(R->BC.W,R->HL.B.l);break;
/* @@@AK, full word IO address */
case OUT_xC_A: OutZ80(R->BC.W,R->AF.B.h);break;
/* @@@WJB, implement "undefined" instruction used to test for CMOS CPU */
case OUT_xC_X: OutZ80(R->BC.W,0xFF);break;

/* @@@AK, Note that for INI,INIR,IND,INDR,OUTI,OTIR,OUTD,OTDR
   the elapsing of time is now fully handled here, not in the CyclesED table.
   This is so I can issue the InZ80 or out OutZ80 call towards the end of the
   right MCycle within the overall instruction.
   To breakdown each instruction into MCycles of varying numbers of TStates
   I referred to T80_MCode.vhd in REMEMOTECH from the T80 project.
   T80 is a cycle-accurate recreation of a Z80 in VHDL).
   No effort is made anywhere to make the RdZ80 or WrZ80 calls occur
   in the right MCycle - this would be huge and pervasive change. */

case INI:
  /* @@@AK EDprefix+opcode+input */
  ELAPSE(4+4+4);
  /* @@@AK, full word IO address */
  WrZ80(R->HL.W++,InZ80(R->BC.W));
  R->BC.B.h--;
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG);
  /* @@@AK, store/increment */
  ELAPSE(4);
  break;

case INIR:
  do
  {
    /* @@@AK EDprefix+opcode+input */
    ELAPSE(4+4+4);
    /* @@@AK, full word IO address */
    WrZ80(R->HL.W++,InZ80(R->BC.W));
    R->BC.B.h--;
    /* @@@AK store/increment+repeat */
    ELAPSE(4+5);
  }
  while(R->BC.B.h&&(R->ICount>0));
  if(R->BC.B.h) { R->AF.B.l=N_FLAG;R->PC.W-=2; }
  else { R->AF.B.l=Z_FLAG|N_FLAG;ELAPSE(-5); }
  break;

case IND:
  /* @@@AK EDprefix+opcode+input */
  ELAPSE(4+4+4);
  /* @@@AK, full word IO address */
  WrZ80(R->HL.W--,InZ80(R->BC.W));
  R->BC.B.h--;
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG);
  ELAPSE(4); /* store/decrement */
  break;

case INDR:
  do
  {
    /* @@@AK EDprefix+opcode+input */
    ELAPSE(4+4+4);
    /* @@@AK, full word IO address */
    WrZ80(R->HL.W--,InZ80(R->BC.W));
    R->BC.B.h--;
    /* @@@AK, store/decrement+repeat */
    ELAPSE(4+5);
  }
  while(R->BC.B.h&&(R->ICount>0));
  if(R->BC.B.h) { R->AF.B.l=N_FLAG;R->PC.W-=2; }
  else { R->AF.B.l=Z_FLAG|N_FLAG;ELAPSE(-5); }
  break;

case OUTI:
  /* @@@AK, EDprefix+opcode+fetch+output */
  ELAPSE(4+5+3+4);
  /* @@@AK, full word IO address */
  OutZ80(R->BC.W,RdZ80(R->HL.W++));
  R->BC.B.h--;
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG);
  break;

case OTIR:
  do
  {
    /* @@@AK, EDprefix+opcode+fetch+output */
    ELAPSE(4+5+3+4);
    /* @@@AK, full word IO address */
    OutZ80(R->BC.W,RdZ80(R->HL.W++));
    R->BC.B.h--;
    /* @@@AK, repeat */
    ELAPSE(5);
  }
  while(R->BC.B.h&&(R->ICount>0));
  if(R->BC.B.h) { R->AF.B.l=N_FLAG;R->PC.W-=2; }
  else { R->AF.B.l=Z_FLAG|N_FLAG;ELAPSE(-5); }
  break;

case OUTD:
  /* @@@AK, EDprefix+opcode+fetch+output */
  ELAPSE(4+5+3+4);
  /* @@@AK, full word IO address */
  OutZ80(R->BC.W,RdZ80(R->HL.W--));
  R->BC.B.h--;
  R->AF.B.l=N_FLAG|(R->BC.B.h? 0:Z_FLAG);
  break;

case OTDR:
  do
  {
    /* @@@AK, EDprefix+opcode+fetch+output */
    ELAPSE(4+5+3+4);
    /* @@@AK, full word IO address */
    OutZ80(R->BC.W,RdZ80(R->HL.W--));
    R->BC.B.h--;
    /* @@@AK, repeat */
    ELAPSE(5);
  }
  while(R->BC.B.h&&(R->ICount>0));
  if(R->BC.B.h) { R->AF.B.l=N_FLAG;R->PC.W-=2; }
  else { R->AF.B.l=Z_FLAG|N_FLAG;ELAPSE(-5); }
  break;

case LDI:
  WrZ80(R->DE.W++,RdZ80(R->HL.W++));
  R->BC.W--;
  R->AF.B.l=(R->AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(R->BC.W? P_FLAG:0);
  break;

case LDIR:
  do
  {
    WrZ80(R->DE.W++,RdZ80(R->HL.W++));
    R->BC.W--;ELAPSE(21);
  }
  while(R->BC.W&&(R->ICount>0));
  R->AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
  if(R->BC.W) { R->AF.B.l|=N_FLAG;R->PC.W-=2; }
  else ELAPSE(-5);
  break;

case LDD:
  WrZ80(R->DE.W--,RdZ80(R->HL.W--));
  R->BC.W--;
  R->AF.B.l=(R->AF.B.l&~(N_FLAG|H_FLAG|P_FLAG))|(R->BC.W? P_FLAG:0);
  break;

case LDDR:
  do
  {
    WrZ80(R->DE.W--,RdZ80(R->HL.W--));
    R->BC.W--;ELAPSE(21);
  }
  while(R->BC.W&&(R->ICount>0));
  R->AF.B.l&=~(N_FLAG|H_FLAG|P_FLAG);
  if(R->BC.W) { R->AF.B.l|=N_FLAG;R->PC.W-=2; }
  else ELAPSE(-5);
  break;

case CPI:
  I=RdZ80(R->HL.W++);
  J.B.l=R->AF.B.h-I;
  R->BC.W--;
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  break;

case CPIR:
  do
  {
    I=RdZ80(R->HL.W++);
    J.B.l=R->AF.B.h-I;
    R->BC.W--;ELAPSE(21);
  }  
  while(R->BC.W&&J.B.l&&(R->ICount>0));
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  if(R->BC.W&&J.B.l) R->PC.W-=2; else ELAPSE(-5);
  break;  

case CPD:
  I=RdZ80(R->HL.W--);
  J.B.l=R->AF.B.h-I;
  R->BC.W--;
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  break;

case CPDR:
  do
  {
    I=RdZ80(R->HL.W--);
    J.B.l=R->AF.B.h-I;
    R->BC.W--;ELAPSE(21);
  }
  while(R->BC.W&&J.B.l);
  R->AF.B.l =
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[J.B.l]|
    ((R->AF.B.h^I^J.B.l)&H_FLAG)|(R->BC.W? P_FLAG:0);
  if(R->BC.W&&J.B.l) R->PC.W-=2; else ELAPSE(-5);
  break;
