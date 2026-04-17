/**
 * MaahiOS HTML Tokenizer Library - libhtml.c
 *
 * Layer 2 (Library). Ring 3.
 * Lightweight streaming HTML tokenizer.
 *
 * Features:
 *   - Recognizes ~40 common HTML tags
 *   - Extracts href attribute from <a> tags
 *   - Skips <script> and <style> block content
 *   - Decodes basic HTML entities (&amp; &lt; &gt; &quot; &nbsp; &#NNN;)
 *   - Collapses whitespace in text tokens
 */

#include "libhtml.h"

/*=============================================================================
 * TAG NAME LOOKUP TABLE
 *===========================================================================*/

typedef struct {
    const char *name;
    uint8_t     id;
} tag_entry_t;

static const tag_entry_t g_tags[] = {
    {"html",    HTML_TAG_HTML},     {"head",    HTML_TAG_HEAD},
    {"title",   HTML_TAG_TITLE},    {"body",    HTML_TAG_BODY},
    {"h1",      HTML_TAG_H1},       {"h2",      HTML_TAG_H2},
    {"h3",      HTML_TAG_H3},       {"h4",      HTML_TAG_H4},
    {"h5",      HTML_TAG_H5},       {"h6",      HTML_TAG_H6},
    {"p",       HTML_TAG_P},        {"br",      HTML_TAG_BR},
    {"hr",      HTML_TAG_HR},
    {"b",       HTML_TAG_B},        {"strong",  HTML_TAG_STRONG},
    {"i",       HTML_TAG_I},        {"em",      HTML_TAG_EM},
    {"a",       HTML_TAG_A},        {"ul",      HTML_TAG_UL},
    {"ol",      HTML_TAG_OL},       {"li",      HTML_TAG_LI},
    {"div",     HTML_TAG_DIV},      {"span",    HTML_TAG_SPAN},
    {"table",   HTML_TAG_TABLE},    {"tr",      HTML_TAG_TR},
    {"td",      HTML_TAG_TD},       {"th",      HTML_TAG_TH},
    {"meta",    HTML_TAG_META},     {"link",    HTML_TAG_LINK},
    {"style",   HTML_TAG_STYLE},    {"script",  HTML_TAG_SCRIPT},
    {"img",     HTML_TAG_IMG},      {"pre",     HTML_TAG_PRE},
    {"code",    HTML_TAG_CODE},
    {"header",  HTML_TAG_HEADER},   {"footer",  HTML_TAG_FOOTER},
    {"nav",     HTML_TAG_NAV},      {"section", HTML_TAG_SECTION},
    {"article", HTML_TAG_ARTICLE},  {"aside",   HTML_TAG_ASIDE},
    {"main",    HTML_TAG_MAIN},
    {"form",    HTML_TAG_FORM},     {"input",   HTML_TAG_INPUT},
    {"button",  HTML_TAG_BUTTON_TAG},
    {"noscript",HTML_TAG_NOSCRIPT},
    {0, 0}
};

/*=============================================================================
 * STRING HELPERS (freestanding)
 *===========================================================================*/

