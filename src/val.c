#include <float.h>
#include <math.h>
#include "mpc_private.h"
#include "parsercombinators.h"
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
    v->fresh = 0; /* vm sets this to 1 if it needs to */
    v->mark = 0;
    v->next = ctx->ohead;
    ctx->ohead = v;
    ++ctx->ocnt;

    /* set pointers to null, mostly for ease of error handling */
    v->len = 0;
    v->s = NULL;

    v->vlen = 0;
    v->rows = 0;
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
        if (v->vv)
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
    case vv_parser:
        free(v->s);
        if (v->p) {
            mpc_undefine(v->p);
            mpc_delete(v->p);
        }
        break;
    case vv_quot:
        vio_bytecode_free(v->bc);
        break;
    default: break; /* nothing else needs freeing */
    }
    free(v);
}

vio_err_t vio_tagword_v(vio_ctx *ctx, vio_val **out, uint32_t nlen, const char *name, uint32_t vlen, va_list ap) {
    vio_err_t err = 0;
    *out = NULL;

    VIO__CHECK(vio_val_new(ctx, out, vv_tagword));

    (*out)->len = nlen;
    (*out)->s = (char *)malloc(nlen);
    VIO__ERRIF((*out)->s == NULL, VE_ALLOC_FAIL);
    strncpy((*out)->s, name, nlen);

    (*out)->vlen = vlen;
    (*out)->vv = (vio_val **)malloc(sizeof(vio_val *) * vlen);
    for (uint32_t i = 0; i < vlen; ++i)
        (*out)->vv[i] = va_arg(ap, vio_val *);

    return 0;

    error:
    if (*out) vio_val_free(*out);
    return err;
}

vio_err_t vio_tagword(vio_ctx *ctx, vio_val **out, uint32_t nlen, const char *name, uint32_t vlen, ...) {
    vio_err_t err = 0;
    va_list ap;
    va_start(ap, vlen);
    err = vio_tagword_v(ctx, out, nlen, name, vlen, ap);
    va_end(ap);
    return err;
}

vio_err_t vio_tagword0(vio_ctx *ctx, vio_val **out, const char *name, uint32_t vlen, ...) {
    vio_err_t err = 0;
    va_list ap;
    va_start(ap, vlen);
    err = vio_tagword_v(ctx, out, strlen(name), name, vlen, ap);
    va_end(ap);
    return err;
}

/* Create a vector with vlen elements.
   Stops at the first NULL vararg or at vlen args. */
vio_err_t vio_vec(vio_ctx *ctx, vio_val **out, uint32_t vlen, ...) {
    vio_err_t err = 0;
    vio_val *v;
    va_list ap;
    va_start(ap, vlen);

    VIO__CHECK(vio_val_new(ctx, out, vv_vec));
    (*out)->vlen = vlen;
    (*out)->vv = (vio_val **)malloc(sizeof(vio_val *) * vlen);
    for (uint32_t i = 0; i < vlen; ++i) {
        v = va_arg(ap, vio_val *);
        if (v == NULL) break;
        (*out)->vv[i] = v;
    }

    va_end(ap);
    return 0;

    error:
    va_end(ap);
    return err;
}

vio_err_t vio_string(vio_ctx *ctx, vio_val **out, uint32_t len, const char *str) {
    vio_err_t err = 0;

    VIO__CHECK(vio_val_new(ctx, out, vv_str));
    (*out)->len = len;
    (*out)->s = (char *)malloc(len);
    VIO__ERRIF((*out)->s == NULL, VE_ALLOC_FAIL);
    memcpy((*out)->s, str, len);

    error:
    return err;
}

vio_err_t vio_string0(vio_ctx *ctx, vio_val **out, const char *str) {
    return vio_string(ctx, out, strlen(str), str);
}

