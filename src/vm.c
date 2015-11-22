#include "art.h"
#include "math.h"
#include "parsercombinators.h"
#include "gc.h"
#include "strings.h"
#include "types.h"
#include "vm.h"

/* return NULL if the stack is empty */
vio_val *vm_pop(vio_ctx *ctx) {
    if (ctx->sp > 0)
        return ctx->stack[--ctx->sp];
    return NULL;
}

/* return 1 if the stack overflowed, 0 otherwise */
int vm_push(vio_ctx *ctx, vio_val *v) {
    if (ctx->sp < VIO_STACK_SIZE) {
        ctx->stack[ctx->sp++] = v;
        return 0;
    }
    return 1;
}

#define SAFE_PUSH(val) \
    if (vm_push(ctx, val)) \
        return VE_STACK_OVERFLOW;

#define SAFE_POP(v) \
    if (!(v = vm_pop(ctx))) \
        return VE_STACK_EMPTY;

int safe_push_clone(vio_ctx *ctx, vio_val *v) {
    vio_val *clone;
    if (vio_val_clone(ctx, v, &clone))
        return 1;
    return vm_push(ctx, clone);
}

#define EXPECT(v, t) \
    if (v->what != t) \
        return VE_STRICT_TYPE_FAIL;

#define CHECK(expr) \
    do{if ((err = (expr))) goto exit;}while(0)

#define EC(prop) (aec->prop)

#define SELF EC(prog)[EC(pc)-1]

#define IMM1 vio_opcode_imm1(SELF)
#define IMM2 vio_opcode_imm2(SELF)
#define IMM3 vio_opcode_imm3(SELF)
#define IMM4 vio_opcode_imm4(SELF)

#define NEXT goto *dispatch[vio_opcode_instr(EC(prog)[EC(pc)++])]
#define NEXT_MAYBEGC if (ctx->ocnt > VIO_GC_THRESH) vio_gc(ctx); NEXT

#define EXIT(code) do{ err = code; goto exit; }while(0)
#define RAISE(code, ...) do{ vio_raise(ctx, code, __VA_ARGS__); EXIT(code); }while(0)

#define PUSH_EXEC_CONTEXT(c) do{ \
    CHECK(alloc_faux_stack_frame(&execctx, c, &aec)); \
    *ecp++ = aec; }while(0)

#define POP_EXEC_CONTEXT() do{ free(*--ecp); aec = *(ecp - 1); }while(0)

/* lightweight stack frame thing for handling function calls
   this is probably a subpar implementation, but I really don't
   want to recursively call vio_exec for every function call */
struct stack_frame {
    vio_opcode *prog;
    vio_val **consts;
    uint32_t pc;
    uint32_t ap_base[VIO_MAX_CALL_DEPTH];
    uint32_t *address_sp;

    struct stack_frame *next;
};

/* exists entirely to hold a free list of stack_frames
   so that we don't have to do an allocation on every function call
   NOTE: currently unused, but exists so the API won't change */
struct exec_ctx {
};

vio_err_t alloc_faux_stack_frame(struct exec_ctx *_unused_ctx, vio_bytecode *from, struct stack_frame **out) {
    *out = (struct stack_frame *)malloc(sizeof(struct stack_frame));
    if (*out == NULL)
        return VE_ALLOC_FAIL;
    (*out)->prog = from->prog;
    (*out)->consts = from->consts;
    (*out)->pc = 0;
    (*out)->address_sp = (*out)->ap_base;
    return 0;
}

