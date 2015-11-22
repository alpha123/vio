#include "strings.h"
#include "stdvio.h"

void vio_load_stdlib(vio_ctx *ctx) {
    vio_register(ctx, "++", vio_strcat);
}
