/**
 ******************************************************************************
 * @file    ota_sha256.c
 * @brief   ota sha256 define.
 * @verbatim    null
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2025 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of POSSUMIC TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */


/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "ota_config.h"

#if (CONFIG_OTA == 1)

#include "ota_sha256.h"


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */

static void _ota_compress(unsigned int *iv, const uint8_t *data);
static void _ota_set(void *to, uint8_t val, unsigned int len);

int OTA_SHA256_Init(OTA_SHA256_t *s)
{
    /* input sanity check: */
    if (s == (OTA_SHA256_t *) 0) {
        return OTA_CRYPTO_FAIL;
    }

    /*
     * Setting the initial state values.
     * These values correspond to the first 32 bits of the fractional parts
     * of the square roots of the first 8 primes: 2, 3, 5, 7, 11, 13, 17
     * and 19.
     */
    _ota_set((uint8_t *) s, 0x00, sizeof(*s));
    s->iv[0] = 0x6a09e667;
    s->iv[1] = 0xbb67ae85;
    s->iv[2] = 0x3c6ef372;
    s->iv[3] = 0xa54ff53a;
    s->iv[4] = 0x510e527f;
    s->iv[5] = 0x9b05688c;
    s->iv[6] = 0x1f83d9ab;
    s->iv[7] = 0x5be0cd19;

    return OTA_CRYPTO_SUCCESS;
}

int OTA_SHA256_Update(OTA_SHA256_t *s, const uint8_t *data, size_t datalen)
{
    /* input sanity check: */
    if (s == (OTA_SHA256_t *) 0 ||
        data == (void *) 0) {
        return OTA_CRYPTO_FAIL;
    } else if (datalen == 0) {
        return OTA_CRYPTO_SUCCESS;
    }

    while (datalen-- > 0) {
        s->leftover[s->leftoverOffset++] = *(data++);
        if (s->leftoverOffset >= OTA_SHA256_BLOCK_SIZE) {
            _ota_compress(s->iv, s->leftover);
            s->leftoverOffset = 0;
            s->bitsHashed += (OTA_SHA256_BLOCK_SIZE << 3);
        }
    }

    return OTA_CRYPTO_SUCCESS;
}

int OTA_SHA256_Final(uint8_t *digest, OTA_SHA256_t *s)
{
    unsigned int i;

    /* input sanity check: */
    if (digest == (uint8_t *) 0 ||
        s == (OTA_SHA256_t *) 0) {
        return OTA_CRYPTO_FAIL;
    }

    s->bitsHashed += (s->leftoverOffset << 3);

    s->leftover[s->leftoverOffset++] = 0x80; /* always room for one byte */
    if (s->leftoverOffset > (sizeof(s->leftover) - 8)) {
        /* there is not room for all the padding in this block */
        _ota_set(s->leftover + s->leftoverOffset, 0x00,
             sizeof(s->leftover) - s->leftoverOffset);
        _ota_compress(s->iv, s->leftover);
        s->leftoverOffset = 0;
    }

    /* add the padding and the length in big-Endian format */
    _ota_set(s->leftover + s->leftoverOffset, 0x00,
         sizeof(s->leftover) - 8 - s->leftoverOffset);
    s->leftover[sizeof(s->leftover) - 1] = (uint8_t)(s->bitsHashed);
    s->leftover[sizeof(s->leftover) - 2] = (uint8_t)(s->bitsHashed >> 8);
    s->leftover[sizeof(s->leftover) - 3] = (uint8_t)(s->bitsHashed >> 16);
    s->leftover[sizeof(s->leftover) - 4] = (uint8_t)(s->bitsHashed >> 24);
    s->leftover[sizeof(s->leftover) - 5] = (uint8_t)(s->bitsHashed >> 32);
    s->leftover[sizeof(s->leftover) - 6] = (uint8_t)(s->bitsHashed >> 40);
    s->leftover[sizeof(s->leftover) - 7] = (uint8_t)(s->bitsHashed >> 48);
    s->leftover[sizeof(s->leftover) - 8] = (uint8_t)(s->bitsHashed >> 56);

    /* hash the padding and length */
    _ota_compress(s->iv, s->leftover);

    /* copy the iv out to digest */
//    volatile unsigned int t;
    for (i = 0; i < OTA_SHA256_STATE_BLOCKS; ++i) {
        volatile unsigned int t = s->iv[i];
        *digest++ = (uint8_t)(t >> 24);
        *digest++ = (uint8_t)(t >> 16);
        *digest++ = (uint8_t)(t >> 8);
        *digest++ = (uint8_t)(t);
    }

    /* destroy the current state */
    _ota_set(s, 0, sizeof(*s));

    return OTA_CRYPTO_SUCCESS;
}

/*
 * Initializing SHA-256 Hash constant words K.
 * These values correspond to the first 32 bits of the fractional parts of the
 * cube roots of the first 64 primes between 2 and 311.
 */
static const unsigned int _ota_k256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static inline unsigned int ROTR(unsigned int a, unsigned int n)
{
    return (((a) >> n) | ((a) << (32 - n)));
}

#define Sigma0(a)(ROTR((a), 2) ^ ROTR((a), 13) ^ ROTR((a), 22))
#define Sigma1(a)(ROTR((a), 6) ^ ROTR((a), 11) ^ ROTR((a), 25))
#define sigma0(a)(ROTR((a), 7) ^ ROTR((a), 18) ^ ((a) >> 3))
#define sigma1(a)(ROTR((a), 17) ^ ROTR((a), 19) ^ ((a) >> 10))

#define Ch(a, b, c)(((a) & (b)) ^ ((~(a)) & (c)))
#define Maj(a, b, c)(((a) & (b)) ^ ((a) & (c)) ^ ((b) & (c)))

static inline unsigned int BigEndian(const uint8_t **c)
{
    unsigned int n = 0;

    n = (((unsigned int)(*((*c)++))) << 24);
    n |= ((unsigned int)(*((*c)++)) << 16);
    n |= ((unsigned int)(*((*c)++)) << 8);
    n |= ((unsigned int)(*((*c)++)));
    return n;
}

static void _ota_compress(unsigned int *iv, const uint8_t *data)
{
    unsigned int a, b, c, d, e, f, g, h;
    unsigned int s0, s1;
    unsigned int t1, t2;
    unsigned int work_space[16];
    unsigned int n;
    unsigned int i;

    a = iv[0]; b = iv[1]; c = iv[2]; d = iv[3];
    e = iv[4]; f = iv[5]; g = iv[6]; h = iv[7];

    for (i = 0; i < 16; ++i) {
        n = BigEndian(&data);
        t1 = work_space[i] = n;
        t1 += h + Sigma1(e) + Ch(e, f, g) + _ota_k256[i];
        t2 = Sigma0(a) + Maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    for ( ; i < 64; ++i) {
        s0 = work_space[(i+1)&0x0f];
        s0 = sigma0(s0);
        s1 = work_space[(i+14)&0x0f];
        s1 = sigma1(s1);

        t1 = work_space[i&0xf] += s0 + s1 + work_space[(i+9)&0xf];
        t1 += h + Sigma1(e) + Ch(e, f, g) + _ota_k256[i];
        t2 = Sigma0(a) + Maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    iv[0] += a; iv[1] += b; iv[2] += c; iv[3] += d;
    iv[4] += e; iv[5] += f; iv[6] += g; iv[7] += h;
}

static void _ota_set(void *to, uint8_t val, unsigned int len)
{
    (void)memset(to, val, len);
}

#endif  /* CONFIG_OTA == 1 */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
