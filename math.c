#include <math.h>
#include <gmp.h>
#include <blas/cblas.h>
#include "attrib.h"
#include "math.h"

#define ENSURE_ATLEAST(n) \
    if (ctx->sp < n) { \
        err = vio_raise(ctx, VE_STACK_EMPTY, "Function requires at least " \
                        #n " operands, stack only has %d items.", ctx->sp); \
        goto error; \
    }

#define CLEANUP \
    return err; \
    error: \
    if (ctx->err == 0) \
    	strcpy(ctx->err_msg, "Unexpected math."); \
    ctx->err = err; \
    return err;

#define CHECK(expr) \
    if ((err = (expr))) goto error;

VIO_CONST
uint32_t vmaxui(uint32_t a, uint32_t b) {
    return a > b ? a : b;
}

#define OP_IMPL(name) \
    vio_err_t generic_##name(vio_ctx *ctx, vio_val *a, vio_val *b) { \
        vio_err_t err = 0; \
        vio_val *va, *vb; \
        if (a->what == vv_str || b->what == vv_str || a->what == vv_quot || b->what == vv_quot || \
            a->what == vv_tagword || b->what == vv_tagword) { \
            err = vio_raise(ctx, VE_WRONG_TYPE, \
                            "Function '" #name "' expects numeric types or vectors; received '%s' and '%s' operands.", \
                            vio_val_type_name(a->what), vio_val_type_name(b->what));\
            goto error; \
        }

#define END_OP(_name) \
    CLEANUP \
}

