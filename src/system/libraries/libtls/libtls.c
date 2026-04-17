/**
 * MaahiOS TLS 1.2 Client — libtls.c
 *
 * Implements TLS 1.2 handshake and record layer using:
 *   Cipher suite: TLS_RSA_WITH_AES_128_CBC_SHA256 (0x003C)
 *
 * Handshake flow:
 *   Client → ClientHello (with SNI)
 *   Server → ServerHello, Certificate, ServerHelloDone
 *   Client → ClientKeyExchange, ChangeCipherSpec, Finished
 *   Server → ChangeCipherSpec, Finished
 *
 * Freestanding — no libc dependency.
 * Layer 2 (Library). Ring 3.
 */

#include "libtls.h"
#include "tls_crypto.h"
#include "../libnet/libnet.h"
#include "../core/syscall_helpers.h"

/* Forward declarations */
int tls_parse_certificate_key(const uint8_t *cert_data, uint32_t cert_len,
                              rsa_pubkey_t *key_out);

/* Simple debug logging via SYS_KLOG / SYS_KLOG_HEX (direct syscall) */
static void _tls_log(const char *msg) {
    syscall3(SYS_KLOG, 3 /* LOG_INFO */, (int)"TLS", (int)msg);
}
static void _tls_log_hex(const char *msg, uint32_t val) {
    syscall4(SYS_KLOG_HEX, 3, (int)"TLS", (int)msg, (int)val);
}
#define TLS_LOG(msg) _tls_log(msg)
#define TLS_LOG_HEX(msg, val) _tls_log_hex(msg, val)

/*=============================================================================
 * TLS SESSION CACHE (for session resumption)
 *===========================================================================*/

#define TLS_SESSION_CACHE_SIZE 4

typedef struct {
    char     hostname[128];
    uint16_t port;
    uint8_t  session_id[32];
    uint8_t  session_id_len;
    uint8_t  master_secret[48];
    uint16_t cipher_suite;
    uint8_t  valid;
} tls_session_cache_entry_t;

static tls_session_cache_entry_t g_session_cache[TLS_SESSION_CACHE_SIZE];
static int g_session_cache_next = 0;

static int _tls_str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return (*a == *b);
}

static void _tls_str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* Forward declaration — defined later with other utilities */
static void _memcpy(void *dst, const void *src, uint32_t n);

static tls_session_cache_entry_t *_session_cache_lookup(const char *hostname) {
    if (!hostname) return (void *)0;
    for (int i = 0; i < TLS_SESSION_CACHE_SIZE; i++) {
        if (g_session_cache[i].valid && _tls_str_eq(g_session_cache[i].hostname, hostname))
            return &g_session_cache[i];
    }
    return (void *)0;
}

static void _session_cache_store(const char *hostname, const tls_conn_t *conn) {
    if (!hostname || !conn->session_id_len) return;
    int slot = g_session_cache_next;
    g_session_cache_next = (g_session_cache_next + 1) % TLS_SESSION_CACHE_SIZE;
    _tls_str_copy(g_session_cache[slot].hostname, hostname, 128);
    _memcpy(g_session_cache[slot].session_id, conn->session_id, conn->session_id_len);
    g_session_cache[slot].session_id_len = conn->session_id_len;
    _memcpy(g_session_cache[slot].master_secret, conn->master_secret, 48);
    g_session_cache[slot].cipher_suite = conn->selected_suite;
    g_session_cache[slot].valid = 1;
}

/*=============================================================================
 * UTILITY
 *===========================================================================*/

static void _memcpy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static void _memset(void *dst, uint8_t val, uint32_t n) {
    uint8_t *p = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++) p[i] = val;
}

