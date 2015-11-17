#ifndef VIO_VALUE_H
#define VIO_VALUE_H

#include <stdio.h>
#include <gmp.h>
#include "attrib.h"
#include "types.h"
#include "context.h"

#define LIST_VAL_TYPES(_X, X, X_) \
    _X(str) \
    X(int) X(float) X(num) \
    X(tagword) \
    X(vecf) X(vec) \
    X(matf) X(mat) \
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

	/* index in `vio_ctx#defs` of a pointer to instructions
           where a quotation's code can be found (at `jmp` offset) */
	uint32_t def_idx;
    };

    uint32_t rows;
    union {
        uint32_t cols;
        uint32_t vlen;

	/* location of where to jump to for executing quotations */
	uint32_t jmp;
    };

    /* for gc */
    int mark;
    vio_val *next;
};

vio_err_t vio_val_new(vio_ctx *ctx, vio_val **out, vio_val_t type);
void vio_val_free(vio_val *v);
void vio_mark_val(vio_val *v);

vio_err_t vio_coerce(vio_ctx *ctx, vio_val *from, vio_val **to, vio_val_t what);

VIO_CONST
int vio_is_numeric_type(vio_val_t what);

#define vio_is_numeric(v) vio_is_numeric_type((v)->what)

#endif
