#include "math.h"
#include "strings.h"
#include "stdrules.h"
#include "stdvio.h"

/* using _ instead of ._ can make complicated
   match patterns look much cleaner */
vio_err_t anyscore(vio_ctx *ctx) {
    return vio_push_tag(ctx, 1, "_", 0);
}

vio_err_t vio_drop(vio_ctx *ctx) {
    if (ctx->sp == 0)
        return vio_raise_empty_stack(ctx, "drop", 1);
    --ctx->sp;
    return 0;
}

void vio_load_stdlib(vio_ctx *ctx) {
    vio_load_stdrules(ctx);

    vio_register(ctx, "_", anyscore, 0);
    vio_register(ctx, "++", vio_strcat, 2);
    vio_register(ctx, "drop", vio_drop, 0);

    vio_register(ctx, "edit-dist", vio_edit_dist, 2);
    vio_register(ctx, "str-sim", vio_approx_edit_dist, 2);

#define REGISTER(f) vio_register(ctx, #f, vio_##f, 1);
    LIST_MATH_UNARY(REGISTER)
#undef REGISTER
}
