/* Generated by tpc version 0.6.1 */

#define IS_ERROR(action) ((action) == 0)
#define IS_ACCEPT(action) ((action) == 19)
#define IS_REDUCE(action) (0 < (action) && (action) < 7)
#define IS_SHIFT(action) (7 <= (action) && (action) < 19)
#define REDUCTION(action) (action)
#define REDUCE_GOTO(state, production) \
    (goto_table[state][production -> nonterm_type])
#define SHIFT_GOTO(action) ((action) - 7)

typedef enum
{
    TT_EOF = 0,
    TT_LPAREN,
    TT_RPAREN,
    TT_DOT,
    TT_ATOM
} terminal_t;

struct production
{
    reduction_t reduction;
    int nonterm_type;
    int count;
};

static struct production productions[7] =
{
    /* 0: <START> ::= <expression> */
    { identity, 0, 1 },

    /* 1: <expression> ::= LPAREN <expression-list> RPAREN */
    { make_list, 1, 3 },

    /* 2: <expression> ::= LPAREN <expression-list> DOT <expression> RPAREN */
    { make_dot_list, 1, 5 },

    /* 3: <expression> ::= LPAREN RPAREN */
    { make_nil, 1, 2 },

    /* 4: <expression> ::= ATOM */
    { identity, 1, 1 },

    /* 5: <expression-list> ::= <expression-list> <expression> */
    { extend_cons, 2, 2 },

    /* 6: <expression-list> ::= <expression> */
    { make_cons, 2, 1 }
};

#define ERR 0
#define ACC 19
#define R(x) (x)
#define S(x) (x + 7)

static unsigned int sr_table[12][5] =
{
    { ERR, S(2), ERR, ERR, S(3) },
    { ACC, ERR, ERR, ERR, ERR },
    { ERR, S(2), S(6), ERR, S(3) },
    { R(4), R(4), R(4), R(4), R(4) },
    { ERR, R(6), R(6), R(6), R(6) },
    { ERR, S(2), S(8), S(9), S(3) },
    { R(3), R(3), R(3), R(3), R(3) },
    { ERR, R(5), R(5), R(5), R(5) },
    { R(1), R(1), R(1), R(1), R(1) },
    { ERR, S(2), ERR, ERR, S(3) },
    { ERR, ERR, S(11), ERR, ERR },
    { R(2), R(2), R(2), R(2), R(2) }
};

#undef ERR
#undef R
#undef S

static unsigned int goto_table[12][3] =
{
    { 0, 1, 0 },
    { 0, 0, 0 },
    { 0, 4, 5 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 7, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 10, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 }
};

