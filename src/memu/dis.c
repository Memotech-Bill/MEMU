/*

dis.c - Z80 Disassembler

This was brutally hacked from bez80.C.
You'll note that I decode instructions somewhat
differently to the way its done in Z80.c.

*/

/*...sincludes:0:*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>

#include "types.h"
#include "mem.h"
#include "dis.h"

/*...vtypes\46\h:0:*/
/*...vmem\46\h:0:*/
/*...vdis\46\h:0:*/
/*...e*/

/*...svars:0:*/
BOOLEAN use_syms           = TRUE;
BOOLEAN show_opcode        = TRUE;
BOOLEAN dis_show_ill       = TRUE;
BOOLEAN dis_mtx_exts       = TRUE;

typedef BOOLEAN (*DISFN)(byte op, word * a, char *s);
static DISFN disfn_table[0x100];

#define	IX_PREFIX       0x01	/* Will be removed if used */
#define	IY_PREFIX       0x02	/* Will be removed if used */
#define	D_FETCHED       0x04	/* Present when D fetch, removed when used */
#define	ILL_PREFIX      0x10	/* Prefix not allowed here */
#define	ILL_SHROT       0x20	/* Unknown shift/rotate type */
#define	ILL_USE_HL_FORM 0x40	/* Use a different opcode */
#define	ILL_2_MEM_OP    0x80	/* 2 memory operands (not allowed) */
static byte state;		/* Internal state */
static byte disp;		/* Valid if D_FETCHED */
/*...e*/

/*...sread8:0:*/
static BOOLEAN read8(word addr, byte *value)
	{
	*value = RdZ80(addr);
	return TRUE;
	}
/*...e*/
/*...sread16:0:*/
static BOOLEAN read16(word addr, word * value)
	{
	byte b0 = RdZ80(addr++);
	byte b1 = RdZ80(addr  );
	*value = (word) ( b0 + ((word) b1 << 8) );
	return TRUE;
	}
/*...e*/
/*...sd_symbolic:0:*/
static BOOLEAN d_symbolic(word addr, char *buf, word *disp)
	{
	return FALSE;
	}
/*...e*/
/*...ssymbolic:0:*/
#define L_SYM (1+200+1+6+1)

static const char *symbolic_addr(word addr, char *buf)
	{
	if ( use_syms )
		{
		word disp;
		if ( d_symbolic(addr, buf, &disp) && buf[0] != '$' )
			{
			if ( disp != 0 )
				sprintf(buf+strlen(buf), "+%04x", disp);
			return buf;
			}		
		}
	sprintf(buf, "%04x", addr);
	return buf;
	}

static const char *symbolic_augment(word addr, char *buf)
	{
	if ( use_syms )
		{
		word disp;
		if ( d_symbolic(addr, buf+1, &disp) && buf[1] != '$' )
			{
			buf[0] = '{';
			if ( disp != 0 )
				sprintf(buf+strlen(buf), "+%04x", disp);
			strcat(buf, "}");
			return buf;
			}		
		}
	buf[0] = '\0';
	return buf;
	}
/*...e*/

/*...srm_fetch_d:0:*/
/* Fetch d, if the prefix has been given, and we've not yet fetched it */

static BOOLEAN rm_fetch_d(word *a)
	{
	if ( (state & (IX_PREFIX|IY_PREFIX)) != 0 &&
	     (state & D_FETCHED)             == 0 )
		{
		if ( !read8((*a)++, &disp) )
			return FALSE;
		state |= D_FETCHED;
		}
	return TRUE;
	}
/*...e*/
/*...srm_operand:0:*/
static BOOLEAN rm_operand(byte rm, word *a, char *buf)
	{
	if ( rm != 6 )
		{
		buf[0] = "bcdehlMa"[rm];
		buf[1] = '\0';
		}
	else
		{
		if ( !rm_fetch_d(a) )
			return FALSE;
		if ( state & IX_PREFIX )
			{
			state &= ~(IX_PREFIX|D_FETCHED);
			if ( disp == 0 )
				strcpy(buf, "(ix)");
			else if ( (sbyte) disp > 0 )
				sprintf(buf, "(ix+%02x)", (unsigned) disp);
			else
				sprintf(buf, "(ix-%02x)", -((int)(sbyte)disp));
			}
		else if ( state & IY_PREFIX )
			{
			state &= ~(IY_PREFIX|D_FETCHED);
			if ( disp == 0 )
				strcpy(buf, "(iy)");
			else if ( (sbyte) disp > 0 )
				sprintf(buf, "(iy+%02x)", (unsigned) disp);
			else
				sprintf(buf, "(iy-%02x)", -((int)(sbyte)disp));
			}
		else
			strcpy(buf, "(hl)");
		}
	return TRUE;
	}
/*...e*/
/*...sinx_operand:0:*/
static const char *inx_operand()
	{
	if ( state & IX_PREFIX )
		{
		state &= ~IX_PREFIX;
		return "ix";
		}
	else if ( state & IY_PREFIX )
		{
		state &= ~IY_PREFIX;
		return "iy";
		}
	else
		return "hl";
	}
