//#define MEMAS_IMPLEMENTATION
#ifndef MEMAS_H_
#define MEMAS_H_

#ifndef NO_STD_HEADERS
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <syscall.h>
#endif

#ifdef  MEMAS_IMPLEMENTATION
#define BYTE_BUFFER_IMPLEMENTATION
#endif

#include "da.h"
#include "byte_buffer.h"

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

typedef signed   char sbyte;
typedef unsigned char ubyte;

/*
70 cb JO	rel8 Jump short if overflow (OF=1).

71 cb JNO	rel8 Jump short if not overflow (OF=0).

72 cb JB	rel8 Jump short if below (CF=1).
72 cb JC	rel8 Jump short if carry (CF=1).
72 cb JNAE	rel8 Jump short if not above or equal (CF=1).

73 cb JAE	rel8 Jump short if above or equal (CF=0).
73 cb JNB	rel8 Jump short if not below (CF=0).
73 cb JNC	rel8 Jump short if not carry (CF=0).

74 cb JE	rel8 Jump short if equal (ZF=1).
74 cb JZ	rel8 Jump short if zero (ZF = 1).

75 cb JNE	rel8 Jump short if not equal (ZF=0).
75 cb JNZ	rel8 Jump short if not zero (ZF=0).

76 cb JBE	rel8 Jump short if below or equal (CF=1 or ZF=1).
76 cb JNA	rel8 Jump short if not above (CF=1 or ZF=1).

77 cb JA	rel8 Jump short if above (CF=0 and ZF=0).
77 cb JNBE	rel8 Jump short if not below or equal (CF=0 and ZF=0).

78 cb JS	rel8 Jump short if sign (SF=1).

79 cb JNS	rel8 Jump short if not sign (SF=0).

7A cb JP	rel8 Jump short if parity (PF=1).
7A cb JPE	rel8 Jump short if parity even (PF=1).

7B cb JNP	rel8 Jump short if not parity (PF=0).
7B cb JPO	rel8 Jump short if parity odd (PF=0).

7C cb JL	rel8 Jump short if less (SF≠ OF).
7C cb JNGE	rel8 Jump short if not greater or equal (SF≠ OF).

7D cb JGE	rel8 Jump short if greater or equal (SF=OF).
7D cb JNL	rel8 Jump short if not less (SF=OF).

7E cb JLE	rel8 Jump short if less or equal (ZF=1 or SF≠ OF).
7E cb JNG	rel8 Jump short if not greater (ZF=1 or SF≠ OF).

7F cb JG	rel8 Jump short if greater (ZF=0 and SF=OF).
7F cb JNLE	rel8 Jump short if not less or equal (ZF=0 and SF=OF).

E3 cb JCXZ	rel8 Jump short if CX register is 0.
E3 cb JECXZ rel8 Jump short if ECX register is 0.
E3 cb JRCXZ rel8 Jump short if RCX register is 0.
*/

#define ASM_ENUM_OP_CODE_LIST					\
	X(OP_JO)									\
	X(OP_JNO)								    \
	X(OP_JB)								    \
	X(OP_JAE)								    \
	X(OP_JE)								    \
	X(OP_JNE)								    \
	X(OP_JBE)								    \
	X(OP_JA)								    \
	X(OP_JS)								    \
	X(OP_JNS)								    \
	X(OP_JP)								    \
	X(OP_JNP)								    \
	X(OP_JL)								    \
	X(OP_JGE)								    \
	X(OP_JLE)								    \
	X(OP_JG)								    \
	X(OP_SETO)									\
	X(OP_SETNO)								    \
	X(OP_SETB)								    \
	X(OP_SETAE)								    \
	X(OP_SETE)								    \
	X(OP_SETNE)								    \
	X(OP_SETBE)								    \
	X(OP_SETA)								    \
	X(OP_SETS)								    \
	X(OP_SETNS)								    \
	X(OP_SETP)								    \
	X(OP_SETNP)								    \
	X(OP_SETL)								    \
	X(OP_SETGE)								    \
	X(OP_SETLE)								    \
	X(OP_SETG)								    \
	X(OP_JMP)								    \
	X(OP_CALL)								    \
	X(OP_RET)								    \
	X(OP_PUSH)								    \
	X(OP_POP)								    \
	X(OP_MOV)								    \
	X(OP_MOVSWD)								\
	X(OP_MOVSBD)								\
	X(OP_MOVSBQ)								\
	X(OP_MOVSDQ)								\
	X(OP_MOVZWD)								\
	X(OP_MOVZBD)								\
	X(OP_MOVZBQ)								\
	X(OP_MOVZDQ)								\
	X(OP_CWD)									\
	X(OP_CDQ)									\
	X(OP_CQO)									\
	X(OP_ADD)									\
	X(OP_SUB)									\
	X(OP_CMP)									\
	X(OP_IMUL)									\
	X(OP_MUL)									\
	X(OP_DIV)									\
	X(OP_IDIV)									\
	X(OP_SHR)									\
	X(OP_SHL)									\
	X(OP_SAR)									\
	X(OP_XOR)									\
	X(OP_AND)									\
	X(OP_OR)									\
	X(OP_NOT)									\
	X(OP_NEG)									\
	X(OP_LEA)									\
	X(OP_SYSCALL)								\
	X(OP_NOP)									\
	X(DIR_IGNORE)								\
	X(DIR_LABEL)								\
	X(DIR_REP)									\
	X(DIR_INT)									\
	X(DIR_ALIGN)								\
	X(DIR_STRING)								\
	X(OP_CODE_COUNT)

#define OP_SAL OP_SHL

#define ASM_ENUM_ARG_TAG_LIST					\
	X(ARG_IMM)									\
	X(ARG_REG)								    \
	X(ARG_LABEL)							    \
	X(ARG_MEM_DR)							    \
	X(ARG_MEM_DRX)							    \
	X(ARG_MEM_DRXS)							    \
	X(ARG_MEM_LR)							    \
	X(ARG_MEM_LRX)							    \
	X(ARG_MEM_LRXS)

#define ASM_ENUM_OP_SIZE_LIST  X(ZB) X(ZW) X(ZD) X(ZQ)
#define ASM_ENUM_SCALE_LIST    X(SB) X(SW) X(SD) X(SQ)
#define ASM_ENUM_ARG_DESC_LIST X(IR) X(RR) X(RM) X(MR) X(IM)
#define ASM_ENUM_REGISTER_LIST					\
	X(RAX)										\
	X(RCX)									    \
	X(RDX)									    \
	X(RBX)									    \
	X(RSP)									    \
	X(RBP)									    \
	X(RSI)									    \
	X(RDI)									    \
	X(R8)									    \
	X(R9)									    \
	X(R10)									    \
	X(R11)									    \
	X(R12)									    \
	X(R13)									    \
	X(R14)									    \
	X(R15)


#define X(name) name,
enum asm_op_code  {ASM_ENUM_OP_CODE_LIST};
enum asm_op_size  {ASM_ENUM_OP_SIZE_LIST};
enum asm_scale    {ASM_ENUM_SCALE_LIST};
enum asm_arg_desc {ASM_ENUM_ARG_DESC_LIST};
enum asm_arg_tag  {ASM_ENUM_ARG_TAG_LIST};
enum asm_register {ASM_ENUM_REGISTER_LIST};
#undef X

#define RIP -1

enum asm_cc { // condition codes
	CC_O,
	CC_NO,
	CC_B,
	CC_AE,
	CC_E,
	CC_NE,
	CC_BE,
	CC_A,
	CC_S,
	CC_NS,
	CC_P,
	CC_NP,
	CC_L,
	CC_GE,
	CC_LE,
	CC_G,
};

/* 	RAX = 0x0, 0.000 (0)   AL        AX    EAX   RAX  ST0  MMX0  XMM0   YMM0   ES    CR0   DR0  */
/* 	RCX = 0x1, 0.001 (1)   CL        CX    ECX   RCX  ST1  MMX1  XMM1   YMM1   CS    CR1   DR1  */
/* 	RDX = 0x2, 0.010 (2)   DL        DX    EDX   RDX  ST2  MMX2  XMM2   YMM2   SS    CR2   DR2  */
/* 	RBX = 0x3, 0.011 (3)   BL        BX    EBX   RBX  ST3  MMX3  XMM3   YMM3   DS    CR3   DR3  */
/*  RSP = 0x4, 0.100 (4)   AH, SPL   SP    ESP   RSP  ST4  MMX4  XMM4   YMM4   FS    CR4   DR4  */
/* 	RBP = 0x5, 0.101 (5)   CH, BPL   BP    EBP   RBP  ST5  MMX5  XMM5   YMM5   GS    CR5   DR5  */
/* 	RSI = 0x6, 0.110 (6)   DH, SIL   SI    ESI   RSI  ST6  MMX6  XMM6   YMM6   -     CR6   DR6  */
/* 	RDI = 0x7, 0.111 (7)   BH, DIL   DI    EDI   RDI  ST7  MMX7  XMM7   YMM7   -     CR7   DR7  */
/* 	R8  = 0x8, 1.000 (8)   R8L       R8W   R8D   R8   -    MMX0  XMM8   YMM8   ES    CR8   DR8  */
/* 	R9  = 0x9, 1.001 (9)   R9L       R9W   R9D   R9   -    MMX1  XMM9   YMM9   CS    CR9   DR9  */
/* 	R10 = 0xa, 1.010 (10)  R10L      R10W  R10D  R10  -    MMX2  XMM10  YMM10  SS    CR10  DR10 */
/* 	R11 = 0xb, 1.011 (11)  R11L      R11W  R11D  R11  -    MMX3  XMM11  YMM11  DS    CR11  DR11 */
/* 	R12 = 0xc, 1.100 (12)  R12L      R12W  R12D  R12  -    MMX4  XMM12  YMM12  FS    CR12  DR12 */
/* 	R13 = 0xd, 1.101 (13)  R13L      R13W  R13D  R13  -    MMX5  XMM13  YMM13  GS    CR13  DR13 */
/* 	R14 = 0xe, 1.110 (14)  R14L      R14W  R14D  R14  -    MMX6  XMM14  YMM14  -     CR14  DR14 */
/* 	R15 = 0xf, 1.111 (15)  R15L      R15W  R15D  R15  -    MMX7  XMM15  YMM15  -     CR15  DR15 */

typedef uint32_t asm_label_id;

typedef struct asm_inst {
	enum asm_op_code  op    : 11;
	enum asm_arg_desc dsc   : 3;
	enum asm_op_size  sz    : 2;  // size suffix
	enum asm_scale    sc    : 2;  // scale
	enum asm_register r0    : 4;  // register
	enum asm_register r1    : 4;  // base register
	enum asm_register rx    : 4;  // flags / index register
	bool              t_x   : 1;  // use rx as flags register
	bool              t_lbl : 1;  // disp is label
	int32_t           disp;
	int64_t  imm;
} Asm_inst;

enum asm_label_type {
	ASM_LBL_T_CONST,
	ASM_LBL_T_BLOCK,
	ASM_LBL_T_FUNC,
	ASM_LBL_T_DATA,
};

enum asm_label_binding {
	ASM_LBL_B_LOCAL,
	ASM_LBL_B_GLOBAL,
};

struct asm_lbl_back_patch {
	int64_t offset;           /* patch value = label.offset + back_patch.offset */
	int64_t loc;               /* offset into buffer */
	int     size;              /* write size */
};

typedef struct asm_label {
	const char *name;
	int64_t offset;
	struct {
		uint32_t len, cap;
		struct asm_lbl_back_patch *elems;
	} patches;
	enum asm_label_type type;
	enum asm_label_binding binding;
	bool is_resolved;
	bool is_extern;
	bool is_init;           // add to .init_array
	bool is_fini;           // add to .fini_array
} Asm_label;

static_assert(sizeof(Asm_inst) == sizeof(int64_t)*2);

typedef struct asm_module {
	struct {
		uint32_t len, cap;
		Asm_label *elems;
	} labels;
	Byte_buffer as;
	Byte_buffer mc;
	bool is_jit;
} Asm_module;

typedef struct asm_arg {
	enum asm_arg_tag tag;
	int64_t i;
	struct {
		int32_t dsp, base, idx, scale;
	} addr;
} Asm_arg;

/* Assembler api */
void asm_inst0(Asm_module *m, enum asm_op_code op, enum asm_op_size size);
void asm_inst1(Asm_module *m, enum asm_op_code op, enum asm_op_size size, Asm_arg src);
void asm_inst2(Asm_module *m, enum asm_op_code op, enum asm_op_size size, Asm_arg src, Asm_arg dst);

struct make_label_opt_args {
	int64_t offset;
	bool is_init;
	bool is_fini;
	bool is_extern;
};

#define asm_make_label(m, name, type, binding, ...)						\
	asm_make_label_impl(m, name, type, binding, (struct make_label_opt_args){__VA_ARGS__})

asm_label_id asm_make_label_impl(Asm_module *m,
								 const char *name,
								 enum asm_label_type type,
								 enum asm_label_binding binding,
								 struct make_label_opt_args opt);
void asm_dir_label(Asm_module *m, asm_label_id label);
void asm_dir_align(Asm_module *m, int64_t alignment);
void asm_dir_rep(Asm_module *m, int64_t n);
void asm_dir_string(Asm_module *m, const char *s);
void asm_dir_int(Asm_module *m, enum asm_op_size sz, Asm_arg x);
void asm_set_executable(Asm_module *m);
void asm_assemble(Asm_module *m);
void asm_assemble_for_jit(Asm_module *m);
void asm_assemble_for_object_file(Asm_module *m);
int64_t asm_get_label_offset(Asm_module *m, asm_label_id lbl);
void *asm_get_label_address(Asm_module *m, asm_label_id lbl);
bool asm_lookup_label_id(Asm_module *m, const char *name, asm_label_id *id_out);
bool asm_lookup_label_address(Asm_module *m, const char *name, void **addr_out);
void asm_dump_bytes(Asm_module *m);
Asm_inst *asm_get_instruction_array(Asm_module *m);
size_t asm_get_instruction_count(Asm_module *m);
uint32_t asm_get_insertion_point(Asm_module *m);
void asm_set_insertion_point(Asm_module *m, uint32_t index);
void asm_set_insertion_at_start(Asm_module *m);
void asm_set_insertion_at_end(Asm_module *m);
const char *asm_op_code_to_str(enum asm_op_code e);
const char *asm_op_size_to_str(enum asm_op_size e);
const char *asm_scale_to_str(enum asm_scale e);
const char *asm_arg_desc_to_str(enum asm_arg_desc e);
const char *asm_arg_tag_to_str(enum asm_arg_tag e);

