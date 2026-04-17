/**
 * MaahiOS JavaScript Engine - libjs.c
 *
 * Minimal tree-walking interpreter.
 * Lexer -> Parser -> AST -> Interpreter.
 *
 * Supported: var/let/const, numbers, strings, booleans, null, undefined,
 *   if/else, for, while, functions, return, break, continue,
 *   objects (property access), arrays (basic), typeof,
 *   arithmetic (+, -, *, /, %), comparisons, logical &&/||/!,
 *   document.write(), document.getElementById().innerHTML,
 *   string concatenation, console.log(), String.length, String.indexOf,
 *   Array.push, Array.length.
 *
 * Freestanding — no libc dependency.
 * Layer 2 (Library). Ring 3.
 */

#include "libjs.h"

/*=============================================================================
 * UTILITY (freestanding)
 *===========================================================================*/

static void js_memset(void *dst, int val, int n) {
    uint8_t *p = (uint8_t *)dst;
    for (int i = 0; i < n; i++) p[i] = (uint8_t)val;
}

static void js_memcpy(void *dst, const void *src, int n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (int i = 0; i < n; i++) d[i] = s[i];
}

static int js_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int js_strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

static int js_streq(const char *a, int alen, const char *b) {
    int blen = js_strlen(b);
    if (alen != blen) return 0;
    return js_strncmp(a, b, alen) == 0;
}

static int js_isdigit(char c) { return c >= '0' && c <= '9'; }
static int js_isalpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$'; }
static int js_isalnum(char c) { return js_isalpha(c) || js_isdigit(c); }
static int js_isspace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

static double js_atof(const char *s, int len) {
    double result = 0.0;
    int i = 0;
    int neg = 0;
    if (i < len && s[i] == '-') { neg = 1; i++; }
    while (i < len && js_isdigit(s[i])) {
        result = result * 10.0 + (s[i] - '0');
        i++;
    }
    if (i < len && s[i] == '.') {
        i++;
        double frac = 0.1;
        while (i < len && js_isdigit(s[i])) {
            result += (s[i] - '0') * frac;
            frac *= 0.1;
            i++;
        }
    }
    return neg ? -result : result;
}

/* Format double to string; returns length written */
static int js_ftoa(double val, char *buf, int max) {
    int pos = 0;
    if (val < 0) { if (pos < max) buf[pos++] = '-'; val = -val; }

    /* Check for integer */
    double int_part = 0;
    double frac_part = val;
    /* Floor */
    int ipart = (int)val;
    frac_part = val - (double)ipart;

    /* Write integer part */
    if (ipart == 0) {
        if (pos < max) buf[pos++] = '0';
    } else {
        char tmp[20]; int tn = 0;
        int v = ipart;
        while (v > 0) { tmp[tn++] = '0' + (v % 10); v /= 10; }
        for (int i = tn - 1; i >= 0 && pos < max; i--) buf[pos++] = tmp[i];
    }

    /* Write fractional part (up to 6 digits) if non-zero */
    if (frac_part > 0.0000005) {
        if (pos < max) buf[pos++] = '.';
        for (int d = 0; d < 6 && pos < max; d++) {
            frac_part *= 10.0;
            int digit = (int)frac_part;
            buf[pos++] = '0' + digit;
            frac_part -= digit;
            if (frac_part < 0.0000005) break;
        }
    }

    if (pos < max) buf[pos] = '\0';
    return pos;
}

static void js_set_error(js_env_t *env, const char *msg) {
    int i = 0;
    while (msg[i] && i < 126) { env->error[i] = msg[i]; i++; }
    env->error[i] = '\0';
    env->running = 0;
}

/*=============================================================================
 * STRING POOL
 *===========================================================================*/

static uint16_t sp_add(js_env_t *env, const char *s, int len) {
    if (env->str_pool_used + len + 1 > JS_MAX_STR_POOL) {
        js_set_error(env, "string pool full");
        return 0;
    }
    uint16_t off = (uint16_t)env->str_pool_used;
    js_memcpy(env->str_pool + off, s, len);
    env->str_pool[off + len] = '\0';
    env->str_pool_used += len + 1;
    return off;
}

static const char *sp_get(js_env_t *env, uint16_t off) {
    if (off >= env->str_pool_used) return "";
    return env->str_pool + off;
}

static int sp_len(js_env_t *env, uint16_t off) {
    return js_strlen(sp_get(env, off));
}

/* Find or add a string (dedup for identifiers) */
static uint16_t sp_intern(js_env_t *env, const char *s, int len) {
    /* Search existing strings */
    int pos = 0;
    while (pos < env->str_pool_used) {
        const char *existing = env->str_pool + pos;
        int elen = js_strlen(existing);
        if (elen == len && js_strncmp(existing, s, len) == 0) {
            return (uint16_t)pos;
        }
        pos += elen + 1;
    }
    return sp_add(env, s, len);
}

/*=============================================================================
 * OUTPUT
 *===========================================================================*/

static void out_append(js_env_t *env, const char *s, int len) {
    for (int i = 0; i < len && env->out_len < env->out_max - 1; i++) {
        env->out_buf[env->out_len++] = s[i];
    }
    env->out_buf[env->out_len] = '\0';
}

static void out_append_str(js_env_t *env, const char *s) {
    out_append(env, s, js_strlen(s));
}

/*=============================================================================
 * LEXER
 *===========================================================================*/

