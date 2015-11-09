#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "tok.h"

/* maximum size of words, rules, and string literals */
#define MAX_LITERAL_SIZE 255

typedef enum { read, quot, esc, digit, dot, angle, btick, msesc, wordc, mod, fail, eof } ret_code;
typedef enum { bad_state, next, str, stresc, num, tagword, rule, mtstr, mtsesc, word, advb, conj, err, done } state_code;

typedef struct _tokenizer {
    uint32_t i;
    uint32_t line;
    uint32_t pos;
    const char *s;
    uint32_t len;
    vio_tok *t;
    vio_tok *fst;
    state_code cs;
    vio_err_t err;

    char sbuf[MAX_LITERAL_SIZE];
    uint32_t slen;
} vio_tokenizer;

typedef ret_code (* state_fn)(vio_tokenizer *);

/* these MUST be in the same order as their corresponding state_codes */
#define LIST_READERS(X, X_) \
    X(begin) \
    X(str) \
    X(stresc) \
    X(num) \
    X(tagword) \
    X(rule) \
    X(matchstr) \
    X(matchstresc) \
    X(word) \
    X(adverb) \
    X_(conj)

#define DEFREADER(name) \
    ret_code read_##name(vio_tokenizer *s);

LIST_READERS(DEFREADER, DEFREADER)

#undef DEFREADER

#define DECLSTATES(name) read_##name,
#define DECLSTATES_(name) read_##name

state_fn states[] = {
    0, /* never called - state_code 0 is bad_state */
    LIST_READERS(DECLSTATES, DECLSTATES_)
};

#undef DECLSTATES
#undef DECLSTATES_

/* Set up our state machine. 0 means we (should) never enter that state (and error if we do). */
state_code transitions[][10] = {
    /*		 read	quot	esc	digit	dot	angle	btick	msesc	wordc	mod */
    [bad_state] = {0,	0,	0,	0,	0,	0,	0,	0,	0,	0},
    [next]   =	{next,	str,	0,	num,	tagword,rule,	mtstr,	0,	word,	conj},
    [str]    =	{next,	next,	stresc,	0,	0,	0,	0,	0,	word,	0},
    [stresc] =	{str,	0,	0,	0,	0,	0,	0,	0,	0,	0},
    [num]    =	{next,	str,	0, 	num,	num,	rule,	mtstr,	0,	word,	conj},
    [tagword]=	{next,	str,	0,	0,	tagword,0,	mtstr,	0,	0,	0},
    [rule]   =	{next,	0,	0, 	0,	0,	next,	0,	mtsesc,	0,	advb},
    [mtstr]  =	{next,	0,	0,	0,	0,	0,	next,	0,	word,	0},
    [mtsesc] =	{mtstr,	0,	0,	0,	0,	0,	0,	0,	0,	0},
    [word]   =	{next,	str,	0, 	0,	0,	0,	mtstr,	0,	word,	advb},
    [advb]   =	{next,	str,	0, 	num,	0,	0,	mtstr,	0,	advb,	advb},
    [conj]   =	{next,	str, 	0,	num,	0,	0,	mtstr,	0,	conj,	conj}
};


void vio_tok_free(vio_tok *t) {
    if (t->s != NULL)
        free((void *)t->s);
    free(t);
}

void vio_tok_free_all(vio_tok *t) {
    while (t) {
        vio_tok_free(t);
        t = t->next;
    }
}

vio_err_t vio_tok_new(vio_tok_t type, vio_tokenizer *s, const char *str, uint32_t len) {
    vio_err_t err = 0;
    vio_tok *t = (vio_tok *)malloc(sizeof(vio_tok));
    if (t == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }

    t->what = type;
    t->line = s->line;
    t->pos = s->i;
    t->s = (char *)malloc(sizeof(char) * len);
    if (t->s == NULL) {
        err = VE_ALLOC_FAIL;
        goto error;
    }
    strncpy(t->s, str, len);
    t->len = len;

    if (s->fst == NULL)
        s->fst = t;
    else
        s->t->next = t;
    s->t = t;

    return 0;

    error:
    if (t != NULL) vio_tok_free(t);
    return err;
}


vio_err_t vio_tokenize(vio_tok **t, const char *s, uint32_t len) {
    vio_tokenizer st;
    st.i = 0;
    st.line = 0;
    st.pos = 0;
    st.s = s;
    st.len = len;
    st.t = NULL;
    st.fst = NULL;
    st.cs = next;
    st.slen = 0;
    st.err = 0;

    state_fn f;
    ret_code rc;

    int isdone = 0;
    while (!isdone) {
        f = states[st.cs];
        rc = f(&st);
        switch (rc) {
        case eof: isdone = 1; break;
        case fail: goto error;
        default:
            st.cs = transitions[st.cs][rc];
            if (st.cs == 0) {
                st.err = VE_TOKENIZER_INVALID_STATE;
                goto error;
            }
        }
        isdone = st.i >= st.len;
    }

    *t = st.fst;
    return 0;

    error:
    vio_tok_free_all(st.fst);
    return st.err;
}

