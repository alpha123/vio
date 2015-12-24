#ifndef VIO_TOK_H
#define VIO_TOK_H

#include <stdint.h>
#include "error.h"

#define LIST_TOK_TYPES(_X, X, X_) \
    _X(str) \
    X(num) \
    X(tagword) \
    X(rule) \
    X(matchstr) \
    X(word) \
    X(adverb) \
    X_(conj)

typedef enum {
#define _DEF_ENUM(t) vt_##t = 1,
#define DEF_ENUM(t)  vt_##t,
#define DEF_ENUM_(t) vt_##t

    LIST_TOK_TYPES(_DEF_ENUM, DEF_ENUM, DEF_ENUM_)

#undef _DEF_ENUM
#undef DEF_ENUM
#undef DEF_ENUM_
} vio_tok_t;

const char *vio_tok_type_name(vio_tok_t type);

typedef struct _vtok {
    vio_tok_t what;
    uint32_t line;
    uint32_t pos;
    uint32_t len;
    char *s;
    struct _vtok *next;
} vio_tok;

vio_err_t vio_tok_clone(vio_tok *t, vio_tok **out);

void vio_tok_free(vio_tok *t);
void vio_tok_free_all(vio_tok *t);

/* maximum size of words, rules, and string literals */
#define MAX_LITERAL_SIZE 255

typedef struct _vtokenizer {
    uint32_t i;
    uint32_t line;
    uint32_t pos;
    const char *s;
    uint32_t len;
    vio_tok *t;
    vio_tok *fst;
    uint32_t cs; /* actually a state_code */
    vio_err_t err;

    char sbuf[MAX_LITERAL_SIZE];
    uint32_t slen;
} vio_tokenizer;

void vio_tokenizer_init(vio_tokenizer *st);

vio_err_t vio_tokenize(vio_tokenizer *st);
vio_err_t vio_tokenize_str(vio_tok **t, const char *s, uint32_t len);

/* Transform a token sequence into a null-terminated string. */
char *vio_untokenize(vio_tok *t);

#endif
