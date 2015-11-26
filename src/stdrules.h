#ifndef VIO_STDRULES_H
#define VIO_STDRULES_H

#include "error.h"
#include "context.h"

void vio_load_stdrules(vio_ctx *ctx);

vio_err_t vio_rule_digit(vio_ctx *ctx);
vio_err_t vio_rule_space(vio_ctx *ctx);
vio_err_t vio_rule_any(vio_ctx *ctx);
vio_err_t vio_rule_range(vio_ctx *ctx);
vio_err_t vio_rule_oneof(vio_ctx *ctx);
vio_err_t vio_rule_noneof(vio_ctx *ctx);

#endif
