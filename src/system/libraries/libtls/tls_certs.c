/**
 * MaahiOS TLS Certificate Parser — tls_certs.c
 *
 * Minimal ASN.1 DER walker to extract RSA public key (modulus + exponent)
 * from an X.509 certificate.
 *
 * We do NOT validate the certificate chain (no PKI for hobby OS).
 * We only extract the SubjectPublicKeyInfo RSA key.
 *
 * Certificate structure (simplified):
 *   Certificate ::= SEQUENCE {
 *     tbsCertificate     TBSCertificate,   ← we dig into this
 *     signatureAlgorithm ...,
 *     signatureValue     ...
 *   }
 *   TBSCertificate ::= SEQUENCE {
 *     version        [0] EXPLICIT ...,
 *     serialNumber   ...,
 *     signature      ...,
 *     issuer         ...,
 *     validity       ...,
 *     subject        ...,
 *     subjectPublicKeyInfo SubjectPublicKeyInfo,  ← we want this
 *     ...
 *   }
 *   SubjectPublicKeyInfo ::= SEQUENCE {
 *     algorithm  AlgorithmIdentifier,
 *     subjectPublicKey BIT STRING containing:
 *       RSAPublicKey ::= SEQUENCE {
 *         modulus        INTEGER,
 *         publicExponent INTEGER
 *       }
 *   }
 *
 * Freestanding — no libc dependency.
 * Layer 2 (Library). Ring 3.
 */

#include "tls_crypto.h"

/*=============================================================================
 * ASN.1 DER PARSER
 *===========================================================================*/

/* ASN.1 tag values */
#define ASN1_SEQUENCE    0x30
#define ASN1_SET         0x31
#define ASN1_INTEGER     0x02
#define ASN1_BITSTRING   0x03
#define ASN1_OCTETSTRING 0x04
#define ASN1_NULL        0x05
#define ASN1_OID         0x06
#define ASN1_CONTEXT_0   0xA0
#define ASN1_CONTEXT_3   0xA3

typedef struct {
    const uint8_t *data;
    uint32_t       len;
    uint32_t       pos;
} asn1_reader_t;

/** Read tag byte. Returns tag or -1 on EOF. */
static int asn1_read_tag(asn1_reader_t *r) {
    if (r->pos >= r->len) return -1;
    return r->data[r->pos++];
}

/** Read DER length. Returns length or -1 on error. */
static int32_t asn1_read_length(asn1_reader_t *r) {
    if (r->pos >= r->len) return -1;
    uint8_t b = r->data[r->pos++];
    if (b < 0x80) return (int32_t)b;

    int num_bytes = b & 0x7f;
    if (num_bytes > 4 || r->pos + num_bytes > r->len) return -1;

    int32_t length = 0;
    for (int i = 0; i < num_bytes; i++) {
        length = (length << 8) | r->data[r->pos++];
    }
    return length;
}

/** Read tag + length, return content start position and content length.
 *  Advances pos past the header. Returns 0 on success, -1 on error. */
static int asn1_enter(asn1_reader_t *r, int expected_tag,
                      uint32_t *content_start, uint32_t *content_len) {
    int tag = asn1_read_tag(r);
    if (tag < 0) return -1;
    if (expected_tag >= 0 && tag != expected_tag) return -1;
    int32_t len = asn1_read_length(r);
    if (len < 0) return -1;
    *content_start = r->pos;
    *content_len = (uint32_t)len;
    return 0;
}

/** Skip over a TLV element (tag already consumed position). */
static int asn1_skip_tlv(asn1_reader_t *r) {
    int tag = asn1_read_tag(r);
    if (tag < 0) return -1;
    int32_t len = asn1_read_length(r);
    if (len < 0 || r->pos + (uint32_t)len > r->len) return -1;
    r->pos += (uint32_t)len;
    return 0;
}

/** Skip one complete TLV from current position (reads tag+length+value). */
static int asn1_skip_element(asn1_reader_t *r) {
    return asn1_skip_tlv(r);
}

/*=============================================================================
 * RSA OID matching
 *===========================================================================*/

/* OID for rsaEncryption: 1.2.840.113549.1.1.1 */
static const uint8_t RSA_OID[] = {
    0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x01
};
#define RSA_OID_LEN  9

static int mem_equal(const uint8_t *a, const uint8_t *b, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        if (a[i] != b[i]) return 0;
    return 1;
}

/*=============================================================================
 * CERTIFICATE KEY EXTRACTION
 *===========================================================================*/

/**
 * Parse an X.509 DER certificate and extract the RSA public key.
 *
 * @param cert_data  DER-encoded certificate bytes
 * @param cert_len   Length of certificate
 * @param key_out    Output RSA public key
 * @return 0 on success, -1 on error
 */
