#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "bytecode.h"
#define DEFAULT_OPCODE_BUFSIZE 1024
#define DEFAULT_CONST_BUFSIZE 512

/* The `min + 5` is there to ensure space for any tokens
   that may result in several opcodes. It's totally arbitrary
   but at the moment no token results in more than 2. */
#define ALLOC_ENSURE(typ, buf, min, cnt) \
    if ((buf) == NULL) { \
        if (((buf) = (typ *)malloc(sizeof(typ) * (cnt))) == NULL) \
            return VE_ALLOC_FAIL; \
    } \
    else if ((min) + 5 < (cnt)) \
        return 0; \
    else { \
    	size_t newsz = cnt; \
    	while (newsz < (min) + 5) newsz *= 2; \
        typ *newbuf = (typ *)realloc((buf), sizeof(typ) * newsz); \
        if (newbuf == NULL) \
            return VE_ALLOC_FAIL; \
        cnt = newsz; \
        buf = newbuf; \
    } \
    return 0;

vio_err_t alloc_opcodes(vio_bytecode *bc) {
    ALLOC_ENSURE(vio_opcode, bc->prog, bc->ip, bc->psz)
}

vio_err_t alloc_consts(vio_bytecode *bc) {
    ALLOC_ENSURE(vio_val *, bc->consts, bc->ic, bc->csz)
}

vio_err_t vio_bytecode_alloc(vio_bytecode **out) {
    vio_bytecode *bc = (vio_bytecode *)malloc(sizeof(vio_bytecode));
    if (bc == NULL) {
        *out = NULL;
        return VE_ALLOC_FAIL;
    }
    bc->psz = DEFAULT_OPCODE_BUFSIZE;
    bc->csz = DEFAULT_CONST_BUFSIZE;
    bc->ip = bc->ic = 0;
    bc->prog = bc->consts = NULL;
    *out = bc;
    return 0;
}

#define EMIT_OPCODE(instr) (bc->prog[bc->ip++] = vio_opcode_pack((instr), imm1, imm2, imm3, imm4))
#define EMIT_CONST(type) \
    VIO__CHECK(vio_val_new(ctx, &v, (type))); \
    imm1 = bc->ic; \
    bc->consts[bc->ic++] = v; \
    EMIT_OPCODE(vop_load);

enum end_cond { end_eof, end_defend };

vio_err_t emit(vio_ctx *ctx, vio_tok **begin, vio_bytecode *bc, vio_opcode final, enum end_cond stop_at);

vio_err_t emit_definition(vio_ctx *ctx, const char *name, uint32_t nlen, vio_tok **def_start) {
    vio_err_t err = 0;
    vio_bytecode *fn;

    VIO__CHECK(vio_bytecode_alloc(&fn));
    VIO__CHECK(emit(ctx, def_start, fn, vop_ret, end_defend));
    vio_dict_store(ctx->dict, name, nlen, ctx->defp);
    VIO__ERRIF(ctx->defp == VIO_MAX_FUNCTIONS, VE_TOO_MANY_WORDS);
    ctx->defs[ctx->defp++] = fn;

    return 0;

    error:
    if (fn) vio_bytecode_free(fn);
    return err;
}

/* can't fail */
void emit_builtin(vio_ctx *ctx, vio_tok *t, vio_bytecode *bc) {
    uint32_t imm1 = 0;
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
        else if (strncmp(t->s, "vec", 3) == 0)
            EMIT_OPCODE(vop_vec);
        break;
    case 4:
        if (strncmp(t->s, "eval", 4) == 0) {
            imm2 = 2;
            EMIT_OPCODE(vop_call);
        }
        else if (strncmp(t->s, "swap", 4) == 0)
            EMIT_OPCODE(vop_swap);
        else if (strncmp(t->s, "vecf", 4) == 0)
            EMIT_OPCODE(vop_vecf);
        break;
    }
}

#define is_def_end(t) ((t) && (t)->what == vt_word && (t)->len == 1 && (t)->s[0] == ';')

vio_err_t emit(vio_ctx *ctx, vio_tok **begin, vio_bytecode *bc, vio_opcode final, enum end_cond stop_at) {
    vio_err_t err = 0;
    vio_val *v;
    vio_tok *t = *begin;
    uint32_t imm1;
    uint8_t imm2, imm3, imm4;
    /* strtol and mpf_(init_)set_strn expect a null-terminated string,
       while our tokenizer gives us a length.
       This is because the tokenizer may simply return a pointer to some location
       in the source string instead of allocating.
       Thus we have to convert to a null-terminated string to convert to a number. */
    char *nulls = NULL;
    while (t) {
        imm1 = imm2 = imm3 = imm4 = 0;
        VIO__CHECK(alloc_opcodes(bc));
        VIO__CHECK(alloc_consts(bc));
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
               - if the word ends with :, it is a definition
               - if it's a built-in word, emit the specific instruction for it
               - if we've seen its definition, emit a direct call to its address
               - otherwise emit an indirect call to its name */
            if (t->s[t->len-1] == ':' && t->len > 1) {
                char *name = t->s;
                uint32_t nlen = t->len - 1;
                t = t->next; /* start with the first token of the definition,
                                gets updated to point to the end */
                emit_definition(ctx, name, nlen, &t);
            }
            else {
                uint32_t oip = bc->ip;
                emit_builtin(ctx, t, bc);
                if (bc->ip - oip == 0) { /* word isn't a builtin */
                    if (vio_dict_lookup(ctx->dict, t->s, t->len, &imm1))
                        imm2 = 1;
                    else {
                        EMIT_CONST(vv_str) {
                            v->len = t->len;
                            v->s = (char *)malloc(v->len);
                            strncpy(v->s, t->s, v->len);
                        }
                        imm2 = 2;
                    }
                    EMIT_OPCODE(vop_call);
                }
                /* otherwise do nothing since emit_builtin did all the work */
            }
            break;
        }
        *begin = t;
        t = t->next;
        if (stop_at == end_defend && is_def_end(t)) {
            *begin = t;
            t = NULL;
        }
    }
    imm1 = imm2 = imm3 = imm4 = 0;
    EMIT_OPCODE(final);

    return 0;

    error:
    return err;
}

vio_err_t vio_emit(vio_ctx *ctx, vio_tok *t, vio_bytecode **out) {
    vio_err_t err = 0;
    VIO__CHECK(vio_bytecode_alloc(out));
    VIO__CHECK(emit(ctx, &t, *out, vop_halt, end_eof));

    return 0;

    error:
    if (*out) vio_bytecode_free(*out);
    return err;
}

void vio_bytecode_free(vio_bytecode *bc) {
    free(bc->prog);
    /* don't free our constants! they're (possibly) referenced on the stack
       and will be gced if they aren't */
    free(bc);
}
