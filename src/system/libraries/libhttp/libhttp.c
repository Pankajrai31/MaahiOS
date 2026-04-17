/**
 * MaahiOS HTTP Client Library - libhttp.c
 *
 * Layer 2 (Library). Ring 3.
 * Minimal HTTP/1.0 client using libnet TCP sockets and DNS.
 *
 * Supports:
 *   - HTTP/1.0 GET requests
 *   - DNS hostname resolution via libnet_dns_resolve
 *   - Direct IP connections (libhttp_get_ip)
 *   - Content-Length and chunked-less transfer
 *   - Response header parsing (status code, content-type, content-length)
 */

#include "libhttp.h"
#include "../libnet/libnet.h"
#include "../libtls/libtls.h"
#include "../core/syscall_helpers.h"

/* Static TLS connection state — one at a time (single-threaded OS) */
static tls_conn_t g_tls_conn;

/*=============================================================================
 * HEARTBEAT CALLBACK (keep GUI alive during long HTTP requests)
 *===========================================================================*/

static void (*g_heartbeat_fn)(void *ctx) = (void*)0;
static void  *g_heartbeat_ctx            = (void*)0;

static void _http_heartbeat(void) {
    if (g_heartbeat_fn) g_heartbeat_fn(g_heartbeat_ctx);
}

void libhttp_set_heartbeat(void (*fn)(void *ctx), void *ctx) {
    g_heartbeat_fn  = fn;
    g_heartbeat_ctx = ctx;
    /* Propagate to libnet so heartbeats fire inside network polling loops too */
    libnet_set_heartbeat(fn, ctx);
}

/*=============================================================================
 * LOW-LEVEL HELPERS (freestanding — no libc)
 *===========================================================================*/

static void _memset(void *dst, uint8_t val, uint32_t size) {
    uint8_t *p = (uint8_t *)dst;
    for (uint32_t i = 0; i < size; i++) p[i] = val;
}

/*=============================================================================
 * STRING HELPERS (freestanding — no libc)
 *===========================================================================*/

static uint32_t _strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static void _strcpy(char *dst, const char *src) {
    while (*src) *dst++ = *src++;
    *dst = '\0';
}

static int _strncmp(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

/** Case-insensitive compare (ASCII only) */
static int _strncasecmp(const char *a, const char *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        if (ca == '\0') return 0;
    }
    return 0;
}

static int _str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return (*a == *b);
}

/** Simple atoi (unsigned) */
static uint32_t _atoi(const char *s) {
    uint32_t val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    return val;
}

/** Convert uint32_t to decimal string, returns length */
static int _itoa(uint32_t val, char *buf) {
    char tmp[12];
    int len = 0;
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return 1; }
    while (val > 0) {
        tmp[len++] = '0' + (val % 10);
        val /= 10;
    }
    for (int i = 0; i < len; i++) buf[i] = tmp[len - 1 - i];
    buf[len] = '\0';
    return len;
}

/** Append string to buffer, returns new position */
static int _append(char *buf, int pos, int max, const char *s) {
    while (*s && pos < max - 1) buf[pos++] = *s++;
    buf[pos] = '\0';
    return pos;
}

/* Forward declaration — defined later */
static void _str_copy(char *dst, const char *src, int max);

/*=============================================================================
 * URL PARSER
 *===========================================================================*/

typedef struct {
    char     host[128];
    char     path[256];
    uint16_t port;
    uint8_t  is_https;
} parsed_url_t;

/**
 * Parse "http://host[:port][/path]" into components.
 * Also accepts bare "host[:port][/path]" without http:// prefix.
 * Returns 0 on success, -1 on error.
 */
static int _parse_url(const char *url, parsed_url_t *out) {
    _memset(out, 0, sizeof(parsed_url_t));
    out->port = 80;
    out->path[0] = '/';
    out->is_https = 0;

    /* Skip scheme prefix if present */
    const char *p = url;
    if (_strncmp(url, "https://", 8) == 0) {
        p = url + 8;
        out->is_https = 1;
        out->port = 443;
    } else if (_strncmp(url, "http://", 7) == 0) {
        p = url + 7;
    }

    /* Extract host (until : or / or end) */
    int hi = 0;
    while (*p && *p != ':' && *p != '/' && hi < 127) {
        out->host[hi++] = *p++;
    }
    out->host[hi] = '\0';
    if (hi == 0) return -1;

    /* Optional port */
    if (*p == ':') {
        p++;
        out->port = (uint16_t)_atoi(p);
        while (*p >= '0' && *p <= '9') p++;
    }

    /* Path (default "/") */
    if (*p == '/') {
        int pi = 0;
        while (*p && pi < 255) {
            out->path[pi++] = *p++;
        }
        out->path[pi] = '\0';
    }

    return 0;
}

