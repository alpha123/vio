#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gmp.h>
#include "uneval.h"

char *vio_uneval_val(vio_val *v) {
    char *out;
    switch (v->what) {
    case vv_str:
        out = (char *)malloc(v->len + 3);
        if (out == NULL) goto die;
        out[0] = '"';
        strncpy(out + 1, v->s, v->len);
        out[v->len + 1] = '"';
        out[v->len + 2] = '\0';
        break;
    case vv_int:
        out = (char *)malloc(22); /* maximum digits of a vio_int type + sign + null terminator */
        if (out == NULL) goto die;
        snprintf(out, 22, "%ld", v->i32);
        break;
    case vv_float:
        out = (char *)malloc(20); /* 16 + dot + sign + e + null */
        if (out == NULL) goto die;
        snprintf(out, 20, "%16g", v->f32);
        break;
    case vv_num:
        out = (char *)malloc(gmp_snprintf(NULL, 0, "%Ff", v->n) + 1);
        if (out == NULL) goto die;
        gmp_sprintf(out, "%Ff", v->n);
        break;
    default:
       out = (char *)malloc(40); /* arbitrary but should fit */
       snprintf(out, 40, "#<%s %p>", vio_val_type_name(v->what), (void *)v);
    }
    return out;

    die:
    fprintf(stderr, "vio_uneval() failed to allocate memory for output string. :(");
    exit(EX_OSERR);
}

char *vio_uneval(vio_ctx *ctx) {
    if (ctx->sp == 0) return NULL;
    return vio_uneval_val(ctx->stack[ctx->sp-1]);
}