int tls_parse_certificate_key(const uint8_t *cert_data, uint32_t cert_len,
                              rsa_pubkey_t *key_out) {
    asn1_reader_t r;
    r.data = cert_data;
    r.len = cert_len;
    r.pos = 0;

    uint32_t cs, cl;

    /* Certificate ::= SEQUENCE */
    if (asn1_enter(&r, ASN1_SEQUENCE, &cs, &cl) != 0) return -1;

    /* TBSCertificate ::= SEQUENCE */
    if (asn1_enter(&r, ASN1_SEQUENCE, &cs, &cl) != 0) return -1;
    uint32_t tbs_end = cs + cl;

    /* version [0] EXPLICIT — optional, skip if present */
    if (r.pos < tbs_end && r.data[r.pos] == ASN1_CONTEXT_0) {
        if (asn1_skip_element(&r) != 0) return -1;
    }

    /* serialNumber INTEGER — skip */
    if (asn1_skip_element(&r) != 0) return -1;

    /* signature AlgorithmIdentifier — skip */
    if (asn1_skip_element(&r) != 0) return -1;

    /* issuer Name — skip */
    if (asn1_skip_element(&r) != 0) return -1;

    /* validity Validity — skip */
    if (asn1_skip_element(&r) != 0) return -1;

    /* subject Name — skip */
    if (asn1_skip_element(&r) != 0) return -1;

    /* subjectPublicKeyInfo SubjectPublicKeyInfo */
    if (asn1_enter(&r, ASN1_SEQUENCE, &cs, &cl) != 0) return -1;

    /* AlgorithmIdentifier ::= SEQUENCE { algorithm OID, parameters } */
    uint32_t alg_cs, alg_cl;
    if (asn1_enter(&r, ASN1_SEQUENCE, &alg_cs, &alg_cl) != 0) return -1;

    /* Read algorithm OID */
    int tag = asn1_read_tag(&r);
    if (tag != ASN1_OID) return -1;
    int32_t oid_len = asn1_read_length(&r);
    if (oid_len < 0 || r.pos + (uint32_t)oid_len > r.len) return -1;

    /* Check it's rsaEncryption */
    if ((uint32_t)oid_len != RSA_OID_LEN ||
        !mem_equal(r.data + r.pos, RSA_OID, RSA_OID_LEN)) {
        return -1; /* Not RSA */
    }

    /* Skip past AlgorithmIdentifier (move to after the SEQUENCE) */
    r.pos = alg_cs + alg_cl;

    /* subjectPublicKey BIT STRING */
    tag = asn1_read_tag(&r);
    if (tag != ASN1_BITSTRING) return -1;
    int32_t bs_len = asn1_read_length(&r);
    if (bs_len < 1) return -1;

    /* First byte of BIT STRING is number of unused bits (should be 0) */
    r.pos++; /* skip unused bits byte */

    /* Inside BIT STRING: RSAPublicKey ::= SEQUENCE { modulus INTEGER, exponent INTEGER } */
    if (asn1_enter(&r, ASN1_SEQUENCE, &cs, &cl) != 0) return -1;

    /* modulus INTEGER */
    tag = asn1_read_tag(&r);
    if (tag != ASN1_INTEGER) return -1;
    int32_t mod_len = asn1_read_length(&r);
    if (mod_len < 0 || r.pos + (uint32_t)mod_len > r.len) return -1;

    /* Strip leading zero byte if present (ASN.1 integer sign byte) */
    const uint8_t *mod_data = r.data + r.pos;
    uint32_t mod_bytes = (uint32_t)mod_len;
    if (mod_bytes > 0 && mod_data[0] == 0x00) {
        mod_data++;
        mod_bytes--;
    }

    if (mod_bytes > RSA_MAX_MODULUS_BYTES) return -1;

    bn_from_bytes(&key_out->n, mod_data, mod_bytes);
    key_out->mod_bytes = mod_bytes;
    r.pos += (uint32_t)mod_len;

    /* publicExponent INTEGER */
    tag = asn1_read_tag(&r);
    if (tag != ASN1_INTEGER) return -1;
    int32_t exp_len = asn1_read_length(&r);
    if (exp_len < 0 || r.pos + (uint32_t)exp_len > r.len) return -1;

    const uint8_t *exp_data = r.data + r.pos;
    uint32_t exp_bytes = (uint32_t)exp_len;
    if (exp_bytes > 0 && exp_data[0] == 0x00) {
        exp_data++;
        exp_bytes--;
    }

    bn_from_bytes(&key_out->e, exp_data, exp_bytes);

    return 0;
}
