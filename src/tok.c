#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "attrib.h"
#include "tok.h"

const char *vio_tok_type_name(vio_tok_t type) {
#define NAME(t) case vt_##t: return #t;

    switch (type) {
    LIST_TOK_TYPES(NAME, NAME, NAME)
    default: return "unknown token";
    }

#undef NAME
}

typedef enum { read, quot, esc, digit, dot, angle, btick, msesc, wordc, mod, fail, eof } ret_code;
typedef enum { bad_state, next, str, stresc, num, tagword, rule, mtstr, mtsesc, word, advb, conj, err, done } state_code;

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
static state_code transitions[][10] = {
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

void vio_tokenizer_init(vio_tokenizer *st) {
    st->i = 0;
    st->line = 0;
    st->pos = 0;
    st->t = NULL;
    st->fst = NULL;
    st->cs = next;
    st->slen = 0;
    st->err = 0;
}

void vio_tok_free(vio_tok *t) {
    if (t->s != NULL) {
        free(t->s);
        t->s = NULL;
    }
    t->next = NULL;
    free(t);
}

void vio_tok_free_all(vio_tok *t) {
    vio_tok *next;
    while (t) {
        next = t->next;
        vio_tok_free(t);
        t = next;
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
    t->next = NULL;

    return 0;

    error:
    if (t != NULL) vio_tok_free(t);
    return err;
}

vio_err_t vio_tokenize_str(vio_tok **t, const char *s, uint32_t len) {
    vio_err_t err = 0;
    vio_tokenizer st;
    vio_tokenizer_init(&st);
    st.s = s;
    st.len = len;
    err = vio_tokenize(&st);
    if (!err) *t = st.fst;
    else vio_tok_free_all(st.fst);
    return err;
}

vio_err_t vio_tokenize(vio_tokenizer *st) {
    state_fn f;
    ret_code rc;

    int isdone = 0;
    while (!isdone) {
        f = states[st->cs];
        rc = f(st);
        switch (rc) {
        case eof: isdone = 1; break;
        case fail: goto error;
        default:
            st->cs = transitions[st->cs][rc];
            if (st->cs == 0) {
                st->err = VE_TOKENIZER_INVALID_STATE;
                goto error;
            }
        }
        isdone = isdone || st->i >= st->len;
    }

    return 0;

    error:
    return st->err;
}

int can_start_mod(vio_tokenizer *s) {
    return s->i < s->len - 1 && !isspace(s->s[s->i + 1]) && !isdigit(s->s[s->i + 1]);
}

int is_mod_char(char c) {
    switch (c) {
    case '&': case '^': case '$':
    case '~': case '#': case '\\':
    case '/': case '@': case '%':
        return 1;
    default:
        return 0;
    }
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
    default:
        if (can_start_mod(s) && is_mod_char(s->s[s->i])) {
            ++s->i;
            SETNEXT(mod)
        }
        else SETNEXT(wordc)
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

VIO_CONST
int is_single_char_word(char c) {
    return c == '{' || c == '}' ||
           c == '[' || c == ']';
}

READER(tagword)
    s->slen = 0;
    while (s->i < s->len && !isspace(s->s[s->i])) {
        s->sbuf[s->slen++] = s->s[s->i++];
        if (is_single_char_word(s->s[s->i-1]))
            break;
    }
    vio_tok_new(vt_tagword, s, s->sbuf, s->slen);
    s->slen = 0;
    ret = read;
END_READER(tagword)

READER(word)
    s->slen = 0;
    if (is_single_char_word(s->s[s->i]))
        s->sbuf[s->slen++] = s->s[s->i++];
    else while (s->i < s->len && !isspace(s->s[s->i])) {
        if (s->s[s->i] == ':' && s->i < s->len - 1 && is_mod_char(s->s[s->i + 1])) {
            s->i += 2;
            ret = mod;
            break;
        }
        else if (is_single_char_word(s->s[s->i]))
            break;
        s->sbuf[s->slen++] = s->s[s->i++];
    }
    vio_tok_new(vt_word, s, s->sbuf, s->slen);
    s->slen = 0;
    if (ret == 0)
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
    	vio_tok_new(vt_adverb, s, s->s + s->i - 1, 1);
    	ret = read;
    }
END_READER(adverb)

READER(conj)
    if (s->i >= s->len) {
        s->err = VE_TOKENIZER_UNEXPECTED_EOF;
        ret = fail;
    }
    else {
    	vio_tok_new(vt_conj, s, s->s + s->i - 1, 1);
    	ret = read;
    }
END_READER(conj)
