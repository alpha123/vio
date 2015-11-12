#ifndef VIO_MATH_H
#define VIO_MATH_H

#include <stdint.h>
#include "val.h"
#include "context.h"

/* Pops 2 elements from the stack. The result will be
   pushed on to the stack.
   These are somewhat higher-level functions that abide
   by Vio's vector semantics. */
vio_err_t vio_add(vio_ctx *ctx);
vio_err_t vio_sub(vio_ctx *ctx);
vio_err_t vio_mul(vio_ctx *ctx);
vio_err_t vio_div(vio_ctx *ctx);

#endif
