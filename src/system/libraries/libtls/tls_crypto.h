/**
 * MaahiOS TLS Cryptographic Primitives — tls_crypto.h
 *
 * Provides: SHA-256, HMAC-SHA-256, SHA-1, HMAC-SHA-1, AES-128-CBC,
 *           RSA (public-key only), TLS 1.2 PRF, and a simple PRNG seeded from RDTSC.
 *
 * Freestanding — no libc dependency.
 * Layer 2 (Library). Ring 3.
 */

#ifndef TLS_CRYPTO_H
#define TLS_CRYPTO_H

#include <stdint.h>

/*=============================================================================
 * SHA-256
 *===========================================================================*/

#define SHA256_BLOCK_SIZE   64
#define SHA256_DIGEST_SIZE  32

typedef struct {
    uint32_t state[8];
    uint64_t bitcount;
    uint8_t  buffer[SHA256_BLOCK_SIZE];
    uint32_t buflen;
} sha256_ctx_t;

void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]);

/** One-shot SHA-256 */
void sha256(const uint8_t *data, uint32_t len, uint8_t digest[SHA256_DIGEST_SIZE]);

/*=============================================================================
 * SHA-1 (for TLS_RSA_WITH_AES_128_CBC_SHA)
 *===========================================================================*/

#define SHA1_BLOCK_SIZE   64
#define SHA1_DIGEST_SIZE  20

typedef struct {
    uint32_t state[5];
    uint64_t bitcount;
    uint8_t  buffer[SHA1_BLOCK_SIZE];
    uint32_t buflen;
} sha1_ctx_t;

void sha1_init(sha1_ctx_t *ctx);
void sha1_update(sha1_ctx_t *ctx, const uint8_t *data, uint32_t len);
void sha1_final(sha1_ctx_t *ctx, uint8_t digest[SHA1_DIGEST_SIZE]);

/*=============================================================================
 * HMAC-SHA-256
 *===========================================================================*/

#define HMAC_SHA256_SIZE  32

typedef struct {
    sha256_ctx_t inner;
    sha256_ctx_t outer;
} hmac_sha256_ctx_t;

void hmac_sha256_init(hmac_sha256_ctx_t *ctx, const uint8_t *key, uint32_t key_len);
void hmac_sha256_update(hmac_sha256_ctx_t *ctx, const uint8_t *data, uint32_t len);
void hmac_sha256_final(hmac_sha256_ctx_t *ctx, uint8_t mac[HMAC_SHA256_SIZE]);

/** One-shot HMAC-SHA-256 */
void hmac_sha256(const uint8_t *key, uint32_t key_len,
                 const uint8_t *data, uint32_t data_len,
                 uint8_t mac[HMAC_SHA256_SIZE]);

/*=============================================================================
 * HMAC-SHA-1 (for TLS_RSA_WITH_AES_128_CBC_SHA)
 *===========================================================================*/

#define HMAC_SHA1_SIZE  20

typedef struct {
    sha1_ctx_t inner;
    sha1_ctx_t outer;
} hmac_sha1_ctx_t;

void hmac_sha1_init(hmac_sha1_ctx_t *ctx, const uint8_t *key, uint32_t key_len);
void hmac_sha1_update(hmac_sha1_ctx_t *ctx, const uint8_t *data, uint32_t len);
void hmac_sha1_final(hmac_sha1_ctx_t *ctx, uint8_t mac[HMAC_SHA1_SIZE]);

/*=============================================================================
 * AES-128
 *===========================================================================*/

#define AES_BLOCK_SIZE  16
#define AES_KEY_SIZE    16
#define AES_ROUNDS      10

typedef struct {
    uint32_t round_key[44]; /* 4 * (AES_ROUNDS + 1) */
} aes128_ctx_t;

void aes128_init(aes128_ctx_t *ctx, const uint8_t key[AES_KEY_SIZE]);
void aes128_encrypt_block(const aes128_ctx_t *ctx,
                          const uint8_t in[AES_BLOCK_SIZE],
                          uint8_t out[AES_BLOCK_SIZE]);
void aes128_decrypt_block(const aes128_ctx_t *ctx,
                          const uint8_t in[AES_BLOCK_SIZE],
                          uint8_t out[AES_BLOCK_SIZE]);

/** CBC encrypt: plaintext must be padded to AES_BLOCK_SIZE multiple.
 *  iv is updated in-place to the last ciphertext block. */
void aes128_cbc_encrypt(const aes128_ctx_t *ctx,
                        uint8_t *iv,
                        const uint8_t *plain, uint32_t len,
                        uint8_t *cipher);

/** CBC decrypt: ciphertext length must be multiple of AES_BLOCK_SIZE.
 *  iv is consumed (overwritten). */
void aes128_cbc_decrypt(const aes128_ctx_t *ctx,
                        const uint8_t *iv,
                        const uint8_t *cipher, uint32_t len,
                        uint8_t *plain);

/*=============================================================================
 * BIG INTEGER (for RSA)
 *
 * 2048-bit unsigned integers stored as 64 × uint32_t (little-endian limbs).
 *===========================================================================*/

#define BIGNUM_LIMBS  66   /* 66 * 32 = 2112 bits — extra limbs for intermediate overflow in modular arithmetic */

typedef struct {
    uint32_t d[BIGNUM_LIMBS];
} bignum_t;

void bn_zero(bignum_t *a);
void bn_from_bytes(bignum_t *a, const uint8_t *buf, uint32_t len);
int  bn_cmp(const bignum_t *a, const bignum_t *b);
void bn_mod_exp(bignum_t *result, const bignum_t *base,
                const bignum_t *exp, const bignum_t *mod);

/*=============================================================================
 * RSA (public-key encryption only — for TLS handshake)
 *===========================================================================*/

#define RSA_MAX_MODULUS_BYTES  256  /* 2048-bit */

typedef struct {
    bignum_t n;         /* modulus */
    bignum_t e;         /* public exponent (typically 65537) */
    uint32_t mod_bytes; /* byte length of modulus */
} rsa_pubkey_t;

/**
 * RSA PKCS#1 v1.5 Type 2 encryption.
 * Encrypts msg (msg_len bytes) with public key, writes to out.
 * out must be rsa->mod_bytes in size.
 * Returns 0 on success, -1 on error.
 */
int rsa_pkcs1_encrypt(const rsa_pubkey_t *rsa,
                      const uint8_t *msg, uint32_t msg_len,
                      uint8_t *out);

/*=============================================================================
 * TLS 1.2 PRF (based on HMAC-SHA-256)
 *===========================================================================*/

/**
 * TLS 1.2 PRF: P_SHA256
 * output = PRF(secret, label, seed) truncated to out_len bytes.
 */
void tls_prf_sha256(const uint8_t *secret, uint32_t secret_len,
                    const char *label,
                    const uint8_t *seed, uint32_t seed_len,
                    uint8_t *output, uint32_t out_len);

/*=============================================================================
 * PRNG (seeded from RDTSC)
 *===========================================================================*/

void tls_random_seed(void);
void tls_random_bytes(uint8_t *buf, uint32_t len);

#endif /* TLS_CRYPTO_H */
