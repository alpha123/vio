#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "emit.h"
#define DEFAULT_OPCODE_BUFSIZE 1024
#define DEFAULT_CONST_BUFSIZE 512

/* The `min + 5` is there to ensure space for any tokens
   that may result in several opcodes. It's totally arbitrary
   but at the moment no token results in more than 2. */
#define ALLOC_ENSURE(typ) \
    if (*buf == NULL) { \
        if ((*buf = (typ *)malloc(sizeof(typ) * *cnt)) == NULL) \
            return VE_ALLOC_FAIL; \
    } \
    else if (min + 5 < *cnt) \
        return 0; \
    else { \
    	size_t newsz = *cnt; \
    	while (newsz < min + 5) newsz *= 2; \
        typ *newbuf = (typ *)realloc(buf, sizeof(typ) * newsz); \
        if (newbuf == NULL) \
            return VE_ALLOC_FAIL; \
        *cnt = newsz; \
        *buf = newbuf; \
    } \
    return 0;

vio_err_t alloc_opcodes(vio_opcode **buf, uint32_t min, size_t *cnt) {
    ALLOC_ENSURE(vio_opcode)
}

vio_err_t alloc_consts(vio_val ***buf, uint32_t min, size_t *cnt) {
    ALLOC_ENSURE(vio_val *)
}

#define CHECK(expr) if ((err = (expr))) goto error;
#define EMIT_OPCODE(instr) ((*prog)[ip++] = vio_opcode_pack((instr), imm1, imm2, imm3, imm4))
#define EMIT_CONST(type) \
    CHECK(vio_val_new(ctx, &v, (type))); \
    imm1 = ic; \
    (*consts)[ic++] = v; \
    EMIT_OPCODE(vop_load);

vio_err_t emit_builtin(vio_ctx *ctx, vio_tok *t, vio_opcode **prog, vio_val ***consts,
                       uint32_t ip, uint32_t ic,
                       uint32_t *pemit_cnt, uint32_t *cemit_cnt) {
    vio_err_t err = 0;
    uint32_t sip = ip, sic = ic, imm1 = 0;
    uint8_t imm2 = 0, imm3 = 0, imm4 = 0;
    switch (t->len) {
    case 1:
        switch (t->s[0]) {
        case '+': EMIT_OPCODE(vop_add); break;
        case '-': EMIT_OPCODE(vop_sub); break;
        case '*': EMIT_OPCODE(vop_mul); break;
        case '/': EMIT_OPCODE(vop_div); break;
        }
        break;
    case 3:
        if (strncmp(t->s, "dup", 3) == 0)
            EMIT_OPCODE(vop_dup);
        else if (strncmp(t->s, "rot", 3) == 0)
            EMIT_OPCODE(vop_rot);
        break;
    case 4:
        if (strncmp(t->s, "eval", 4) == 0) {
            imm2 = 2;
            EMIT_OPCODE(vop_call);
        }
        else if (strncmp(t->s, "swap", 4) == 0)
            EMIT_OPCODE(vop_swap);
        break;
    }
    *pemit_cnt = ip - sip;
    *cemit_cnt = ic - sic;
    return 0;

    error:
    return err;
}

vio_err_t vio_emit(vio_ctx *ctx, vio_tok *t, uint32_t *plen, vio_opcode **prog, uint32_t *clen, vio_val ***consts) {
    vio_err_t err = 0;
    vio_val *v;
    uint32_t ip = 0, ic = 0, builtin_op_cnt, builtin_const_cnt, imm1;
    size_t psz = DEFAULT_OPCODE_BUFSIZE, csz = DEFAULT_CONST_BUFSIZE;
    uint8_t imm2, imm3, imm4;
    *prog = NULL;
    *consts = NULL;
    /* strtol and mpf_(init_)set_strn expect a null-terminated string,
       while our tokenizer gives us a length.
       This is because the tokenizer may simply return a pointer to some location
       in the source string instead of allocating.
       Thus we have to convert to a null-terminated string to convert to a number. */
    char *nulls = NULL;
    while (t) {
        builtin_op_cnt = builtin_const_cnt = 0;
        imm1 = imm2 = imm3 = imm4 = 0;
        CHECK(alloc_opcodes(prog, ip, &psz))
        CHECK(alloc_consts(consts, ic, &csz))
        switch (t->what) {
        case vt_str:
            EMIT_CONST(vv_str) {
                v->len = t->len;
                v->s = (char *)malloc(v->len);
                strncpy(v->s, t->s, v->len);
            }
            break;
        case vt_num:
            nulls = (char *)malloc(t->len + 1);
            strncpy(nulls, t->s, t->len);
            nulls[t->len] = '\0';
            if (memchr(t->s, '.', t->len) == NULL) {
                EMIT_CONST(vv_int) {
                    v->i32 = strtol(nulls, NULL, 10);
                }
            }
            else {
                EMIT_CONST(vv_num) {
                    if (mpf_init_set_str(v->n, nulls, 10)) {
                        mpf_clear(v->n); /* mpf_t is initialized even if the conversion fails */
                        free(nulls);
                        err = VE_TOKENIZER_BAD_NUMBER;
                        goto error;
                    }
                }
            }
            free(nulls);
            break;
        case vt_word:
            /* try, in order:
               - if it's a built-in word, emit the specific instruction for it
               - if we've seen its definition, emit a direct call to its address
               - otherwise emit an indirect call to its name */
            CHECK(emit_builtin(ctx, t, prog, consts, ip, ic, &builtin_op_cnt, &builtin_const_cnt))
            if (builtin_op_cnt == 0) {
                if (vio_dict_lookup(ctx->dict, t->s, t->len, &imm1))
                    imm2 = 1;
                else {
                    EMIT_CONST(vv_str) {
                        v->len = t->len;
                        v->s = (char *)malloc(v->len);
                        strncpy(v->s, t->s, v->len);
                    }
                    imm2 = 3;
                }
                EMIT_OPCODE(vop_call);
            }
            else {
                ip += builtin_op_cnt;
                ic += builtin_const_cnt;
            }
            break;
        }
        t = t->next;
    }
    imm1 = imm2 = imm3 = imm4 = 0;
    EMIT_OPCODE(vop_halt);
    *plen = ip;
    *clen = ic;

    return 0;

    error:
    if (*prog) free(*prog);
    if (*consts) {
        for (uint32_t i = 0; i < ic; ++i)
            vio_val_free((*consts)[i]);
        free(*consts);
    }
    *plen = 0;
    *clen = 0;
    return err;
}
