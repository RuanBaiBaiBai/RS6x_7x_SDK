/**
 ******************************************************************************
 * @file    hif_log.h
 * @brief   hif log define.
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

#ifndef _HIF_LOG_H
#define _HIF_LOG_H

/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "hif_msg.h"
#include "hif_config.h"

#ifndef LOG_MODULE
#define LOG_MODULE                      "HIF"
#endif
#ifndef LOG_LEVEL
#define LOG_LEVEL                       CONFIG_HIF_LOG_LEVEL
#endif
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ----------------------------------------------------------------------------
 */
#ifndef HIF_LOG_TAG
#define HIF_LOG_TAG                                 CONFIG_HIF_LOG_IO_TAG
#endif

#ifndef HIF_LOG_IO_MEM
#define HIF_LOG_IO_MEM                              CONFIG_HIF_LOG_IO_MEM
#endif



#define HIF_LOGIO_NUM_0                             4
#define HIF_LOGIO_NUM_1                             5
#define HIF_LOGIO_NUM_2                             11


#define HIF_LOGIO_CMDRSP                            0
#define HIF_LOGIO_REPORT                            0
#define HIF_LOGIO_SEND_CB                           0
#define HIF_LOGIO_RECV_CB                           0
#define HIF_LOGIO_PM                                0
#define HIF_LOGIO_TRANS                             0

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

/* Exported macro.
 * ----------------------------------------------------------------------------
 */

#define HIF_LOG_ERR(fmt, arg...)                    LOG_ERR(fmt, ##arg)
#define HIF_LOG_ERR_HEX(data, len, fmt, arg...)     LOG_ERR_HEX(data, len, fmt, ##arg)
#define HIF_LOG_WRN(fmt, arg...)                    LOG_WRN(fmt, ##arg)
#define HIF_LOG_WRN_HEX(data, len, fmt, arg...)     LOG_WRN_HEX(data, len, fmt, ##arg)
#define HIF_LOG_INF(fmt, arg...)                    LOG_INF(fmt, ##arg)
#define HIF_LOG_INF_HEX(data, len, fmt, arg...)     LOG_INF_HEX(data, len, fmt, ##arg)
#define HIF_LOG_DBG(fmt, arg...)                    LOG_DBG(fmt, ##arg)
#define HIF_LOG_DBG_HEX(data, len, fmt, arg...)     LOG_DBG_HEX(data, len, fmt, ##arg)


#if (HIF_LOG_IO_MEM != 0)

int hif_LOG_Init(uint16_t maxPoint);
void hif_LOG_Deinit(void);
void hif_LOG_Ctrl(uint8_t num, uint8_t val, uint8_t flip);
void hif_LOG_Enable(void);
void hif_LOG_Disable(void);
uint16_t hif_LOG_Dump(void);

#else

#define hif_LOG_Init(maxPoint)              0
#define hif_LOG_Deinit()
#define hif_LOG_Ctrl(num, val, flip)
#define hif_LOG_Enable()
#define hif_LOG_Disable()
#define hif_LOG_Dump()                      0

#endif


#if (CONFIG_HIF_LOG_TIME != 0)

void hif_LOG_TimeInit(void);
void hif_LOG_TimeDeinit(void);
void hif_LOG_TimeEnable(void);
void hif_LOG_TimeDisable(void);
uint32_t hif_LOG_TimeDump(uint8_t idx);
void hif_LOG_TimeRecord(void);

#else

#define hif_LOG_TimeInit()
#define hif_LOG_TimeDeinit()
#define hif_LOG_TimeEnable()
#define hif_LOG_TimeDisable()
#define hif_LOG_TimeDump(idx)       0
#define hif_LOG_TimeRecord()

#endif


#if (HIF_LOG_IO_MEM != 0)

#define HIF_LOG_IO(num, val, flip)                  hif_LOG_Ctrl(num, val, flip)

#else

#if (HIF_LOG_TAG != 0)
#define HIF_LOG_IO(num, val, flip)                  LOG_IO(HIF_LOG_TAG, num, val, flip)
#else
#define HIF_LOG_IO(num, val, flip)
#endif

#endif


/* Exported functions.
 * ----------------------------------------------------------------------------
 */

#ifdef __cplusplus
}
#endif

#endif /* _HIF_LOG_H */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

