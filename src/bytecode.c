#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "art.h"
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

vio_err_t vio_bytecode_alloc_opcodes(vio_bytecode *bc) {
    ALLOC_ENSURE(vio_opcode, bc->prog, bc->ip, bc->psz)
}

vio_err_t vio_bytecode_alloc_consts(vio_bytecode *bc) {
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

enum end_cond { end_eof, end_defend, end_quotend };

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

vio_err_t emit_quot(vio_ctx *ctx, vio_tok **start, vio_bytecode *bc) {
    vio_err_t err = 0;
    vio_val *v;
    vio_bytecode *fn;
    uint32_t imm1 = 0;
    uint8_t imm2 = 0, imm3 = 0, imm4 = 0;

    VIO__CHECK(vio_bytecode_alloc(&fn));
    VIO__CHECK(emit(ctx, start, fn, vop_retq, end_quotend));

    EMIT_CONST(vv_quot) {
        v->bc = fn;
    }

    return 0;
    error:
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
        if (strncmp(t->s, "dip", 3) == 0)
            EMIT_OPCODE(vop_dip);
        else if (strncmp(t->s, "dup", 3) == 0)
            EMIT_OPCODE(vop_dup);
        else if (strncmp(t->s, "rot", 3) == 0)
            EMIT_OPCODE(vop_rot);
        break;
    case 4:
        if (strncmp(t->s, "eval", 4) == 0)
            EMIT_OPCODE(vop_callq);
        else if (strncmp(t->s, "keep", 4) == 0)
            EMIT_OPCODE(vop_keep);
        else if (strncmp(t->s, "swap", 4) == 0)
            EMIT_OPCODE(vop_swap);
        break;
    case 5:
        if (strncmp(t->s, "match", 5) == 0)
            EMIT_OPCODE(vop_pcmatchstr);
        else if (strncmp(t->s, "parse", 5) == 0)
            EMIT_OPCODE(vop_pcparse);
        break;
    case 6:
        if (strncmp(t->s, "tagend", 6) == 0)
            EMIT_OPCODE(vop_tend);
        else if (strncmp(t->s, "vecend", 6) == 0)
            EMIT_OPCODE(vop_vend);
        break;
    case 8:
        if (strncmp(t->s, "tagstart", 8) == 0)
            EMIT_OPCODE(vop_tstart);
        else if (strncmp(t->s, "vecstart", 8) == 0)
            EMIT_OPCODE(vop_vstart);
        break;
    }
}

#define is_short_word(t,c) ((t) && (t)->what == vt_word && (t)->len == 1 && (t)->s[0] == (c))
#define is_def_end(t) is_short_word(t, ';')
#define is_quot_start(t) is_short_word(t, '[')
#define is_quot_end(t) is_short_word(t, ']')

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
        VIO__CHECK(vio_bytecode_alloc_opcodes(bc));
        VIO__CHECK(vio_bytecode_alloc_consts(bc));
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
        case vt_tagword:
            EMIT_CONST(vv_tagword) {
                v->len = t->len;
                v->s = (char *)malloc(v->len);
                strncpy(v->s, t->s, v->len);
                v->vlen = 0;
                v->vv = NULL;
            }
            break;
        case vt_word:
            /* try, in order:
               - quotation
               - if the word ends with :, it is a definition
               - if it's a built-in word, emit the specific instruction for it
               - if it's a registered C function, emit the callc instruction
               - if we've seen its definition, emit a direct call to its address
               - otherwise emit an indirect call to its name */
            if (is_quot_start(t)) {
                VIO__ERRIF(t->next == NULL, VE_UNCLOSED_QUOT);
                t = t->next;
                emit_quot(ctx, &t, bc);
            }
            else if (t->s[t->len-1] == ':' && t->len > 1) {
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
                    if (vio_cdict_lookup(ctx->cdict, t->s, t->len) && imm2 != 1)
                        EMIT_OPCODE(vop_callc);
                    else
                        EMIT_OPCODE(vop_call);
                }
                /* otherwise do nothing since emit_builtin did all the work */
            }
            break;
        case vt_rule:
            VIO__CHECK(vio_val_new(ctx, &v, vv_parser));
            v->len = t->len;
            VIO__ERRIF((v->s = (char *)malloc(v->len)) == NULL, VE_ALLOC_FAIL);
            strncpy(v->s, t->s, v->len);
            v->p = NULL;
            bc->consts[bc->ic] = v;
            imm1 = bc->ic++;
            EMIT_OPCODE(vop_pcloadrule);
            break;
        }
        *begin = t;
        t = t->next;
        if (stop_at == end_defend && is_def_end(t)) {
            *begin = t;
            t = NULL;
        }
        else if (stop_at == end_quotend && is_quot_end(t)) {
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
    /* Don't free the values of bc->consts here, since they are allocated
       normally with vio_val_new and thus get marked and swept. */
    free(bc->consts);
    free(bc);
}

vio_err_t vio_bytecode_clone(vio_ctx *ctx, vio_bytecode *bc, vio_bytecode **out) {
    vio_err_t err = 0;
    VIO__CHECK(vio_bytecode_alloc(out));
    (*out)->ic = bc->ic;
    (*out)->ip = bc->ip;
    (*out)->csz = bc->csz;
    (*out)->psz = bc->psz;
    VIO__CHECK(vio_bytecode_alloc_opcodes(*out));
    memcpy((*out)->prog, bc->prog, sizeof(vio_opcode) * bc->psz);
    VIO__CHECK(vio_bytecode_alloc_consts(*out));
    for (uint32_t i = 0; i < bc->ic; ++i)
        VIO__CHECK(vio_val_clone(ctx, bc->consts[i], (*out)->consts + i));

    return 0;
    error:
    if (*out) vio_bytecode_free(*out);
    return err;
}
