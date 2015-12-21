#include "cmp.h"
#include "vm.h"
#include "vecapply.h"

struct partition_item {
    vio_val *v;
    struct partition_item *next;
};

struct partition {
    uint32_t size;
    struct partition_item *last;
};

#define SKIPLIST_KEY vio_val *
#define SKIPLIST_VALUE struct partition *
#define SKIPLIST_STATIC
#define SKIPLIST_IMPLEMENTATION
#include "skiplist.h"

vio_err_t vio_fold_val(vio_ctx *ctx, vio_val *v, vio_val *q, vio_val *initial, vio_val **out) {
    vio_err_t err = 0;
    vio_val *acc = initial;

    if (v->what == vv_matf) {
        VIO__CHECK(vio_coerce(ctx, v, &acc, vv_mat));
        v = acc;
    }
    if (v->what == vv_mat) {
        VIO__CHECK(vio_vec(ctx, out, v->rows, NULL));
        VIO__CHECK(vio_vec(ctx, &acc, v->cols, NULL));
        for (uint32_t i = 0; i < v->rows; ++i) {
            for (uint32_t j = 0; j < v->cols; ++j)
                acc->vv[j] = vio_mat_idx(v, i, j);
            VIO__CHECK(vio_fold_val(ctx, acc, q, initial, (*out)->vv + i));
        }
    }
    
    for (uint32_t i = 0; i < v->vlen; ++i) {
        VIO__ERRIF(ctx->sp + 1 >= VIO_STACK_SIZE, VE_STACK_OVERFLOW);
        ctx->stack[ctx->sp++] = acc;
        ctx->stack[ctx->sp++] = v->vv[i];
        vio_exec(ctx, q->bc);
        acc = ctx->stack[--ctx->sp];
    }

    *out = acc;
    return 0;

    error:
    return err;
}

vio_err_t vio_fold(vio_ctx *ctx) {
    vio_err_t err = 0;
    vio_val *q, *initial, *vec, *acc;
    VIO__RAISEIF(ctx->sp < 3, VE_STACK_EMPTY,
                 "Fold context requires a quotation, initial value, and vector, "
                 "but the stack doesnt't have enough values.");

    VIO__CHECK(vio_coerce(ctx, ctx->stack[--ctx->sp], &q, vv_quot));
    initial = ctx->stack[--ctx->sp];
    VIO__CHECK(vio_coerce(ctx, ctx->stack[--ctx->sp], &vec, vv_vec));
    VIO__CHECK(vio_fold_val(ctx, vec, q, initial, &acc));
    ctx->stack[ctx->sp++] = acc;
    return 0;

    error:
    return err;
}

int gotta_free_em_all(vio_val *_v, struct partition *p, void *_data) {
    struct partition_item *it = p->last, *next;
    while (it) {
        next = it->next;
        free(it);
        it = next;
    }
    free(p);
    return 0;
}

struct part_data {
    int i;
    vio_val *rets;
    vio_val *parts;
    vio_ctx *ctx;
};

int fill_partitions_and_free(vio_val *ret, struct partition *p, void *data) {
    int j = 0;
    struct part_data *pd = (struct part_data *)data;
    struct partition_item *it = p->last, *next;
    pd->rets->vv[pd->i] = ret;
    vio_vec(pd->ctx, pd->parts->vv + pd->i++, p->size, NULL);
    while (it) {
        next = it->next;
        pd->parts->vv[pd->i-1]->vv[p->size - ++j] = it->v;
        free(it);
        it = next;
    }
    free(p);
    return 0;
}

int cmp_vals(vio_val *a, vio_val *b, void *ctx) {
    return vio_cmp_val((vio_ctx *)ctx, a, b);
}

vio_err_t vio_partition_val(vio_ctx *ctx, vio_val *v, vio_val *q, vio_val **out_rets, vio_val **out_parts) {
    vio_err_t err = 0;
    struct partition *p = NULL;
    struct partition_item *it = NULL;
    vio_val *ret;
    sl_skiplist parts;

    sl_init(&parts, cmp_vals, ctx, NULL, NULL);

    for (uint32_t i = 0; i < v->vlen; ++i) {
        VIO__ERRIF(ctx->sp >= VIO_STACK_SIZE, VE_STACK_OVERFLOW);
        ctx->stack[ctx->sp++] = v->vv[i];
        vio_exec(ctx, q->bc);
        ret = ctx->stack[--ctx->sp];
        if (!sl_find(&parts, ret, &p)) {
            VIO__ERRIF((p = (struct partition *)malloc(sizeof(struct partition))) == NULL, VE_ALLOC_FAIL);
            p->last = NULL;
            p->size = 0;
            sl_insert(&parts, ret, p, NULL);
        }
        VIO__ERRIF((it = (struct partition_item *)malloc(sizeof(struct partition_item))) == NULL, VE_ALLOC_FAIL);
        it->next = p->last;
        it->v = v->vv[i];
        p->last = it;
        ++p->size;
    }

    VIO__CHECK(vio_vec(ctx, out_rets, sl_size(&parts), NULL));
    VIO__CHECK(vio_vec(ctx, out_parts, sl_size(&parts), NULL));
    struct part_data pd = { .i = 0, .rets = *out_rets, .parts = *out_parts, .ctx = ctx };
    sl_iter(&parts, fill_partitions_and_free, &pd);
    sl_free(&parts);
    return 0;

    error:
    sl_iter(&parts, gotta_free_em_all, NULL);
    sl_free(&parts);
    return err;
}

