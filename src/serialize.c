#include <math.h>
#include <gmp.h>
#include "art.h"
#include "json.h"
#include "mpc_private.h"
#include "mpc.h"
#include "tpl.h"
#include "uneval.h"
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

#define LOAD(fmt, ...) do{ \
    tn = tpl_map(fmt, __VA_ARGS__); \
    VIO__ERRIF(tpl_load(tn, TPL_FD, fd) != 0, VE_CORRUPT_IMAGE); \
    tpl_unpack(tn, 0); \
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

/* tpl kinda chokes on 0-length fixed arrays */
#define SAVESTR(v, other, ...) do{ \
    SAVE("u", &v->len); \
    if (v->len > 0) \
        SAVE("c#" other, v->s, v->len, __VA_ARGS__); \
    else \
        SAVE(other, __VA_ARGS__); }while(0)

#define LOADSTR(v, other, ...) do{ \
    LOAD("u", &v->len); \
    if (v->len > 0) { \
        v->s = (char *)malloc(v->len); \
        VIO__ERRIF(v->s == NULL, VE_ALLOC_FAIL); \
        LOAD("c#" other, v->s, v->len, __VA_ARGS__); \
    } \
    else \
        LOAD(other, __VA_ARGS__); }while(0)

vio_err_t vio_dump_val(vio_val *v, FILE *fp) {
    vio_err_t err = 0;
    tpl_node *tn;
    char *ns;
    mp_exp_t ex;
    int fd = fileno(fp);
    SAVE("u", &v->what);
    switch (v->what) {
    case vv_int: SAVE("I", &v->i32); break;
    case vv_float: SAVE("f", &v->f32); break;
    case vv_num:
        ns = mpf_get_str(NULL, &ex, 10, 0, v->n);
        SAVE("Us", &ex, &ns);
        free(ns);
        break;
    case vv_quot:
        SAVE("uu", &v->def_idx, &v->jmp);
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
        SAVE("u", &v->vlen);
        SAVE("f#", v->vf32, v->vlen);
        break;
    case vv_matf:
        SAVE("uu", &v->rows, &v->cols);
        SAVE("f#", v->vf32, v->rows * v->cols);
        break;
    case vv_vec:
        SAVE("u", &v->vlen);
        for (uint32_t i = 0; i < v->vlen; ++i)
            VIO__CHECK(vio_dump_val(v->vv[i], fp));
        break;
    case vv_mat:
        SAVE("uu", &v->rows, &v->cols);
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
    if ((err = vio_load_val(ctx, &v, fp)))
        return err;
    if (ctx->sp == VIO_STACK_SIZE)
        return VE_STACK_OVERFLOW;
    ctx->stack[ctx->sp++] = v;
    return 0;
}

vio_err_t vio_load_val(vio_ctx *ctx, vio_val **out, FILE *fp) {
    vio_err_t err = 0;
    tpl_node *tn;
    char *ns;
    mp_exp_t ex;
    int fd = fileno(fp);
    vio_val_t what;
    vio_val *v;
    LOAD("u", &what);
    VIO__CHECK(vio_val_new(ctx, &v, what));
    switch (what) {
    case vv_int: LOAD("I", &v->i32); break;
    case vv_float: LOAD("f", &v->f32); break;
    case vv_num:
        LOAD("Us", &ex, &ns);
        /* for some reason mpf_get_str and mpf_set_str use different formats */
        mpf_init_set_str(v->n, ns, 10);
        mpf_div_ui(v->n, v->n, (unsigned long)pow(10, ex));
        free(ns);
        break;
    case vv_quot:
        LOAD("uu", &v->def_idx, &v->jmp);
        break;
    case vv_parser:
        break;
    case vv_str:
        LOADSTR(v, "", NULL);
        break;
    case vv_tagword: 
        LOADSTR(v, "u", v->vlen);
        for (uint32_t i = 0; i < v->vlen; ++i)
            VIO__CHECK(vio_dump_val(v->vv[i], fp));
        break;
    case vv_vecf:
        LOAD("u", &v->vlen);
        v->vf32 = (vio_float *)malloc(sizeof(vio_float) * v->vlen);
        VIO__ERRIF(v->vf32 == NULL, VE_ALLOC_FAIL);
        LOAD("f#", v->vf32, v->vlen);
        break;
    case vv_matf:
        LOAD("uu", &v->rows, &v->cols);
        v->vf32 = (vio_float *)malloc(sizeof(vio_float) * v->rows * v->cols);
        VIO__ERRIF(v->vf32 == NULL, VE_ALLOC_FAIL);
        LOAD("f#", v->vf32, v->rows * v->cols);
        break;
    case vv_vec:
        LOAD("u", &v->vlen);
        v->vv = (vio_val **)malloc(sizeof(vio_val *) * v->vlen);
        VIO__ERRIF(v->vv == NULL, VE_ALLOC_FAIL);
        for (uint32_t i = 0; i < v->vlen; ++i)
            VIO__CHECK(vio_load_val(ctx, v->vv + i, fp));
        break;
    case vv_mat:
        LOAD("uu", &v->rows, &v->cols);
        v->vv = (vio_val **)malloc(sizeof(vio_val *) * v->rows * v->cols);
        VIO__ERRIF(v->vv == NULL, VE_ALLOC_FAIL);
        for (uint32_t i = 0; i < v->rows * v->cols; ++i)
            VIO__CHECK(vio_load_val(ctx, v->vv + i, fp));
        break;
    }

    *out = v;
    return 0;

    error:
    if (v) free(v);
    return err;
}

vio_err_t vio_dump_bytecode(vio_bytecode *bc, FILE *fp) {
    vio_err_t err = 0;
    int fd = fileno(fp);
    tpl_node *tn;
    SAVE("u", &bc->ic);
    for (uint32_t i = 0; i < bc->ic; ++i)
        vio_dump_val(bc->consts[i], fp);
    SAVE("u", &bc->ip);
    SAVE("U#", bc->prog, bc->ip);
    return 0;

    error:
    return err;
}

vio_err_t vio_load_bytecode(vio_ctx *ctx, vio_bytecode **bc, FILE *fp) {
    vio_err_t err = 0; tpl_node *tn; int fd = fileno(fp);
    uint32_t ic, ip;
    LOAD("u", &ic);
    VIO__CHECK(vio_bytecode_alloc(bc));
    (*bc)->ic = ic;
    VIO__CHECK(vio_bytecode_alloc_consts(*bc));
    for (uint32_t i = 0; i < ic; ++i)
        VIO__CHECK(vio_load_val(ctx, (*bc)->consts + i, fp));
    LOAD("u", &ip);
    (*bc)->ip = ip;
    VIO__CHECK(vio_bytecode_alloc_opcodes(*bc));
    LOAD("U#", (*bc)->prog, ip);
    return 0;
    error:
    return err;
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
    SAVE("u", &klen);
    SAVE("c#u", (char *)key, klen, &def_idx);
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

vio_err_t load_def(vio_ctx *ctx, FILE *fp) {
    vio_err_t err = 0; tpl_node *tn; int fd = fileno(fp);
    char *name = NULL;
    uint32_t nlen, def_idx;
    LOAD("u", &nlen);
    name = (char *)malloc(nlen);
    VIO__ERRIF(name == NULL, VE_ALLOC_FAIL);
    LOAD("c#u", name, nlen, &def_idx);
    vio_dict_store(ctx->dict, name, nlen, def_idx);
    vio_load_bytecode(ctx, ctx->defs + def_idx, fp);
    free(name);
    return 0;
    error:
    if (name) free(name);
    return err;
}

vio_err_t vio_load_dict(vio_ctx *ctx, FILE *fp) {
    vio_err_t err = 0; tpl_node *tn; int fd = fileno(fp);
    uint64_t dict_sz;
    LOAD("U", &dict_sz);
    for (uint64_t i = 0; i < dict_sz; ++i)
        VIO__CHECK(load_def(ctx, fp));
    return 0;
    error:
    return err;
}

vio_err_t vio_dump(vio_ctx *ctx, FILE *fp) {
    vio_err_t err = 0;
    tpl_node *tn;
    int fd = fileno(fp);
    VIO__CHECK(vio_dump_dict(ctx, fp));
    SAVE("u", &ctx->sp);
    for (uint32_t i = 0; i < ctx->sp; ++i)
        VIO__CHECK(vio_dump_val(ctx->stack[i], fp));
    return 0;
    error:
    return err;
}

vio_err_t vio_load(vio_ctx *ctx, FILE *fp) {
    vio_err_t err = 0;
    tpl_node *tn;
    int fd = fileno(fp);
    uint32_t cnt;
    VIO__CHECK(vio_load_dict(ctx, fp));
    LOAD("u", &cnt);
    for (uint32_t i = 0; i < cnt; ++i)
        VIO__CHECK(vio_load_top(ctx, fp));

    return 0;
    error:
    return err;
}

void jsonify_val(JsonNode *n, vio_val *v) {
    json_append_member(n, "what", json_mkstring(vio_val_type_name(v->what)));
    const char *repr = vio_uneval_val(v);
    char *s;
    long ex;
    JsonNode *m;
    json_append_member(n, "repr", json_mkstring(repr));
    free(repr);
    switch (v->what) {
    case vv_int:
        json_append_member(n, "value", json_mknumber(v->i32));
        break;
    case vv_float:
        json_append_member(n, "value", json_mknumber(v->f32));
        break;
    case vv_num:
        s = mpf_get_str(NULL, &ex, 10, 0, v->n);
        json_append_member(n, "value", json_mkstring(s));
        json_append_member(n, "exp", json_mknumber(ex));
        free(s);
        break;
    case vv_str:
        s = (char *)malloc(v->len);
        strncpy(s, v->s, v->len);
        s[v->len-1] = '\0';
        json_append_member(n, "value", json_mkstring(s));
        free(s);
        break;
    case vv_vec:
        json_append_member(n, "len", json_mknumber(v->vlen));
        m = json_mkarray();
        jsonify_vals(m, v->vv, v->vlen);
        json_append_member(n, "values", m);
        break;
    case vv_mat:
        json_append_member(n, "rows", json_mknumber(v->rows));
        json_append_member(n, "cols", json_mknumber(v->cols));
        m = json_mkarray();
        jsonify_vals(m, v->vv, v->rows * v->cols);
        json_append_member(n, "values", m);
        break;
    default: break;
    }
}

void jsonify_vals(JsonNode *n, vio_val **vs, uint32_t cnt) {
    for (uint32_t i = 0; i < cnt; ++i) {
        JsonNode *m = json_mkobject(); /* gets deleted when n is freed */
        jsonify_val(m, vs[i]);
        json_append_element(n, m);
    }
}

const char *vio_json_val(vio_val *v) {
    JsonNode *n = json_mkobject();
    jsonify_val(n, v);
    const char *out = json_stringify(n, "\t");
    json_delete(n);
    return out;
}
const char *vio_json_vals(vio_val **vs, uint32_t cnt) {
    JsonNode *n = json_mkarray();
    jsonify_vals(n, vs, cnt);
    const char *out = json_stringify(n, "\t");
    json_delete(n);
    return out;
}

const char *vio_json_top(vio_ctx *ctx) {
    if (ctx->sp == 0) return NULL;
    return vio_json_val(ctx->stack[ctx->sp-1]);
}

const char *vio_json_stack(vio_ctx *ctx) {
    return vio_json_vals(ctx->stack, ctx->sp);
}
