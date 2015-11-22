#include "math.h"
#include "strings.h"
#include "stdvio.h"

void vio_load_stdlib(vio_ctx *ctx) {
    vio_register(ctx, "++", vio_strcat, 2);

#define REGISTER(f) vio_register(ctx, #f, vio_##f, 1);
    LIST_MATH_UNARY(REGISTER)
#undef REGISTER
}
