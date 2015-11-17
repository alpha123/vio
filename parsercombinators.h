#ifndef VIO_PARSERCOMBINATORS_H
#define VIO_PARSERCOMBINATORS_H

#include "error.h"
#include "context.h"

vio_err_t vio_pc_str(vio_ctx *ctx);
vio_err_t vio_pc_then(vio_ctx *ctx);
vio_err_t vio_pc_or(vio_ctx *ctx);
vio_err_t vio_pc_not(vio_ctx *ctx);
vio_err_t vio_pc_maybe(vio_ctx *ctx);
vio_err_t vio_pc_many(vio_ctx *ctx);
vio_err_t vio_pc_more(vio_ctx *ctx);

#endif
