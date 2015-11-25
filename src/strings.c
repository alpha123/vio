#include "divsufsort.h"
#include "mpc.h"
#include "strings.h"

vio_err_t vio_strcat(vio_ctx *ctx) {
    vio_err_t err = 0;
    char *a, *b, *cat = NULL;
    uint32_t alen, blen;
    vio_val *v;

    VIO__CHECK(vio_pop_str(ctx, &alen, &a));
    VIO__CHECK(vio_pop_str(ctx, &blen, &b));

    VIO__ERRIF((cat = (char *)malloc(alen + blen)) == NULL, VE_ALLOC_FAIL);
    memcpy(cat, b, blen);
    memcpy(cat + blen, a, alen);
    VIO__CHECK(vio_val_new(ctx, &v, vv_str));
    v->len = alen + blen;
    v->s = cat;
    ctx->stack[ctx->sp++] = v;
    return 0;

    error:
    if (cat) free(cat);
    return err;
}

vio_err_t vio_strsplit(vio_ctx *ctx) {
    return 0;
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
