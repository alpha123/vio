#ifndef VIO_STDVIO_H
#define VIO_STDVIO_H

#include "context.h"

#define VIO_LIST_BUILTINS(X) \
    X("bi", "^keep eval") \
    X("bi*", "^dip eval") \
    X("bi@", "dup bi*") \
    X("bi2", "^keep2 eval") \
    X("bi2*", "^dip2 eval") \
    X("bi2@", "dup bi2*") \
\
    X("dip2", "swap ^dip") \
    X("dup2", "over over") \
    X("keep2", "^dup2 dip2") \
\
    X("over", "^dup swap") \
\
    X("preserve", "keep swap") \
    X("preserve2", "keep2 rot rot")

void vio_load_stdlib(vio_ctx *ctx);

int vio_is_builtin(uint32_t nlen, const char *name);
VIO_CONST
uint32_t vio_builtin_count(void);

vio_err_t vio_drop(vio_ctx *ctx);

#endif