/*...e*/
/*...sss_operand:0:*/
static const char *ss_operand(byte ss)
	{
	switch ( ss )
		{
		case 0: return "bc";
		case 1: return "de";
		case 2: return "hl";
		case 3: return "sp";
		}
	return 0; /* Won't get here */
	}
/*...e*/
/*...sdd_operand:0:*/
static const char *dd_operand(byte dd)
	{
	switch ( dd )
		{
		case 0: return "bc";
		case 1: return "de";
		case 2: return inx_operand();
		case 3: return "sp";
		}
	return 0; /* Won't get here */
	}
/*...e*/
/*...sqq_operand:0:*/
static const char *qq_operand(byte qq)
	{
	switch ( qq )
		{
		case 0: return "bc";
		case 1: return "de";
		case 2: return inx_operand();
		case 3: return "af";
		}
	return 0; /* Won't get here */
	}
/*...e*/

/*...sld group:0:*/
/*...sd_ld_rm_rm:0:*/
static BOOLEAN d_ld_rm_rm(byte op, word *a, char *s)
	{
	char buf1[10+1];
	char buf2[10+1];
	byte rm1, rm2;
	rm1 = (byte) ((op>>3)&7);
	if ( !rm_operand(rm1, a, buf1) )
		return FALSE;
	rm2 = (byte) ( op    &7);
	if ( !rm_operand(rm2, a, buf2) )
		return FALSE;
	sprintf(s, "ld      %s,%s", buf1, buf2);
	if ( rm1 == 6 && rm2 == 6 )
		state |= ILL_2_MEM_OP;
	return TRUE;
	}
/*...e*/
/*...sd_ld_rm_n:0:*/
static BOOLEAN d_ld_rm_n(byte op, word *a, char *s)
	{
	byte rm = (byte) ((op>>3)&7);
	char buf[10+1];
	byte n;
	if ( !rm_operand(rm, a, buf) )
		return FALSE;
	if ( !read8((*a)++, &n) )
		return FALSE;
	sprintf(s, "ld      %s,%02x",
		buf,
		(unsigned) n);
	if ( n >= ' ' && n <= '~' )
		sprintf(s+strlen(s), " ; ='%c'", (unsigned) n);
	return TRUE;
	}
/*...e*/
/*...sd_ld_bc_a:0:*/
static BOOLEAN d_ld_bc_a(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "ld      (bc),a");
	return TRUE;
	}
/*...e*/
/*...sd_ld_a_bc:0:*/
static BOOLEAN d_ld_a_bc(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "ld      a,(bc)");
	return TRUE;
	}
/*...e*/
/*...sd_ld_de_a:0:*/
static BOOLEAN d_ld_de_a(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "ld      (de),a");
	return TRUE;
	}
/*...e*/
/*...sd_ld_a_de:0:*/
static BOOLEAN d_ld_a_de(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "ld      a,(de)");
	return TRUE;
	}
/*...e*/
/*...sd_ld_nn_a:0:*/
static BOOLEAN d_ld_nn_a(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "ld      (%s),a", symbolic_addr(nn, buf));
	return TRUE;
	}
/*...e*/
/*...sd_ld_a_nn:0:*/
static BOOLEAN d_ld_a_nn(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	byte n;
	op=op; /* Suppress warning */
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "ld      a,(%s)",
		symbolic_addr(nn, buf));
	if ( read8(nn, &n) )
		{
		sprintf(s+strlen(s), " ; =%02x", (unsigned) n);
		if ( n >= ' ' && n <= '~' )
			sprintf(s+strlen(s), " ='%c'", (unsigned) n);
		}
	return TRUE;
	}
/*...e*/
/*...sd_ld_dd_nn:0:*/
static BOOLEAN d_ld_dd_nn(byte op, word *a, char *s)
	{
	word nn;
	byte dd;
	char buf[L_SYM+1];
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	dd = (byte) ((op>>4)&3);
	sprintf(s, "ld      %s,%04x%s",
		dd_operand(dd),
		(unsigned) nn,
		symbolic_augment(nn, buf));
	return TRUE;
	}
/*...e*/
/*...sd_ld_inx_nn:0:*/
static BOOLEAN d_ld_inx_nn(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	word mm;
	op=op; /* Suppress warning */
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "ld      %s,(%s)",
		inx_operand(),
		symbolic_addr(nn, buf));
	if ( read16(nn, &mm) )
		sprintf(s+strlen(s), " ; =%04x%s",
			(unsigned) mm,
			symbolic_augment(mm, buf));
	return TRUE;
	}
/*...e*/
/*...sd_ld_nn_inx:0:*/
static BOOLEAN d_ld_nn_inx(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "ld      (%s),%s",
		symbolic_addr(nn, buf),
		inx_operand());
	return TRUE;
	}
/*...e*/
/*...sd_ld_sp_inx:0:*/
static BOOLEAN d_ld_sp_inx(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	sprintf(s, "ld      sp,%s", inx_operand());
	return TRUE;
	}
