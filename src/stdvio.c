#include <stdio.h>
#include "cmp.h"
#include "eval.h"
#include "math.h"
#include "strings.h"
#include "vecapply.h"
#include "stdrules.h"
#include "stdvio.h"

vio_err_t vio_drop(vio_ctx *ctx) {
    if (ctx->sp == 0)
        return vio_raise_empty_stack(ctx, "drop", 1);
    --ctx->sp;
    return 0;
}

static const char *vio_builtins[] = {
    /*"=: cmp [ 0: .t ; _: .f ]",
    "<: cmp [ = not ] [ &abs bi@ = not ] bi and",
    "<=: &= &< bi or",
    ">: <= not",
    ">=: < not",*/
    
    "bi: ^keep eval",
    "bi*: ^dip eval",
    "bi@: dup bi*",
    "bi2: ^keep2 eval",
    "bi2*: ^dip2 eval",
    "bi2@: dup bi2*",

    "dip2: swap ^dip",
    "dup2: over over",
    "keep2: ^dup2 dip2",

    "over: ^dup swap",

    "preserve: keep swap",
    "preserve2: keep2 rot rot",
    0
};

void vio_load_stdlib(vio_ctx *ctx) {
    vio_load_stdrules(ctx);

    /* -N = non-scalar arity N */
    vio_register(ctx, "++", vio_strcat, 2);
    vio_register(ctx, "cmp", vio_compare, 2);
    vio_register(ctx, "vcmp", vio_compare, -2); /* register again with arity -2 to compare vectors directly */
    vio_register(ctx, "drop", vio_drop, 0);

    vio_register(ctx, "fold", vio_fold, -3);
    vio_register(ctx, "partition", vio_partition, -2);

    vio_register(ctx, "edit-dist", vio_edit_dist, 2);
    vio_register(ctx, "str-sim", vio_approx_edit_dist, 2);

#define REGISTER(f) vio_register(ctx, #f, vio_##f, 1);
    LIST_MATH_UNARY(REGISTER)
#undef REGISTER

    for (uint32_t i = 0; vio_builtins[i]; ++i) {
        if (vio_eval(ctx, -1, vio_builtins[i])) {
            fprintf(stderr, "error loading vio stdlib: cannot continue: %s", vio_err_msg(ctx->err));
            exit(1);
        }
    }
}
