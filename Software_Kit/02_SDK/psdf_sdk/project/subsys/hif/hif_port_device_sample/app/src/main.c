/**
 ******************************************************************************
 * @file    main.c
 * @brief   main define.
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
#include "common.h"

#include "hif_config.h"
#include "hif.h"

#include "log.h"
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */

#define HIF_SAMP_REPORT_MSG_ID                          0xCF

/* Private macros.
 * ----------------------------------------------------------------------------
 */

#define HIF_SAMP_REPORT_DELAY                           1000

/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */


int main(void)
{
    int status = HAL_STATUS_OK;

    LOG_PRINT("-----------------------------------------------\n");
    LOG_PRINT("\tHIF Port Device Sample\n");
    LOG_PRINT("-----------------------------------------------\n");


    LOG_PRINT("\n\n1>. Init HIF\n");

    LOG_PRINT("\t1.1. Communication type: SPI.\n");
    LOG_PRINT("\t     Obtain default parameter configuration.\n");
    HIF_CfgParam_t hifCfgParam;
    HIF_DefaultInitParam(&hifCfgParam, HIF_COM_TYPE_SPI, 56000000);

    LOG_PRINT("\t1.2. Init\n");
    status = HIF_Init(&hifCfgParam);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("\t     FAIL (%d)\n", status);
        return status;
    }
    LOG_PRINT("\t     SUCCESS\n");


    uint8_t reportBuff[5];
    reportBuff[0] = 1;
    uint32_t *pReportTick = (uint32_t *)&reportBuff[1];
    *pReportTick = 0;
    LOG_PRINT("\t1.3. Tick\n");
    do {
        LOG_PRINT("\t%d\n", *pReportTick);
        HIF_MsgReport(HIF_SAMP_REPORT_MSG_ID, reportBuff, sizeof(reportBuff), NULL);
        pReportTick[0]++;
        OSI_MSleep(HIF_SAMP_REPORT_DELAY);

    } while (1);


    LOG_PRINT("\n\nSAMPLE Done\n");


    return 0;
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
