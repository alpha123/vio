#ifndef VIO_OPCODES_H
#define VIO_OPCODES_H

#include <stdint.h>
#include "attrib.h"

typedef uint8_t vio_instruction_t;
typedef uint64_t vio_opcode;

#define VIO_OP_INSTR_MASK	0x00000000000000ffULL
#define VIO_OP_IMM1_MASK	0x000000ffffffff00ULL
#define VIO_OP_IMM2_MASK	0x0000ff0000000000ULL
#define VIO_OP_IMM3_MASK	0x00ff000000000000ULL
#define VIO_OP_IMM4_MASK	0xff00000000000000ULL
#define VIO_OP_IMM1_SHIFT	0x08
#define VIO_OP_IMM2_SHIFT	0x28
#define VIO_OP_IMM3_SHIFT	0x30
#define VIO_OP_IMM4_SHIFT	0x38

#define vio_opcode_instr(oc) ((oc) & VIO_OP_INSTR_MASK)
#define vio_opcode_imm1(oc) ((uint32_t)(((oc) & VIO_OP_IMM1_MASK) >> VIO_OP_IMM1_SHIFT))
#define vio_opcode_imm2(oc) ((uint8_t)(((oc) & VIO_OP_IMM2_MASK) >> VIO_OP_IMM2_SHIFT))
#define vio_opcode_imm3(oc) ((uint8_t)(((oc) & VIO_OP_IMM3_MASK) >> VIO_OP_IMM3_SHIFT))
#define vio_opcode_imm4(oc) ((uint8_t)((oc) >> VIO_OP_IMM4_SHIFT))

VIO_CONST
vio_opcode vio_opcode_pack(
    vio_instruction_t instr, uint32_t imm1,
    uint8_t imm2, uint8_t imm3, uint8_t imm4
);

/* pcX opcodes handle parser combinators */

#define LIST_VM_INSTRUCTIONS(X, X_) \
    X(halt) \
    X(load) \
    X(call) \
    X(ret) \
    X(callc) \
    X(callq) \
    X(retq) \
    X(reljmp) \
    X(add) \
    X(sub) \
    X(mul) \
    X(div) \
    X(dup) \
    X(rot) \
    X(swap) \
    X(vec) \
    X(vecf) \
    X(pcparse) \
    X(pcor) \
    X(pcthen) \
    X(pcnot) \
    X(pcmaybe) \
    X(pcmany) \
    X(pcmore) \
    X(pcmatchstr) \
    X_(nop)

#define DEF_INSTR_CONSTS(instr) vop_##instr,
#define DEF_INSTR_CONSTS_(instr) vop_##instr

#ifdef __GNUC__
#define PACKED_ENUM __attribute__((__packed__))
#else
#define PACKED_ENUM
#endif

/* should be the size of vio_instruction_t above, i.e. 8 bits */
enum PACKED_ENUM vio_instructions {
    LIST_VM_INSTRUCTIONS(DEF_INSTR_CONSTS, DEF_INSTR_CONSTS_)
};

#undef DEF_INSTR_CONSTS
#undef DEF_INSTR_CONSTS_

VIO_CONST
const char *vio_instr_mneumonic(vio_instruction_t instr);

/*typedef uint64_t vio_compiled_program;

#define VIO_OPCODE_DECOMPRESSION_BUFSIZE 32

typedef struct _vopstate {
    LZ4_streamDecode_t *lz;
    vio_opcode buf[VIO_OPCODE_DECOMPRESSION_BUFSIZE];
    uint32_t bufi;

    vio_compiled_program *p;
    uint32_t plen;
    uint32_t off;
} vio_opcode_reader;

void vio_opcode_reader_open(vio_opcode_reader *r, vio_compiled_program *prog, uint32_t plen);
void vio_opcode_reader_close(vio_opcode_reader *r);
vio_opcode vio_read_opcode(vio_opcode_reader *r, uint32_t idx);*/

#endif