static void lex(js_env_t *env, const char *src, int src_len) {
    env->token_count = 0;
    int i = 0;

    while (i < src_len && env->token_count < JS_MAX_TOKENS - 1) {
        /* Skip whitespace */
        while (i < src_len && js_isspace(src[i])) i++;
        if (i >= src_len) break;

        js_token_t *t = &env->tokens[env->token_count];
        t->str_off = 0; t->str_len = 0; t->num_val = 0;

        char c = src[i];

        /* Single-line comment */
        if (c == '/' && i + 1 < src_len && src[i + 1] == '/') {
            while (i < src_len && src[i] != '\n') i++;
            continue;
        }
        /* Multi-line comment */
        if (c == '/' && i + 1 < src_len && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src_len && !(src[i] == '*' && src[i + 1] == '/')) i++;
            if (i + 1 < src_len) i += 2;
            continue;
        }

        /* Number */
        if (js_isdigit(c) || (c == '.' && i + 1 < src_len && js_isdigit(src[i + 1]))) {
            int start = i;
            if (c == '0' && i + 1 < src_len && (src[i + 1] == 'x' || src[i + 1] == 'X')) {
                /* Hex */
                i += 2;
                double hval = 0;
                while (i < src_len) {
                    char h = src[i];
                    if (h >= '0' && h <= '9') hval = hval * 16 + (h - '0');
                    else if (h >= 'a' && h <= 'f') hval = hval * 16 + (h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') hval = hval * 16 + (h - 'A' + 10);
                    else break;
                    i++;
                }
                t->type = TOK_NUM;
                t->num_val = hval;
            } else {
                while (i < src_len && (js_isdigit(src[i]) || src[i] == '.')) i++;
                t->type = TOK_NUM;
                t->num_val = js_atof(src + start, i - start);
            }
            t->str_off = sp_add(env, src + start, i - start);
            t->str_len = (uint16_t)(i - start);
            env->token_count++;
            continue;
        }

        /* String literal */
        if (c == '"' || c == '\'' || c == '`') {
            char quote = c;
            i++;
            int start = i;
            /* Build string handling escape sequences */
            char tmp[512];
            int tlen = 0;
            while (i < src_len && src[i] != quote && tlen < 510) {
                if (src[i] == '\\' && i + 1 < src_len) {
                    i++;
                    switch (src[i]) {
                        case 'n': tmp[tlen++] = '\n'; break;
                        case 't': tmp[tlen++] = '\t'; break;
                        case 'r': tmp[tlen++] = '\r'; break;
                        case '\\': tmp[tlen++] = '\\'; break;
                        case '\'': tmp[tlen++] = '\''; break;
                        case '"': tmp[tlen++] = '"';  break;
                        default: tmp[tlen++] = src[i]; break;
                    }
                } else {
                    tmp[tlen++] = src[i];
                }
                i++;
            }
            if (i < src_len) i++; /* skip closing quote */
            t->type = TOK_STR;
            t->str_off = sp_add(env, tmp, tlen);
            t->str_len = (uint16_t)tlen;
            env->token_count++;
            continue;
        }

        /* Identifier / keyword */
        if (js_isalpha(c)) {
            int start = i;
            while (i < src_len && js_isalnum(src[i])) i++;
            int len = i - start;

            /* Check keywords */
            if      (js_streq(src + start, len, "var"))       t->type = TOK_VAR;
            else if (js_streq(src + start, len, "let"))       t->type = TOK_LET;
            else if (js_streq(src + start, len, "const"))     t->type = TOK_CONST;
            else if (js_streq(src + start, len, "if"))        t->type = TOK_IF;
            else if (js_streq(src + start, len, "else"))      t->type = TOK_ELSE;
            else if (js_streq(src + start, len, "for"))       t->type = TOK_FOR;
            else if (js_streq(src + start, len, "while"))     t->type = TOK_WHILE;
            else if (js_streq(src + start, len, "function"))  t->type = TOK_FUNCTION;
            else if (js_streq(src + start, len, "return"))    t->type = TOK_RETURN;
            else if (js_streq(src + start, len, "true"))      t->type = TOK_TRUE;
            else if (js_streq(src + start, len, "false"))     t->type = TOK_FALSE;
            else if (js_streq(src + start, len, "null"))      t->type = TOK_NULL_TOK;
            else if (js_streq(src + start, len, "new"))       t->type = TOK_NEW;
            else if (js_streq(src + start, len, "typeof"))    t->type = TOK_TYPEOF;
            else if (js_streq(src + start, len, "break"))     t->type = TOK_BREAK;
            else if (js_streq(src + start, len, "continue"))  t->type = TOK_CONTINUE;
            else {
                t->type = TOK_IDENT;
            }

            t->str_off = sp_intern(env, src + start, len);
            t->str_len = (uint16_t)len;
            env->token_count++;
            continue;
        }

        /* Operators and delimiters */
        switch (c) {
        case '+':
            if (i + 1 < src_len && src[i+1] == '+') { t->type = TOK_PLUS_PLUS; i += 2; }
            else if (i + 1 < src_len && src[i+1] == '=') { t->type = TOK_PLUS_ASSIGN; i += 2; }
            else { t->type = TOK_PLUS; i++; }
            break;
        case '-':
            if (i + 1 < src_len && src[i+1] == '-') { t->type = TOK_MINUS_MINUS; i += 2; }
            else if (i + 1 < src_len && src[i+1] == '=') { t->type = TOK_MINUS_ASSIGN; i += 2; }
            else { t->type = TOK_MINUS; i++; }
            break;
        case '*': t->type = TOK_STAR; i++; break;
        case '/': t->type = TOK_SLASH; i++; break;
        case '%': t->type = TOK_PERCENT; i++; break;
        case '=':
            if (i + 2 < src_len && src[i+1] == '=' && src[i+2] == '=') { t->type = TOK_SEQ; i += 3; }
            else if (i + 1 < src_len && src[i+1] == '=') { t->type = TOK_EQ; i += 2; }
            else { t->type = TOK_ASSIGN; i++; }
            break;
        case '!':
            if (i + 2 < src_len && src[i+1] == '=' && src[i+2] == '=') { t->type = TOK_SNEQ; i += 3; }
            else if (i + 1 < src_len && src[i+1] == '=') { t->type = TOK_NEQ; i += 2; }
            else { t->type = TOK_NOT; i++; }
            break;
        case '<':
            if (i + 1 < src_len && src[i+1] == '=') { t->type = TOK_LTE; i += 2; }
            else { t->type = TOK_LT; i++; }
            break;
        case '>':
            if (i + 1 < src_len && src[i+1] == '=') { t->type = TOK_GTE; i += 2; }
            else { t->type = TOK_GT; i++; }
            break;
        case '&':
            if (i + 1 < src_len && src[i+1] == '&') { t->type = TOK_AND; i += 2; }
            else { i++; continue; } /* skip bitwise & */
            break;
        case '|':
            if (i + 1 < src_len && src[i+1] == '|') { t->type = TOK_OR; i += 2; }
            else { i++; continue; } /* skip bitwise | */
            break;
        case '(': t->type = TOK_LPAREN; i++; break;
        case ')': t->type = TOK_RPAREN; i++; break;
        case '{': t->type = TOK_LBRACE; i++; break;
        case '}': t->type = TOK_RBRACE; i++; break;
        case '[': t->type = TOK_LBRACKET; i++; break;
        case ']': t->type = TOK_RBRACKET; i++; break;
        case ';': t->type = TOK_SEMI; i++; break;
        case ',': t->type = TOK_COMMA; i++; break;
        case '.': t->type = TOK_DOT; i++; break;
        case ':': t->type = TOK_COLON; i++; break;
        case '?': t->type = TOK_QUESTION; i++; break;
        default:
            i++; /* skip unknown character */
            continue;
        }
        env->token_count++;
    }

    /* Append EOF */
    env->tokens[env->token_count].type = TOK_EOF;
    env->tokens[env->token_count].str_off = 0;
    env->tokens[env->token_count].str_len = 0;
    env->tokens[env->token_count].num_val = 0;
    env->token_count++;
}

/*=============================================================================
 * PARSER HELPERS
 *===========================================================================*/

static js_token_t *peek(js_env_t *env) {
    return &env->tokens[env->tok_pos];
}

static js_token_t *advance(js_env_t *env) {
    js_token_t *t = &env->tokens[env->tok_pos];
    if (t->type != TOK_EOF) env->tok_pos++;
    return t;
}

static int expect(js_env_t *env, js_tok_type_t type) {
    if (peek(env)->type != type) return 0;
    advance(env);
    return 1;
}

static int16_t alloc_node(js_env_t *env) {
    if (env->node_count >= JS_MAX_NODES) {
        js_set_error(env, "too many AST nodes");
        return -1;
    }
    int16_t idx = (int16_t)env->node_count++;
    js_node_t *n = &env->nodes[idx];
    js_memset(n, 0, sizeof(js_node_t));
    n->left = n->right = n->extra = n->extra2 = -1;
    return idx;
}

/*=============================================================================
 * PARSER — RECURSIVE DESCENT
 *===========================================================================*/

static int16_t parse_expr(js_env_t *env);
static int16_t parse_assign_expr(js_env_t *env);
static int16_t parse_stmt(js_env_t *env);
static int16_t parse_block(js_env_t *env);