static int _is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int _is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static char _tolower(char c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

static int _streq_nocase(const char *a, int a_len, const char *b) {
    int i = 0;
    while (b[i]) {
        if (i >= a_len) return 0;
        if (_tolower(a[i]) != _tolower(b[i])) return 0;
        i++;
    }
    return (i == a_len);
}

/*=============================================================================
 * TAG LOOKUP
 *===========================================================================*/

static uint8_t _lookup_tag(const char *name, int name_len) {
    for (int i = 0; g_tags[i].name; i++) {
        if (_streq_nocase(name, name_len, g_tags[i].name))
            return g_tags[i].id;
    }
    return HTML_TAG_UNKNOWN;
}

/*=============================================================================
 * ENTITY DECODING
 *===========================================================================*/

/**
 * Try to decode an HTML entity at src[pos] (starting after '&').
 * Returns the decoded character and advances *pos past the ';'.
 * If not a recognized entity, returns '&' and doesn't advance.
 */
static char _decode_entity(const char *src, int len, int *pos) {
    int start = *pos;
    int end = start;

    /* Find the ';' — max 10 chars */
    while (end < len && end - start < 10 && src[end] != ';') end++;

    if (end >= len || src[end] != ';') return '&'; /* Not an entity */

    int elen = end - start;

    /* Named entities */
    if (elen == 3 && _streq_nocase(src + start, 3, "amp"))  { *pos = end + 1; return '&'; }
    if (elen == 2 && _streq_nocase(src + start, 2, "lt"))   { *pos = end + 1; return '<'; }
    if (elen == 2 && _streq_nocase(src + start, 2, "gt"))   { *pos = end + 1; return '>'; }
    if (elen == 4 && _streq_nocase(src + start, 4, "quot")) { *pos = end + 1; return '"'; }
    if (elen == 4 && _streq_nocase(src + start, 4, "nbsp")) { *pos = end + 1; return ' '; }
    if (elen == 4 && _streq_nocase(src + start, 4, "apos")) { *pos = end + 1; return '\''; }

    /* Numeric entity: &#NNN; or &#xHHH; */
    if (elen >= 2 && src[start] == '#') {
        int val = 0;
        int i = start + 1;
        if (i < end && (src[i] == 'x' || src[i] == 'X')) {
            /* Hex */
            i++;
            while (i < end) {
                char c = _tolower(src[i]);
                if (c >= '0' && c <= '9') val = val * 16 + (c - '0');
                else if (c >= 'a' && c <= 'f') val = val * 16 + (c - 'a' + 10);
                else break;
                i++;
            }
        } else {
            /* Decimal */
            while (i < end) {
                if (src[i] >= '0' && src[i] <= '9') val = val * 10 + (src[i] - '0');
                else break;
                i++;
            }
        }
        if (val > 0 && val < 128) {
            *pos = end + 1;
            return (char)val;
        }
        /* Non-ASCII entity → render as space */
        if (val >= 128) {
            *pos = end + 1;
            return ' ';
        }
    }

    return '&'; /* Unknown entity — keep the '&' */
}

/*=============================================================================
 * EXTRACT HREF ATTRIBUTE
 *===========================================================================*/

/**
 * Look for href="..." or href='...' in the tag body.
 * tag_body points to the content after the tag name.
 */
static void _extract_href(const char *tag_body, int body_len,
                           char *href_out, int href_max) {
    href_out[0] = '\0';

    /* Find "href" (case-insensitive) */
    for (int i = 0; i + 4 < body_len; i++) {
        if (_tolower(tag_body[i])   == 'h' &&
            _tolower(tag_body[i+1]) == 'r' &&
            _tolower(tag_body[i+2]) == 'e' &&
            _tolower(tag_body[i+3]) == 'f') {
            int j = i + 4;
            /* Skip whitespace and '=' */
            while (j < body_len && _is_space(tag_body[j])) j++;
            if (j < body_len && tag_body[j] == '=') j++;
            while (j < body_len && _is_space(tag_body[j])) j++;

            /* Get quote char */
            char quote = '"';
            if (j < body_len && (tag_body[j] == '"' || tag_body[j] == '\'')) {
                quote = tag_body[j];
                j++;
            }

            /* Copy until closing quote or end */
            int out = 0;
            while (j < body_len && tag_body[j] != quote &&
                   tag_body[j] != '>' && out < href_max - 1) {
                href_out[out++] = tag_body[j++];
            }
            href_out[out] = '\0';
            return;
        }
    }
}

/*=============================================================================
 * SKIP SCRIPT/STYLE CONTENT
 *===========================================================================*/

/**
 * Skip content inside <script> or <style> blocks.
 * Advances p->pos past the closing </script> or </style>.
 */
static void _skip_block(html_parser_t *p, const char *close_tag) {
    int tag_len = 0;
    while (close_tag[tag_len]) tag_len++;

    int saved_pos = p->pos;

    while (p->pos + tag_len + 3 < p->len) {  /* need "</tag>" */
        if (p->src[p->pos] == '<' && p->src[p->pos + 1] == '/') {
            /* Check if this is the closing tag */
            int match = 1;
            for (int i = 0; i < tag_len; i++) {
                if (_tolower(p->src[p->pos + 2 + i]) != close_tag[i]) {
                    match = 0;
                    break;
                }
            }
            if (match && (p->src[p->pos + 2 + tag_len] == '>' ||
                          _is_space(p->src[p->pos + 2 + tag_len]))) {
                /* Skip past </tag> or </tag ...> */
                p->pos += 2 + tag_len;
                while (p->pos < p->len && p->src[p->pos] != '>') p->pos++;
                if (p->pos < p->len) p->pos++;
                return;
            }
        }
        p->pos++;
    }
    /* Closing tag not found (truncated response).
     * Skip to end — the block content (CSS/JS) would just create
     * thousands of useless text tokens if parsed as HTML. */
    p->pos = p->len;
}

/*=============================================================================
 * PARSER IMPLEMENTATION
 *===========================================================================*/

void html_parser_init(html_parser_t *p, const char *html, int html_len) {
    p->src = html;
    p->len = html_len;
    p->pos = 0;
    p->in_script = 0;
    p->in_style = 0;
    p->script_buf = (void *)0;
    p->script_len = 0;
    p->script_max = 0;
}

int html_parser_next(html_parser_t *p, html_token_t *tok) {
    if (p->pos >= p->len) return 0;

    tok->text[0] = '\0';
    tok->text_len = 0;
    tok->href[0] = '\0';
    tok->tag = HTML_TAG_UNKNOWN;

    /* Skip script/style blocks */
    if (p->in_script) {
        /* Capture script content if buffer is provided */
        if (p->script_buf && p->script_max > 0) {
            int start = p->pos;
            _skip_block(p, "script");
            /* Content is between start and the </script> tag */
            int content_end = p->pos;
            /* Walk back to find start of </script> from the block we just skipped */
            /* _skip_block left us after </script>, so measure from start to where
             * the closing tag began. We can compute: content is src[start..X]
             * where X is where </script> started. Scan backward. */
            int tag_start = content_end;
            /* After _skip_block, pos is past '>'. Walk back to find '<' */
            for (int k = content_end - 1; k >= start; k--) {
                if (p->src[k] == '<' && k + 1 < p->len && p->src[k + 1] == '/') {
                    tag_start = k;
                    break;
                }
            }
            int clen = tag_start - start;
            if (clen < 0) clen = 0;
            if (clen > p->script_max - 1) clen = p->script_max - 1;
            for (int k = 0; k < clen; k++) p->script_buf[k] = p->src[start + k];
            p->script_buf[clen] = '\0';
            p->script_len = clen;
        } else {
            _skip_block(p, "script");
            p->script_len = 0;
        }
        p->in_script = 0;
        /* Emit a synthetic close tag */
        tok->type = HTML_TOK_CLOSE;
        tok->tag = HTML_TAG_SCRIPT;
        return 1;
    }
    if (p->in_style) {
        _skip_block(p, "style");
        p->in_style = 0;
        tok->type = HTML_TOK_CLOSE;
        tok->tag = HTML_TAG_STYLE;
        return 1;
    }

    if (p->src[p->pos] == '<') {
        /*=== TAG TOKEN ===*/
        p->pos++; /* skip '<' */

        /* Skip comments: <!-- ... --> */
        if (p->pos + 2 < p->len &&
            p->src[p->pos] == '!' && p->src[p->pos+1] == '-' && p->src[p->pos+2] == '-') {
            p->pos += 3;
            while (p->pos + 2 < p->len) {
                if (p->src[p->pos] == '-' && p->src[p->pos+1] == '-' && p->src[p->pos+2] == '>') {
                    p->pos += 3;
                    return html_parser_next(p, tok); /* recurse to get next real token */
                }
                p->pos++;
            }
            /* Comment not closed (truncated response). Skip to end but try
             * to find any remaining '<' that might start valid HTML. */
            while (p->pos < p->len && p->src[p->pos] != '<') p->pos++;
            if (p->pos < p->len)
                return html_parser_next(p, tok);
            return 0;
        }

        /* Skip <!DOCTYPE ...> */
        if (p->pos < p->len && p->src[p->pos] == '!') {
            while (p->pos < p->len && p->src[p->pos] != '>') p->pos++;
            if (p->pos < p->len) p->pos++; /* skip '>' */
            return html_parser_next(p, tok);
        }

        /* Check for closing tag */
        int is_close = 0;
        if (p->pos < p->len && p->src[p->pos] == '/') {
            is_close = 1;
            p->pos++;
        }

        /* Read tag name */
        char tag_name[32];
        int tag_name_len = 0;
        while (p->pos < p->len && !_is_space(p->src[p->pos]) &&
               p->src[p->pos] != '>' && p->src[p->pos] != '/' &&
               tag_name_len < 31) {
            tag_name[tag_name_len++] = p->src[p->pos++];
        }
        tag_name[tag_name_len] = '\0';

        /* Remember attribute section start for href extraction */
        int attr_start = p->pos;

        /* Skip to end of tag '>' */
        while (p->pos < p->len && p->src[p->pos] != '>') p->pos++;
        int attr_end = p->pos;
        if (p->pos < p->len) p->pos++; /* skip '>' */

        /* Look up tag */
        uint8_t tag_id = _lookup_tag(tag_name, tag_name_len);

        tok->type = is_close ? HTML_TOK_CLOSE : HTML_TOK_OPEN;
        tok->tag = tag_id;

        /* Copy tag name for unknown tags */
        if (tag_id == HTML_TAG_UNKNOWN) {
            int i;
            for (i = 0; i < tag_name_len && i < HTML_MAX_TEXT - 1; i++)
                tok->text[i] = tag_name[i];
            tok->text[i] = '\0';
            tok->text_len = i;
        }

        /* Extract href for <a> tags */
        if (tag_id == HTML_TAG_A && !is_close) {
            _extract_href(p->src + attr_start, attr_end - attr_start,
                          tok->href, HTML_MAX_ATTR);
        }

        /* Handle script/style blocks */
        if (tag_id == HTML_TAG_SCRIPT && !is_close) p->in_script = 1;
        if (tag_id == HTML_TAG_STYLE && !is_close)  p->in_style = 1;

        return 1;
    } else {
        /*=== TEXT TOKEN ===*/
        tok->type = HTML_TOK_TEXT;
        tok->tag = HTML_TAG_UNKNOWN;

        int out = 0;
        int had_space = 0;

        while (p->pos < p->len && p->src[p->pos] != '<' && out < HTML_MAX_TEXT - 1) {
            char c = p->src[p->pos];

            /* Entity decoding */
            if (c == '&') {
                p->pos++;
                c = _decode_entity(p->src, p->len, &p->pos);
                if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                    if (!had_space && out > 0) {
                        tok->text[out++] = ' ';
                        had_space = 1;
                    }
                } else {
                    tok->text[out++] = c;
                    had_space = 0;
                }
                continue;
            }

            /* Whitespace collapsing */
            if (_is_space(c)) {
                if (!had_space && out > 0) {
                    tok->text[out++] = ' ';
                    had_space = 1;
                }
                p->pos++;
                continue;
            }

            tok->text[out++] = c;
            had_space = 0;
            p->pos++;
        }

        tok->text[out] = '\0';
        tok->text_len = out;

        /* Skip empty text tokens */
        if (out == 0) {
            return html_parser_next(p, tok);
        }

        return 1;
    }
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

int html_is_block_tag(uint8_t tag) {
    switch (tag) {
        case HTML_TAG_HTML: case HTML_TAG_HEAD: case HTML_TAG_BODY:
        case HTML_TAG_H1: case HTML_TAG_H2: case HTML_TAG_H3:
        case HTML_TAG_H4: case HTML_TAG_H5: case HTML_TAG_H6:
        case HTML_TAG_P: case HTML_TAG_DIV:
        case HTML_TAG_UL: case HTML_TAG_OL: case HTML_TAG_LI:
        case HTML_TAG_TABLE: case HTML_TAG_TR:
        case HTML_TAG_HEADER: case HTML_TAG_FOOTER: case HTML_TAG_NAV:
        case HTML_TAG_SECTION: case HTML_TAG_ARTICLE: case HTML_TAG_ASIDE:
        case HTML_TAG_MAIN: case HTML_TAG_FORM:
        case HTML_TAG_HR: case HTML_TAG_BR:
        case HTML_TAG_PRE: case HTML_TAG_TITLE:
            return 1;
        default:
            return 0;
    }
}

int html_is_void_tag(uint8_t tag) {
    switch (tag) {
        case HTML_TAG_BR: case HTML_TAG_HR:
        case HTML_TAG_IMG: case HTML_TAG_INPUT:
        case HTML_TAG_META: case HTML_TAG_LINK:
            return 1;
        default:
            return 0;
    }
}
