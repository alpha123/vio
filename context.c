#include <stdio.h>
#include <string.h>
#include "context.h"

vio_err_t vio_open(vio_ctx *ctx) {
    ctx->ohead = NULL;
    ctx->ocnt = 0;
    ctx->sp = 0;
    return 0;
}

void vio_close(vio_ctx *ctx) {
    vio_val *v = ctx->ohead, *n;
    while (v) {
        n = v->next;
        /* no need to recursively check tag/vec/mats beacuse all allocated objects
           will be in the list anyway */
        free(v);
        v = n;
    }
}

void vio_raise(vio_ctx *ctx, vio_err_t err, const char *msg, ...) {
    char emsg[VIO_MAX_ERR_LEN];
    va_list args;
    va_start(args, msg);
    vsnprintf(emsg, VIO_MAX_ERR_LEN, msg, args);
    va_end(args);
}

vio_err_t pop_expect(vio_ctx *ctx, vio_val **v, int expect) {
    if (ctx->sp == 0)
        return VE_STACK_EMPTY;
    if (expect > 0 && ctx->stack[ctx->sp-1]->what != expect)
        return VE_STRICT_TYPE_FAIL;
    *v = ctx->stack[--ctx->sp];
    return 0;
}

vio_err_t push(vio_ctx *ctx, vio_val *v) {
    if (ctx->sp == VIO_STACK_SIZE)
        return VE_STACK_OVERFLOW;
    ctx->stack[ctx->sp++] = v;
    return 0;
}

#define POP(type) \
    vio_val *v; \
    vio_err_t err; \
    if ((err = pop_expect(ctx, &v, type))) \
        goto error;

vio_err_t vio_pop_str(vio_ctx *ctx, uint32_t *len, char **out) {
    POP(vv_str)

    *len = v->len;
    *out = v->s;
    return 0;

    error:
    *len = 0;
    *out = NULL;
    return err;
}

vio_err_t vio_pop_i32(vio_ctx *ctx, int32_t *out) {
    POP(vv_int)
    *out = v->i32;
    return 0;

    error:
    *out = 0;
    return err;
}

vio_err_t vio_pop_f32(vio_ctx *ctx, float *out) {
    POP(vv_float)
    *out = v->f32;
    return 0;

    error:
    *out = 0.0f;
    return err;
}

vio_err_t vio_pop_num(vio_ctx *ctx, mpf_t *out) {
    POP(vv_num)
    mpf_set(*out, v->n);
    return 0;

    error:
    return err;
}

vio_err_t vio_pop_vecf32(vio_ctx *ctx, uint32_t *len, float **out) {
    *out = NULL;
    POP(vv_vecf)
    *len = v->vlen;
    *out = v->vf32;
    return 0;

    error:
    *len = 0;
    *out = NULL;
    return err;
}

vio_err_t vio_pop_matf32(vio_ctx *ctx, uint32_t *rows, uint32_t *cols, float **out) {
    *out = NULL;
    POP(vv_matf);
    *rows = v->rows;
    *cols = v->cols;
    *out = v->vf32;
    return 0;

    error:
    *rows = 0;
    *cols = 0;
    *out = NULL;
    return err;
}


vio_err_t vio_pop_tag(vio_ctx *ctx, uint32_t *nlen, char **name, uint32_t *vlen) {
    uint32_t i = 0;
    POP(vv_tagword)
    *nlen = v->len;
    *name = v->s;
    *vlen = v->vlen;
    for (; i < v->vlen; ++i) {
        if ((err = push(ctx, v->vv[i])))
            goto error;
    }
    return 0;

    error:
    *nlen = 0;
    *name = NULL;
    *vlen = 0;

    /* if we stack overflowed at some point, undo the push operations to
       ensure the stack looks the same as it did before this function call */
    while (i) {
        --ctx->sp;
        --i;
    }
    return err;
}

vio_err_t vio_pop_vec(vio_ctx *ctx, uint32_t *len) {
    uint32_t i = 0;
    POP(vv_vec)
    *len = v->vlen;
    for (; i < v->vlen; ++i) {
        if ((err = push(ctx, v->vv[i])))
            goto error;
    }
    return 0;

    error:
    *len = 0;
    while (i) {
        --ctx->sp;
        --i;
    }
    return err;
}

vio_err_t vio_pop_mat(vio_ctx *ctx, uint32_t *rows, uint32_t *cols) {
    uint32_t i = 0, len;
    POP(vv_mat)
    len = v->rows * v->cols;
    *rows = v->rows;
    *cols = v->cols;
    for (; i < len; ++i) {
        if ((err = push(ctx, v->vv[i])))
            goto error;
    }
    return 0;

    error:
    *rows = 0;
    *cols = 0;
    while (i) {
        --ctx->sp;
        --i;
    }
    return err;
}

