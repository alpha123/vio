#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gmp.h>
#include "uneval.h"

char *vio_uneval_val(vio_val *v) {
    char *out, **vout;
    uint32_t total;
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
    case vv_vec:
        vout = (char **)malloc(v->vlen * sizeof(char *));
        total = 4 + v->vlen; /* spaces between each value */
        for (uint32_t i = 0; i < v->vlen; ++i) {
            vout[i] = vio_uneval_val(v->vv[i]);
            total += strlen(vout[i]);
        }
        out = (char *)malloc(total);
        out[0] = '{';
        out[1] = ' ';
        out[2] = '\0';
        for (uint32_t i = 0; i < v->vlen; ++i) {
            strcat(out, vout[i]);
            strcat(out, " ");
        }
        out[total-2] = '}';
        out[total-1] = '\0';
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

void vio_print_stack(vio_ctx *ctx, FILE *f) {
    uint32_t i = ctx->sp;
    char *s;
    while (i--) {
        s = vio_uneval_val(ctx->stack[i]);
        if (f == NULL) {
            puts(s);
            putchar('\n');
        }
        else {
            fputs(s, f);
            fputc('\n', f);
        }
        free(s);
    }
}