/*...e*/
/*...e*/
/*...spush and pop group:0:*/
/*...sd_push_qq:0:*/
static BOOLEAN d_push_qq(byte op, word *a, char *s)
	{
	byte qq = (byte) ((op>>4)&3);
	a=a; /* Suppress warning */
	sprintf(s, "push    %s", qq_operand(qq));
	return TRUE;
	}
/*...e*/
/*...sd_pop_qq:0:*/
static BOOLEAN d_pop_qq(byte op, word *a, char *s)
	{
	byte qq = (byte) ((op>>4)&3);
	a=a; /* Suppress warning */
	sprintf(s, "pop     %s", qq_operand(qq));
	return TRUE;
	}
/*...e*/
/*...e*/
/*...sexchange group:0:*/
/*...sd_ex_de_hl:0:*/
static BOOLEAN d_ex_de_hl(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	strcpy(s, "ex      de,hl");
	return TRUE;
	}
/*...e*/
/*...sd_ex_af_af:0:*/
static BOOLEAN d_ex_af_af(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	strcpy(s, "ex      af,af'");
	return TRUE;
	}
/*...e*/
/*...sd_exx:0:*/
static BOOLEAN d_exx(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	strcpy(s, "exx");
	return TRUE;
	}
/*...e*/
/*...sd_ex_sp_inx:0:*/
static BOOLEAN d_ex_sp_inx(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	sprintf(s, "ex      (sp),%s", inx_operand());
	return TRUE;
	}
/*...e*/
/*...e*/
/*...sbinop8 group:0:*/
static const char *binop8_name[] =
	{ "add     a,", "adc     a,", "sub     ", "sbc     a,",
	  "and     "  , "xor     "  , "or      ", "cp      "  };

/*...sd_binop8_a_rm:0:*/
static BOOLEAN d_binop8_a_rm(byte op, word *a, char *s)
	{
	byte rm = (byte) (op&7);
	char buf[10+1];
	if ( !rm_operand(rm, a, buf) )
		return FALSE;
	sprintf(s, "%s%s",
		binop8_name[(op>>3)&7],
		buf);
	return TRUE;
	}
/*...e*/
/*...sd_binop8_a_n:0:*/
static BOOLEAN d_binop8_a_n(byte op, word *a, char *s)
	{
	byte n;
	if ( !read8((*a)++, &n) )
		return FALSE;
	sprintf(s, "%s%02x",
		binop8_name[(op>>3)&7],
		(unsigned) n);
	if ( n >= ' ' && n <= '~' )
		sprintf(s+strlen(s), " ; ='%c'", (unsigned) n);
	return TRUE;
	}
/*...e*/
/*...e*/
/*...sinc8 and dec8 group:0:*/
/*...sd_inc8_rm:0:*/
static BOOLEAN d_inc8_rm(byte op, word *a, char *s)
	{
	byte rm = (byte) ((op>>3)&7);
	char buf[10+1];
	if ( !rm_operand(rm, a, buf) )
		return FALSE;
	sprintf(s, "inc     %s", buf);
	return TRUE;
	}
/*...e*/
/*...sd_dec8_rm:0:*/
static BOOLEAN d_dec8_rm(byte op, word *a, char *s)
	{
	byte rm = (byte) ((op>>3)&7);
	char buf[10+1];
	if ( !rm_operand(rm, a, buf) )
		return FALSE;
	sprintf(s, "dec     %s", buf);
	return TRUE;
	}
/*...e*/
/*...e*/
/*...sinx arith group:0:*/
/*...sd_add_inx:0:*/
/* Note: ix or iy prefix, if present, applies to lvalue and rvalue. */
/* So we avoid inx_operand removing it, and dd_operand not seeing it. */

static BOOLEAN d_add_inx(byte op, word *a, char *s)
	{
	byte prefix = (byte) ( state & (IX_PREFIX|IY_PREFIX) );
	byte dd;
	op=op; a=a; /* Suppress warning */
	sprintf(s, "add     %s,", inx_operand());
	state |= prefix;
	dd = (byte) ((op>>4)&3);
	strcat(s, dd_operand(dd));
	return TRUE;
	}
/*...e*/
/*...sd_inc_inx:0:*/
static BOOLEAN d_inc_inx(byte op, word *a, char *s)
	{
	byte dd = (byte) ((op>>4)&3);
	a=a; /* Suppress warning */
	sprintf(s, "inc     %s", dd_operand(dd));
	return TRUE;
	}
/*...e*/
/*...sd_dec_inx:0:*/
static BOOLEAN d_dec_inx(byte op, word *a, char *s)
	{
	byte dd = (byte) ((op>>4)&3);
	a=a; /* Suppress warning */
	sprintf(s, "dec     %s", dd_operand(dd));
	return TRUE;
	}
/*...e*/
/*...e*/
/*...sbit group:0:*/
/*...sd_rlca:0:*/
static BOOLEAN d_rlca(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	strcpy(s, "rlca");
	return TRUE;
	}
