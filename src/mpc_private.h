#ifndef MPC_PRIVATE_H
#define MPC_PRIVATE_H

#include "mpc.h"

/* We need access to mpc's internal definitions
   in order to serialize parsers. */

enum {
    MPC_TYPE_UNDEFINED = 0,
    MPC_TYPE_PASS      = 1,
    MPC_TYPE_FAIL      = 2,
    MPC_TYPE_LIFT      = 3,
    MPC_TYPE_LIFT_VAL  = 4,
    MPC_TYPE_EXPECT    = 5,
    MPC_TYPE_ANCHOR    = 6,
    MPC_TYPE_STATE     = 7,

    MPC_TYPE_ANY       = 8,
    MPC_TYPE_SINGLE    = 9,
    MPC_TYPE_ONEOF     = 10,
    MPC_TYPE_NONEOF    = 11,
    MPC_TYPE_RANGE     = 12,
    MPC_TYPE_SATISFY   = 13,
    MPC_TYPE_STRING    = 14,

    MPC_TYPE_APPLY     = 15,
    MPC_TYPE_APPLY_TO  = 16,
    MPC_TYPE_PREDICT   = 17,
    MPC_TYPE_NOT       = 18,
    MPC_TYPE_MAYBE     = 19,
    MPC_TYPE_MANY      = 20,
    MPC_TYPE_MANY1     = 21,
    MPC_TYPE_COUNT     = 22,

    MPC_TYPE_OR        = 23,
    MPC_TYPE_AND       = 24
};

typedef struct { char *m; } mpc_pdata_fail_t;
typedef struct { mpc_ctor_t lf; void *x; } mpc_pdata_lift_t;
typedef struct { mpc_parser_t *x; char *m; } mpc_pdata_expect_t;
typedef struct { int(*f)(char,char); } mpc_pdata_anchor_t;
typedef struct { char x; } mpc_pdata_single_t;
typedef struct { char x; char y; } mpc_pdata_range_t;
typedef struct { int(*f)(char); } mpc_pdata_satisfy_t;
typedef struct { char *x; } mpc_pdata_string_t;
typedef struct { mpc_parser_t *x; mpc_apply_t f; } mpc_pdata_apply_t;
typedef struct { mpc_parser_t *x; mpc_apply_to_t f; void *d; } mpc_pdata_apply_to_t;
typedef struct { mpc_parser_t *x; } mpc_pdata_predict_t;
typedef struct { mpc_parser_t *x; mpc_dtor_t dx; mpc_ctor_t lf; } mpc_pdata_not_t;
typedef struct { int n; mpc_fold_t f; mpc_parser_t *x; mpc_dtor_t dx; } mpc_pdata_repeat_t;
typedef struct { int n; mpc_parser_t **xs; } mpc_pdata_or_t;
typedef struct { int n; mpc_fold_t f; mpc_parser_t **xs; mpc_dtor_t *dxs;  } mpc_pdata_and_t;

typedef union {
    mpc_pdata_fail_t fail;
    mpc_pdata_lift_t lift;
    mpc_pdata_expect_t expect;
    mpc_pdata_anchor_t anchor;
    mpc_pdata_single_t single;
    mpc_pdata_range_t range;
    mpc_pdata_satisfy_t satisfy;
    mpc_pdata_string_t string;
    mpc_pdata_apply_t apply;
    mpc_pdata_apply_to_t apply_to;
    mpc_pdata_predict_t predict;
    mpc_pdata_not_t not;
    mpc_pdata_repeat_t repeat;
    mpc_pdata_and_t and;
    mpc_pdata_or_t or;
} mpc_pdata_t;

struct mpc_parser_t {
    char retained;
    char *name;
    char type;
    mpc_pdata_t data;
};

#endif
