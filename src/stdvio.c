#include <stdio.h>
#include "eval.h"
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

static const char *vio_builtins[] = {
    "bi: &keep dip eval",
    "bi*: &dip dip eval",
    "bi@: dup bi*",
    "bi2: &keep2 dip eval",
    "bi2*: &dip2 dip eval",
    "bi2@: dup bi2*",

    "dip2: swap &dip dip",
    "dup2: over over",
    "keep2: &dup2 dip dip2",

    "over: &dup dip swap",

    "preserve: keep swap",
    "preserve2: keep2 rot rot",
    0
};

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

    for (uint32_t i = 0; vio_builtins[i]; ++i) {
        if (vio_eval(ctx, -1, vio_builtins[i]))
            fprintf(stderr, "error loading vio stdlib: cannot continue: %s", vio_err_msg(ctx->err));
    }
}
