#include "math.h"
#include "gc.h"
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

#define EXPECT(v, t) \
    if (v->what != t) \
        return VE_STRICT_TYPE_FAIL;

#define CHECK(expr) \
    do{if ((err = (expr))) goto exit;}while(0)

#define SELF prog[pc-1]

#define IMM1 vio_opcode_imm1(SELF)
#define IMM2 vio_opcode_imm2(SELF)
#define IMM3 vio_opcode_imm3(SELF)
#define IMM4 vio_opcode_imm4(SELF)

#define NEXT goto *dispatch[vio_opcode_instr(prog[pc++])]
#define NEXT_MAYBEGC if (ctx->ocnt > VIO_GC_THRESH) vio_gc(ctx); NEXT

#define EXIT(code) do{ err = code; goto exit; }while(0)
#define RAISE(code, ...) do{ vio_raise(ctx, code, __VA_ARGS__); EXIT(code); }while(0)

vio_err_t vio_exec(vio_ctx *ctx, vio_bytecode *bc) {
    vio_err_t err = 0;
    vio_opcode *prog = bc->prog;
    vio_val **consts = bc->consts;

#define INIT_DISPATCH_TABLE(instr) [vop_##instr] = &&op_##instr,
#define INIT_DISPATCH_TABLE_(instr) [vop_##instr] = &&op_##instr
    static void *dispatch[] = {
        LIST_VM_INSTRUCTIONS(INIT_DISPATCH_TABLE, INIT_DISPATCH_TABLE_)
    };
#undef INIT_DISPATCH_TABLE
#undef INIT_DISPATCH_TABLE_

    uint32_t ap_base[VIO_MAX_CALL_DEPTH], *address_sp,
             pc = 0, idx, vcnt;
    vio_val *v;
    vio_int vcnti;
    vio_float *vf;
    address_sp = ap_base;

    NEXT;

op_halt:
    EXIT(0);
op_load:
    idx = IMM1;
    SAFE_PUSH(consts[idx])
    NEXT;
op_call:
    if (address_sp - ap_base >= VIO_MAX_CALL_DEPTH)
        EXIT(VE_EXCEEDED_MAX_CALL_DEPTH);
    *address_sp++ = pc + 1;
    if (IMM2 == 1) /* address as operand--function call */
        pc = IMM1;
    else if (IMM2 == 2) { /* pop quotation from stack */
        SAFE_POP(v)
        EXPECT(v, vv_quot)
        pc = v->jmp;
    }
    else if (IMM2 == 3) { /* pop string from stack and look up address */
        SAFE_POP(v)
        EXPECT(v, vv_str)
        if (!vio_dict_lookup(ctx->dict, v->s, v->len, &pc))
            RAISE(VE_CALL_TO_UNDEFINED_WORD, "Attempted to call '%.*s', but that word is not defined.", v->s, v->len);
    }
    NEXT;
op_ret:
    /* assume the opcode stream is valid and we never have more rets than calls */
    pc = *--address_sp;
    NEXT;
op_reljmp:
    /* backward jump if imm2 is 1, forward jump if it is 0 */
    pc += IMM1 * (-2 * IMM2 + 1);
    NEXT;
op_add:
    CHECK(vio_add(ctx));
    NEXT_MAYBEGC;
op_sub:
    CHECK(vio_sub(ctx));
    NEXT_MAYBEGC;
op_mul:
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
op_nop:
    NEXT;

    exit:
    return err;
}