/*...e*/
/*...sd_rla:0:*/
static BOOLEAN d_rla(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	strcpy(s, "rla");
	return TRUE;
	}
/*...e*/
/*...sd_rrca:0:*/
static BOOLEAN d_rrca(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	strcpy(s, "rrca");
	return TRUE;
	}
/*...e*/
/*...sd_rra:0:*/
static BOOLEAN d_rra(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warning */
	strcpy(s, "rra");
	return TRUE;
	}
/*...e*/
/*...sd_bitop:0:*/
static const char *shrot_name[] =
	{ "rlc", "rrc", "rl ", "rr ", "sla", "sra", "???", "srl" };

static const char *bitop_name[] =
	{ "???", "bit", "res", "set" };

static BOOLEAN d_bitop(byte op, word *a, char *s)
	{
	byte op2;
	byte bitop;
	byte shrot;
	byte rm;
	char buf[10+1];
	op=op; /* Suppress warning */
	if ( !rm_fetch_d(a) )
		return FALSE;
	if ( !read8((*a)++, &op2) )
		return FALSE;
	bitop = (byte) ((op2>>6)&3);
	shrot = (byte) ((op2>>3)&7);
	rm    = (byte) ( op2    &7);
	rm_operand(rm, a, buf);
	if ( bitop == 0 )
		{
		sprintf(s, "%s     %s",
			shrot_name[shrot],
			buf);
		if ( shrot == 6 )
			state |= ILL_SHROT;
		}
	else
		sprintf(s, "%s     %u,%s",
			bitop_name[bitop],
			(unsigned) shrot,		/* bitshift actually */
			buf);
	return TRUE;
	}
/*...e*/
/*...e*/
/*...scontrol flow group:0:*/
static const char *cc_name[] =
	{ "nz", "z", "nc", "c", "po", "pe", "p", "m" };

/*...sd_jp_nn:0:*/
static BOOLEAN d_jp_nn(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "jp      %s",
		symbolic_addr(nn, buf));
	return TRUE;
	}
/*...e*/
/*...sd_jp_cc_nn:0:*/
static BOOLEAN d_jp_cc_nn(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "jp      %s,%s",
		cc_name[(op>>3)&7],
		symbolic_addr(nn, buf));
	return TRUE;
	}
/*...e*/
/*...sd_jr:0:*/
static BOOLEAN d_jr(byte op, word *a, char *s)
	{
	byte e;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &e) )
		return FALSE;
	sprintf(s, "jr      %s",
		symbolic_addr((word) (*a+(sword)(sbyte)e), buf));
	return TRUE;
	}
/*...e*/
/*...sd_jr_c:0:*/
static BOOLEAN d_jr_c(byte op, word *a, char *s)
	{
	byte e;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &e) )
		return FALSE;
	sprintf(s, "jr      c,%s",
		symbolic_addr((word) (*a+(sword)(sbyte)e), buf));
	return TRUE;
	}
/*...e*/
/*...sd_jr_nc:0:*/
static BOOLEAN d_jr_nc(byte op, word *a, char *s)
	{
	byte e;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &e) )
		return FALSE;
	sprintf(s, "jr      nc,%s",
		symbolic_addr((word) (*a+(sword)(sbyte)e), buf));
	return TRUE;
	}
/*...e*/
/*...sd_jr_z:0:*/
static BOOLEAN d_jr_z(byte op, word *a, char *s)
	{
	byte e;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &e) )
		return FALSE;
	sprintf(s, "jr      z,%s",
		symbolic_addr((word) (*a+(sword)(sbyte)e), buf));
	return TRUE;
	}
/*...e*/
/*...sd_jr_nz:0:*/
static BOOLEAN d_jr_nz(byte op, word *a, char *s)
	{
	byte e;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &e) )
		return FALSE;
	sprintf(s, "jr      nz,%s",
		symbolic_addr((word) (*a+(sword)(sbyte)e), buf));
	return TRUE;
	}
/*...e*/
/*...sd_jp_inx:0:*/
static BOOLEAN d_jp_inx(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	sprintf(s, "jp      (%s)",
		inx_operand());
	return TRUE;
	}
/*...e*/
/*...sd_djnz:0:*/
static BOOLEAN d_djnz(byte op, word *a, char *s)
	{
	byte e;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &e) )
		return FALSE;
	sprintf(s, "djnz    %s",
		symbolic_addr((word) (*a+(sword)(sbyte)e), buf));
	return TRUE;
	}
/*...e*/
/*...sd_call_nn:0:*/
static BOOLEAN d_call_nn(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	op=op; /* Suppress warning */
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "call    %s",
		symbolic_addr(nn, buf));
	return TRUE;
	}
/*...e*/
/*...sd_call_cc_nn:0:*/
static BOOLEAN d_call_cc_nn(byte op, word *a, char *s)
	{
	word nn;
	char buf[L_SYM+1];
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "call    %s,%s",
		cc_name[(op>>3)&7],
		symbolic_addr(nn, buf));
	return TRUE;
	}
/*...e*/
/*...sd_ret:0:*/
static BOOLEAN d_ret(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "ret");
	return TRUE;
	}