struct _asm_init_args { bool is_jit; };
Asm_module *asm_init_module_impl(Asm_module *m, struct _asm_init_args opt);
#define asm_init_module(M, ...)										\
	asm_init_module_impl(M, (struct _asm_init_args){__VA_ARGS__})

#define asm_mov(module, size, src, dst) asm_inst2(module, OP_MOV, size, src, dst)
#define asm_movswd(module, src, dst)    asm_inst2(module, OP_MOVSWD, 0, src, dst)
#define asm_movsdq(module, src, dst)    asm_inst2(module, OP_MOVSDQ, 0, src, dst)
#define asm_movsbd(module, src, dst)    asm_inst2(module, OP_MOVSBD, 0, src, dst)
#define asm_movsbq(module, src, dst)    asm_inst2(module, OP_MOVSBQ, 0, src, dst)
#define asm_movzwd(module, src, dst)    asm_inst2(module, OP_MOVZWD, 0, src, dst)
#define asm_movzdq(module, src, dst)    asm_inst2(module, OP_MOVZDQ, 0, src, dst)
#define asm_movzbd(module, src, dst)    asm_inst2(module, OP_MOVZBD, 0, src, dst)
#define asm_movzbq(module, src, dst)    asm_inst2(module, OP_MOVZBQ, 0, src, dst)
#define asm_add(module, size, src, dst) asm_inst2(module, OP_ADD, size, src, dst)
#define asm_sub(module, size, src, dst) asm_inst2(module, OP_SUB, size, src, dst)
#define asm_cmp(module, size, src, dst) asm_inst2(module, OP_CMP, size, src, dst)
#define asm_and(module, size, src, dst) asm_inst2(module, OP_AND, size, src, dst)
#define asm_or(module, size, src, dst)  asm_inst2(module, OP_OR, size, src, dst)
#define asm_xor(module, size, src, dst) asm_inst2(module, OP_XOR, size, src, dst)
#define asm_sal(module, size, src, dst) asm_inst2(module, OP_SAL, size, src, dst)
#define asm_sar(module, size, src, dst) asm_inst2(module, OP_SAR, size, src, dst)
#define asm_shl(module, size, src, dst) asm_inst2(module, OP_SHL, size, src, dst)
#define asm_shr(module, size, src, dst) asm_inst2(module, OP_SHR, size, src, dst)
#define asm_lea(module, size, src, dst) asm_inst2(module, OP_LEA, size, src, dst)
#define asm_ret(module)                 asm_inst0(module, OP_RET, 0)
#define asm_syscall(module)             asm_inst0(module, OP_SYSCALL, 0)
#define asm_cwd(module)                 asm_inst0(module, OP_CWD, 0)
#define asm_cdq(module)                 asm_inst0(module, OP_CDQ, 0)
#define asm_cqo(module)                 asm_inst0(module, OP_CQO, 0)
#define asm_jmp(module, dst)            asm_inst1(module, OP_JMP, 0, dst)
#define asm_call(module, dst)           asm_inst1(module, OP_CALL, 0, dst)
#define asm_push(module, size, dst)     asm_inst1(module, OP_PUSH, size, dst)
#define asm_pop(module, size, dst)      asm_inst1(module, OP_POP, size, dst)
#define asm_div(module, size, dst)		asm_inst1(module, OP_DIV, size, dst)
#define asm_idiv(module, size, dst)		asm_inst1(module, OP_IDIV, size, dst)
#define asm_neg(module, size, dst)		asm_inst1(module, OP_NEG, size, dst)
#define asm_not(module, size, dst)		asm_inst1(module, OP_NOT, size, dst)
#define asm_jo(module, dst)             asm_inst1(module, OP_JO,  0, dst)
#define asm_jno(module, dst) 			asm_inst1(module, OP_JNO, 0, dst)
#define asm_jb(module, dst) 			asm_inst1(module, OP_JB,  0, dst)
#define asm_jc(module, dst) 			asm_inst1(module, OP_JB,  0, dst)
#define asm_jae(module, dst) 			asm_inst1(module, OP_JAE, 0, dst)
#define asm_jnb(module, dst) 			asm_inst1(module, OP_JAE, 0, dst)
#define asm_jnc(module, dst) 			asm_inst1(module, OP_JAE, 0, dst)
#define asm_je(module, dst) 			asm_inst1(module, OP_JE,  0, dst)
#define asm_jz(module, dst) 			asm_inst1(module, OP_JE,  0, dst)
#define asm_jne(module, dst) 			asm_inst1(module, OP_JNE, 0, dst)
#define asm_jnz(module, dst) 			asm_inst1(module, OP_JNE, 0, dst)
#define asm_jbe(module, dst) 			asm_inst1(module, OP_JBE, 0, dst)
#define asm_ja(module, dst) 			asm_inst1(module, OP_JA,  0, dst)
#define asm_js(module, dst) 			asm_inst1(module, OP_JS,  0, dst)
#define asm_jns(module, dst) 			asm_inst1(module, OP_JNS, 0, dst)
#define asm_jp(module, dst) 			asm_inst1(module, OP_JP,  0, dst)
#define asm_jpe(module, dst) 			asm_inst1(module, OP_JP,  0, dst)
#define asm_jnp(module, dst) 			asm_inst1(module, OP_JNP, 0, dst)
#define asm_jpo(module, dst) 			asm_inst1(module, OP_JNP, 0, dst)
#define asm_jl(module, dst) 			asm_inst1(module, OP_JL,  0, dst)
#define asm_jge(module, dst) 			asm_inst1(module, OP_JGE, 0, dst)
#define asm_jle(module, dst) 			asm_inst1(module, OP_JLE, 0, dst)
#define asm_jg(module, dst) 			asm_inst1(module, OP_JG,  0, dst)
#define asm_seto(module, dst)           asm_inst1(module, OP_SETO,  0, dst)
#define asm_setno(module, dst) 			asm_inst1(module, OP_SETNO, 0, dst)
#define asm_setb(module, dst) 			asm_inst1(module, OP_SETB,  0, dst)
#define asm_setc(module, dst) 			asm_inst1(module, OP_SETB,  0, dst)
#define asm_setae(module, dst) 			asm_inst1(module, OP_SETAE, 0, dst)
#define asm_setnb(module, dst) 			asm_inst1(module, OP_SETAE, 0, dst)
#define asm_setnc(module, dst) 			asm_inst1(module, OP_SETAE, 0, dst)
#define asm_sete(module, dst) 			asm_inst1(module, OP_SETE,  0, dst)
#define asm_setz(module, dst) 			asm_inst1(module, OP_SETE,  0, dst)
#define asm_setne(module, dst) 			asm_inst1(module, OP_SETNE, 0, dst)
#define asm_setnz(module, dst) 			asm_inst1(module, OP_SETNE, 0, dst)
#define asm_setbe(module, dst) 			asm_inst1(module, OP_SETBE, 0, dst)
#define asm_seta(module, dst) 			asm_inst1(module, OP_SETA,  0, dst)
#define asm_sets(module, dst) 			asm_inst1(module, OP_SETS,  0, dst)
#define asm_setns(module, dst) 			asm_inst1(module, OP_SETNS, 0, dst)
#define asm_setp(module, dst) 			asm_inst1(module, OP_SETP,  0, dst)
#define asm_setpe(module, dst) 			asm_inst1(module, OP_SETP,  0, dst)
#define asm_setnp(module, dst) 			asm_inst1(module, OP_SETNP, 0, dst)
#define asm_setpo(module, dst) 			asm_inst1(module, OP_SETNP, 0, dst)
#define asm_setl(module, dst) 			asm_inst1(module, OP_SETL,  0, dst)
#define asm_setge(module, dst) 			asm_inst1(module, OP_SETGE, 0, dst)
#define asm_setle(module, dst) 			asm_inst1(module, OP_SETLE, 0, dst)
#define asm_setg(module, dst) 			asm_inst1(module, OP_SETG,  0, dst)
#define asm_jcc(module, cc, dst)        asm_inst1(module, (OP_JO & 0xf0)|((cc)&0x0f), 0, dst)
#define asm_setcc(module, cc, dst)      asm_inst1(module, (OP_SETO & 0xf0)|((cc) & 0x0f), 0, dst)

#define INT(x)                  ((Asm_arg){.tag=ARG_IMM,      .i=x})
#define REG(x)                  ((Asm_arg){.tag=ARG_REG,      .i=x})
#define LABEL(L, D)             ((Asm_arg){.tag=ARG_LABEL,    .i=D, .addr = {.dsp=L}})
#define MEM_DR(D, R)		    ((Asm_arg){.tag=ARG_MEM_DR,   .addr = {.dsp=D, .base=R}})
#define MEM_DRX(D, R, X)	    ((Asm_arg){.tag=ARG_MEM_DRX,  .addr = {.dsp=D, .base=R, .idx=X}})
#define MEM_DRXS(D, R, X, S)    ((Asm_arg){.tag=ARG_MEM_DRXS, .addr = {.dsp=D, .base=R, .idx=X, .scale=S}})
#define MEM_LR(L, D, R)         ((Asm_arg){.tag=ARG_MEM_LR,   .i=D, .addr = {.dsp=L, .base=R}})
#define MEM_LRX(L, D, R, X)	    ((Asm_arg){.tag=ARG_MEM_LRX,  .i=D, .addr = {.dsp=L, .base=R, .idx=X}})
#define MEM_LRXS(L, D, R, X, S) ((Asm_arg){.tag=ARG_MEM_LRXS, .i=D, .addr = {.dsp=L, .base=R, .idx=X, .scale=S}})

/* Elf builder api */
#include "elf.h"

enum asm_eb_section {
	ASM_SH_NULL,
	ASM_SH_INTERP,
	ASM_SH_INIT_ARRAY,
	ASM_SH_FINI_ARRAY,
	ASM_SH_TEXT,
	ASM_SH_DATA,
	ASM_SH_RODATA,
	ASM_SH_BSS,
	ASM_SH_RELA_TEXT,
	ASM_SH_RELA_INIT_ARRAY,
	ASM_SH_SYMTAB,
	ASM_SH_STRTAB,
	ASM_SH_SHSTRTAB,
	ASM_SH_COUNT,
};

typedef struct asm_elf_builder {
	Elf64_header hdr;
	struct {
		uint32_t len, cap;
		Elf64_program_header *elems;
	} phs;
	Elf64_section_header shs[ASM_SH_COUNT];
	Byte_buffer bufs[ASM_SH_COUNT];
} Asm_elf_builder;

Asm_elf_builder asm_make_elf_builder(enum elf_file_type e_type);
elf64_word asm_elf_add_to_strtab(Asm_elf_builder *eb, const char *name);
elf64_word asm_elf_add_to_shstrtab(Asm_elf_builder *eb, const char *name);
elf64_word asm_elf_add_symbol(Asm_elf_builder *eb,
							  const char *name,
							  enum elf_symbol_type type,
							  enum elf_symbol_binding binding,
							  elf64_half shndx,
							  elf64_addr value,
							  elf64_xword size);
void asm_output_object_file(Asm_module *m, const char *filename);
void asm_output_static_executable(Asm_module *m, asm_label_id entry_lbl, const char *filename);

#endif /* MEMAS_H_ */

#ifdef MEMAS_IMPLEMENTATION

#ifndef MALLOC
#define MALLOC malloc
#endif
#ifndef REALLOC
#define REALLOC realloc
#endif
#ifndef FREE
#define FREE free
#endif
#ifndef ASSERT
#define ASSERT assert
#endif

#ifndef INIT_BUFFER_SIZE
#define INIT_BUFFER_SIZE PAGE_SIZE
#endif

#ifndef FAILWITH
#define FAILWITH(_fmt_msg, ...)										\
	do {															\
		fflush(stdout);												\
		fflush(stderr);												\
		fprintf(stderr, "%s: %d: [FAILWITH] ", __FILE__, __LINE__); \
		fprintf(stderr, _fmt_msg __VA_OPT__(,) __VA_ARGS__);		\
		fputc('\n', stderr);										\
		__asm__("int3");											\
		exit(1);													\
	} while (0)
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096LU
#endif

#define ARG_MATCH1(dtag)       (dst.tag == (dtag))
#define ARG_MATCH2(stag, dtag) ((((stag) & 0x0f) << 4) | ((dtag) & 0x0f))
#define SZDSC(_sz, _dsc)       (((_sz) << 3)|(_dsc))

/*
  REX layout
    7                           0
  +---+---+---+---+---+---+---+---+
  | 0   1   0   0 | W | R | X | B |
  +---+---+---+---+---+---+---+---+
*/
#define REX(W, R, X, B) ((1 << 6)|(((W)&1) << 3)|(((X)&1) << 1)|(((R)&1) << 2)|(((B)&1) << 0))
/*
  ModR/M layout
    7                           0
  +---+---+---+---+---+---+---+---+
  |  mod  |    reg    |     rm    |
  +---+---+---+---+---+---+---+---+
*/
#define MODRM(_mod, _r, _rm) ((((_mod)&3) << 6)|(((_r)&7) << 3)|(((_rm)&7) << 0))

/*
  SIB layout
    7                           0
  +---+---+---+---+---+---+---+---+
  | scale |   index   |    base   |
  +---+---+---+---+---+---+---+---+
*/
#define SIB(_s, _x, _b) ((((_s)&3) << 6)|(((_x)&7) << 3)|(((_b)&7) << 0))

#define NOP_BYTE  0x90
#define OPOR_BYTE 0x66 /* operand size overide prefix */

