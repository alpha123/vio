#include "mpc.h"
#include "val.h"
#include "parsercombinators.h"
#include "stdrules.h"

void vio_load_stdrules(vio_ctx *ctx) {
    vio_register(ctx, ",", vio_pc_then, 2);
    vio_register(ctx, ",|", vio_pc_or, 2);
    vio_register(ctx, ",?", vio_pc_maybe, 1);
    vio_register(ctx, ",+", vio_pc_more, 1);
    vio_register(ctx, ",*", vio_pc_many, 1);
    
    vio_register(ctx, "d", vio_rule_digit, 0);
    vio_register(ctx, "s", vio_rule_space, 0);
    vio_register(ctx, "any", vio_rule_any, 0);

    vio_register(ctx, "range", vio_rule_range, 2);
    vio_register(ctx, "one-of", vio_rule_oneof, 1);
    vio_register(ctx, "none-of", vio_rule_noneof, 1);
}

#define PROMOTE(parser) mpc_apply_to((parser), vio_mpc_apply_lift, ctx)

#define defrule(fname, wname, parser) \
    vio_err_t vio_rule_##fname(vio_ctx *ctx) { \
        return vio_push_parser(ctx, strlen((wname)), (wname), \
                   PROMOTE(parser)); \
    }

defrule(digit, "d", mpc_digit())
defrule(space, "s", mpc_whitespace())
defrule(any, "any", mpc_any())

vio_err_t vio_rule_range(vio_ctx *ctx) {
    vio_err_t err = 0;
    uint32_t len;
    char *from, *to;
    VIO__CHECK(vio_pop_str(ctx, &len, &to));
    VIO__CHECK(vio_pop_str(ctx, &len, &from));
    VIO__CHECK(vio_push_parser(ctx, 5, "range", PROMOTE(mpc_range(*from, *to))));

    return 0;
    error:
    return err;
}

vio_err_t pop_cstr(vio_ctx *ctx, char **out) {
    vio_err_t err = 0;
    uint32_t len;
    char *s, *chars = NULL;
    VIO__CHECK(vio_pop_str(ctx, &len, &s));
    VIO__ERRIF((chars = (char *)malloc(len+1)) == NULL, VE_ALLOC_FAIL);
    memcpy(chars, s, len);
    chars[len] = '\0';
    *out = chars;
    return 0;
    error:
    *out = NULL;
    return err;
}

vio_err_t vio_rule_oneof(vio_ctx *ctx) {
    vio_err_t err = 0;
    char *chars = NULL;
    VIO__CHECK(pop_cstr(ctx, &chars));
    VIO__CHECK(vio_push_parser(ctx, 6, "one-of", PROMOTE(mpc_oneof(chars))));
    free(chars);
    return 0;
    error:
    if (chars) free(chars);
    return err;
}

vio_err_t vio_rule_noneof(vio_ctx *ctx) {
    vio_err_t err = 0;
    char *chars = NULL;
    VIO__CHECK(pop_cstr(ctx, &chars));
    VIO__CHECK(vio_push_parser(ctx, 7, "none-of", PROMOTE(mpc_noneof(chars))));
    free(chars);
    return 0;
    error:
    if (chars) free(chars);
    return err;
}
