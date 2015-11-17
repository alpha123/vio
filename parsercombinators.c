#include "mpc.h"
#include "parsercombinators.h"

vio_err_t vio_pc_str(vio_ctx *ctx) {
    vio_err_t err = 0;
    VIO__ENSURE_ATLEAST(1)

    uint32_t len;
    char *s, *name;
    mpc_parser_t *p;
    VIO__CHECK(vio_pop_str(ctx, &len, &s));

    VIO__ERRIF((name = (char *)malloc(len)) == NULL, VE_ALLOC_FAIL);

    strncpy(name, s, len);
    if (len == 1)
        p = mpc_char(s[0]);
    else
        p = mpc_string(s);

    VIO__CHECK(vio_push_parser(ctx, len, name, p));
    return 0;

    error:
    if (name) free(name);
    return err;
}

#define BIN_COMBINATOR(name, combine) \
    vio_err_t vio_pc_##name(vio_ctx *ctx) { \
        vio_err_t err = 0; \
        VIO__ENSURE_ATLEAST(2) \
\
        mpc_parser_t *a, *b; \
        uint32_t name_len_ignore; \
        char *name_ignore; \
\
        VIO__CHECK(vio_pop_parser(ctx, &name_len_ignore, &name_ignore, &b)); \
        VIO__CHECK(vio_pop_parser(ctx, &name_len_ignore, &name_ignore, &a)); \
        VIO__CHECK(vio_push_parser(ctx, 0, NULL, combine)); \
        return 0; \
\
        error: \
        return err; \
    }

#define UN_COMBINATOR(name, combine) \
    vio_err_t vio_pc_##name(vio_ctx *ctx) { \
        vio_err_t err = 0; \
        VIO__ENSURE_ATLEAST(1) \
\
        mpc_parser_t *a; \
        uint32_t name_len_ignore; \
        char *name_ignore; \
\
        VIO__CHECK(vio_pop_parser(ctx, &name_len_ignore, &name_ignore, &a)); \
        VIO__CHECK(vio_push_parser(ctx, 0, NULL, combine)); \
        return 0; \
\
        error: \
        return err; \
    }

BIN_COMBINATOR(then, mpca_and(2, a, b))
BIN_COMBINATOR(or, mpca_or(2, a, b))

UN_COMBINATOR(not, mpca_not(a))
UN_COMBINATOR(maybe, mpca_maybe(a))
UN_COMBINATOR(many, mpca_many(a))
UN_COMBINATOR(more, mpca_many1(a))
