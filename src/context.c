#include <stdio.h>
#include <string.h>
#include "context.h"

vio_err_t vio_open(vio_ctx *ctx) {
    vio_err_t err = 0;
    ctx->ohead = NULL;
    ctx->ocnt = 0;
    ctx->sp = 0;
    ctx->defp = 0;
    VIO__ERRIF((ctx->defs = (vio_bytecode **)malloc(
        sizeof(vio_bytecode *) * VIO_MAX_FUNCTIONS)) == NULL, VE_ALLOC_FAIL);
    VIO__ERRIF((ctx->dict = (vio_dict *)malloc(sizeof(vio_dict))) == NULL, VE_ALLOC_FAIL);
    vio_dict_open(ctx->dict);
    return 0;

    error:
    if (ctx->defs != NULL)
        free(ctx->defs);
    if (ctx->dict != NULL) {
        vio_dict_close(ctx->dict);
        free(ctx->dict);
    }
    return err;
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
    vio_dict_close(ctx->dict);
    for (uint32_t i = 0; i < ctx->defp; ++i)
        vio_bytecode_free(ctx->defs[i]);
    free(ctx->defs);
}

vio_err_t vio_raise(vio_ctx *ctx, vio_err_t err, const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vsnprintf(ctx->err_msg, VIO_MAX_ERR_LEN, msg, args);
    va_end(args);

    ctx->err = err;
    return err;
}

int vio_what(vio_ctx *ctx) {
    if (ctx->sp == 0)
        return 0;
    return ctx->stack[ctx->sp - 1]->what;
}

/* An expected type of -1 will do no type checking.
   If both the expected type and actual type are numeric,
   this function will attempt to convert if possible.
   This conversion only occurs between the float, int,
   and num types. */
vio_err_t top_expect(vio_ctx *ctx, vio_val **v, int expect) {
    vio_err_t err = 0;
    
    if (ctx->sp == 0)
        err = VE_STACK_EMPTY;
    else {
        vio_val *top = ctx->stack[ctx->sp-1];
        if (vio_is_numeric_type(expect) && vio_is_numeric(top))
            err = vio_coerce(ctx, top, v, expect);
        else if (expect > 0 && top->what != expect)
            err = VE_STRICT_TYPE_FAIL;
        else
            *v = top;
    }
    return err;
}

vio_err_t pop_expect(vio_ctx *ctx, vio_val **v, int expect) {
    vio_err_t err = 0;
    if ((err = top_expect(ctx, v, expect)))
        return err;
    --ctx->sp;
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

#define TOP(type) \
   vio_val *v; \
   vio_err_t err; \
   if ((err = top_expect(ctx, &v, type))) \
       goto error;

#define PT_STR(name, which) \
    vio_err_t vio_ ## name ## _str(vio_ctx *ctx, uint32_t *len, char **out) { \
        which(vv_str) \
\
        *len = v->len; \
        *out = v->s; \
        return 0; \
\
        error: \
        *len = 0; \
        *out = NULL; \
        return err; \
    }

#define PT_INT(name, which) \
    vio_err_t vio_ ## name ## _int(vio_ctx *ctx, vio_int *out) { \
        which(vv_int) \
        *out = v->i32; \
        return 0; \
\
        error: \
        *out = 0; \
        return err; \
    }

#define PT_FLOAT(name, which) \
    vio_err_t vio_ ## name ## _float(vio_ctx *ctx, vio_float *out) { \
        which(vv_float) \
        *out = v->f32; \
        return 0; \
\
        error: \
        *out = 0.0f; \
        return err; \
    }

#define PT_NUM(name, which) \
    vio_err_t vio_ ## name ## _num(vio_ctx *ctx, mpf_t *out) { \
        which(vv_num) \
        mpf_set(*out, v->n); \
        return 0; \
\
        error: \
        return err; \
    }

#define PT_VECF(name, which) \
    vio_err_t vio_ ## name ## _vecf32(vio_ctx *ctx, uint32_t *len, vio_float **out) { \
        *out = NULL; \
        which(vv_vecf) \
        *len = v->vlen; \
        *out = v->vf32; \
        return 0; \
\
        error: \
        *len = 0; \
        *out = NULL; \
        return err; \
    }

#define PT_MATF(name, which) \
    vio_err_t vio_ ## name ## _matf32(vio_ctx *ctx, uint32_t *rows, uint32_t *cols, vio_float **out) { \
        *out = NULL; \
        which(vv_matf); \
        *rows = v->rows; \
        *cols = v->cols; \
        *out = v->vf32; \
        return 0; \
\
        error: \
        *rows = 0; \
        *cols = 0; \
        *out = NULL; \
        return err; \
    }

#define PT_PARSER(name, which) \
    vio_err_t vio_ ## name ## _parser(vio_ctx *ctx, uint32_t *nlen, char **name, mpc_parser_t **out) { \
        *out = NULL; \
        which(vv_parser) \
        *nlen = v->len; \
        *name = v->s; \
        *out = v->p; \
        return 0; \
