#include <gmp.h>
#include "art.h"
#include "mpc_private.h"
#include "mpc.h"
#include "tpl.h"
#include "serialize.h"

vio_err_t vio_dump_top(vio_ctx *ctx, FILE *fp) {
    if (ctx->sp == 0)
        return VE_STACK_EMPTY;
    return vio_dump_val(ctx->stack[ctx->sp-1], fp);
}

#define SAVE(fmt, ...) do{ \
    tn = tpl_map((fmt), __VA_ARGS__); \
    tpl_pack(tn, 0); \
    VIO__ERRIF(tpl_dump(tn, TPL_FD, fd) != 0, VE_IO_FAIL); \
    tpl_free(tn); }while(0)

vio_err_t vio_dump_parser(mpc_parser_t *p, FILE *fp) {
    vio_err_t err = 0;
    int fd = fileno(fp);
    tpl_node *tn;
    SAVE("csc", &p->retained, &p->name, &p->type);
    switch (p->type) {
    case MPC_TYPE_EXPECT:
        SAVE("s", &p->data.expect.m);
        vio_dump_parser(p->data.expect.x, fp);
        break;
    case MPC_TYPE_SINGLE:
        SAVE("c", &p->data.single.x);
        break;
    case MPC_TYPE_RANGE:
        SAVE("cc", &p->data.range.x, &p->data.range.y);
        break;
    case MPC_TYPE_STRING:
    case MPC_TYPE_ONEOF:
    case MPC_TYPE_NONEOF:
        SAVE("s", &p->data.string.x);
        break;
    case MPC_TYPE_MAYBE: /* these use the same structure internally */
    case MPC_TYPE_NOT:
        vio_dump_parser(p->data.not.x, fp);
        break;
    case MPC_TYPE_MANY:
    case MPC_TYPE_MANY1:
    case MPC_TYPE_COUNT:
        SAVE("i", &p->data.repeat.n);
        VIO__CHECK(vio_dump_parser(p->data.repeat.x, fp));
        break;
    case MPC_TYPE_OR:
        SAVE("i", &p->data.or.n);
        for (uint32_t i = 0; i < p->data.or.n; ++i)
            VIO__CHECK(vio_dump_parser(p->data.or.xs[i], fp));
        break;
    case MPC_TYPE_AND:
        SAVE("i", &p->data.and.n);
        for (uint32_t i = 0; i < p->data.and.n; ++i)
            VIO__CHECK(vio_dump_parser(p->data.and.xs[i], fp));
        break;
    }
    return 0;

    error:
    return err;
}

#define SAVEV(fmt, ...) SAVE("u" fmt, &v->what, __VA_ARGS__)

/* tpl kinda chokes on 0-length fixed arrays */
#define SAVESTR(v, other, ...) do{ \
    if (v->len > 0) \
        SAVEV("uc#" other, &v->len, v->s, v->len, __VA_ARGS__); \
    else \
        SAVEV("u" other, &v->len, __VA_ARGS__); }while(0)

vio_err_t vio_dump_val(vio_val *v, FILE *fp) {
    vio_err_t err = 0;
    tpl_node *tn;
    char *ns;
    mp_exp_t ex;
    int fd = fileno(fp);
    switch (v->what) {
    case vv_int: SAVEV("I", &v->i32); break;
    case vv_float: SAVEV("f", &v->f32); break;
    case vv_num:
        ns = mpf_get_str(NULL, &ex, 10, 0, v->n);
        SAVEV("is", &ex, &ns);
        break;
    case vv_quot:
        SAVEV("uu", &v->def_idx, &v->jmp);
        break;
    case vv_parser:
        SAVESTR(v, "", NULL);
        vio_dump_parser(v->p, fp);
        break;
    case vv_str:
        SAVESTR(v, "", NULL);
        break;
    case vv_tagword: 
        SAVESTR(v, "u", v->vlen);
        for (uint32_t i = 0; i < v->vlen; ++i)
            VIO__CHECK(vio_dump_val(v->vv[i], fp));
        break;
    case vv_vecf:
        SAVEV("uf#", &v->vlen, v->vf32, v->vlen);
        break;
    case vv_matf:
        SAVEV("uuf#", &v->rows, &v->cols, v->vf32, v->rows * v->cols);
        break;
    case vv_vec:
        SAVEV("u", &v->vlen);
        for (uint32_t i = 0; i < v->vlen; ++i)
            VIO__CHECK(vio_dump_val(v->vv[i], fp));
        break;
    case vv_mat:
        SAVEV("uu", &v->rows, &v->cols);
        for (uint32_t i = 0; i < v->rows * v->cols; ++i)
            VIO__CHECK(vio_dump_val(v->vv[i], fp));
        break;
    }

    return 0;

    error:
    return err;
}

vio_err_t vio_load_top(vio_ctx *ctx, FILE *fp) {
    vio_err_t err = 0;
    vio_val *v;
    if ((err = vio_load_val(&v, fp)))
        return err;
    if (ctx->sp == VIO_STACK_SIZE)
        return VE_STACK_OVERFLOW;
    ctx->stack[ctx->sp++] = v;
    return 0;
}

vio_err_t vio_load_val(vio_val **v, FILE *fp) {
    return 0;
}

vio_err_t vio_dump_bytecode(vio_bytecode *bc, FILE *fp) {
    vio_err_t err = 0;
    int fd = fileno(fp);
    tpl_node *tn;
    SAVE("u", &bc->ic);
    for (uint32_t i = 0; i < bc->ic; ++i)
        vio_dump_val(bc->consts[i], fp);
    SAVE("uU#", &bc->ip, bc->prog, bc->ip);
    return 0;

    error:
    return err;
}

vio_err_t vio_load_bytecode(vio_bytecode **bc, FILE *fp) {
    return 0;
}

struct savedata {
    FILE *fp;
    vio_ctx *ctx;
};

int save_def(void *data, const unsigned char *key, uint32_t klen, void *val) {
    vio_err_t err = 0;
    struct savedata *sd = (struct savedata *)data;
    vio_ctx *ctx = sd->ctx;
    int fd = fileno(sd->fp);
    uint32_t def_idx = (uint32_t)val - 1;
    tpl_node *tn;
    vio_bytecode *bc = ctx->defs[def_idx];
    SAVE("u", &def_idx);
    VIO__CHECK(vio_dump_bytecode(bc, sd->fp));
    return 0;
    error:
    return err;
}

vio_err_t vio_dump_dict(vio_ctx *ctx, FILE *fp) {
    vio_err_t err = 0;
    int fd = fileno(fp);
    tpl_node *tn;

    uint64_t dict_sz = art_size(&ctx->dict->words);
    SAVE("U", &dict_sz);
    struct savedata sd = { .fp = fp, .ctx = ctx };
    if ((err = (vio_err_t)art_iter(&ctx->dict->words, save_def, &sd)))
        goto error;

    return 0;
    error:
    return err;
}

vio_err_t vio_load_dict(vio_ctx *ctx, FILE *fp) {
    return 0;
}

vio_err_t vio_dump(vio_ctx *ctx, FILE *fp) {
    vio_err_t err = 0;
    VIO__CHECK(vio_dump_dict(ctx, fp));
    for (uint32_t i = 0; i < ctx->sp; ++i)
        VIO__CHECK(vio_dump_val(ctx->stack[i], fp));
    return 0;
    error:
    return err;
}

vio_err_t vio_load(vio_ctx *ctx, FILE *fp) {
    return 0;
}
