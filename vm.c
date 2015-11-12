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

#define SELF prog[pc-1]

#define IMM1 vio_opcode_imm1(SELF)
#define IMM2 vio_opcode_imm2(SELF)
#define IMM3 vio_opcode_imm3(SELF)
#define IMM4 vio_opcode_imm4(SELF)

#define NEXT goto *dispatch[vio_opcode_instr(prog[pc++])]
#define NEXT_MAYBEGC if (ctx->ocnt > VIO_GC_THRESH) vio_gc(ctx); NEXT

#define EXIT(code) err = code; goto exit

vio_err_t vio_exec(vio_ctx *ctx, vio_opcode *prog, vio_val **consts) {
    vio_err_t err = 0;

#define INIT_DISPATCH_TABLE(instr) [vop_##instr] = &&op_##instr,
#define INIT_DISPATCH_TABLE_(instr) [vop_##instr] = &&op_##instr
    static void *dispatch[] = {
        LIST_VM_INSTRUCTIONS(INIT_DISPATCH_TABLE, INIT_DISPATCH_TABLE_)
    };
#undef INIT_DISPATCH_TABLE
#undef INIT_DISPATCH_TABLE_

    uint32_t ap_base[VIO_MAX_CALL_DEPTH], *address_sp,
             pc = 0, idx;
    vio_val *v;
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
    *address_sp++ = pc;
    if (IMM2 == 1) /* address as operand--function call */
        pc = IMM1;
    else if (IMM2 == 2) { /* pop quotation from stack */
        SAFE_POP(v)
        EXPECT(v, vv_quot)
        pc = v->jmp;
    }
    NEXT;
op_ret:
    /* assume the opcode stream is valid and we never have more rets than calls */
    address_sp--;
    pc = *address_sp;
    NEXT;
op_add:
    vio_add(ctx);
    NEXT_MAYBEGC;
op_sub:
    vio_sub(ctx);
    NEXT_MAYBEGC;
op_mul:
    vio_mul(ctx);
    NEXT_MAYBEGC;
op_div:
    vio_div(ctx);
    NEXT_MAYBEGC;
op_nop:
    NEXT;

    exit:
    return err;
}