vio_err_t vio_partition(vio_ctx *ctx) {
    vio_err_t err = 0;
    vio_val *q, *vec, *rets, *parts;
    VIO__RAISEIF(ctx->sp < 2, VE_STACK_EMPTY,
                 "Partition context requires a quotation and vector, but the "
                 "stack doesnt't have enough values.");

    VIO__CHECK(vio_coerce(ctx, ctx->stack[--ctx->sp], &q, vv_quot));
    VIO__CHECK(vio_coerce(ctx, ctx->stack[--ctx->sp], &vec, vv_vec));
    VIO__CHECK(vio_partition_val(ctx, vec, q, &rets, &parts));
    ctx->stack[ctx->sp++] = rets;
    ctx->stack[ctx->sp++] = parts;
    return 0;

    error:
    return err;
}

vio_err_t vio_mask_val(vio_ctx *ctx, vio_val *v, vio_val *w, vio_val **out) {
    vio_err_t err = 0;
    uint32_t j = 0, m;
    VIO__RAISEIF(v->what != vv_vec || w->what != vv_vec, VE_WRONG_TYPE,
                 "Mask expects two vectors, not '%s' and '%s'.",
                 vio_val_type_name(v->what), vio_val_type_name(w->what));

    VIO__CHECK(vio_vec(ctx, out, v->vlen, NULL));
    m = v->vlen > w->vlen ? w->vlen : v->vlen;
    for (uint32_t i = 0; i < m; ++i) {
        if (vio_true_val(w->vv[i]))
            (*out)->vv[j++] = v->vv[i];
    }
    (*out)->vlen = j;

    return 0;
    error:
    return err;
}

vio_err_t vio_mask(vio_ctx *ctx) {
    vio_err_t err = 0;
    vio_val *v, *w, *out;
    VIO__RAISEIF(ctx->sp < 2, VE_STACK_EMPTY,
                 "Mask needs two vectors, but the stack doesn't have enough values.");
    w = ctx->stack[--ctx->sp];
    v = ctx->stack[--ctx->sp];
    VIO__CHECK(vio_mask_val(ctx, v, w, &out));
    ctx->stack[ctx->sp++] = out;
    return 0;
    error:
    return err;
}

vio_err_t vio_elemof_val(vio_ctx *ctx, vio_val *v, vio_val *vs, int *out) {
    return 0;
}

vio_err_t vio_elemof(vio_ctx *ctx) {
    return 0;
}

vio_err_t vio_vector_cat_val(vio_ctx *ctx, vio_val *v, vio_val *w, vio_val **out) {
    vio_err_t err = 0;
    VIO__RAISEIF(v->what != vv_vec || w->what != vv_vec, VE_WRONG_TYPE,
                 "Vector-cat expects two vectors, not '%s' and '%s'.",
                 vio_val_type_name(v->what), vio_val_type_name(w->what));
    VIO__CHECK(vio_vec(ctx, out, v->vlen + w->vlen, NULL));
    memcpy((*out)->vv, v->vv, v->vlen * sizeof(vio_val *));
    memcpy((*out)->vv + v->vlen, w->vv, w->vlen * sizeof(vio_val *));

    return 0;
    error:
    return err;
}

vio_err_t vio_vector_cat(vio_ctx *ctx) {
    vio_err_t err = 0;
    vio_val *v, *w, *out;
    VIO__RAISEIF(ctx->sp < 2, VE_STACK_EMPTY,
                 "Vector-cat needs two vectors, but the stack doesn't have enough values.");
    w = ctx->stack[--ctx->sp];
    v = ctx->stack[--ctx->sp];
    VIO__CHECK(vio_vector_cat_val(ctx, v, w, &out));
    ctx->stack[ctx->sp++] = out;
    return 0;
    error:
    return err;
}