\
        error: \
        *nlen = 0; \
        *name = NULL; \
        *out = NULL; \
        return err; \
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

#define TYPES(X) \
    X(PT_STR) \
    X(PT_INT) \
    X(PT_FLOAT) \
    X(PT_NUM) \
    X(PT_VECF) \
    X(PT_MATF) \
    X(PT_PARSER) \

#define DEF_BOTH(typ) \
   typ(pop, POP) \
   typ(top, TOP) \

TYPES(DEF_BOTH)

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

vio_err_t vio_push_int(vio_ctx *ctx, vio_int val) {
    PUSH(vv_int)
    v->i32 = val;
    return 0;
    HANDLE_ERRORS
}

vio_err_t vio_push_float(vio_ctx *ctx, vio_float val) {
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

vio_err_t vio_push_vecf32(vio_ctx *ctx, uint32_t len, vio_float *val) {
    PUSH(vv_vecf)
    v->vlen = len;
    v->vf32 = (vio_float *)malloc(len * sizeof(vio_float));
    if (v->vf32 == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    memcpy(v->vf32, val, len * sizeof(vio_float));
    return 0;

    /* we'll never have a state where vf32 has been allocated
       successfully and we still have an error
       thus we don't need to free it here */
    HANDLE_ERRORS
}

vio_err_t vio_push_matf32(vio_ctx *ctx, uint32_t rows, uint32_t cols, vio_float *val) {
    uint32_t len = rows * cols;
    PUSH(vv_matf)
    v->rows = rows;
    v->cols = cols;
    v->vf32 = (vio_float *)malloc(len * sizeof(vio_float));
    if (v->vf32 == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    memcpy(v->vf32, val, len * sizeof(vio_float));
    return 0;

    HANDLE_ERRORS
}

vio_err_t vio_push_parser(vio_ctx *ctx, uint32_t nlen, char *name, mpc_parser_t *val) {
    PUSH(vv_parser)
    v->len = nlen;
    v->s = (char *)malloc(nlen);
    if (v->s == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    strncpy(v->s, name, nlen);
    v->p = val;
    return 0;

    HANDLE_ERRORS
}

#define PUSH_CNT(push) \
    vio_err_t err; \
    for (uint32_t i = 0; i < cnt; ++i) { \
        if ((err = push)) \
            return err; \
    } \
    return 0;

vio_err_t vio_push_str_cnt(vio_ctx *ctx, uint32_t cnt, uint32_t *lens, char **vals) {
    PUSH_CNT(vio_push_str(ctx, lens[i], vals[i]))
}

vio_err_t vio_push_int_cnt(vio_ctx *ctx, uint32_t cnt, vio_int *vals) {
    PUSH_CNT(vio_push_int(ctx, vals[i]))
}

vio_err_t vio_push_float_cnt(vio_ctx *ctx, uint32_t cnt, vio_float *vals) {
    PUSH_CNT(vio_push_float(ctx, vals[i]))
}

vio_err_t vio_push_num_cnt(vio_ctx *ctx, uint32_t cnt, const mpf_t *vals) {
    PUSH_CNT(vio_push_num(ctx, vals[i]))
}

vio_err_t pop_into_vec(vio_ctx *ctx, uint32_t len, uint32_t *idx) {
    vio_err_t err = 0;
    if (ctx->sp < len + 1)
        err = VE_STACK_EMPTY;
    else {
        vio_val *v = ctx->stack[--ctx->sp];
        uint32_t i = len;
        v->vv = (vio_val **)malloc(len * sizeof(vio_val *));
        if (v->vv == NULL)
            err = VE_ALLOC_FAIL;
        else while (i) {
            if ((err = pop_expect(ctx, v->vv + --i, -1)))
                break;
        }
        if (err == 0)
            push(ctx, v);
        *idx = len - i;
    }
    return err;
}

/* gc will take care of the vio_val (if it actually gets allocated),
   but we should try to restore the stack to what it was like before
   the error to maximize our chances of recovery */
#define HANDLE_VEC_ERRORS \
    error: \
    while (i--)  \
        --ctx->sp; \
    if (err != VE_STACK_OVERFLOW) \
        --ctx->sp; \
    if (v) \
        vio_val_free(v); \
    return err;

vio_err_t vio_push_tag(vio_ctx *ctx, uint32_t nlen, char *name, uint32_t vlen) {
    /* i must exist so we can properly clean up in case of an error
       (see HANDLE_VEC_ERRORS) */
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
    VIO__CHECK(pop_into_vec(ctx, len, &i));
    return 0;

    HANDLE_VEC_ERRORS
}

vio_err_t vio_push_mat(vio_ctx *ctx, uint32_t rows, uint32_t cols) {
    uint32_t i = 0, len = rows * cols;
    PUSH(vv_mat)
    v->rows = rows;
    v->cols = cols;
    VIO__CHECK(pop_into_vec(ctx, len, &i));
    return 0;

    HANDLE_VEC_ERRORS
}
