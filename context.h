#ifndef VIO_CONTEXT_H
#define VIO_CONTEXT_H

#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <gmp.h>
#include "error.h"
#include "val.h"

#define VIO_MAX_ERR_LEN 255
#define VIO_STACK_SIZE 2048

typedef struct _vctx {
    /* first node in the linked list of objects that we scan to gc */
    struct _vval *ohead;
    /* number of objects we've allocated so we know when to gc */
    uint32_t ocnt;
    
    struct _vval *stack[VIO_STACK_SIZE];
    uint32_t sp;
} vio_ctx;

vio_err_t vio_open(vio_ctx *ctx);
void vio_close(vio_ctx *ctx);

void vio_raise(vio_ctx *ctx, vio_err_t err, const char *msg, ...);

vio_err_t vio_pop_str(vio_ctx *ctx, uint32_t *len, char **out);
vio_err_t vio_pop_i32(vio_ctx *ctx, int32_t *out);
vio_err_t vio_pop_f32(vio_ctx *ctx, float *out);
vio_err_t vio_pop_num(vio_ctx *ctx, mpf_t *out);
vio_err_t vio_pop_vecf32(vio_ctx *ctx, uint32_t *len, float **out);
vio_err_t vio_pop_matf32(vio_ctx *ctx, uint32_t *rows, uint32_t *cols, float **out);

/* These functions push the internal tag/vec/mat values onto the stack
   and so do not take an out parameter for those (but do take one for the
   number of elements they have pushed). */
vio_err_t vio_pop_tag(vio_ctx *ctx, uint32_t *nlen, char **name, uint32_t *vlen);
vio_err_t vio_pop_vec(vio_ctx *ctx, uint32_t *len);
vio_err_t vio_pop_mat(vio_ctx *ctx, uint32_t *rows, uint32_t *cols);

vio_err_t vio_push_str(vio_ctx *ctx, uint32_t len, char *val);
vio_err_t vio_push_i32(vio_ctx *ctx, int32_t val);
vio_err_t vio_push_f32(vio_ctx *ctx, float val);
vio_err_t vio_push_num(vio_ctx *ctx, const mpf_t val);
vio_err_t vio_push_vecf32(vio_ctx *ctx, uint32_t len, float *val);
vio_err_t vio_push_matf32(vio_ctx *ctx, uint32_t rows, uint32_t cols, float *val);

/* Similarly, these pop some number of elements from the stack. */
vio_err_t vio_push_tag(vio_ctx *ctx, uint32_t nlen, char *name, uint32_t vlen);
vio_err_t vio_push_vec(vio_ctx *ctx, uint32_t len);
vio_err_t vio_push_mat(vio_ctx *ctx, uint32_t rows, uint32_t cols);

#endif
