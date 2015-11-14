#include <float.h>
#include <math.h>
#include "val.h"

VIO_CONST
int vio_is_numeric_type(vio_val_t what) {
    return what == vv_int || what == vv_float || what == vv_num;
}

VIO_CONST
const char *vio_val_type_name(vio_val_t what) {
#define NAME(t) case vv_##t: return #t;

    switch (what) {
    LIST_VAL_TYPES(NAME, NAME, NAME)
    default: return "unknown type";
    }

#undef NAME
}

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
    default: break; /* nothing else needs freeing */
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
    default: break; /* nothing else references anything */
    }
}

#define TRY_INIT(_to) !(err = vio_val_new(ctx, _to, what))

/* If the value to convert is already the target type, sets *to to from
   and does not allocate a new vio_val. Otherwise allocates (and can return VE_ALLOC_FAIL).
   If the types cannot be converted, *to is set to NULL (and no allocation is performed).
   0 (i.e. no error) is then returned.
   *Vio does not consider an impossible type coercion to be an error condition.*
   It is up to the caller to determine if an error should be raised. */
vio_err_t vio_coerce(vio_ctx *ctx, vio_val *from, vio_val **to, vio_val_t what) {
    vio_err_t err = 0;
    uint32_t i = 0;
    vio_val *as_f;
    *to = NULL;
    if (from->what == what)
        *to = from;
    else if (from->what == vv_vecf && what == vv_vec) {
        if (TRY_INIT(to)) {
            if (((*to)->vv = (vio_val **)malloc(from->vlen * sizeof(vio_val *))) == NULL)
                err = VE_ALLOC_FAIL;
            else {
                (*to)->vlen = from->vlen;
                for (; i < from->vlen; ++i) {
                    if ((err = vio_val_new(ctx, (*to)->vv + i, vv_float)))
                        break;
                    (*to)->vv[i]->f32 = from->vf32[i];
                }
                /* probably an alloc error in vio_val_new */
                if (i < from->vlen) {
                    vio_val_free(*to);
                    *to = NULL;
                }
            }
        }
    }
    else if (from->what == vv_vec && what == vv_vecf) {
        if (TRY_INIT(to)) {
            if (((*to)->vf32 = (vio_float *)malloc(from->vlen * sizeof(vio_float))) == NULL)
                err = VE_ALLOC_FAIL;
            else {
                (*to)->vlen = from->vlen;
                for (; i < from->vlen; ++i) {
                    if ((err = vio_coerce(ctx, from->vv[i], &as_f, vv_float)) || as_f == NULL)
                        break;
                    (*to)->vf32[i] = as_f->f32;
                    /* don't bother waiting for the next gc cycle to free this
                       since we know it isn't referenced by anything */
                    vio_val_free(as_f);
                }
                /* either there was an error or one of the values
                   could not convert to float */
                if (i < from->vlen) {
                    vio_val_free(*to);
                    *to = NULL;
                }
            }
        }
    }
    else if (what == vv_vecf) {
        if (!(err = vio_coerce(ctx, from, &as_f, vv_float)) && as_f != NULL && TRY_INIT(to)) {
            if (((*to)->vf32 = (vio_float *)malloc(sizeof(vio_float))) == NULL)
                err = VE_ALLOC_FAIL;
            else {
                (*to)->vlen = 1;
                (*to)->vf32[0] = as_f->f32;
                vio_val_free(as_f);
            }
        }
    }
    else if (what == vv_vec) {
        if (TRY_INIT(to)) {
            if (((*to)->vv = (vio_val **)malloc(sizeof(vio_val))) == NULL)
                err = VE_ALLOC_FAIL;
            else {
                (*to)->vlen = 1;
                (*to)->vv[0] = from;
            }
        }
    }
    else switch (from->what) {
    case vv_int: switch (what) {
        case vv_float:
            if (TRY_INIT(to))
                (*to)->f32 = (vio_float)from->i32;
            break;
        case vv_num:
            if (TRY_INIT(to))
                mpf_init_set_si((*to)->n, from->i32);
            break;
        default: *to = NULL;
    } break;
    case vv_float: switch (what) {
        case vv_int:
            if ((vio_int)from->f32 == from->f32 && TRY_INIT(to))
                (*to)->i32 = (vio_int)from->f32;
            break;
        case vv_num:
            if (TRY_INIT(to))
                mpf_init_set_d((*to)->n, from->f32);
            break;
        default: *to = NULL;
    } break;
    case vv_num: switch (what) {
        case vv_int:
            if (mpf_integer_p(from->n) && mpf_fits_slong_p(from->n) && TRY_INIT(to))
                (*to)->i32 = (vio_int)mpf_get_si(from->n);
            break;
        case vv_float:
            /* assume conversion to vio_float won't truncate (much) if the value would
               fit in an integer */
            if (mpf_fits_slong_p(from->n) && TRY_INIT(to))
                (*to)->f32 = (vio_float)mpf_get_d(from->n);
            break;
        default: *to = NULL;
    } break;
    }
    return err;
}