/*=============================================================================
 * HTTP RESPONSE PARSER
 *===========================================================================*/

/**
 * Find "\r\n\r\n" in buffer to locate end of headers.
 * Returns offset of first byte of body, or -1 if not found.
 */
static int _find_header_end(const char *buf, int len) {
    for (int i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' &&
            buf[i+2] == '\r' && buf[i+3] == '\n') {
            return i + 4;
        }
    }
    return -1;
}

/**
 * Parse status line: "HTTP/1.x NNN reason\r\n"
 * Returns status code, or -1 on error.
 */
static int _parse_status_code(const char *buf, int len) {
    /* Find first space after "HTTP/" */
    int i = 0;
    while (i < len && buf[i] != ' ') i++;
    if (i >= len) return -1;
    i++; /* skip space */
    if (i + 3 > len) return -1;
    return (int)_atoi(&buf[i]);
}

/**
 * Search headers for a specific header value (case-insensitive).
 * Copies value to out_val (up to max_len chars).
 * Returns 0 if found, -1 if not found.
 */
static int _find_header(const char *headers, int hdr_len,
                        const char *name, char *out_val, int max_len) {
    int name_len = (int)_strlen(name);
    int i = 0;

    /* Skip status line */
    while (i < hdr_len && !(headers[i] == '\r' && headers[i+1] == '\n')) i++;
    if (i + 2 <= hdr_len) i += 2;

    while (i + name_len < hdr_len) {
        /* Check if current line starts with the header name */
        if (_strncasecmp(&headers[i], name, name_len) == 0 && headers[i + name_len] == ':') {
            /* Found it — skip "Name:" and optional whitespace */
            int vi = i + name_len + 1;
            while (vi < hdr_len && (headers[vi] == ' ' || headers[vi] == '\t')) vi++;

            /* Copy value until \r\n */
            int oi = 0;
            while (vi < hdr_len && headers[vi] != '\r' && oi < max_len - 1) {
                out_val[oi++] = headers[vi++];
            }
            out_val[oi] = '\0';
            return 0;
        }

        /* Skip to next line */
        while (i < hdr_len && !(headers[i] == '\r' && headers[i+1] == '\n')) i++;
        if (i + 2 <= hdr_len) i += 2;
        else break;
    }

    return -1;
}

/*=============================================================================
 * HTTP GET IMPLEMENTATION
 *===========================================================================*/

/**
 * Internal helper: perform HTTPS GET using TLS.
 * Same interface as libhttp_get_ip but wraps connection with TLS.
 * keep_alive: if 1, use HTTP/1.1 + Connection: keep-alive and reuse for
 *             same-host redirects.  If 0, use HTTP/1.0 + Connection: close.
 */

/* Keep-alive state for HTTPS connections */
static char g_ka_host[128] = "";
static int  g_ka_active = 0;

static void _https_close_ka(void) {
    if (g_ka_active) {
        tls_close(&g_tls_conn);
        g_ka_active = 0;
        g_ka_host[0] = '\0';
    }
}

