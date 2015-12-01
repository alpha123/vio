#ifndef VIO_CMP_H
#define VIO_CMP_H

#include "context.h"
#include "val.h"

int vio_cmp_val(vio_ctx *ctx, vio_val *a, vio_val *b);
int vio_cmp(vio_ctx *ctx);

vio_err_t vio_compare(vio_ctx *ctx);

#endif
