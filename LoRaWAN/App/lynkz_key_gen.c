#include "platform.h"
#include "sys_app.h"
#include "stm32wlxx_ll_system.h"
#include "LmHandler.h"
#include "lynkz_key_gen.h"

#include <string.h>
#include <stdint.h>

/* -------------------------------------------------------------------------
 * Prefixes (upper 4 bytes of the 8-byte DevEUI).
 * The Lynkz provisioning server tries every prefix and confirms with MIC.
 * ------------------------------------------------------------------------- */
static const uint8_t PREFIXES[4][4] = {
    {0xA1, 0xB2, 0xC3, 0xD4},
    {0x5F, 0x9E, 0x21, 0xA7},
    {0xD8, 0xA4, 0xF1, 0x6C},
    {0x3C, 0x7B, 0x92, 0xE5},
};

/* =========================================================================
 * Minimal SHA-256 (FIPS 180-4)
 * ========================================================================= */

#define ROTR32(x, n) (((x) >> (n)) | ((x) << (32u - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)       (ROTR32(x,  2) ^ ROTR32(x, 13) ^ ROTR32(x, 22))
#define EP1(x)       (ROTR32(x,  6) ^ ROTR32(x, 11) ^ ROTR32(x, 25))
#define SIG0(x)      (ROTR32(x,  7) ^ ROTR32(x, 18) ^ ((x) >>  3))
#define SIG1(x)      (ROTR32(x, 17) ^ ROTR32(x, 19) ^ ((x) >> 10))