static int _https_get_ip(uint32_t ip, uint16_t port, const char *path,
                         const char *host_hdr,
                         char *body_buf, uint32_t body_max,
                         libhttp_response_t *response, int keep_alive) {
    if (!path || !body_buf || body_max == 0) return -4;
    int ret;

    /* Check if we can reuse existing keep-alive connection */
    int reuse = 0;
    if (keep_alive && g_ka_active && host_hdr && _str_eq(g_ka_host, host_hdr)) {
        reuse = 1;
        syscall3(SYS_KLOG, 3, (int)"HTTPS", (int)"Reusing keep-alive connection");
    }

    if (!reuse) {
        _https_close_ka(); /* close any old keep-alive connection */

        /* Create TCP socket */
        int sock = libnet_socket_create(LIBNET_SOCK_TCP);
        if (sock < 0) return -1;

        /* Connect to server */
        _http_heartbeat();
        ret = libnet_connect(sock, ip, port);
        if (ret != 0) {
            libnet_close(sock);
            return -1;
        }

        /* TLS handshake */
        _http_heartbeat();
        tls_set_heartbeat(&g_tls_conn, g_heartbeat_fn, g_heartbeat_ctx);
        ret = tls_connect(&g_tls_conn, sock, host_hdr);
        if (ret != TLS_OK) {
            libnet_close(sock);
            return -10; /* TLS handshake failure — distinct from TCP -1 */
        }
    }

    /* Build HTTP GET request */
    _http_heartbeat();
    char req_buf[512];
    int pos = 0;
    pos = _append(req_buf, pos, sizeof(req_buf), "GET ");
    pos = _append(req_buf, pos, sizeof(req_buf), path);
    pos = _append(req_buf, pos, sizeof(req_buf),
                  keep_alive ? " HTTP/1.1\r\n" : " HTTP/1.0\r\n");

    if (host_hdr && host_hdr[0]) {
        pos = _append(req_buf, pos, sizeof(req_buf), "Host: ");
        pos = _append(req_buf, pos, sizeof(req_buf), host_hdr);
        pos = _append(req_buf, pos, sizeof(req_buf), "\r\n");
    }

    pos = _append(req_buf, pos, sizeof(req_buf), "User-Agent: MaahiOS/1.0\r\n");
    pos = _append(req_buf, pos, sizeof(req_buf), "Accept: */*\r\n");
    pos = _append(req_buf, pos, sizeof(req_buf),
                  keep_alive ? "Connection: keep-alive\r\n" : "Connection: close\r\n");
    pos = _append(req_buf, pos, sizeof(req_buf), "\r\n");

    /* Send request via TLS */
    syscall3(SYS_KLOG, 3, (int)"HTTPS", (int)"Sending HTTP GET over TLS...");
    ret = tls_send(&g_tls_conn, req_buf, (uint16_t)pos);
    if (ret <= 0) {
        syscall4(SYS_KLOG_HEX, 4, (int)"HTTPS", (int)"tls_send failed ret=", ret);
        g_ka_active = 0; g_ka_host[0] = '\0';
        tls_close(&g_tls_conn);
        return -2;
    }
    syscall4(SYS_KLOG_HEX, 3, (int)"HTTPS", (int)"tls_send ok, bytes=", ret);

    /* Receive response via TLS */
    _http_heartbeat();
    char *recv_buf = body_buf;
    uint32_t recv_max = body_max;
    uint32_t total = 0;
    int header_end = -1;
    int empty_polls = 0;
    uint32_t recv_t0 = (uint32_t)syscall0(115); /* SYS_TIME_GET_TICKS */

    while (total < recv_max && empty_polls < 100) {
        uint16_t chunk_max = 4096;
        if (total + chunk_max > recv_max) chunk_max = (uint16_t)(recv_max - total);

        int got = tls_recv(&g_tls_conn, recv_buf + total, chunk_max);
        if (got > 0) {
            total += (uint32_t)got;
            empty_polls = 0;

            if (header_end < 0) {
                header_end = _find_header_end(recv_buf, (int)total);
            }
            if (header_end > 0) {
                /* For non-keep-alive, redirect early-stop (skip body download) */
                if (!keep_alive) {
                    int sc = _parse_status_code(recv_buf, header_end);
                    if (sc >= 301 && sc <= 308 && sc != 304 && sc != 305) {
                        char loc[4] = {0};
                        if (_find_header(recv_buf, header_end, "Location",
                                         loc, sizeof(loc)) == 0 && loc[0])
                            break; /* Have redirect Location — no need for body */
                    }
                }
                /* Check Content-Length completion */
                char cl_str[16] = {0};
                if (_find_header(recv_buf, header_end, "Content-Length",
                                 cl_str, sizeof(cl_str)) == 0) {
                    uint32_t cl = _atoi(cl_str);
                    uint32_t body_so_far = total - (uint32_t)header_end;
                    if (body_so_far >= cl) break;
                }
            }
        } else if (got == 0) {
            empty_polls++;
            if ((empty_polls % 50) == 0) _http_heartbeat();
            syscall1(SYS_SLEEP, 1);
        } else {
            /* recv error — connection is likely broken */
            if (keep_alive) { g_ka_active = 0; g_ka_host[0] = '\0'; }
            break;
        }
    }

    if (keep_alive) {
        /* Store keep-alive state for potential reuse */
        _str_copy(g_ka_host, host_hdr, sizeof(g_ka_host));
        g_ka_active = 1;
    } else {
        tls_close(&g_tls_conn);
    }

    {
        uint32_t recv_t1 = (uint32_t)syscall0(115);
        syscall4(SYS_KLOG_HEX, 3, (int)"HTTPS", (int)"Recv ticks:", recv_t1 - recv_t0);
    }
    syscall4(SYS_KLOG_HEX, 3, (int)"HTTPS", (int)"Total recv bytes=", total);
    if (total == 0) return -3;

    /* Parse response (same as plain HTTP) */
    if (header_end < 0) header_end = _find_header_end(recv_buf, (int)total);
    if (header_end < 0) {
        syscall3(SYS_KLOG, 4, (int)"HTTPS", (int)"No header end found in response");
        /* Log first 60 bytes to see what we got */
        char dbg[64]; int dlen = (total > 60) ? 60 : (int)total;
        for (int i = 0; i < dlen; i++) dbg[i] = recv_buf[i];
        dbg[dlen] = '\0';
        syscall3(SYS_KLOG, 3, (int)"HTTPS", (int)dbg);
        if (response) { _memset(response, 0, sizeof(libhttp_response_t)); response->body_length = total; }
        return (int)total;
    }
    int status_code = _parse_status_code(recv_buf, header_end);
    syscall4(SYS_KLOG_HEX, 3, (int)"HTTPS", (int)"HTTP status code=", (uint32_t)status_code);
    /* Log first 80 bytes of body for debugging */
    {
        uint32_t bstart = (uint32_t)header_end;
        uint32_t blen = total - bstart;
        char dbg[84]; int dlen = (blen > 80) ? 80 : (int)blen;
        for (int i = 0; i < dlen; i++) dbg[i] = recv_buf[bstart + i];
        dbg[dlen] = '\0';
        syscall3(SYS_KLOG, 3, (int)"HTTPS", (int)dbg);
    }
    if (response) {
        _memset(response, 0, sizeof(libhttp_response_t));
        response->status_code = status_code;
        response->header_length = (uint32_t)header_end;
        char cl_str[16] = {0};
        if (_find_header(recv_buf, header_end, "Content-Length", cl_str, sizeof(cl_str)) == 0)
            response->content_length = _atoi(cl_str);
        _find_header(recv_buf, header_end, "Content-Type", response->content_type, sizeof(response->content_type));
        _find_header(recv_buf, header_end, "Location", response->location, sizeof(response->location));
    }
    uint32_t body_len = total - (uint32_t)header_end;
    if (body_len > 0 && header_end > 0) {
        for (uint32_t i = 0; i < body_len; i++) body_buf[i] = recv_buf[header_end + i];
    }
    if (response) response->body_length = body_len;
    if (body_len < body_max) body_buf[body_len] = '\0';
    return (int)body_len;
}

