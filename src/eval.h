#ifndef VIO_EVAL_H
#define VIO_EVAL_H

#include "error.h"
#include "context.h"

vio_err_t vio_eval(vio_ctx *ctx, int64_t len, const char *expr);

#endif
