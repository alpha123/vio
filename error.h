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
    X(VE_IO_FAIL, "IO Error: unknown.") \
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
    X(VE_EXCEEDED_MAX_CALL_DEPTH, "Exceeded maximum call depth.") \
    X(VE_CALL_TO_UNDEFINED_WORD, "Attempt to call undefined word.") \
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

#endif