#define ASM_APPEND(M, ...)											\
	do {															\
		auto _VAR = __VA_ARGS__;									\
		byte_buffer_insert_bytes(&(M)->as, &_VAR, sizeof(_VAR));	\
	} while (0)

#if ASM_USE_STATIC_BUFFER
#ifndef ASM_STATIC_BUFFER_SIZE
#define ASM_STATIC_BUFFER_SIZE ((1<<10)*64)
#endif
__attribute__((aligned(4096)))
static byte_t jit_buffer[ASM_STATIC_BUFFER_SIZE];
#endif

#define SWITCH_TO_STR(E, LIST) switch (E) { LIST default: FAILWITH("Unreachable"); break; } return NULL
#define X(name) case name: return #name;

const char *
asm_op_code_to_str(enum asm_op_code e)
{
	SWITCH_TO_STR(e, ASM_ENUM_OP_CODE_LIST);
}

const char *
asm_op_size_to_str(enum asm_op_size e)
{
	SWITCH_TO_STR(e, ASM_ENUM_OP_SIZE_LIST);
}

const char *
asm_scale_to_str(enum asm_scale e)
{
	SWITCH_TO_STR(e, ASM_ENUM_SCALE_LIST);
}

const char *
asm_arg_desc_to_str(enum asm_arg_desc e)
{
	SWITCH_TO_STR(e, ASM_ENUM_ARG_DESC_LIST);
}

const char *
asm_arg_tag_to_str(enum asm_arg_tag e)
{
	SWITCH_TO_STR(e, ASM_ENUM_ARG_TAG_LIST);
}

#undef SWITCH_TO_STR
#undef X

#ifndef popcnt
static inline int
popcnt(uint64_t n)
{
	int c;
	for (c = 0; n; ++c) n &= n - 1;
	return c;
}
#endif

static inline uint64_t
align_next(uint64_t n, uint64_t alignment)
{
	if (popcnt(alignment) != 1) {
		printf("popcnt(alignment) = %d\n", popcnt(alignment));
	}
	ASSERT(popcnt(alignment) == 1); /* alignment must be a power of 2 */
	uint64_t mask = alignment - 1;
	return (n + mask) & ~mask;
}

Asm_inst *
asm_get_instruction_array(Asm_module *m)
{
	return (Asm_inst *)m->as.data;
}

size_t
asm_get_instruction_count(Asm_module *m)
{
	return m->as.len / sizeof(Asm_inst);
}

uint32_t
asm_get_insertion_point(Asm_module *m)
{
	return byte_buffer_get_cursor_offset(&m->as) / sizeof(Asm_inst);
}

void
asm_set_insertion_point(Asm_module *m, uint32_t index)
{
	size_t offset = index * sizeof(Asm_inst);
	ASSERT(offset <= m->as.len);
	byte_buffer_set_cursor_offset(&m->as, offset);
}

void
asm_set_insertion_at_start(Asm_module *m)
{
	byte_buffer_set_cursor_start(&m->as);
}

void
asm_set_insertion_at_end(Asm_module *m)
{
	byte_buffer_set_cursor_end(&m->as);
}


void
asm_inst0(Asm_module *m, enum asm_op_code op, enum asm_op_size size)
{
	ASM_APPEND(m, (Asm_inst){.op = op, .sz = size});
}

void
asm_inst1(Asm_module *m, enum asm_op_code op, enum asm_op_size size, Asm_arg dst)
{
	if (ARG_MATCH1(ARG_MEM_LR)) {
		if (dst.addr.base == RIP) {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_lbl = true,
					.t_x   = true,
					.rx    = 1,
					.disp  = dst.addr.dsp,
					.imm   = dst.i,
				});
		} else {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_lbl = true,
					.t_x   = true,
					.r1	   = dst.addr.base,
					.disp  = dst.addr.dsp,
					.imm   = dst.i,
				});
		}
	} else if (ARG_MATCH1(ARG_MEM_DR)) {
		if (dst.addr.base == RIP) {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_x   = true,
					.rx    = 1,
					.disp  = dst.addr.dsp,
					.imm   = dst.i,
				});
		} else {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_x   = true,
					.r1	   = dst.addr.base,
					.disp  = dst.addr.dsp,
					.imm   = dst.i,
				});
		}
	} else if (ARG_MATCH1(ARG_LABEL)) {
		ASM_APPEND(m, (Asm_inst){
				.op	   = op,
				.dsc   = IR,
				.sz	   = size,
				.t_lbl = true,
				.t_x   = true,
				.disp  = dst.addr.dsp,
				.imm   = dst.i,
			});
	} else if (ARG_MATCH1(ARG_IMM)) {
		ASM_APPEND(m, (Asm_inst){
				.op	   = op,
				.dsc   = IR,
				.sz	   = size,
				.t_x   = true,
				.imm   = dst.i,

			});
	} else if (ARG_MATCH1(ARG_REG)) {
		ASSERT(dst.i != RIP);
		ASM_APPEND(m, (Asm_inst){
				.op	   = op,
				.dsc   = RR,
				.sz	   = size,
				.r1    = dst.i,
			});
	} else {
#ifndef NO_STD_HEADERS
		FAILWITH("TODO: asm_inst1 %s, %s, %s",
				 asm_op_code_to_str(op),
				 asm_op_size_to_str(size),
				 asm_arg_tag_to_str(dst.tag));
#else
		FAILWITH(DEBUG_MSG("TODO: asm_inst1"));
#endif
	}
}

void
asm_inst2(Asm_module *m, enum asm_op_code op, enum asm_op_size size, Asm_arg src, Asm_arg dst)
{
	switch (ARG_MATCH2(src.tag, dst.tag)) {
	case ARG_MATCH2(ARG_REG, ARG_REG):
		ASSERT(src.i != RIP);
		ASSERT(dst.i != RIP);
		ASM_APPEND(m, (Asm_inst){
				.op  = op,
				.dsc = RR,
				.sz  = size,
				.r0  = src.i,
				.r1  = dst.i,
			});
		break;
	case ARG_MATCH2(ARG_LABEL, ARG_REG):
		ASSERT(dst.i != RIP);
		ASM_APPEND(m, (Asm_inst){
				.op  = op,
				.dsc = IR,
				.sz  = size,
				.t_lbl = true,
				.t_x   = true,
				.r1  = dst.i,
				.disp = src.addr.dsp,
				.imm  = src.i,
			});
		break;
	case ARG_MATCH2(ARG_IMM, ARG_REG):
		ASSERT(dst.i != RIP);
		ASM_APPEND(m, (Asm_inst){
				.op  = op,
				.dsc = IR,
				.sz  = size,
				.r1  = dst.i,
				.imm = src.i,
			});
		break;
	case ARG_MATCH2(ARG_MEM_LR, ARG_REG):
		ASSERT(dst.i != RIP);
		if (src.addr.base == RIP) {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_lbl = true,
					.t_x   = true,
					.r0	   = dst.i,
					.rx    = 1,
					.disp  = src.addr.dsp,
					.imm   = src.i
				});
		} else {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_lbl = true,
					.t_x   = true,
					.r0	   = dst.i,
					.r1	   = src.addr.base,
					.disp  = src.addr.dsp,
					.imm   = src.i
				});
		}
		break;
	case ARG_MATCH2(ARG_REG, ARG_MEM_LR):
		ASSERT(src.i != RIP);
		if (dst.addr.base == RIP) {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = RM,
					.sz	   = size,
					.t_lbl = true,
					.t_x   = true,
					.r0	   = src.i,
					.rx    = 1,
					.disp  = dst.addr.dsp,
					.imm   = dst.i
				});
		} else {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = RM,
					.sz	   = size,
					.t_lbl = true,
					.t_x   = true,
					.r0	   = src.i,
					.r1	   = dst.addr.base,
					.disp  = dst.addr.dsp,
					.imm   = dst.i
				});
		}
		break;
	case ARG_MATCH2(ARG_MEM_DR, ARG_REG):
		ASSERT(dst.i != RIP);
		if (src.addr.base == RIP) {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_x   = true,
					.r0	   = dst.i,
					.rx    = 1,
					.disp  = src.addr.dsp,
				});
		} else {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = MR,
					.sz	   = size,
					.t_x   = true,
					.r0	   = dst.i,
					.r1	   = src.addr.base,
					.disp  = src.addr.dsp,
				});
		}
		break;
	case ARG_MATCH2(ARG_MEM_DRXS, ARG_REG):
		ASSERT(dst.i != RIP);
		ASSERT(src.addr.base != RIP);
		ASM_APPEND(m, (Asm_inst){
				.op	  = op,
				.dsc  = MR,
				.sz	  = size,
				.r0	  = dst.i,
				.r1	  = src.addr.base,
				.rx   = src.addr.idx,
				.sc   = src.addr.scale,
				.disp = src.addr.dsp,
			});
		break;
	case ARG_MATCH2(ARG_REG, ARG_MEM_DR):
		ASSERT(dst.i != RIP);
		if (src.addr.base == RIP) {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = RM,
					.sz	   = size,
					.t_x   = true,
					.r0	   = dst.i,
					.rx    = 1,
					.disp  = src.addr.dsp,
				});
		} else {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = RM,
					.sz	   = size,
					.t_x   = true,
					.r0	   = src.i,
					.r1	   = dst.addr.base,
					.disp  = dst.addr.dsp,
				});
		}
		break;
	case ARG_MATCH2(ARG_IMM, ARG_MEM_DR):
		if (src.addr.base == RIP) {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = IM,
					.sz	   = size,
					.t_x   = true,
					.rx    = 1,
					.disp  = dst.addr.dsp,
					.imm   = src.i,
				});
		} else {
			ASM_APPEND(m, (Asm_inst){
					.op	   = op,
					.dsc   = IM,
					.sz	   = size,
					.t_x   = true,
					.r1	   = dst.addr.base,
					.disp  = dst.addr.dsp,
					.imm   = src.i,
				});
		}
		break;
	default:
#ifndef NO_STD_HEADERS
		FAILWITH("TODO: ARG_MATCH2(%s, %s) asm_inst2",
				 asm_arg_tag_to_str(src.tag),
				 asm_arg_tag_to_str(dst.tag));
#else
		FAILWITH(DEBUG_MSG("TODO: asm_inst2"));
#endif
	}
}

asm_label_id
asm_make_label_impl(Asm_module *m,
					const char *name,
					enum asm_label_type type,
					enum asm_label_binding binding,
					struct make_label_opt_args opt)
{
	asm_label_id label_id = m->labels.len;
	da_append(&m->labels, (Asm_label){
			.name    = name,
			.type    = type,
			.binding = binding,
			.offset  = opt.offset,
			.is_init = opt.is_init,
			.is_fini = opt.is_fini,
			.is_extern = opt.is_extern,
		});
	return label_id;
}

void
asm_dir_label(Asm_module *m, asm_label_id label)
{
	ASM_APPEND(m, (Asm_inst){
			.op = DIR_LABEL,
			.t_lbl = true,
			.disp = label,
		});
}

void
asm_dir_align(Asm_module *m, int64_t alignment)
{
	ASM_APPEND(m, (Asm_inst){.op = DIR_ALIGN, .imm = alignment});
}

void
asm_dir_rep(Asm_module *m, int64_t n)
{
	ASM_APPEND(m, (Asm_inst){.op = DIR_REP, .imm = n});
}

void
asm_dir_string(Asm_module *m, const char *s)
{
	ASM_APPEND(m, (Asm_inst){.op = DIR_STRING, .imm = (int64_t)s});
}

void
asm_dir_int(Asm_module *m, enum asm_op_size sz, Asm_arg x)
{
	switch ((int)x.tag) {
	case ARG_IMM:
		ASM_APPEND(m, (Asm_inst){
				.op  = DIR_INT,
				.sz  = sz,
				.imm = x.i
			});
		break;
	case ARG_LABEL:
		ASM_APPEND(m, (Asm_inst){
				.op    = DIR_INT,
				.sz    = sz,
				.t_lbl = true,
				.t_x   = true,
				.disp  = x.addr.dsp,
				.imm   = x.i,
			});
		break;
	default:
#ifndef NO_STD_HEADERS
		FAILWITH("[ERROR] Invalid arg %s", asm_arg_tag_to_str(x.tag));
#else
		FAILWITH(DEBUG_MSG("Invalid arg"));
#endif
	}
}

#ifndef IMPLEMENTATION_ALLOCATORS

static void
asm_alloc_code_buffer(Byte_buffer *buff, size_t size)
{
	void *ptr = mmap(NULL, size,
					 PROT_READ|PROT_WRITE,
					 MAP_PRIVATE|MAP_ANONYMOUS,
					 -1, 0);
	if (ptr == MAP_FAILED) {
#ifndef NO_STD_HEADERS
		FAILWITH("[ERROR] %s\n", strerror(errno));
#else
		FAILWITH(DEBUG_MSG("mmap failed"));
#endif
	}
	buff->cap = size;
	buff->len = 0;
	buff->data = ptr;
}

static void
asm_realloc_code_buffer(Byte_buffer *buff, size_t size)
{
	void *ptr = mremap(buff->data, buff->cap, size, MREMAP_MAYMOVE);
	if (ptr == MAP_FAILED) {
#ifndef NO_STD_HEADERS
		FAILWITH("[ERROR] %s\n", strerror(errno));
#else
		FAILWITH(DEBUG_MSG("mmap failed"));
#endif
	}
	buff->cap = size;
	buff->data = ptr;
}

static void
asm_free_code_buffer(Byte_buffer *buff)
{
	if (munmap(buff->data, buff->cap) == -1) {
#ifndef NO_STD_HEADERS
		FAILWITH("[ERROR] %s\n", strerror(errno));
#else
		FAILWITH(DEBUG_MSG("munmap failed"));
#endif
	}
		buff->cap = 0;
	buff->len = 0;
	buff->data = NULL;
}

void
asm_set_executable(Asm_module *m)
{
	if (mprotect(m->mc.data, m->mc.cap, PROT_READ|PROT_WRITE|PROT_EXEC) == -1) {
#ifndef NO_STD_HEADERS
		FAILWITH("[ERROR] %s\n", strerror(errno));
#else
		FAILWITH(DEBUG_MSG("mprotect failed"));
#endif
	}
}
#endif