int libhttp_get_ip(uint32_t ip, uint16_t port, const char *path,
                   const char *host_hdr,
                   char *body_buf, uint32_t body_max,
                   libhttp_response_t *response) {
    if (!path || !body_buf || body_max == 0) return -4;

    /* Create TCP socket */
    int sock = libnet_socket_create(LIBNET_SOCK_TCP);
    if (sock < 0) return -1;

    /* Connect to server */
    _http_heartbeat();
    int ret = libnet_connect(sock, ip, port);
    if (ret != 0) {
        libnet_close(sock);
        return -1;
    }

    /* Build HTTP/1.0 GET request */
    _http_heartbeat();
    char req_buf[512];
    int pos = 0;
    pos = _append(req_buf, pos, sizeof(req_buf), "GET ");
    pos = _append(req_buf, pos, sizeof(req_buf), path);
    pos = _append(req_buf, pos, sizeof(req_buf), " HTTP/1.0\r\n");

    if (host_hdr && host_hdr[0]) {
        pos = _append(req_buf, pos, sizeof(req_buf), "Host: ");
        pos = _append(req_buf, pos, sizeof(req_buf), host_hdr);
        pos = _append(req_buf, pos, sizeof(req_buf), "\r\n");
    }

    pos = _append(req_buf, pos, sizeof(req_buf), "User-Agent: MaahiOS/1.0\r\n");
    pos = _append(req_buf, pos, sizeof(req_buf), "Accept: */*\r\n");
    pos = _append(req_buf, pos, sizeof(req_buf), "Connection: close\r\n");
    pos = _append(req_buf, pos, sizeof(req_buf), "\r\n");

    /* Send request */
    ret = libnet_send(sock, req_buf, (uint16_t)pos);
    if (ret <= 0) {
        libnet_close(sock);
        return -2;
    }

    /* Receive response — accumulate into a temp buffer (headers + body) */
    _http_heartbeat();
    char *recv_buf = body_buf;
    uint32_t recv_max = body_max;
    uint32_t total = 0;
    int header_end = -1;
    int empty_polls = 0;

    while (total < recv_max && empty_polls < 100) {
        uint16_t chunk_max = 4096;
        if (total + chunk_max > recv_max) chunk_max = (uint16_t)(recv_max - total);

        int got = libnet_recv(sock, recv_buf + total, chunk_max);
        if (got > 0) {
            total += (uint32_t)got;
            empty_polls = 0;

            /* Try to find end of headers if not yet found */
            if (header_end < 0) {
                header_end = _find_header_end(recv_buf, (int)total);
            }

            /* If we know content-length, check if we have all body */
            if (header_end > 0) {
                /* For redirect responses, stop downloading body immediately */
                int sc = _parse_status_code(recv_buf, header_end);
                if (sc >= 301 && sc <= 308 && sc != 304 && sc != 305) {
                    char loc[4] = {0};
                    if (_find_header(recv_buf, header_end, "Location", loc, sizeof(loc)) == 0 && loc[0])
                        break; /* Have redirect Location — no need for body */
                }
                char cl_str[16] = {0};
                if (_find_header(recv_buf, header_end, "Content-Length", cl_str, sizeof(cl_str)) == 0) {
                    uint32_t cl = _atoi(cl_str);
                    uint32_t body_so_far = total - (uint32_t)header_end;
                    if (body_so_far >= cl) break; /* Got all body */
                }
            }
        } else if (got == 0) {
            empty_polls++;
            /* Heartbeat every 50 empty polls (~1 sec) to keep WM alive */
            if ((empty_polls % 50) == 0) _http_heartbeat();
            syscall1(SYS_SLEEP, 1);
        } else {
            /* Error or connection closed */
            break;
        }
    }

    libnet_close(sock);

    if (total == 0) return -3;

    /* Parse response headers */
    if (header_end < 0) {
        header_end = _find_header_end(recv_buf, (int)total);
    }
    if (header_end < 0) {
        /* No proper headers — treat everything as body */
        if (response) {
            _memset(response, 0, sizeof(libhttp_response_t));
            response->body_length = total;
        }
        return (int)total;
    }

    int status_code = _parse_status_code(recv_buf, header_end);

    /* Fill response metadata if requested */
    if (response) {
        _memset(response, 0, sizeof(libhttp_response_t));
        response->status_code = status_code;
        response->header_length = (uint32_t)header_end;

        char cl_str[16] = {0};
        if (_find_header(recv_buf, header_end, "Content-Length", cl_str, sizeof(cl_str)) == 0) {
            response->content_length = _atoi(cl_str);
        }

        _find_header(recv_buf, header_end, "Content-Type",
                     response->content_type, sizeof(response->content_type));

        /* Extract Location header for redirect responses */
        _find_header(recv_buf, header_end, "Location",
                     response->location, sizeof(response->location));
    }

    /* Move body data to the beginning of body_buf */
    uint32_t body_len = total - (uint32_t)header_end;
    if (body_len > 0 && header_end > 0) {
        /* Shift body bytes to start of buffer */
        for (uint32_t i = 0; i < body_len; i++) {
            body_buf[i] = recv_buf[header_end + i];
        }
    }

    if (response) {
        response->body_length = body_len;
    }

    /* Null-terminate if space allows (convenience for text responses) */
    if (body_len < body_max) {
        body_buf[body_len] = '\0';
    }

    return (int)body_len;
}

