#ifndef VIO_GC_H
#define VIO_GC_H

#include <stdint.h>
#include <gmp.h>
#include "context.h"

#define VIO_GC_THRESH 100

void vio_mark(vio_ctx *ctx);
void vio_sweep(vio_ctx *ctx);
void vio_gc(vio_ctx *ctx);

#endif
