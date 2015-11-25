#ifndef VIO_VALUE_H
#define VIO_VALUE_H

#include <stdio.h>
#include <stdarg.h>
#include <gmp.h>
#include "mpc.h"
#include "attrib.h"
#include "types.h"
#include "context.h"

#define LIST_VAL_TYPES(_X, X, X_) \
    _X(str) \
    X(int) X(float) X(num) \
    X(tagword) \
    X(vecf) X(vec) \
    X(matf) X(mat) \
    X(parser) \
    X_(quot) 

typedef enum {
#define _DEF_ENUM(t) vv_##t = 1,
#define DEF_ENUM(t)  vv_##t,
#define DEF_ENUM_(t) vv_##t

    LIST_VAL_TYPES(_DEF_ENUM, DEF_ENUM, DEF_ENUM_)

#undef _DEF_ENUM
#undef DEF_ENUM
#undef DEF_ENUM_
} vio_val_t;

VIO_CONST
const char *vio_val_type_name(vio_val_t what);

struct _vval {
    vio_val_t what;

    char *s;
    uint32_t len;

    union {
        vio_float f32;
        vio_int i32;
        mpf_t n;

        /* matrices are stored row-major */
        vio_float *vf32;
        /* also used for tagword values */
        vio_val **vv;

        mpc_parser_t *p;

        /* a quotation's executable instructions */
        vio_bytecode *bc;
    };

    uint32_t rows;
    union {
        uint32_t cols;
        uint32_t vlen;
    };

    /* vm uses it for marking new vectors/tagwords */
    int fresh;

    /* for gc */
    int mark;
    vio_val *next;
};

vio_err_t vio_val_new(vio_ctx *ctx, vio_val **out, vio_val_t type);
void vio_val_free(vio_val *v);
void vio_mark_val(vio_val *v);

vio_err_t vio_tagword(vio_ctx *ctx, vio_val **out, uint32_t nlen, const char *name, uint32_t vlen, ...);
vio_err_t vio_tagword0(vio_ctx *ctx, vio_val **out, const char *name, uint32_t vlen, ...);

vio_err_t vio_vec(vio_ctx *ctx, vio_val **out, uint32_t vlen, ...);

vio_err_t vio_string(vio_ctx *ctx, vio_val **out, uint32_t len, const char *str);
vio_err_t vio_string0(vio_ctx *ctx, vio_val **out, const char *str);

vio_err_t vio_coerce(vio_ctx *ctx, vio_val *from, vio_val **to, vio_val_t what);

vio_err_t vio_val_clone(vio_ctx *ctx, vio_val *v, vio_val **out);

VIO_CONST
int vio_is_numeric_type(vio_val_t what);

#define vio_is_numeric(v) vio_is_numeric_type((v)->what)

#endif