#define READER(name) \
    ret_code read_##name(vio_tokenizer *s) { \
        ret_code ret = 0;

#define END_READER(name) \
        return ret; \
    }

#define SETNEXT(st) ret = st; break;

READER(begin)
    if (s->i >= s->len)
        ret = eof;
    else if (isdigit(s->s[s->i]))
        ret = digit;
    else if (isspace(s->s[s->i])) {
        ++s->i;
        ret = read;
    }
    else switch (s->s[s->i]) {
    case '"': ++s->i; SETNEXT(quot)
    case '.': ++s->i; SETNEXT(dot)
    case '<': ++s->i; SETNEXT(angle)
    case '`': ++s->i; SETNEXT(btick)
    case ':': ++s->i; SETNEXT(mod)
    default : SETNEXT(wordc)
    }
END_READER(begin)

READER(str)
    while (1) {
        if (s->i >= s->len) {
            s->err = VE_TOKENIZER_UNTERMINATED_STRING_LITERAL;
            SETNEXT(fail)
        }
        else if (s->s[s->i] == '"') {
            ++s->i;
            SETNEXT(read)
        }
        else if (s->s[s->i] == '\\') {
            ++s->i;
            SETNEXT(esc)
        }
        else
            s->sbuf[s->slen++] = s->s[s->i++];
    }
    if (ret == read) {
        vio_tok_new(vt_str, s, s->sbuf, s->slen);
        s->slen = 0;
    }
END_READER(str)

#define ESCCHAR(e,c) case e: s->sbuf[s->slen++] = c; ++s->i; break;

READER(stresc)
    switch (s->s[s->i]) {
    ESCCHAR('t', '\t')
    ESCCHAR('n', '\n')
    ESCCHAR('r', '\r')
    ESCCHAR('"', '"')
    ESCCHAR('\\', '\\')
    default:
        s->err = VE_TOKENIZER_BAD_ESCAPE_SEQUENCE;
        SETNEXT(fail)
    }
END_READER(stresc)

READER(num)
    uint32_t start = s->i;
    while (s->i < s->len && isdigit(s->s[s->i])) ++s->i;
    if (s->i < s->len && s->s[s->i] == '.') {
        ++s->i;
        if (!isdigit(s->s[s->i])) {
            s->err = VE_TOKENIZER_BAD_NUMBER;
            ret = fail;
        }
        while (s->i < s->len && isdigit(s->s[s->i])) ++s->i;
    }
    if (ret != fail) {
        vio_tok_new(vt_num, s, s->s + start, s->i - start);
        ret = read;
    }
END_READER(num)

READER(tagword)
    ret = read_word(s);
    s->t->what = vt_tagword;
END_READER(tagword)

READER(word)
    s->slen = 0;
    while (!isspace(s->s[s->i]))
        s->sbuf[s->slen++] = s->s[s->i++];
    vio_tok_new(vt_word, s, s->sbuf, s->slen);
    s->slen = 0;
    ret = read;
END_READER(word)

READER(rule)
    s->slen = 0;
    while (1) {
        if (s->i >= s->len) {
            s->err = VE_TOKENIZER_UNTERMINATED_RULE_NAME;
            SETNEXT(fail)
        }
        else if (s->s[s->i] == '>') {
            ++s->i;
            SETNEXT(read)
        }
        else if (isspace(s->s[s->i])) {
            s->err = VE_TOKENIZER_BAD_RULE_NAME;
            SETNEXT(fail)
        }
        else
            s->sbuf[s->slen++] = s->s[s->i++];
    }
    if (ret == read)
        vio_tok_new(vt_rule, s, s->sbuf, s->slen);
    s->slen = 0;
END_READER(rule)

READER(matchstr)
    while (1) {
        if (s->i >= s->len) {
            s->err = VE_TOKENIZER_UNTERMINATED_MATCH_STRING;
            SETNEXT(fail)
        }
        else if (s->s[s->i] == '`') {
            ++s->i;
            SETNEXT(read)
        }
        else if (s->s[s->i] == '\\') {
            ++s->i;
            SETNEXT(msesc)
        }
        else
            s->sbuf[s->slen++] = s->s[s->i++];
    }
    if (ret == read) {
        vio_tok_new(vt_matchstr, s, s->sbuf, s->slen);
        s->slen = 0;
    }
END_READER(matchstr)

READER(matchstresc)
    return read_stresc(s);
END_READER(matchstresc)

READER(adverb)
    if (s->i >= s->len) {
        s->err = VE_TOKENIZER_UNEXPECTED_EOF;
        ret = fail;
    }
    else {
    	vio_tok_new(vt_adverb, s, s->s + s->i++, 1);
    	ret = word;
    }
END_READER(adverb)

READER(conj)
    if (s->i >= s->len) {
        s->err = VE_TOKENIZER_UNEXPECTED_EOF;
        ret = fail;
    }
    else {
    	vio_tok_new(vt_conj, s, s->s + s->i++, 1);
    	ret = read;
    }
END_READER(conj)
