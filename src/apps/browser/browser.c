/**
 * browser.mex - MaahiOS Web Browser
 *
 * Windowed GUI application. Fetches web pages via HTTP,
 * parses HTML, lays out content, and renders it in a scrollable view.
 *
 * Features:
 *   - URL bar with keyboard input
 *   - HTTP/1.0 GET via libhttp (DNS + TCP)
 *   - HTML tokenizer (libhtml) with entity decoding
 *   - Flow layout engine with word wrapping
 *   - Heading sizes (h1-h6), bold, italic, links, lists, <hr>
 *   - Vertical scrolling (arrow keys, Page Up/Down)
 *   - Status bar with loading state
 *
 * Layer 1 (App). Ring 3.
 * Uses: libwindow, libgui, libhttp, libnet, libhtml, liblog, libfs
 */

#include "../../system/libraries/libwindow/libwindow.h"
#include "../../system/libraries/libgui/libgui.h"
#include "../../system/libraries/libhttp/libhttp.h"
#include "../../system/libraries/libnet/libnet.h"
#include "../../system/libraries/libhtml/libhtml.h"
#include "../../system/libraries/libjs/libjs.h"
#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libfs/libfs.h"
#include "../../system/libraries/libwm/libwm.h"
#include "../../system/libraries/libwindow/theme.h"
#include "../../system/libraries/core/syscall_helpers.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define WIN_W           800
#define WIN_H           560

#define BROWSER_TOOLBAR_H   36
#define BROWSER_STATUSBAR_H 24
#define CONTENT_PAD     10

/* URL bar layout (within toolbar) */
#define URL_BAR_X       40
#define URL_BAR_Y       6
#define URL_BAR_H       24
#define GO_BTN_W        36
#define GO_BTN_H        24
#define GO_BTN_Y        6
#define RELOAD_BTN_W    24
#define RELOAD_BTN_H    24
#define RELOAD_BTN_GAP  4

/* Colors */
#define COL_URL_BG      0x00FFFFFF
#define COL_URL_BORDER  0x00B0B4C8
#define COL_URL_FOCUS   0x002B5BB5
#define COL_URL_TEXT    0x001A1A2E
#define COL_GO_BG       0x002B5BB5
#define COL_GO_TEXT     0x00FFFFFF
#define COL_RELOAD_BG   0x00606880
#define COL_TOOLBAR_BG  0x00E8EAF0
#define COL_TOOLBAR_SEP 0x00C0C4D0
#define COL_CONTENT_BG  0x00FFFFFF
#define COL_STATUS_BG   0x00E8EAF0
#define COL_STATUS_TEXT 0x005A5D76
#define COL_LINK        0x001A5CC8
#define COL_HEADING     0x001A1A2E
#define COL_BODY_TEXT   0x00333344
#define COL_ITALIC      0x005A5D76
#define COL_HRULE       0x00C0C4D0
#define COL_BULLET      0x005A5D76
#define COL_SCROLLBAR   0x00C0C4D0
#define COL_SCROLLTHUMB 0x00808498

/* Scroll */
#define SCROLL_STEP     30
#define SCROLL_PAGE     300

/* Render items */
#define MAX_RENDER_ITEMS  1024
#define TEXT_POOL_SIZE    65536
#define PAGE_BUF_SIZE     131072

/* Render item types */
#define RI_TEXT     0
#define RI_HRULE    1

/*=============================================================================
 * RENDER ITEM
 *===========================================================================*/

typedef struct {
    int16_t  x, y, w, h;
    uint32_t color;
    uint8_t  font_size;     /* font_size_t enum value */
    uint8_t  type;          /* RI_TEXT or RI_HRULE */
    uint8_t  underline;     /* 1 = draw underline (for links) */
    uint8_t  _pad;
    uint16_t text_off;      /* offset into g_text_pool */
    uint16_t text_len;      /* length of text */
} render_item_t;  /* 16 bytes */

/*=============================================================================
 * STATIC STATE
 *===========================================================================*/

/* URL bar */
static char g_url[256];
static int  g_url_len      = 0;
static int  g_url_cursor   = 0;
static int  g_url_focused  = 1;

/* Page data */
static char g_page_buf[PAGE_BUF_SIZE];
static int  g_page_len     = 0;

/* Text pool for render items */
static char g_text_pool[TEXT_POOL_SIZE];
static int  g_text_pool_used = 0;

/* Render items */
static render_item_t g_items[MAX_RENDER_ITEMS];
static int  g_item_count   = 0;

/* Scroll & layout */
static int  g_scroll_y     = 0;
static int  g_content_height = 0;  /* Total laid-out page height */

/* Browser state */
static int  g_state        = 0;    /* 0=idle, 1=loading */
static char g_status[128];
static char g_title[64];

/* Deferred navigation — set flag, execute in on_tick */
static int  g_nav_pending  = 0;

/* Loading animation */
static int  g_loading_tick = 0;   /* animation frame counter */
static int  g_loading_pos  = 0;   /* progress bar position (0..100) */

/* Cursor blink for URL bar */
static int  g_cursor_visible = 1;
static int  g_cursor_tick    = 0;

/* Window ref for invalidation */
static window_t *g_win = (void*)0;

/* JavaScript engine */
static js_env_t  g_js_env;
static char      g_js_out[8192];     /* document.write() output buffer */
static char      g_script_buf[8192]; /* captured <script> content */

/*=============================================================================
 * HELPERS
 *===========================================================================*/

static int _min(int a, int b) { return a < b ? a : b; }
static int _max(int a, int b) { return a > b ? a : b; }

static void _memset8(void *dst, uint8_t val, int size) {
    uint8_t *p = (uint8_t *)dst;
    for (int i = 0; i < size; i++) p[i] = val;
}

