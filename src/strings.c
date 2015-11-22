#include "strings.h"

vio_err_t strcat_recur(vio_ctx *ctx, vio_val *a, vio_val *b, vio_val **out) {
    vio_err_t err = 0;

    if (a->what == vv_vec && b->what == vv_vec) {
        uint32_t len = a->vlen > b->vlen ? a->vlen : b->vlen;
        VIO__CHECK(vio_vec(ctx, out, len, NULL));
        for (uint32_t i = 0; i < len; ++i) {
            VIO__CHECK(strcat_recur(ctx, a->vv[i % a->vlen], b->vv[i % b->vlen],
                                    (*out)->vv + i));
        }
    }
    else if (b->what == vv_vec) {
        vio_val *tmp = b;
        b = a;
        a = tmp;
    }
    if (a->what == vv_vec && b->what != vv_vec) {
        VIO__CHECK(vio_vec(ctx, out, a->vlen, NULL));
        for (uint32_t i = 0; i < a->vlen; ++i)
            VIO__CHECK(strcat_recur(ctx, a->vv[i], b, (*out)->vv + i));
    }
    else {
        VIO__RAISEIF(a->what != vv_str || b->what != vv_str,
                     VE_WRONG_TYPE,
                     "++ expected both arguments to be strings, but it got "
                     "'%s' and '%s'.",
                     vio_val_type_name(a->what), vio_val_type_name(b->what));
        char *cat = (char *)malloc(a->len + b->len);
        memcpy(cat, b->s, b->len);
        memcpy(cat + b->len, a->s, a->len);
        VIO__CHECK(vio_val_new(ctx, out, vv_str));
        (*out)->len = a->len + b->len;
        (*out)->s = cat;
    }
    return 0;

    error:
    return err;
}

vio_err_t vio_strcat(vio_ctx *ctx) {
    vio_err_t err = 0;
    vio_val *a, *b, *v;
    VIO__RAISEIF(ctx->sp < 2, VE_STACK_EMPTY,
             "++ expects two strings, but the stack doesn't have enough values.");

    a = ctx->stack[--ctx->sp];
    b = ctx->stack[--ctx->sp];
    VIO__CHECK(strcat_recur(ctx, a, b, &v));
    ctx->stack[ctx->sp++] = v;

    error:
    return err;
}

vio_err_t vio_edit_dist(vio_ctx *ctx) {
    return 0;
}

vio_err_t vio_sorted_sufarray(vio_ctx *ctx) {
    return 0;
}

vio_err_t vio_lcss(vio_ctx *ctx) {
    return 0;
}

vio_err_t vio_bwtransform(vio_ctx *ctx) {
    return 0;
}
