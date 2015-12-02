#ifndef VIO_TYPES_H
#define VIO_TYPES_H

#include "error.h"
#include <stdint.h>

typedef double vio_float;
typedef int64_t vio_int;

typedef struct _vctx vio_ctx;
typedef struct _vval vio_val;
typedef struct _vbytecode vio_bytecode;
typedef struct _vfninfo vio_function_info;

typedef vio_err_t (* vio_function)(vio_ctx *);

#endif
