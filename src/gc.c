#include "val.h"
#include "gc.h"

void vio_mark(vio_ctx *ctx) {
    for (uint32_t i = 0; i < ctx->sp; ++i)
        vio_mark_val(ctx->stack[i]);
    /* never gc constants from functions, that would be bad */
    for (uint32_t i = 0; i < ctx->defp; ++i) {
        for (uint32_t j = 0; j < ctx->defs[i]->ic; ++j)
            vio_mark_val(ctx->defs[i]->consts[j]);
    }
}

void vio_sweep(vio_ctx *ctx) {
    vio_val *v = ctx->ohead, *last, *w;
    while (v) {
        if (v->mark) {
            v->mark = 0;
            last = v;
            v = v->next;
        }
        else {
            w = v->next;
            if (v == ctx->ohead)
                ctx->ohead = w;
            else
                last->next = w;
            vio_val_free(v);
            --ctx->ocnt;
            v = w;
        }
    }
}

void vio_gc(vio_ctx *ctx) {
    vio_mark(ctx);
    vio_sweep(ctx);
}
