#ifndef VIO_VECAPPLY_H
#define VIO_VECAPPLY_H

#include "context.h"
#include "val.h"

vio_err_t vio_fold_val(vio_ctx *ctx, vio_val *v, vio_val *q, vio_val *initial, vio_val **out);
vio_err_t vio_fold(vio_ctx *ctx);

vio_err_t vio_partition_val(vio_ctx *ctx, vio_val *v, vio_val *q, vio_val **out_rets, vio_val **out_parts);
vio_err_t vio_partition(vio_ctx *ctx);

#endif
