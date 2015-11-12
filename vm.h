#ifndef VIO_VM_H
#define VIO_VM_H

#include "context.h"
#include "val.h"
#include "opcodes.h"

#define VIO_MAX_CALL_DEPTH 2000

vio_err_t vio_exec(vio_ctx *ctx, vio_opcode *prog, vio_val **consts);

#endif