/*=============================================================================
 * REDIRECT CACHE — remember final URLs after redirects
 *===========================================================================*/

#define REDIR_CACHE_SIZE  8

typedef struct {
    char from_host[128]; /* original host */
    char from_path[128]; /* original path */
    char to_url[384];    /* final redirect URL */
    uint8_t is_https;    /* original scheme */
} redir_cache_entry_t;

static redir_cache_entry_t g_redir_cache[REDIR_CACHE_SIZE];
static int g_redir_next = 0;

static int _redir_str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return (*a == *b);
}

static const char *_redir_cache_lookup(const char *host, const char *path, uint8_t is_https) {
    for (int i = 0; i < REDIR_CACHE_SIZE; i++) {
        if (g_redir_cache[i].to_url[0] &&
            g_redir_cache[i].is_https == is_https &&
            _redir_str_eq(g_redir_cache[i].from_host, host) &&
            _redir_str_eq(g_redir_cache[i].from_path, path)) {
            return g_redir_cache[i].to_url;
        }
    }
    return (const char *)0;
}

static void _redir_cache_store(const char *host, const char *path, uint8_t is_https, const char *to_url) {
    int slot = g_redir_next;
    g_redir_next = (g_redir_next + 1) % REDIR_CACHE_SIZE;
    _str_copy(g_redir_cache[slot].from_host, host, 128);
    _str_copy(g_redir_cache[slot].from_path, path, 128);
    _str_copy(g_redir_cache[slot].to_url, to_url, 384);
    g_redir_cache[slot].is_https = is_https;
}