Asm_module *
asm_init_module_impl(Asm_module *m, struct _asm_init_args opt)
{
	memset(m, 0, sizeof(*m));
	byte_buffer_init(&m->mc,
					 INIT_BUFFER_SIZE,
					 asm_alloc_code_buffer,
					 asm_realloc_code_buffer,
					 asm_free_code_buffer);
	byte_buffer_init_default(&m->as);
	m->is_jit = opt.is_jit;
	return m;
}

#define EMIT_OPT_REX(_W, r, x, b)										\
	do {																\
		int _R = ((r) >> 3) & 1;										\
		int _X = ((x) >> 3) & 1;										\
		int _B = ((b) >> 3) & 1;										\
		if (opt.force_rex|(_W)|(_R)|(_X)|(_B)) byte_buffer_put_byte(&m->mc, REX(_W, _R, _X, _B)); \
	} while (0)

#define IS_RIP_REL(inst)  ((inst).t_x && (inst).rx == 1)
#define IS_INT8_SIZED(c)  ((c) <= INT8_MAX && (c) >= INT8_MIN)
#define IS_INT32_SIZED(c) ((c) <= INT32_MAX && (c) >= INT32_MIN)

struct _dispatch_options {
	uint32_t sz;
	bool opor      : 1;   /* Emits the operand size overide byte (0x66) */
	bool rex_w     : 1;   /* Sets the w bit of the REX byte */
	bool force_rex : 1;   /* Force an REX byte to be emitted even when it otherwise wouldn't be */
	bool is_mem    : 1;   /* Tells certain emitters that an operand is a memory address
							 rather than a register */
};

typedef void (*fn_emitter)(Asm_module *m, Asm_inst inst, struct _dispatch_options opt);

struct _dispatch {
	fn_emitter fn;
	struct _dispatch_options opt;
};

struct _calc_rip_rel_disp_opt {
	int64_t *disp8_out;
	int64_t *disp32_out;
	int64_t off8;         // offset to field
	int64_t off32;        // offset to field
	size_t *sz_out;
};

struct opcode_bytes {
	ubyte len;
	ubyte code[3];
};

#define OPCODE_BYTES(...)												\
	((struct opcode_bytes){sizeof((ubyte[]){__VA_ARGS__}), {__VA_ARGS__}})

static int64_t
get_disp(Asm_module *m, Asm_inst inst, size_t *sz_out)
{
	int64_t disp = inst.disp;
	if (!inst.t_lbl) {
		if (IS_INT8_SIZED(disp)) {
			*sz_out = sizeof(int8_t);
			return disp;
		}
		ASSERT(IS_INT32_SIZED(disp));
		if (sz_out) *sz_out = sizeof(int32_t);
		return disp;
	}
	Asm_label *lbl = &m->labels.elems[disp];
	switch (lbl->type) {
	case ASM_LBL_T_CONST: FAILWITH("TODO: ASM_LBL_T_CONST"); break;
	case ASM_LBL_T_BLOCK:
	case ASM_LBL_T_DATA:
	case ASM_LBL_T_FUNC:
		if (lbl->is_resolved) {
			disp = lbl->offset + inst.imm;
			if (IS_INT8_SIZED(disp)) {
				if (sz_out) *sz_out = sizeof(int8_t);
				return disp;
			}
			ASSERT(IS_INT32_SIZED(disp));
			if (sz_out) *sz_out = sizeof(int32_t);
			return disp;
		}
		da_append(&m->labels.elems[disp].patches, (struct asm_lbl_back_patch){
				.offset = 0,
				.loc    = byte_buffer_get_cursor_offset(&m->mc),
				.size   = sizeof(int32_t),
			});
		if (sz_out) *sz_out = sizeof(int32_t);
		return disp;
	default:
#ifndef NO_STD_HEADERS
		FAILWITH("Unreachable label: %s", lbl->name);
#else
		FAILWITH(DEBUG_MSG("Unreachable label"));
#endif
	}
}

#define calc_rip_rel_disp(m, inst, ...)			\
	calc_rip_rel_disp_impl(m, inst, ((struct _calc_rip_rel_disp_opt){__VA_ARGS__}))

static int64_t
calc_rip_rel_disp_impl(Asm_module *m, Asm_inst inst, struct _calc_rip_rel_disp_opt opt)
{
	int64_t disp = inst.disp;
	if (!inst.t_lbl) {
		int64_t d8  = disp - (byte_buffer_get_cursor_offset(&m->mc) + opt.off8  + sizeof(int8_t));
		int64_t d32 = disp - (byte_buffer_get_cursor_offset(&m->mc) + opt.off32 + sizeof(int32_t));
		if (opt.disp8_out) *opt.disp8_out = d8;
		if (opt.disp32_out) *opt.disp32_out = d32;
		if (IS_INT8_SIZED(d8)) {
			if (opt.sz_out) *opt.sz_out = sizeof(int8_t);
			return d8;
		} else {
			ASSERT(IS_INT32_SIZED(d32));
			if (opt.sz_out) *opt.sz_out = sizeof(int32_t);
			return d32;
		}
	}
	Asm_label *lbl = &m->labels.elems[disp];
	switch (lbl->type) {
	case ASM_LBL_T_CONST: FAILWITH("TODO: ASM_LBL_T_CONST"); break;
	case ASM_LBL_T_BLOCK:
	case ASM_LBL_T_DATA:
	case ASM_LBL_T_FUNC:
		if (lbl->is_resolved) {
			disp = lbl->offset + inst.imm;
			int64_t d8  = disp - (byte_buffer_get_cursor_offset(&m->mc) + opt.off8  + sizeof(int8_t));
			int64_t d32 = disp - (byte_buffer_get_cursor_offset(&m->mc) + opt.off32 + sizeof(int32_t));
			if (opt.disp8_out) *opt.disp8_out = d8;
			if (opt.disp32_out) *opt.disp32_out = d32;
			if (IS_INT8_SIZED(d8)) {
				if (opt.sz_out) *opt.sz_out = sizeof(int8_t);
				return d8;
			} else {
				ASSERT(IS_INT32_SIZED(d32));
				if (opt.sz_out) *opt.sz_out = sizeof(int32_t);
				return d32;
			}
		}
		da_append(&m->labels.elems[disp].patches, (struct asm_lbl_back_patch){
				.offset = inst.imm - (byte_buffer_get_cursor_offset(&m->mc) + opt.off32 + sizeof(int32_t)),
				.loc    = byte_buffer_get_cursor_offset(&m->mc) + opt.off32,
				.size   = sizeof(int32_t),
			});
		if (opt.sz_out)     *opt.sz_out = sizeof(int32_t);
		if (opt.disp8_out)  *opt.disp8_out = 0;
		if (opt.disp32_out) *opt.disp32_out = 0;
		return 0;
	default:
#ifndef NO_STD_HEADERS
		FAILWITH("Unreachable label: %s", lbl->name);
#else
		FAILWITH(DEBUG_MSG("Unreachable label"));
#endif
	}
}

static void
emit_indirect_addressing(Asm_module *m, struct opcode_bytes opcode, Asm_inst inst, struct _dispatch_options opt)
{
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	if (IS_RIP_REL(inst)) {
		/* RIP relative */
		EMIT_OPT_REX(opt.rex_w, inst.r0, 0, 0);
		byte_buffer_insert_bytes(&m->mc, opcode.code, opcode.len);
		byte_buffer_put_byte(&m->mc, MODRM(0, inst.r0, RBP));
		int64_t disp;
		calc_rip_rel_disp(m, inst, .disp32_out = &disp);
		byte_buffer_insert_bytes(&m->mc, &disp, sizeof(int32_t));
		return;
	}
	int mod;
	size_t sz;
	int64_t disp = get_disp(m, inst, &sz);
	switch (sz) {
	case sizeof(int8_t):
		if (disp == 0) {
			mod = 0;
			sz  = 0;
		} else {
			mod = 1;
		}
		break;
	case sizeof(int32_t):
		mod = 2;
		break;
	default: FAILWITH("Unreachable");
	}
	if (inst.t_x) {
		/* no index register used */
		EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);
		byte_buffer_insert_bytes(&m->mc, opcode.code, opcode.len);
		byte_buffer_put_byte(&m->mc, MODRM(mod, inst.r0, inst.r1));
		/* special case where base is RSP or R12; SIB byte must be used */
		if (inst.r1 == RSP || inst.r1 == R12)
			byte_buffer_put_byte(&m->mc, SIB(0, RSP, inst.r1));
		byte_buffer_insert_bytes(&m->mc, &disp, sz);
		return;
	}
	/* index register used */
	ASSERT(inst.rx != RSP);
	ASSERT(inst.r1 != RBP);
	EMIT_OPT_REX(opt.rex_w, inst.r0, inst.rx, inst.r1);
	byte_buffer_insert_bytes(&m->mc, opcode.code, opcode.len);
	byte_buffer_put_byte(&m->mc, MODRM(mod, inst.r0, RSP)); /* use SIB byte */
	byte_buffer_put_byte(&m->mc, SIB(inst.sc, inst.rx, inst.r1));
	byte_buffer_insert_bytes(&m->mc, &disp, sz);
}

static void
emit_mov_zwdq_mr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	/* 8B /r | MOV r32, r/m32 | Move r/m32 to r32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x8b), inst, opt);
}

/** NOTE: For all byte-sized operand instructions.
 ** The `Intel 64 and IA-32 Architectures Software Developer’s Manual` states:
 ** "With a REX prefix in 64-bit mode, attempts to access AH, BH, CH, or DH
 ** will instead access SPL, DIL, BPL, or SIL, respectively."
 ** I don't ever care about accessing the high byte registers.
 ** (If I change my mind about this, I can add a workaround later.)
 ** If RSP, RDI, RBP, or RSI are addressed in a ZB instruction, we need to add
 ** an REX prefix to prevent AH, BH, CH, or DH from being addressed instead.
 **/

static void
emit_mov_zb_mr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	/* 8A /r | MOV r81, r/m81 | Move r8 to r/m8.
	 * NOTE:
	 * 1. With a REX prefix in 64-bit mode, attempts to access AH, BH, CH, or DH
	 * will instead access SPL, DIL, BPL, or SIL, respectively. */
	switch ((int)inst.r0) {
	case RSP: case RBP: case RDI: case RSI:
		opt.force_rex = true;
	default:
	}
	emit_indirect_addressing(m, OPCODE_BYTES(0x8a), inst, opt);
}

static void
emit_mov_zb_rm(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	/* 88 /r | MOV r/m81, r81 | Move r8 to r/m8.
	 * NOTE:
	 * 1. With a REX prefix in 64-bit mode, attempts to access AH, BH, CH, or DH
	 * will instead access SPL, DIL, BPL, or SIL, respectively. */
	switch ((int)inst.r0) {
	case RSP: case RBP: case RDI: case RSI:
		opt.force_rex = true;
	default:
	}
	emit_indirect_addressing(m, OPCODE_BYTES(0x88), inst, opt);
}

static void
emit_mov_zwdq_rm(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	/* 89 /r | MOV r/m32, r32 | Move r32 to r/m32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x89), inst, opt);
}

static void
emit_mov_zw_im(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	/* C7 /0 id | MOV r/m32, imm32 | Move imm32 to r/m32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0xc7), inst, opt);
	byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int16_t));
}

static void
emit_mov_zdq_im(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	/* C7 /0 id | MOV r/m32, imm32 | Move imm32 to r/m32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0xc7), inst, opt);
	byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int32_t));
}

static void
emit_mov_zwdq_ir(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	size_t sz = sizeof(int32_t);
	int64_t imm = inst.t_lbl ? asm_get_label_offset(m, inst.disp) + inst.imm : inst.imm;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		ASSERT(imm <= INT16_MAX && imm >= INT16_MIN);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
		sz = sizeof(int16_t);
	}
	if (imm <= INT32_MAX && imm >= INT32_MIN) {
		int opcode = 0xc7;
		/* C7 /0 id | MOV r/m32, imm32 | Move imm32 to r/m32 */
		EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
		/* op code */
		byte_buffer_put_byte(&m->mc, opcode);
		/* mod r/m */
		byte_buffer_put_byte(&m->mc, MODRM(3, 0, inst.r1));
		/* imm32 */
		byte_buffer_insert_bytes(&m->mc, &imm, sz);
		return;
	}
	/* REX.W + B8+ rd io | MOV r64, imm64 | Move imm64 to r64 */
	int opcode = 0xb8;
	ASSERT(opt.rex_w);
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode + (inst.r1 & 7));		/* op code */
	byte_buffer_insert_bytes(&m->mc, &imm, sizeof(int64_t));	/* imm64 */
}

static void
emit_mov_zb_ir(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	int64_t imm = inst.t_lbl ? asm_get_label_offset(m, inst.disp) + inst.imm : inst.imm;
	ASSERT(imm <= INT8_MAX && imm >= INT8_MIN);
	/* B0+ rb ib | MOV r8, imm8 | Move imm8 to r8. */
	switch ((int)inst.r1) {
	case RSP: case RBP: case RDI: case RSI:
		opt.force_rex = true;
	default:
	}
	EMIT_OPT_REX(0, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0xb0 + (inst.r1 & 7));		/* op code */
	byte_buffer_insert_bytes(&m->mc, &imm, sizeof(int8_t));
}


static void
emit_mov_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	int opcode = 0x89;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	/* 89 /r | MOV r/m32, r32 | Move r32 to r/m32. */
	EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode);                        	/* op code */
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1));      /* mod r/m */
}

static void
emit_mov_zb_rr(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOV);
	/* 88 /r | MOV r/m81, r81 | Move r8 to r/m8. */
	/* NOTES:
	   1. With a REX prefix in 64-bit mode, attempts to access AH, BH, CH, or DH
	   will instead access SPL, DIL, BPL, or SIL, respectively. */
	switch ((int)inst.r0) {
	case RSP: case RBP: case RDI: case RSI:
		opt.force_rex = true;
	default:
	}
	switch ((int)inst.r1) {
	case RSP: case RBP: case RDI: case RSI:
		opt.force_rex = true;
	default:
	}
	EMIT_OPT_REX(0, inst.r0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0x88);                        	/* op code */
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1));      /* mod r/m */
}

