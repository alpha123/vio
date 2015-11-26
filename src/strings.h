#ifndef VIO_STRINGS_H
#define VIO_STRINGS_H

#include "context.h"
#include "error.h"

vio_err_t vio_strcat(vio_ctx *ctx);

vio_err_t vio_strsplit(vio_ctx *ctx);

/* various edit distance functions (levenshtein and sift4) */
vio_err_t vio_edit_dist(vio_ctx *ctx);
vio_err_t vio_approx_edit_dist(vio_ctx *ctx);

/* sorted suffix array */
vio_err_t vio_sorted_sufarray(vio_ctx *ctx);

/* longest common substring */
vio_err_t vio_lcss(vio_ctx *ctx);

/* burrows-wheeler transform */
vio_err_t vio_bwtransform(vio_ctx *ctx);

#endif
