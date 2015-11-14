#ifndef VIO_SERIALIZE_H
#define VIO_SERIALIZE_H

#include <stdio.h>
#include "context.h"
#include "val.h"

/* Write the top of the stack to a file. */
vio_err_t vio_dump(FILE *fp, vio_ctx *ctx);
vio_err_t vio_dump_val(FILE *fp, vio_val *v);

/* Read a value from a file and pushes onto the top of the stack. */
vio_err_t vio_load(FILE *fp, vio_ctx *ctx);
vio_err_t vio_load_val(FILE *fp, vio_val **v);

#endif
