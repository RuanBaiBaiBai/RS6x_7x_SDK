/**
 ******************************************************************************
 * @file    hif_log.c
 * @brief   hif_log define.
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
#include "hif.h"
#include "hif_config.h"
#include "hif_log.h"


/* Private typedef.
 * ----------------------------------------------------------------------------
 */
#if (CONFIG_HIF_LOG_IO_MEM != 0)
typedef struct {
    uint16_t *pPoint;
    uint16_t size;
    uint16_t write;
    uint16_t read;
    uint8_t state;
} HIF_LOG_Record_t;
#endif


#if (CONFIG_HIF_LOG_TIME != 0)

typedef struct {
    uint32_t    *pTime;
    uint32_t    lastTime;
    uint8_t     nodeMax;
    uint8_t     nodeIdx;
    uint8_t     state;
} HIF_LOG_Time_t;

#endif


/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private macros.
 * ----------------------------------------------------------------------------
 */

/* Private variables.
 * ----------------------------------------------------------------------------
 */
#if (CONFIG_HIF_LOG_IO_MEM != 0)

static HIF_LOG_Record_t hifLogRecord;

#endif

#if (CONFIG_HIF_LOG_TIME != 0)

static HIF_LOG_Time_t hifLogTime;

#endif

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
#if (CONFIG_HIF_LOG_IO_MEM != 0)

int hif_LOG_Init(uint16_t maxPoint)
{
    hifLogRecord.pPoint = OSI_Malloc(2*maxPoint);
    if (hifLogRecord.pPoint == NULL) {
        return HIF_ERRCODE_NO_BUFFER;
    }

    hifLogRecord.size = maxPoint;
    hifLogRecord.write = 0;

    return HIF_ERRCODE_SUCCESS;
}


void hif_LOG_Deinit(void)
{
    OSI_Free(hifLogRecord.pPoint);
    hifLogRecord.pPoint = NULL;
}


void hif_LOG_Ctrl(uint8_t num, uint8_t val, uint8_t flip)
{
    if (hifLogRecord.state == 0) {
        return ;
    }

    hifLogRecord.pPoint[hifLogRecord.write++] = (flip << 8) | ((val & 0xF) << 4) | (num & 0xF);
    if (hifLogRecord.write >= hifLogRecord.size) {
        hifLogRecord.write = 0;
    }
}


void hif_LOG_Enable(void)
{
    if (hifLogRecord.pPoint != NULL) {
        hifLogRecord.state = 1;
    }
}


void hif_LOG_Disable(void)
{
    hifLogRecord.state = 0;
    hifLogRecord.read = hifLogRecord.write;
}


uint16_t hif_LOG_Dump(void)
{
    if (hifLogRecord.state != 0) {
        hifLogRecord.state = 0;
        hifLogRecord.read = hifLogRecord.write;
    }

    if (hifLogRecord.read == 0) {
        hifLogRecord.read = hifLogRecord.size;
    } else {
        hifLogRecord.read--;
    }

    if (hifLogRecord.read != hifLogRecord.size) {
        return hifLogRecord.pPoint[hifLogRecord.read];
    } else {
        return 0;
    }
}

#endif /* CONFIG_HIF */


#if (CONFIG_HIF_LOG_TIME != 0)

void hif_LOG_TimeInit(void)
{
    hifLogTime.nodeIdx = 0;
    hifLogTime.state = 0;

    hifLogTime.nodeMax = CONFIG_HIF_LOG_TIME_SIZE;
    hifLogTime.pTime = OSI_Malloc(sizeof(uint32_t) * hifLogTime.nodeMax);
    if (hifLogTime.pTime != NULL) {
        return;
    }

    hifLogTime.state = 0x10;
}


void hif_LOG_TimeDeinit(void)
{
    OSI_Free(hifLogTime.pTime);
    hifLogTime.pTime = NULL;
}


void hif_LOG_TimeEnable(void)
{
    if (hifLogTime.pTime != NULL) {
        hifLogTime.nodeIdx = 0;
        hifLogTime.state = 0x11;
        hifLogTime.lastTime = csi_coret_get_value();
    }
}


void hif_LOG_TimeDisable(void)
{
    hifLogTime.state = 0x10;
}


uint32_t hif_LOG_TimeDump(uint8_t idx)
{
    //printf("dump\n");
    if (hifLogTime.state != 0x10) {
        //printf("state\n");
        return 0;
    }

    if ((hifLogTime.pTime != NULL) && (idx < hifLogTime.nodeIdx)) {
        //printf("ret %d %d %p\n", hifLogTime.nodeIdx, idx, hifLogTime.pTime);
        return hifLogTime.pTime[idx];
    } else {
        return 0;
    }
}


void hif_LOG_TimeRecord(void)
{
    if (hifLogTime.state == 0x11) {
        if ((hifLogTime.nodeIdx < hifLogTime.nodeMax) && (hifLogTime.pTime != NULL)) {
            uint32_t currentTime = csi_coret_get_value();
            hifLogTime.pTime[hifLogTime.nodeIdx++] = currentTime - hifLogTime.lastTime;
            hifLogTime.lastTime = currentTime;
        }
    }
}

#endif

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