/* EMIT MOVSDQ */
static void
emit_movsdq_mr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOVSDQ);
	/* REX.W + 63 /r | MOVSXD r64, r/m32 | Move doubleword to quadword with sign-extension. */
	ASSERT(opt.rex_w);
	emit_indirect_addressing(m, OPCODE_BYTES(0x63), inst, opt);
}

/* EMIT MOVSBD */
static void
emit_movsbd_mr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOVSBD);
	/* 0F BE /r | MOVSX r32, r/m8 | Move byte to doubleword with sign-extension. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x0f, 0xbe), inst, opt);
}

/* EMIT MOVZWD */
static void
emit_movzwd_mr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_MOVZWD);
	/* 0F B7 /r | MOVZX r32, r/m16 | Move word to doubleword, zero-extension. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x0f, 0xb7), inst, opt);
}

/* EMIT PUSH */
static void
emit_push_zwdq_i(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_PUSH);
	size_t sz = sizeof(int32_t);
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		ASSERT(inst.imm <= INT16_MAX && inst.imm >= INT16_MIN);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
		sz = sizeof(int16_t);
	}
	EMIT_OPT_REX(opt.rex_w, 0, 0, 0);
	if (inst.imm <= INT8_MAX && inst.imm >= INT8_MIN) {
		/* 6A ib | PUSH imm8 */
		byte_buffer_put_byte(&m->mc, 0x6a);
		byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int8_t));
		return;
	}
	/* 68 id | PUSH imm32 */
	ASSERT(inst.imm <= INT32_MAX && inst.imm >= INT32_MIN);
	byte_buffer_put_byte(&m->mc, 0x68);
	byte_buffer_insert_bytes(&m->mc, &inst.imm, sz);
}

static void
emit_push_zwdq_r(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_PUSH);
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	/* 50+rd | PUSH r32 */
	byte_buffer_put_byte(&m->mc, 0x50 + (inst.r1 & 7));
}

/* EMIT POP */
static void
emit_pop_zwdq_r(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_POP);
	/* 58+rd | PUSH r32 */
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	/* 50+rd | PUSH r32 */
	byte_buffer_put_byte(&m->mc, 0x58 + (inst.r1 & 7));
}

/* EMIT NEG */
static void
emit_neg_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_NEG);
	/* F7 /3 | NEG r/m32 | Two's complement negate r/m32. */
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0xf7);
	byte_buffer_put_byte(&m->mc, MODRM(3, 3, inst.r1));
}

static void
emit_neg_zb_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_NEG);
	/* F6 /3 | NEG r/m81 | Two's complement negate r/m8. */
	/* NOTES:
	   1. With a REX prefix in 64-bit mode, attempts to access AH, BH, CH, or DH
	   will instead access SPL, DIL, BPL, or SIL, respectively. */
	switch ((int)inst.r1) {
	case RSP: case RBP: case RDI: case RSI:
		opt.force_rex = true;
	default:
	}
	EMIT_OPT_REX(0, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0xf6);
	byte_buffer_put_byte(&m->mc, MODRM(3, 3, inst.r1));
}

/* EMIT ADD */
static void
emit_add_zwdq_ir(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_ADD);
	int opcode = 0x81;
	size_t imm_size = sizeof(int32_t);
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
		imm_size = sizeof(int16_t);
	}
	if (inst.imm <= INT8_MAX && inst.imm >= INT8_MIN) {
		/* 83 /0 ib | ADD r/m32, imm8 | Add sign-extended imm8 to r/m32. */
		opcode = 0x83;
		imm_size = sizeof(int8_t);
	} else if (inst.r1 == RAX) {
		/* 05 id | ADD EAX, imm32 | Add imm32 to EAX. */
		EMIT_OPT_REX(opt.rex_w, 0, 0, 0);
		byte_buffer_put_byte(&m->mc, 0x05);
		byte_buffer_insert_bytes(&m->mc, &inst.imm, imm_size);
		return;
	}
	/* 81 /0 id | ADD r/m32, imm32 | Add imm32 to r/m32. */
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode);
	byte_buffer_put_byte(&m->mc, MODRM(3, 0, inst.r1));
	byte_buffer_insert_bytes(&m->mc, &inst.imm, imm_size);
}

static void
emit_add_zb_ir(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_ADD);
	ASSERT(IS_INT8_SIZED(inst.imm));
	if (inst.r1 == RAX) {
		/* 04 ib | ADD AL, imm8 | Add imm8 to AL. */
		byte_buffer_put_byte(&m->mc, 0x04);
		byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int8_t));
	} else {
		/* 80 /0 ib | ADD r/m81, imm8 | Add imm8 to r/m8. */
		switch ((int)inst.r1) {
		case RSP: case RBP: case RDI: case RSI:
			opt.force_rex = true;
		default:
		}
		EMIT_OPT_REX(0, 0, 0, inst.r1);
		byte_buffer_put_byte(&m->mc, 0x80);
		byte_buffer_put_byte(&m->mc, MODRM(3, 0, inst.r1));
		byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int8_t));
	}
}

static void
emit_add_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_ADD);
	/* 01 /r | ADD r/m32, r32 | Add r32 to r/m32. */
	int opcode = 0x01;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	/* optional rex prefix */
	EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);
	/* op code */
	byte_buffer_put_byte(&m->mc, opcode);
	/* mod r/m */
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1));
}

static void
emit_add_zwdq_mr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_ADD);
	/* 03 /r | ADD r32, r/m32 | Add r/m32 to r32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x03), inst, opt);
}

static void
emit_add_zwdq_rm(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_ADD);
	/* 01 /r | ADD r/m32, r32 | Add r32 to r/m32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x01), inst, opt);
}

/* EMIT SUB */
static void
emit_sub_zwdq_ir(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_SUB);
	int opcode = 0x81;
	size_t imm_size = sizeof(int32_t);
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
		imm_size = sizeof(int16_t);
	}
	if (inst.imm <= INT8_MAX && inst.imm >= INT8_MIN) {
		/* 83 /5 ib | SUB r/m32, imm8 | Subtract sign-extended imm8 to r/m32. */
		opcode = 0x83;
		imm_size = sizeof(int8_t);
	} else if (inst.r1 == RAX) {
		/* 2D id | SUB EAX, imm32 | Subtract imm32 to EAX. */
		EMIT_OPT_REX(opt.rex_w, 0, 0, 0);
		byte_buffer_put_byte(&m->mc, 0x2D);
		byte_buffer_insert_bytes(&m->mc, &inst.imm, imm_size);
		return;
	}
	/* 81 /5 id | SUB r/m32, imm32 | Subtract imm32 to r/m32. */
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode);
	byte_buffer_put_byte(&m->mc, MODRM(3, 5, inst.r1));
	byte_buffer_insert_bytes(&m->mc, &inst.imm, imm_size);
}

static void
emit_sub_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_SUB);
	/* 29 /r | SUB r/m32, r32 | Subtract r32 from r/m32. */
	int opcode = 0x29;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);	   /* optional rex prefix */
	byte_buffer_put_byte(&m->mc, opcode);					   /* op code */
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1)); /* mod r/m */
}

static void
emit_sub_zwdq_mr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_SUB);
	/* 2B /r | SUB r32, r/m32 | Subtract r/m32 from r32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x2b), inst, opt);
}

static void
emit_sub_zwdq_rm(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_SUB);
	/* 29 /r | SUB r/m32, r32 | Subtract r32 from r/m32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x29), inst, opt);
}

/* EMIT IDIV */
static void
emit_idiv_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_IDIV);
	/* F7 /7 | IDIV r/m32 | Signed divide EDX:EAX by r/m32,
	                        with result stored in EAX := Quotient, EDX := Remainder. */
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);   	   /* optional rex prefix */
	byte_buffer_put_byte(&m->mc, 0xf7);					   /* op code */
	byte_buffer_put_byte(&m->mc, MODRM(3, 7, inst.r1));       /* mod r/m */
}

/* EMIT DIV */
static void
emit_div_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_DIV);
	/* F7 /6 | DIV r/m32 | Unsigned divide EDX:EAX by r/m32,
	                       with result stored in EAX := Quotient, EDX := Remainder. */
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);   	   /* optional rex prefix */
	byte_buffer_put_byte(&m->mc, 0xf7);					   /* op code */
	byte_buffer_put_byte(&m->mc, MODRM(3, 6, inst.r1));       /* mod r/m */
}

/* EMIT SAR */
static void
emit_sar_zdq_ir(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_SAR);
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	if (inst.imm == 1) {
		/* D1 /7 | SHL r/m82, 1 | Multiply r/m32 by 2, once. */
		byte_buffer_put_byte(&m->mc, 0xd1);
		byte_buffer_put_byte(&m->mc, MODRM(3, 7, inst.r1));
	} else {
		/* C1 /7 ib | SHL r/m32, imm8 | Multiply r/m32 by 2, imm8 times. */
		byte_buffer_put_byte(&m->mc, 0xc1);
		byte_buffer_put_byte(&m->mc, MODRM(3, 7, inst.r1));
		byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int8_t));
	}
}

/* EMIT SHL */
static void
emit_shl_zdq_ir(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_SHL);
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	if (inst.imm == 1) {
		/* D1 /4 | SHL r/m82, 1 | Multiply r/m32 by 2, once. */
		byte_buffer_put_byte(&m->mc, 0xd1);
		byte_buffer_put_byte(&m->mc, MODRM(3, 4, inst.r1));
	} else {
		/* C1 /4 ib | SHL r/m32, imm8 | Multiply r/m32 by 2, imm8 times. */
		byte_buffer_put_byte(&m->mc, 0xc1);
		byte_buffer_put_byte(&m->mc, MODRM(3, 4, inst.r1));
		byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int8_t));
	}
}

static void
emit_shl_zdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	/* D3 /4 | SHL r/m32, CL | Multiply r/m32 by 2, CL times. */
	ASSERT(inst.op == OP_SHL);
	ASSERT(inst.r0 == RCX);
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0xd3);
	byte_buffer_put_byte(&m->mc, MODRM(3, 4, inst.r1));
}

/* EMIT CMP */
static void
emit_cmp_zwdq_ir(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_CMP);
	int opcode = 0x81;
	size_t imm_size = sizeof(int32_t);
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
		imm_size = sizeof(int16_t);
	}
	if (inst.imm <= INT8_MAX && inst.imm >= INT8_MIN) {
		/* 83 /7 ib | CMP r/m32, imm8 | Compare sign-extended imm8 to r/m32. */
		opcode = 0x83;
		imm_size = sizeof(int8_t);
	} else if (inst.r1 == RAX) {
		/* 3D id | CMP EAX, imm32 | Compare imm32 to EAX. */
		EMIT_OPT_REX(opt.rex_w, 0, 0, 0);
		byte_buffer_put_byte(&m->mc, 0x3D);
		byte_buffer_insert_bytes(&m->mc, &inst.imm, imm_size);
		return;
	}
	/* 81 /7 id | CMP r/m32, imm32 | Compare imm32 to r/m32. */
	EMIT_OPT_REX(opt.rex_w, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode);
	byte_buffer_put_byte(&m->mc, MODRM(3, 7, inst.r1));
	byte_buffer_insert_bytes(&m->mc, &inst.imm, imm_size);
}

static void
emit_cmp_zb_ir(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_CMP);
	/* 80 /7 ib | CMP r/m81, imm8 | Compare imm8 with r/m8. */
	/* NOTES:
	   1. With a REX prefix in 64-bit mode, attempts to access AH, BH, CH, or DH
	   will instead access SPL, DIL, BPL, or SIL, respectively. */
	ASSERT(IS_INT8_SIZED(inst.imm));
	switch ((int)inst.r1) {
	case RSP: case RBP: case RDI: case RSI:
		opt.force_rex = true;
	default:
	}
	EMIT_OPT_REX(0, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0x80);
	byte_buffer_put_byte(&m->mc, MODRM(3, 7, inst.r1));
	byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int8_t));
}

static void
emit_cmp_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_CMP);
	/* 39 /r | CMP r/m32, r32 | Compare r32 from r/m32. */
	int opcode = 0x39;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);	   /* optional rex prefix */
	byte_buffer_put_byte(&m->mc, opcode); 	                   /* op code */
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1)); /* mod r/m */
}

static void
emit_cmp_zb_im(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_CMP);
	/* 80 /7 ib | CMP r/m81, imm8 | Compare imm8 with r/m8. */
	/* NOTES:
	   1. With a REX prefix in 64-bit mode, attempts to access AH, BH, CH, or DH
	   will instead access SPL, DIL, BPL, or SIL, respectively. */
	ASSERT(IS_INT8_SIZED(inst.imm));
	inst.r0 = 7;
	emit_indirect_addressing(m, OPCODE_BYTES(0x80), inst, opt);
	byte_buffer_insert_bytes(&m->mc, &inst.imm, sizeof(int8_t));
}

/* EMIT AND */
static void
emit_and_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_AND);
	/* 21 /r | AND r/m32, r32 | r/m32 AND r32. */
	int opcode = 0x21;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode);
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1));
}

/* EMIT OR */
static void
emit_or_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_OR);
	/* 09 /r | OR r/m32, r32 | r/m32 OR r32. */
	int opcode = 0x09;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode);
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1));
}

/* EMIT XOR */
static void
emit_xor_zwdq_rr(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_XOR);
	/* 31 /r | XOR r/m32, r32 | r/m32 XOR r32. */
	int opcode = 0x31;
	if (opt.opor) {
		ASSERT(!opt.rex_w);
		byte_buffer_put_byte(&m->mc, OPOR_BYTE);
	}
	EMIT_OPT_REX(opt.rex_w, inst.r0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, opcode);
	byte_buffer_put_byte(&m->mc, MODRM(3, inst.r0, inst.r1));
}

static void
emit_lea_zwdq(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_LEA);
	/* 8D /r | LEA r32,m | Store effective address for m in register r32. */
	emit_indirect_addressing(m, OPCODE_BYTES(0x8d), inst, opt);
}

