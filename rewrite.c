#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rewrite.h"

#define MAX_VECTOR_NEST 32

#define is_short_word(t) ((t)->what == vt_word && (t)->len == 1)
#define is_vec_start(t) (is_short_word(t) && (t)->s[0] == '{')
#define is_vec_end(t) (is_short_word(t) && (t)->s[0] == '}')

/* rewrite vector literals `{a b c}` into `a b c 3 vec` */
vio_err_t rewrite_vec(vio_tok **begin) {
    vio_err_t err = 0;
    unsigned int vs_start[MAX_VECTOR_NEST], *vecsize;
    vecsize = vs_start;
    vio_tok *t = *begin, *tmp, *last = NULL;
    while (t) {
        if (is_vec_start(t)) {
            VIO__ERRIF(vecsize - MAX_VECTOR_NEST == vs_start, VE_EXCEEDED_NESTED_VECTOR_LIMIT);
            /* handle vectors of vectors */
            if (vecsize != vs_start)
                ++*vecsize;
            *++vecsize = 0;
            tmp = t;
            if (last)
                last->next = t->next;
            else
                *begin = t->next;
            t = t->next;
            vio_tok_free(tmp);
        }
        else if (is_vec_end(t)) {
            VIO__ERRIF(vecsize == vs_start, VE_UNMATCHED_VEC_CLOSE);
            tmp = t->next;
            free(t->s);
            t->what = vt_num;
            t->len = floor(log10(*vecsize)) + 1;
            t->s = (char *)malloc(t->len+1);
            snprintf(t->s, t->len+1, "%d", *vecsize);
            t->next = (vio_tok *)malloc(sizeof(vio_tok));
            VIO__ERRIF(t->next == NULL, VE_ALLOC_FAIL);
            t->next->what = vt_word;
            t->next->line = t->line;
            t->next->pos = t->pos;
            t->next->len = 3;
            t->next->s = (char *)malloc(3);
            strncpy(t->next->s, "vec", 3);
            t->next->next = tmp;
            --vecsize;
            last = t->next;
            t = tmp;
        }
        else if (vecsize != vs_start) {
            ++*vecsize;
            last = t;
            t = t->next;
        }
        else { /* just copying regular tokens */
            last = t;
            t = t->next;
        }
    }
    VIO__ERRIF(vecsize != vs_start, VE_UNMATCHED_VEC_OPEN);
    return 0;

    error:
    return err;
}

vio_err_t vio_rewrite(vio_tok **t) {
    return rewrite_vec(t);
}
