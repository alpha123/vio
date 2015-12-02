#include "alloca.h"
#include <math.h>
#include "divsufsort.h"
#include "mpc.h"
#include "strings.h"

/* 5 is chosen rather arbitrarily to avoid moving on every
   loop. Larger values will be faster but less accurate.
   Generally increasing it would only be useful if you are
   comparing long strings with many sequences of equal characters. */
#define VIO_SIFT4_OFF_SHIFT 5

/* See http://siderite.blogspot.com/2014/11/super-fast-and-accurate-string-distance.html

   Don't use really large values for maxoff.

   If maxdist > -1, stop once we hit maxdist differences. */
uint32_t vio_sift4_dist(uint32_t alen, const char *a, uint32_t blen, const char *b, int maxoff, int maxdist) {
    if (alen == 0) return blen;
    if (blen == 0) return alen;

    uint32_t ia = 0, ib = 0, lcss = 0, local_cs = 0, trans_cnt = 0,
             olen = 0,
             *off_a = (uint32_t *)alloca(maxoff),
             *off_b = (uint32_t *)alloca(maxoff);
    uint8_t *off_test = (uint8_t *)alloca(maxoff);

    while (ia < alen && ib < blen) {
        if (a[ia] == b[ib]) {
            ++local_cs;
            /* Check for transpositions. Not fully accurate, as it will only look
               at a section of the string at most `maxoff` long. */
            uint8_t transp = 0;
            for (uint32_t i = 0; i < olen;) {
                uint32_t oa = off_a[i], ob = off_b[i];
                uint8_t test = off_test[i];
                if (ia < oa && ib < ob) {
                    transp = abs(ib - ia) >= abs(ob - oa);
                    if (transp)
                        ++trans_cnt;
                    else if (!test) {
                        off_test[i] = 1;
                        ++trans_cnt;
                    }
                    break;
                }
                if (ia > ob && ib > oa) {
                    memmove(off_a + i, off_a + i + 1, 1);
                    memmove(off_b + i, off_b + i + 1, 1);
                    memmove(off_test + i, off_test + i + 1, 1);
                    --olen;
                }
                else
                    ++i;
            }
            if (olen == maxoff) {
                memmove(off_a, off_a + VIO_SIFT4_OFF_SHIFT, VIO_SIFT4_OFF_SHIFT);
                memmove(off_b, off_b + VIO_SIFT4_OFF_SHIFT, VIO_SIFT4_OFF_SHIFT);
                memmove(off_test, off_test + VIO_SIFT4_OFF_SHIFT, VIO_SIFT4_OFF_SHIFT);
                olen -= VIO_SIFT4_OFF_SHIFT;
            }
            off_a[olen] = ia;
            off_b[olen] = ib;
            off_test[olen++] = transp;
        }
        else {
            lcss += local_cs;
            local_cs = 0;
            if (ia > ib) ia = ib;
            else ib = ia;
            for (uint32_t i = 0; i < maxoff && (ia + i < alen || ib + i < blen); ++i) {
                if (ia + i < alen && a[ia + i] == b[ib]) {
                    ia += i - 1;
                    --ib;
                    break;
                }
                else if (ib + i < blen && b[ib + i] == a[ia]) {
                    ib += i - 1;
                    --ia;
                    break;
                }
            }
        }
        ++ia;
        ++ib;
        if (maxdist > -1) {
            uint32_t check = (ia > ib ? ia : ib) - lcss + trans_cnt;
            if (check >= maxdist)
                return check;
        }
        if (ia >= alen || ib >= blen) {
            lcss += local_cs;
            local_cs = 0;
            if (ia > ib) ia = ib;
            else ib = ia;
        }
    }
    lcss += local_cs;
    return (alen > blen ? alen : blen) - lcss + trans_cnt;
}

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

vio_err_t vio_approx_edit_dist(vio_ctx *ctx) {
    vio_err_t err = 0;
    uint32_t alen, blen;
    char *a, *b;
    VIO__CHECK(vio_pop_str(ctx, &alen, &a));
    VIO__CHECK(vio_pop_str(ctx, &blen, &b));
    VIO__CHECK(vio_push_int(ctx, vio_sift4_dist(alen, a, blen, b, 10, -1)));

    return 0;
    error:
    return err;
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
