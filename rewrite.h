#ifndef VIO_REWRITE_H
#define VIO_REWRITE_H

#include "error.h"
#include "tok.h"

/* Vio does actually have some syntax, such as vector literals and quotations.
   Rewrite the token stream to simplify compiling those. */
vio_err_t vio_rewrite(vio_tok **t);

#endif