#define PUSH(what) \
    vio_val *v; \
    vio_err_t err = 0; \
    if ((err = vio_val_new(ctx, &v, what)) || \
        (err = push(ctx, v))) \
        goto error; \

#define HANDLE_ERRORS \
    error: \
    if (v) free(v); \
    if (err != VE_STACK_OVERFLOW) \
        --ctx->sp; \
    return err;

vio_err_t vio_push_str(vio_ctx *ctx, uint32_t len, char *val) {
    PUSH(vv_str)
    v->len = len;
    v->s = (char *)malloc(len);
    if (v->s == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    strncpy(v->s, val, len);
    return 0;

    HANDLE_ERRORS
}

vio_err_t vio_push_i32(vio_ctx *ctx, int32_t val) {
    PUSH(vv_int)
    v->i32 = val;
    return 0;
    HANDLE_ERRORS
}

vio_err_t vio_push_f32(vio_ctx *ctx, float val) {
    PUSH(vv_float)
    v->f32 = val;
    return 0;
    HANDLE_ERRORS
}

vio_err_t vio_push_num(vio_ctx *ctx, const mpf_t val) {
    PUSH(vv_num)
    mpf_init(v->n);
    mpf_set(v->n, val);
    return 0;
    HANDLE_ERRORS
}

vio_err_t vio_push_vecf32(vio_ctx *ctx, uint32_t len, float *val) {
    PUSH(vv_vecf)
    v->vlen = len;
    v->vf32 = (float *)malloc(len * sizeof(float));
    if (v->vf32 == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    memcpy(v->vf32, val, len * sizeof(float));
    return 0;

    /* we'll never have a state where vf32 has been allocated
       successfully and we still have an error
       thus we don't need to free it here */
    HANDLE_ERRORS
}

vio_err_t vio_push_matf32(vio_ctx *ctx, uint32_t rows, uint32_t cols, float *val) {
    uint32_t len = rows * cols;
    PUSH(vv_matf)
    v->rows = rows;
    v->cols = cols;
    v->vf32 = (float *)malloc(len * sizeof(float));
    if (v->vf32 == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    memcpy(v->vf32, val, len * sizeof(float));
    return 0;

    HANDLE_ERRORS
}

vio_err_t pop_into_vec(vio_ctx *ctx, uint32_t len, uint32_t *idx) {
    vio_err_t err = 0;
    if (ctx->sp < len + 1)
        err = VE_STACK_EMPTY;
    else {
        vio_val *v = ctx->stack[--ctx->sp];
        uint32_t i = 0;
        v->vv = (vio_val **)malloc(len * sizeof(vio_val *));
        if (v->vv == NULL)
            err = VE_ALLOC_FAIL;
        else for (; i < len; ++i) {
            if ((err = pop_expect(ctx, v->vv + i, -1)))
                break;
        }
        push(ctx, v);
        *idx = i;
    }
    return err;
}

#define HANDLE_VEC_ERRORS \
    error: \
    if (v->s) \
        free(v->s); \
    if (v->vv) \
        free(v->vv); \
    while (i) { \
        --ctx->sp; \
        --i; \
    } \
    if (err != VE_STACK_OVERFLOW) \
        --ctx->sp; \
    if (v) \
        free(v); \
    return err;

vio_err_t vio_push_tag(vio_ctx *ctx, uint32_t nlen, char *name, uint32_t vlen) {
    uint32_t i = 0;
    PUSH(vv_tagword)
    v->len = nlen;
    v->s = (char *)malloc(nlen);
    if (v->s == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    strncpy(v->s, name, nlen);

    if (vlen > 0 || (err = pop_into_vec(ctx, vlen, &i)))
        goto error;
    return 0;

    HANDLE_VEC_ERRORS
}

vio_err_t vio_push_vec(vio_ctx *ctx, uint32_t len) {
    uint32_t i = 0;
    PUSH(vv_vec)
    v->vlen = len;
    if ((err = pop_into_vec(ctx, len, &i)))
        goto error;
    return 0;

    HANDLE_VEC_ERRORS
}

vio_err_t vio_push_mat(vio_ctx *ctx, uint32_t rows, uint32_t cols) {
    uint32_t i = 0, len = rows * cols;
    PUSH(vv_mat)
    v->rows = rows;
    v->cols = cols;
    if ((err = pop_into_vec(ctx, len, &i)))
        goto error;
    return 0;

    HANDLE_VEC_ERRORS
}
