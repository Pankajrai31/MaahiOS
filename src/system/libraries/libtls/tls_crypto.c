/**
 * MaahiOS TLS Cryptographic Primitives — tls_crypto.c
 *
 * Implements SHA-256, HMAC-SHA-256, AES-128-CBC, big-integer arithmetic,
 * RSA PKCS#1 v1.5 public-key encryption, TLS 1.2 PRF, and a simple PRNG.
 *
 * Freestanding — no libc dependency.
 * Layer 2 (Library). Ring 3.
 */

#include "tls_crypto.h"

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

/*=============================================================================
 * SHA-256
 *
 * FIPS 180-4 compliant.
 *===========================================================================*/

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROR32(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x,y,z)   (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)       (ROR32(x,2) ^ ROR32(x,13) ^ ROR32(x,22))
#define EP1(x)       (ROR32(x,6) ^ ROR32(x,11) ^ ROR32(x,25))
#define SIG0(x)      (ROR32(x,7) ^ ROR32(x,18) ^ ((x) >> 3))
#define SIG1(x)      (ROR32(x,17) ^ ROR32(x,19) ^ ((x) >> 10))

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h, t1, t2;

    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i*4] << 24) |
               ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) |
               ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 64; i++) {
        W[i] = SIG1(W[i-2]) + W[i-7] + SIG0(W[i-15]) + W[i-16];
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + K256[i] + W[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

void sha256_init(sha256_ctx_t *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->bitcount = 0;
    ctx->buflen = 0;
}

void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == 64) {
            sha256_transform(ctx, ctx->buffer);
            ctx->bitcount += 512;
            ctx->buflen = 0;
        }
    }
}

void sha256_final(sha256_ctx_t *ctx, uint8_t digest[SHA256_DIGEST_SIZE]) {
    ctx->bitcount += (uint64_t)ctx->buflen * 8;

    ctx->buffer[ctx->buflen++] = 0x80;
    if (ctx->buflen > 56) {
        while (ctx->buflen < 64) ctx->buffer[ctx->buflen++] = 0;
        sha256_transform(ctx, ctx->buffer);
        ctx->buflen = 0;
    }
    while (ctx->buflen < 56) ctx->buffer[ctx->buflen++] = 0;

    /* Append bit length (big-endian) */
    for (int i = 7; i >= 0; i--)
        ctx->buffer[ctx->buflen++] = (uint8_t)(ctx->bitcount >> (i * 8));

    sha256_transform(ctx, ctx->buffer);

    for (int i = 0; i < 8; i++) {
        digest[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

void sha256(const uint8_t *data, uint32_t len, uint8_t digest[SHA256_DIGEST_SIZE]) {
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, digest);
}

/*=============================================================================
 * HMAC-SHA-256 (RFC 2104)
 *===========================================================================*/

void hmac_sha256_init(hmac_sha256_ctx_t *ctx, const uint8_t *key, uint32_t key_len) {
    uint8_t k_pad[SHA256_BLOCK_SIZE];
    uint8_t k_hash[SHA256_DIGEST_SIZE];

    /* If key > block size, hash it first */
    if (key_len > SHA256_BLOCK_SIZE) {
        sha256(key, key_len, k_hash);
        key = k_hash;
        key_len = SHA256_DIGEST_SIZE;
    }

    /* Pad key to block size */
    _memset(k_pad, 0, SHA256_BLOCK_SIZE);
    _memcpy(k_pad, key, key_len);

    /* Inner hash: H(K ^ ipad || ...) */
    uint8_t ipad[SHA256_BLOCK_SIZE];
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) ipad[i] = k_pad[i] ^ 0x36;
    sha256_init(&ctx->inner);
    sha256_update(&ctx->inner, ipad, SHA256_BLOCK_SIZE);

    /* Outer hash state: prepared with K ^ opad */
    uint8_t opad[SHA256_BLOCK_SIZE];
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) opad[i] = k_pad[i] ^ 0x5c;
    sha256_init(&ctx->outer);
    sha256_update(&ctx->outer, opad, SHA256_BLOCK_SIZE);
}

void hmac_sha256_update(hmac_sha256_ctx_t *ctx, const uint8_t *data, uint32_t len) {
    sha256_update(&ctx->inner, data, len);
}

void hmac_sha256_final(hmac_sha256_ctx_t *ctx, uint8_t mac[HMAC_SHA256_SIZE]) {
    uint8_t inner_hash[SHA256_DIGEST_SIZE];
    sha256_final(&ctx->inner, inner_hash);
    sha256_update(&ctx->outer, inner_hash, SHA256_DIGEST_SIZE);
    sha256_final(&ctx->outer, mac);
}

void hmac_sha256(const uint8_t *key, uint32_t key_len,
                 const uint8_t *data, uint32_t data_len,
                 uint8_t mac[HMAC_SHA256_SIZE]) {
    hmac_sha256_ctx_t ctx;
    hmac_sha256_init(&ctx, key, key_len);
    hmac_sha256_update(&ctx, data, data_len);
    hmac_sha256_final(&ctx, mac);
}

/*=============================================================================
 * SHA-1 (FIPS 180-4 — for TLS_RSA_WITH_AES_128_CBC_SHA)
 *===========================================================================*/