static uint32_t _strlen(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static void _heartbeat(tls_conn_t *conn) {
    if (conn->heartbeat_fn) conn->heartbeat_fn(conn->heartbeat_ctx);
}

/*=============================================================================
 * BYTE WRITING HELPERS (big-endian)
 *===========================================================================*/

static void put_u8(uint8_t *buf, uint8_t v) { buf[0] = v; }
static void put_u16(uint8_t *buf, uint16_t v) {
    buf[0] = (uint8_t)(v >> 8);
    buf[1] = (uint8_t)(v);
}
static void put_u24(uint8_t *buf, uint32_t v) {
    buf[0] = (uint8_t)(v >> 16);
    buf[1] = (uint8_t)(v >> 8);
    buf[2] = (uint8_t)(v);
}

static uint16_t get_u16(const uint8_t *buf) {
    return ((uint16_t)buf[0] << 8) | buf[1];
}
static uint32_t get_u24(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
}

/*=============================================================================
 * RAW RECORD I/O (send/recv TLS records over TCP)
 *===========================================================================*/

/** Send a raw TLS record. Returns 0 on success.
 *  record_version: use 0x0301 for initial ClientHello, 0x0303 otherwise. */
static int send_record_v(tls_conn_t *conn, uint8_t content_type,
                         uint16_t record_version,
                         const uint8_t *data, uint16_t data_len) {
    /* TLS record header: type(1) + version(2) + length(2) */
    uint8_t header[5];
    header[0] = content_type;
    put_u16(header + 1, record_version);
    put_u16(header + 3, data_len);

    /* Send header */
    int sent = libnet_send(conn->sock, header, 5);
    if (sent <= 0) return TLS_ERR_SOCKET;

    /* Send payload in chunks (libnet limit ~240 bytes per call) */
    uint16_t off = 0;
    while (off < data_len) {
        uint16_t chunk = data_len - off;
        if (chunk > 200) chunk = 200;
        sent = libnet_send(conn->sock, data + off, chunk);
        if (sent <= 0) return TLS_ERR_SOCKET;
        off += (uint16_t)sent;
    }

    return TLS_OK;
}

/** Default send_record using TLS 1.2 record version */
static int send_record(tls_conn_t *conn, uint8_t content_type,
                       const uint8_t *data, uint16_t data_len) {
    return send_record_v(conn, content_type, TLS_VERSION_1_2, data, data_len);
}

/** Read exactly `needed` bytes into conn->recv_buf starting at conn->recv_len.
 *  Blocks with polling. Returns 0 on success. */
static int recv_exact(tls_conn_t *conn, uint32_t needed) {
    int empty_polls = 0;
    while (conn->recv_len < needed) {
        int want = (int)(needed - conn->recv_len);
        if (want > 8192) want = 8192;
        int got = libnet_recv_bulk(conn->sock, conn->recv_buf + conn->recv_len, want);
        if (got > 0) {
            conn->recv_len += (uint32_t)got;
            empty_polls = 0;
        } else if (got == 0) {
            empty_polls++;
            if (empty_polls > 500) return TLS_ERR_SOCKET; /* ~10 sec timeout */
            if ((empty_polls % 50) == 0) _heartbeat(conn);
            syscall1(SYS_SLEEP, 1);
        } else {
            return TLS_ERR_SOCKET;
        }
    }
    return TLS_OK;
}

/** Receive one TLS record. Puts content_type and payload length in *out_type, *out_len.
 *  Payload is at conn->recv_buf[0..out_len-1]. */
static int recv_record(tls_conn_t *conn, uint8_t *out_type, uint16_t *out_len) {
    /* Read 5-byte header */
    conn->recv_len = 0;
    int rc = recv_exact(conn, 5);
    if (rc != TLS_OK) return rc;

    *out_type = conn->recv_buf[0];
    uint16_t rec_len = get_u16(conn->recv_buf + 3);

    if (rec_len > TLS_MAX_RECORD_SIZE) return TLS_ERR_RECORD;

    /* Read payload */
    /* Shift: move payload start after reading header */
    conn->recv_len = 0;
    rc = recv_exact(conn, rec_len);
    if (rc != TLS_OK) return rc;

    *out_len = rec_len;
    return TLS_OK;
}

/*=============================================================================
 * ENCRYPTED RECORD I/O (after ChangeCipherSpec)
 *===========================================================================*/

/** Compute HMAC for a TLS record (either SHA-256 or SHA-1 based on conn->mac_len) */
static void compute_record_mac(tls_conn_t *conn, const uint8_t *mac_key,
                               uint64_t seq_num, uint8_t content_type,
                               const uint8_t *data, uint16_t data_len,
                               uint8_t *mac_out) {
    uint8_t mac_header[13];
    for (int i = 7; i >= 0; i--)
        mac_header[7 - i] = (uint8_t)(seq_num >> (i * 8));
    mac_header[8] = content_type;
    put_u16(mac_header + 9, TLS_VERSION_1_2);
    put_u16(mac_header + 11, data_len);

    if (conn->mac_len == 32) {
        hmac_sha256_ctx_t hctx;
        hmac_sha256_init(&hctx, mac_key, 32);
        hmac_sha256_update(&hctx, mac_header, 13);
        hmac_sha256_update(&hctx, data, data_len);
        hmac_sha256_final(&hctx, mac_out);
    } else {
        hmac_sha1_ctx_t hctx;
        hmac_sha1_init(&hctx, mac_key, 20);
        hmac_sha1_update(&hctx, mac_header, 13);
        hmac_sha1_update(&hctx, data, data_len);
        hmac_sha1_final(&hctx, mac_out);
    }
}

/** Send encrypted record.
 *  Format: explicit_IV(16) || AES-CBC(data + HMAC + padding) */
static int send_encrypted_record(tls_conn_t *conn, uint8_t content_type,
                                 const uint8_t *data, uint16_t data_len) {
    /* Compute HMAC */
    uint8_t mac[32]; /* big enough for SHA-256 */
    compute_record_mac(conn, conn->client_write_mac_key, conn->client_seq,
                       content_type, data, data_len, mac);

    conn->client_seq++;

    /* Build plaintext: data + mac + padding */
    uint32_t content_len = data_len + conn->mac_len;
    uint8_t pad_len = (uint8_t)((AES_BLOCK_SIZE - ((content_len + 1) % AES_BLOCK_SIZE)) % AES_BLOCK_SIZE);
    uint32_t padded_len = content_len + 1 + pad_len;

    /* Buffer: explicit_iv(16) + encrypted(padded_len) */
    uint8_t record_buf[16 + TLS_MAX_PLAINTEXT + 32 + 256];
    uint32_t total = 16 + padded_len;
    if (total > sizeof(record_buf)) return TLS_ERR_BUFFER;

    /* Generate random explicit IV */
    uint8_t iv[16];
    tls_random_bytes(iv, 16);
    _memcpy(record_buf, iv, 16);

    /* Build plaintext to encrypt */
    uint8_t plain_buf[TLS_MAX_PLAINTEXT + 32 + 256];
    _memcpy(plain_buf, data, data_len);
    _memcpy(plain_buf + data_len, mac, conn->mac_len);
    _memset(plain_buf + content_len, pad_len, 1 + pad_len);

    /* Encrypt */
    aes128_cbc_encrypt(&conn->client_write_ctx, iv, plain_buf, padded_len,
                       record_buf + 16);

    return send_record(conn, content_type, record_buf, (uint16_t)total);
}

/** Receive and decrypt one record.
 *  Returns decrypted plaintext in out_buf, sets *out_len and *out_type.
 *  Returns TLS_OK on success. */
static int recv_encrypted_record(tls_conn_t *conn, uint8_t *out_type,
                                 uint8_t *out_buf, uint16_t *out_len) {
    uint8_t rec_type;
    uint16_t rec_len;
    int rc = recv_record(conn, &rec_type, &rec_len);
    if (rc != TLS_OK) return rc;
    *out_type = rec_type;

    if (rec_len < 48) return TLS_ERR_RECORD; /* minimum: IV(16) + one block(16 with mac) */

    /* Extract explicit IV */
    uint8_t iv[16];
    _memcpy(iv, conn->recv_buf, 16);
    uint32_t cipher_len = rec_len - 16;
    if (cipher_len % AES_BLOCK_SIZE != 0) return TLS_ERR_RECORD;

    /* Decrypt */
    uint8_t plain[TLS_MAX_RECORD_SIZE];
    aes128_cbc_decrypt(&conn->server_write_ctx, iv,
                       conn->recv_buf + 16, cipher_len, plain);

    /* Remove padding (last byte = padding length) */
    uint8_t pad_len = plain[cipher_len - 1];
    if (pad_len >= cipher_len) return TLS_ERR_DECRYPT;
    uint32_t unpadded = cipher_len - 1 - pad_len;
    /* Verify padding bytes */
    for (uint32_t i = 0; i < (uint32_t)pad_len; i++) {
        if (plain[cipher_len - 2 - i] != pad_len) return TLS_ERR_DECRYPT;
    }

    /* Split into data + MAC */
    uint32_t ml = conn->mac_len;
    if (unpadded < ml) return TLS_ERR_MAC;
    uint32_t data_len = unpadded - ml;

    /* Verify HMAC */
    uint8_t expected_mac[32];
    compute_record_mac(conn, conn->server_write_mac_key, conn->server_seq,
                       rec_type, plain, (uint16_t)data_len, expected_mac);

    /* Constant-time compare */
    uint8_t diff = 0;
    for (uint32_t i = 0; i < ml; i++) diff |= expected_mac[i] ^ plain[data_len + i];
    if (diff != 0) return TLS_ERR_MAC;

    conn->server_seq++;

    if (data_len > TLS_MAX_PLAINTEXT) data_len = TLS_MAX_PLAINTEXT;
    _memcpy(out_buf, plain, data_len);
    *out_len = (uint16_t)data_len;
    return TLS_OK;
}

/*=============================================================================
 * HANDSHAKE: ClientHello
 *===========================================================================*/

static int send_client_hello(tls_conn_t *conn, const char *hostname) {
    /* Generate client_random */
    tls_random_seed();
    tls_random_bytes(conn->client_random, 32);

    uint32_t host_len = hostname ? _strlen(hostname) : 0;

    /* Check session cache for resumption */
    tls_session_cache_entry_t *cached = _session_cache_lookup(hostname);

    /* Build ClientHello payload */
    uint8_t buf[512];
    uint32_t pos = 0;

    /* Handshake header placeholder — fill after we know the length */
    uint32_t hs_start = pos;
    buf[pos++] = TLS_HS_CLIENT_HELLO;
    pos += 3; /* length placeholder */

    uint32_t ch_start = pos;

    /* Client version */
    put_u16(buf + pos, TLS_VERSION_1_2); pos += 2;

    /* Client random (32 bytes) */
    _memcpy(buf + pos, conn->client_random, 32); pos += 32;

    /* Session ID — send cached ID if available for resumption */
    if (cached && cached->session_id_len > 0) {
        buf[pos++] = cached->session_id_len;
        _memcpy(buf + pos, cached->session_id, cached->session_id_len);
        pos += cached->session_id_len;
    } else {
        buf[pos++] = 0;
    }

    /* Cipher suites: 4 suites + renegotiation SCSV */
    put_u16(buf + pos, 10); pos += 2;  /* 5 × 2 = 10 bytes */
    put_u16(buf + pos, TLS_CIPHER_SUITE); pos += 2;         /* 0x003C AES_128_CBC_SHA256 */
    put_u16(buf + pos, TLS_CIPHER_AES256_SHA256); pos += 2; /* 0x003D AES_256_CBC_SHA256 */
    put_u16(buf + pos, TLS_CIPHER_AES128_SHA); pos += 2;    /* 0x002F AES_128_CBC_SHA */
    put_u16(buf + pos, TLS_CIPHER_AES256_SHA); pos += 2;    /* 0x0035 AES_256_CBC_SHA */
    put_u16(buf + pos, TLS_SCSV_RENEGOTIATION); pos += 2;   /* 0x00FF SCSV */

    /* Compression methods: null only */
    buf[pos++] = 1;
    buf[pos++] = 0;

    /* Extensions */
    uint32_t ext_len_pos = pos;
    pos += 2; /* extensions length placeholder */
    uint32_t ext_start = pos;

    /* SNI extension (type 0x0000) */
    if (host_len > 0) {
        put_u16(buf + pos, 0x0000); pos += 2; /* extension type: SNI */
        uint16_t sni_ext_len = (uint16_t)(host_len + 5);
        put_u16(buf + pos, sni_ext_len); pos += 2;
        put_u16(buf + pos, (uint16_t)(host_len + 3)); pos += 2; /* server name list length */
        buf[pos++] = 0; /* host name type */
        put_u16(buf + pos, (uint16_t)host_len); pos += 2;
        _memcpy(buf + pos, (const uint8_t *)hostname, host_len); pos += host_len;
    }

    /* Signature algorithms extension (type 0x000D) — required for TLS 1.2 */
    put_u16(buf + pos, 0x000D); pos += 2;
    put_u16(buf + pos, 8); pos += 2;      /* extension data length */
    put_u16(buf + pos, 6); pos += 2;      /* sig hash alg list length */
    put_u16(buf + pos, 0x0401); pos += 2; /* SHA-256 + RSA */
    put_u16(buf + pos, 0x0201); pos += 2; /* SHA-1 + RSA */
    put_u16(buf + pos, 0x0501); pos += 2; /* SHA-384 + RSA */

    /* Renegotiation info extension (type 0xFF01) — empty */
    put_u16(buf + pos, 0xFF01); pos += 2;
    put_u16(buf + pos, 1); pos += 2;      /* extension length */
    buf[pos++] = 0;                        /* renegotiated_connection length = 0 */

    /* Fill extension length */
    put_u16(buf + ext_len_pos, (uint16_t)(pos - ext_start));

    /* Fill handshake header length */
    uint32_t ch_len = pos - ch_start;
    put_u24(buf + hs_start + 1, ch_len);

    /* Update handshake hash */
    sha256_update(&conn->hs_hash, buf, pos);

    /* Send as TLS record (use TLS 1.0 record version for compatibility) */
    return send_record_v(conn, TLS_CT_HANDSHAKE, 0x0301, buf, (uint16_t)pos);
}

/*=============================================================================
 * HANDSHAKE: Parse Server messages
 *===========================================================================*/

/**
 * Receive and process server handshake messages until ServerHelloDone.
 * Extracts server_random and server's RSA public key.
 */
static int recv_server_hello_done(tls_conn_t *conn, rsa_pubkey_t *server_key) {
    int got_hello = 0, got_cert = 0, got_done = 0;

    while (!got_done) {
        uint8_t rec_type;
        uint16_t rec_len;
        int rc = recv_record(conn, &rec_type, &rec_len);
        if (rc != TLS_OK) { TLS_LOG_HEX("recv_record fail rc=", (uint32_t)rc); return rc; }

        if (rec_type != TLS_CT_HANDSHAKE) {
            if (rec_type == TLS_CT_ALERT) {
                TLS_LOG_HEX("Got alert, byte0=", conn->recv_buf[0]);
                TLS_LOG_HEX("Alert byte1=", conn->recv_buf[1]);
                return TLS_ERR_HANDSHAKE;
            }
            if (rec_type == TLS_CT_CHANGECIPHERSPEC && got_hello) {
                /* Server accepted session resumption — abbreviated handshake.
                 * CCS already consumed; caller will recv encrypted Finished. */
                TLS_LOG("Server sent CCS after hello — session resumed");
                return TLS_RESUMED;
            }
            TLS_LOG_HEX("Got non-handshake record type=", rec_type);
            continue;
        }

        /* Process handshake messages in this record (may be multiple) */
        uint32_t off = 0;
        while (off + 4 <= rec_len) {
            uint8_t hs_type = conn->recv_buf[off];
            uint32_t hs_len = get_u24(conn->recv_buf + off + 1);
            if (off + 4 + hs_len > rec_len) break;

            /* Update handshake hash */
            sha256_update(&conn->hs_hash, conn->recv_buf + off, 4 + hs_len);

            uint8_t *hs_data = conn->recv_buf + off + 4;

            switch (hs_type) {
            case TLS_HS_SERVER_HELLO: {
                TLS_LOG("Got ServerHello");
                if (hs_len < 38) return TLS_ERR_HANDSHAKE;
                /* version(2) + server_random(32) + session_id_len(1)... */
                _memcpy(conn->server_random, hs_data + 2, 32);
                uint8_t sid_len = hs_data[34];
                /* Save session ID for resumption */
                if (sid_len > 0 && sid_len <= 32) {
                    _memcpy(conn->session_id, hs_data + 35, sid_len);
                    conn->session_id_len = sid_len;
                }
                uint32_t idx = 35 + sid_len;
                if (idx + 2 > hs_len) return TLS_ERR_HANDSHAKE;
                uint16_t chosen_suite = get_u16(hs_data + idx);
                TLS_LOG_HEX("Chosen cipher suite=", chosen_suite);
                /* Check if server chose a suite we support */
                if (chosen_suite == TLS_CIPHER_SUITE || chosen_suite == TLS_CIPHER_AES256_SHA256) {
                    conn->selected_suite = chosen_suite;
                    conn->mac_len = 32; /* SHA-256 */
                } else if (chosen_suite == TLS_CIPHER_AES128_SHA || chosen_suite == TLS_CIPHER_AES256_SHA) {
                    conn->selected_suite = chosen_suite;
                    conn->mac_len = 20; /* SHA-1 */
                } else {
                    TLS_LOG("Server chose unsupported cipher suite!");
                    return TLS_ERR_HANDSHAKE;
                }
                got_hello = 1;
                break;
            }
            case TLS_HS_CERTIFICATE: {
                TLS_LOG("Got Certificate");
                if (hs_len < 3) return TLS_ERR_CERT;
                uint32_t certs_len = get_u24(hs_data);
                if (certs_len + 3 > hs_len) return TLS_ERR_CERT;

                /* Parse first certificate only */
                if (certs_len >= 3) {
                    uint32_t cert_len = get_u24(hs_data + 3);
                    if (cert_len + 6 > hs_len) return TLS_ERR_CERT;
                    rc = tls_parse_certificate_key(hs_data + 6, cert_len, server_key);
                    if (rc != 0) {
                        TLS_LOG_HEX("Cert parse failed, rc=", (uint32_t)rc);
                        return TLS_ERR_CERT;
                    }
                    TLS_LOG("Certificate key extracted OK");
                    got_cert = 1;
                }
                break;
            }
            case TLS_HS_SERVER_HELLO_DONE:
                TLS_LOG("Got ServerHelloDone");
                got_done = 1;
                break;
            default:
                /* Skip unknown handshake messages (e.g., ServerKeyExchange for non-RSA) */
                break;
            }

            off += 4 + hs_len;
        }

        _heartbeat(conn);
    }

    if (!got_hello || !got_cert) return TLS_ERR_HANDSHAKE;
    return TLS_OK;
}

/*=============================================================================
 * HANDSHAKE: ClientKeyExchange + ChangeCipherSpec + Finished
 *===========================================================================*/

static int send_client_key_exchange(tls_conn_t *conn, const rsa_pubkey_t *server_key) {
    /* Generate 48-byte pre-master secret */
    uint8_t pre_master[48];
    put_u16(pre_master, TLS_VERSION_1_2);  /* client_version */
    tls_random_bytes(pre_master + 2, 46);

    /* RSA encrypt pre-master secret */
    uint8_t encrypted[RSA_MAX_MODULUS_BYTES];
    int rc = rsa_pkcs1_encrypt(server_key, pre_master, 48, encrypted);
    if (rc != 0) return TLS_ERR_HANDSHAKE;

    /* Build ClientKeyExchange handshake message */
    uint32_t enc_len = server_key->mod_bytes;
    uint8_t buf[4 + 2 + RSA_MAX_MODULUS_BYTES];
    uint32_t pos = 0;

    buf[pos++] = TLS_HS_CLIENT_KEY_EXCH;
    put_u24(buf + pos, enc_len + 2); pos += 3;
    put_u16(buf + pos, (uint16_t)enc_len); pos += 2;
    _memcpy(buf + pos, encrypted, enc_len); pos += enc_len;

    /* Update handshake hash */
    sha256_update(&conn->hs_hash, buf, pos);

    /* Send */
    rc = send_record(conn, TLS_CT_HANDSHAKE, buf, (uint16_t)pos);
    if (rc != TLS_OK) return rc;

    /* Derive master secret */
    uint8_t seed[64];
    _memcpy(seed, conn->client_random, 32);
    _memcpy(seed + 32, conn->server_random, 32);
    tls_prf_sha256(pre_master, 48, "master secret", seed, 64,
                   conn->master_secret, 48);

    /* Derive key material */
    uint8_t key_seed[64];
    _memcpy(key_seed, conn->server_random, 32);
    _memcpy(key_seed + 32, conn->client_random, 32);

    /* key_block layout depends on cipher suite:
     * MAC key size: 32 (SHA-256) or 20 (SHA-1)
     * AES key size: 16 (AES-128) for all supported suites
     * key_block = client_MAC + server_MAC + client_key + server_key */
    uint32_t mac_key_len = conn->mac_len;  /* 32 or 20 */
    uint32_t enc_key_len = 16;             /* AES-128 */
    uint32_t kb_len = 2 * mac_key_len + 2 * enc_key_len;
    uint8_t key_block[128]; /* enough for any combination */
    tls_prf_sha256(conn->master_secret, 48, "key expansion",
                   key_seed, 64, key_block, kb_len);

    _memcpy(conn->client_write_mac_key, key_block, mac_key_len);
    _memcpy(conn->server_write_mac_key, key_block + mac_key_len, mac_key_len);

    uint8_t client_write_key[16], server_write_key[16];
    _memcpy(client_write_key, key_block + 2 * mac_key_len, enc_key_len);
    _memcpy(server_write_key, key_block + 2 * mac_key_len + enc_key_len, enc_key_len);

    aes128_init(&conn->client_write_ctx, client_write_key);
    aes128_init(&conn->server_write_ctx, server_write_key);

    /* Zero out sensitive material */
    _memset(pre_master, 0, 48);
    _memset(key_block, 0, sizeof(key_block));

    return TLS_OK;
}

static int send_change_cipher_spec(tls_conn_t *conn) {
    uint8_t ccs = 1;
    return send_record(conn, TLS_CT_CHANGECIPHERSPEC, &ccs, 1);
}

static int send_finished(tls_conn_t *conn) {
    /* Hash of all handshake messages so far */
    sha256_ctx_t hash_copy = conn->hs_hash;
    uint8_t hs_digest[SHA256_DIGEST_SIZE];
    sha256_final(&hash_copy, hs_digest);

    /* verify_data = PRF(master_secret, "client finished", Hash(handshake))[0..11] */
    uint8_t verify_data[12];
    tls_prf_sha256(conn->master_secret, 48, "client finished",
                   hs_digest, SHA256_DIGEST_SIZE, verify_data, 12);

    /* Build Finished handshake message */
    uint8_t buf[16];
    buf[0] = TLS_HS_FINISHED;
    put_u24(buf + 1, 12);
    _memcpy(buf + 4, verify_data, 12);

    /* Update handshake hash (Finished message is included for server's Finished check) */
    sha256_update(&conn->hs_hash, buf, 16);

    /* Send as encrypted record */
    return send_encrypted_record(conn, TLS_CT_HANDSHAKE, buf, 16);
}

static int recv_server_finished(tls_conn_t *conn) {
    /* Expect ChangeCipherSpec — but skip NewSessionTicket or other handshake msgs.
     * RFC 5077: NewSessionTicket is sent after client Finished but before server CCS. */
    uint8_t rec_type;
    uint16_t rec_len;

    for (int attempts = 0; attempts < 10; attempts++) {
        int rc = recv_record(conn, &rec_type, &rec_len);
        if (rc != TLS_OK) return rc;

        if (rec_type == TLS_CT_CHANGECIPHERSPEC) break;

        if (rec_type == TLS_CT_HANDSHAKE) {
            /* Skip (e.g., NewSessionTicket type 4) — add to handshake hash */
            TLS_LOG_HEX("Skipping pre-CCS handshake type=", conn->recv_buf[0]);
            sha256_update(&conn->hs_hash, conn->recv_buf, rec_len);
            _heartbeat(conn);
            continue;
        }
        if (rec_type == TLS_CT_ALERT) {
            TLS_LOG_HEX("Got alert during finished, byte0=", conn->recv_buf[0]);
            TLS_LOG_HEX("Alert byte1=", conn->recv_buf[1]);
            return TLS_ERR_HANDSHAKE;
        }
        TLS_LOG_HEX("Unexpected record type=", rec_type);
        return TLS_ERR_UNEXPECTED;
    }

    /* Now expect encrypted Finished */
    uint8_t plaintext[256];
    uint16_t plain_len;
    int rc = recv_encrypted_record(conn, &rec_type, plaintext, &plain_len);
    if (rc != TLS_OK) {
        TLS_LOG_HEX("Encrypted Finished recv failed rc=", (uint32_t)rc);
        return rc;
    }
    if (rec_type != TLS_CT_HANDSHAKE) {
        TLS_LOG_HEX("Expected handshake got type=", rec_type);
        return TLS_ERR_UNEXPECTED;
    }

    /* Parse Finished */
    if (plain_len < 16) {
        TLS_LOG_HEX("Finished too short, len=", plain_len);
        return TLS_ERR_HANDSHAKE;
    }
    if (plaintext[0] != TLS_HS_FINISHED) {
        TLS_LOG_HEX("Expected Finished(20) got type=", plaintext[0]);
        return TLS_ERR_HANDSHAKE;
    }

    /* Compute expected server verify_data */
    sha256_ctx_t hash_copy = conn->hs_hash;
    uint8_t hs_digest[SHA256_DIGEST_SIZE];
    sha256_final(&hash_copy, hs_digest);

    uint8_t expected[12];
    tls_prf_sha256(conn->master_secret, 48, "server finished",
                   hs_digest, SHA256_DIGEST_SIZE, expected, 12);

    /* Compare */
    uint8_t diff = 0;
    for (int i = 0; i < 12; i++) diff |= expected[i] ^ plaintext[4 + i];
    if (diff != 0) return TLS_ERR_HANDSHAKE;

    return TLS_OK;
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

int tls_connect(tls_conn_t *conn, int sock, const char *hostname) {
    _memset(conn, 0, sizeof(tls_conn_t));
    conn->sock = sock;
    sha256_init(&conn->hs_hash);

    int rc;
    uint32_t t0 = (uint32_t)syscall0(115); /* SYS_TIME_GET_TICKS */

    /* Check if we have a cached session for this host */
    tls_session_cache_entry_t *cached_session = _session_cache_lookup(hostname);

    /* 1. Send ClientHello (includes session ID if cached) */
    TLS_LOG("Sending ClientHello...");
    _heartbeat(conn);
    rc = send_client_hello(conn, hostname);
    if (rc != TLS_OK) { TLS_LOG_HEX("ClientHello failed, rc=", (uint32_t)rc); return rc; }

    /* 2. Receive ServerHello + Certificate + ServerHelloDone
     *    OR ServerHello + CCS (abbreviated handshake / session resumption) */
    TLS_LOG("Waiting for ServerHello...");
    _heartbeat(conn);
    rsa_pubkey_t server_key;
    rc = recv_server_hello_done(conn, &server_key);

    uint32_t t1 = (uint32_t)syscall0(115);
    TLS_LOG_HEX("Ticks for hello+cert:", t1 - t0);

    if (rc == TLS_RESUMED && cached_session) {
        /* ===== ABBREVIATED HANDSHAKE (Session Resumption) =====
         * Server accepted our session_id.  Flow:
         *   Server already sent: ServerHello, CCS (consumed), next is encrypted Finished
         *   We must: restore keys → recv server Finished → send CCS + Finished
         */
        TLS_LOG("Session resumed! Restoring keys...");
        _memcpy(conn->master_secret, cached_session->master_secret, 48);

        /* Derive NEW keys from cached master_secret + fresh randoms */
        uint8_t seed[64];
        _memcpy(seed, conn->server_random, 32);
        _memcpy(seed + 32, conn->client_random, 32);
        uint8_t key_block[128];
        tls_prf_sha256(conn->master_secret, 48, "key expansion",
                       seed, 64, key_block, 128);

        uint8_t ml = conn->mac_len;
        _memcpy(conn->client_write_mac_key, key_block, ml);
        _memcpy(conn->server_write_mac_key, key_block + ml, ml);
        aes128_init(&conn->client_write_ctx, key_block + 2 * ml);
        aes128_init(&conn->server_write_ctx, key_block + 2 * ml + 16);

        /* Receive server's encrypted Finished (CCS was already consumed) */
        TLS_LOG("Receiving server Finished (resumed)...");
        _heartbeat(conn);
        {
            uint8_t plaintext[256];
            uint16_t plain_len;
            uint8_t rec_type2;
            rc = recv_encrypted_record(conn, &rec_type2, plaintext, &plain_len);
            if (rc != TLS_OK) { TLS_LOG_HEX("Resumed: server Finished recv failed rc=", (uint32_t)rc); return rc; }
            if (rec_type2 != TLS_CT_HANDSHAKE || plain_len < 16 || plaintext[0] != TLS_HS_FINISHED) {
                TLS_LOG("Resumed: unexpected record instead of Finished");
                return TLS_ERR_HANDSHAKE;
            }
            /* Verify server Finished */
            sha256_ctx_t hash_copy = conn->hs_hash;
            uint8_t hs_digest[SHA256_DIGEST_SIZE];
            sha256_final(&hash_copy, hs_digest);
            uint8_t expected[12];
            tls_prf_sha256(conn->master_secret, 48, "server finished",
                           hs_digest, SHA256_DIGEST_SIZE, expected, 12);
            uint8_t diff = 0;
            for (int i = 0; i < 12; i++) diff |= expected[i] ^ plaintext[4 + i];
            if (diff != 0) { TLS_LOG("Resumed: server Finished verify failed"); return TLS_ERR_HANDSHAKE; }
            /* Add server Finished to handshake hash for client Finished calc */
            sha256_update(&conn->hs_hash, plaintext, 4 + 12);
        }

        /* Send client CCS + Finished */
        TLS_LOG("Sending ChangeCipherSpec (resumed)...");
        rc = send_change_cipher_spec(conn);
        if (rc != TLS_OK) { TLS_LOG_HEX("CCS failed, rc=", (uint32_t)rc); return rc; }
        TLS_LOG("Sending Finished (resumed)...");
        _heartbeat(conn);
        rc = send_finished(conn);
        if (rc != TLS_OK) { TLS_LOG_HEX("Finished send failed, rc=", (uint32_t)rc); return rc; }
    } else if (rc == TLS_OK) {
        /* ===== FULL HANDSHAKE ===== */
        TLS_LOG_HEX("Server cert RSA key size:", server_key.mod_bytes);

        /* 3. Send ClientKeyExchange (derives keys) */
        TLS_LOG("RSA encrypt + key derivation...");
        _heartbeat(conn);
        rc = send_client_key_exchange(conn, &server_key);
        if (rc != TLS_OK) { TLS_LOG_HEX("ClientKeyExchange failed, rc=", (uint32_t)rc); return rc; }

        uint32_t t2 = (uint32_t)syscall0(115);
        TLS_LOG_HEX("Ticks for RSA+keygen:", t2 - t1);

        /* 4. Send ChangeCipherSpec */
        TLS_LOG("Sending ChangeCipherSpec...");
        rc = send_change_cipher_spec(conn);
        if (rc != TLS_OK) { TLS_LOG_HEX("CCS failed, rc=", (uint32_t)rc); return rc; }

        /* 5. Send Finished */
        TLS_LOG("Sending Finished...");
        _heartbeat(conn);
        rc = send_finished(conn);
        if (rc != TLS_OK) { TLS_LOG_HEX("Finished send failed, rc=", (uint32_t)rc); return rc; }

        /* 6. Receive server ChangeCipherSpec + Finished */
        TLS_LOG("Waiting for server Finished...");
        _heartbeat(conn);
        rc = recv_server_finished(conn);
        if (rc != TLS_OK) { TLS_LOG_HEX("Server Finished failed, rc=", (uint32_t)rc); return rc; }
    } else {
        TLS_LOG_HEX("ServerHello/Cert failed, rc=", (uint32_t)rc);
        return rc;
    }

    uint32_t t3 = (uint32_t)syscall0(115);
    TLS_LOG_HEX("Ticks total handshake:", t3 - t0);

    conn->handshake_done = 1;
    TLS_LOG("TLS handshake complete!");

    /* Save session for future resumption */
    _session_cache_store(hostname, conn);

    return TLS_OK;
}

int tls_send(tls_conn_t *conn, const void *data, uint16_t len) {
    if (!conn->handshake_done || conn->closed) return TLS_ERR_CLOSED;
    if (len == 0) return 0;
    if (len > TLS_MAX_PLAINTEXT) len = TLS_MAX_PLAINTEXT;

    int rc = send_encrypted_record(conn, TLS_CT_APPLICATION_DATA,
                                   (const uint8_t *)data, len);
    if (rc != TLS_OK) return rc;
    return (int)len;
}

int tls_recv(tls_conn_t *conn, void *buf, uint16_t max_len) {
    if (!conn->handshake_done) return TLS_ERR_CLOSED;

    /* Check for buffered application data from previous record */
    if (conn->app_len > conn->app_off) {
        uint32_t avail = conn->app_len - conn->app_off;
        uint32_t to_copy = (avail < max_len) ? avail : max_len;
        _memcpy(buf, conn->app_buf + conn->app_off, to_copy);
        conn->app_off += to_copy;
        return (int)to_copy;
    }

    if (conn->closed) return TLS_ERR_CLOSED;

    /* Try to receive a record (with short timeout) */
    /* Peek if data is available */
    uint8_t peek;
    int got = libnet_recv(conn->sock, &peek, 1);
    if (got == 0) return 0; /* no data */
    if (got < 0) return TLS_ERR_SOCKET;

    /* We got one byte — put it back by pre-filling recv_buf */
    conn->recv_buf[0] = peek;
    conn->recv_len = 1;

    /* Read rest of record header (need 5 bytes total) */
    int rc = recv_exact(conn, 5);
    if (rc != TLS_OK) return rc;

    uint8_t rec_type = conn->recv_buf[0];
    uint16_t rec_len = get_u16(conn->recv_buf + 3);

    if (rec_len > TLS_MAX_RECORD_SIZE) return TLS_ERR_RECORD;

    /* Read payload */
    conn->recv_len = 0;
    rc = recv_exact(conn, rec_len);
    if (rc != TLS_OK) return rc;

    /* Handle alerts */
    if (rec_type == TLS_CT_ALERT) {
        conn->closed = 1;
        return TLS_ERR_CLOSED;
    }

    if (rec_type != TLS_CT_APPLICATION_DATA) {
        /* Could be a renegotiation or something — skip */
        return 0;
    }

    /* Decrypt */
    uint8_t plain[TLS_MAX_RECORD_SIZE];

    if (rec_len < 48) return TLS_ERR_RECORD;

    /* Extract IV */
    uint8_t iv[16];
    _memcpy(iv, conn->recv_buf, 16);
    uint32_t cipher_len = rec_len - 16;
    if (cipher_len % AES_BLOCK_SIZE != 0) return TLS_ERR_RECORD;

    aes128_cbc_decrypt(&conn->server_write_ctx, iv,
                       conn->recv_buf + 16, cipher_len, plain);

    /* Remove padding */
    uint8_t pad_len = plain[cipher_len - 1];
    if (pad_len >= cipher_len) return TLS_ERR_DECRYPT;
    uint32_t unpadded = cipher_len - 1 - pad_len;
    for (uint32_t i = 0; i < (uint32_t)pad_len; i++) {
        if (plain[cipher_len - 2 - i] != pad_len) return TLS_ERR_DECRYPT;
    }

    uint32_t ml = conn->mac_len;
    if (unpadded < ml) return TLS_ERR_MAC;
    uint32_t data_len = unpadded - ml;

    /* Verify HMAC */
    uint8_t expected_mac[32];
    compute_record_mac(conn, conn->server_write_mac_key, conn->server_seq,
                       TLS_CT_APPLICATION_DATA, plain, (uint16_t)data_len,
                       expected_mac);

    uint8_t diff = 0;
    for (uint32_t i = 0; i < ml; i++) diff |= expected_mac[i] ^ plain[data_len + i];
    if (diff != 0) return TLS_ERR_MAC;

    conn->server_seq++;

    /* Copy to caller or buffer */
    if (data_len <= max_len) {
        _memcpy(buf, plain, data_len);
        return (int)data_len;
    } else {
        _memcpy(buf, plain, max_len);
        /* Buffer the rest */
        uint32_t remaining = data_len - max_len;
        if (remaining > TLS_MAX_PLAINTEXT) remaining = TLS_MAX_PLAINTEXT;
        _memcpy(conn->app_buf, plain + max_len, remaining);
        conn->app_len = remaining;
        conn->app_off = 0;
        return (int)max_len;
    }
}

void tls_close(tls_conn_t *conn) {
    if (conn->handshake_done && !conn->closed) {
        /* Send close_notify alert */
        uint8_t alert[2] = { 1, 0 }; /* warning, close_notify */
        send_encrypted_record(conn, TLS_CT_ALERT, alert, 2);
    }
    conn->closed = 1;
    libnet_close(conn->sock);
}

void tls_set_heartbeat(tls_conn_t *conn, void (*fn)(void *ctx), void *ctx) {
    conn->heartbeat_fn = fn;
    conn->heartbeat_ctx = ctx;
}