static int _strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void _strcpy(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static void _strncpy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/*=============================================================================
 * TEXT POOL
 *===========================================================================*/

static void pool_reset(void) {
    g_text_pool_used = 0;
    g_text_pool[0] = '\0';
}

/** Add null-terminated text to pool. Returns offset. */
static uint16_t pool_add(const char *text, int len) {
    if (g_text_pool_used + len + 1 > TEXT_POOL_SIZE)
        return 0; /* overflow — return offset 0 (empty string) */
    uint16_t off = (uint16_t)g_text_pool_used;
    for (int i = 0; i < len; i++)
        g_text_pool[g_text_pool_used++] = text[i];
    g_text_pool[g_text_pool_used++] = '\0';
    return off;
}

static const char *pool_get(uint16_t off) {
    return &g_text_pool[off];
}

/*=============================================================================
 * LAYOUT ENGINE
 *===========================================================================*/

typedef struct {
    int cursor_x;       /* Current x position */
    int cursor_y;       /* Current y position (top of current line) */
    int line_height;    /* Height of tallest item on current line */
    int max_width;      /* Available width for content */
    int left_margin;    /* Left margin (indentation) */

    /* Style state */
    uint8_t  font_size; /* font_size_t enum */
    uint32_t color;
    int      bold;
    int      italic;
    int      in_link;
    int      in_head;
    int      in_title;
    int      in_pre;
    int      in_noscript; /* inside <noscript>: render content even in head */
    int      list_depth;

    /* Line accumulation buffer */
    char line_buf[512];
    int  line_len;
    int  line_start_x;
} layout_state_t;

static layout_state_t g_layout;

/** Emit the current line buffer as a render item */
static void layout_emit_line(void) {
    if (g_layout.line_len == 0) return;
    if (g_item_count >= MAX_RENDER_ITEMS) return;

    render_item_t *ri = &g_items[g_item_count];
    ri->type = RI_TEXT;
    ri->x = (int16_t)g_layout.line_start_x;
    ri->y = (int16_t)g_layout.cursor_y;
    ri->font_size = g_layout.font_size;
    ri->underline = g_layout.in_link ? 1 : 0;

    if (g_layout.in_link) {
        ri->color = COL_LINK;
    } else if (g_layout.italic) {
        ri->color = COL_ITALIC;
    } else {
        ri->color = g_layout.color;
    }

    /* Measure the line */
    g_layout.line_buf[g_layout.line_len] = '\0';
    ri->w = (int16_t)surface_measure_text(g_layout.line_buf, (font_size_t)g_layout.font_size);
    ri->h = (int16_t)surface_text_height((font_size_t)g_layout.font_size);

    ri->text_off = pool_add(g_layout.line_buf, g_layout.line_len);
    ri->text_len = (uint16_t)g_layout.line_len;

    g_item_count++;

    /* Update line height */
    if (ri->h > g_layout.line_height)
        g_layout.line_height = ri->h;

    /* Advance cursor_x past this text */
    g_layout.cursor_x = g_layout.line_start_x + ri->w;
    g_layout.line_len = 0;
}

/** Start a new line */
static void layout_newline(void) {
    layout_emit_line();
    if (g_layout.line_height == 0)
        g_layout.line_height = surface_text_height((font_size_t)g_layout.font_size);
    g_layout.cursor_y += g_layout.line_height + 2;
    g_layout.cursor_x = g_layout.left_margin;
    g_layout.line_start_x = g_layout.cursor_x;
    g_layout.line_height = 0;
}

/** Add a word to the current line, wrapping if needed */
static void layout_add_word(const char *word, int word_len) {
    if (word_len == 0) return;

    /* Measure the word */
    char tmp[256];
    int copy_len = _min(word_len, 255);
    for (int i = 0; i < copy_len; i++) tmp[i] = word[i];
    tmp[copy_len] = '\0';

    int word_w = surface_measure_text(tmp, (font_size_t)g_layout.font_size);

    /* Check if we need to wrap */
    int line_w = 0;
    if (g_layout.line_len > 0) {
        g_layout.line_buf[g_layout.line_len] = '\0';
        line_w = surface_measure_text(g_layout.line_buf, (font_size_t)g_layout.font_size);
    }

    if (g_layout.cursor_x + line_w + word_w > g_layout.left_margin + g_layout.max_width &&
        g_layout.line_len > 0) {
        /* Wrap: emit current line and start new one */
        layout_newline();
    }

    /* Add space before word if line already has content */
    if (g_layout.line_len > 0 && g_layout.line_len < 510) {
        g_layout.line_buf[g_layout.line_len++] = ' ';
    }

    /* Append word to line buffer */
    for (int i = 0; i < word_len && g_layout.line_len < 510; i++) {
        g_layout.line_buf[g_layout.line_len++] = word[i];
    }
}

/** Process a text token — split into words and lay out */
static void layout_process_text(const char *text, int text_len) {
    if (g_layout.in_head && !g_layout.in_title) return; /* skip non-title head content */

    /* Debug: log every visible text token via direct kernel syscall */
    if (text_len > 0 && text_len < 60 && !g_layout.in_title) {
        char dbg[80] = "TEXT[";
        int di = 5;
        for (int k = 0; k < text_len && di < 70; k++) dbg[di++] = text[k];
        dbg[di++] = ']'; dbg[di] = '\0';
        syscall3(240, 3, (int)"LAYOUT", (int)dbg);
    }

    if (g_layout.in_title) {
        /* Capture title text */
        _strncpy(g_title, text, _min(text_len + 1, 63));
        return;
    }

    int i = 0;
    while (i < text_len) {
        /* Skip leading spaces */
        while (i < text_len && (text[i] == ' ' || text[i] == '\t')) i++;
        if (i >= text_len) break;

        /* Find word end */
        int word_start = i;
        while (i < text_len && text[i] != ' ' && text[i] != '\t') i++;

        layout_add_word(text + word_start, i - word_start);
    }
}

/** Handle opening block tag */
static void layout_block_open(uint8_t tag) {
    switch (tag) {
        case HTML_TAG_HEAD:
            g_layout.in_head = 1;
            break;

        case HTML_TAG_TITLE:
            g_layout.in_title = 1;
            break;

        case HTML_TAG_BODY:
            g_layout.in_head = 0;
            break;

        case HTML_TAG_NOSCRIPT:
            break;

        case HTML_TAG_H1:
            layout_newline();
            g_layout.cursor_y += 8; /* extra spacing before heading */
            g_layout.font_size = FONT_TITLE;
            g_layout.color = COL_HEADING;
            g_layout.bold = 1;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_H2:
            layout_newline();
            g_layout.cursor_y += 6;
            g_layout.font_size = FONT_H2;
            g_layout.color = COL_HEADING;
            g_layout.bold = 1;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_H3:
            layout_newline();
            g_layout.cursor_y += 4;
            g_layout.font_size = FONT_H3;
            g_layout.color = COL_HEADING;
            g_layout.bold = 1;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_H4: case HTML_TAG_H5: case HTML_TAG_H6:
            layout_newline();
            g_layout.cursor_y += 2;
            g_layout.font_size = FONT_BODY;
            g_layout.color = COL_HEADING;
            g_layout.bold = 1;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_P:
            layout_newline();
            g_layout.cursor_y += 6; /* paragraph spacing */
            g_layout.font_size = FONT_SMALL;
            g_layout.color = COL_BODY_TEXT;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_DIV: case HTML_TAG_SECTION: case HTML_TAG_ARTICLE:
        case HTML_TAG_MAIN: case HTML_TAG_HEADER: case HTML_TAG_FOOTER:
        case HTML_TAG_NAV: case HTML_TAG_ASIDE:
            layout_newline();
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_UL: case HTML_TAG_OL:
            layout_newline();
            g_layout.list_depth++;
            g_layout.left_margin += 20;
            g_layout.cursor_x = g_layout.left_margin;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_LI:
            layout_newline();
            g_layout.line_start_x = g_layout.cursor_x;
            /* Add bullet point */
            layout_add_word("\xB7", 1); /* middle dot as bullet */
            break;

        case HTML_TAG_BR:
            layout_newline();
            break;

        case HTML_TAG_HR:
            layout_newline();
            g_layout.cursor_y += 4;
            if (g_item_count < MAX_RENDER_ITEMS) {
                render_item_t *ri = &g_items[g_item_count++];
                ri->type = RI_HRULE;
                ri->x = (int16_t)g_layout.left_margin;
                ri->y = (int16_t)g_layout.cursor_y;
                ri->w = (int16_t)g_layout.max_width;
                ri->h = 1;
                ri->color = COL_HRULE;
                ri->font_size = 0;
                ri->underline = 0;
                ri->text_off = 0;
                ri->text_len = 0;
            }
            g_layout.cursor_y += 6;
            g_layout.cursor_x = g_layout.left_margin;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_B: case HTML_TAG_STRONG:
            layout_emit_line(); /* flush before style change */
            g_layout.bold = 1;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_I: case HTML_TAG_EM:
            layout_emit_line();
            g_layout.italic = 1;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_A:
            layout_emit_line();
            g_layout.in_link = 1;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_PRE: case HTML_TAG_CODE:
            layout_newline();
            g_layout.in_pre = 1;
            break;

        default:
            break;
    }
}

/** Handle closing block tag */
static void layout_block_close(uint8_t tag) {
    switch (tag) {
        case HTML_TAG_HEAD:
            g_layout.in_head = 0;
            break;

        case HTML_TAG_NOSCRIPT:
            break;

        case HTML_TAG_TITLE:
            g_layout.in_title = 0;
            break;

        case HTML_TAG_H1: case HTML_TAG_H2: case HTML_TAG_H3:
        case HTML_TAG_H4: case HTML_TAG_H5: case HTML_TAG_H6:
            layout_newline();
            g_layout.cursor_y += 4; /* spacing after heading */
            g_layout.font_size = FONT_SMALL;
            g_layout.color = COL_BODY_TEXT;
            g_layout.bold = 0;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_P:
            layout_newline();
            g_layout.cursor_y += 4;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_DIV: case HTML_TAG_SECTION: case HTML_TAG_ARTICLE:
        case HTML_TAG_MAIN: case HTML_TAG_HEADER: case HTML_TAG_FOOTER:
        case HTML_TAG_NAV: case HTML_TAG_ASIDE:
            layout_newline();
            break;

        case HTML_TAG_UL: case HTML_TAG_OL:
            layout_newline();
            if (g_layout.list_depth > 0) {
                g_layout.list_depth--;
                g_layout.left_margin -= 20;
                if (g_layout.left_margin < CONTENT_PAD)
                    g_layout.left_margin = CONTENT_PAD;
            }
            g_layout.cursor_x = g_layout.left_margin;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_LI:
            layout_newline();
            break;

        case HTML_TAG_B: case HTML_TAG_STRONG:
            layout_emit_line();
            g_layout.bold = 0;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_I: case HTML_TAG_EM:
            layout_emit_line();
            g_layout.italic = 0;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_A:
            layout_emit_line();
            g_layout.in_link = 0;
            g_layout.line_start_x = g_layout.cursor_x;
            break;

        case HTML_TAG_PRE: case HTML_TAG_CODE:
            layout_newline();
            g_layout.in_pre = 0;
            break;

        default:
            break;
    }
}

/** Process a single HTML token through the layout engine */
static void layout_process_token(const html_token_t *tok) {
    switch (tok->type) {
        case HTML_TOK_TEXT:
            layout_process_text(tok->text, tok->text_len);
            break;
        case HTML_TOK_OPEN:
            layout_block_open(tok->tag);
            break;
        case HTML_TOK_CLOSE:
            layout_block_close(tok->tag);
            break;
    }
}

/** Initialize layout engine for a given content width */
static void layout_init(int content_width) {
    _memset8(&g_layout, 0, sizeof(g_layout));
    g_layout.cursor_x = CONTENT_PAD;
    g_layout.cursor_y = CONTENT_PAD;
    g_layout.max_width = content_width - CONTENT_PAD * 2;
    g_layout.left_margin = CONTENT_PAD;
    g_layout.font_size = FONT_SMALL;
    g_layout.color = COL_BODY_TEXT;
    g_layout.line_start_x = CONTENT_PAD;
}

/** Feed HTML fragment through the tokenizer into the layout engine */
static void layout_feed_html(const char *html, int html_len) {
    html_parser_t p2;
    html_parser_init(&p2, html, html_len);
    html_token_t t2;
    while (html_parser_next(&p2, &t2))
        layout_process_token(&t2);
}

/** Run the full layout: tokenize HTML and produce render items */
static void layout_page(const char *html, int html_len, int content_width) {
    /* Reset state */
    g_item_count = 0;
    pool_reset();
    g_title[0] = '\0';
    layout_init(content_width);

    /* Debug: dump first 200 chars of raw HTML via direct kernel syscall */
    {
        char raw[204];
        int rl = html_len < 200 ? html_len : 200;
        for (int k = 0; k < rl; k++) raw[k] = html[k];
        raw[rl] = '\0';
        syscall3(240, 3, (int)"RAWHTML", (int)raw);
    }

    /* Initialize JS engine (fresh per page load) */
    js_init(&g_js_env, g_js_out, sizeof(g_js_out));

    /* Tokenize and lay out */
    html_parser_t parser;
    html_parser_init(&parser, html, html_len);
    /* Enable script capture */
    parser.script_buf = g_script_buf;
    parser.script_max = sizeof(g_script_buf);

    html_token_t tok;
    while (html_parser_next(&parser, &tok)) {
        /* Execute captured script on </script> close */
        if (tok.type == HTML_TOK_CLOSE && tok.tag == HTML_TAG_SCRIPT &&
            parser.script_len > 0) {
            liblog_hex(LOG_INFO, "JS", "Executing script, len:",
                       (uint32_t)parser.script_len);
            g_js_env.out_len = 0;
            int rc = js_exec(&g_js_env, g_script_buf, parser.script_len);
            if (rc != 0 && g_js_env.error[0]) {
                liblog(LOG_WARN, "JS", g_js_env.error);
            }
            /* If document.write() produced output, feed it into layout */
            if (g_js_env.out_len > 0) {
                liblog_hex(LOG_INFO, "JS", "document.write output len:",
                           (uint32_t)g_js_env.out_len);
                layout_feed_html(g_js_out, g_js_env.out_len);
            }
            parser.script_len = 0;
        }

        layout_process_token(&tok);
    }

    /* Flush final line */
    layout_emit_line();

    /* Record total content height */
    g_content_height = g_layout.cursor_y + g_layout.line_height + CONTENT_PAD;

    /* If nothing rendered, show a helpful message */
    if (g_item_count == 0 && html_len > 0) {
        g_layout.cursor_y = CONTENT_PAD + 20;
        g_layout.font_size = FONT_BODY;
        g_layout.color = COL_BODY_TEXT;
        const char *msg1 = "This page could not be rendered.";
        const char *msg2 = "The page content could not be displayed.";
        g_layout.line_len = 0;
        for (int i = 0; msg1[i]; i++) g_layout.line_buf[g_layout.line_len++] = msg1[i];
        layout_emit_line();
        g_layout.cursor_y += 20;
        g_layout.line_len = 0;
        for (int i = 0; msg2[i]; i++) g_layout.line_buf[g_layout.line_len++] = msg2[i];
        layout_emit_line();
        g_content_height = g_layout.cursor_y + 40;
    }
}

/*=============================================================================
 * NAVIGATION
 *===========================================================================*/

static void set_status(const char *msg) {
    _strncpy(g_status, msg, 127);
    if (g_win) window_invalidate(g_win);
}

/**
 * Heartbeat callback — invoked by libhttp/libnet during blocking HTTP calls.
 * Writes directly to WM's SHM heartbeat table via libwm_heartbeat(),
 * keeping the window alive while navigate() blocks the event loop.
 */
static void browser_heartbeat(void *ctx) {
    (void)ctx;
    if (!g_win) return;
    libwm_heartbeat(g_win->wm_handle);
}

static void navigate(void) {
    if (g_url_len == 0) return;

    /* Register heartbeat so network operations keep WM alive */
    libhttp_set_heartbeat(browser_heartbeat, (void*)0);

    /* Auto-prepend http:// if no scheme present; accept https:// natively */
    {
        int has_scheme = 0;
        if (g_url_len > 7 && g_url[0]=='h' && g_url[1]=='t' && g_url[2]=='t' &&
            g_url[3]=='p') {
            if (g_url[4]==':' && g_url[5]=='/' && g_url[6]=='/')
                has_scheme = 1;  /* http:// */
            if (g_url[4]=='s' && g_url_len > 8 &&
                g_url[5]==':' && g_url[6]=='/' && g_url[7]=='/')
                has_scheme = 1;  /* https:// — supported via libtls */
        }
        if (!has_scheme) {
            /* Shift existing URL right by 7 bytes to insert "http://" */
            if (g_url_len + 7 < 254) {
                for (int i = g_url_len; i >= 0; i--)
                    g_url[i + 7] = g_url[i];
                g_url[0]='h'; g_url[1]='t'; g_url[2]='t'; g_url[3]='p';
                g_url[4]=':'; g_url[5]='/'; g_url[6]='/';
                g_url_len += 7;
                g_url_cursor = g_url_len;
            }
        }
    }

    liblog(LOG_INFO, "BROWSER", "Navigating...");

    g_state = 1;
    set_status("Connecting...");
    g_scroll_y = 0;

    /* Clear old page content so user sees a blank page while loading */
    g_item_count = 0;
    pool_reset();
    g_content_height = 0;
    g_title[0] = '\0';
    if (g_win) window_invalidate(g_win);

    /* Fetch the page */
    libhttp_response_t resp;
    _memset8(&resp, 0, sizeof(resp));
    _memset8(g_page_buf, 0, 64); /* clear start */

    set_status("Connecting...");
    g_page_len = libhttp_get(g_url, g_page_buf, PAGE_BUF_SIZE - 1, &resp);

    if (g_page_len < 0) {
        g_state = 0;

        /* User-friendly error messages */
        const char *err_msg;
        const char *err_detail = (const char*)0;
        if (g_page_len == -10) {
            err_msg = "HTTPS connection failed";
            err_detail = "TLS handshake with the server could not complete.";
        } else if (g_page_len == -1) {
            err_msg = "Could not connect to server";
            err_detail = "DNS lookup or TCP connection failed.";
        } else if (g_page_len == -2) {
            err_msg = "Failed to send request";
        } else if (g_page_len == -3) {
            err_msg = "No response from server";
        } else if (g_page_len == -4) {
            err_msg = "Invalid URL";
        } else {
            err_msg = "Fetch failed";
        }
        set_status(err_msg);

        /* Build an error page in the content area */
        g_item_count = 0;
        pool_reset();

        int y = CONTENT_PAD;

        /* Title */
        {
            const char *title = err_msg;
            int tlen = 0; while (title[tlen]) tlen++;
            render_item_t *ri = &g_items[g_item_count++];
            ri->type = RI_TEXT; ri->x = CONTENT_PAD; ri->y = (int16_t)y;
            ri->font_size = FONT_H2; ri->color = 0x00CC3333;
            ri->underline = 0; ri->_pad = 0;
            ri->text_off = pool_add(title, tlen);
            ri->text_len = (uint16_t)tlen;
            ri->w = (int16_t)surface_measure_text(title, FONT_H2);
            ri->h = (int16_t)surface_text_height(FONT_H2);
            y += ri->h + 8;
        }

        /* Detail line */
        if (err_detail && g_item_count < MAX_RENDER_ITEMS) {
            int dlen = 0; while (err_detail[dlen]) dlen++;
            render_item_t *ri = &g_items[g_item_count++];
            ri->type = RI_TEXT; ri->x = CONTENT_PAD; ri->y = (int16_t)y;
            ri->font_size = FONT_BODY; ri->color = COL_BODY_TEXT;
            ri->underline = 0; ri->_pad = 0;
            ri->text_off = pool_add(err_detail, dlen);
            ri->text_len = (uint16_t)dlen;
            ri->w = (int16_t)surface_measure_text(err_detail, FONT_BODY);
            ri->h = (int16_t)surface_text_height(FONT_BODY);
            y += ri->h + 6;
        }

        /* HTTPS-specific: extra explanation */
        if (g_page_len == -10 && g_item_count < MAX_RENDER_ITEMS) {
            const char *note = "The server may use unsupported TLS features.";
            int nlen = 0; while (note[nlen]) nlen++;
            render_item_t *ri = &g_items[g_item_count++];
            ri->type = RI_TEXT; ri->x = CONTENT_PAD; ri->y = (int16_t)y;
            ri->font_size = FONT_BODY; ri->color = COL_BODY_TEXT;
            ri->underline = 0; ri->_pad = 0;
            ri->text_off = pool_add(note, nlen);
            ri->text_len = (uint16_t)nlen;
            ri->w = (int16_t)surface_measure_text(note, FONT_BODY);
            ri->h = (int16_t)surface_text_height(FONT_BODY);
            y += ri->h + 16;
        } else {
            y += 16;
        }

        /* Suggestion */
        if (g_item_count < MAX_RENDER_ITEMS) {
            const char *sug = "Try these HTTP sites:";
            int slen = 0; while (sug[slen]) slen++;
            render_item_t *ri = &g_items[g_item_count++];
            ri->type = RI_TEXT; ri->x = CONTENT_PAD; ri->y = (int16_t)y;
            ri->font_size = FONT_BODY; ri->color = COL_HEADING;
            ri->underline = 0; ri->_pad = 0;
            ri->text_off = pool_add(sug, slen);
            ri->text_len = (uint16_t)slen;
            ri->w = (int16_t)surface_measure_text(sug, FONT_BODY);
            ri->h = (int16_t)surface_text_height(FONT_BODY);
            y += ri->h + 4;
        }

        /* Example URLs */
        static const char *examples[] = {
            "http://example.com",
            "http://info.cern.ch",
            "http://neverssl.com",
            (const char*)0
        };
        for (int ei = 0; examples[ei] && g_item_count < MAX_RENDER_ITEMS; ei++) {
            const char *ex = examples[ei];
            int elen = 0; while (ex[elen]) elen++;
            render_item_t *ri = &g_items[g_item_count++];
            ri->type = RI_TEXT;
            ri->x = CONTENT_PAD + 16;
            ri->y = (int16_t)y;
            ri->font_size = FONT_BODY; ri->color = COL_LINK;
            ri->underline = 1; ri->_pad = 0;
            ri->text_off = pool_add(ex, elen);
            ri->text_len = (uint16_t)elen;
            ri->w = (int16_t)surface_measure_text(ex, FONT_BODY);
            ri->h = (int16_t)surface_text_height(FONT_BODY);
            y += ri->h + 2;
        }

        g_content_height = y + CONTENT_PAD;
        if (g_win) window_invalidate(g_win);
        return;
    }

    g_page_buf[g_page_len] = '\0';

    /* Lay out the page */
    set_status("Rendering...");
    int cw = g_win ? g_win->content_w : (WIN_W);
    layout_page(g_page_buf, g_page_len, cw);

    g_state = 0;

    /* Build status string */
    char status_buf[128] = "Done - ";
    int si = 7;
    char nbuf[12];
    {
        int val = g_page_len;
        char t[12]; int tn = 0;
        if (val == 0) { t[tn++] = '0'; }
        else { while (val > 0) { t[tn++] = '0' + (val%10); val /= 10; } }
        for (int i = tn-1; i >= 0 && si < 120; i--) status_buf[si++] = t[i];
    }
    _strcpy(status_buf + si, " bytes");
    set_status(status_buf);

    /* Update window title if we got a page title */
    if (g_title[0] && g_win) {
        /* Compose "title - MaahiOS Browser" */
        char title_buf[80];
        _strncpy(title_buf, g_title, 60);
        int tl = _strlen(title_buf);
        _strcpy(title_buf + tl, " - Browser");
        /* Can't change window title dynamically in current libwindow,
         * so just show in status */
    }

    if (g_win) window_invalidate(g_win);
}

/*=============================================================================
 * RENDERING
 *===========================================================================*/

/** Draw the toolbar (URL bar + Go button) */
static void draw_toolbar(surface_t *surf, int cx, int cy, int cw) {
    /* Toolbar background */
    surface_fill_rect(surf, cx, cy, cw, BROWSER_TOOLBAR_H, COL_TOOLBAR_BG);

    /* Back button area (future) */
    surface_draw_text(surf, cx + 8, cy + 10, "<", FONT_BODY, COL_URL_TEXT);

    /* URL bar */
    int url_w = cw - URL_BAR_X - GO_BTN_W - RELOAD_BTN_W - RELOAD_BTN_GAP - 16;
    int url_x = cx + URL_BAR_X;
    int url_y = cy + URL_BAR_Y;

    /* URL bar background + border (sunken inset) */
    surface_fill_rect(surf, url_x, url_y, url_w, URL_BAR_H, COL_URL_BG);
    /* Sunken bevel: dark on top/left, light on bottom/right */
    surface_draw_hline(surf, url_x, url_y, url_w, THEME_BEVEL_DARK);
    surface_draw_vline(surf, url_x, url_y, URL_BAR_H, THEME_BEVEL_DARK);
    surface_draw_hline(surf, url_x, url_y + URL_BAR_H - 1, url_w, THEME_BEVEL_LIGHT);
    surface_draw_vline(surf, url_x + url_w - 1, url_y, URL_BAR_H, THEME_BEVEL_LIGHT);
    if (g_url_focused) {
        /* Focus highlight: inner accent border */
        surface_draw_hline(surf, url_x + 1, url_y + 1, url_w - 2, THEME_ACCENT);
        surface_draw_vline(surf, url_x + 1, url_y + 1, URL_BAR_H - 2, THEME_ACCENT);
        surface_draw_hline(surf, url_x + 1, url_y + URL_BAR_H - 2, url_w - 2, THEME_ACCENT);
        surface_draw_vline(surf, url_x + url_w - 2, url_y + 1, URL_BAR_H - 2, THEME_ACCENT);
    }

    /* URL text */
    if (g_url_len > 0) {
        /* Clip text to URL bar width (simple: just draw it) */
        surface_draw_text(surf, url_x + 4, url_y + 4, g_url,
                          FONT_SMALL, COL_URL_TEXT);
    } else if (!g_url_focused) {
        surface_draw_text(surf, url_x + 4, url_y + 4, "Enter URL...",
                          FONT_SMALL, 0x009CA0B4);
    }

    /* Cursor in URL bar */
    if (g_url_focused && g_cursor_visible) {
        char tmp[256];
        _strncpy(tmp, g_url, _min(g_url_cursor + 1, 255));
        tmp[g_url_cursor] = '\0';
        int cursor_x = url_x + 4 + surface_measure_text(tmp, FONT_SMALL);
        surface_fill_rect(surf, cursor_x, url_y + 4, 1,
                          surface_text_height(FONT_SMALL), COL_URL_TEXT);
    }

    /* Go button — ACCENT style: blue gradient, raised bevel, white text */
    int reload_x = cx + cw - RELOAD_BTN_W - 6;
    int go_x = reload_x - GO_BTN_W - RELOAD_BTN_GAP;
    int go_y = cy + GO_BTN_Y;
    /* Blue gradient fill */
    for (int row = 0; row < GO_BTN_H; row++) {
        uint32_t c = THEME_ACCENT_LIGHT;
        if (GO_BTN_H > 1) {
            int r0 = (THEME_ACCENT_LIGHT >> 16) & 0xFF, g0 = (THEME_ACCENT_LIGHT >> 8) & 0xFF, b0 = THEME_ACCENT_LIGHT & 0xFF;
            int r1 = (THEME_ACCENT_DARK >> 16) & 0xFF, g1 = (THEME_ACCENT_DARK >> 8) & 0xFF, b1 = THEME_ACCENT_DARK & 0xFF;
            int r = r0 + (r1 - r0) * row / (GO_BTN_H - 1);
            int g = g0 + (g1 - g0) * row / (GO_BTN_H - 1);
            int b = b0 + (b1 - b0) * row / (GO_BTN_H - 1);
            c = (uint32_t)(((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
        }
        surface_draw_hline(surf, go_x + 2, go_y + row, GO_BTN_W - 4, c);
    }
    /* Raised bevel */
    surface_draw_hline(surf, go_x, go_y, GO_BTN_W, THEME_ACCENT_LIGHT);
    surface_draw_hline(surf, go_x + 1, go_y + 1, GO_BTN_W - 2, THEME_ACCENT_LIGHT);
    surface_draw_vline(surf, go_x, go_y, GO_BTN_H, THEME_ACCENT_LIGHT);
    surface_draw_vline(surf, go_x + 1, go_y + 1, GO_BTN_H - 2, THEME_ACCENT_LIGHT);
    surface_draw_hline(surf, go_x, go_y + GO_BTN_H - 1, GO_BTN_W, THEME_ACCENT_DARK);
    surface_draw_hline(surf, go_x + 1, go_y + GO_BTN_H - 2, GO_BTN_W - 2, THEME_ACCENT_DARK);
    surface_draw_vline(surf, go_x + GO_BTN_W - 1, go_y, GO_BTN_H, THEME_ACCENT_DARK);
    surface_draw_vline(surf, go_x + GO_BTN_W - 2, go_y + 1, GO_BTN_H - 2, THEME_ACCENT_DARK);
    /* Centered text */
    {
        int tw = surface_measure_text("Go", FONT_SMALL);
        int th = surface_text_height(FONT_SMALL);
        surface_draw_text(surf, go_x + (GO_BTN_W - tw) / 2, go_y + (GO_BTN_H - th) / 2, "Go", FONT_SMALL, THEME_TEXT_INVERSE);
    }

    /* Reload button — STANDARD style: chrome bg, raised bevel */
    int reload_y = cy + GO_BTN_Y;
    surface_fill_rect(surf, reload_x, reload_y, RELOAD_BTN_W, RELOAD_BTN_H, THEME_CHROME);
    /* Raised bevel */
    surface_draw_hline(surf, reload_x, reload_y, RELOAD_BTN_W, THEME_BEVEL_LIGHT);
    surface_draw_hline(surf, reload_x + 1, reload_y + 1, RELOAD_BTN_W - 2, THEME_BEVEL_LIGHT);
    surface_draw_vline(surf, reload_x, reload_y, RELOAD_BTN_H, THEME_BEVEL_LIGHT);
    surface_draw_vline(surf, reload_x + 1, reload_y + 1, RELOAD_BTN_H - 2, THEME_BEVEL_LIGHT);
    surface_draw_hline(surf, reload_x, reload_y + RELOAD_BTN_H - 1, RELOAD_BTN_W, THEME_BEVEL_DARK);
    surface_draw_hline(surf, reload_x + 1, reload_y + RELOAD_BTN_H - 2, RELOAD_BTN_W - 2, THEME_BEVEL_DARK);
    surface_draw_vline(surf, reload_x + RELOAD_BTN_W - 1, reload_y, RELOAD_BTN_H, THEME_BEVEL_DARK);
    surface_draw_vline(surf, reload_x + RELOAD_BTN_W - 2, reload_y + 1, RELOAD_BTN_H - 2, THEME_BEVEL_DARK);
    /* Centered "R" */
    {
        int tw = surface_measure_text("R", FONT_SMALL);
        int th = surface_text_height(FONT_SMALL);
        surface_draw_text(surf, reload_x + (RELOAD_BTN_W - tw) / 2, reload_y + (RELOAD_BTN_H - th) / 2, "R", FONT_SMALL, THEME_TEXT);
    }

    /* Toolbar separator */
    surface_draw_hline(surf, cx, cy + BROWSER_TOOLBAR_H - 1, cw, COL_TOOLBAR_SEP);
}

/** Draw the HTML content area */
static void draw_content(surface_t *surf, int cx, int cy, int cw, int ch) {
    /* Clear content area */
    surface_fill_rect(surf, cx, cy, cw, ch, COL_CONTENT_BG);

    if (g_item_count == 0 && g_state == 0) {
        /* Empty state — show welcome text */
        surface_draw_text(surf, cx + 20, cy + 20,
                          "MaahiOS Browser", FONT_TITLE, COL_HEADING);
        surface_draw_text(surf, cx + 20, cy + 56,
                          "Type a URL in the address bar and press Enter.",
                          FONT_BODY, COL_BODY_TEXT);
        surface_draw_text(surf, cx + 20, cy + 80,
                          "Try: http://example.com",
                          FONT_SMALL, COL_LINK);
        return;
    }

    if (g_state == 1) {
        surface_draw_text(surf, cx + 20, cy + 20,
                          "Loading...", FONT_BODY, COL_BODY_TEXT);
        return;
    }

    /* Render visible items with scroll offset */
    for (int i = 0; i < g_item_count; i++) {
        render_item_t *ri = &g_items[i];

        int ry = (int)ri->y - g_scroll_y;

        /* Clip: skip items outside visible area */
        if (ry + ri->h < 0) continue;
        if (ry > ch) break; /* items are sorted by y, so we can stop */

        int rx = cx + (int)ri->x;
        int draw_y = cy + ry;

        if (ri->type == RI_HRULE) {
            surface_draw_hline(surf, rx, draw_y, ri->w, ri->color);
        } else if (ri->type == RI_TEXT) {
            const char *text = pool_get(ri->text_off);
            surface_draw_text(surf, rx, draw_y, text,
                              (font_size_t)ri->font_size, ri->color);
            /* Underline for links */
            if (ri->underline) {
                int uh = surface_text_height((font_size_t)ri->font_size);
                surface_draw_hline(surf, rx, draw_y + uh - 1, ri->w, ri->color);
            }
        }
    }

    /* Scrollbar */
    if (g_content_height > ch) {
        int sb_x = cx + cw - 8;
        int sb_h = ch;
        surface_fill_rect(surf, sb_x, cy, 8, sb_h, COL_SCROLLBAR);

        /* Thumb */
        int thumb_h = _max(20, (ch * ch) / g_content_height);
        int thumb_y = (g_scroll_y * (sb_h - thumb_h)) /
                      _max(1, g_content_height - ch);
        surface_fill_rect(surf, sb_x + 1, cy + thumb_y, 6, thumb_h, COL_SCROLLTHUMB);
    }
}

/** Draw the status bar — dark themed with loading animation */
static void draw_statusbar(surface_t *surf, int cx, int cy, int cw) {
    /* Dark background matching THEME_PRIMARY_DARK */
    surface_fill_rect(surf, cx, cy, cw, BROWSER_STATUSBAR_H, THEME_PRIMARY_DARK);
    /* Top separator */
    surface_draw_hline(surf, cx, cy, cw, 0x002A3444);

    if (g_state == 1) {
        /* Loading state: animated progress bar + text */
        /* Progress bar background */
        int bar_x = cx + 8;
        int bar_y = cy + (BROWSER_STATUSBAR_H - 6) / 2;
        int bar_max_w = 120;
        surface_fill_rect(surf, bar_x, bar_y, bar_max_w, 6, 0x002A3444);

        /* Animated progress bar (bouncing) */
        int bar_w = 40;
        int pos = g_loading_pos % (bar_max_w * 2);
        if (pos >= bar_max_w) pos = bar_max_w * 2 - pos;  /* bounce back */
        if (pos + bar_w > bar_max_w) bar_w = bar_max_w - pos;
        if (bar_w > 0) {
            surface_fill_rect(surf, bar_x + pos, bar_y, bar_w, 6, THEME_ACCENT);
        }

        /* Loading text with animated dots */
        const char *dots[] = { "Loading", "Loading.", "Loading..", "Loading..." };
        int dot_idx = (g_loading_tick / 8) % 4;
        surface_draw_text(surf, bar_x + bar_max_w + 10, cy + 5,
                          dots[dot_idx], FONT_SMALL, THEME_TEXT_INVERSE);
    } else if (g_status[0]) {
        surface_draw_text(surf, cx + 8, cy + 5, g_status,
                          FONT_SMALL, THEME_TEXT_INVERSE);
    }
}

/*=============================================================================
 * WINDOW CALLBACKS
 *===========================================================================*/

static void on_paint(window_t *win, surface_t *surf, void *userdata) {
    (void)userdata;

    int cx = win->content_x;
    int cy = win->content_y;
    int cw = win->content_w;
    int ch = win->content_h;

    /* Toolbar */
    draw_toolbar(surf, cx, cy, cw);

    /* Content area */
    int content_y = cy + BROWSER_TOOLBAR_H;
    int content_h = ch - BROWSER_TOOLBAR_H - BROWSER_STATUSBAR_H;
    draw_content(surf, cx, content_y, cw, content_h);

    /* Status bar */
    int status_y = cy + ch - BROWSER_STATUSBAR_H;
    draw_statusbar(surf, cx, status_y, cw);
}

static void on_key(window_t *win, int scancode, char ascii, void *userdata) {
    (void)userdata;

    /* Tab: toggle focus between URL bar and content */
    if (scancode == 0x0F) {
        g_url_focused = !g_url_focused;
        window_invalidate(win);
        return;
    }

    if (g_url_focused) {
        /* URL bar input */
        if (ascii >= 32 && ascii < 127 && g_url_len < 254) {
            /* Insert character at cursor */
            for (int i = g_url_len; i > g_url_cursor; i--)
                g_url[i] = g_url[i - 1];
            g_url[g_url_cursor++] = ascii;
            g_url_len++;
            g_url[g_url_len] = '\0';
            window_invalidate(win);
        } else if (scancode == 0x0E && g_url_cursor > 0) {
            /* Backspace */
            g_url_cursor--;
            for (int i = g_url_cursor; i < g_url_len - 1; i++)
                g_url[i] = g_url[i + 1];
            g_url_len--;
            g_url[g_url_len] = '\0';
            window_invalidate(win);
        } else if (scancode == 0x53 && g_url_cursor < g_url_len) {
            /* Delete key */
            for (int i = g_url_cursor; i < g_url_len - 1; i++)
                g_url[i] = g_url[i + 1];
            g_url_len--;
            g_url[g_url_len] = '\0';
            window_invalidate(win);
        } else if (scancode == 0x4B && g_url_cursor > 0) {
            /* Left arrow */
            g_url_cursor--;
            window_invalidate(win);
        } else if (scancode == 0x4D && g_url_cursor < g_url_len) {
            /* Right arrow */
            g_url_cursor++;
            window_invalidate(win);
        } else if (scancode == 0x47) {
            /* Home */
            g_url_cursor = 0;
            window_invalidate(win);
        } else if (scancode == 0x4F) {
            /* End */
            g_url_cursor = g_url_len;
            window_invalidate(win);
        } else if (scancode == 0x1C) {
            /* Enter — schedule navigation (deferred to on_tick) */
            g_nav_pending = 1;
            g_state = 1;
            g_loading_tick = 0;
            g_loading_pos = 0;
            _strcpy(g_status, "Loading...");
            window_invalidate(win);
        }
    } else {
        /* Content area scrolling */
        int max_scroll = _max(0, g_content_height -
                              (g_win ? g_win->content_h - BROWSER_TOOLBAR_H - BROWSER_STATUSBAR_H : 400));

        if (scancode == 0x48) {
            /* Up arrow */
            g_scroll_y -= SCROLL_STEP;
            if (g_scroll_y < 0) g_scroll_y = 0;
            window_invalidate(win);
        } else if (scancode == 0x50) {
            /* Down arrow */
            g_scroll_y += SCROLL_STEP;
            if (g_scroll_y > max_scroll) g_scroll_y = max_scroll;
            window_invalidate(win);
        } else if (scancode == 0x49) {
            /* Page Up */
            g_scroll_y -= SCROLL_PAGE;
            if (g_scroll_y < 0) g_scroll_y = 0;
            window_invalidate(win);
        } else if (scancode == 0x51) {
            /* Page Down */
            g_scroll_y += SCROLL_PAGE;
            if (g_scroll_y > max_scroll) g_scroll_y = max_scroll;
            window_invalidate(win);
        } else if (scancode == 0x47) {
            /* Home — scroll to top */
            g_scroll_y = 0;
            window_invalidate(win);
        } else if (scancode == 0x4F) {
            /* End — scroll to bottom */
            g_scroll_y = max_scroll;
            window_invalidate(win);
        }
    }

    /* F5 = Refresh (schedule navigation) */
    if (scancode == 0x3F) {
        g_nav_pending = 1;
        g_state = 1;
        g_loading_tick = 0;
        g_loading_pos = 0;
        _strcpy(g_status, "Loading...");
        window_invalidate(win);
    }
}

static void on_tick(window_t *win, void *userdata) {
    (void)userdata;

    /* Cursor blink in URL bar (~500ms) */
    g_cursor_tick++;
    if (g_cursor_tick >= 15) {
        g_cursor_tick = 0;
        g_cursor_visible = !g_cursor_visible;
        if (g_url_focused) window_invalidate(win);
    }

    /* Loading animation — update progress bar when loading */
    if (g_state == 1) {
        g_loading_tick++;
        g_loading_pos += 3;
        /* Redraw statusbar with animation every few ticks */
        if ((g_loading_tick % 3) == 0) {
            window_invalidate(win);
        }
    }

    /* Deferred navigation — execute after the window has painted
     * the "Loading..." state at least once, avoiding the
     * "not responding" dialog on the initial click frame. */
    if (g_nav_pending) {
        g_nav_pending = 0;
        navigate();
    }
}

/** Mouse click handler for toolbar buttons and URL bar */
static void on_mouse(window_t *win, int cx, int cy, int event,
                     void *userdata) {
    (void)userdata;

    /* Only handle mouse-up (click) */
    if (event != WIN_MOUSE_UP) return;

    int cw = win->content_w;

    /* Hit-test: Go button */
    int reload_x = cw - RELOAD_BTN_W - 6;
    int go_x = reload_x - GO_BTN_W - RELOAD_BTN_GAP;
    int go_y = GO_BTN_Y;

    if (cx >= go_x && cx < go_x + GO_BTN_W &&
        cy >= go_y && cy < go_y + GO_BTN_H) {
        /* Schedule deferred navigation */
        g_nav_pending = 1;
        g_state = 1;
        g_loading_tick = 0;
        g_loading_pos = 0;
        _strcpy(g_status, "Loading...");
        window_invalidate(win);
        return;
    }

    /* Hit-test: Reload button */
    int reload_y = GO_BTN_Y;
    if (cx >= reload_x && cx < reload_x + RELOAD_BTN_W &&
        cy >= reload_y && cy < reload_y + RELOAD_BTN_H) {
        /* Schedule deferred navigation */
        g_nav_pending = 1;
        g_state = 1;
        g_loading_tick = 0;
        g_loading_pos = 0;
        _strcpy(g_status, "Loading...");
        window_invalidate(win);
        return;
    }

    /* Hit-test: URL bar — click to focus */
    int url_w = cw - URL_BAR_X - GO_BTN_W - RELOAD_BTN_W - RELOAD_BTN_GAP - 16;
    if (cx >= URL_BAR_X && cx < URL_BAR_X + url_w &&
        cy >= URL_BAR_Y && cy < URL_BAR_Y + URL_BAR_H) {
        if (!g_url_focused) {
            g_url_focused = 1;
            window_invalidate(win);
        }
        return;
    }

    /* Click below toolbar → focus content for scrolling */
    if (cy >= BROWSER_TOOLBAR_H && g_url_focused) {
        g_url_focused = 0;
        window_invalidate(win);
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    liblog_init();

    /* Initialize URL */
    _strcpy(g_url, "http://example.com");
    g_url_len = _strlen(g_url);
    g_url_cursor = g_url_len;
    _strcpy(g_status, "Ready");

    /* Check network availability */
    if (!libnet_is_available()) {
        _strcpy(g_status, "Warning: Network not available");
    }

    /* Center window on screen */
    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    int win_x = (scr_w - WIN_W) / 2;
    int win_y = (scr_h - WIN_H) / 2;

    /* Create window */
    window_t *win = window_create("MaahiOS Browser", win_x, win_y, WIN_W, WIN_H);
    if (!win) return;
    g_win = win;

    /* Load window icon */
    {
        static uint8_t icon_buf[4096];
        int sz = libfs_read_file("C:/icons/", "BROWSER.BMP", icon_buf, sizeof(icon_buf));
        if (sz > 0) window_set_icon(win, icon_buf, sz);
    }

    /* Set callbacks */
    window_set_on_paint(win, on_paint, (void*)0);
    window_set_on_key(win, on_key, (void*)0);
    window_set_on_tick(win, on_tick, (void*)0);
    window_set_on_mouse(win, on_mouse, (void*)0);

    /* Enter event loop */
    window_run(win);

    /* Cleanup */
    g_win = (void*)0;
    window_destroy(win);
}
