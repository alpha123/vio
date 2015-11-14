#ifndef VIO_UNEVAL_H
#define VIO_UNEVAL_H

#ifdef BSD
#include <sysexit.h>
#else
#define EX_OSERR 71
#endif

#include "context.h"
#include "val.h"

/* For internal use, but some debugging tools may need it.
   @returns a pointer that the caller must free. */
char *vio_uneval_val(vio_val *v);

/* Create a string representation of the top of the stack.

   @ctx Look at the value on top of this context. Does not pop it.

   This functions is meant more for debugging or printing
   than metaprogramming.
   In theory it should vio_eval() to the correct value,
   but that is not guaranteed.

   Note: this functions calls malloc(). Unlike most vio functions,
         it assumes malloc will succeed, and dies if it does not.

   @returns In the case of an empty stack, returns NULL. Otherwise
            returns a pointer to some freshly allocated character array.
            The caller is responsible for freeing this value. */
char *vio_uneval(vio_ctx *ctx);

#endif
