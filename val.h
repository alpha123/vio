#ifndef VIO_VALUE_H
#define VIO_VALUE_H

#include <stdio.h>
#include <gmp.h>
#include "attrib.h"
#include "types.h"
#include "context.h"

typedef enum {
    vv_str = 1,
    vv_int, vv_float, vv_num,
    vv_tagword,
    vv_vecf, vv_vec,
    vv_matf, vv_mat,
    vv_quot
} vio_val_t;

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

	/* location of where to jump to for executing quotations */
	uint32_t jmp;
    };

    uint32_t rows;
    /* used for matrix columns, vector size, and tagword value count --
       the union is purely for readability of matrix operations */
    union {
        uint32_t cols;
        uint32_t vlen;
    };

    /* for gc */
    int mark;
    vio_val *next;
};

vio_err_t vio_val_new(vio_ctx *ctx, vio_val **out, vio_val_t type);
void vio_val_free(vio_val *v);
void vio_mark_val(vio_val *v);

vio_err_t vio_coerce(vio_ctx *ctx, vio_val *from, vio_val **to, vio_val_t what);

/* Return the type of the value on top of the stack.
   Returns 0 if the stack is empty, because using vio_err_t
   would be clumpsy. However that may be subject to change. */

#define vio_what(ctx) ((vio_val_t)((ctx)->sp && (ctx)->stack[(ctx)->sp-1]->what))

VIO_CONST
int vio_is_numeric_type(vio_val_t what);

#define vio_is_numeric(v) vio_is_numeric_type((v)->what)

#endif
