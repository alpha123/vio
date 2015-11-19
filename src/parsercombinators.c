#include "mpc.h"
#include "parsercombinators.h"

vio_err_t vio_pc_parse(vio_ctx *ctx) {
    vio_err_t err = 0;

    char *s, *ns = NULL, *ss = NULL;
    uint32_t len;
    mpc_parser_t *p;
    VIO__CHECK(vio_pop_parser(ctx, &len, &s, &p)); /* ignore name, we're not interested in it */
    VIO__CHECK(vio_pop_str(ctx, &len, &s));

    /* mpc only accepts null-terminated strings */
    VIO__ERRIF((ns = (char *)malloc(len + 1)) == NULL, VE_ALLOC_FAIL);
    strncpy(ns, s, len);
    ns[len] = '\0';

    if (len > 12) {
        ss = (char *)malloc(16);
        strncpy(ss, s, 12);
        strncpy(ss + 12, "...", 3);
        ss[15] = '\0';
    }
    else {
        ss = (char *)malloc(len + 1);
        strncpy(ss, s, len);
        ss[len] = '\0';
    }

    mpc_print(p);
    putchar('\n');

    mpc_result_t res;
    if (mpc_parse(ss, ns, p, &res))
        printf("%s\n", (char *)res.output);
    else {
        mpc_err_print(res.error);
        mpc_err_delete(res.error);
    }
    free(ns);
    free(ss);
    return 0;

    error:
    if (ns) free(ns);
    if (ss) free(ss);
    return err;
}

vio_err_t vio_pc_str(vio_ctx *ctx) {
    vio_err_t err = 0;

    uint32_t len;
    char *raw_s, *s;
    mpc_parser_t *p;
    VIO__CHECK(vio_pop_str(ctx, &len, &raw_s));

    /* mpc_string expects a null-terminated argument */
    VIO__ERRIF((s= (char *)malloc(len + 1)) == NULL, VE_ALLOC_FAIL);

    strncpy(s, raw_s, len);
    s[len] = '\0';
    if (len == 1)
        p = mpc_char(s[0]);
    else
        p = mpc_string(s);

    VIO__CHECK(vio_push_parser(ctx, len, s, p));
    return 0;

    error:
    if (s) free(s);
    return err;
}

#define BIN_COMBINATOR(name, combine) \
    vio_err_t vio_pc_##name(vio_ctx *ctx) { \
        vio_err_t err = 0; \
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

BIN_COMBINATOR(then, mpc_and(2, mpcf_strfold, a, b))
BIN_COMBINATOR(or, mpc_or(2, a, b))

UN_COMBINATOR(not, mpc_not(a, free))
UN_COMBINATOR(maybe, mpc_maybe_lift(a, mpcf_ctor_str))
UN_COMBINATOR(many, mpc_many(mpcf_strfold, a))
UN_COMBINATOR(more, mpc_many1(mpcf_strfold, a))
