#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "rewrite.h"

/* Coincidentally, this file probably needs to be rewritten. ;-) */

#define MAX_VECTOR_NEST 32

#define is_short_word(t) ((t)->what == vt_word && (t)->len == 1)
#define is_short_comma_combinator(t) ((t)->what == vt_word && (t)->s[0] == ',' && (t)->len > 3)
#define is_comma_rule(t) (is_short_comma_combinator(t) && (t)->s[1] == '<')
#define is_comma_matchstr(t) (is_short_comma_combinator(t) && (t)->s[1] == '`')

#define simple_is_vec_start(t) (is_short_word(t) && (t)->s[0] == '{')
#define simple_is_vec_end(t) (is_short_word(t) && (t)->s[0] == '}')
#define simple_is_tagvec_start(t) ((t)->what == vt_tagword && (t)->len > 1 && \
    (t)->s[(t)->len-1] == '{')

typedef vio_err_t (* ins_action)(vio_tok **, uint32_t, const char *);
typedef int (* bounds_condition)(vio_tok *, void *);

vio_err_t replacetok(vio_tok **t, uint32_t swlen, const char *start_word) {
    vio_tok *u = *t;
    free(u->s);
    u->len = swlen;
    u->s = (char *)malloc(swlen);
    if (u->s == NULL)
        return VE_ALLOC_FAIL;
    strncpy(u->s, start_word, swlen);
    return 0;
}

vio_err_t inserttag(vio_tok **t, uint32_t swlen, const char *start_word) {
    vio_tok *u = *t, *v, *tmp = (*t)->next;
    u->len -= 1; /* remove { */
    v = u->next = (vio_tok *)malloc(sizeof(vio_tok));
    if (v == NULL)
        return VE_ALLOC_FAIL;
    v->what = vt_word;
    v->line = u->line;
    v->pos = u->pos;
    v->len = swlen;
    v->s = (char *)malloc(swlen);
    if (v->s == NULL) {
        free(v);
        return VE_ALLOC_FAIL;
    }
    strncpy(v->s, start_word, swlen);
    v->next = tmp;
    *t = v;
    return 0;
}

int is_vec_start(vio_tok *t, void *_data) {
    return simple_is_vec_start(t);
}
int is_vec_end(vio_tok *t, void *_data) {
    return simple_is_vec_end(t);
}

struct tagword_state {
    int in_twp;
    int in_tw[MAX_VECTOR_NEST];
    vio_err_t err;
};

int is_tagvec_start(vio_tok *t, void *data) {
    struct tagword_state *tws = (struct tagword_state *)data;
    int res = simple_is_tagvec_start(t);
    if (tws->in_twp == MAX_VECTOR_NEST) {
        tws->err = VE_EXCEEDED_NESTED_VECTOR_LIMIT;
        return 0;
    }
    /* count { so is_tagvec_end knows if a } ends a tagword or a vector */
    if (res)
        tws->in_tw[tws->in_twp++] = 1;
    else if (is_vec_start(t, NULL))
        tws->in_tw[tws->in_twp++] = 0;
    return res;
}

int is_tagvec_end(vio_tok *t, void *data) {
    struct tagword_state *tws = (struct tagword_state *)data;
    if (is_vec_end(t, NULL) && tws->in_twp > 0)
        return tws->in_tw[--tws->in_twp];
    return 0;
}

/* rewrite match strings `foo`  into '"foo" match' */
vio_err_t rewrite_matchstr(vio_tok **begin) {
    vio_tok *t = *begin, *next;
    while (t) {
        if (t->what == vt_matchstr) {
            t->what = vt_str;
            next = t->next;
            t->next = (vio_tok *)malloc(sizeof(vio_tok));
            if (t->next == NULL)
                return VE_ALLOC_FAIL;
            t->next->what = vt_word;
            t->next->len = 5;
            t->next->s = (char *)malloc(5);
            if (t->next->s == NULL)
                return VE_ALLOC_FAIL;
            strncpy(t->next->s, "match", 5);
            t->next->next = next;
            t = next;
        }
        else
            t = t->next;
    }
    return 0;
}

/* rewrite ,<rule> and ,`str` into '<rule> ,' and '`str` ,' */
vio_err_t rewrite_short_comma_combinator(vio_tok **begin) {
    vio_tok *t = *begin, *next;
    char *rest;
    while (t) {
        if (is_comma_rule(t) || is_comma_matchstr(t)) {
            t->what = is_comma_rule(t) ? vt_rule : vt_matchstr;
            /* remove initial comma and surrounding brackets/backticks */
            rest = (char *)malloc(t->len - 3);
            strncpy(rest, t->s + 2, t->len - 3);
            free(t->s);
            t->s = rest;
            t->len -= 3;
            next = t->next;
            t->next = (vio_tok *)malloc(sizeof(vio_tok));
            if (t->next == NULL)
                return VE_ALLOC_FAIL;
            t->next->what = vt_word;
            t->next->len = 1;
            t->next->s = (char *)malloc(1);
            if (t->next->s == NULL)
                return VE_ALLOC_FAIL;
            t->next->s[0] = ',';
            t->next->next = next;
            t = next;
        }
        else
            t = t->next;
    }
    return 0;
}

vio_err_t rewrite_collection(vio_tok **begin,
                             bounds_condition is_start, bounds_condition is_end,
                             void *state_data, ins_action insert,
                             uint32_t swlen, const char *start_word,
                             uint32_t ewlen, const char *end_word) {
    vio_err_t err = 0;
    uint32_t vecnest = 0;
    vio_tok *t = *begin;
    while (t) {
        if (is_start(t, state_data)) {
            VIO__CHECK(insert(&t, swlen, start_word));
            ++vecnest;
        }
        else if (is_end(t, state_data)) {
            VIO__ERRIF(vecnest == 0, VE_UNMATCHED_VEC_CLOSE);
            free(t->s);
            t->len = ewlen;
            t->s = (char *)malloc(ewlen);
            strncpy(t->s, end_word, ewlen);
            --vecnest;
        }
        t = t->next;
    }
    VIO__ERRIF(vecnest != 0, VE_UNMATCHED_VEC_OPEN);
    return 0;

    error:
    return err;
}

/* rewrite vector literals {a b c} into 'vecstart a b c vecend' */
vio_err_t rewrite_vec(vio_tok **begin) {
    return rewrite_collection(begin, is_vec_start, is_vec_end, NULL,
                              replacetok, 8, "vecstart", 6, "vecend");
}

/* rewrite tagword vector part .tag{ a b } into .tag tagstart a b tagend */
vio_err_t rewrite_tag_vec(vio_tok **begin) {
    struct tagword_state tws;
    tws.err = 0;
    tws.in_twp = 0;
    return rewrite_collection(begin, is_tagvec_start, is_tagvec_end, &tws,
                              inserttag, 8, "tagstart", 6, "tagend") || tws.err;
}

vio_err_t vio_rewrite(vio_tok **t) {
    return rewrite_short_comma_combinator(t) || rewrite_matchstr(t) ||
        rewrite_tag_vec(t) || rewrite_vec(t);
}