/* EMIT Jcc */
static void
emit_jcc(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op < 0x10);
	size_t sz;
	int64_t disp = calc_rip_rel_disp(m, inst, .off8 = 1, .off32 = 2, .sz_out = &sz);
	switch (sz) {
	case sizeof(int8_t): {
		byte_buffer_put_byte(&m->mc, 0x70|(inst.op & 0x0f));
	} break;
	case sizeof(int32_t): {
		byte_buffer_put_byte(&m->mc, 0x0f);
		byte_buffer_put_byte(&m->mc, 0x80|(inst.op & 0x0f));
	} break;
	default: FAILWITH("Unreachable");
	}
	byte_buffer_insert_bytes(&m->mc, &disp, sz);
}

/* EMIT SETcc */
static void
emit_setcc_mr(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op < 0x20 && inst.op > 0x10);
	/* 0F 90 | SETcc r/m8 */
	emit_indirect_addressing(m, OPCODE_BYTES(0x0f, 0x90|(inst.op & 0x0f)), inst, opt);
}


/* EMIT JMP */
static void
emit_jmp(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_JMP);
	/* E9 cd | JMP rel32 | Jump near, relative, RIP = RIP + 32-bit displacement sign extended to 64-bits. */
	size_t sz;
	int64_t disp = calc_rip_rel_disp(m, inst, .off8 = 1, .off32 = 1, .sz_out = &sz);
	switch (sz) {
	case sizeof(int8_t):
		/* jmp short */
		byte_buffer_put_byte(&m->mc, 0xeb);
		break;
	case sizeof(int32_t):
		/* jmp near */
		byte_buffer_put_byte(&m->mc, 0xe9);
		break;
	default: FAILWITH("Unreachable");
	}
	byte_buffer_insert_bytes(&m->mc, &disp, sz);
}

static void
emit_jmp_indirect(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_JMP);
	/* FF /4 | JMP r/m64 | Jump near, absolute indirect, RIP = 64-Bit offset from register or memory. */
	if (opt.is_mem) {
		inst.r0 = 4;
		emit_indirect_addressing(m, OPCODE_BYTES(0xff), inst, opt);
		return;
	}
	EMIT_OPT_REX(0, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0xff);
	byte_buffer_put_byte(&m->mc, MODRM(3, 4, inst.r1));
}

/* Emit CALL */
static void
emit_call(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_CALL);
	/* E8 cd | CALL rel32 |
	   Call near, relative, displacement relative to next
	   instruction. 32-bit displacement sign extended to
	   64-bits in 64-bit mode. */
	byte_buffer_put_byte(&m->mc, 0xe8);
	int64_t disp;
	calc_rip_rel_disp(m, inst, .disp32_out = &disp);
	byte_buffer_insert_bytes(&m->mc, &disp, sizeof(int32_t));
}

static void
emit_call_indirect(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_CALL);
	/* FF /2 | CALL r/m64 | Call near, absolute indirect, address given in r/m64. */
	if (opt.is_mem) {
		inst.r0 = 2;
		emit_indirect_addressing(m, OPCODE_BYTES(0xff), inst, opt);
		return;
	}
	EMIT_OPT_REX(0, 0, 0, inst.r1);
	byte_buffer_put_byte(&m->mc, 0xff);
	byte_buffer_put_byte(&m->mc, MODRM(3, 2, inst.r1));
}

static void
emit_ret(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_RET);
	byte_buffer_put_byte(&m->mc, 0xc3);
}

static void
emit_syscall(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	ASSERT(inst.op == OP_SYSCALL);
	byte_buffer_insert_bytes(&m->mc, (ubyte[]){0x0f, 0x05}, 2);
}

static void
emit_cdq(Asm_module *m, Asm_inst inst, UNUSED struct _dispatch_options opt)
{
	/* 99 | CDQ | EDX:EAX := sign-extend of EAX. */
	switch ((int)inst.op) {
	case OP_CWD: byte_buffer_put_byte(&m->mc, OPOR_BYTE); break;
	case OP_CDQ: /* do nothing */ break;
	case OP_CQO: byte_buffer_put_byte(&m->mc, REX(1, 0, 0, 0)); break;
	default: FAILWITH("Unreachable"); break;
	}
	byte_buffer_put_byte(&m->mc, 0x99);
}

static void
emit_int(Asm_module *m, Asm_inst inst, struct _dispatch_options opt)
{
	int64_t x = inst.imm;
	if (inst.t_lbl) {
		if (m->labels.elems[inst.disp].is_resolved) {
			x += m->labels.elems[inst.disp].offset;
		} else {
			FAILWITH("TODO: back-patch");
		}
	}
	byte_buffer_insert_bytes(&m->mc, &x, opt.sz);
}

static const struct _dispatch dispatch_table[OP_CODE_COUNT][0x20] = {
	[OP_JO]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JNO] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JB]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JAE] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JE]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JNE] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JBE] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JA]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JS]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JNS] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JP]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JNP] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JL]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JGE] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JLE] = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_JG]	 = {[SZDSC(0, IR)] = {emit_jcc}},
	[OP_SETO]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETNO] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETB]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETAE] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETE]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETNE] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETBE] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETA]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETS]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETNS] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETP]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETNP] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETL]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETGE] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETLE] = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_SETG]  = {[SZDSC(ZB, MR)] = {emit_setcc_mr}},
	[OP_JMP] = {
		[SZDSC(0, RR)] = {emit_jmp_indirect},
		[SZDSC(0, MR)] = {emit_jmp_indirect, {.is_mem = true}},
		[SZDSC(0, IR)] = {emit_jmp},
	},
	[OP_CALL] = {
		[SZDSC(0, RR)] = {emit_call_indirect},
		[SZDSC(0, MR)] = {emit_call_indirect, {.is_mem = true}},
		[SZDSC(0, IR)] = {emit_call},
	},
	[OP_RET] = {
		[0] = {emit_ret}
	},
	[OP_PUSH] = {
		[SZDSC(ZW, IR)] = {emit_push_zwdq_i, {.opor = true}},
		[SZDSC(ZD, IR)] = {emit_push_zwdq_i},
		[SZDSC(ZQ, IR)] = {emit_push_zwdq_i, {.rex_w = true}},
		[SZDSC(ZW, RR)] = {emit_push_zwdq_r, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_push_zwdq_r},
		[SZDSC(ZQ, RR)] = {emit_push_zwdq_r, {.rex_w = true}},
	},
	[OP_POP] = {
		[SZDSC(ZW, RR)] = {emit_pop_zwdq_r, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_pop_zwdq_r},
		[SZDSC(ZQ, RR)] = {emit_pop_zwdq_r, {.rex_w = true}},
	},
	[OP_MOV] = {
		[SZDSC(ZW, IM)] = {emit_mov_zw_im,   {.opor = true}},
		[SZDSC(ZD, IM)] = {emit_mov_zdq_im},
		[SZDSC(ZQ, IM)] = {emit_mov_zdq_im,  {.rex_w = true}},
		[SZDSC(ZB, RM)] = {emit_mov_zb_rm},
		[SZDSC(ZW, RM)] = {emit_mov_zwdq_rm, {.opor = true}},
		[SZDSC(ZD, RM)] = {emit_mov_zwdq_rm},
		[SZDSC(ZQ, RM)] = {emit_mov_zwdq_rm, {.rex_w = true}},
		[SZDSC(ZB, MR)] = {emit_mov_zb_mr},
		[SZDSC(ZW, MR)] = {emit_mov_zwdq_mr, {.opor = true}},
		[SZDSC(ZD, MR)] = {emit_mov_zwdq_mr},
		[SZDSC(ZQ, MR)] = {emit_mov_zwdq_mr, {.rex_w = true}},
		[SZDSC(ZB, IR)] = {emit_mov_zb_ir},
		[SZDSC(ZW, IR)] = {emit_mov_zwdq_ir, {.opor = true}},
		[SZDSC(ZD, IR)] = {emit_mov_zwdq_ir},
		[SZDSC(ZQ, IR)] = {emit_mov_zwdq_ir, {.rex_w = true}},
		[SZDSC(ZB, RR)] = {emit_mov_zb_rr},
		[SZDSC(ZW, RR)] = {emit_mov_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_mov_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_mov_zwdq_rr, {.rex_w = true}},
	},
	[OP_MOVSBD] = {
		[SZDSC(ZB, MR)] = {emit_movsbd_mr},
	},
	[OP_MOVSDQ] = {
		[SZDSC(ZB, MR)] = {emit_movsdq_mr, {.rex_w = true}},
	},
	[OP_MOVZWD] = {
		[SZDSC(ZB, MR)] = {emit_movzwd_mr},
	},
	[OP_NEG] = {
		[SZDSC(ZB, RR)] = {emit_neg_zb_rr},
		[SZDSC(ZW, RR)] = {emit_neg_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_neg_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_neg_zwdq_rr, {.rex_w = true}},
	},
	[OP_ADD] = {
		[SZDSC(ZW, RM)] = {emit_add_zwdq_rm, {.opor = true}},
		[SZDSC(ZD, RM)]	= {emit_add_zwdq_rm},
		[SZDSC(ZQ, RM)]	= {emit_add_zwdq_rm, {.rex_w = true}},
		[SZDSC(ZW, MR)] = {emit_add_zwdq_mr, {.opor = true}},
		[SZDSC(ZD, MR)]	= {emit_add_zwdq_mr},
		[SZDSC(ZQ, MR)]	= {emit_add_zwdq_mr, {.rex_w = true}},
		[SZDSC(ZB, IR)] = {emit_add_zb_ir},
		[SZDSC(ZW, IR)] = {emit_add_zwdq_ir, {.opor = true}},
		[SZDSC(ZD, IR)] = {emit_add_zwdq_ir},
		[SZDSC(ZQ, IR)] = {emit_add_zwdq_ir, {.rex_w = true}},
		[SZDSC(ZW, RR)] = {emit_add_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_add_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_add_zwdq_rr, {.rex_w = true}},
	},
	[OP_SUB] = {
		[SZDSC(ZW, RM)] = {emit_sub_zwdq_rm, {.opor = true}},
		[SZDSC(ZD, RM)]	= {emit_sub_zwdq_rm},
		[SZDSC(ZQ, RM)]	= {emit_sub_zwdq_rm, {.rex_w = true}},
		[SZDSC(ZW, MR)] = {emit_sub_zwdq_mr, {.opor = true}},
		[SZDSC(ZD, MR)]	= {emit_sub_zwdq_mr},
		[SZDSC(ZQ, MR)]	= {emit_sub_zwdq_mr, {.rex_w = true}},
		[SZDSC(ZW, IR)] = {emit_sub_zwdq_ir, {.opor = true}},
		[SZDSC(ZD, IR)] = {emit_sub_zwdq_ir},
		[SZDSC(ZQ, IR)] = {emit_sub_zwdq_ir, {.rex_w = true}},
		[SZDSC(ZW, RR)] = {emit_sub_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_sub_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_sub_zwdq_rr, {.rex_w = true}},
	},
	[OP_IDIV] = {
		[SZDSC(ZW, RR)] = {emit_idiv_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_idiv_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_idiv_zwdq_rr, {.rex_w = true}},
	},
	[OP_DIV] = {
		[SZDSC(ZW, RR)] = {emit_div_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_div_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_div_zwdq_rr, {.rex_w = true}},
	},
	[OP_SAR] = {
		[SZDSC(ZD, IR)] = {emit_sar_zdq_ir},
		[SZDSC(ZQ, IR)] = {emit_sar_zdq_ir, {.rex_w = true}},
	},
	[OP_SHL] = {
		[SZDSC(ZD, RR)] = {emit_shl_zdq_rr},
		[SZDSC(ZQ, RR)] = {emit_shl_zdq_rr, {.rex_w = true}},
		[SZDSC(ZD, IR)] = {emit_shl_zdq_ir},
		[SZDSC(ZQ, IR)] = {emit_shl_zdq_ir, {.rex_w = true}},
	},
	[OP_CMP] = {
		[SZDSC(ZB, IR)] = {emit_cmp_zb_ir},
		[SZDSC(ZW, IR)] = {emit_cmp_zwdq_ir, {.opor = true}},
		[SZDSC(ZD, IR)] = {emit_cmp_zwdq_ir},
		[SZDSC(ZQ, IR)] = {emit_cmp_zwdq_ir, {.rex_w = true}},
		[SZDSC(ZW, RR)] = {emit_cmp_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_cmp_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_cmp_zwdq_rr, {.rex_w = true}},
		[SZDSC(ZB, IM)] = {emit_cmp_zb_im},
	},
	[OP_AND] = {
		[SZDSC(ZW, RR)] = {emit_and_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_and_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_and_zwdq_rr, {.rex_w = true}},
	},
	[OP_OR] = {
		[SZDSC(ZW, RR)] = {emit_or_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_or_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_or_zwdq_rr, {.rex_w = true}},
	},
	[OP_XOR] = {
		[SZDSC(ZW, RR)] = {emit_xor_zwdq_rr, {.opor = true}},
		[SZDSC(ZD, RR)] = {emit_xor_zwdq_rr},
		[SZDSC(ZQ, RR)] = {emit_xor_zwdq_rr, {.rex_w = true}},
	},
	[OP_CWD] = {[0] = {emit_cdq}},
	[OP_CDQ] = {[0] = {emit_cdq}},
	[OP_CQO] = {[0] = {emit_cdq}},
	[OP_SYSCALL] = {
		[0] = {emit_syscall},
	},
	[OP_LEA] = {
		[SZDSC(ZW, MR)] = {emit_lea_zwdq, {.opor = true}},
		[SZDSC(ZD, MR)] = {emit_lea_zwdq},
		[SZDSC(ZQ, MR)] = {emit_lea_zwdq, {.rex_w = true}},
	},
	[DIR_INT] = {
		[SZDSC(ZB, 0)] = {emit_int, {.sz = sizeof(int8_t)}},
		[SZDSC(ZW, 0)] = {emit_int, {.sz = sizeof(int16_t)}},
		[SZDSC(ZD, 0)] = {emit_int, {.sz = sizeof(int32_t)}},
		[SZDSC(ZQ, 0)] = {emit_int, {.sz = sizeof(int64_t)}},
	},
};

