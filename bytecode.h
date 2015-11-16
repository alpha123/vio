#ifndef VIO_BYTECODE_H
#define VIO_BYTECODE_H

#include "error.h"
#include "opcodes.h"
#include "tok.h"
#include "val.h"

typedef struct _vbytecode_state {
    vio_opcode *prog;
    vio_val **consts;

    size_t psz, csz;
    uint32_t ip, ic;
} vio_bytecode;

vio_err_t vio_emit(vio_ctx *ctx, vio_tok *t, vio_bytecode **out);
void vio_bytecode_free(vio_bytecode *bc);

#endif
