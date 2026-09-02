/**
 ******************************************************************************
 * @file    ota log.h
 * @brief   ota log define.
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

#ifndef _OTA_LOG_H_
#define _OTA_LOG_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "common.h"
#include "ota_config.h"

#if (CONFIG_OTA == 1)

#include "ota_types.h"

#if (CFG_OTA_LOG_EN == 1)

#define LOG_MODULE                      "OTA"

#ifndef LOG_LEVEL
#define LOG_LEVEL                       CONFIG_OTA_LOG_LEVEL
#endif

#include "log.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Exported types.
 * ----------------------------------------------------------------------------
 */

/* Exported macro.
 * ----------------------------------------------------------------------------
 */

#define OTA_LOG_ERR(fmt, arg...)                    LOG_ERR(fmt, ##arg)
#define OTA_LOG_ERR_HEX(data, len, fmt, arg...)     LOG_ERR_HEX(data, len, fmt, ##arg)
#define OTA_LOG_WRN(fmt, arg...)                    LOG_WRN(fmt, ##arg)
#define OTA_LOG_WRN_HEX(data, len, fmt, arg...)     LOG_WRN_HEX(data, len, fmt, ##arg)
#define OTA_LOG_INF(fmt, arg...)                    LOG_INF(fmt, ##arg)
#define OTA_LOG_INF_HEX(data, len, fmt, arg...)     LOG_INF_HEX(data, len, fmt, ##arg)
#define OTA_LOG_DBG(fmt, arg...)                    LOG_DBG(fmt, ##arg)
#define OTA_LOG_DBG_HEX(data, len, fmt, arg...)     LOG_DBG_HEX(data, len, fmt, ##arg)

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


#ifdef __cplusplus
}
#endif

#endif  /* CONFIG_OTA == 1 */

#endif /* _OTA_LOG_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