/*...e*/
/*...sd_ret_cc:0:*/
static BOOLEAN d_ret_cc(byte op, word *a, char *s)
	{
	a=a; /* Suppress warnings */
	sprintf(s, "ret     %s",
		cc_name[(op>>3)&7]);
	return TRUE;
	}
/*...e*/
/*...sd_rst:0:*/
static BOOLEAN d_rst(byte op, word *a, char *s)
	{
	a=a; /* Suppress warning */
	sprintf(s, "rst     %02x",
		(unsigned) (op&0x38));
	if ( dis_mtx_exts )
		switch ( op&0x38 )
			{
/*...s0x08 \45\ dehl:24:*/
case 0x08:
	strcat(s, "{dehl}");
	break;
/*...e*/
/*...s0x10 \45\ scn:24:*/
case 0x10:
	strcat(s, "{scn}");
	for ( ;; )
		{
		byte code;
		if ( !read8(*a, &code) )
			{
			strcat(s, " ... can't read");
			return TRUE;
			}
		(*a)++;
		sprintf(s+strlen(s), ",0x%02x", code);
		switch ( code & 0xc0 )
			{
			case 0x00:
				strcat(s, "{write}");
				break;
			case 0x40:
				// Virtual screen
				sprintf(s+strlen(s), "{vs%d%s}", code&7, (code&8)?",cls":"");
				break;
			case 0x80:
				// Write string
				{
				byte i;
				strcat(s, "{writestr}");
				for ( i = 0; i < (code&0x1f); i++ )
					{
					byte b;
					if ( !read8(*a, &b) )
						{
						strcat(s, " ... can't read");
						return TRUE;
						}
					(*a)++;
					if ( b < ' ' || b > '~' )
						sprintf(s+strlen(s), ",0x%02x", b);	
					else if ( s[strlen(s)-1] == '\'' )
						sprintf(s+strlen(s)-1, "%c'", b); 
					else
						sprintf(s+strlen(s), ",'%c'", b); 
					}
				}
				break;
			case 0xc0:
				// Write BC
				strcat(s, "{writebc}");
				break;
			}
		if ( (code & 0x20) == 0 )
			break;
		if ( strlen(s) > 300 )
			{
			strcat(s, " ... too long");
			return TRUE;
			}
		}
	break;
/*...e*/
/*...s0x28 \45\ err:24:*/
case 0x28:
	{
	byte errcode;
	if ( !read8(*a, &errcode) )
		{
		strcat(s, " ... can't read");
		return TRUE;
		}
	(*a)++;
	sprintf(s+strlen(s), "{err},0x%02x", errcode);
	}
	break;
/*...e*/
			}
	return TRUE;
	}
/*...e*/
/*...e*/
/*...sio group:0:*/
/*...sd_in_a_n:0:*/
static BOOLEAN d_in_a_n(byte op, word *a, char *s)
	{
	byte n;
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &n) )
		return FALSE;
	sprintf(s, "in      a,(%02x)", (unsigned) n);
	return TRUE;
	}
/*...e*/
/*...sd_out_n_a:0:*/
static BOOLEAN d_out_n_a(byte op, word *a, char *s)
	{
	byte n;
	op=op; /* Suppress warning */
	if ( !read8((*a)++, &n) )
		return FALSE;
	sprintf(s, "out     (%02x),a", (unsigned) n);
	return TRUE;
	}
/*...e*/
/*...e*/
/*...smisc group:0:*/
/*...sd_daa:0:*/
static BOOLEAN d_daa(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "daa");
	return TRUE;
	}
/*...e*/
/*...sd_cpl:0:*/
static BOOLEAN d_cpl(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "cpl");
	return TRUE;
	}
/*...e*/
/*...sd_ccf:0:*/
static BOOLEAN d_ccf(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "ccf");
	return TRUE;
	}
/*...e*/
/*...sd_scf:0:*/
static BOOLEAN d_scf(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "scf");
	return TRUE;
	}
/*...e*/
/*...sd_nop:0:*/
static BOOLEAN d_nop(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "nop");
	return TRUE;
	}
/*...e*/
/*...sd_halt:0:*/
static BOOLEAN d_halt(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "halt");
	return TRUE;
	}
/*...e*/
/*...sd_di:0:*/
static BOOLEAN d_di(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "di");
	return TRUE;
	}
/*...e*/
/*...sd_ei:0:*/
static BOOLEAN d_ei(byte op, word *a, char *s)
	{
	op=op; a=a; /* Suppress warnings */
	strcpy(s, "ei");
	return TRUE;
	}
/*...e*/
/*...sd_ed_prefix:0:*/
/* opcode 0xed 0x?? */