#define GIVEN_VECF \
    if (a->what == vv_vecf && b->what == vv_vecf && a->vlen == b->vlen) {

#define DONE_VECF }

#define GIVEN_NUMERIC \
    else if (vio_is_numeric(a) && vio_is_numeric(b)) { \
        CHECK(vio_coerce(ctx, b, &vb, a->what)) \
        if (vb == NULL) { \
            CHECK(vio_coerce(ctx, a, &va, b->what)) \
            if (va == NULL) { \
                err = VE_NUMERIC_CONVERSION_FAIL; \
                goto error; \
            } \
            a = b; \
            b = va; \
        } \
        else \
            b = vb; \
        switch (a->what) {

#define DONE_NUMERIC \
        default: err = VE_WRONG_TYPE; goto error; \
        } \
    }

#define STANDARD_NUMERIC(op, fop) \
    GIVEN_NUMERIC \
        case vv_int: CHECK(vio_push_int(ctx, b->i32 op a->i32)) break; \
        case vv_float: CHECK(vio_push_float(ctx, b->f32 op a->f32)) break; \
        case vv_num: { mpf_t c; mpf_init(c); mpf_##fop(c, b->n, a->n); CHECK(vio_push_num(ctx, c)) break; } \
    DONE_NUMERIC

#define GIVEN_VEC \
    else CHECK(vio_coerce(ctx, a, &va, vv_vec)) \
    else CHECK(vio_coerce(ctx, b, &vb, vv_vec)) \
    else if (va && vb) { \

#define DONE_VEC }

#define AUTO_VECTORIZE(name) \
    GIVEN_VEC \
        uint32_t maxl = vmaxui(va->vlen, vb->vlen); \
        for (uint32_t i = 0; i < maxl; ++i) \
            CHECK(generic_##name(ctx, va->vv[i % va->vlen], vb->vv[i % vb->vlen])) \
        CHECK(vio_push_vec(ctx, maxl)) \
    DONE_VEC

OP_IMPL(add)
    GIVEN_VECF
        cblas_daxpy(b->vlen, 1, a->vf32, 1, b->vf32, 1);
        CHECK(vio_push_vecf32(ctx, b->vlen, b->vf32))
    DONE_VECF
    STANDARD_NUMERIC(+, add)
    AUTO_VECTORIZE(add)
END_OP(add)

OP_IMPL(sub)
    GIVEN_VECF
        cblas_daxpy(b->vlen, -1.0, a->vf32, 1, b->vf32, 1);
        CHECK(vio_push_vecf32(ctx, b->vlen, b->vf32))
    DONE_VECF
    STANDARD_NUMERIC(-, sub)
    AUTO_VECTORIZE(sub)
END_OP(sub)

vio_err_t generic_mul(vio_ctx *ctx, vio_val *a, vio_val *b) {
  vio_err_t err = 0;
  vio_val *va, *vb;
  if (a->what == vv_str || b->what == vv_str || a->what == vv_quot ||
      b->what == vv_quot || a->what == vv_tagword || b->what == vv_tagword) {
    err = vio_raise(ctx, VE_WRONG_TYPE, "Function '" "mul" "' expects numeric types or vectors; received '%s' and '%s' operands.",
        vio_val_type_name(a->what), vio_val_type_name(b->what));
    goto error;
  }
  if (b->what == vv_vecf && a->what == vv_float) {
    va = a;
    a = b;
    b = va;
  }
  if (a->what == vv_vecf && b->what == vv_float) {
    cblas_dscal(a->vlen, b->f32, a->vf32, 1);
    if ((err = (vio_push_vecf32(ctx, a->vlen, a->vf32))))
      goto error;
  } else if (a->what == vv_vecf && b->what == vv_vecf && a->vlen == b->vlen) {
    for (uint32_t i = 0; i < a->vlen; ++i)
      a->vf32[i] *= b->vf32[i];
    if ((err = (vio_push_vecf32(ctx, a->vlen, a->vf32))))
      goto error;
  } else if (vio_is_numeric_type((a)->what) && vio_is_numeric_type((b)->what)) {
    if ((err = (vio_coerce(ctx, b, &vb, a->what))))
      goto error;
    if (vb == NULL) {
      if ((err = (vio_coerce(ctx, a, &va, b->what))))
        goto error;
      if (va == NULL) {
        err = VE_NUMERIC_CONVERSION_FAIL;
        goto error;
      }
      a = b;
      b = va;
    } else
      b = vb;
    switch (a->what) {
    case vv_int:
      if ((err = (vio_push_int(ctx, b->i32 * a->i32))))
        goto error;
      break;
    case vv_float:
      if ((err = (vio_push_float(ctx, b->f32 * a->f32))))
        goto error;
      break;
    case vv_num: {
      mpf_t c;
      __gmpf_init(c);
      __gmpf_mul(c, b->n, a->n);
      if ((err = (vio_push_num(ctx, c))))
        goto error;
      break;
    }
    default:
      err = VE_WRONG_TYPE;
      goto error;
    }
  }

  else if ((err = (vio_coerce(ctx, a, &va, vv_vec))))
    goto error;
  else if ((err = (vio_coerce(ctx, b, &vb, vv_vec))))
    goto error;
  else if (va && vb) {
    uint32_t maxl = vmaxui(va->vlen, vb->vlen);
    for (uint32_t i = 0; i < maxl; ++i)
      if ((err =
               (generic_mul(ctx, va->vv[i % va->vlen], vb->vv[i % vb->vlen]))))
        goto error;
    if ((err = (vio_push_vec(ctx, maxl))))
      goto error;
  }
  return err;
error:
  if (ctx->err == 0)
    strcpy(ctx->err_msg, "Unexpected math.");
  ctx->err = err;
  return err;
}

/*OP_IMPL(mul)
    if (b->what == vv_vecf && a->what == vv_float) {
        va = a;
        a = b;
        b = va;
    }
    if (a->what == vv_vecf && b->what == vv_float) {
        cblas_dscal(a->vlen, b->f32, a->vf32, 1);
        CHECK(vio_push_vecf32(ctx, a->vlen, a->vf32))
    }
    else GIVEN_VECF
        for (uint32_t i = 0; i < a->vlen; ++i)
            a->vf32[i] *= b->vf32[i];
        CHECK(vio_push_vecf32(ctx, a->vlen, a->vf32))
    DONE_VECF
    STANDARD_NUMERIC(*, mul)
    AUTO_VECTORIZE(mul)
    DONE_VEC
END_OP(mul)*/

OP_IMPL(div)
    if (b->what == vv_vecf && a->what == vv_float) {
        va = a;
        a = b;
        b = va;
    }
    if (a->what == vv_vecf && b->what == vv_float) {
        cblas_dscal(a->vlen, 1.0 / b->f32, a->vf32, 1);
        CHECK(vio_push_vecf32(ctx, a->vlen, a->vf32))
    }
    else GIVEN_VECF
        for (uint32_t i = 0; i < a->vlen; ++i)
            a->vf32[i] /= b->vf32[i];
        CHECK(vio_push_vecf32(ctx, a->vlen, a->vf32))
    DONE_VECF
    STANDARD_NUMERIC(/, div)
    AUTO_VECTORIZE(div)
END_OP(div)

#define BIN_OP(name) \
    vio_err_t vio_##name(vio_ctx *ctx) { \
        vio_err_t err = 0; \
        ENSURE_ATLEAST(2) \
\
        vio_val *a = ctx->stack[--ctx->sp], *b; \
        b = ctx->stack[--ctx->sp]; \
        err = generic_##name(ctx, a, b); \
\
        CLEANUP \
    }

BIN_OP(add)
BIN_OP(sub)
BIN_OP(mul)
BIN_OP(div)
