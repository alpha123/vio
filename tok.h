#ifndef VIO_TOK_H
#define VIO_TOK_H

#include <stdint.h>
#include "error.h"

typedef enum { vt_str = 1, vt_num, vt_tagword, vt_rule, vt_matchstr, vt_word, vt_adverb, vt_conj } vio_tok_t;

typedef struct _vtok {
    vio_tok_t what;
    uint32_t line;
    uint32_t pos;
    uint32_t len;
    char *s;
    struct _vtok *next;
} vio_tok;

void vio_tok_free_all(vio_tok *t);

vio_err_t vio_tokenize(vio_tok **t, const char *s, uint32_t len);

#endif