static void
emit_alignment_bytes(Asm_module *m, uint64_t alignment)
{
	alignment = align_next(m->mc.len, alignment);
	while (m->mc.len < alignment)
		byte_buffer_put_byte(&m->mc, NOP_BYTE);
}

static size_t
emit_inst(Asm_module *m, size_t inst_idx)
{
	Asm_inst inst = asm_get_instruction_array(m)[inst_idx];
	switch ((int)inst.op) {
	case DIR_IGNORE: break;
	case DIR_LABEL: {
		asm_label_id lbl = inst.disp;
		if (m->labels.elems[lbl].is_resolved)
			FAILWITH("[ERROR] label resolved before it is defined");
		m->labels.elems[lbl].offset = m->mc.len;
		m->labels.elems[lbl].is_resolved = true;
	} break;
	case DIR_REP: {
		ASSERT(inst.imm >= 0);
		inst_idx++;
		for (int64_t rep = inst.imm; rep--; emit_inst(m, inst_idx));
	} break;
	case DIR_ALIGN: {
		emit_alignment_bytes(m, inst.imm);
	} break;
	case DIR_STRING: {
		byte_buffer_put_str(&m->mc, (const char *)inst.imm);
		byte_buffer_put_byte(&m->mc, 0);
	} break;
	default: {
		const struct _dispatch *d = &dispatch_table[inst.op][SZDSC(inst.sz, inst.dsc)];
		if (d->fn == NULL) {
#ifndef NO_STD_HEADERS
			FAILWITH("TODO: %s SZDSC(%s, %s)",
					 asm_op_code_to_str(inst.op),
					 asm_op_size_to_str(inst.sz),
					 asm_arg_desc_to_str(inst.dsc));
#else
			FAILWITH(DEBUG_MSG("TODO: SZDSC"));
#endif
		}
		d->fn(m, inst, d->opt);
	} break;
	}
	return inst_idx + 1;
}

void
asm_assemble(Asm_module *m)
{
	if (m->is_jit) {
		for (size_t i = 0; i < m->labels.len; ++i) {
			Asm_label *lbl = &m->labels.elems[i];
			if (!lbl->is_extern) continue;
			switch (lbl->type) {
			case ASM_LBL_T_BLOCK:
#ifndef NO_STD_HEADERS
				FAILWITH("[ERROR] External reference to block label `%s`.", lbl->name);
#else
				FAILWITH(DEBUG_MSG("External reference to block label"));
#endif
				break;
			case ASM_LBL_T_FUNC: {
				/* There's a good chance that the foreign function is more than
				 * 2Gb away from the jitted code, so we roll our own plt.
				 */
				emit_alignment_bytes(m, 16);
				int64_t offset = m->mc.len;
				/* movq $"lbl->offset", %rax */
				byte_buffer_put_byte(&m->mc, REX(1, 0, 0, 0));
				byte_buffer_put_byte(&m->mc, 0xb8+RAX);
				byte_buffer_insert_bytes(&m->mc, &lbl->offset, sizeof(int64_t));
				/* jmp *%rax */
				byte_buffer_put_byte(&m->mc, 0xff);
				byte_buffer_put_byte(&m->mc, MODRM(3, 4, RAX));
				lbl->offset = offset;
				lbl->is_resolved = true;
			} break;
			case ASM_LBL_T_DATA:  lbl->is_resolved = true; break;
			case ASM_LBL_T_CONST: FAILWITH("TODO ASM_LBL_T_CONST"); break;
			default:
#ifndef NO_STD_HEADERS
				FAILWITH("Unreachable");
#else
				FAILWITH(DEBUG_MSG("Unreachable"));
#endif
			}
		}
	}
	size_t count = asm_get_instruction_count(m);
	for (size_t i = 0; i < count; i = emit_inst(m, i));
	/* patch labels */
	for (size_t i = 0; i < m->labels.len; ++i) {
		Asm_label *lbl = &m->labels.elems[i];
		if (lbl->is_extern) continue; /* if assembled for elf, extern symbols get resolved at link-time */
		if (!lbl->is_resolved) {
#ifndef NO_STD_HEADERS
			FAILWITH("[ERROR] memas label `%s` is unresolved", lbl->name);
#else
			FAILWITH(DEBUG_MSG("unresolved label"));
#endif
		}
		for (size_t i = 0; i < lbl->patches.len; ++i) {
			struct asm_lbl_back_patch *p = &lbl->patches.elems[i];
			int64_t value = lbl->offset + p->offset;
			byte_buffer_write_bytes_at_offset(&m->mc, p->loc, &value, p->size);
		}
		da_free(&lbl->patches);
	}
}

void
asm_assemble_for_jit(Asm_module *m)
{
	m->is_jit = true;
	asm_assemble(m);
}

void
asm_assemble_for_object_file(Asm_module *m)
{
	m->is_jit = false;
	asm_assemble(m);
}

int64_t
asm_get_label_offset(Asm_module *m, asm_label_id lbl)
{
	ASSERT(lbl < m->labels.len);
	if (m->labels.elems[lbl].is_resolved)
		return m->labels.elems[lbl].offset;
	return 0;
}

void *
asm_get_label_address(Asm_module *m, asm_label_id lbl)
{
	ASSERT(lbl < m->labels.len);
	if (m->labels.elems[lbl].is_resolved)
		return (ubyte *)m->mc.data + m->labels.elems[lbl].offset;
	return NULL;
}

bool
asm_lookup_label_id(Asm_module *m, const char *name, asm_label_id *id_out)
{
	for (asm_label_id i = 0; i < m->labels.len; ++i) {
		if (strcmp(name, m->labels.elems[i].name) == 0) {
			*id_out = i;
			return true;
		}
	}
	return false;
}

bool
asm_lookup_label_address(Asm_module *m, const char *name, void **addr_out)
{
	asm_label_id id;
	if (asm_lookup_label_id(m, name, &id) && (*addr_out = asm_get_label_address(m, id)))
		return true;
	return false;
}

#ifndef NO_STD_HEADERS
void
asm_dump_bytes(Asm_module *m)
{
	printf("{");
	for (size_t i = 0; i < m->mc.len; ++i) {
		printf("0x%02x, ", i[(ubyte *)m->mc.data]);
	}
	printf("}\n");
}
#endif

Asm_elf_builder
asm_make_elf_builder(enum elf_file_type e_type)
{
	Asm_elf_builder eb = {
		.hdr.e_ident[EI_MAG0]	 = ELF_MAGIC[EI_MAG0],
		.hdr.e_ident[EI_MAG1]	 = ELF_MAGIC[EI_MAG1],
		.hdr.e_ident[EI_MAG2]	 = ELF_MAGIC[EI_MAG2],
		.hdr.e_ident[EI_MAG3]	 = ELF_MAGIC[EI_MAG3],
		.hdr.e_ident[EI_CLASS]	 = ELFCLASS64,
		.hdr.e_ident[EI_DATA]	 = ELFDATA2LSB,
		.hdr.e_ident[EI_VERSION] = 1,
		.hdr.e_ident[EI_OSABI]	 = ELFOSABI_SYSV,
		.hdr.e_type				 = e_type,
		.hdr.e_machine			 = EM_X86_64,
		.hdr.e_version			 = 1,
		.hdr.e_ehsize            = sizeof(Elf64_header),
		.hdr.e_phentsize		 = sizeof(Elf64_program_header),
		.hdr.e_shentsize		 = sizeof(Elf64_section_header),
		.hdr.e_shnum             = ASM_SH_COUNT,
		.hdr.e_shstrndx          = ASM_SH_SHSTRTAB,
	};
	for (int i = 1; i < ASM_SH_COUNT; ++i) {
		byte_buffer_init_default(&eb.bufs[i]);
	}
	/* Initialize section headers */
	/* .shstrtab */
	byte_buffer_put_byte(&eb.bufs[ASM_SH_SHSTRTAB], 0);
	eb.shs[ASM_SH_SHSTRTAB].sh_name = asm_elf_add_to_shstrtab(&eb, ".shstrtab");
	eb.shs[ASM_SH_SHSTRTAB].sh_type = SHT_STRTAB;
	/* .strtab */
	byte_buffer_put_byte(&eb.bufs[ASM_SH_STRTAB], 0);
	eb.shs[ASM_SH_STRTAB].sh_name = asm_elf_add_to_shstrtab(&eb, ".strtab");
	eb.shs[ASM_SH_STRTAB].sh_type = SHT_STRTAB;
	/* .symtab */
	eb.shs[ASM_SH_SYMTAB].sh_name = asm_elf_add_to_shstrtab(&eb, ".symtab");
	eb.shs[ASM_SH_SYMTAB].sh_type = SHT_SYMTAB;
	eb.shs[ASM_SH_SYMTAB].sh_link = ASM_SH_STRTAB;
	eb.shs[ASM_SH_SYMTAB].sh_info = 1;
	eb.shs[ASM_SH_SYMTAB].sh_entsize = sizeof(Elf64_symbol);
	/* The first symbol in the table must be all zeroes */
	byte_buffer_insert_padding(&eb.bufs[ASM_SH_SYMTAB], 0, sizeof(Elf64_symbol));
	/* .interp */
	eb.shs[ASM_SH_INTERP].sh_name = asm_elf_add_to_shstrtab(&eb, ".interp");
	eb.shs[ASM_SH_INTERP].sh_type = SHT_PROGBITS;
	byte_buffer_put_str(&eb.bufs[ASM_SH_INTERP], "/lib64/ld-linux-x86-64.so.2");
	byte_buffer_put_byte(&eb.bufs[ASM_SH_INTERP], 0);
	/* .init_array */
	eb.shs[ASM_SH_INIT_ARRAY].sh_name = asm_elf_add_to_shstrtab(&eb, ".init_array");
	eb.shs[ASM_SH_INIT_ARRAY].sh_type = SHT_INIT_ARRAY;
	eb.shs[ASM_SH_INIT_ARRAY].sh_flags = SHF_WRITE|SHF_ALLOC;
	asm_elf_add_symbol(&eb, ".init_array", STT_SECTION, STB_LOCAL, ASM_SH_INIT_ARRAY, 0, 0);
	/* .fini_array */
	eb.shs[ASM_SH_FINI_ARRAY].sh_name = asm_elf_add_to_shstrtab(&eb, ".fini_array");
	eb.shs[ASM_SH_FINI_ARRAY].sh_type = SHT_FINI_ARRAY;
	eb.shs[ASM_SH_FINI_ARRAY].sh_flags = SHF_WRITE|SHF_ALLOC;
	asm_elf_add_symbol(&eb, ".fini_array", STT_SECTION, STB_LOCAL, ASM_SH_FINI_ARRAY, 0, 0);
	/* .text */
	eb.shs[ASM_SH_TEXT].sh_name = asm_elf_add_to_shstrtab(&eb, ".text");
	eb.shs[ASM_SH_TEXT].sh_type = SHT_PROGBITS;
	eb.shs[ASM_SH_TEXT].sh_flags = SHF_ALLOC|SHF_EXECINSTR;
	asm_elf_add_symbol(&eb, ".text", STT_SECTION, STB_LOCAL, ASM_SH_TEXT, 0, 0);
	/* .data */
	eb.shs[ASM_SH_DATA].sh_name = asm_elf_add_to_shstrtab(&eb, ".data");
	eb.shs[ASM_SH_DATA].sh_type = SHT_PROGBITS;
	eb.shs[ASM_SH_DATA].sh_flags = SHF_ALLOC|SHF_WRITE;
	asm_elf_add_symbol(&eb, ".data", STT_SECTION, STB_LOCAL, ASM_SH_DATA, 0, 0);
	/* .rodata */
	eb.shs[ASM_SH_RODATA].sh_name  = asm_elf_add_to_shstrtab(&eb, ".rodata");
	eb.shs[ASM_SH_RODATA].sh_type  = SHT_PROGBITS;
	eb.shs[ASM_SH_RODATA].sh_flags = SHF_ALLOC;
	asm_elf_add_symbol(&eb, ".rodata", STT_SECTION, STB_LOCAL, ASM_SH_RODATA, 0, 0);
	/* .bss */
	eb.shs[ASM_SH_BSS].sh_name  = asm_elf_add_to_shstrtab(&eb, ".bss");
	eb.shs[ASM_SH_BSS].sh_type  = SHT_NOBITS;
	eb.shs[ASM_SH_BSS].sh_flags = SHF_ALLOC|SHF_WRITE;
	asm_elf_add_symbol(&eb, ".bss", STT_SECTION, STB_LOCAL, ASM_SH_BSS, 0, 0);
	/* .rela.text */
	eb.shs[ASM_SH_RELA_TEXT].sh_name = asm_elf_add_to_shstrtab(&eb, ".rela.text");
	eb.shs[ASM_SH_RELA_TEXT].sh_type = SHT_RELA;
	eb.shs[ASM_SH_RELA_TEXT].sh_link = ASM_SH_SYMTAB;
	eb.shs[ASM_SH_RELA_TEXT].sh_info = ASM_SH_TEXT;
	eb.shs[ASM_SH_RELA_TEXT].sh_entsize = sizeof(Elf64_rela);
	asm_elf_add_symbol(&eb, ".rela.text", STT_SECTION, STB_LOCAL, ASM_SH_RELA_TEXT, 0, 0);
	/* .rela.init_array */
	eb.shs[ASM_SH_RELA_INIT_ARRAY].sh_name = asm_elf_add_to_shstrtab(&eb, ".rela.init_array");
	eb.shs[ASM_SH_RELA_INIT_ARRAY].sh_type = SHT_RELA;
	eb.shs[ASM_SH_RELA_INIT_ARRAY].sh_link = ASM_SH_SYMTAB;
	eb.shs[ASM_SH_RELA_INIT_ARRAY].sh_info = ASM_SH_INIT_ARRAY;
	eb.shs[ASM_SH_RELA_INIT_ARRAY].sh_entsize = sizeof(Elf64_rela);
	asm_elf_add_symbol(&eb, ".rela.init_array", STT_SECTION, STB_LOCAL, ASM_SH_RELA_INIT_ARRAY, 0, 0);
	return eb;
}

