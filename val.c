#include "val.h"

vio_err_t vio_val_new(vio_ctx *ctx, vio_val **out, vio_val_t type) {
    vio_val *v = (vio_val *)malloc(sizeof(vio_val));
    if (v == NULL)
        return VE_ALLOC_FAIL;
    v->what = type;
    v->mark = 0;
    v->next = ctx->ohead;
    ctx->ohead = v;
    ++ctx->ocnt;

    /* set pointers to null, mostly for ease of error handling */
    v->s = NULL;
    v->vv = NULL; /* also sets vf32 to NULL */

    /* can't actually trigger a gc here in case we're in an
       opcode and accidentally gc an object that was just popped
       but still being operated on. */
   
    *out = v;
    return 0;
}

void vio_val_free(vio_val *v) {
    switch (v->what) {
    case vv_tagword:
        free(v->vv);
        /* FALLTHROUGH (clear tagname) */
    case vv_str:
        free(v->s);
        break;
    case vv_num:
        mpf_clear(v->n);
        break;
    case vv_vecf:
    case vv_matf:
        free(v->vf32);
        break;
    case vv_vec:
    case vv_mat:
        free(v->vv);
        break;
    }
    free(v);
}

void vio_mark_val(vio_val *v) {
    if (v->mark)
        return;
    
    v->mark = 1;
    uint32_t i, l;
    switch (v->what) {
    case vv_vec:
    case vv_tagword:
        for (i = 0; i < v->vlen; ++i)
            vio_mark_val(v->vv[i]);
        break;
    case vv_mat:
        l = v->rows * v->cols;
        for (i = 0; i < l; ++i)
            vio_mark_val(v->vv[i]);
        break;
    }
}
