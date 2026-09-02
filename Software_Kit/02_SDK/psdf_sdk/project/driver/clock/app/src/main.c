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
#include "hal_clock.h"

#include "log.h"
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
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
    uint32_t freq = 0;

    LOG_PRINT("clock driver demo\n");

    do {
        HAL_Dev_t *pClockDev = HAL_DEV_Find(HAL_DEV_TYPE_PWR_CLK, 0);
        if (pClockDev == NULL) {
            LOG_PRINT("clock power not init\n");
            break;
        }

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_LSI);
        LOG_PRINT("lsi : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_MSI);
        LOG_PRINT("msi : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_LSE);
        LOG_PRINT("lse : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_DCXO);
        LOG_PRINT("dcxo : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_LPCLK);
        LOG_PRINT("lpclk : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_LPCLK_INTE_DIV);
        LOG_PRINT("inte div : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_LPCLK_FRAC_DIV);
        LOG_PRINT("frac div : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_PLL);
        LOG_PRINT("pll : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_PLL_SOC);
        LOG_PRINT("pll soc : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_PLL_DEV);
        LOG_PRINT("pll dev : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_PLL_DEV1);
        LOG_PRINT("pll div1 : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_PLL_CDAA);
        LOG_PRINT("pll cdaa : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_SYS);
        LOG_PRINT("sys : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_AHB);
        LOG_PRINT("ahb : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_APB0);
        LOG_PRINT("apb0 : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_APB1);
        LOG_PRINT("apb1 : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_CURR_CPU);
        LOG_PRINT("current cpu : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_CPUS);
        LOG_PRINT("cpus : %u\n", freq);

        freq = HAL_CLOCK_GetFreq(pClockDev, CLOCK_CPUF);
        LOG_PRINT("cpuf : %u\n", freq);

    } while (0);


    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