static const uint32_t K[64] = {
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
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

typedef struct {
    uint8_t  buf[64];
    uint32_t buflen;
    uint64_t bitlen;
    uint32_t s[8];
} sha256_ctx_t;

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t blk[64])
{
    uint32_t m[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;

    for (i = 0; i < 16; i++)
        m[i] = ((uint32_t)blk[i*4]   << 24) | ((uint32_t)blk[i*4+1] << 16) |
               ((uint32_t)blk[i*4+2] <<  8) |  (uint32_t)blk[i*4+3];
    for (; i < 64; i++)
        m[i] = SIG1(m[i-2]) + m[i-7] + SIG0(m[i-15]) + m[i-16];

    a = ctx->s[0]; b = ctx->s[1]; c = ctx->s[2]; d = ctx->s[3];
    e = ctx->s[4]; f = ctx->s[5]; g = ctx->s[6]; h = ctx->s[7];

    for (i = 0; i < 64; i++) {
        t1 = h + EP1(e) + CH(e,f,g) + K[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }

    ctx->s[0]+=a; ctx->s[1]+=b; ctx->s[2]+=c; ctx->s[3]+=d;
    ctx->s[4]+=e; ctx->s[5]+=f; ctx->s[6]+=g; ctx->s[7]+=h;
}

static void sha256_init(sha256_ctx_t *ctx)
{
    ctx->buflen = 0; ctx->bitlen = 0;
    ctx->s[0]=0x6a09e667; ctx->s[1]=0xbb67ae85;
    ctx->s[2]=0x3c6ef372; ctx->s[3]=0xa54ff53a;
    ctx->s[4]=0x510e527f; ctx->s[5]=0x9b05688c;
    ctx->s[6]=0x1f83d9ab; ctx->s[7]=0x5be0cd19;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        ctx->buf[ctx->buflen++] = data[i];
        if (ctx->buflen == 64) {
            sha256_transform(ctx, ctx->buf);
            ctx->bitlen += 512;
            ctx->buflen = 0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32])
{
    uint32_t i = ctx->buflen;
    int j, w;

    ctx->buf[i++] = 0x80;
    if (i > 56) {
        while (i < 64) ctx->buf[i++] = 0;
        sha256_transform(ctx, ctx->buf);
        i = 0;
    }
    while (i < 56) ctx->buf[i++] = 0;

    ctx->bitlen += ctx->buflen * 8;
    for (j = 7; j >= 0; j--)
        ctx->buf[56 + (7-j)] = (uint8_t)((ctx->bitlen >> (j * 8)) & 0xFF);
    sha256_transform(ctx, ctx->buf);

    for (i = 0; i < 4; i++)
        for (w = 0; w < 8; w++)
            hash[w*4 + i] = (uint8_t)((ctx->s[w] >> (24 - i*8)) & 0xFF);
}

/* =========================================================================
 * HMAC-SHA256
 * ========================================================================= */

static void hmac_sha256(const uint8_t *key, size_t key_len,
                        const uint8_t *msg, size_t msg_len,
                        uint8_t out[32])
{
    sha256_ctx_t ctx;
    uint8_t k_ipad[64], k_opad[64], inner[32], kbuf[32];
    size_t i;

    if (key_len > 64) {
        sha256_init(&ctx);
        sha256_update(&ctx, key, key_len);
        sha256_final(&ctx, kbuf);
        key = kbuf; key_len = 32;
    }

    memset(k_ipad, 0x36, 64);
    memset(k_opad, 0x5C, 64);
    for (i = 0; i < key_len; i++) {
        k_ipad[i] ^= key[i];
        k_opad[i] ^= key[i];
    }

    sha256_init(&ctx);
    sha256_update(&ctx, k_ipad, 64);
    sha256_update(&ctx, msg, msg_len);
    sha256_final(&ctx, inner);

    sha256_init(&ctx);
    sha256_update(&ctx, k_opad, 64);
    sha256_update(&ctx, inner, 32);
    sha256_final(&ctx, out);
}

/* =========================================================================
 * HKDF-SHA256 (RFC 5869) — single-block expand (output <= 32 bytes)
 * ========================================================================= */

static void hkdf_sha256_16(const uint8_t *ikm, size_t ikm_len,
                            uint8_t salt_byte,
                            uint8_t out[16])
{
    uint8_t prk[32], t[32];
    const uint8_t counter = 0x01;

    /* Extract: PRK = HMAC-SHA256(salt, IKM) */
    hmac_sha256(&salt_byte, 1, ikm, ikm_len, prk);

    /* Expand T(1) = HMAC-SHA256(PRK, 0x01)  [info="" so data = counter] */
    hmac_sha256(prk, 32, &counter, 1, t);

    memcpy(out, t, 16);
}

/* =========================================================================
 * Public API
 * ========================================================================= */

void LynkzKeyGen_Apply(void)
{
    uint8_t  dev_eui[8];
    uint8_t  app_s_key[16];
    uint8_t  nwk_s_key[16];
    uint32_t dev_addr;

    /* Match the exact DevAddr logic used by GetDevAddr() in sys_app.c */
    dev_addr = LL_FLASH_GetUDN();
    if (dev_addr == 0xFFFFFFFF)
        dev_addr = HAL_GetUIDw0() ^ HAL_GetUIDw1() ^ HAL_GetUIDw2();

    /* DevEUI = prefix[4] || dev_addr[4] (big-endian)
     * Prefix index is the two lowest bits of the DevAddr — deterministic
     * and device-specific, no configuration required. */
    memcpy(dev_eui, PREFIXES[dev_addr & 0x03], 4);
    dev_eui[4] = (uint8_t)(dev_addr >> 24);
    dev_eui[5] = (uint8_t)(dev_addr >> 16);
    dev_eui[6] = (uint8_t)(dev_addr >>  8);
    dev_eui[7] = (uint8_t)(dev_addr);

    hkdf_sha256_16(dev_eui, 8, 0x00, app_s_key);
    hkdf_sha256_16(dev_eui, 8, 0x01, nwk_s_key);

    LmHandlerSetDevEUI(dev_eui);
    LmHandlerSetKey(APP_S_KEY, app_s_key);
    LmHandlerSetKey(NWK_S_KEY, nwk_s_key);

    APP_LOG(TS_OFF, VLEVEL_M,
            "LYNKZ DevEUI:  %02X%02X%02X%02X%02X%02X%02X%02X\r\n",
            dev_eui[0], dev_eui[1], dev_eui[2], dev_eui[3],
            dev_eui[4], dev_eui[5], dev_eui[6], dev_eui[7]);
    APP_LOG(TS_OFF, VLEVEL_M,
            "LYNKZ DevAddr: %08X\r\n", (unsigned)dev_addr);
}