static BOOLEAN d_ed_prefix(byte op, word *a, char *s)
	{
	byte op2;
	if ( !read8((*a)++, &op2) )
		return FALSE;
	switch ( op2 )
		{
		case 0x44: strcpy(s, "neg"        ); return TRUE;
		case 0x45: strcpy(s, "retn"       ); return TRUE;
		case 0x46: strcpy(s, "im 0"       ); return TRUE;
		case 0x47: strcpy(s, "ld      i,a"); return TRUE;
		case 0x4d: strcpy(s, "reti"       ); return TRUE;
		case 0x4f: strcpy(s, "ld      r,a"); return TRUE;
		case 0x56: strcpy(s, "im 1"       ); return TRUE;
		case 0x57: strcpy(s, "ld      a,i"); return TRUE;
		case 0x5e: strcpy(s, "im 2"       ); return TRUE;
		case 0x5f: strcpy(s, "ld      a,r"); return TRUE;
		case 0x67: strcpy(s, "rrd"        ); return TRUE;
		case 0x6f: strcpy(s, "rld"        ); return TRUE;
		case 0xa0: strcpy(s, "ldi"        ); return TRUE;
		case 0xa1: strcpy(s, "cpi"        ); return TRUE;
		case 0xa2: strcpy(s, "ini"        ); return TRUE;
		case 0xa3: strcpy(s, "outi"       ); return TRUE;
		case 0xa8: strcpy(s, "ldd"        ); return TRUE;
		case 0xa9: strcpy(s, "cpd"        ); return TRUE;
		case 0xaa: strcpy(s, "ind"        ); return TRUE;
		case 0xab: strcpy(s, "outd"       ); return TRUE;
		case 0xb0: strcpy(s, "ldir"       ); return TRUE;
		case 0xb1: strcpy(s, "cpir"       ); return TRUE;
		case 0xb2: strcpy(s, "inir"       ); return TRUE;
		case 0xb3: strcpy(s, "otir"       ); return TRUE;
		case 0xb8: strcpy(s, "lddr"       ); return TRUE;
		case 0xb9: strcpy(s, "cpdr"       ); return TRUE;
		case 0xba: strcpy(s, "indr"       ); return TRUE;
		case 0xbb: strcpy(s, "otdr"       ); return TRUE;
		case 0xfe: strcpy(s, "patch"      ); return TRUE;
		}
	switch ( op2 & 0xcf )
		{
/*...s0x4b \45\ ld dd\44\\40\nn\41\:16:*/
case 0x4b:
	{
	word nn;
	byte dd = (byte) ((op2>>4)&3);
	word mm;
	char buf[L_SYM+1];
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	sprintf(s, "ld      %s,(%s)",
		dd_operand(dd),
		symbolic_addr(nn, buf));
	if ( dd == 2 )
		state |= ILL_USE_HL_FORM;
	if ( read16(nn, &mm) )
		sprintf(s+strlen(s), " ; =%04x%s",
			(unsigned) mm,
			symbolic_augment(mm, buf));
	return TRUE;
	}
/*...e*/
/*...s0x43 \45\ ld \40\nn\41\\44\dd:16:*/
case 0x43:
	{
	word nn;
	byte dd;
	char buf[L_SYM+1];
	if ( !read16(*a, &nn) )
		return FALSE;
	(*a) += 2;
	dd = (byte) ((op2>>4)&3);
	sprintf(s, "ld      (%s),%s",
		symbolic_addr(nn, buf),
		dd_operand(dd));
	if ( dd == 2 )
		state |= ILL_USE_HL_FORM;
	return TRUE;
	}
/*...e*/
/*...s0x4a \45\ adc hl\44\ss:16:*/
case 0x4a:
	{
	byte ss = (byte) ((op2>>4)&3);
	sprintf(s, "adc     hl,%s",
		ss_operand(ss));
	return TRUE;
	}
/*...e*/
/*...s0x42 \45\ sbc hl\44\ss:16:*/
case 0x42:
	{
	byte ss = (byte) ((op2>>4)&3);
	sprintf(s, "sbc     hl,%s",
		ss_operand(ss));
	return TRUE;
	}
/*...e*/
		}
	switch ( op2 & 0xc7 )
		{
/*...s0x40 \45\ in r\44\\40\c\41\:16:*/
case 0x40:
	{
	byte rm;
	char buf[10+1];
	if ( state & (IX_PREFIX|IY_PREFIX) )
		state |= ILL_PREFIX;
	rm = (byte) ((op2>>3)&7);
	if ( !rm_operand(rm, a, buf) )
		return FALSE;
	sprintf(s, "in      %s,(c)", buf);
	return TRUE;
	}
/*...e*/
/*...s0x41 \45\ out \40\c\41\\44\r:16:*/
case 0x41:
	{
	byte rm;
	char buf[10+1];
	if ( state & (IX_PREFIX|IY_PREFIX) )
		state |= ILL_PREFIX;
	rm = (byte) ((op2>>3)&7);
	if ( !rm_operand(rm, a, buf) )
		return FALSE;
	sprintf(s, "out     (c),%s", buf);
	return TRUE;
	}
/*...e*/
		}
	sprintf(s, "db      %02x,%02x", (unsigned) op, (unsigned) op2);
	return TRUE;
	}
