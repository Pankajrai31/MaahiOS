/**
 * MaahiOS JavaScript Engine - libjs.h
 *
 * Description:
 *   Minimal tree-walking JavaScript interpreter for MaahiOS browser.
 *   Supports: variables, numbers, strings, booleans, null, undefined,
 *   arithmetic, comparisons, if/else, for/while, functions, objects
 *   (property access), arrays (basic), document.write(), console.log().
 *
 *   Freestanding — no libc dependency.
 *   Layer 2 (Library). Ring 3.
 *
 * Usage:
 *   js_env_t env;
 *   js_init(&env, output_buf, output_max);
 *   js_exec(&env, script_text, script_len);
 *   // env.out_buf now contains any document.write() output
 */

#ifndef LIBJS_H
#define LIBJS_H

#include <stdint.h>

/*=============================================================================
 * LIMITS
 *===========================================================================*/

#define JS_MAX_VARS       128    /* Max variables per scope */
#define JS_MAX_DEPTH      32     /* Max call/scope depth */
#define JS_MAX_STR_POOL   16384  /* String pool size */
#define JS_MAX_NODES       2048  /* Max AST nodes */
#define JS_MAX_TOKENS      4096  /* Max tokens */
#define JS_MAX_PROPS       32    /* Max properties per object */
#define JS_MAX_OBJECTS     64    /* Max objects alive at once */
#define JS_MAX_FUNCS       32    /* Max function definitions */

/*=============================================================================
 * VALUE TYPES
 *===========================================================================*/

typedef enum {
    JS_UNDEFINED = 0,
    JS_NULL,
    JS_BOOL,
    JS_NUMBER,
    JS_STRING,
    JS_OBJECT,
    JS_FUNCTION,
} js_type_t;

typedef struct {
    js_type_t type;
    union {
        double   num;
        int      boolean;
        uint16_t str_off;    /* offset into string pool */
        uint16_t obj_id;     /* object table index */
        uint16_t func_id;    /* function table index */
    };
} js_val_t;

/*=============================================================================
 * OBJECT (property bag)
 *===========================================================================*/

typedef struct {
    uint16_t key_off;     /* string pool offset for property name */
    js_val_t value;
} js_prop_t;

typedef struct {
    js_prop_t props[JS_MAX_PROPS];
    int       prop_count;
    int       in_use;
} js_object_t;

/*=============================================================================
 * VARIABLE BINDING
 *===========================================================================*/

typedef struct {
    uint16_t name_off;   /* string pool offset */
    js_val_t value;
} js_var_t;

/*=============================================================================
 * SCOPE
 *===========================================================================*/

typedef struct {
    js_var_t vars[JS_MAX_VARS];
    int      var_count;
} js_scope_t;

/*=============================================================================
 * TOKEN TYPES
 *===========================================================================*/

typedef enum {
    /* Literals */
    TOK_NUM, TOK_STR, TOK_IDENT, TOK_TRUE, TOK_FALSE, TOK_NULL_TOK,
    /* Keywords */
    TOK_VAR, TOK_LET, TOK_CONST, TOK_IF, TOK_ELSE, TOK_FOR, TOK_WHILE,
    TOK_FUNCTION, TOK_RETURN, TOK_NEW, TOK_TYPEOF, TOK_BREAK, TOK_CONTINUE,
    /* Operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_NEQ, TOK_SEQ, TOK_SNEQ,   /* == != === !== */
    TOK_LT, TOK_GT, TOK_LTE, TOK_GTE,
    TOK_AND, TOK_OR, TOK_NOT,              /* && || ! */
    TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN,
    TOK_PLUS_PLUS, TOK_MINUS_MINUS,
    /* Delimiters */
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_SEMI, TOK_COMMA, TOK_DOT, TOK_COLON, TOK_QUESTION,
    /* Special */
    TOK_EOF,
} js_tok_type_t;

typedef struct {
    js_tok_type_t type;
    uint16_t      str_off;   /* string pool offset for STR/IDENT/NUM text */
    uint16_t      str_len;
    double        num_val;   /* parsed numeric value for TOK_NUM */
} js_token_t;

/*=============================================================================
 * AST NODE TYPES
 *===========================================================================*/

