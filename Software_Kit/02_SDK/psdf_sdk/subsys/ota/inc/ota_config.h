/**
 ******************************************************************************
 * @file    ota_config.h
 * @brief   ota config define.
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


#ifndef _OTA_CFG_H_
#define _OTA_CFG_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types.
 * ----------------------------------------------------------------------------
 */

/* Exported constants.
 * ----------------------------------------------------------------------------
 */
#ifndef CONFIG_OTA
#define CONFIG_OTA                              0
#endif

#ifndef CONFIG_OTA_LOG_LEVEL
#define CONFIG_OTA_LOG_LEVEL                    0
#endif


/* Exported macro.
 * ----------------------------------------------------------------------------
 */

#if (CONFIG_OTA_LOG_LEVEL == 0)
#define CFG_OTA_LOG_EN                          0
#else
#define CFG_OTA_LOG_EN                          1
#endif

/*
 * ERR : 1
 * WRN : 2
 * INF : 3
 * DBG : 4
 */
#define CFG_OTA_LOG_LEVEL                       CONFIG_OTA_LOG_LEVEL

#define CFG_OTA_ALIGN_SIZE                      4
#define CFG_OTA_ALIGN_MSK                       3

#define CFG_OTA_IMG_NAME_SIZE                   16
#define CFG_OTA_REMAP_ADDR_CNT                  16
#define CFG_OTA_IMG_MOVE_CNT                    4
#define CFG_OTA_SUB_IMG_MAX_CNT                 4

#define CFG_OTA_TLV_HDR_SIZE                    4
#define CFG_OTA_HASH_SIZE                       32
#define CFG_OTA_HASH_BUF_SIZE                   256

#define CFG_OTA_MOVE_BUF_SIZE                   256

#define CFG_OTA_PORT_STORE_CACHE_EN             1
#define CFG_OTA_PORT_STORE_BASE_ADDR            0x08000000

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


#ifdef __cplusplus
}
#endif

#endif /* _OTA_CFG_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
