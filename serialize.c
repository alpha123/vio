#include <gmp.h>
#include "serialize.h"

vio_err_t vio_dump(FILE *fp, vio_ctx *ctx) {
    if (ctx->sp == 0)
        return VE_STACK_EMPTY;
    return vio_dump_val(fp, ctx->stack[ctx->sp-1]);
}

#define TRY_WRITE(a, t, cnt) if (fwrite(a, sizeof(t), cnt, fp) != cnt) { err = VE_IO_FAIL; goto error; }

vio_err_t vio_dump_val(FILE *fp, vio_val *v) {
    vio_err_t err = 0;
    TRY_WRITE(&v->what, vio_val_t, 1)
    switch (v->what) {
    case vv_int: TRY_WRITE(&v->i32, vio_int, 1) break;
    case vv_float: TRY_WRITE(&v->f32, vio_float, 1) break;
    case vv_num: mpf_out_str(fp, 10, 0, v->n); break;
    case vv_quot:
        TRY_WRITE(&v->def_idx, uint32_t, 1)
        TRY_WRITE(&v->jmp, uint32_t, 1)
        break;
    case vv_parser: /* save parsers by name */
    case vv_str:
        TRY_WRITE(&v->len, uint32_t, 1)
        TRY_WRITE(v->s, char, v->len)
        break;
    case vv_tagword: 
        TRY_WRITE(&v->len, uint32_t, 1)
        TRY_WRITE(v->s, char, v->len)
        TRY_WRITE(&v->vlen, uint32_t, 1)
        for (uint32_t i = 0; i < v->vlen; ++i)
            if ((err = vio_dump_val(fp, v->vv[i]))) goto error;
        break;
    case vv_vecf:
        TRY_WRITE(&v->vlen, uint32_t, 1)
        TRY_WRITE(&v->vf32, vio_float, v->vlen)
        break;
    case vv_matf:
        TRY_WRITE(&v->rows, uint32_t, 1)
        TRY_WRITE(&v->cols, uint32_t, 1)
        TRY_WRITE(&v->vf32, vio_float, v->rows * v->cols)
        break;
    case vv_vec:
        TRY_WRITE(&v->vlen, uint32_t, 1)
        for (uint32_t i = 0; i < v->vlen; ++i)
            if ((err = vio_dump_val(fp, v->vv[i]))) goto error;
        break;
    case vv_mat:
        TRY_WRITE(&v->rows, uint32_t, 1)
        TRY_WRITE(&v->cols, uint32_t, 1)
        for (uint32_t i = 0; i < v->rows * v->cols; ++i)
            if ((err = vio_dump_val(fp, v->vv[i]))) goto error;
        break;
    }

    return 0;

    error:
    return err;
}

vio_err_t vio_load(FILE *fp, vio_ctx *ctx) {
    vio_err_t err = 0;
    vio_val *v;
    if ((err = vio_load_val(fp, &v)))
        return err;
    if (ctx->sp == VIO_STACK_SIZE)
        return VE_STACK_OVERFLOW;
    ctx->stack[ctx->sp++] = v;
    return 0;
}

vio_err_t vio_load_val(FILE *fp, vio_val **v) {
    return 0;
}

