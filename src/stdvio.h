#ifndef VIO_STDVIO_H
#define VIO_STDVIO_H

#include "context.h"

void vio_load_stdlib(vio_ctx *ctx);

vio_err_t vio_drop(vio_ctx *ctx);

#endif
