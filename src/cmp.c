#include <string.h>
#include <gmp.h>
#include "cmp.h"

#define is_mat(v) ((v)->what == vv_mat || (v)->what == vv_matf)
#define is_vec(v) ((v)->what == vv_vec || (v)->what == vv_vecf)
#define is_tag(v) ((v)->what == vv_tagword)
#define is_scal(v) (!is_mat((v)) && !is_vec((v)) && !is_tag((v)))
#define is_num(v) ((v)->what == vv_num || (v)->what == vv_int || (v)->what == vv_float)
#define is_str(v) ((v)->what == vv_str)
#define is_pars(v) ((v)->what == vv_parser)
#define is_quot(v) ((v)->what == vv_quot)

int num_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    int cmp;
    vio_val *n, *m;
    vio_coerce(ctx, a, &n, vv_num);
    vio_coerce(ctx, b, &m, vv_num);
    cmp = mpf_cmp(n->n, m->n);
    return cmp;
}

int str_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    int cmp = strncmp(a->s, b->s, a->len > b->len ? b->len : a->len);
    return cmp == 0 ? a->len - b->len : cmp;
}

int parser_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    /* just compare by name */
    return str_cmp(ctx, a, b);
}

int quot_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    /* how do you order quotations?! */
    return 0;
}

int scalar_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    if (is_num(a) && !is_num(b))
        return -1;
    else if (is_num(b) && !is_num(a))
        return 1;
    else if (is_str(a) && !is_str(b))
        return -1;
    else if (is_str(b) && !is_str(a))
        return 1;
    else if (is_pars(a) && !is_pars(b))
        return -1;
    else if (is_pars(b) && !is_pars(a))
        return 1;

    if (is_num(a))
        return num_cmp(ctx, a, b);
    else if (is_str(a))
        return str_cmp(ctx, a, b);
    else if (is_pars(a))
        return parser_cmp(ctx, a, b);
    else if (is_quot(a))
        return quot_cmp(ctx, a, b);
    
    return 0;
}

int vector_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    int cmp;
    uint32_t mlen = a->vlen > b->vlen ? b->vlen : a->vlen;
    for (uint32_t i = 0; i < mlen; ++i) {
        cmp = vio_cmp_val(ctx, a->vv[i], b->vv[i]);
        if (cmp)
            return cmp;
    }
    return a->vlen - b->vlen;
}

int tagword_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    int cmp = strncmp(a->s, b->s, a->len > b->len ? b->len : a->len);
    return cmp == 0 ? (cmp = a->len - b->len) == 0 ? vector_cmp(ctx, a, b) : cmp : cmp;
}


int matrix_cmp(vio_ctx *ctx, vio_val *a, vio_val *b) {
    int cmp;
    uint32_t alen = a->rows * a->cols, blen = b->rows * b->cols, mlen;
    mlen = alen > blen ? blen : alen;

    for (uint32_t i = 0; i < mlen; ++i) {
        cmp = vio_cmp_val(ctx, a->vv[i], b->vv[i]);
        if (cmp)
            return cmp;
    }
    cmp = alen - blen;
    return cmp == 0 ? (cmp = a->rows - b->rows) == 0 ? a->cols - b->cols : cmp : cmp;
}

int vio_cmp_val(vio_ctx *ctx, vio_val *a, vio_val *b) {
    /* sort order:
       - scalars
         + numbers
         + strings
         + parsers
         + quotations
       - tagwords
         + lexographic by name
           - element-wise comparison
             + vector length
       - vectors
         + element-wise comparison
           - length
       - matrices
         + element-wise comparison
           - rows * cols
             + rows
               - cols
       */

    if (is_scal(a) && !is_scal(b))
        return -1;
    else if (is_scal(b) && !is_scal(a))
        return 1;
    else if (is_tag(a) && !is_tag(b))
        return -1;
    else if (is_tag(b) && !is_tag(a))
        return 1;
    else if (is_vec(a) && is_mat(b))
        return -1;
    else if (is_vec(b) && is_mat(a))
        return 1;

    if (is_scal(a)) /* b will also be a scalar */
        return scalar_cmp(ctx, a, b);
    else if (is_tag(a))
        return tagword_cmp(ctx, a, b);
    else if (is_vec(a))
        return vector_cmp(ctx, a, b);
    else if (is_mat(a))
        return matrix_cmp(ctx, a, b);

    return 0;
}

int vio_cmp(vio_ctx *ctx) {
    if (ctx->sp < 2) return 0;
    return vio_cmp_val(ctx, ctx->stack[ctx->sp-1], ctx->stack[ctx->sp-2]);
}

vio_err_t vio_compare(vio_ctx *ctx) {
    int cmp = vio_cmp(ctx);
    ctx->sp -= ctx->sp < 2 ? ctx->sp : 2;
    return vio_push_int(ctx, cmp);
}
