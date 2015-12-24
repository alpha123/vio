#include <limits.h>
#include "bytecode.h"
#include "rewrite.h"
#include "tok.h"
#include "vm.h"
#include "eval.h"

vio_err_t vio_eval(vio_ctx *ctx, int64_t len, const char *expr) {
    vio_err_t err = 0;
    vio_tok *t = NULL, *original = NULL;
    vio_bytecode *bc = NULL;
    if (len < 0)
        len = strlen(expr);
    if (len > UINT_MAX)
        len = UINT_MAX;
    VIO__CHECK(vio_tokenize_str(&t, expr, (uint32_t)len));
    VIO__CHECK(vio_tok_clone(t, &original));
    VIO__CHECK(vio_rewrite(&t));
    VIO__CHECK(vio_emit(ctx, t, &bc));
    bc->tok = original;
    VIO__CHECK(vio_exec(ctx, bc));
    vio_tok_free_all(t);
    vio_bytecode_free(bc);
    return 0;

    error:
    if (t) vio_tok_free_all(t);
    if (original) vio_tok_free_all(original);
    if (bc) vio_bytecode_free(bc);
    return err;
}
