#include "eval.h"
#include "val.h"
#include "parsercombinators.h"

struct vio_mpc_val *vio_mpc_val_new(vio_ctx *ctx, vio_val *v, uint32_t nlen, char *name) {
    struct vio_mpc_val *x = (struct vio_mpc_val *)malloc(sizeof(struct vio_mpc_val));
    if (x == NULL) return NULL;
    x->ctx = ctx;
    x->v = v;
    x->nlen = nlen;
    x->name = nlen == 0 ? NULL : (char *)malloc(nlen);
    if (x->name)
        memcpy(x->name, name, nlen);
    return x;
}

void vio_mpc_val_free(struct vio_mpc_val *x) {
    if (x->nlen > 0) free(x->name);
    free(x);
    /* let the gc take care of x->v. */
}

struct vio_mpc_apply_data {
    vio_ctx *ctx;
    char *name;
    uint32_t nlen;
};

struct vio_mpc_apply_data *vio_mpc_ad_name(vio_ctx *ctx, uint32_t nlen, char *name) {
    struct vio_mpc_apply_data *d = (struct vio_mpc_apply_data *)malloc(sizeof(struct vio_mpc_apply_data));
    d->ctx = ctx;
    d->nlen = nlen;
    d->name = (char *)malloc(nlen);
    memcpy(d->name, name, nlen);
    return d;
}

void vio_mpc_ad_free(struct vio_mpc_apply_data *data) {
    if (data->nlen > 0) free(data->name);
    free(data);
}

void vio_mpc_dtor(mpc_val_t *v) {
    vio_mpc_val_free((struct vio_mpc_val *)v);
}

mpc_val_t *vio_mpc_fold(int n, mpc_val_t **xs) {
    struct vio_mpc_val *x, *acc;
    vio_ctx *ctx = ((struct vio_mpc_val *)xs[0])->ctx;
    vio_val *v;
    vio_vec(ctx, &v, n, NULL);
    for (int i = 0; i < n; ++i) {
        x = (struct vio_mpc_val *)xs[i];
        if (x == NULL)
            continue;
        v->vv[i] = x->v;
        vio_mpc_val_free(x);
    }
    acc = vio_mpc_val_new(ctx, v, 0, NULL);
    return acc;
}

mpc_val_t *vio_mpc_apply_lift(mpc_val_t *x, void *data) {
    vio_ctx *ctx = (vio_ctx *)data;
    vio_val *v;
    vio_string0(ctx, &v, (char *)x);
    return vio_mpc_val_new(ctx, v, 0, NULL);
}

mpc_val_t *vio_mpc_apply_maybe(mpc_val_t *x, void *data) {
    vio_ctx *ctx = (vio_ctx *)data;
    vio_val *v;
    if (x == NULL) {
        vio_string(ctx, &v, 0, "");
        return vio_mpc_val_new(ctx, v, 0, NULL);
    }
    return x;
}

mpc_val_t *vio_mpc_apply_to(mpc_val_t *x, void *data_) {
    struct vio_mpc_apply_data *data = (struct vio_mpc_apply_data *)data_;
    vio_val *v;
    struct vio_mpc_val *y = vio_mpc_val_new(data->ctx, NULL, data->nlen, data->name),
                       *z = (struct vio_mpc_val *)x;
    if (z->v->what == vv_vec) {
        vio_tagword(data->ctx, &v, y->nlen, y->name, z->v->vlen, NULL);
        for (uint32_t i = 0; i < v->vlen; ++i)
            v->vv[i] = z->v->vv[i];
    }
    else
        vio_tagword(data->ctx, &v, y->nlen, y->name, 1, z->v);
    y->v = v;
    vio_mpc_val_free(z);
    vio_mpc_ad_free(data);
    return y;
}

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
        resv = ((struct vio_mpc_val *)res.output)->v;
        VIO__CHECK(vio_tagword0(ctx, out, "success", 1, resv));
    }
    else {
        VIO__CHECK(vio_val_new(ctx, &resv, vv_str));
        resv->s = mpc_err_string(res.error);
        resv->len = strlen(resv->s) - 1; /* strip trailing \n */

        VIO__CHECK(vio_tagword0(ctx, out, "fail", 1, resv));
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
             "but it got a '%s'.", vio_val_type_name(parser->what));
        VIO__RAISEIF(str->what != vv_str, VE_WRONG_TYPE,
             "Parse expects a string or vector of strings to parse, but it got a '%s'",
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
    VIO__ERRIF((s = (char *)malloc(len + 1)) == NULL, VE_ALLOC_FAIL);

    strncpy(s, raw_s, len);
    s[len] = '\0';
    if (len == 1)
        p = mpc_char(s[0]);
    else
        p = mpc_string(s);

    p = mpc_apply_to(p, vio_mpc_apply_lift, ctx);
    VIO__CHECK(vio_push_parser(ctx, 0, NULL, p));
    return 0;

    error:
    if (s) free(s);
    return err;
}

vio_err_t vio_pc_loadrule(vio_ctx *ctx, vio_val *v) {
    vio_err_t err = 0;
    uint32_t ignore_nlen;
    char *ignore_name, *nulls = (char *)malloc(v->len + 1);
    VIO__ERRIF(nulls == NULL, VE_ALLOC_FAIL);
    snprintf(nulls, v->len + 1, "%.*s", v->len, v->s);
    VIO__CHECK(vio_pop_parser(ctx, &ignore_nlen, &ignore_name, &v->p));
    mpc_parser_t *p = mpc_new(nulls);
    mpc_define(p, v->p);
    v->p = p;
    return 0;

    error:
    if (nulls) free(nulls);
    return err;
}

/* We need to re-init every time the parser is used
   because its apply_data gets freed afterwards. */
vio_err_t vio_pc_initrule(vio_ctx *ctx, vio_val *v, mpc_parser_t **out) {
    *out = mpc_apply_to(v->p, vio_mpc_apply_to, vio_mpc_ad_name(ctx, v->len, v->s));
    return 0;
}

#define BIN_COMBINATOR(name, combine) \
    vio_err_t vio_pc_##name(vio_ctx *ctx) { \
        vio_err_t err = 0; \
        mpc_parser_t *a, *b; \
        uint32_t anlen, bnlen; \
        char *aname, *bname; \
\
        VIO__CHECK(vio_pop_parser(ctx, &bnlen, &bname, &b)); \
        VIO__CHECK(vio_pop_parser(ctx, &anlen, &aname, &a)); \
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
        uint32_t nlen; \
        char *name; \
\
        VIO__CHECK(vio_pop_parser(ctx, &nlen, &name, &a)); \
        VIO__CHECK(vio_push_parser(ctx, 0, NULL, combine)); \
        return 0; \
\
        error: \
        return err; \
    }

BIN_COMBINATOR(then, mpc_and(2, vio_mpc_fold, a, b, vio_mpc_dtor))
BIN_COMBINATOR(or, mpc_or(2, a, b))

UN_COMBINATOR(not, mpc_not(a, vio_mpc_dtor))
UN_COMBINATOR(maybe, mpc_apply_to(mpc_maybe(a), vio_mpc_apply_maybe, ctx))
UN_COMBINATOR(many, mpc_many(vio_mpc_fold, a))
UN_COMBINATOR(more, mpc_many1(vio_mpc_fold, a))
