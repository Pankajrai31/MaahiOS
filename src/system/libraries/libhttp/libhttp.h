/**
 * MaahiOS HTTP Client Library - libhttp.h
 *
 * Description:
 *   Minimal HTTP/1.0 client for MaahiOS.
 *   Uses libnet TCP sockets and DNS resolver for connectivity.
 *
 * Usage:
 *   #include "libhttp.h"
 *
 *   char buf[8192];
 *   int len = libhttp_get("http://example.com/index.html", buf, sizeof(buf));
 *   if (len > 0) {
 *       // buf contains HTTP response body
 *   }
 *
 * Layer 2 (Library). Ring 3.
 * Talks to Network Executive via libnet.
 */

#ifndef LIBHTTP_H
#define LIBHTTP_H

#include <stdint.h>

/*=============================================================================
 * HTTP RESPONSE
 *===========================================================================*/

#define HTTP_MAX_HEADER_SIZE    2048
#define HTTP_MAX_HEADERS        32

typedef struct {
    int      status_code;           /* HTTP status code (200, 404, etc.) */
    uint32_t content_length;        /* Content-Length if present, 0 otherwise */
    uint32_t header_length;         /* Byte length of headers (including \r\n\r\n) */
    uint32_t body_length;           /* Actual body bytes received */
    char     content_type[64];      /* Content-Type header value */
    char     location[256];         /* Location header (for redirects) */
} libhttp_response_t;

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

/**
 * libhttp_get — Perform an HTTP GET request
 *
 * @param url         Full URL (http://host[:port]/path)
 * @param body_buf    Output buffer for response body
 * @param body_max    Size of body_buf
 * @param response    Optional output for response metadata (may be NULL)
 * @return bytes written to body_buf on success, negative on error
 *         -1 = DNS/connect failure
 *         -2 = send failure
 *         -3 = recv failure / timeout
 *         -4 = invalid URL
 *        -10 = site requires HTTPS (not supported)
 */
int libhttp_get(const char *url, char *body_buf, uint32_t body_max,
                libhttp_response_t *response);

/**
 * libhttp_get_ip — Perform HTTP GET to a known IP (skip DNS)
 *
 * @param ip          Target IP (host byte order) — use LIBNET_IP()
 * @param port        Target port (usually 80)
 * @param path        Request path (e.g. "/index.html")
 * @param host_hdr    Host header value (e.g. "example.com"), or NULL
 * @param body_buf    Output buffer for response body
 * @param body_max    Size of body_buf
 * @param response    Optional output for response metadata (may be NULL)
 * @return bytes of body on success, negative on error
 */
int libhttp_get_ip(uint32_t ip, uint16_t port, const char *path,
                   const char *host_hdr,
                   char *body_buf, uint32_t body_max,
                   libhttp_response_t *response);

/**
 * libhttp_set_heartbeat — Register a keep-alive callback invoked
 *                         periodically during HTTP requests.
 *                         GUI apps should call this before libhttp_get()
 *                         to send WM heartbeats and avoid "not responding".
 *                         Also propagates to libnet's heartbeat callback.
 * @param fn   Callback function (NULL to clear)
 * @param ctx  User context passed to fn
 */
void libhttp_set_heartbeat(void (*fn)(void *ctx), void *ctx);

/* Error codes */
#define LIBHTTP_ERR_HTTPS_REQUIRED  -10

#endif /* LIBHTTP_H */
