#include "opcodes.h"

VIO_CONST
vio_opcode vio_opcode_pack(vio_instruction_t instr, uint32_t imm1,
                                     uint8_t imm2, uint8_t imm3, uint8_t imm4) {
    return
     ((vio_opcode)imm4 << VIO_OP_IMM4_SHIFT) | ((vio_opcode)imm3 << VIO_OP_IMM3_SHIFT) |
     ((vio_opcode)imm2 << VIO_OP_IMM2_SHIFT) | ((vio_opcode)imm1 << VIO_OP_IMM1_SHIFT) |
     (vio_opcode)instr;
}

/*
    Eventually I'd like to compress opcodes (since Vio uses very
    wide (64-bit) opcodes) to save memory. Originally LZ4 looked
    good since the overhead is neglible, but now I'm considering
    an integer SIMD compression scheme.
    (See https://github.com/lemire/simdcomp)

void vio_opcode_reader_open(vio_opcode_reader *r, vio_compiled_program *p, uint32_t plen) {
    r->lz = LZ4_createStreamDecode();
    r->p = p;
    r->plen = plen;
    r->off = 0;
    r->bufi = 0;
}

void vio_opcode_reader_close(vio_opcode_reader *r) {
    No need to actually do anything until I switch the implementation
    to use LZ4 streaming.
}

vio_opcode vio_read_opcode(vio_opcode_reader *r, uint32_t idx) {
    if (r->bufi < VIO_OPCODE_DECOMPRESSION_BUFSIZE)
        return r->buf[r->bufi++];

    if (LZ4_decompress_safe_partial(r->p + r->off, r->buf, r->plen - r->off,
                                    VIO_OPCODE_DECOMPRESSION_BUFSIZE * sizeof(vio_opcode),
                                    VIO_OPCODE_DECOMPRESSION_BUFSIZE * sizeof(vio_opcode))) {
        r->bufi = 0;
        return r->buf[r->bufi++];
    }

    return (vio_opcode)vop_halt;
}*/