/*...e*/
/*...sd_illegal:0:*/
static BOOLEAN d_illegal(byte op, word *a, char *s)
	{
	a=a; /* Suppress warning */
	sprintf(s, "db      %02x", (unsigned) op);
	if ( op >= ' ' && op <= '~' )
		sprintf(s+strlen(s), " ; ='%c'", (unsigned) op);
	return TRUE;
	}
/*...e*/
/*...e*/

/*...sdecode tables:0:*/
#define	X	2		/* Can be 0 or a 1 */
#define	REG_M	X,X,X		/* One of 7 registers or memory */
#define	DD_	X,X		/* Choice of 4 register pairs */
#define	QQ_	X,X		/* Choice of 4 register pairs */
#define	SS_	X,X		/* Choice of 4 register pairs, no prefix */
#define	BIN8_	X,X,X		/* Choice of 8 2-operand 8 bit arithmetic ops */
#define	CC___	X,X,X		/* Condition code */
#define	T____	X,X,X		/* Restart number */

/*...sadd_ins:0:*/
static void add_ins(
	int b7, int b6, int b5, int b4, int b3, int b2, int b1, int b0,
	DISFN disfn
	)
	{
	int i;
	for ( i = 0; i < 0x100; i++ )
		if ( ( b7 == X || b7 == ((i >> 7) & 1) ) &&
		     ( b6 == X || b6 == ((i >> 6) & 1) ) &&
		     ( b5 == X || b5 == ((i >> 5) & 1) ) &&
		     ( b4 == X || b4 == ((i >> 4) & 1) ) &&
		     ( b3 == X || b3 == ((i >> 3) & 1) ) &&
		     ( b2 == X || b2 == ((i >> 2) & 1) ) &&
		     ( b1 == X || b1 == ((i >> 1) & 1) ) &&
		     ( b0 == X || b0 == ( i       & 1) ) )
			disfn_table[i] = disfn;
	}
/*...e*/

static void build_decode_tables()
	{
	int i;
	for ( i = 0; i < 0x100; i++ )
		disfn_table[i] = &d_illegal;

/*...sld group:8:*/
add_ins(0,1,REG_M,REG_M, &d_ld_rm_rm        );
add_ins(0,0,REG_M,1,1,0, &d_ld_rm_n         );
add_ins(0,0,0,0,0,0,1,0, &d_ld_bc_a         );
add_ins(0,0,0,0,1,0,1,0, &d_ld_a_bc         );
add_ins(0,0,0,1,0,0,1,0, &d_ld_de_a         );
add_ins(0,0,0,1,1,0,1,0, &d_ld_a_de         );
add_ins(0,0,1,1,0,0,1,0, &d_ld_nn_a         );
add_ins(0,0,1,1,1,0,1,0, &d_ld_a_nn         );
add_ins(0,0,DD_,0,0,0,1, &d_ld_dd_nn        );
add_ins(0,0,1,0,1,0,1,0, &d_ld_inx_nn       );
add_ins(0,0,1,0,0,0,1,0, &d_ld_nn_inx       );
add_ins(1,1,1,1,1,0,0,1, &d_ld_sp_inx       );
/*...e*/
/*...spush and pop group:8:*/
add_ins(1,1,QQ_,0,1,0,1, &d_push_qq         );
add_ins(1,1,QQ_,0,0,0,1, &d_pop_qq          );
/*...e*/
/*...sexchange group:8:*/
add_ins(1,1,1,0,1,0,1,1, &d_ex_de_hl        );
add_ins(0,0,0,0,1,0,0,0, &d_ex_af_af        );
add_ins(1,1,0,1,1,0,0,1, &d_exx             );
add_ins(1,1,1,0,0,0,1,1, &d_ex_sp_inx       );
/*...e*/
/*...sbinop8 group:8:*/
add_ins(1,0,BIN8_,REG_M, &d_binop8_a_rm     );
add_ins(1,1,BIN8_,1,1,0, &d_binop8_a_n      );
/*...e*/
/*...sinc8 and dec8 group:8:*/
add_ins(0,0,REG_M,1,0,0, &d_inc8_rm         );
add_ins(0,0,REG_M,1,0,1, &d_dec8_rm         );
/*...e*/
/*...sinx arith group:8:*/
add_ins(0,0,SS_,1,0,0,1, &d_add_inx         );
add_ins(0,0,SS_,0,0,1,1, &d_inc_inx         );
add_ins(0,0,SS_,1,0,1,1, &d_dec_inx         );
/*...e*/
/*...sbit group:8:*/
add_ins(0,0,0,0,0,1,1,1, &d_rlca            );
add_ins(0,0,0,1,0,1,1,1, &d_rla             );
add_ins(0,0,0,0,1,1,1,1, &d_rrca            );
add_ins(0,0,0,1,1,1,1,1, &d_rra             );
add_ins(1,1,0,0,1,0,1,1, &d_bitop           );
/*...e*/
/*...scontrol flow group:8:*/
add_ins(1,1,0,0,0,0,1,1, &d_jp_nn           );
add_ins(1,1,CC___,0,1,0, &d_jp_cc_nn        );
add_ins(0,0,0,1,1,0,0,0, &d_jr              );
add_ins(0,0,1,1,1,0,0,0, &d_jr_c            );
add_ins(0,0,1,1,0,0,0,0, &d_jr_nc           );
add_ins(0,0,1,0,1,0,0,0, &d_jr_z            );
add_ins(0,0,1,0,0,0,0,0, &d_jr_nz           );
add_ins(1,1,1,0,1,0,0,1, &d_jp_inx          );
add_ins(0,0,0,1,0,0,0,0, &d_djnz            );
add_ins(1,1,0,0,1,1,0,1, &d_call_nn         );
add_ins(1,1,CC___,1,0,0, &d_call_cc_nn      );
add_ins(1,1,0,0,1,0,0,1, &d_ret             );
add_ins(1,1,CC___,0,0,0, &d_ret_cc          );
add_ins(1,1,T____,1,1,1, &d_rst             );
/*...e*/
/*...sio group:8:*/
add_ins(1,1,0,1,1,0,1,1, &d_in_a_n          );
add_ins(1,1,0,1,0,0,1,1, &d_out_n_a         );
/*...e*/
/*...smisc:8:*/
add_ins(0,0,1,0,0,1,1,1, &d_daa             );
add_ins(0,0,1,0,1,1,1,1, &d_cpl             );
add_ins(0,0,1,1,1,1,1,1, &d_ccf             );
add_ins(0,0,1,1,0,1,1,1, &d_scf             );
add_ins(0,0,0,0,0,0,0,0, &d_nop             );
add_ins(0,1,1,1,0,1,1,0, &d_halt            );
add_ins(1,1,1,1,0,0,1,1, &d_di              );
add_ins(1,1,1,1,1,0,1,1, &d_ei              );
add_ins(1,1,1,0,1,1,0,1, &d_ed_prefix       );
/*...e*/
	}