typedef enum {
    NODE_NUM_LIT, NODE_STR_LIT, NODE_BOOL_LIT, NODE_NULL_LIT,
    NODE_IDENT,
    NODE_BINARY,       /* left op right */
    NODE_UNARY,        /* op operand */
    NODE_ASSIGN,       /* target = value */
    NODE_VAR_DECL,     /* var/let/const name = init */
    NODE_IF,           /* cond, then_body, else_body */
    NODE_WHILE,        /* cond, body */
    NODE_FOR,          /* init, cond, update, body */
    NODE_BLOCK,        /* statement list */
    NODE_EXPR_STMT,    /* expression as statement */
    NODE_CALL,         /* callee(args) */
    NODE_MEMBER,       /* object.property */
    NODE_INDEX,        /* object[index] */
    NODE_FUNC_DEF,     /* function name(params) { body } */
    NODE_RETURN,       /* return expr */
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_TYPEOF,       /* typeof expr */
    NODE_TERNARY,      /* cond ? then : else */
    NODE_ARRAY_LIT,    /* [elements] */
    NODE_OBJECT_LIT,   /* {key: value, ...} */
    NODE_COMPOUND_ASSIGN, /* += -= */
    NODE_UPDATE,       /* ++x, x++ */
} js_node_type_t;

typedef struct {
    js_node_type_t type;
    uint16_t       str_off;   /* for ident/string literals */
    uint16_t       str_len;
    double         num_val;   /* for number literals */
    int            bool_val;
    uint8_t        op;        /* operator for binary/unary (js_tok_type_t) */
    int16_t        left;      /* child node index (-1 = none) */
    int16_t        right;     /* child node index (-1 = none) */
    int16_t        extra;     /* else branch / for-update / array next */
    int16_t        extra2;    /* for-body */
} js_node_t;

/*=============================================================================
 * FUNCTION DEFINITION
 *===========================================================================*/

typedef struct {
    uint16_t name_off;       /* function name in string pool */
    uint16_t param_offs[8];  /* parameter name offsets (max 8 params) */
    int      param_count;
    int16_t  body_node;      /* AST node index of function body */
    int      in_use;
} js_func_t;

/*=============================================================================
 * INTERPRETER ENVIRONMENT
 *===========================================================================*/

typedef struct {
    /* String pool */
    char     str_pool[JS_MAX_STR_POOL];
    int      str_pool_used;

    /* Tokens (reusable between parse calls) */
    js_token_t tokens[JS_MAX_TOKENS];
    int        token_count;
    int        tok_pos;  /* current token index during parsing */

    /* AST nodes */
    js_node_t  nodes[JS_MAX_NODES];
    int        node_count;

    /* Scope stack */
    js_scope_t scopes[JS_MAX_DEPTH];
    int        scope_depth;

    /* Objects */
    js_object_t objects[JS_MAX_OBJECTS];

    /* Functions */
    js_func_t  funcs[JS_MAX_FUNCS];

    /* Output buffer (document.write output) */
    char      *out_buf;
    int        out_len;
    int        out_max;

    /* Execution state */
    int        running;   /* 0 = stopped (error/done) */
    int        returning; /* return flag */
    int        breaking;  /* break flag */
    int        continuing;/* continue flag */
    js_val_t   ret_val;   /* return value */

    /* Error */
    char       error[128];
} js_env_t;

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

/**
 * js_init — Initialize JS environment
 * @env:      Environment to initialize
 * @out_buf:  Buffer for document.write() output
 * @out_max:  Size of output buffer
 */
void js_init(js_env_t *env, char *out_buf, int out_max);

/**
 * js_exec — Execute JavaScript source code
 * @env:      Initialized environment (state carries across calls)
 * @src:      JavaScript source text
 * @src_len:  Length of source text
 * @return 0 on success, -1 on error (check env->error)
 */
int js_exec(js_env_t *env, const char *src, int src_len);

/**
 * js_set_element_text_callback — Register callback for DOM element updates
 * @env:  Environment
 * @cb:   Callback(id_string, text, text_len, ctx)
 * @ctx:  User context pointer
 */
typedef void (*js_dom_write_cb)(const char *id, const char *text, int len, void *ctx);
void js_set_dom_callback(js_env_t *env, js_dom_write_cb cb, void *ctx);

#endif /* LIBJS_H */