VIO_CONST
vio_val *vio_mat_idx(vio_val *mat, uint32_t row, uint32_t col) {
    return mat->vv[row * mat->cols + col];
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

/* BAD; strongly coupled, pretty much relies on knowing
   how the mpc_pc_* functions work. */
vio_err_t clone_parser_data(mpc_val_t *v, mpc_val_t **out) {
    struct vio_mpc_val *a = (struct vio_mpc_val *)v, *b;
    b = vio_mpc_val_new(a->ctx, a->v, a->nlen, a->name);
    if (b == NULL) return VE_ALLOC_FAIL;
    *out = b;
    return 0;
}

/* ALMOST CERTAINLY DIES ON RECURSIVE PARSERS */
vio_err_t clone_parser(mpc_parser_t *p, mpc_parser_t **out) {
    vio_err_t err = 0;
    mpc_parser_t *q = (mpc_parser_t *)malloc(sizeof(mpc_parser_t));
    VIO__ERRIF(q == NULL, VE_ALLOC_FAIL);
    memcpy(q, p, sizeof(mpc_parser_t));
    /* memcpy got most of the stuff, now just copy all the pointers */
    if (p->name) {
        q->name = (char *)malloc(strlen(p->name));
        VIO__ERRIF(q->name == NULL, VE_ALLOC_FAIL);
        strcpy(q->name, p->name);
    }
    switch (p->type) {
    case MPC_TYPE_APPLY_TO:
        q->data.apply_to.f = p->data.apply_to.f;
        VIO__CHECK(clone_parser_data(p->data.apply_to.d, &q->data.apply_to.d));
        VIO__CHECK(clone_parser(p->data.apply_to.x, &q->data.apply_to.x));
        break;
    case MPC_TYPE_EXPECT:
        q->data.expect.m = (char *)malloc(strlen(p->data.expect.m));
        VIO__ERRIF(q->data.expect.m == NULL, VE_ALLOC_FAIL);
        strcpy(q->data.expect.m, p->data.expect.m);
        VIO__CHECK(clone_parser(p->data.expect.x, &q->data.expect.x));
        break;
    case MPC_TYPE_STRING:
        q->data.string.x = (char *)malloc(strlen(p->data.string.x));
        VIO__ERRIF(q->data.string.x == NULL, VE_ALLOC_FAIL);
        strcpy(q->data.string.x, p->data.string.x);
        break;
    case MPC_TYPE_NOT:
    case MPC_TYPE_MAYBE:
        VIO__CHECK(clone_parser(p->data.not.x, &q->data.not.x));
        break;
    case MPC_TYPE_MANY:
    case MPC_TYPE_MANY1:
    case MPC_TYPE_COUNT:
        VIO__CHECK(clone_parser(p->data.repeat.x, &q->data.repeat.x));
        break;
    case MPC_TYPE_AND:
        q->data.and.dxs = (mpc_dtor_t *)malloc(sizeof(mpc_dtor_t) * p->data.and.n);
        memcpy(q->data.and.dxs, p->data.and.dxs, sizeof(mpc_dtor_t) * p->data.and.n);
        q->data.and.xs = (mpc_parser_t **)malloc(sizeof(mpc_parser_t) * p->data.and.n);
        VIO__ERRIF(q->data.and.xs == NULL, VE_ALLOC_FAIL);
        for (uint32_t i = 0; i < p->data.and.n; ++i)
            VIO__CHECK(clone_parser(p->data.and.xs[i], q->data.and.xs + i));
        break;
    case MPC_TYPE_OR:
        q->data.or.xs = (mpc_parser_t **)malloc(sizeof(mpc_parser_t) * p->data.or.n);
        for (uint32_t i = 0; i < p->data.or.n; ++i)
            VIO__CHECK(clone_parser(p->data.or.xs[i], q->data.or.xs + i));
        break;
    }
    *out = q;
    return 0;

    error:
    if (q->name) free(q->name);
    if (q) free(q);
    /* Do I hear something leaking? */
    return err;
}

vio_err_t vio_val_clone(vio_ctx *ctx, vio_val *v, vio_val **out) {
    vio_err_t err = 0;

    VIO__CHECK(vio_val_new(ctx, out, v->what));
    switch (v->what) {
    case vv_int: (*out)->i32 = v->i32; break;
    case vv_float: (*out)->f32 = v->f32; break;
    case vv_num: mpf_init_set((*out)->n, v->n); break;
    case vv_tagword:
        (*out)->vv = (vio_val **)malloc(sizeof(vio_val *) * v->vlen);
        for (uint32_t i = 0; i < v->vlen; ++i)
            VIO__CHECK(vio_val_clone(ctx, v->vv[i], (*out)->vv + i));
        /* FALLTHROUGH -- copy name */
    case vv_str:
        (*out)->len = v->len;
        (*out)->s = (char *)malloc(v->len);
        VIO__ERRIF((*out)->s == NULL, VE_ALLOC_FAIL);
        strncpy((*out)->s, v->s, v->len);
        break;
    /* Quotations and parsers are immutable, but copy
       them so that they aren't tied to the lifetime
       of the original. */
    case vv_quot:
        VIO__CHECK(vio_bytecode_clone(ctx, v->bc, &(*out)->bc));
        break;
    case vv_parser:
        VIO__CHECK(clone_parser(v->p, &(*out)->p));
        break;
    case vv_vecf:
        (*out)->vf32 = (vio_float *)malloc(sizeof(vio_float) * v->vlen);
        memcpy((*out)->vf32, v->vf32, sizeof(vio_float) * v->vlen);
        break;
    case vv_matf:
        (*out)->vf32 = (vio_float *)malloc(sizeof(vio_float) * v->rows * v->cols);
        memcpy((*out)->vf32, v->vf32, sizeof(vio_float) * v->rows * v->cols);
        break;
    case vv_vec:
        (*out)->vv = (vio_val **)malloc(sizeof(vio_val *) * v->vlen);
        for (uint32_t i = 0; i < v->vlen; ++i)
            VIO__CHECK(vio_val_clone(ctx, v->vv[i], (*out)->vv + i));
        break;
    case vv_mat:
        (*out)->vv = (vio_val **)malloc(sizeof(vio_val *) * v->rows * v->cols);
        for (uint32_t i = 0; i < v->rows * v->cols; ++i)
            VIO__CHECK(vio_val_clone(ctx, v->vv[i], (*out)->vv + i));
        break;
    }
    return 0;

    error:
    if (*out) free(*out);
    return err;
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
        default: *to = NULL; /* these are only here to shut the compiler up */
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
    default: *to = NULL;
    }
    return err;
}
