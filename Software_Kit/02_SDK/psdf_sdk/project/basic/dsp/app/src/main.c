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
#include "hal_board.h"

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
#define LOOP_TIMES         (100)

/* Private variables.
 * ----------------------------------------------------------------------------
 */

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
extern int test_abs_f32(void);
extern int test_fft_f32(void);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int main(void)
{
    LOG_PRINT("dsp lib test\n");

    int status = 0;
    uint32_t startTime, stopTime;
    uint8_t cpuid = csi_get_cpu_id();
    uint32_t freq = HAL_BOARD_GetFreq(CLOCK_AHB);
    uint32_t idx = 0;

    LOG_PRINT("current cpu: %s\n", (cpuid == 0) ? "CPUS" : "CPUF");
    LOG_PRINT("timer freq: %u\n", freq);


    LOG_PRINT("\n-------------------------------------------\n");
    startTime = HAL_BOARD_GetTime(HAL_TIME_CYCLE);
    for (idx = 0; idx < LOOP_TIMES; idx++) {
        status = test_abs_f32();
        if (status != 0) {
            break;
        }
    }
    stopTime = HAL_BOARD_GetTime(HAL_TIME_CYCLE);

    LOG_PRINT("abs test: %s\n", (status == 0) ? "success" : "fail");
    LOG_PRINT("run cnt: %d\n", idx);
    LOG_PRINT("run time: %u\n", stopTime - startTime);


    LOG_PRINT("\n-------------------------------------------\n");
    startTime = HAL_BOARD_GetTime(HAL_TIME_CYCLE);
    for (idx = 0; idx < LOOP_TIMES; idx++) {
        status = test_fft_f32();
        if (status != 0) {
            break;
        }
    }
    stopTime = HAL_BOARD_GetTime(HAL_TIME_CYCLE);

    LOG_PRINT("cfft test: %s\n", (status == 0) ? "success" : "fail");
    LOG_PRINT("run cnt: %u\n", idx);
    LOG_PRINT("run time: %u\n", stopTime - startTime);


    return 0;
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

