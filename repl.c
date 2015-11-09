#include <stdlib.h>
#include <stdio.h>
#include "linenoise.h"
#include "context.h"

int main(void) {
    vio_err_t err;
    vio_ctx ctx;
    vio_open(&ctx);
    vio_push_str(&ctx, 3, "foo");
    vio_push_str(&ctx, 3, "bar");
    if ((err = vio_push_vec(&ctx, 2))) {
        printf("error creating vector: %d\n", err);
        return 1;
    }

    uint32_t vl;
    if ((err = vio_pop_vec(&ctx, &vl))) {
        printf("error reading vector: %d\n", err);
        return 1;
    }

    char *s;
    uint32_t len;
    vio_pop_str(&ctx, &len, &s);
    printf("str: %.*s (length %d)\n", len, s, len);
    vio_pop_str(&ctx, &len, &s);
    printf("str: %.*s (length %d)\n", len, s, len);
    return 0;
}