void
asm_elf_set_entry(Asm_elf_builder *eb, elf64_addr addr)
{
	eb->hdr.e_entry = addr;
}

elf64_word
asm_elf_add_to_strtab(Asm_elf_builder *eb, const char *name)
{
	elf64_word s = eb->bufs[ASM_SH_STRTAB].len;
	byte_buffer_put_str(&eb->bufs[ASM_SH_STRTAB], name);
	byte_buffer_put_byte(&eb->bufs[ASM_SH_STRTAB], 0);
	return s;
}

elf64_word
asm_elf_find_str(Asm_elf_builder *eb, const char *name)
{
	if (eb->bufs[ASM_SH_STRTAB].len < 2) return 0;
	const char *strtab = eb->bufs[ASM_SH_STRTAB].data;
	for (elf64_word i = 1; i < eb->bufs[ASM_SH_STRTAB].len; ++i) {
		if (strcmp(name, &strtab[i]) == 0) return i;
		/* skip to next string */
		for (; strtab[i]; ++i);
	}
	return 0;
}


elf64_word
asm_elf_add_to_shstrtab(Asm_elf_builder *eb, const char *name)
{
	elf64_word s = eb->bufs[ASM_SH_SHSTRTAB].len;
	byte_buffer_put_str(&eb->bufs[ASM_SH_SHSTRTAB], name);
	byte_buffer_put_byte(&eb->bufs[ASM_SH_SHSTRTAB], 0);
	return s;
}

elf64_word
asm_elf_add_symbol(Asm_elf_builder *eb,
				   const char *name,
				   enum elf_symbol_type type,
				   enum elf_symbol_binding binding,
				   elf64_half shndx,
				   elf64_addr value,
				   elf64_xword size)
{
	elf64_word sym_num;
	Elf64_symbol sym = {
		.st_name  = asm_elf_add_to_strtab(eb, name),
		.st_info  = (binding << 4) | type,
		.st_shndx = shndx,
		.st_value = value,
		.st_size  = size,
	};
	if (binding == STB_LOCAL
		&& eb->shs[ASM_SH_SYMTAB].sh_info * sizeof(Elf64_symbol) < eb->bufs[ASM_SH_SYMTAB].len) {
		/* All local symbols must come before other symbol types or linkers will complain.
		 * This code maintains this grouping by appending the first non-local symbol to the end
		 * of the symbol buffer. The new local symbol is then slotted into the old location.
		 * The field sh_info is the number of local symbols.
		 */
		sym_num = eb->shs[ASM_SH_SYMTAB].sh_info;
		Elf64_symbol *symview = eb->bufs[ASM_SH_SYMTAB].data;
		byte_buffer_insert_bytes(&eb->bufs[ASM_SH_SYMTAB], &symview[sym_num], sizeof(Elf64_symbol));
		symview[sym_num] = sym;
	} else {
		sym_num = eb->bufs[ASM_SH_SYMTAB].len / sizeof(Elf64_symbol);
		byte_buffer_insert_bytes(&eb->bufs[ASM_SH_SYMTAB], &sym, sizeof(Elf64_symbol));
	}
	if (binding == STB_LOCAL) eb->shs[ASM_SH_SYMTAB].sh_info++;
	return sym_num;
}

elf64_word
asm_elf_find_symbol(Asm_elf_builder *eb, const char *name)
{
	elf64_word str_num = asm_elf_find_str(eb, name);
	if (str_num == 0) return 0;
	Elf64_symbol *symbols = eb->bufs[ASM_SH_SYMTAB].data;
	size_t sym_count = eb->bufs[ASM_SH_SYMTAB].len / sizeof(Elf64_symbol);
	for (elf64_word i = 0; i < sym_count; ++i) {
		if (symbols[i].st_name == str_num) return i;
	}
	return 0;
}


elf64_word
asm_elf_add_rela_text(Asm_elf_builder *eb, elf64_word symndx, enum elf_amd64_system_v_rela_type type,
					  elf64_off offset, elf64_sxword addend)
{
	elf64_word s = eb->bufs[ASM_SH_SYMTAB].len / sizeof(Elf64_rela);
	Elf64_rela rela = {
		.r_info = ELF64_R_INFO(symndx, type),
		.r_offset = offset,
		.r_addend = addend,
	};
	byte_buffer_insert_bytes(&eb->bufs[ASM_SH_RELA_TEXT], &rela, sizeof(rela));
	return s;
}

elf64_word
asm_elf_add_rela_init_array(Asm_elf_builder *eb, elf64_word symndx, enum elf_amd64_system_v_rela_type type,
					  elf64_off offset, elf64_sxword addend)
{
	elf64_word s = eb->bufs[ASM_SH_SYMTAB].len / sizeof(Elf64_rela);
	Elf64_rela rela = {
		.r_info = ELF64_R_INFO(symndx, type),
		.r_offset = offset,
		.r_addend = addend,
	};
	byte_buffer_insert_bytes(&eb->bufs[ASM_SH_RELA_INIT_ARRAY], &rela, sizeof(rela));
	return s;
}

void
asm_elf_add_init_thunk(Asm_elf_builder *eb, elf64_word sym_num)
{
	elf64_off offset = byte_buffer_get_cursor_offset(&eb->bufs[ASM_SH_INIT_ARRAY]);
	asm_elf_add_rela_init_array(eb, sym_num, R_X86_64_64, offset, 0);
	byte_buffer_fill_bytes(&eb->bufs[ASM_SH_INIT_ARRAY], 0, sizeof(int64_t));
}


#ifndef NO_STD_HEADERS
void
asm_elf_dump_shstrtab(Asm_elf_builder *eb)
{
	const char *tab = eb->bufs[ASM_SH_SHSTRTAB].data;
	for (size_t i = 1; i < eb->bufs[ASM_SH_SHSTRTAB].len; ++i) {
		if (tab[i])
			fputc(tab[i], stdout);
		else
			fputc('\n', stdout);
	}
}

void
asm_elf_resolve_symbols(Asm_module *m, Asm_elf_builder *eb)
{
	/* Add symbols. This must be done before adding RELAs because re-ordering may occur */
	for (size_t i = 0; i < m->labels.len; ++i) {
		Asm_label *lbl = &m->labels.elems[i];
		if (lbl->type == ASM_LBL_T_BLOCK) continue;
		enum elf_symbol_type st = STT_NOTYPE;
		enum elf_symbol_binding sb = STB_GLOBAL;
		switch (lbl->type) {
		case ASM_LBL_T_CONST: FAILWITH("TODO: ASM_LBL_CONST"); break;
		case ASM_LBL_T_DATA:  st = STT_OBJECT; break;
		case ASM_LBL_T_FUNC:  st = STT_FUNC;   break;
		case ASM_LBL_T_BLOCK:
		default: FAILWITH("Unreachable"); break;
		}
		switch (lbl->binding) {
		case ASM_LBL_B_GLOBAL: sb = STB_GLOBAL; break;
		case ASM_LBL_B_LOCAL:  sb = STB_LOCAL;  break;
		default: FAILWITH("Unreachable"); break;
		}
		if (lbl->is_extern) {
			asm_elf_add_symbol(eb, lbl->name, st, STB_GLOBAL, 0, 0, 0);
		} else {
			asm_elf_add_symbol(eb, lbl->name, st, sb, ASM_SH_TEXT, lbl->offset, 0);
		}
	}
	for (size_t i = 0; i < m->labels.len; ++i) {
		Asm_label *lbl = &m->labels.elems[i];
		if (lbl->type == ASM_LBL_T_BLOCK) {
			ASSERT(!lbl->is_extern);
			ASSERT(lbl->is_resolved);
			ASSERT(lbl->patches.len == 0);
			continue;
		}
		elf64_word sym_num = asm_elf_find_symbol(eb, lbl->name);
		ASSERT(sym_num);
		if (lbl->is_extern) {
			ASSERT(!lbl->is_resolved);
			enum elf_amd64_system_v_rela_type type;
			switch (lbl->type) {
			case ASM_LBL_T_CONST: FAILWITH("TODO: ASM_LBL_CONST"); break;
			case ASM_LBL_T_DATA:  type = R_X86_64_PC32;  break;
			case ASM_LBL_T_FUNC:  type = R_X86_64_PLT32; break;
			case ASM_LBL_T_BLOCK:
			default: FAILWITH("Unreachable"); break;
			}
			for (size_t i = 0; i < lbl->patches.len; ++i) {
				struct asm_lbl_back_patch *p = &lbl->patches.elems[i];
				ASSERT(p->size == sizeof(int32_t));
				asm_elf_add_rela_text(eb, sym_num, type, p->loc, -4);
			}
			lbl->is_resolved = true;
			da_free(&lbl->patches);
		} else {
			ASSERT(lbl->patches.len == 0);
		}
		if (lbl->is_init)
			asm_elf_add_init_thunk(eb, sym_num);
		if (lbl->is_fini)
			FAILWITH("TODO: fini");
	}
}

int
asm_elf_to_file(Asm_elf_builder *eb, int fileno)
{
	eb->hdr.e_phnum = eb->phs.len;
	if (eb->hdr.e_phnum)
		eb->hdr.e_phoff = sizeof(Elf64_header);
	elf64_off e_shoff = sizeof(Elf64_header);
	for (int i = 1; i < ASM_SH_COUNT; ++i) {
		eb->shs[i].sh_offset = e_shoff;
		eb->shs[i].sh_size = eb->bufs[i].len;
		e_shoff += eb->bufs[i].len;
	}
	eb->hdr.e_shoff = e_shoff;
	if (write(fileno, &eb->hdr, sizeof(eb->hdr)) == -1) return -1;
	if (write(fileno, eb->phs.elems, sizeof(*eb->phs.elems) * eb->phs.len) == -1) return -1;
	for (int i = 1; i < ASM_SH_COUNT; ++i)
		if (write(fileno, eb->bufs[i].data, eb->bufs[i].len) == -1) return -1;
	for (int i = 0; i < ASM_SH_COUNT; ++i)
		if (write(fileno, &eb->shs[i], sizeof(eb->shs[0])) == -1) return -1;
	return 0;
}

void
asm_output_static_executable(Asm_module *m, asm_label_id entry_lbl, const char *filename)
{
	elf64_addr vaddr = PAGE_SIZE * 16;
	elf64_off pg_offset = sizeof(Elf64_header) + sizeof(Elf64_program_header);
	elf64_off entry_point = vaddr + pg_offset + asm_get_label_offset(m, entry_lbl);
	Byte_buffer elf_buff = byte_buffer_create_default();
	Elf64_header eh = {
		.e_ident[EI_MAG0]	 = ELF_MAGIC[EI_MAG0],
		.e_ident[EI_MAG1]	 = ELF_MAGIC[EI_MAG1],
		.e_ident[EI_MAG2]	 = ELF_MAGIC[EI_MAG2],
		.e_ident[EI_MAG3]	 = ELF_MAGIC[EI_MAG3],
		.e_ident[EI_CLASS]	 = ELFCLASS64,
		.e_ident[EI_DATA]	 = ELFDATA2LSB,
		.e_ident[EI_VERSION] = 1,
		.e_ident[EI_OSABI]	 = ELFOSABI_SYSV,
		.e_type				 = ET_EXEC,
		.e_machine			 = EM_X86_64,
		.e_version			 = 1,
		.e_entry			 = entry_point,
		.e_phoff			 = sizeof(Elf64_header),
		.e_ehsize			 = sizeof(Elf64_header),
		.e_phentsize		 = sizeof(Elf64_program_header),
		.e_phnum			 = 1,
	};
	Elf64_program_header ph = {
		.p_type	  = PT_LOAD,
		.p_flags  = PF_R|PF_W|PF_X,
		.p_offset = 0,
		.p_vaddr  = vaddr,
		.p_filesz = pg_offset + m->mc.len,
		.p_memsz  = pg_offset + m->mc.len,
	};
	byte_buffer_insert_bytes(&elf_buff, &eh, sizeof(eh));
	byte_buffer_insert_bytes(&elf_buff, &ph, sizeof(ph));
	byte_buffer_insert_bytes(&elf_buff, m->mc.data, m->mc.len);
	int out;
	if ((out = open(filename, O_CREAT|O_TRUNC|O_WRONLY)) == -1)		goto fail0;
	if (byte_buffer_write_data_to_file(&elf_buff, out) == -1)       goto fail1;
	if (fchmod(out, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH) == -1) goto fail1;
	if (close(out) == -1)											goto fail0;
	elf_buff.free(&elf_buff);
	return;
fail1:
	close(out);
fail0:
	elf_buff.free(&elf_buff);
#ifndef NO_STD_HEADERS
	FAILWITH("[ERROR] %s\n", strerror(errno));
#else
	FAILWITH(DEBUG_MSG("Unable to write to output file"));
#endif
}

/* NOTE:
 * ld -dynamic-linker /lib/ld-linux-x86-64.so.2 /usr/lib/crt1.o /usr/lib/crti.o -lc test.o /usr/lib/crtn.o -o test
 */

void
asm_output_object_file(Asm_module *m, const char *filename)
{
	Asm_elf_builder eb = asm_make_elf_builder(ET_REL);
	byte_buffer_insert_bytes(&eb.bufs[ASM_SH_TEXT], m->mc.data, m->mc.len);
	asm_elf_resolve_symbols(m, &eb);
	int out;
	if ((out = open(filename, O_CREAT|O_TRUNC|O_WRONLY)) == -1)	goto fail0;
	if (asm_elf_to_file(&eb, out) == -1)                        goto fail1;
	if (fchmod(out, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) == -1)     goto fail1;
	if (close(out) == -1)										goto fail0;
	return;
fail1:
	close(out);
fail0:
#ifndef NO_STD_HEADERS
	FAILWITH("[ERROR] %s\n", strerror(errno));
#else
	FAILWITH(DEBUG_MSG("Unable to write to output file"));
#endif
}

#endif /* NO_STD_HEADERS */


#endif /* MEM_IMPLEMENTATION */