/* Primary: literals, identifiers, (expr), [array], {object}, function */
static int16_t parse_primary(js_env_t *env) {
    js_token_t *t = peek(env);

    if (t->type == TOK_NUM) {
        advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_NUM_LIT;
        env->nodes[n].num_val = t->num_val;
        return n;
    }
    if (t->type == TOK_STR) {
        advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_STR_LIT;
        env->nodes[n].str_off = t->str_off;
        env->nodes[n].str_len = t->str_len;
        return n;
    }
    if (t->type == TOK_TRUE || t->type == TOK_FALSE) {
        int bval = (t->type == TOK_TRUE) ? 1 : 0;
        advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BOOL_LIT;
        env->nodes[n].bool_val = bval;
        return n;
    }
    if (t->type == TOK_NULL_TOK) {
        advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_NULL_LIT;
        return n;
    }
    if (t->type == TOK_IDENT) {
        advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_IDENT;
        env->nodes[n].str_off = t->str_off;
        env->nodes[n].str_len = t->str_len;
        return n;
    }
    if (t->type == TOK_LPAREN) {
        advance(env);
        int16_t n = parse_expr(env);
        expect(env, TOK_RPAREN);
        return n;
    }
    if (t->type == TOK_LBRACKET) {
        advance(env);
        /* Array literal: chain elements via extra */
        int16_t arr = alloc_node(env); if (arr < 0) return -1;
        env->nodes[arr].type = NODE_ARRAY_LIT;
        int16_t first = -1, prev = -1;
        while (peek(env)->type != TOK_RBRACKET && peek(env)->type != TOK_EOF) {
            int16_t elem = parse_assign_expr(env);
            if (elem < 0) break;
            if (first < 0) first = elem;
            if (prev >= 0) env->nodes[prev].extra = elem;
            prev = elem;
            if (peek(env)->type == TOK_COMMA) advance(env);
        }
        expect(env, TOK_RBRACKET);
        env->nodes[arr].left = first;
        return arr;
    }
    if (t->type == TOK_LBRACE) {
        advance(env);
        /* Object literal: store key-value pairs */
        int16_t obj = alloc_node(env); if (obj < 0) return -1;
        env->nodes[obj].type = NODE_OBJECT_LIT;
        int16_t first = -1, prev = -1;
        while (peek(env)->type != TOK_RBRACE && peek(env)->type != TOK_EOF) {
            /* key: value */
            js_token_t *key_tok = advance(env);
            expect(env, TOK_COLON);
            int16_t val = parse_assign_expr(env);
            /* Store as ident node with value in right */
            int16_t kv = alloc_node(env); if (kv < 0) return -1;
            env->nodes[kv].type = NODE_IDENT;
            env->nodes[kv].str_off = key_tok->str_off;
            env->nodes[kv].str_len = key_tok->str_len;
            env->nodes[kv].right = val;
            if (first < 0) first = kv;
            if (prev >= 0) env->nodes[prev].extra = kv;
            prev = kv;
            if (peek(env)->type == TOK_COMMA) advance(env);
        }
        expect(env, TOK_RBRACE);
        env->nodes[obj].left = first;
        return obj;
    }
    if (t->type == TOK_FUNCTION) {
        advance(env);
        uint16_t name_off = 0; uint16_t name_len = 0;
        if (peek(env)->type == TOK_IDENT) {
            js_token_t *nt = advance(env);
            name_off = nt->str_off; name_len = nt->str_len;
        }
        expect(env, TOK_LPAREN);
        int16_t fnode = alloc_node(env); if (fnode < 0) return -1;
        env->nodes[fnode].type = NODE_FUNC_DEF;
        env->nodes[fnode].str_off = name_off;
        env->nodes[fnode].str_len = name_len;
        /* Parse params — store in extra chain */
        int16_t first_param = -1, prev_param = -1;
        while (peek(env)->type != TOK_RPAREN && peek(env)->type != TOK_EOF) {
            if (peek(env)->type == TOK_IDENT) {
                js_token_t *pt = advance(env);
                int16_t pn = alloc_node(env); if (pn < 0) return -1;
                env->nodes[pn].type = NODE_IDENT;
                env->nodes[pn].str_off = pt->str_off;
                env->nodes[pn].str_len = pt->str_len;
                if (first_param < 0) first_param = pn;
                if (prev_param >= 0) env->nodes[prev_param].extra = pn;
                prev_param = pn;
            }
            if (peek(env)->type == TOK_COMMA) advance(env);
            else break;
        }
        expect(env, TOK_RPAREN);
        int16_t body = parse_block(env);
        env->nodes[fnode].left = first_param;  /* param list */
        env->nodes[fnode].right = body;        /* body */
        return fnode;
    }
    if (t->type == TOK_NOT) {
        advance(env);
        int16_t operand = parse_primary(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_UNARY;
        env->nodes[n].op = TOK_NOT;
        env->nodes[n].left = operand;
        return n;
    }
    if (t->type == TOK_MINUS) {
        advance(env);
        int16_t operand = parse_primary(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_UNARY;
        env->nodes[n].op = TOK_MINUS;
        env->nodes[n].left = operand;
        return n;
    }
    if (t->type == TOK_TYPEOF) {
        advance(env);
        int16_t operand = parse_primary(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_TYPEOF;
        env->nodes[n].left = operand;
        return n;
    }
    if (t->type == TOK_PLUS_PLUS || t->type == TOK_MINUS_MINUS) {
        uint8_t op = (uint8_t)t->type;
        advance(env);
        int16_t operand = parse_primary(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_UPDATE;
        env->nodes[n].op = op;
        env->nodes[n].left = operand;
        env->nodes[n].bool_val = 1; /* prefix */
        return n;
    }
    if (t->type == TOK_NEW) {
        advance(env);
        /* new Constructor(args) — create empty object */
        int16_t callee = parse_primary(env);
        /* Skip args if present */
        if (peek(env)->type == TOK_LPAREN) {
            advance(env);
            while (peek(env)->type != TOK_RPAREN && peek(env)->type != TOK_EOF) {
                parse_assign_expr(env);
                if (peek(env)->type == TOK_COMMA) advance(env);
            }
            expect(env, TOK_RPAREN);
        }
        /* Return an empty object node */
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_OBJECT_LIT;
        env->nodes[n].left = -1;
        return n;
    }

    /* Unknown token — skip it */
    advance(env);
    return -1;
}

/* Postfix: calls, member access, indexing, ++ -- */
static int16_t parse_postfix(js_env_t *env) {
    int16_t node = parse_primary(env);
    if (node < 0) return -1;

    for (;;) {
        if (peek(env)->type == TOK_DOT) {
            advance(env);
            if (peek(env)->type == TOK_IDENT) {
                js_token_t *prop = advance(env);
                int16_t mem = alloc_node(env); if (mem < 0) return -1;
                env->nodes[mem].type = NODE_MEMBER;
                env->nodes[mem].left = node;
                env->nodes[mem].str_off = prop->str_off;
                env->nodes[mem].str_len = prop->str_len;
                node = mem;
            }
        } else if (peek(env)->type == TOK_LBRACKET) {
            advance(env);
            int16_t idx = parse_expr(env);
            expect(env, TOK_RBRACKET);
            int16_t n = alloc_node(env); if (n < 0) return -1;
            env->nodes[n].type = NODE_INDEX;
            env->nodes[n].left = node;
            env->nodes[n].right = idx;
            node = n;
        } else if (peek(env)->type == TOK_LPAREN) {
            advance(env);
            /* Function call: collect args */
            int16_t call = alloc_node(env); if (call < 0) return -1;
            env->nodes[call].type = NODE_CALL;
            env->nodes[call].left = node; /* callee */
            int16_t first_arg = -1, prev_arg = -1;
            while (peek(env)->type != TOK_RPAREN && peek(env)->type != TOK_EOF) {
                int16_t arg = parse_assign_expr(env);
                if (arg < 0) break;
                if (first_arg < 0) first_arg = arg;
                if (prev_arg >= 0) env->nodes[prev_arg].extra = arg;
                prev_arg = arg;
                if (peek(env)->type == TOK_COMMA) advance(env);
            }
            expect(env, TOK_RPAREN);
            env->nodes[call].right = first_arg; /* arg list */
            node = call;
        } else if (peek(env)->type == TOK_PLUS_PLUS || peek(env)->type == TOK_MINUS_MINUS) {
            uint8_t op = (uint8_t)peek(env)->type;
            advance(env);
            int16_t n = alloc_node(env); if (n < 0) return -1;
            env->nodes[n].type = NODE_UPDATE;
            env->nodes[n].op = op;
            env->nodes[n].left = node;
            env->nodes[n].bool_val = 0; /* postfix */
            node = n;
        } else {
            break;
        }
    }
    return node;
}

/* Multiplicative: * / % */
static int16_t parse_mult(js_env_t *env) {
    int16_t left = parse_postfix(env);
    while (peek(env)->type == TOK_STAR || peek(env)->type == TOK_SLASH ||
           peek(env)->type == TOK_PERCENT) {
        uint8_t op = (uint8_t)peek(env)->type;
        advance(env);
        int16_t right = parse_postfix(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BINARY;
        env->nodes[n].op = op;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    }
    return left;
}

/* Additive: + - */
static int16_t parse_add(js_env_t *env) {
    int16_t left = parse_mult(env);
    while (peek(env)->type == TOK_PLUS || peek(env)->type == TOK_MINUS) {
        uint8_t op = (uint8_t)peek(env)->type;
        advance(env);
        int16_t right = parse_mult(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BINARY;
        env->nodes[n].op = op;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    }
    return left;
}

/* Comparison: < > <= >= */
static int16_t parse_compare(js_env_t *env) {
    int16_t left = parse_add(env);
    while (peek(env)->type == TOK_LT || peek(env)->type == TOK_GT ||
           peek(env)->type == TOK_LTE || peek(env)->type == TOK_GTE) {
        uint8_t op = (uint8_t)peek(env)->type;
        advance(env);
        int16_t right = parse_add(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BINARY;
        env->nodes[n].op = op;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    }
    return left;
}

/* Equality: == != === !== */
static int16_t parse_equality(js_env_t *env) {
    int16_t left = parse_compare(env);
    while (peek(env)->type == TOK_EQ || peek(env)->type == TOK_NEQ ||
           peek(env)->type == TOK_SEQ || peek(env)->type == TOK_SNEQ) {
        uint8_t op = (uint8_t)peek(env)->type;
        advance(env);
        int16_t right = parse_compare(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BINARY;
        env->nodes[n].op = op;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    }
    return left;
}

/* Logical AND: && */
static int16_t parse_logical_and(js_env_t *env) {
    int16_t left = parse_equality(env);
    while (peek(env)->type == TOK_AND) {
        advance(env);
        int16_t right = parse_equality(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BINARY;
        env->nodes[n].op = TOK_AND;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    }
    return left;
}

/* Logical OR: || */
static int16_t parse_logical_or(js_env_t *env) {
    int16_t left = parse_logical_and(env);
    while (peek(env)->type == TOK_OR) {
        advance(env);
        int16_t right = parse_logical_and(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BINARY;
        env->nodes[n].op = TOK_OR;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    }
    return left;
}

/* Ternary: expr ? expr : expr */
static int16_t parse_ternary(js_env_t *env) {
    int16_t left = parse_logical_or(env);
    if (peek(env)->type == TOK_QUESTION) {
        advance(env);
        int16_t then_expr = parse_assign_expr(env);
        expect(env, TOK_COLON);
        int16_t else_expr = parse_assign_expr(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_TERNARY;
        env->nodes[n].left = left;
        env->nodes[n].right = then_expr;
        env->nodes[n].extra = else_expr;
        left = n;
    }
    return left;
}

/* Assignment: expr = expr, expr += expr, expr -= expr */
static int16_t parse_assign_expr(js_env_t *env) {
    int16_t left = parse_ternary(env);
    if (peek(env)->type == TOK_ASSIGN) {
        advance(env);
        int16_t right = parse_assign_expr(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_ASSIGN;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    } else if (peek(env)->type == TOK_PLUS_ASSIGN || peek(env)->type == TOK_MINUS_ASSIGN) {
        uint8_t op = (uint8_t)peek(env)->type;
        advance(env);
        int16_t right = parse_assign_expr(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_COMPOUND_ASSIGN;
        env->nodes[n].op = op;
        env->nodes[n].left = left;
        env->nodes[n].right = right;
        left = n;
    }
    return left;
}

/* Comma expression */
static int16_t parse_expr(js_env_t *env) {
    return parse_assign_expr(env);
}

/* Block: { stmts } */
static int16_t parse_block(js_env_t *env) {
    if (!expect(env, TOK_LBRACE)) return -1;
    int16_t block = alloc_node(env); if (block < 0) return -1;
    env->nodes[block].type = NODE_BLOCK;
    int16_t first = -1, prev = -1;
    while (peek(env)->type != TOK_RBRACE && peek(env)->type != TOK_EOF) {
        int16_t s = parse_stmt(env);
        if (s < 0) break;
        if (first < 0) first = s;
        if (prev >= 0) env->nodes[prev].extra = s;
        prev = s;
    }
    expect(env, TOK_RBRACE);
    env->nodes[block].left = first;
    return block;
}

/* Statement */
static int16_t parse_stmt(js_env_t *env) {
    js_token_t *t = peek(env);

    /* Skip stray semicolons */
    if (t->type == TOK_SEMI) { advance(env); return parse_stmt(env); }

    /* var/let/const declaration */
    if (t->type == TOK_VAR || t->type == TOK_LET || t->type == TOK_CONST) {
        advance(env);
        if (peek(env)->type != TOK_IDENT) { js_set_error(env, "expected var name"); return -1; }
        js_token_t *name = advance(env);
        int16_t init = -1;
        if (peek(env)->type == TOK_ASSIGN) {
            advance(env);
            init = parse_assign_expr(env);
        }
        if (peek(env)->type == TOK_SEMI) advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_VAR_DECL;
        env->nodes[n].str_off = name->str_off;
        env->nodes[n].str_len = name->str_len;
        env->nodes[n].left = init;
        return n;
    }

    /* if statement */
    if (t->type == TOK_IF) {
        advance(env);
        expect(env, TOK_LPAREN);
        int16_t cond = parse_expr(env);
        expect(env, TOK_RPAREN);
        int16_t then_body = parse_stmt(env);
        int16_t else_body = -1;
        if (peek(env)->type == TOK_ELSE) {
            advance(env);
            else_body = parse_stmt(env);
        }
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_IF;
        env->nodes[n].left = cond;
        env->nodes[n].right = then_body;
        env->nodes[n].extra = else_body;
        return n;
    }

    /* while loop */
    if (t->type == TOK_WHILE) {
        advance(env);
        expect(env, TOK_LPAREN);
        int16_t cond = parse_expr(env);
        expect(env, TOK_RPAREN);
        int16_t body = parse_stmt(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_WHILE;
        env->nodes[n].left = cond;
        env->nodes[n].right = body;
        return n;
    }

    /* for loop */
    if (t->type == TOK_FOR) {
        advance(env);
        expect(env, TOK_LPAREN);
        int16_t init = -1;
        if (peek(env)->type != TOK_SEMI) {
            if (peek(env)->type == TOK_VAR || peek(env)->type == TOK_LET) {
                init = parse_stmt(env); /* var decl consumes ; */
            } else {
                init = parse_expr(env);
                if (peek(env)->type == TOK_SEMI) advance(env);
            }
        } else {
            advance(env); /* skip ; */
        }
        int16_t cond = -1;
        if (peek(env)->type != TOK_SEMI) cond = parse_expr(env);
        if (peek(env)->type == TOK_SEMI) advance(env);
        int16_t update = -1;
        if (peek(env)->type != TOK_RPAREN) update = parse_expr(env);
        expect(env, TOK_RPAREN);
        int16_t body = parse_stmt(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_FOR;
        env->nodes[n].left = init;
        env->nodes[n].right = cond;
        env->nodes[n].extra = update;
        env->nodes[n].extra2 = body;
        return n;
    }

    /* function declaration */
    if (t->type == TOK_FUNCTION) {
        return parse_primary(env); /* parse_primary handles 'function' */
    }

    /* return */
    if (t->type == TOK_RETURN) {
        advance(env);
        int16_t val = -1;
        if (peek(env)->type != TOK_SEMI && peek(env)->type != TOK_RBRACE &&
            peek(env)->type != TOK_EOF) {
            val = parse_expr(env);
        }
        if (peek(env)->type == TOK_SEMI) advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_RETURN;
        env->nodes[n].left = val;
        return n;
    }

    /* break */
    if (t->type == TOK_BREAK) {
        advance(env);
        if (peek(env)->type == TOK_SEMI) advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_BREAK;
        return n;
    }

    /* continue */
    if (t->type == TOK_CONTINUE) {
        advance(env);
        if (peek(env)->type == TOK_SEMI) advance(env);
        int16_t n = alloc_node(env); if (n < 0) return -1;
        env->nodes[n].type = NODE_CONTINUE;
        return n;
    }

    /* Block */
    if (t->type == TOK_LBRACE) {
        return parse_block(env);
    }

    /* Expression statement */
    int16_t expr = parse_expr(env);
    if (peek(env)->type == TOK_SEMI) advance(env);
    if (expr < 0) return -1;
    int16_t n = alloc_node(env); if (n < 0) return -1;
    env->nodes[n].type = NODE_EXPR_STMT;
    env->nodes[n].left = expr;
    return n;
}

/* Parse a program: list of statements */
static int16_t parse_program(js_env_t *env) {
    int16_t block = alloc_node(env); if (block < 0) return -1;
    env->nodes[block].type = NODE_BLOCK;
    int16_t first = -1, prev = -1;
    while (peek(env)->type != TOK_EOF && env->running) {
        int16_t s = parse_stmt(env);
        if (s < 0) { if (!env->running) return -1; continue; }
        if (first < 0) first = s;
        if (prev >= 0) env->nodes[prev].extra = s;
        prev = s;
    }
    env->nodes[block].left = first;
    return block;
}

/*=============================================================================
 * INTERPRETER — SCOPE MANAGEMENT
 *===========================================================================*/

static js_scope_t *cur_scope(js_env_t *env) {
    return &env->scopes[env->scope_depth];
}

static void push_scope(js_env_t *env) {
    if (env->scope_depth + 1 < JS_MAX_DEPTH) {
        env->scope_depth++;
        env->scopes[env->scope_depth].var_count = 0;
    }
}

static void pop_scope(js_env_t *env) {
    if (env->scope_depth > 0) env->scope_depth--;
}

/* Find variable in scope chain (current → parent → ... → global) */
static js_var_t *find_var(js_env_t *env, uint16_t name_off) {
    const char *name = sp_get(env, name_off);
    int name_len = js_strlen(name);
    for (int d = env->scope_depth; d >= 0; d--) {
        js_scope_t *sc = &env->scopes[d];
        for (int i = 0; i < sc->var_count; i++) {
            const char *vname = sp_get(env, sc->vars[i].name_off);
            if (js_strlen(vname) == name_len && js_strncmp(vname, name, name_len) == 0)
                return &sc->vars[i];
        }
    }
    return (void *)0;
}

/* Set variable (in scope where found, or create in current scope) */
static void set_var(js_env_t *env, uint16_t name_off, js_val_t val) {
    js_var_t *v = find_var(env, name_off);
    if (v) { v->value = val; return; }
    /* Create in current scope */
    js_scope_t *sc = cur_scope(env);
    if (sc->var_count >= JS_MAX_VARS) { js_set_error(env, "too many vars"); return; }
    sc->vars[sc->var_count].name_off = name_off;
    sc->vars[sc->var_count].value = val;
    sc->var_count++;
}

/*=============================================================================
 * INTERPRETER — OBJECT MANAGEMENT
 *===========================================================================*/

static uint16_t obj_alloc(js_env_t *env) {
    for (int i = 1; i < JS_MAX_OBJECTS; i++) { /* 0 reserved */
        if (!env->objects[i].in_use) {
            env->objects[i].in_use = 1;
            env->objects[i].prop_count = 0;
            return (uint16_t)i;
        }
    }
    js_set_error(env, "too many objects");
    return 0;
}

static js_val_t *obj_get(js_env_t *env, uint16_t obj_id, const char *key) {
    if (obj_id == 0 || obj_id >= JS_MAX_OBJECTS || !env->objects[obj_id].in_use)
        return (void *)0;
    js_object_t *obj = &env->objects[obj_id];
    int key_len = js_strlen(key);
    for (int i = 0; i < obj->prop_count; i++) {
        const char *pk = sp_get(env, obj->props[i].key_off);
        if (js_strlen(pk) == key_len && js_strncmp(pk, key, key_len) == 0)
            return &obj->props[i].value;
    }
    return (void *)0;
}

static void obj_set(js_env_t *env, uint16_t obj_id, const char *key, js_val_t val) {
    if (obj_id == 0 || obj_id >= JS_MAX_OBJECTS || !env->objects[obj_id].in_use) return;
    js_object_t *obj = &env->objects[obj_id];
    int key_len = js_strlen(key);
    /* Find existing */
    for (int i = 0; i < obj->prop_count; i++) {
        const char *pk = sp_get(env, obj->props[i].key_off);
        if (js_strlen(pk) == key_len && js_strncmp(pk, key, key_len) == 0) {
            obj->props[i].value = val;
            return;
        }
    }
    /* Add new */
    if (obj->prop_count >= JS_MAX_PROPS) return;
    obj->props[obj->prop_count].key_off = sp_intern(env, key, key_len);
    obj->props[obj->prop_count].value = val;
    obj->prop_count++;
}

/*=============================================================================
 * INTERPRETER — FUNCTION MANAGEMENT
 *===========================================================================*/

static int func_register(js_env_t *env, uint16_t name_off, int16_t params_node,
                         int param_count, int16_t body_node) {
    for (int i = 0; i < JS_MAX_FUNCS; i++) {
        if (!env->funcs[i].in_use) {
            env->funcs[i].in_use = 1;
            env->funcs[i].name_off = name_off;
            env->funcs[i].body_node = body_node;
            env->funcs[i].param_count = 0;
            /* Collect param names */
            int16_t p = params_node;
            while (p >= 0 && env->funcs[i].param_count < 8) {
                env->funcs[i].param_offs[env->funcs[i].param_count++] =
                    env->nodes[p].str_off;
                p = env->nodes[p].extra;
            }
            return i;
        }
    }
    return -1;
}

static js_func_t *func_find(js_env_t *env, uint16_t name_off) {
    const char *name = sp_get(env, name_off);
    int name_len = js_strlen(name);
    for (int i = 0; i < JS_MAX_FUNCS; i++) {
        if (env->funcs[i].in_use) {
            const char *fn = sp_get(env, env->funcs[i].name_off);
            if (js_strlen(fn) == name_len && js_strncmp(fn, name, name_len) == 0)
                return &env->funcs[i];
        }
    }
    return (void *)0;
}

/*=============================================================================
 * INTERPRETER — VALUE HELPERS
 *===========================================================================*/

static js_val_t js_undefined(void) {
    js_val_t v; v.type = JS_UNDEFINED; v.num = 0; return v;
}
static js_val_t js_null(void) {
    js_val_t v; v.type = JS_NULL; v.num = 0; return v;
}
static js_val_t js_bool(int b) {
    js_val_t v; v.type = JS_BOOL; v.boolean = b; return v;
}
static js_val_t js_num(double n) {
    js_val_t v; v.type = JS_NUMBER; v.num = n; return v;
}
static js_val_t js_str(js_env_t *env, const char *s, int len) {
    js_val_t v; v.type = JS_STRING; v.str_off = sp_add(env, s, len); return v;
}
static js_val_t js_str_off(uint16_t off) {
    js_val_t v; v.type = JS_STRING; v.str_off = off; return v;
}
static js_val_t js_obj(uint16_t id) {
    js_val_t v; v.type = JS_OBJECT; v.obj_id = id; return v;
}
static js_val_t js_func_val(uint16_t id) {
    js_val_t v; v.type = JS_FUNCTION; v.func_id = id; return v;
}

/* Truthiness */
static int js_truthy(js_env_t *env, js_val_t v) {
    switch (v.type) {
        case JS_UNDEFINED: case JS_NULL: return 0;
        case JS_BOOL: return v.boolean;
        case JS_NUMBER: return v.num != 0.0;
        case JS_STRING: return sp_len(env, v.str_off) > 0;
        default: return 1;
    }
}

/* Convert value to number */
static double js_to_number(js_env_t *env, js_val_t v) {
    switch (v.type) {
        case JS_NUMBER: return v.num;
        case JS_BOOL: return v.boolean ? 1.0 : 0.0;
        case JS_STRING: { const char *s = sp_get(env, v.str_off); return js_atof(s, js_strlen(s)); }
        default: return 0.0;
    }
}

/* Convert value to string (returns offset into string pool) */
static uint16_t js_to_string(js_env_t *env, js_val_t v) {
    switch (v.type) {
        case JS_STRING: return v.str_off;
        case JS_NUMBER: {
            char buf[32];
            int l = js_ftoa(v.num, buf, 31);
            return sp_add(env, buf, l);
        }
        case JS_BOOL: return v.boolean ? sp_intern(env, "true", 4) : sp_intern(env, "false", 5);
        case JS_NULL: return sp_intern(env, "null", 4);
        case JS_UNDEFINED: return sp_intern(env, "undefined", 9);
        case JS_OBJECT: return sp_intern(env, "[object Object]", 15);
        case JS_FUNCTION: return sp_intern(env, "function", 8);
        default: return sp_intern(env, "", 0);
    }
}

/*=============================================================================
 * INTERPRETER — EVAL (tree walk)
 *===========================================================================*/

static js_val_t eval_node(js_env_t *env, int16_t idx);

/* Count args in a linked list */
static int count_chain(js_env_t *env, int16_t head) {
    int c = 0;
    while (head >= 0) { c++; head = env->nodes[head].extra; }
    return c;
}

/* Handle built-in function calls (document.write, console.log, etc.) */
static int try_builtin_call(js_env_t *env, int16_t callee_node,
                            int16_t args_head, js_val_t *result) {
    js_node_t *cn = &env->nodes[callee_node];

    /* document.write(...) or document.writeln(...) */
    if (cn->type == NODE_MEMBER && cn->left >= 0) {
        js_node_t *obj_node = &env->nodes[cn->left];
        const char *prop = sp_get(env, cn->str_off);

        /* document.write / document.writeln */
        if (obj_node->type == NODE_IDENT) {
            const char *obj_name = sp_get(env, obj_node->str_off);
            if (js_streq(obj_name, js_strlen(obj_name), "document")) {
                if (js_streq(prop, js_strlen(prop), "write") ||
                    js_streq(prop, js_strlen(prop), "writeln")) {
                    int16_t arg = args_head;
                    while (arg >= 0) {
                        js_val_t av = eval_node(env, arg);
                        uint16_t s = js_to_string(env, av);
                        out_append_str(env, sp_get(env, s));
                        arg = env->nodes[arg].extra;
                    }
                    if (js_streq(prop, js_strlen(prop), "writeln"))
                        out_append(env, "\n", 1);
                    *result = js_undefined();
                    return 1;
                }
                /* document.getElementById — return a simple object */
                if (js_streq(prop, js_strlen(prop), "getElementById")) {
                    js_val_t arg_val = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
                    uint16_t elem_id = obj_alloc(env);
                    if (elem_id) {
                        /* Store the id as a property */
                        obj_set(env, elem_id, "id", arg_val);
                        obj_set(env, elem_id, "innerHTML", js_str(env, "", 0));
                        obj_set(env, elem_id, "textContent", js_str(env, "", 0));
                        obj_set(env, elem_id, "style", js_obj(obj_alloc(env)));
                    }
                    *result = js_obj(elem_id);
                    return 1;
                }
                /* document.createElement — return a simple object */
                if (js_streq(prop, js_strlen(prop), "createElement")) {
                    js_val_t arg_val = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
                    uint16_t elem_id = obj_alloc(env);
                    if (elem_id) {
                        obj_set(env, elem_id, "tagName", arg_val);
                        obj_set(env, elem_id, "innerHTML", js_str(env, "", 0));
                        obj_set(env, elem_id, "textContent", js_str(env, "", 0));
                        obj_set(env, elem_id, "style", js_obj(obj_alloc(env)));
                    }
                    *result = js_obj(elem_id);
                    return 1;
                }
            }

            /* console.log */
            if (js_streq(obj_name, js_strlen(obj_name), "console") &&
                js_streq(prop, js_strlen(prop), "log")) {
                /* Silently consume — just evaluate args */
                int16_t arg = args_head;
                while (arg >= 0) {
                    eval_node(env, arg);
                    arg = env->nodes[arg].extra;
                }
                *result = js_undefined();
                return 1;
            }
        }

        /* String methods: str.indexOf(needle), str.substring(a,b), str.charAt(n),
         * str.toLowerCase(), str.toUpperCase(), str.trim(), str.split(delim),
         * str.replace(search, repl) */
        js_val_t obj_val = eval_node(env, cn->left);
        if (obj_val.type == JS_STRING) {
            const char *s = sp_get(env, obj_val.str_off);
            int slen = js_strlen(s);

            if (js_streq(prop, js_strlen(prop), "indexOf")) {
                js_val_t needle = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
                if (needle.type == JS_STRING) {
                    const char *ns = sp_get(env, needle.str_off);
                    int nlen = js_strlen(ns);
                    int found = -1;
                    for (int i = 0; i <= slen - nlen; i++) {
                        if (js_strncmp(s + i, ns, nlen) == 0) { found = i; break; }
                    }
                    *result = js_num((double)found);
                } else {
                    *result = js_num(-1);
                }
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "substring") ||
                js_streq(prop, js_strlen(prop), "slice")) {
                js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_num(0);
                int start = (int)js_to_number(env, a);
                if (start < 0) start = 0;
                int end = slen;
                if (args_head >= 0 && env->nodes[args_head].extra >= 0) {
                    js_val_t b = eval_node(env, env->nodes[args_head].extra);
                    end = (int)js_to_number(env, b);
                }
                if (end > slen) end = slen;
                if (start > end) { int tmp2 = start; start = end; end = tmp2; }
                *result = js_str(env, s + start, end - start);
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "charAt")) {
                js_val_t idx = (args_head >= 0) ? eval_node(env, args_head) : js_num(0);
                int ix = (int)js_to_number(env, idx);
                if (ix >= 0 && ix < slen) *result = js_str(env, s + ix, 1);
                else *result = js_str(env, "", 0);
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "toLowerCase")) {
                char tmp[512]; int tl = slen > 511 ? 511 : slen;
                for (int i = 0; i < tl; i++)
                    tmp[i] = (s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i];
                *result = js_str(env, tmp, tl);
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "toUpperCase")) {
                char tmp[512]; int tl = slen > 511 ? 511 : slen;
                for (int i = 0; i < tl; i++)
                    tmp[i] = (s[i] >= 'a' && s[i] <= 'z') ? s[i] - 32 : s[i];
                *result = js_str(env, tmp, tl);
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "trim")) {
                int a = 0, b = slen;
                while (a < b && js_isspace(s[a])) a++;
                while (b > a && js_isspace(s[b-1])) b--;
                *result = js_str(env, s + a, b - a);
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "replace")) {
                js_val_t search_v = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
                js_val_t repl_v = (args_head >= 0 && env->nodes[args_head].extra >= 0) ?
                    eval_node(env, env->nodes[args_head].extra) : js_undefined();
                if (search_v.type == JS_STRING && repl_v.type == JS_STRING) {
                    const char *needle = sp_get(env, search_v.str_off);
                    int nlen = js_strlen(needle);
                    const char *repl = sp_get(env, repl_v.str_off);
                    int rlen = js_strlen(repl);
                    char tmp[512]; int tl = 0; int replaced = 0;
                    for (int i = 0; i < slen && tl < 510; ) {
                        if (!replaced && i <= slen - nlen && nlen > 0 &&
                            js_strncmp(s + i, needle, nlen) == 0) {
                            for (int j = 0; j < rlen && tl < 510; j++) tmp[tl++] = repl[j];
                            i += nlen; replaced = 1;
                        } else {
                            tmp[tl++] = s[i++];
                        }
                    }
                    *result = js_str(env, tmp, tl);
                } else {
                    *result = obj_val;
                }
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "split")) {
                /* Return array of substrings split by delimiter */
                js_val_t delim_v = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
                uint16_t arr_id = obj_alloc(env);
                if (arr_id && delim_v.type == JS_STRING) {
                    const char *delim = sp_get(env, delim_v.str_off);
                    int dlen = js_strlen(delim);
                    int count = 0, start = 0;
                    for (int i = 0; i <= slen && count < 30; ) {
                        if (dlen == 0) {
                            /* Split on each character */
                            if (i < slen) {
                                char idx_buf[8]; int il = js_ftoa(count, idx_buf, 7);
                                obj_set(env, arr_id, idx_buf, js_str(env, s + i, 1));
                                count++; i++;
                            } else break;
                        } else if (i + dlen <= slen && js_strncmp(s + i, delim, dlen) == 0) {
                            char idx_buf[8]; int il = js_ftoa(count, idx_buf, 7);
                            obj_set(env, arr_id, idx_buf, js_str(env, s + start, i - start));
                            count++; i += dlen; start = i;
                        } else if (i >= slen) {
                            char idx_buf[8]; int il = js_ftoa(count, idx_buf, 7);
                            obj_set(env, arr_id, idx_buf, js_str(env, s + start, slen - start));
                            count++; break;
                        } else {
                            i++;
                        }
                    }
                    obj_set(env, arr_id, "length", js_num(count));
                }
                *result = js_obj(arr_id);
                return 1;
            }
        }

        /* Array/Object methods: arr.push(val), arr.join(sep) */
        if (obj_val.type == JS_OBJECT && obj_val.obj_id > 0) {
            if (js_streq(prop, js_strlen(prop), "push")) {
                js_val_t *len_v = obj_get(env, obj_val.obj_id, "length");
                int len = (len_v && len_v->type == JS_NUMBER) ? (int)len_v->num : 0;
                int16_t arg = args_head;
                while (arg >= 0) {
                    js_val_t av = eval_node(env, arg);
                    char idx_buf[8]; js_ftoa(len, idx_buf, 7);
                    obj_set(env, obj_val.obj_id, idx_buf, av);
                    len++;
                    arg = env->nodes[arg].extra;
                }
                obj_set(env, obj_val.obj_id, "length", js_num(len));
                *result = js_num(len);
                return 1;
            }
            if (js_streq(prop, js_strlen(prop), "join")) {
                js_val_t sep_v = (args_head >= 0) ? eval_node(env, args_head) : js_str(env, ",", 1);
                const char *sep = sp_get(env, js_to_string(env, sep_v));
                int seplen = js_strlen(sep);
                js_val_t *len_v = obj_get(env, obj_val.obj_id, "length");
                int len = (len_v && len_v->type == JS_NUMBER) ? (int)len_v->num : 0;
                char tmp[512]; int tl = 0;
                for (int i = 0; i < len && tl < 510; i++) {
                    if (i > 0) { for (int j = 0; j < seplen && tl < 510; j++) tmp[tl++] = sep[j]; }
                    char idx_buf[8]; js_ftoa(i, idx_buf, 7);
                    js_val_t *elem = obj_get(env, obj_val.obj_id, idx_buf);
                    if (elem) {
                        uint16_t es = js_to_string(env, *elem);
                        const char *estr = sp_get(env, es);
                        int el = js_strlen(estr);
                        for (int j = 0; j < el && tl < 510; j++) tmp[tl++] = estr[j];
                    }
                }
                *result = js_str(env, tmp, tl);
                return 1;
            }
        }
    }

    /* Math.floor, Math.ceil, Math.round, Math.random, Math.abs, Math.min, Math.max */
    if (cn->type == NODE_MEMBER && cn->left >= 0) {
        js_node_t *obj_node = &env->nodes[cn->left];
        if (obj_node->type == NODE_IDENT) {
            const char *obj_name = sp_get(env, obj_node->str_off);
            const char *prop = sp_get(env, cn->str_off);
            if (js_streq(obj_name, js_strlen(obj_name), "Math")) {
                if (js_streq(prop, js_strlen(prop), "floor")) {
                    js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_num(0);
                    double v = js_to_number(env, a);
                    *result = js_num((double)(int)v);
                    return 1;
                }
                if (js_streq(prop, js_strlen(prop), "ceil")) {
                    js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_num(0);
                    double v = js_to_number(env, a);
                    int iv = (int)v;
                    *result = js_num((v > (double)iv) ? (double)(iv + 1) : (double)iv);
                    return 1;
                }
                if (js_streq(prop, js_strlen(prop), "round")) {
                    js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_num(0);
                    double v = js_to_number(env, a);
                    *result = js_num((double)(int)(v + 0.5));
                    return 1;
                }
                if (js_streq(prop, js_strlen(prop), "abs")) {
                    js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_num(0);
                    double v = js_to_number(env, a);
                    *result = js_num(v < 0 ? -v : v);
                    return 1;
                }
                if (js_streq(prop, js_strlen(prop), "random")) {
                    /* Simple pseudo-random (not cryptographically secure) */
                    static uint32_t seed = 12345;
                    seed = seed * 1103515245 + 12345;
                    double r = (double)(seed & 0x7FFFFFFF) / 2147483647.0;
                    *result = js_num(r);
                    return 1;
                }
                if (js_streq(prop, js_strlen(prop), "min") || js_streq(prop, js_strlen(prop), "max")) {
                    int is_min = js_streq(prop, js_strlen(prop), "min");
                    double res = is_min ? 1e30 : -1e30;
                    int16_t arg = args_head;
                    while (arg >= 0) {
                        js_val_t av = eval_node(env, arg);
                        double v = js_to_number(env, av);
                        if (is_min ? (v < res) : (v > res)) res = v;
                        arg = env->nodes[arg].extra;
                    }
                    *result = js_num(res);
                    return 1;
                }
            }
        }
    }

    /* parseInt / parseFloat / isNaN */
    if (cn->type == NODE_IDENT) {
        const char *fn_name = sp_get(env, cn->str_off);
        if (js_streq(fn_name, js_strlen(fn_name), "parseInt")) {
            js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
            *result = js_num((double)(int)js_to_number(env, a));
            return 1;
        }
        if (js_streq(fn_name, js_strlen(fn_name), "parseFloat")) {
            js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
            *result = js_num(js_to_number(env, a));
            return 1;
        }
        if (js_streq(fn_name, js_strlen(fn_name), "isNaN")) {
            js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
            /* Simple: treat NaN as 0 since we don't have real NaN */
            *result = js_bool(a.type == JS_UNDEFINED);
            return 1;
        }
        if (js_streq(fn_name, js_strlen(fn_name), "String")) {
            js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
            uint16_t s = js_to_string(env, a);
            *result = js_str_off(s);
            return 1;
        }
        if (js_streq(fn_name, js_strlen(fn_name), "Number")) {
            js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
            *result = js_num(js_to_number(env, a));
            return 1;
        }
        if (js_streq(fn_name, js_strlen(fn_name), "Boolean")) {
            js_val_t a = (args_head >= 0) ? eval_node(env, args_head) : js_undefined();
            *result = js_bool(js_truthy(env, a));
            return 1;
        }
        /* alert — just silently consume */
        if (js_streq(fn_name, js_strlen(fn_name), "alert") ||
            js_streq(fn_name, js_strlen(fn_name), "setTimeout") ||
            js_streq(fn_name, js_strlen(fn_name), "setInterval") ||
            js_streq(fn_name, js_strlen(fn_name), "clearTimeout") ||
            js_streq(fn_name, js_strlen(fn_name), "clearInterval") ||
            js_streq(fn_name, js_strlen(fn_name), "addEventListener") ||
            js_streq(fn_name, js_strlen(fn_name), "removeEventListener") ||
            js_streq(fn_name, js_strlen(fn_name), "requestAnimationFrame")) {
            /* Silently consume — evaluate args but discard */
            int16_t arg = args_head;
            while (arg >= 0) { eval_node(env, arg); arg = env->nodes[arg].extra; }
            *result = js_undefined();
            return 1;
        }
    }

    return 0; /* not a built-in */
}

/*=============================================================================
 * INTERPRETER — EVAL NODE
 *===========================================================================*/

static js_val_t eval_node(js_env_t *env, int16_t idx) {
    if (idx < 0 || !env->running) return js_undefined();

    js_node_t *n = &env->nodes[idx];

    switch (n->type) {
    case NODE_NUM_LIT:
        return js_num(n->num_val);

    case NODE_STR_LIT:
        return js_str_off(n->str_off);

    case NODE_BOOL_LIT:
        return js_bool(n->bool_val);

    case NODE_NULL_LIT:
        return js_null();

    case NODE_IDENT: {
        js_var_t *v = find_var(env, n->str_off);
        if (v) return v->value;
        /* Check if it's a function name */
        js_func_t *f = func_find(env, n->str_off);
        if (f) return js_func_val((uint16_t)(f - env->funcs));
        /* Built-in globals */
        const char *name = sp_get(env, n->str_off);
        if (js_streq(name, js_strlen(name), "undefined")) return js_undefined();
        if (js_streq(name, js_strlen(name), "NaN")) return js_num(0);
        if (js_streq(name, js_strlen(name), "Infinity")) return js_num(1e30);
        /* window, document, navigator, etc. — return objects that won't crash */
        if (js_streq(name, js_strlen(name), "window") ||
            js_streq(name, js_strlen(name), "self") ||
            js_streq(name, js_strlen(name), "globalThis"))
            return js_undefined(); /* window === undefined is fine, won't crash */
        return js_undefined();
    }

    case NODE_BINARY: {
        /* Short-circuit for && and || */
        if (n->op == TOK_AND) {
            js_val_t l = eval_node(env, n->left);
            if (!js_truthy(env, l)) return l;
            return eval_node(env, n->right);
        }
        if (n->op == TOK_OR) {
            js_val_t l = eval_node(env, n->left);
            if (js_truthy(env, l)) return l;
            return eval_node(env, n->right);
        }

        js_val_t l = eval_node(env, n->left);
        js_val_t r = eval_node(env, n->right);

        /* String concatenation */
        if (n->op == TOK_PLUS && (l.type == JS_STRING || r.type == JS_STRING)) {
            uint16_t ls = js_to_string(env, l);
            uint16_t rs = js_to_string(env, r);
            const char *a = sp_get(env, ls);
            const char *b = sp_get(env, rs);
            int alen = js_strlen(a), blen = js_strlen(b);
            char tmp[1024];
            int tl = 0;
            for (int i = 0; i < alen && tl < 1022; i++) tmp[tl++] = a[i];
            for (int i = 0; i < blen && tl < 1022; i++) tmp[tl++] = b[i];
            return js_str(env, tmp, tl);
        }

        double ln = js_to_number(env, l);
        double rn = js_to_number(env, r);

        switch (n->op) {
            case TOK_PLUS:    return js_num(ln + rn);
            case TOK_MINUS:   return js_num(ln - rn);
            case TOK_STAR:    return js_num(ln * rn);
            case TOK_SLASH:   return js_num(rn != 0 ? ln / rn : 0);
            case TOK_PERCENT: return js_num(rn != 0 ? (double)((int)ln % (int)rn) : 0);
            case TOK_LT:     return js_bool(ln < rn);
            case TOK_GT:     return js_bool(ln > rn);
            case TOK_LTE:    return js_bool(ln <= rn);
            case TOK_GTE:    return js_bool(ln >= rn);
            case TOK_EQ:     {
                if (l.type == JS_STRING && r.type == JS_STRING)
                    return js_bool(js_strncmp(sp_get(env, l.str_off), sp_get(env, r.str_off),
                                              sp_len(env, l.str_off) + 1) == 0);
                if (l.type == JS_NULL && r.type == JS_NULL) return js_bool(1);
                if (l.type == JS_UNDEFINED && r.type == JS_UNDEFINED) return js_bool(1);
                if (l.type == JS_NULL && r.type == JS_UNDEFINED) return js_bool(1);
                if (l.type == JS_UNDEFINED && r.type == JS_NULL) return js_bool(1);
                if (l.type == JS_BOOL && r.type == JS_BOOL) return js_bool(l.boolean == r.boolean);
                return js_bool(ln == rn);
            }
            case TOK_NEQ:    {
                js_val_t eq = eval_node(env, idx); /* This would recurse — just inline */
                /* Simplified: compare as numbers or strings */
                if (l.type == JS_STRING && r.type == JS_STRING)
                    return js_bool(js_strncmp(sp_get(env, l.str_off), sp_get(env, r.str_off),
                                              sp_len(env, l.str_off) + 1) != 0);
                return js_bool(ln != rn);
            }
            case TOK_SEQ:    return js_bool(l.type == r.type &&
                (l.type == JS_STRING ? js_strncmp(sp_get(env, l.str_off), sp_get(env, r.str_off),
                                                   sp_len(env, l.str_off) + 1) == 0 : ln == rn));
            case TOK_SNEQ:   return js_bool(l.type != r.type ||
                (l.type == JS_STRING ? js_strncmp(sp_get(env, l.str_off), sp_get(env, r.str_off),
                                                   sp_len(env, l.str_off) + 1) != 0 : ln != rn));
            default: return js_undefined();
        }
    }

    case NODE_UNARY: {
        js_val_t operand = eval_node(env, n->left);
        if (n->op == TOK_NOT) return js_bool(!js_truthy(env, operand));
        if (n->op == TOK_MINUS) return js_num(-js_to_number(env, operand));
        return js_undefined();
    }

    case NODE_TYPEOF: {
        js_val_t v = eval_node(env, n->left);
        const char *t;
        switch (v.type) {
            case JS_UNDEFINED: t = "undefined"; break;
            case JS_NULL:      t = "object"; break; /* typeof null === "object" */
            case JS_BOOL:      t = "boolean"; break;
            case JS_NUMBER:    t = "number"; break;
            case JS_STRING:    t = "string"; break;
            case JS_OBJECT:    t = "object"; break;
            case JS_FUNCTION:  t = "function"; break;
            default:           t = "undefined"; break;
        }
        return js_str(env, t, js_strlen(t));
    }

    case NODE_ASSIGN: {
        js_val_t val = eval_node(env, n->right);
        /* Target can be ident, member, or index */
        if (n->left >= 0) {
            js_node_t *target = &env->nodes[n->left];
            if (target->type == NODE_IDENT) {
                set_var(env, target->str_off, val);
            } else if (target->type == NODE_MEMBER) {
                js_val_t obj = eval_node(env, target->left);
                if (obj.type == JS_OBJECT) {
                    const char *prop = sp_get(env, target->str_off);
                    obj_set(env, obj.obj_id, prop, val);
                }
            } else if (target->type == NODE_INDEX) {
                js_val_t obj = eval_node(env, target->left);
                js_val_t idx_v = eval_node(env, target->right);
                if (obj.type == JS_OBJECT) {
                    uint16_t key_s = js_to_string(env, idx_v);
                    obj_set(env, obj.obj_id, sp_get(env, key_s), val);
                }
            }
        }
        return val;
    }

    case NODE_COMPOUND_ASSIGN: {
        js_val_t old_val = eval_node(env, n->left);
        js_val_t right_val = eval_node(env, n->right);
        js_val_t new_val;
        if (n->op == TOK_PLUS_ASSIGN) {
            /* String concat if either is string */
            if (old_val.type == JS_STRING || right_val.type == JS_STRING) {
                uint16_t ls = js_to_string(env, old_val);
                uint16_t rs = js_to_string(env, right_val);
                const char *a = sp_get(env, ls);
                const char *b = sp_get(env, rs);
                int alen = js_strlen(a), blen = js_strlen(b);
                char tmp[1024]; int tl = 0;
                for (int i = 0; i < alen && tl < 1022; i++) tmp[tl++] = a[i];
                for (int i = 0; i < blen && tl < 1022; i++) tmp[tl++] = b[i];
                new_val = js_str(env, tmp, tl);
            } else {
                new_val = js_num(js_to_number(env, old_val) + js_to_number(env, right_val));
            }
        } else {
            new_val = js_num(js_to_number(env, old_val) - js_to_number(env, right_val));
        }
        /* Store back */
        if (n->left >= 0) {
            js_node_t *target = &env->nodes[n->left];
            if (target->type == NODE_IDENT) set_var(env, target->str_off, new_val);
            else if (target->type == NODE_MEMBER) {
                js_val_t obj = eval_node(env, target->left);
                if (obj.type == JS_OBJECT) obj_set(env, obj.obj_id, sp_get(env, target->str_off), new_val);
            }
        }
        return new_val;
    }

    case NODE_UPDATE: {
        js_val_t old_val = eval_node(env, n->left);
        double old_n = js_to_number(env, old_val);
        double new_n = (n->op == TOK_PLUS_PLUS) ? old_n + 1 : old_n - 1;
        js_val_t new_val = js_num(new_n);
        if (n->left >= 0) {
            js_node_t *target = &env->nodes[n->left];
            if (target->type == NODE_IDENT) set_var(env, target->str_off, new_val);
        }
        return n->bool_val ? new_val : js_num(old_n); /* prefix vs postfix */
    }

    case NODE_VAR_DECL: {
        js_val_t init_val = (n->left >= 0) ? eval_node(env, n->left) : js_undefined();
        set_var(env, n->str_off, init_val);
        return init_val;
    }

    case NODE_IF: {
        js_val_t cond = eval_node(env, n->left);
        if (js_truthy(env, cond)) {
            return eval_node(env, n->right);
        } else if (n->extra >= 0) {
            return eval_node(env, n->extra);
        }
        return js_undefined();
    }

    case NODE_WHILE: {
        int iterations = 0;
        while (env->running && iterations < 100000) {
            js_val_t cond = eval_node(env, n->left);
            if (!js_truthy(env, cond)) break;
            eval_node(env, n->right);
            if (env->breaking) { env->breaking = 0; break; }
            if (env->continuing) { env->continuing = 0; }
            if (env->returning) break;
            iterations++;
        }
        return js_undefined();
    }

    case NODE_FOR: {
        /* init */
        if (n->left >= 0) eval_node(env, n->left);
        int iterations = 0;
        while (env->running && iterations < 100000) {
            /* cond */
            if (n->right >= 0) {
                js_val_t cond = eval_node(env, n->right);
                if (!js_truthy(env, cond)) break;
            }
            /* body */
            if (n->extra2 >= 0) eval_node(env, n->extra2);
            if (env->breaking) { env->breaking = 0; break; }
            if (env->continuing) { env->continuing = 0; }
            if (env->returning) break;
            /* update */
            if (n->extra >= 0) eval_node(env, n->extra);
            iterations++;
        }
        return js_undefined();
    }

    case NODE_BLOCK: {
        int16_t stmt = n->left;
        js_val_t last = js_undefined();
        while (stmt >= 0 && env->running && !env->returning &&
               !env->breaking && !env->continuing) {
            last = eval_node(env, stmt);
            stmt = env->nodes[stmt].extra;
        }
        return last;
    }

    case NODE_EXPR_STMT:
        return eval_node(env, n->left);

    case NODE_CALL: {
        /* Try built-in first */
        js_val_t result;
        if (try_builtin_call(env, n->left, n->right, &result))
            return result;

        /* User function call */
        js_val_t callee = eval_node(env, n->left);
        if (callee.type == JS_FUNCTION && callee.func_id < JS_MAX_FUNCS) {
            js_func_t *f = &env->funcs[callee.func_id];
            if (!f->in_use) return js_undefined();

            /* Push new scope and bind params */
            push_scope(env);
            int16_t arg_node = n->right;
            for (int i = 0; i < f->param_count; i++) {
                js_val_t arg_val = js_undefined();
                if (arg_node >= 0) {
                    arg_val = eval_node(env, arg_node);
                    arg_node = env->nodes[arg_node].extra;
                }
                set_var(env, f->param_offs[i], arg_val);
            }

            /* Execute body */
            eval_node(env, f->body_node);
            js_val_t ret = env->ret_val;
            env->returning = 0;
            env->ret_val = js_undefined();
            pop_scope(env);
            return ret;
        }
        return js_undefined();
    }

    case NODE_MEMBER: {
        js_val_t obj = eval_node(env, n->left);
        const char *prop = sp_get(env, n->str_off);

        /* String.length */
        if (obj.type == JS_STRING && js_streq(prop, js_strlen(prop), "length")) {
            return js_num((double)sp_len(env, obj.str_off));
        }

        /* Object property */
        if (obj.type == JS_OBJECT && obj.obj_id > 0) {
            /* array.length */
            js_val_t *pv = obj_get(env, obj.obj_id, prop);
            if (pv) return *pv;
        }

        return js_undefined();
    }

    case NODE_INDEX: {
        js_val_t obj = eval_node(env, n->left);
        js_val_t idx = eval_node(env, n->right);

        /* String indexing */
        if (obj.type == JS_STRING) {
            int ix = (int)js_to_number(env, idx);
            const char *s = sp_get(env, obj.str_off);
            int slen = js_strlen(s);
            if (ix >= 0 && ix < slen) return js_str(env, s + ix, 1);
            return js_undefined();
        }

        /* Object/array indexing */
        if (obj.type == JS_OBJECT && obj.obj_id > 0) {
            uint16_t key_s = js_to_string(env, idx);
            js_val_t *pv = obj_get(env, obj.obj_id, sp_get(env, key_s));
            if (pv) return *pv;
        }

        return js_undefined();
    }

    case NODE_FUNC_DEF: {
        /* Register function */
        int fid = func_register(env, n->str_off, n->left, 0, n->right);
        if (fid >= 0) {
            js_val_t fval = js_func_val((uint16_t)fid);
            if (n->str_len > 0) {
                /* Named function — bind in current scope */
                set_var(env, n->str_off, fval);
            }
            return fval;
        }
        return js_undefined();
    }

    case NODE_RETURN: {
        env->ret_val = (n->left >= 0) ? eval_node(env, n->left) : js_undefined();
        env->returning = 1;
        return env->ret_val;
    }

    case NODE_BREAK:
        env->breaking = 1;
        return js_undefined();

    case NODE_CONTINUE:
        env->continuing = 1;
        return js_undefined();

    case NODE_TERNARY: {
        js_val_t cond = eval_node(env, n->left);
        if (js_truthy(env, cond)) return eval_node(env, n->right);
        return eval_node(env, n->extra);
    }

    case NODE_ARRAY_LIT: {
        uint16_t arr_id = obj_alloc(env);
        if (!arr_id) return js_undefined();
        int count = 0;
        int16_t elem = n->left;
        while (elem >= 0) {
            js_val_t v = eval_node(env, elem);
            char idx_buf[8]; js_ftoa(count, idx_buf, 7);
            obj_set(env, arr_id, idx_buf, v);
            count++;
            elem = env->nodes[elem].extra;
        }
        obj_set(env, arr_id, "length", js_num(count));
        return js_obj(arr_id);
    }

    case NODE_OBJECT_LIT: {
        uint16_t obj_id = obj_alloc(env);
        if (!obj_id) return js_undefined();
        int16_t kv = n->left;
        while (kv >= 0) {
            const char *key = sp_get(env, env->nodes[kv].str_off);
            js_val_t val = (env->nodes[kv].right >= 0) ?
                eval_node(env, env->nodes[kv].right) : js_undefined();
            obj_set(env, obj_id, key, val);
            kv = env->nodes[kv].extra;
        }
        return js_obj(obj_id);
    }

    default:
        return js_undefined();
    }
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

void js_init(js_env_t *env, char *out_buf, int out_max) {
    js_memset(env, 0, sizeof(js_env_t));
    env->out_buf = out_buf;
    env->out_max = out_max;
    env->out_len = 0;
    env->running = 1;
    env->scope_depth = 0;
    if (out_buf && out_max > 0) out_buf[0] = '\0';
}

int js_exec(js_env_t *env, const char *src, int src_len) {
    if (!src || src_len <= 0) return 0;

    env->running = 1;
    env->returning = 0;
    env->breaking = 0;
    env->continuing = 0;
    env->error[0] = '\0';

    /* Reset tokens and nodes for this script (preserve env state) */
    env->token_count = 0;
    env->node_count = 0;
    env->tok_pos = 0;

    /* Lex */
    lex(env, src, src_len);
    if (!env->running) return -1;

    /* Parse */
    env->tok_pos = 0;
    int16_t program = parse_program(env);
    if (!env->running || program < 0) return -1;

    /* Execute */
    env->running = 1;
    eval_node(env, program);

    return env->running ? 0 : -1;
}

void js_set_dom_callback(js_env_t *env, js_dom_write_cb cb, void *ctx) {
    /* Reserved for future use — DOM element update notifications */
    (void)env; (void)cb; (void)ctx;
}