/*...e*/

/*...sdisassemble_non_prefix:0:*/
static BOOLEAN disassemble_non_prefix(byte op, word *a, char *s)
	{
	DISFN f = disfn_table[op];
	return (*f)(op, a, s);
	}
/*...e*/
/*...sdisassemble:0:*/
static BOOLEAN disassemble(word *a, char *s)
	{
	word a_start = *a;
	char *p = s;
	byte op;

	if ( show_opcode )
		p += (1+8+1);

	if ( !read8((*a)++, &op) )
		return FALSE;

	state = 0; /* No prefixes, no illegalities */
	switch ( op )
		{
		case 0xdd:
			state |= IX_PREFIX;
			if ( !read8((*a)++, &op) )
				return FALSE;
			break;
		case 0xfd:
			state |= IY_PREFIX;
			if ( !read8((*a)++, &op) )
				return FALSE;
			break;
		}

	if ( ! disassemble_non_prefix(op, a, p) )
		return FALSE;

	if ( dis_show_ill )
		/* Display illegal aspects of instruction we've decoded */
		{
		if ( state & ILL_PREFIX )
			strcat(s, " illegal-ix/iy-prefix");
		if ( state & ILL_SHROT )
			strcat(s, " illegal-shift/rotate-op");
		if ( state & ILL_USE_HL_FORM )
			strcat(s, " long-form");
		if ( state & ILL_2_MEM_OP )
			strcat(s, " 2-mem-op");
		}

	if ( show_opcode )
		{
		word a2;
		p = s;
		for ( a2 = a_start; a2 != (*a) && p < s+8+2-2; a2++ )
			{
			if ( !read8(a2, &op) )
				return FALSE;
			sprintf(p, "%02x", (unsigned) op);
			p += 2;
			}
		while ( p < s+8+2 )
			*p++ = ' ';
		}

	return TRUE;
	}
/*...e*/

/*...sdis_init:0:*/
void dis_init(void)
	{
	build_decode_tables();
	}
/*...e*/
/*...sdis_instruction:0:*/
BOOLEAN dis_instruction(word *a, char *s)
	{
	return disassemble(a, s);
	}
/*...e*/
/*...sdis_ref_code:0:*/
BOOLEAN dis_ref_code(word a, word *r)
	{
	byte op;
	if ( !read8(a++, &op) )
		return FALSE;

	if (  op       == 0xc3 || // jp nn
	     (op&0xc7) == 0xc2 || // jp cc,nn
	      op       == 0xcd || // call nn
	     (op&0xc7) == 0xc4 )  // call cc,nn
		return read16(a, r);

	if ( op == 0x18 || // jr
	     op == 0x38 || // jr c
	     op == 0x30 || // jr nc
	     op == 0x28 || // jr z
	     op == 0x20 || // jr nz
	     op == 0x10 )  // djnz
		{
		byte e;
		if ( !read8(a++, &e) )
			return FALSE;
		(*r) = (word) ( a+(sword)(sbyte)e );
		return TRUE;
		}		

	if ( (op&0xc7) == 0xc7 ) // rst
		{
		(*r) = (word) (op&0x38);
		return TRUE;
		}

	return FALSE;
	}
/*...e*/
