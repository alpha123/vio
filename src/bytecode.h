#ifndef VIO_BYTECODE_H
#define VIO_BYTECODE_H

#include "error.h"
#include "opcodes.h"
#include "tok.h"
#include "types.h"
#include "val.h"

struct _vbytecode {
    vio_opcode *prog;
    vio_val **consts;

    size_t psz, csz;
    uint32_t ip, ic;
};

vio_err_t vio_emit(vio_ctx *ctx, vio_tok *t, vio_bytecode **out);
vio_err_t vio_bytecode_alloc(vio_bytecode **out);
void vio_bytecode_free(vio_bytecode *bc);

vio_err_t vio_bytecode_alloc_opcodes(vio_bytecode *bc);
vio_err_t vio_bytecode_alloc_consts(vio_bytecode *bc);

vio_err_t vio_bytecode_clone(vio_ctx *ctx, vio_bytecode *bc, vio_bytecode **out);

#endif