/*=============================================================================
 * REDIRECT HANDLING
 *===========================================================================*/

#define HTTP_MAX_REDIRECTS  5

/** Copy src into dst (up to max-1 chars), null-terminate */
static void _str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

int libhttp_get(const char *url, char *body_buf, uint32_t body_max,
                libhttp_response_t *response) {
    if (!url || !body_buf) return -4;

    /* Working copy of the URL (may change on redirects) */
    char cur_url[384];
    _str_copy(cur_url, url, sizeof(cur_url));

    /* Check redirect cache — skip directly to final URL */
    {
        parsed_url_t p0;
        if (_parse_url(cur_url, &p0) == 0) {
            const char *cached = _redir_cache_lookup(p0.host, p0.path, p0.is_https);
            if (cached) {
                _str_copy(cur_url, cached, sizeof(cur_url));
            }
        }
    }

    /* Keep track of the original host for relative redirects */
    for (int redirect = 0; redirect <= HTTP_MAX_REDIRECTS; redirect++) {
        parsed_url_t parsed;
        if (_parse_url(cur_url, &parsed) != 0) return -4;

        /* Resolve hostname to IP */
        _http_heartbeat();
        uint32_t ip = 0;
        int ret = libnet_dns_resolve(parsed.host, &ip);
        if (ret != 0) return -1;

        /* Fetch */
        _http_heartbeat();
        libhttp_response_t local_resp;
        _memset(&local_resp, 0, sizeof(local_resp));
        libhttp_response_t *rp = response ? response : &local_resp;

        int body_len;
        if (parsed.is_https) {
            body_len = _https_get_ip(ip, parsed.port, parsed.path, parsed.host,
                                    body_buf, body_max, rp, 1);
        } else {
            body_len = libhttp_get_ip(ip, parsed.port, parsed.path, parsed.host,
                                     body_buf, body_max, rp);
        }
        if (body_len < 0) { _https_close_ka(); return body_len; }

        /* Check for redirect (301, 302, 303, 307, 308) */
        if (rp->status_code >= 301 && rp->status_code <= 308 &&
            rp->status_code != 304 && rp->status_code != 305 &&
            rp->location[0]) {

            /* Build new URL from Location header */
            if (_strncmp(rp->location, "http://", 7) == 0 ||
                _strncmp(rp->location, "https://", 8) == 0) {
                /* Absolute URL — follow directly */
                if (_strncmp(rp->location, "https://", 8) == 0) {
                    /* Follow HTTPS redirect natively (we have TLS now) */
                    _str_copy(cur_url, rp->location, sizeof(cur_url));
                } else {
                    _str_copy(cur_url, rp->location, sizeof(cur_url));
                }
            } else if (rp->location[0] == '/') {
                /* Absolute path — prepend scheme + host */
                int pos = 0;
                const char *prefix = parsed.is_https ? "https://" : "http://";
                while (*prefix && pos < 383) cur_url[pos++] = *prefix++;
                const char *h = parsed.host;
                while (*h && pos < 383) cur_url[pos++] = *h++;
                if ((parsed.is_https && parsed.port != 443) ||
                    (!parsed.is_https && parsed.port != 80)) {
                    cur_url[pos++] = ':';
                    char pbuf[8];
                    int plen = _itoa(parsed.port, pbuf);
                    for (int j = 0; j < plen && pos < 383; j++)
                        cur_url[pos++] = pbuf[j];
                }
                const char *loc = rp->location;
                while (*loc && pos < 383) cur_url[pos++] = *loc++;
                cur_url[pos] = '\0';
            } else {
                /* Relative path — not common, skip redirect */
                return body_len;
            }
            /* Cache this redirect for future requests */
            _redir_cache_store(parsed.host, parsed.path, parsed.is_https, cur_url);
            continue; /* Follow the redirect */
        }

        _https_close_ka();
        return body_len; /* Not a redirect — done */
    }

    _https_close_ka();
    return -1; /* Too many redirects */
}
