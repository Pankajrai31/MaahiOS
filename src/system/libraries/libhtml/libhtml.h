/**
 * MaahiOS HTML Tokenizer Library - libhtml.h
 *
 * Description:
 *   Lightweight streaming HTML tokenizer for MaahiOS browser.
 *   Pull-based: caller calls html_parser_next() repeatedly.
 *   Handles: basic tags, text, entities, script/style skipping.
 *
 * Usage:
 *   html_parser_t parser;
 *   html_parser_init(&parser, html_text, html_len);
 *   html_token_t tok;
 *   while (html_parser_next(&parser, &tok)) {
 *       // process tok
 *   }
 *
 * Layer 2 (Library). Ring 3.
 */

#ifndef LIBHTML_H
#define LIBHTML_H

#include <stdint.h>

/*=============================================================================
 * TAG IDS
 *===========================================================================*/

typedef enum {
    HTML_TAG_UNKNOWN = 0,
    HTML_TAG_HTML, HTML_TAG_HEAD, HTML_TAG_TITLE, HTML_TAG_BODY,
    HTML_TAG_H1, HTML_TAG_H2, HTML_TAG_H3, HTML_TAG_H4, HTML_TAG_H5, HTML_TAG_H6,
    HTML_TAG_P, HTML_TAG_BR, HTML_TAG_HR,
    HTML_TAG_B, HTML_TAG_STRONG, HTML_TAG_I, HTML_TAG_EM,
    HTML_TAG_A, HTML_TAG_UL, HTML_TAG_OL, HTML_TAG_LI,
    HTML_TAG_DIV, HTML_TAG_SPAN,
    HTML_TAG_TABLE, HTML_TAG_TR, HTML_TAG_TD, HTML_TAG_TH,
    HTML_TAG_META, HTML_TAG_LINK, HTML_TAG_STYLE, HTML_TAG_SCRIPT,
    HTML_TAG_IMG, HTML_TAG_PRE, HTML_TAG_CODE,
    HTML_TAG_HEADER, HTML_TAG_FOOTER, HTML_TAG_NAV, HTML_TAG_SECTION,
    HTML_TAG_ARTICLE, HTML_TAG_ASIDE, HTML_TAG_MAIN,
    HTML_TAG_FORM, HTML_TAG_INPUT, HTML_TAG_BUTTON_TAG,
    HTML_TAG_NOSCRIPT,
    HTML_TAG_DOCTYPE,
    HTML_TAG_COUNT
} html_tag_id_t;

/*=============================================================================
 * TOKEN TYPES
 *===========================================================================*/

#define HTML_TOK_TEXT    0   /* Plain text content */
#define HTML_TOK_OPEN    1   /* Opening tag <tag> */
#define HTML_TOK_CLOSE   2   /* Closing tag </tag> */

/*=============================================================================
 * TOKEN STRUCTURE
 *===========================================================================*/

#define HTML_MAX_TEXT    480
#define HTML_MAX_ATTR   128

typedef struct {
    uint8_t  type;                  /* HTML_TOK_TEXT, HTML_TOK_OPEN, HTML_TOK_CLOSE */
    uint8_t  tag;                   /* html_tag_id_t */
    int      text_len;              /* length of text[] content */
    char     text[HTML_MAX_TEXT];   /* text content or raw tag name for UNKNOWN */
    char     href[HTML_MAX_ATTR];   /* href attribute for <a> tags */
} html_token_t;

/*=============================================================================
 * PARSER STATE
 *===========================================================================*/

typedef struct {
    const char *src;
    int len;
    int pos;
    int in_script;   /* inside <script> block — skip content */
    int in_style;    /* inside <style> block — skip content */

    /* Script capture: if script_buf is non-NULL, script content is captured
     * into script_buf instead of being discarded. After a SCRIPT CLOSE token,
     * check script_len for the captured length. */
    char *script_buf;
    int   script_len;
    int   script_max;
} html_parser_t;

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

/**
 * html_parser_init — Initialize parser with HTML source text
 * @p:    Parser state
 * @html: HTML source (does not need to be null-terminated)
 * @html_len: Length of HTML source in bytes
 */
void html_parser_init(html_parser_t *p, const char *html, int html_len);

/**
 * html_parser_next — Get next token from parser
 * @p:   Parser state
 * @tok: Output token
 * @return 1 if token produced, 0 at end of input
 */
int html_parser_next(html_parser_t *p, html_token_t *tok);

/**
 * html_is_block_tag — Check if tag is a block-level element
 * Block elements cause line breaks before and after.
 */
int html_is_block_tag(uint8_t tag);

/**
 * html_is_void_tag — Check if tag is a void (self-closing) element
 * Void elements have no closing tag: <br>, <hr>, <img>, <meta>, <link>, <input>
 */
int html_is_void_tag(uint8_t tag);

#endif /* LIBHTML_H */