vio_err_t vio_exec(vio_ctx *ctx, vio_bytecode *bc) {
    vio_err_t err = 0;

    uint32_t idx, vcnt;
    vio_val *v;
    vio_int vcnti;
    vio_float *vf;

    struct exec_ctx execctx;
    struct stack_frame *ec_stack[VIO_MAX_CALL_DEPTH];
    struct stack_frame *aec, **ecp = ec_stack;
    PUSH_EXEC_CONTEXT(bc);

#define INIT_DISPATCH_TABLE(instr) [vop_##instr] = &&op_##instr,
#define INIT_DISPATCH_TABLE_(instr) [vop_##instr] = &&op_##instr
    static void *dispatch[] = {
        LIST_VM_INSTRUCTIONS(INIT_DISPATCH_TABLE, INIT_DISPATCH_TABLE_)
    };
#undef INIT_DISPATCH_TABLE
#undef INIT_DISPATCH_TABLE_
    NEXT;

op_halt:
    EXIT(0);
op_load:
    idx = IMM1;
    safe_push_clone(ctx, EC(consts)[idx]);
    NEXT;
op_callq:
    SAFE_POP(v)
    EXPECT(v, vv_quot)
    PUSH_EXEC_CONTEXT(v->bc);
    NEXT;
op_call: {
    vio_bytecode *fn;
    uint32_t idx;
    if (IMM2 == 1) /* index as operand--call a known function */
        idx = IMM1;
    else { /* pop string from stack and look up index */
        SAFE_POP(v)
        EXPECT(v, vv_str)
        if (!vio_dict_lookup(ctx->dict, v->s, v->len, &idx))
            RAISE(VE_CALL_TO_UNDEFINED_WORD, "Attempted to call '%.*s', but that word is not defined.", v->len, v->s);
    }
    fn = ctx->defs[idx];
    PUSH_EXEC_CONTEXT(fn);
    NEXT;
}
op_callc: {
    vio_function_info *fi;
    vio_val *a, *b, *vout, *w;
    SAFE_POP(v)
    EXPECT(v, vv_str)
    fi = (vio_function_info *)art_search(ctx->cdict,
                                    (const unsigned char *)v->s, v->len);
    if (fi == NULL)
        RAISE(VE_CALL_TO_UNDEFINED_WORD, "If you're seeing this, something very strange happened.");
    /* Auto-vectorize functions of arity 1 and 2 */
    if (fi->arity == 1) {
        SAFE_POP(a)
        if (a->what == vv_vec) {
            CHECK(vio_vec(ctx, &vout, a->vlen * fi->ret_cnt, NULL));
            for (uint32_t i = 0; i < a->vlen; ++i) {
                SAFE_PUSH(a->vv[i])
                CHECK(fi->fn(ctx));
                for (uint32_t j = 0; j < fi->ret_cnt; ++j) {
                    SAFE_POP(w)
                    vout->vv[i * fi->ret_cnt + j] = w;
                }
            }
            SAFE_PUSH(vout)
        }
        else {
            SAFE_PUSH(a)
            CHECK(fi->fn(ctx));
        }
    }
    else if (fi->arity == 2) {
        SAFE_POP(a)
        SAFE_POP(b)
        if (a->what == vv_vec && b->what == vv_vec) {
            uint32_t len = a->vlen > b->vlen ? a->vlen : b->vlen;
            CHECK(vio_vec(ctx, &vout, len * fi->ret_cnt, NULL));
            for (uint32_t i = 0; i < len; ++i) {
                SAFE_PUSH(b->vv[i % b->vlen])
                SAFE_PUSH(a->vv[i % a->vlen])
                CHECK(fi->fn(ctx));
                for (uint32_t j = 0; j < fi->ret_cnt; ++j) {
                    SAFE_POP(w)
                    vout->vv[i * fi->ret_cnt + j] = w;
                }
            }
            SAFE_PUSH(vout)
        }
        else {
            int swapped = 0;
            if (b->what == vv_vec) {
                w = b;
                b = a;
                a = w;
                swapped = 1;
            }
            if (a->what == vv_vec) {
                CHECK(vio_vec(ctx, &vout, a->vlen * fi->ret_cnt, NULL));
                for (uint32_t i = 0; i < a->vlen; ++i) {
                    if (swapped) {
                        SAFE_PUSH(a->vv[i])
                        SAFE_PUSH(b)
                    }
                    else {
                        SAFE_PUSH(b)
                        SAFE_PUSH(a->vv[i])
                    }
                    CHECK(fi->fn(ctx));
                    for (uint32_t j = 0; j < fi->ret_cnt; ++j) {
                        SAFE_POP(w)
                        vout->vv[i * fi->ret_cnt + j] = w;
                    }
                }
                SAFE_PUSH(vout)
            }
            else {
                SAFE_PUSH(b)
                SAFE_PUSH(a)
                CHECK(fi->fn(ctx));
            }
        }
    }
    else
        CHECK(fi->fn(ctx));
    NEXT_MAYBEGC;
}
op_retq:
    POP_EXEC_CONTEXT();
    NEXT;
op_ret:
    POP_EXEC_CONTEXT();
    NEXT;
op_reljmp:
    /* backward jump if imm2 is 1, forward jump if it is 0 */
    EC(pc) += IMM1 * (-2 * IMM2 + 1);
    NEXT;
op_add:
    if (vio_what(ctx) == vv_parser)
        goto op_pcmore;
    CHECK(vio_add(ctx));
    NEXT_MAYBEGC;
op_sub:
    CHECK(vio_sub(ctx));
    NEXT_MAYBEGC;
op_mul:
    if (vio_what(ctx) == vv_parser)
        goto op_pcmany;
    CHECK(vio_mul(ctx));
    NEXT_MAYBEGC;
op_div:
    CHECK(vio_div(ctx));
    NEXT_MAYBEGC;
op_dup:
    if (ctx->sp == 0) RAISE(VE_STACK_EMPTY, "'dup' has nothing to duplicate!");
    SAFE_PUSH(ctx->stack[ctx->sp-1])
    NEXT;
op_rot:
    if (ctx->sp < 3)
        RAISE(VE_STACK_EMPTY, "'rot' requires a stack size of at least 3, but only %d values exist", ctx->sp);
    v = ctx->stack[ctx->sp-3];
    ctx->stack[ctx->sp-3] = ctx->stack[ctx->sp-1];
    ctx->stack[ctx->sp-1] = ctx->stack[ctx->sp-2];
    ctx->stack[ctx->sp-2] = v;
    NEXT;
op_swap:
    if (ctx->sp < 2)
        RAISE(VE_STACK_EMPTY, "Can't use 'swap' on an empty stack or a stack with a single value.");
    v = ctx->stack[ctx->sp-2];
    ctx->stack[ctx->sp-2] = ctx->stack[ctx->sp-1];
    ctx->stack[ctx->sp-1] = v;
    NEXT;
op_vec:
    if (ctx->sp == 0) EXIT(VE_STACK_EMPTY);
    CHECK(vio_pop_int(ctx, &vcnti));
    if (vcnti < 0) RAISE(VE_COULDNT_CREATE_VECTOR, "Negatively sized vectors don't make sense.");
    vcnt = (uint32_t)vcnti;
    if (ctx->sp < vcnt)
        RAISE(VE_STACK_EMPTY, "Attempted to create a vector of %d elements, but the stack only has %d values.", vcnt, ctx->sp);
    vio_push_vec(ctx, vcnt);
    NEXT_MAYBEGC;
op_vecf:
    if (ctx->sp == 0) EXIT(VE_STACK_EMPTY);
    CHECK(vio_pop_int(ctx, &vcnti));
    if (vcnti < 0) RAISE(VE_COULDNT_CREATE_VECTOR, "Negatively sized vectors don't make sense.");
    vcnt = (uint32_t)vcnti;
    if (ctx->sp < vcnt)
        RAISE(VE_STACK_EMPTY, "Attempted to create a float vector of %d elements, but the stack only has %d values.", vcnt, ctx->sp);
    vf = (vio_float *)malloc(vcnt * sizeof(vio_float));
    for (uint32_t i = 0; i < vcnt; ++i)
        vio_pop_float(ctx, vf + i);
    vio_push_vecf32(ctx, vcnt, vf);
    NEXT;
op_pcparse:
    CHECK(vio_pc_parse(ctx));
    NEXT_MAYBEGC;
op_pcmatchstr:
    CHECK(vio_pc_str(ctx));
    NEXT_MAYBEGC;
op_pcthen:
    CHECK(vio_pc_then(ctx));
    NEXT_MAYBEGC;
op_pcor:
    CHECK(vio_pc_or(ctx));
    NEXT_MAYBEGC;
op_pcnot:
    CHECK(vio_pc_not(ctx));
    NEXT_MAYBEGC;
op_pcmaybe:
    CHECK(vio_pc_maybe(ctx));
    NEXT_MAYBEGC;
op_pcmany:
    CHECK(vio_pc_many(ctx));
    NEXT_MAYBEGC;
op_pcmore:
    CHECK(vio_pc_more(ctx));
    NEXT_MAYBEGC;
op_nop:
    NEXT;

    exit:
    return err;
}