#define SHA1_ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_transform(sha1_ctx_t *ctx, const uint8_t block[64]) {
    uint32_t W[80];
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 80; i++) {
        W[i] = SHA1_ROL(W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16], 1);
    }

    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2];
    uint32_t d = ctx->state[3], e = ctx->state[4];

    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        uint32_t tmp = SHA1_ROL(a, 5) + f + e + k + W[i];
        e = d; d = c; c = SHA1_ROL(b, 30); b = a; a = tmp;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c;
    ctx->state[3] += d; ctx->state[4] += e;
}

void sha1_init(sha1_ctx_t *ctx) {
    ctx->state[0] = 0x67452301; ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE; ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->bitcount = 0;
    ctx->buflen = 0;
}

void sha1_update(sha1_ctx_t *ctx, const uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        ctx->buffer[ctx->buflen++] = data[i];
        if (ctx->buflen == 64) {
            sha1_transform(ctx, ctx->buffer);
            ctx->bitcount += 512;
            ctx->buflen = 0;
        }
    }
}

void sha1_final(sha1_ctx_t *ctx, uint8_t digest[SHA1_DIGEST_SIZE]) {
    ctx->bitcount += (uint64_t)ctx->buflen * 8;

    ctx->buffer[ctx->buflen++] = 0x80;
    if (ctx->buflen > 56) {
        while (ctx->buflen < 64) ctx->buffer[ctx->buflen++] = 0;
        sha1_transform(ctx, ctx->buffer);
        ctx->buflen = 0;
    }
    while (ctx->buflen < 56) ctx->buffer[ctx->buflen++] = 0;

    for (int i = 7; i >= 0; i--)
        ctx->buffer[ctx->buflen++] = (uint8_t)(ctx->bitcount >> (i * 8));

    sha1_transform(ctx, ctx->buffer);

    for (int i = 0; i < 5; i++) {
        digest[i*4]   = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

/*=============================================================================
 * HMAC-SHA-1 (RFC 2104, for TLS_RSA_WITH_AES_128_CBC_SHA)
 *===========================================================================*/

void hmac_sha1_init(hmac_sha1_ctx_t *ctx, const uint8_t *key, uint32_t key_len) {
    uint8_t k_pad[SHA1_BLOCK_SIZE];
    uint8_t k_hash[SHA1_DIGEST_SIZE];

    if (key_len > SHA1_BLOCK_SIZE) {
        sha1_ctx_t h;
        sha1_init(&h);
        sha1_update(&h, key, key_len);
        sha1_final(&h, k_hash);
        key = k_hash;
        key_len = SHA1_DIGEST_SIZE;
    }

    _memset(k_pad, 0, SHA1_BLOCK_SIZE);
    _memcpy(k_pad, key, key_len);

    uint8_t ipad[SHA1_BLOCK_SIZE];
    for (int i = 0; i < SHA1_BLOCK_SIZE; i++) ipad[i] = k_pad[i] ^ 0x36;
    sha1_init(&ctx->inner);
    sha1_update(&ctx->inner, ipad, SHA1_BLOCK_SIZE);

    uint8_t opad[SHA1_BLOCK_SIZE];
    for (int i = 0; i < SHA1_BLOCK_SIZE; i++) opad[i] = k_pad[i] ^ 0x5c;
    sha1_init(&ctx->outer);
    sha1_update(&ctx->outer, opad, SHA1_BLOCK_SIZE);
}

void hmac_sha1_update(hmac_sha1_ctx_t *ctx, const uint8_t *data, uint32_t len) {
    sha1_update(&ctx->inner, data, len);
}

void hmac_sha1_final(hmac_sha1_ctx_t *ctx, uint8_t mac[HMAC_SHA1_SIZE]) {
    uint8_t inner_hash[SHA1_DIGEST_SIZE];
    sha1_final(&ctx->inner, inner_hash);
    sha1_update(&ctx->outer, inner_hash, SHA1_DIGEST_SIZE);
    sha1_final(&ctx->outer, mac);
}

/*=============================================================================
 * AES-128 (FIPS 197)
 *===========================================================================*/

static const uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t inv_sbox[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static const uint8_t rcon[11] = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

/* Galois field multiply for MixColumns */
static uint8_t gf_mul2(uint8_t x) {
    return (uint8_t)((x << 1) ^ (((x >> 7) & 1) * 0x1b));
}

static uint8_t gf_mul3(uint8_t x) { return gf_mul2(x) ^ x; }

/* Inverse GF multiply helpers for InvMixColumns */
static uint8_t gf_mul(uint8_t a, uint8_t b) {
    uint8_t r = 0;
    for (int i = 0; i < 8; i++) {
        if (b & 1) r ^= a;
        uint8_t hi = a & 0x80;
        a <<= 1;
        if (hi) a ^= 0x1b;
        b >>= 1;
    }
    return r;
}

void aes128_init(aes128_ctx_t *ctx, const uint8_t key[AES_KEY_SIZE]) {
    /* Copy key into first 4 words */
    for (int i = 0; i < 4; i++) {
        ctx->round_key[i] = ((uint32_t)key[4*i] << 24) |
                            ((uint32_t)key[4*i+1] << 16) |
                            ((uint32_t)key[4*i+2] << 8) |
                            ((uint32_t)key[4*i+3]);
    }
    /* Key expansion */
    for (int i = 4; i < 44; i++) {
        uint32_t temp = ctx->round_key[i-1];
        if (i % 4 == 0) {
            /* RotWord + SubWord + Rcon */
            temp = ((uint32_t)sbox[(temp >> 16) & 0xff] << 24) |
                   ((uint32_t)sbox[(temp >> 8)  & 0xff] << 16) |
                   ((uint32_t)sbox[temp & 0xff] << 8) |
                   ((uint32_t)sbox[(temp >> 24) & 0xff]);
            temp ^= ((uint32_t)rcon[i/4] << 24);
        }
        ctx->round_key[i] = ctx->round_key[i-4] ^ temp;
    }
}

static void add_round_key(uint8_t state[16], const uint32_t *rk) {
    for (int i = 0; i < 4; i++) {
        state[4*i]   ^= (uint8_t)(rk[i] >> 24);
        state[4*i+1] ^= (uint8_t)(rk[i] >> 16);
        state[4*i+2] ^= (uint8_t)(rk[i] >> 8);
        state[4*i+3] ^= (uint8_t)(rk[i]);
    }
}

static void sub_bytes(uint8_t state[16]) {
    for (int i = 0; i < 16; i++) state[i] = sbox[state[i]];
}

static void shift_rows(uint8_t s[16]) {
    uint8_t t;
    /* Row 1: shift left 1 */
    t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
    /* Row 2: shift left 2 */
    t = s[2]; s[2] = s[10]; s[10] = t;
    t = s[6]; s[6] = s[14]; s[14] = t;
    /* Row 3: shift left 3 */
    t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;
}

static void mix_columns(uint8_t s[16]) {
    for (int c = 0; c < 4; c++) {
        int i = c * 4;
        uint8_t a0 = s[i], a1 = s[i+1], a2 = s[i+2], a3 = s[i+3];
        s[i]   = gf_mul2(a0) ^ gf_mul3(a1) ^ a2 ^ a3;
        s[i+1] = a0 ^ gf_mul2(a1) ^ gf_mul3(a2) ^ a3;
        s[i+2] = a0 ^ a1 ^ gf_mul2(a2) ^ gf_mul3(a3);
        s[i+3] = gf_mul3(a0) ^ a1 ^ a2 ^ gf_mul2(a3);
    }
}

void aes128_encrypt_block(const aes128_ctx_t *ctx,
                          const uint8_t in[AES_BLOCK_SIZE],
                          uint8_t out[AES_BLOCK_SIZE]) {
    uint8_t state[16];
    _memcpy(state, in, 16);

    add_round_key(state, &ctx->round_key[0]);

    for (int round = 1; round < AES_ROUNDS; round++) {
        sub_bytes(state);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, &ctx->round_key[round * 4]);
    }

    sub_bytes(state);
    shift_rows(state);
    add_round_key(state, &ctx->round_key[AES_ROUNDS * 4]);

    _memcpy(out, state, 16);
}

/* --- Decrypt --- */

static void inv_sub_bytes(uint8_t state[16]) {
    for (int i = 0; i < 16; i++) state[i] = inv_sbox[state[i]];
}

static void inv_shift_rows(uint8_t s[16]) {
    uint8_t t;
    /* Row 1: shift right 1 */
    t = s[13]; s[13] = s[9]; s[9] = s[5]; s[5] = s[1]; s[1] = t;
    /* Row 2: shift right 2 */
    t = s[2]; s[2] = s[10]; s[10] = t;
    t = s[6]; s[6] = s[14]; s[14] = t;
    /* Row 3: shift right 3 */
    t = s[3]; s[3] = s[7]; s[7] = s[11]; s[11] = s[15]; s[15] = t;
}

/* Precomputed GF(2^8) multiply-by-constant tables for InvMixColumns.
 * Avoids the hot 8-iteration loop in gf_mul(). */
static const uint8_t gf_mul_9[256] = {
    0x00,0x09,0x12,0x1b,0x24,0x2d,0x36,0x3f,0x48,0x41,0x5a,0x53,0x6c,0x65,0x7e,0x77,
    0x90,0x99,0x82,0x8b,0xb4,0xbd,0xa6,0xaf,0xd8,0xd1,0xca,0xc3,0xfc,0xf5,0xee,0xe7,
    0x3b,0x32,0x29,0x20,0x1f,0x16,0x0d,0x04,0x73,0x7a,0x61,0x68,0x57,0x5e,0x45,0x4c,
    0xab,0xa2,0xb9,0xb0,0x8f,0x86,0x9d,0x94,0xe3,0xea,0xf1,0xf8,0xc7,0xce,0xd5,0xdc,
    0x76,0x7f,0x64,0x6d,0x52,0x5b,0x40,0x49,0x3e,0x37,0x2c,0x25,0x1a,0x13,0x08,0x01,
    0xe6,0xef,0xf4,0xfd,0xc2,0xcb,0xd0,0xd9,0xae,0xa7,0xbc,0xb5,0x8a,0x83,0x98,0x91,
    0x4d,0x44,0x5f,0x56,0x69,0x60,0x7b,0x72,0x05,0x0c,0x17,0x1e,0x21,0x28,0x33,0x3a,
    0xdd,0xd4,0xcf,0xc6,0xf9,0xf0,0xeb,0xe2,0x95,0x9c,0x87,0x8e,0xb1,0xb8,0xa3,0xaa,
    0xec,0xe5,0xfe,0xf7,0xc8,0xc1,0xda,0xd3,0xa4,0xad,0xb6,0xbf,0x80,0x89,0x92,0x9b,
    0x7c,0x75,0x6e,0x67,0x58,0x51,0x4a,0x43,0x34,0x3d,0x26,0x2f,0x10,0x19,0x02,0x0b,
    0xd7,0xde,0xc5,0xcc,0xf3,0xfa,0xe1,0xe8,0x9f,0x96,0x8d,0x84,0xbb,0xb2,0xa9,0xa0,
    0x47,0x4e,0x55,0x5c,0x63,0x6a,0x71,0x78,0x0f,0x06,0x1d,0x14,0x2b,0x22,0x39,0x30,
    0x9a,0x93,0x88,0x81,0xbe,0xb7,0xac,0xa5,0xd2,0xdb,0xc0,0xc9,0xf6,0xff,0xe4,0xed,
    0x0a,0x03,0x18,0x11,0x2e,0x27,0x3c,0x35,0x42,0x4b,0x50,0x59,0x66,0x6f,0x74,0x7d,
    0xa1,0xa8,0xb3,0xba,0x85,0x8c,0x97,0x9e,0xe9,0xe0,0xfb,0xf2,0xcd,0xc4,0xdf,0xd6,
    0x31,0x38,0x23,0x2a,0x15,0x1c,0x07,0x0e,0x79,0x70,0x6b,0x62,0x5d,0x54,0x4f,0x46
};
static const uint8_t gf_mul_11[256] = {
    0x00,0x0b,0x16,0x1d,0x2c,0x27,0x3a,0x31,0x58,0x53,0x4e,0x45,0x74,0x7f,0x62,0x69,
    0xb0,0xbb,0xa6,0xad,0x9c,0x97,0x8a,0x81,0xe8,0xe3,0xfe,0xf5,0xc4,0xcf,0xd2,0xd9,
    0x7b,0x70,0x6d,0x66,0x57,0x5c,0x41,0x4a,0x23,0x28,0x35,0x3e,0x0f,0x04,0x19,0x12,
    0xcb,0xc0,0xdd,0xd6,0xe7,0xec,0xf1,0xfa,0x93,0x98,0x85,0x8e,0xbf,0xb4,0xa9,0xa2,
    0xf6,0xfd,0xe0,0xeb,0xda,0xd1,0xcc,0xc7,0xae,0xa5,0xb8,0xb3,0x82,0x89,0x94,0x9f,
    0x46,0x4d,0x50,0x5b,0x6a,0x61,0x7c,0x77,0x1e,0x15,0x08,0x03,0x32,0x39,0x24,0x2f,
    0x8d,0x86,0x9b,0x90,0xa1,0xaa,0xb7,0xbc,0xd5,0xde,0xc3,0xc8,0xf9,0xf2,0xef,0xe4,
    0x3d,0x36,0x2b,0x20,0x11,0x1a,0x07,0x0c,0x65,0x6e,0x73,0x78,0x49,0x42,0x5f,0x54,
    0xf7,0xfc,0xe1,0xea,0xdb,0xd0,0xcd,0xc6,0xaf,0xa4,0xb9,0xb2,0x83,0x88,0x95,0x9e,
    0x47,0x4c,0x51,0x5a,0x6b,0x60,0x7d,0x76,0x1f,0x14,0x09,0x02,0x33,0x38,0x25,0x2e,
    0x8c,0x87,0x9a,0x91,0xa0,0xab,0xb6,0xbd,0xd4,0xdf,0xc2,0xc9,0xf8,0xf3,0xee,0xe5,
    0x3c,0x37,0x2a,0x21,0x10,0x1b,0x06,0x0d,0x64,0x6f,0x72,0x79,0x48,0x43,0x5e,0x55,
    0x01,0x0a,0x17,0x1c,0x2d,0x26,0x3b,0x30,0x59,0x52,0x4f,0x44,0x75,0x7e,0x63,0x68,
    0xb1,0xba,0xa7,0xac,0x9d,0x96,0x8b,0x80,0xe9,0xe2,0xff,0xf4,0xc5,0xce,0xd3,0xd8,
    0x7a,0x71,0x6c,0x67,0x56,0x5d,0x40,0x4b,0x22,0x29,0x34,0x3f,0x0e,0x05,0x18,0x13,
    0xca,0xc1,0xdc,0xd7,0xe6,0xed,0xf0,0xfb,0x92,0x99,0x84,0x8f,0xbe,0xb5,0xa8,0xa3
};
static const uint8_t gf_mul_13[256] = {
    0x00,0x0d,0x1a,0x17,0x34,0x39,0x2e,0x23,0x68,0x65,0x72,0x7f,0x5c,0x51,0x46,0x4b,
    0xd0,0xdd,0xca,0xc7,0xe4,0xe9,0xfe,0xf3,0xb8,0xb5,0xa2,0xaf,0x8c,0x81,0x96,0x9b,
    0xbb,0xb6,0xa1,0xac,0x8f,0x82,0x95,0x98,0xd3,0xde,0xc9,0xc4,0xe7,0xea,0xfd,0xf0,
    0x6b,0x66,0x71,0x7c,0x5f,0x52,0x45,0x48,0x03,0x0e,0x19,0x14,0x37,0x3a,0x2d,0x20,
    0x6d,0x60,0x77,0x7a,0x59,0x54,0x43,0x4e,0x05,0x08,0x1f,0x12,0x31,0x3c,0x2b,0x26,
    0xbd,0xb0,0xa7,0xaa,0x89,0x84,0x93,0x9e,0xd5,0xd8,0xcf,0xc2,0xe1,0xec,0xfb,0xf6,
    0xd6,0xdb,0xcc,0xc1,0xe2,0xef,0xf8,0xf5,0xbe,0xb3,0xa4,0xa9,0x8a,0x87,0x90,0x9d,
    0x06,0x0b,0x1c,0x11,0x32,0x3f,0x28,0x25,0x6e,0x63,0x74,0x79,0x5a,0x57,0x40,0x4d,
    0xda,0xd7,0xc0,0xcd,0xee,0xe3,0xf4,0xf9,0xb2,0xbf,0xa8,0xa5,0x86,0x8b,0x9c,0x91,
    0x0a,0x07,0x10,0x1d,0x3e,0x33,0x24,0x29,0x62,0x6f,0x78,0x75,0x56,0x5b,0x4c,0x41,
    0x61,0x6c,0x7b,0x76,0x55,0x58,0x4f,0x42,0x09,0x04,0x13,0x1e,0x3d,0x30,0x27,0x2a,
    0xb1,0xbc,0xab,0xa6,0x85,0x88,0x9f,0x92,0xd9,0xd4,0xc3,0xce,0xed,0xe0,0xf7,0xfa,
    0xb7,0xba,0xad,0xa0,0x83,0x8e,0x99,0x94,0xdf,0xd2,0xc5,0xc8,0xeb,0xe6,0xf1,0xfc,
    0x67,0x6a,0x7d,0x70,0x53,0x5e,0x49,0x44,0x0f,0x02,0x15,0x18,0x3b,0x36,0x21,0x2c,
    0x0c,0x01,0x16,0x1b,0x38,0x35,0x22,0x2f,0x64,0x69,0x7e,0x73,0x50,0x5d,0x4a,0x47,
    0xdc,0xd1,0xc6,0xcb,0xe8,0xe5,0xf2,0xff,0xb4,0xb9,0xae,0xa3,0x80,0x8d,0x9a,0x97
};
static const uint8_t gf_mul_14[256] = {
    0x00,0x0e,0x1c,0x12,0x38,0x36,0x24,0x2a,0x70,0x7e,0x6c,0x62,0x48,0x46,0x54,0x5a,
    0xe0,0xee,0xfc,0xf2,0xd8,0xd6,0xc4,0xca,0x90,0x9e,0x8c,0x82,0xa8,0xa6,0xb4,0xba,
    0xdb,0xd5,0xc7,0xc9,0xe3,0xed,0xff,0xf1,0xab,0xa5,0xb7,0xb9,0x93,0x9d,0x8f,0x81,
    0x3b,0x35,0x27,0x29,0x03,0x0d,0x1f,0x11,0x4b,0x45,0x57,0x59,0x73,0x7d,0x6f,0x61,
    0xad,0xa3,0xb1,0xbf,0x95,0x9b,0x89,0x87,0xdd,0xd3,0xc1,0xcf,0xe5,0xeb,0xf9,0xf7,
    0x4d,0x43,0x51,0x5f,0x75,0x7b,0x69,0x67,0x3d,0x33,0x21,0x2f,0x05,0x0b,0x19,0x17,
    0x76,0x78,0x6a,0x64,0x4e,0x40,0x52,0x5c,0x06,0x08,0x1a,0x14,0x3e,0x30,0x22,0x2c,
    0x96,0x98,0x8a,0x84,0xae,0xa0,0xb2,0xbc,0xe6,0xe8,0xfa,0xf4,0xde,0xd0,0xc2,0xcc,
    0x41,0x4f,0x5d,0x53,0x79,0x77,0x65,0x6b,0x31,0x3f,0x2d,0x23,0x09,0x07,0x15,0x1b,
    0xa1,0xaf,0xbd,0xb3,0x99,0x97,0x85,0x8b,0xd1,0xdf,0xcd,0xc3,0xe9,0xe7,0xf5,0xfb,
    0x9a,0x94,0x86,0x88,0xa2,0xac,0xbe,0xb0,0xea,0xe4,0xf6,0xf8,0xd2,0xdc,0xce,0xc0,
    0x7a,0x74,0x66,0x68,0x42,0x4c,0x5e,0x50,0x0a,0x04,0x16,0x18,0x32,0x3c,0x2e,0x20,
    0xec,0xe2,0xf0,0xfe,0xd4,0xda,0xc8,0xc6,0x9c,0x92,0x80,0x8e,0xa4,0xaa,0xb8,0xb6,
    0x0c,0x02,0x10,0x1e,0x34,0x3a,0x28,0x26,0x7c,0x72,0x60,0x6e,0x44,0x4a,0x58,0x56,
    0x37,0x39,0x2b,0x25,0x0f,0x01,0x13,0x1d,0x47,0x49,0x5b,0x55,0x7f,0x71,0x63,0x6d,
    0xd7,0xd9,0xcb,0xc5,0xef,0xe1,0xf3,0xfd,0xa7,0xa9,0xbb,0xb5,0x9f,0x91,0x83,0x8d
};

static void inv_mix_columns(uint8_t s[16]) {
    for (int c = 0; c < 4; c++) {
        int i = c * 4;
        uint8_t a0 = s[i], a1 = s[i+1], a2 = s[i+2], a3 = s[i+3];
        s[i]   = gf_mul_14[a0] ^ gf_mul_11[a1] ^ gf_mul_13[a2] ^ gf_mul_9[a3];
        s[i+1] = gf_mul_9[a0]  ^ gf_mul_14[a1] ^ gf_mul_11[a2] ^ gf_mul_13[a3];
        s[i+2] = gf_mul_13[a0] ^ gf_mul_9[a1]  ^ gf_mul_14[a2] ^ gf_mul_11[a3];
        s[i+3] = gf_mul_11[a0] ^ gf_mul_13[a1] ^ gf_mul_9[a2]  ^ gf_mul_14[a3];
    }
}

void aes128_decrypt_block(const aes128_ctx_t *ctx,
                          const uint8_t in[AES_BLOCK_SIZE],
                          uint8_t out[AES_BLOCK_SIZE]) {
    uint8_t state[16];
    _memcpy(state, in, 16);

    add_round_key(state, &ctx->round_key[AES_ROUNDS * 4]);

    for (int round = AES_ROUNDS - 1; round >= 1; round--) {
        inv_shift_rows(state);
        inv_sub_bytes(state);
        add_round_key(state, &ctx->round_key[round * 4]);
        inv_mix_columns(state);
    }

    inv_shift_rows(state);
    inv_sub_bytes(state);
    add_round_key(state, &ctx->round_key[0]);

    _memcpy(out, state, 16);
}

/* --- CBC mode --- */

void aes128_cbc_encrypt(const aes128_ctx_t *ctx,
                        uint8_t *iv,
                        const uint8_t *plain, uint32_t len,
                        uint8_t *cipher) {
    uint32_t blocks = len / AES_BLOCK_SIZE;
    for (uint32_t b = 0; b < blocks; b++) {
        uint8_t tmp[AES_BLOCK_SIZE];
        for (int i = 0; i < AES_BLOCK_SIZE; i++)
            tmp[i] = plain[b * AES_BLOCK_SIZE + i] ^ iv[i];
        aes128_encrypt_block(ctx, tmp, cipher + b * AES_BLOCK_SIZE);
        _memcpy(iv, cipher + b * AES_BLOCK_SIZE, AES_BLOCK_SIZE);
    }
}

void aes128_cbc_decrypt(const aes128_ctx_t *ctx,
                        const uint8_t *iv,
                        const uint8_t *cipher, uint32_t len,
                        uint8_t *plain) {
    uint32_t blocks = len / AES_BLOCK_SIZE;
    const uint8_t *prev = iv;
    for (uint32_t b = 0; b < blocks; b++) {
        uint8_t tmp[AES_BLOCK_SIZE];
        aes128_decrypt_block(ctx, cipher + b * AES_BLOCK_SIZE, tmp);
        for (int i = 0; i < AES_BLOCK_SIZE; i++)
            plain[b * AES_BLOCK_SIZE + i] = tmp[i] ^ prev[i];
        prev = cipher + b * AES_BLOCK_SIZE;
    }
}

/*=============================================================================
 * BIG INTEGER (2048-bit, little-endian limbs)
 *===========================================================================*/

void bn_zero(bignum_t *a) {
    for (int i = 0; i < BIGNUM_LIMBS; i++) a->d[i] = 0;
}

/** Load big-endian byte buffer into bignum (little-endian limbs) */
void bn_from_bytes(bignum_t *a, const uint8_t *buf, uint32_t len) {
    bn_zero(a);
    for (uint32_t i = 0; i < len && (i / 4) < BIGNUM_LIMBS; i++) {
        uint32_t limb_idx = i / 4;
        uint32_t byte_idx = i % 4;
        a->d[limb_idx] |= ((uint32_t)buf[len - 1 - i]) << (byte_idx * 8);
    }
}

/** Write bignum to big-endian byte buffer */
static void bn_to_bytes(const bignum_t *a, uint8_t *buf, uint32_t len) {
    _memset(buf, 0, len);
    for (uint32_t i = 0; i < len && (i / 4) < BIGNUM_LIMBS; i++) {
        uint32_t limb_idx = i / 4;
        uint32_t byte_idx = i % 4;
        buf[len - 1 - i] = (uint8_t)(a->d[limb_idx] >> (byte_idx * 8));
    }
}

int bn_cmp(const bignum_t *a, const bignum_t *b) {
    for (int i = BIGNUM_LIMBS - 1; i >= 0; i--) {
        if (a->d[i] > b->d[i]) return 1;
        if (a->d[i] < b->d[i]) return -1;
    }
    return 0;
}

static int bn_is_zero(const bignum_t *a) {
    for (int i = 0; i < BIGNUM_LIMBS; i++)
        if (a->d[i] != 0) return 0;
    return 1;
}

static int bn_bit(const bignum_t *a, int n) {
    return (a->d[n / 32] >> (n % 32)) & 1;
}

static int bn_bit_length(const bignum_t *a) {
    for (int i = BIGNUM_LIMBS - 1; i >= 0; i--) {
        if (a->d[i] == 0) continue;
        uint32_t v = a->d[i];
        int bits = i * 32;
        while (v) { bits++; v >>= 1; }
        return bits;
    }
    return 0;
}

/** a = a + b, returns carry */
static uint32_t bn_add(bignum_t *a, const bignum_t *b) {
    uint64_t carry = 0;
    for (int i = 0; i < BIGNUM_LIMBS; i++) {
        uint64_t sum = (uint64_t)a->d[i] + (uint64_t)b->d[i] + carry;
        a->d[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    return (uint32_t)carry;
}

/** a = a - b, assumes a >= b */
static void bn_sub(bignum_t *a, const bignum_t *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < BIGNUM_LIMBS; i++) {
        uint64_t diff = (uint64_t)a->d[i] - (uint64_t)b->d[i] - borrow;
        a->d[i] = (uint32_t)diff;
        borrow = (diff >> 63) & 1;
    }
}

/** a = a << 1 */
static void bn_shl1(bignum_t *a) {
    uint32_t carry = 0;
    for (int i = 0; i < BIGNUM_LIMBS; i++) {
        uint32_t next_carry = a->d[i] >> 31;
        a->d[i] = (a->d[i] << 1) | carry;
        carry = next_carry;
    }
}

/** result = (a * b) mod m — uses word-level Comba multiply + Barrett-like reduction */
static void bn_mod_mul(bignum_t *result, const bignum_t *a,
                       const bignum_t *b, const bignum_t *m) {
    /* Double-width product using word-level multiply (Comba / schoolbook) */
    uint32_t prod[BIGNUM_LIMBS * 2];
    for (int i = 0; i < BIGNUM_LIMBS * 2; i++) prod[i] = 0;

    /* Schoolbook word multiply: O(n^2) word ops instead of O(n^2) bit ops */
    int a_top = BIGNUM_LIMBS - 1;
    while (a_top > 0 && a->d[a_top] == 0) a_top--;
    int b_top = BIGNUM_LIMBS - 1;
    while (b_top > 0 && b->d[b_top] == 0) b_top--;

    for (int i = 0; i <= a_top; i++) {
        uint64_t carry = 0;
        for (int j = 0; j <= b_top; j++) {
            uint64_t uv = (uint64_t)a->d[i] * (uint64_t)b->d[j]
                        + (uint64_t)prod[i + j] + carry;
            prod[i + j] = (uint32_t)uv;
            carry = uv >> 32;
        }
        prod[i + b_top + 1] += (uint32_t)carry;
    }

    /* Reduction: repeated subtraction using aligned shifts of m.
     * Find highest bit of product, subtract m << shift until product < m. */
    int m_top = BIGNUM_LIMBS - 1;
    while (m_top > 0 && m->d[m_top] == 0) m_top--;
    int m_bits = m_top * 32;
    { uint32_t v = m->d[m_top]; while (v) { m_bits++; v >>= 1; } }

    int p_top = BIGNUM_LIMBS * 2 - 1;
    while (p_top > 0 && prod[p_top] == 0) p_top--;
    int p_bits = p_top * 32;
    { uint32_t v = prod[p_top]; while (v) { p_bits++; v >>= 1; } }

    /* Trial subtraction at each bit position from top down */
    for (int shift = p_bits - m_bits; shift >= 0; shift--) {
        /* Compare prod >= (m << shift) */
        int sw = shift / 32;
        int sb = shift % 32;

        /* Check if prod >= m_shifted */
        int ge = 0;
        {
            /* Compare from top down */
            int cmp = 0;
            for (int k = m_top + sw + 1; k >= sw; k--) {
                uint32_t mk;
                int mi = k - sw;
                if (mi < 0 || mi > m_top + 1) { mk = 0; }
                else if (mi > m_top) {
                    mk = (sb > 0) ? (m->d[mi - 1] >> (32 - sb)) : 0;
                } else if (mi == 0) {
                    mk = m->d[0] << sb;
                } else {
                    mk = (m->d[mi] << sb) | (sb > 0 ? (m->d[mi - 1] >> (32 - sb)) : 0);
                }
                uint32_t pk = (k < BIGNUM_LIMBS * 2) ? prod[k] : 0;
                if (pk > mk) { cmp = 1; break; }
                if (pk < mk) { cmp = -1; break; }
            }
            ge = (cmp >= 0);
        }

        if (ge) {
            /* Subtract m << shift from prod */
            uint64_t borrow = 0;
            for (int k = 0; k <= m_top + 1; k++) {
                uint32_t mk;
                if (k > m_top) {
                    mk = (sb > 0) ? (m->d[k - 1] >> (32 - sb)) : 0;
                } else if (k == 0) {
                    mk = m->d[0] << sb;
                } else {
                    mk = (m->d[k] << sb) | (sb > 0 ? (m->d[k - 1] >> (32 - sb)) : 0);
                }
                int idx = k + sw;
                if (idx >= BIGNUM_LIMBS * 2) break;
                uint64_t diff = (uint64_t)prod[idx] - (uint64_t)mk - borrow;
                prod[idx] = (uint32_t)diff;
                borrow = (diff >> 63) & 1;
            }
        }
    }

    /* Copy result */
    for (int i = 0; i < BIGNUM_LIMBS; i++) result->d[i] = prod[i];
}

/**
 * Modular exponentiation: result = base^exp mod mod
 * Uses binary (square-and-multiply) method.
 */
void bn_mod_exp(bignum_t *result, const bignum_t *base,
                const bignum_t *exp, const bignum_t *mod) {
    bignum_t r, b;
    bn_zero(&r);
    r.d[0] = 1; /* r = 1 */
    b = *base;

    /* Reduce base mod m */
    while (bn_cmp(&b, mod) >= 0) bn_sub(&b, mod);

    int bits = bn_bit_length(exp);
    for (int i = 0; i < bits; i++) {
        if (bn_bit(exp, i)) {
            bn_mod_mul(&r, &r, &b, mod);
        }
        bn_mod_mul(&b, &b, &b, mod);
    }

    *result = r;
}

/*=============================================================================
 * RSA PKCS#1 v1.5 Type 2 Encryption
 *===========================================================================*/

int rsa_pkcs1_encrypt(const rsa_pubkey_t *rsa,
                      const uint8_t *msg, uint32_t msg_len,
                      uint8_t *out) {
    uint32_t k = rsa->mod_bytes;
    /* PKCS#1: EM = 0x00 || 0x02 || PS || 0x00 || M */
    /* PS must be at least 8 bytes of non-zero random */
    if (msg_len + 11 > k) return -1;

    uint8_t em[RSA_MAX_MODULUS_BYTES];
    _memset(em, 0, k);
    em[0] = 0x00;
    em[1] = 0x02;

    /* Fill padding with non-zero random bytes */
    uint32_t ps_len = k - msg_len - 3;
    tls_random_bytes(em + 2, ps_len);
    /* Ensure no zero bytes in padding */
    for (uint32_t i = 0; i < ps_len; i++) {
        while (em[2 + i] == 0) {
            tls_random_bytes(&em[2 + i], 1);
        }
    }

    em[2 + ps_len] = 0x00;
    _memcpy(em + 3 + ps_len, msg, msg_len);

    /* Convert to bignum and perform RSA: c = m^e mod n */
    bignum_t m_bn, c_bn;
    bn_from_bytes(&m_bn, em, k);
    bn_mod_exp(&c_bn, &m_bn, &rsa->e, &rsa->n);
    bn_to_bytes(&c_bn, out, k);

    return 0;
}

/*=============================================================================
 * TLS 1.2 PRF (RFC 5246 Section 5)
 *
 * PRF(secret, label, seed) = P_SHA256(secret, label + seed)
 * P_hash(secret, seed) = HMAC(secret, A(1)+seed) || HMAC(secret, A(2)+seed) || ...
 * A(0) = seed,  A(i) = HMAC(secret, A(i-1))
 *===========================================================================*/

void tls_prf_sha256(const uint8_t *secret, uint32_t secret_len,
                    const char *label,
                    const uint8_t *seed, uint32_t seed_len,
                    uint8_t *output, uint32_t out_len) {
    uint32_t label_len = _strlen(label);

    /* Concatenate label + seed into ls_buf */
    uint8_t ls_buf[128];
    uint32_t ls_len = label_len + seed_len;
    if (ls_len > sizeof(ls_buf)) ls_len = sizeof(ls_buf);
    _memcpy(ls_buf, (const uint8_t *)label, label_len);
    if (seed_len > 0 && label_len + seed_len <= sizeof(ls_buf))
        _memcpy(ls_buf + label_len, seed, seed_len);

    /* A(0) = label + seed */
    uint8_t a[HMAC_SHA256_SIZE];
    /* A(1) = HMAC(secret, A(0)) */
    hmac_sha256(secret, secret_len, ls_buf, ls_len, a);

    uint32_t pos = 0;
    while (pos < out_len) {
        /* P_hash chunk = HMAC(secret, A(i) + label + seed) */
        hmac_sha256_ctx_t hctx;
        hmac_sha256_init(&hctx, secret, secret_len);
        hmac_sha256_update(&hctx, a, HMAC_SHA256_SIZE);
        hmac_sha256_update(&hctx, ls_buf, ls_len);
        uint8_t chunk[HMAC_SHA256_SIZE];
        hmac_sha256_final(&hctx, chunk);

        uint32_t to_copy = out_len - pos;
        if (to_copy > HMAC_SHA256_SIZE) to_copy = HMAC_SHA256_SIZE;
        _memcpy(output + pos, chunk, to_copy);
        pos += to_copy;

        /* A(i+1) = HMAC(secret, A(i)) */
        hmac_sha256(secret, secret_len, a, HMAC_SHA256_SIZE, a);
    }
}

/*=============================================================================
 * PRNG — seeded from RDTSC + simple xorshift
 *===========================================================================*/

static uint32_t prng_state[4] = {0, 0, 0, 0};
static int prng_seeded = 0;

static uint32_t rdtsc_low(void) {
    uint32_t lo;
    __asm__ volatile ("rdtsc" : "=a"(lo) : : "edx");
    return lo;
}

void tls_random_seed(void) {
    prng_state[0] = rdtsc_low();
    prng_state[1] = rdtsc_low() ^ 0xCAFEBABE;
    prng_state[2] = rdtsc_low() ^ 0xDEADBEEF;
    prng_state[3] = rdtsc_low() ^ 0x8BADF00D;
    /* Mix the state */
    for (int i = 0; i < 20; i++) {
        uint32_t t = prng_state[3];
        uint32_t s = prng_state[0];
        prng_state[3] = prng_state[2];
        prng_state[2] = prng_state[1];
        prng_state[1] = s;
        t ^= t << 11;
        t ^= t >> 8;
        prng_state[0] = t ^ s ^ (s >> 19);
    }
    prng_seeded = 1;
}

static uint32_t prng_next(void) {
    if (!prng_seeded) tls_random_seed();
    uint32_t t = prng_state[3];
    uint32_t s = prng_state[0];
    prng_state[3] = prng_state[2];
    prng_state[2] = prng_state[1];
    prng_state[1] = s;
    t ^= t << 11;
    t ^= t >> 8;
    prng_state[0] = t ^ s ^ (s >> 19);
    return prng_state[0];
}

void tls_random_bytes(uint8_t *buf, uint32_t len) {
    uint32_t i = 0;
    while (i < len) {
        uint32_t r = prng_next();
        for (int j = 0; j < 4 && i < len; j++, i++) {
            buf[i] = (uint8_t)(r >> (j * 8));
        }
    }
}
