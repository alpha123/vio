#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef USE_SYSTEM_LINENOISE
#include <linenoise.h>
#else
#include "linenoise.h"
#endif
#include "context.h"
#include "opcodes.h"
#include "vm.h"

int main(void) {
    vio_err_t err;
    vio_ctx ctx;
    vio_open(&ctx);

    vio_opcode prog[3];
    prog[0] = vio_opcode_pack(vop_load, 0, 0, 0, 0);
    prog[1] = vio_opcode_pack(vop_load, 1, 0, 0, 0);
    prog[2] = vio_opcode_pack(vop_add, 0, 0, 0, 0);
    vio_val *consts[2];
    vio_val_new(&ctx, consts, vv_vecf);
    vio_val_new(&ctx, consts + 1, vv_vec);
    vio_float v1[] = {3, 6};
    consts[0]->vf32 = v1; consts[0]->vlen = 2;

    consts[1]->vv = (vio_val **)malloc(2 * sizeof(vio_val));
    consts[1]->vlen = 2;
    vio_val_new(&ctx, consts[1]->vv, vv_int);
    vio_val_new(&ctx, consts[1]->vv + 1, vv_int);
    consts[1]->vv[0]->i32 = 5;
    consts[1]->vv[1]->i32 = 10;
    vio_exec(&ctx, prog, consts);

    uint32_t len;
    vio_float a, b;
    err = vio_pop_vec(&ctx, &len) || 
          vio_pop_float(&ctx, &a) ||
          vio_pop_float(&ctx, &b);
    if (err)
        printf("error: %d\n", err);
    else
        printf("{%g %g}\n", len, a, b);
    return err;
}
