/**
 ******************************************************************************
 * @file    ota sha256.h
 * @brief   ota sha256 define.
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

#ifndef _OTA_SHA256_H_
#define _OTA_SHA256_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "ota_config.h"

#if (CONFIG_OTA == 1)

#include "ota_types.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

#define OTA_CRYPTO_SUCCESS                      1
#define OTA_CRYPTO_FAIL                         0

#define OTA_ZERO_BYTE                           0x00

#define OTA_SHA256_BLOCK_SIZE                   (64)
#define OTA_SHA256_DIGEST_SIZE                  (32)
#define OTA_SHA256_STATE_BLOCKS                 (OTA_SHA256_DIGEST_SIZE/4)

/* Exported types.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    uint32_t    iv[OTA_SHA256_STATE_BLOCKS];
    uint64_t    bitsHashed;
    uint8_t     leftover[OTA_SHA256_BLOCK_SIZE];
    size_t      leftoverOffset;
} OTA_SHA256_t;

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


/**
 *  @brief SHA256 initialization procedure
 *  Initializes s
 *  @return returns TC_CRYPTO_SUCCESS (1)
 *          returns TC_CRYPTO_FAIL (0) if s == NULL
 *  @param s Sha256 state struct
 */
int OTA_SHA256_Init(OTA_SHA256_t *s);

/**
 *  @brief SHA256 update procedure
 *  Hashes data_length bytes addressed by data into state s
 *  @return returns TC_CRYPTO_SUCCESS (1)
 *          returns TC_CRYPTO_FAIL (0) if:
 *                s == NULL,
 *                s->iv == NULL,
 *                data == NULL
 *  @note Assumes s has been initialized by tc_sha256_init
 *  @warning The state buffer 'leftover' is left in memory after processing
 *           If your application intends to have sensitive data in this
 *           buffer, remind to erase it after the data has been processed
 *  @param s Sha256 state struct
 *  @param data message to hash
 *  @param datalen length of message to hash
 */
int OTA_SHA256_Update (OTA_SHA256_t *s, const uint8_t *data, size_t datalen);

/**
 *  @brief SHA256 final procedure
 *  Inserts the completed hash computation into digest
 *  @return returns TC_CRYPTO_SUCCESS (1)
 *          returns TC_CRYPTO_FAIL (0) if:
 *                s == NULL,
 *                s->iv == NULL,
 *                digest == NULL
 *  @note Assumes: s has been initialized by tc_sha256_init
 *        digest points to at least OTA_SHA256_DIGEST_SIZE bytes
 *  @warning The state buffer 'leftover' is left in memory after processing
 *           If your application intends to have sensitive data in this
 *           buffer, remind to erase it after the data has been processed
 *  @param digest unsigned eight bit integer
 *  @param Sha256 state struct
 */
int OTA_SHA256_Final(uint8_t *digest, OTA_SHA256_t *s);

#ifdef __cplusplus
}
#endif

#endif  /* CONFIG_OTA == 1 */

#endif /* _OTA_SHA256_H_ */


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
