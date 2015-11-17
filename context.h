#ifndef VIO_CONTEXT_H
#define VIO_CONTEXT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <gmp.h>
#include "error.h"
#include "dict.h"
#include "bytecode.h"
#include "types.h"
#include "val.h"

#define VIO_MAX_ERR_LEN 255
#define VIO_STACK_SIZE 2048
#define VIO_MAX_FUNCTIONS (32*1024)

struct _vctx {
    /* first node in the linked list of objects that we scan to gc */
    vio_val *ohead;
    /* number of objects we've allocated so we know when to gc */
    uint32_t ocnt;
    
    vio_val *stack[VIO_STACK_SIZE];
    uint32_t sp;

    vio_bytecode **defs;
    uint32_t defp; /* next index in `defs` */
    vio_dict *dict; /* maps function names to indices in `defs` */

    vio_err_t err;
    char err_msg[VIO_MAX_ERR_LEN];
};

vio_err_t vio_open(vio_ctx *ctx);
void vio_close(vio_ctx *ctx);

/* Does not actually interrupt exeuction of the program; rather
   sets ctx->err and ctx->err_msg. It is up to the VM to suspend
   operation in case of an error.
   @returns its error argument */
vio_err_t vio_raise(vio_ctx *ctx, vio_err_t err, const char *msg, ...);

/* Return the type of the value on top of the stack.
   Returns 0 if the stack is empty, because using vio_err_t
   would be clumpsy. However that may be subject to change.
   @returns either 0 or an int that can safely be cast to a vio_val_t */
int vio_what(vio_ctx *ctx);

vio_err_t vio_pop_str(vio_ctx *ctx, uint32_t *len, char **out);
vio_err_t vio_pop_int(vio_ctx *ctx, vio_int *out);
vio_err_t vio_pop_float(vio_ctx *ctx, vio_float *out);
vio_err_t vio_pop_num(vio_ctx *ctx, mpf_t *out);
vio_err_t vio_pop_vecf32(vio_ctx *ctx, uint32_t *len, vio_float **out);
vio_err_t vio_pop_matf32(vio_ctx *ctx, uint32_t *rows, uint32_t *cols, vio_float **out);

vio_err_t vio_top_str(vio_ctx *ctx, uint32_t *len, char **out);
vio_err_t vio_top_int(vio_ctx *ctx, vio_int *out);
vio_err_t vio_top_float(vio_ctx *ctx, vio_float *out);
vio_err_t vio_top_num(vio_ctx *ctx, mpf_t *out);
vio_err_t vio_top_vecf32(vio_ctx *ctx, uint32_t *len, vio_float **out);
vio_err_t vio_top_matf32(vio_ctx *ctx, uint32_t *rows, uint32_t *cols, vio_float **out);

/* These functions push the internal tag/vec/mat values onto the stack
   and so do not take an out parameter for those (but do take one for the
   number of elements they have pushed). */
vio_err_t vio_pop_tag(vio_ctx *ctx, uint32_t *nlen, char **name, uint32_t *vlen);
vio_err_t vio_pop_vec(vio_ctx *ctx, uint32_t *len);
vio_err_t vio_pop_mat(vio_ctx *ctx, uint32_t *rows, uint32_t *cols);

vio_err_t vio_push_str(vio_ctx *ctx, uint32_t len, char *val);
vio_err_t vio_push_int(vio_ctx *ctx, vio_int val);
vio_err_t vio_push_float(vio_ctx *ctx, vio_float val);
vio_err_t vio_push_num(vio_ctx *ctx, const mpf_t val);
vio_err_t vio_push_vecf32(vio_ctx *ctx, uint32_t len, vio_float *val);
vio_err_t vio_push_matf32(vio_ctx *ctx, uint32_t rows, uint32_t cols, vio_float *val);

vio_err_t vio_push_str_cnt(vio_ctx *ctx, uint32_t cnt, uint32_t *lens, char **vals);
vio_err_t vio_push_int_cnt(vio_ctx *ctx, uint32_t cnt, vio_int *vals);
vio_err_t vio_push_float_cnt(vio_ctx *ctx, uint32_t cnt, vio_float *vals);
vio_err_t vio_push_num_cnt(vio_ctx *ctx, uint32_t cnt, const mpf_t *vals);

/* Similarly, these pop some number of elements from the stack. */
vio_err_t vio_push_tag(vio_ctx *ctx, uint32_t nlen, char *name, uint32_t vlen);
vio_err_t vio_push_vec(vio_ctx *ctx, uint32_t len);
vio_err_t vio_push_mat(vio_ctx *ctx, uint32_t rows, uint32_t cols);

#endif
