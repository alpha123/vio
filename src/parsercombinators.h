#ifndef VIO_PARSERCOMBINATORS_H
#define VIO_PARSERCOMBINATORS_H

#include "mpc.h"
#include "error.h"
#include "context.h"
#include "val.h"

/* pass the vio context around to dtor and fold functions */
struct vio_mpc_val {
    vio_ctx *ctx;
    vio_val *v;
    uint32_t nlen;
    char *name;
};

struct vio_mpc_val *vio_mpc_val_new(vio_ctx *ctx, vio_val *v, uint32_t nlen, char *name);
void vio_mpc_val_free(struct vio_mpc_val *x);

/* expects a parser on top of the stack and
   a string immediately below it */
vio_err_t vio_pc_parse(vio_ctx *ctx);

/* Parser creation */
vio_err_t vio_pc_str(vio_ctx *ctx);
vio_err_t vio_pc_loadrule(vio_ctx *ctx, vio_val *v);

/* Binary combinators */
vio_err_t vio_pc_then(vio_ctx *ctx);
vio_err_t vio_pc_or(vio_ctx *ctx);

/* Unary combinators */
vio_err_t vio_pc_not(vio_ctx *ctx);
vio_err_t vio_pc_maybe(vio_ctx *ctx);
vio_err_t vio_pc_many(vio_ctx *ctx);
vio_err_t vio_pc_more(vio_ctx *ctx);

/* Generation of tagword ASTs */
mpc_val_t *vio_mpc_apply_lift(mpc_val_t *x, void *data);
mpc_val_t *vio_mpc_apply_maybe(mpc_val_t *x, void *data);
mpc_val_t *vio_mpc_fold(int n, mpc_val_t **xs);
void vio_mpc_dtor(mpc_val_t *v);

#endif
