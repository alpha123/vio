#ifndef VIO_ERROR_H
#define VIO_ERROR_H

#include <stdint.h>

#define LIST_ERRORS(_X, X, X_) \
    _X(VE_ALLOC_FAIL, "Fatal: unable to allocate memory.") \
\
    X(VE_TOKENIZER_INVALID_STATE, "Bizarre internal error: tokenizer entered invalid state.") \
    X(VE_TOKENIZER_BAD_ESCAPE_SEQUENCE, "Syntax error: bad escape sequence in string.") \
    X(VE_TOKENIZER_BAD_NUMBER, "Syntax error: bad number.") \
    X(VE_TOKENIZER_UNTERMINATED_STRING_LITERAL, "Syntax error: unterminated string literal.") \
    X(VE_TOKENIZER_UNTERMINATED_RULE_NAME, "Syntax error: unterminated rule name.") \
    X(VE_TOKENIZER_BAD_RULE_NAME, "Sytax error: malformatted rule.") \
    X(VE_TOKENIZER_UNTERMINATED_MATCH_STRING, "Syntax error: unterminated match string literal.") \
    X(VE_TOKENIZER_UNEXPECTED_EOF, "Syntax error: unexpected end-of-file.") \
\
    X(VE_EXCEEDED_NESTED_VECTOR_LIMIT, "Syntax error: Too deeply nested vector.") \
    X(VE_UNMATCHED_VEC_OPEN, "Syntax error: unmatched '{'") \
    X(VE_UNMATCHED_VEC_CLOSE, "Syntax error: unmatched '}'") \
    X(VE_UNCLOSED_QUOT, "Syntax error: Unclosed quotation.") \
\
    X(VE_IO_FAIL, "IO Error: unknown.") \
    X(VE_CORRUPT_IMAGE, "Image appears to be corrupt.") \
\
    X(VE_STACK_EMPTY, "Attempt to pop empty stack.") \
    X(VE_STACK_OVERFLOW, "Stack overflow.") \
\
    X(VE_STRICT_TYPE_FAIL, "Type error: strict type check failure.") \
    X(VE_WRONG_TYPE, "Type error: unexpected or incorrect type.") \
    X(VE_NUMERIC_CONVERSION_FAIL, "Bizarre type error: unable to convert number to number.") \
\
    X(VE_DICTIONARY_ALLOC_FAIL, "Fatal: unable to allocate dictionary.") \
\
    X(VE_COULDNT_CREATE_VECTOR, "Failed to create a vector.") \
\
    X(VE_TOO_MANY_WORDS, "Too many words defined.") \
    X(VE_EXCEEDED_MAX_CALL_DEPTH, "Exceeded maximum call depth.") \
    X(VE_CALL_TO_UNDEFINED_WORD, "Attempted to call undefined word.") \
    X_(VE_BAD_OPCODE, "VM Error: Bad opcode.")

#define _DEF_ENUM(e,_d) e = 1,
#define DEF_ENUM(e,_d)  e,
#define DEF_ENUM_(e,_d) e

typedef enum {
    LIST_ERRORS(_DEF_ENUM, DEF_ENUM, DEF_ENUM_)
} vio_err_t;

#undef _DEF_ENUM
#undef DEF_ENUM
#undef DEF_ENUM_

const char *vio_err_msg(vio_err_t err);

/* not really intended for public use */
#define VIO__CHECK(expr) do{ if ((err = (expr))) goto error; }while(0)
#define VIO__ERRIF(expr, errcode) VIO__CHECK((expr) ? errcode : 0)
#define VIO__ENSURE_ATLEAST(n) \
    if (ctx->sp < n) { \
        err = vio_raise(ctx, VE_STACK_EMPTY, "Function requires at least " \
                        #n " operands, stack only has %d items.", ctx->sp); \
        goto error; \
    }


#endif
