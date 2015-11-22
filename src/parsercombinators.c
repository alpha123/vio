#include "mpc.h"
#include "parsercombinators.h"

vio_err_t pc_parse(vio_ctx *ctx, mpc_parser_t *p, uint32_t len, const char *s, vio_val **out) {
    vio_err_t err = 0;

    char *ns = NULL, *sfname = NULL;

    vio_val *resv;

    /* mpc only accepts null-terminated strings */
    VIO__ERRIF((ns = (char *)malloc(len + 1)) == NULL, VE_ALLOC_FAIL);
    strncpy(ns, s, len);
    ns[len] = '\0';

    /* create a pseudo-file name for mpc's error messages */
    if (len > 12) {
        sfname = (char *)malloc(16);
        strncpy(sfname, s, 12);
        strncpy(sfname + 12, "...", 3);
        sfname[15] = '\0';
    }
    else {
        sfname = (char *)malloc(len + 1);
        strncpy(sfname, s, len);
        sfname[len] = '\0';
    }

    mpc_result_t res;
    int success;
    if ((success = mpc_parse(sfname, ns, p, &res))) {
        VIO__CHECK(vio_val_new(ctx, &resv, vv_str));
        resv->s = (char *)res.output;
        resv->len = strlen(resv->s);

        VIO__CHECK(vio_tagword(ctx, out, "success", 1, resv));
    }
    else {
        VIO__CHECK(vio_val_new(ctx, &resv, vv_str));
        resv->s = mpc_err_string(res.error);
        resv->len = strlen(resv->s) - 1; /* strip trailing \n */

        VIO__CHECK(vio_tagword(ctx, out, "fail", 1, resv));
        mpc_err_delete(res.error);
    }
    free(ns);
    free(sfname);
    return 0;

    error:
    if (ns) free(ns);
    if (sfname) free(sfname);
    if (!success) mpc_err_delete(res.error);
    return err;
}

vio_err_t parse_recur(vio_ctx *ctx, vio_val *parser, vio_val *str, vio_val **out) {
    vio_err_t err = 0;

    if (parser->what == vv_vec && str->what == vv_vec) {
        /* TODO: Build a matrix of parsers X strings */
    }
    else if (parser->what == vv_vec) {
        VIO__CHECK(vio_vec(ctx, out, parser->vlen, NULL));
        for (uint32_t i = 0; i < parser->vlen; ++i)
            VIO__CHECK(parse_recur(ctx, parser->vv[i], str, (*out)->vv + i));
    }
    else if (str->what == vv_vec) {
        VIO__CHECK(vio_vec(ctx, out, str->vlen, NULL));
        for (uint32_t i = 0; i < str->vlen; ++i)
            VIO__CHECK(parse_recur(ctx, parser, str->vv[i], (*out)->vv + i));
    }
    else {
        VIO__RAISEIF(parser->what != vv_parser, VE_WRONG_TYPE,
             "Parse expects a parser or vector of parsers on top of the stack, "
             "but it found a %s.", vio_val_type_name(parser->what));
        VIO__RAISEIF(str->what != vv_str, VE_WRONG_TYPE,
             "Parse expects a string or strings to parse, but it got a %s",
             vio_val_type_name(parser->what));

        VIO__CHECK(pc_parse(ctx, parser->p, str->len, str->s, out));
    }

    return 0;
    error:
    if (*out) vio_val_free(*out);
    return err;
}

vio_err_t vio_pc_parse(vio_ctx *ctx) {
    vio_err_t err = 0;

    VIO__RAISEIF(ctx->sp < 2, VE_STACK_EMPTY,
         "Parse requires at least a parser and a string to parse, "
         "but the stack doesn't have enough values.");

    vio_val *par, *str, *out;
    par = ctx->stack[--ctx->sp];
    str = ctx->stack[--ctx->sp];

    VIO__CHECK(parse_recur(ctx, par, str, &out));
    ctx->stack[ctx->sp++] = out;

    return 0;
    error:
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
