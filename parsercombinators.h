#ifndef VIO_PARSERCOMBINATORS_H
#define VIO_PARSERCOMBINATORS_H

#include "error.h"
#include "context.h"

/* expects a parser on top of the stack and
   a string immediately below it */
vio_err_t vio_pc_parse(vio_ctx *ctx);

/* Parser creation */
vio_err_t vio_pc_str(vio_ctx *ctx);

/* Binary combinators */
vio_err_t vio_pc_then(vio_ctx *ctx);
vio_err_t vio_pc_or(vio_ctx *ctx);

/* Unary combinators */
vio_err_t vio_pc_not(vio_ctx *ctx);
vio_err_t vio_pc_maybe(vio_ctx *ctx);
vio_err_t vio_pc_many(vio_ctx *ctx);
vio_err_t vio_pc_more(vio_ctx *ctx);

#endif
