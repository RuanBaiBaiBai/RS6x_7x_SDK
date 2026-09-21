/**
 ******************************************************************************
 * @file    ota.h
 * @brief   ota define.
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


#ifndef _OTA_H_
#define _OTA_H_


/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "ota_config.h"

#if (CONFIG_OTA == 1)

#include "ota_types.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ----------------------------------------------------------------------------
 */

typedef struct {
    uint16_t minorR   ;
    uint8_t  typeR    ;
    uint8_t  projectM ;
    uint16_t numberM  ;
    uint16_t minorM   ;
    uint8_t  majorV   ;
    uint8_t  minorV   ;
    uint16_t revisionV;
} OTA_Version_t;

typedef struct {
    uint32_t        area_size;          /* Upgrade area size (Bytes) */
    OTA_Version_t   version;            /* Current image version */
} OTA_Info_t;

typedef enum {
    OTA_UPGRADE_MODE_TEST = 0,
    OTA_UPGRADE_MODE_PERM = 1,   /* permanent */
} OTA_UpgradeMode_t;

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int OTA_GetUpgradeInfo(OTA_Info_t *pinfo);

int OTA_WriteImage(uint32_t offset, void *pimg, uint32_t len);

int OTA_VerifyImage(void);

int OTA_RequestUpgrade(const OTA_UpgradeMode_t type);

int OTA_ConfirmImage(void);

int OTA_ImageIsPermanent(void);


#if (CONFIG_OTA_DEBUG == 1)

void ota_get_state(void);

#else

#define ota_get_state()

#endif



#ifdef __cplusplus
}
#endif

#endif /* (CONFIG_OTA == 1) */

#endif /* _OTA_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
