#ifndef VIO_VALUE_H
#define VIO_VALUE_H

#include <stdio.h>
#include <gmp.h>
#include "context.h"

typedef enum {
    vv_str = 1,
    vv_int, vv_float, vv_num,
    vv_tagword,
    vv_vecf, vv_vec,
    vv_matf, vv_mat
} vio_val_t;

typedef struct _vval {
    vio_val_t what;

    char *s;
    uint32_t len;

    union {
        float f32;
        int32_t i32;
        mpf_t n;

	/* matrices are stored row-major */
	float *vf32;
	/* also used for tagword values */
	struct _vval **vv;
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
    struct _vval *next;
} vio_val;

vio_err_t vio_val_new(struct _vctx *ctx, vio_val **out, vio_val_t type);
void vio_val_free(vio_val *v);
void vio_mark_val(vio_val *v);

#endif
