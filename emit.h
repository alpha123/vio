#ifndef VIO_EMIT_H
#define VIO_EMIT_H

#include "error.h"
#include "opcodes.h"
#include "tok.h"
#include "val.h"

vio_err_t vio_emit(vio_ctx *ctx, vio_tok *t, uint32_t *plen, vio_opcode **prog, uint32_t *clen, vio_val ***consts);

#endif
