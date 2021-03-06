#ifndef VIO_SERIALIZE_H
#define VIO_SERIALIZE_H

#include <stdio.h>
#include "context.h"
#include "val.h"

/* Save a Vio image file. A Vio image is a snapshot
   of the system at some point in time, which can be
   loaded and resumed later. */
vio_err_t vio_dump(vio_ctx *ctx, FILE *fp);
vio_err_t vio_load(vio_ctx *ctx, FILE *fp);

/* Write the top of the stack to a file.
   In the case of an empty stack, returns VE_STACK_EMPTY
   and does nothing. */
vio_err_t vio_dump_top(vio_ctx *ctx, FILE *fp);
/* Read a value from a file and pushes onto the top of the stack. */
vio_err_t vio_load_top(vio_ctx *ctx, FILE *fp);

/* Writes an arbitrary value to a file.
   Normally one does not have to handle vio_vals directly,
   and this function should be considered a semi-private
   API. */
vio_err_t vio_dump_val(vio_val *v, FILE *fp);
vio_err_t vio_load_val(vio_ctx *ctx, vio_val **v, FILE *fp);


vio_err_t vio_dump_bytecode(vio_bytecode *bc, FILE *fp);
vio_err_t vio_load_bytecode(vio_ctx *ctx, vio_bytecode **bc, FILE *fp);

/* Save/load all currently defined words */
vio_err_t vio_dump_dict(vio_ctx *ctx, FILE *fp);
vio_err_t vio_load_dict(vio_ctx *ctx, FILE *fp);

const char *vio_json_vals(vio_val **vs, uint32_t cnt);
const char *vio_json_val(vio_val *v);
const char *vio_json_top(vio_ctx *ctx);
const char *vio_json_stack(vio_ctx *ctx);

vio_err_t vio_save_image(vio_ctx *ctx, const char *image_file);

vio_err_t vio_open_image(vio_ctx *ctx, const char *image_file);
vio_err_t vio_close_image(vio_ctx *ctx, const char *image_file);

#endif
