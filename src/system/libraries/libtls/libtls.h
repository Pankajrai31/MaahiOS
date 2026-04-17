/**
 * MaahiOS TLS Library — libtls.h
 *
 * Minimal TLS 1.2 client using TLS_RSA_WITH_AES_128_CBC_SHA256.
 * Wraps a connected libnet TCP socket with encryption.
 *
 * Freestanding — no libc dependency.
 * Layer 2 (Library). Ring 3.
 */

#ifndef LIBTLS_H
#define LIBTLS_H

#include <stdint.h>
#include "tls_crypto.h"

/*=============================================================================
 * TLS CONSTANTS
 *===========================================================================*/

#define TLS_VERSION_1_2         0x0303
#define TLS_MAX_RECORD_SIZE     8192   /* we limit to save memory; certs can be large */
#define TLS_MAX_PLAINTEXT       2048

/* Content types */
#define TLS_CT_CHANGECIPHERSPEC  20
#define TLS_CT_ALERT             21
#define TLS_CT_HANDSHAKE         22
#define TLS_CT_APPLICATION_DATA  23

/* Handshake types */
#define TLS_HS_CLIENT_HELLO      1
#define TLS_HS_SERVER_HELLO      2
#define TLS_HS_CERTIFICATE       11
#define TLS_HS_SERVER_HELLO_DONE 14
#define TLS_HS_CLIENT_KEY_EXCH   16
#define TLS_HS_FINISHED          20

/* Cipher suites we support */
#define TLS_CIPHER_SUITE         0x003C  /* TLS_RSA_WITH_AES_128_CBC_SHA256 */
#define TLS_CIPHER_AES128_SHA    0x002F  /* TLS_RSA_WITH_AES_128_CBC_SHA */
#define TLS_CIPHER_AES256_SHA    0x0035  /* TLS_RSA_WITH_AES_256_CBC_SHA */
#define TLS_CIPHER_AES256_SHA256 0x003D  /* TLS_RSA_WITH_AES_256_CBC_SHA256 */
#define TLS_SCSV_RENEGOTIATION   0x00FF  /* TLS_EMPTY_RENEGOTIATION_INFO_SCSV */

/* Error codes */
#define TLS_OK                   0
#define TLS_ERR_SOCKET          -1
#define TLS_ERR_HANDSHAKE       -2
#define TLS_ERR_RECORD          -3
#define TLS_ERR_DECRYPT         -4
#define TLS_ERR_MAC             -5
#define TLS_ERR_UNEXPECTED      -6
#define TLS_ERR_CERT            -7
#define TLS_ERR_BUFFER          -8
#define TLS_ERR_CLOSED          -9
#define TLS_RESUMED              1   /* Session resumption: abbreviated handshake */

/*=============================================================================
 * TLS CONNECTION STATE
 *===========================================================================*/

typedef struct {
    /* Underlying TCP socket (from libnet) */
    int sock;

    /* Connection state */
    int handshake_done;
    int closed;
    uint16_t selected_suite;  /* which cipher suite the server chose */
    uint8_t  mac_len;         /* 32 for SHA-256, 20 for SHA-1 */

    /* Random values */
    uint8_t client_random[32];
    uint8_t server_random[32];

    /* Session keys (after handshake) */
    aes128_ctx_t client_write_ctx;  /* encrypt outgoing */
    aes128_ctx_t server_write_ctx;  /* decrypt incoming */
    uint8_t      client_write_mac_key[32];
    uint8_t      server_write_mac_key[32];

    /* Sequence numbers for MAC */
    uint64_t client_seq;
    uint64_t server_seq;

    /* Handshake hash — accumulates all handshake messages for Finished */
    sha256_ctx_t hs_hash;

    /* Receive buffer for record reassembly */
    uint8_t  recv_buf[TLS_MAX_RECORD_SIZE + 256];
    uint32_t recv_len;

    /* Decrypted application data overflow (leftover after tls_recv) */
    uint8_t  app_buf[TLS_MAX_PLAINTEXT];
    uint32_t app_len;
    uint32_t app_off;

    /* Master secret (needed for Finished verify) */
    uint8_t master_secret[48];

    /* Session ID (for resumption) */
    uint8_t  session_id[32];
    uint8_t  session_id_len;

    /* Heartbeat callback */
    void (*heartbeat_fn)(void *ctx);
    void  *heartbeat_ctx;
} tls_conn_t;

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

/**
 * tls_connect — Perform TLS 1.2 handshake over a connected TCP socket.
 *
 * @param conn      TLS connection state (caller allocates, zero-initialized)
 * @param sock      Connected libnet TCP socket handle
 * @param hostname  Server hostname (for SNI extension)
 * @return TLS_OK on success, negative error code on failure
 */
int tls_connect(tls_conn_t *conn, int sock, const char *hostname);

/**
 * tls_send — Send encrypted application data.
 *
 * @param conn  TLS connection
 * @param data  Data to send
 * @param len   Length of data
 * @return bytes sent on success, negative on error
 */
int tls_send(tls_conn_t *conn, const void *data, uint16_t len);

/**
 * tls_recv — Receive and decrypt application data (non-blocking).
 *
 * @param conn     TLS connection
 * @param buf      Output buffer
 * @param max_len  Maximum bytes to receive
 * @return bytes received (>0), 0 if no data available, negative on error
 */
int tls_recv(tls_conn_t *conn, void *buf, uint16_t max_len);

/**
 * tls_close — Send TLS close_notify and close connection.
 *
 * @param conn  TLS connection
 */
void tls_close(tls_conn_t *conn);

/**
 * tls_set_heartbeat — Register a callback for keep-alive during handshake.
 */
void tls_set_heartbeat(tls_conn_t *conn, void (*fn)(void *ctx), void *ctx);

#endif /* LIBTLS_H */
