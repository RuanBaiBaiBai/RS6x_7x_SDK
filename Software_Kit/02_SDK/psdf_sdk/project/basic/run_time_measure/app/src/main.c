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
#define SAMPLE_DELAY                            500

#define SAMPLE_LOG_IO_TAG                       0x00040000
#define SAMPLE_LOG_IO                           0x3

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
    LOG_PRINT("\n=========================================\n");
    LOG_PRINT("\tRun Time Measure\n");
    LOG_PRINT("=========================================\n\n");
    LOG_PRINT("There are two types of measurement methods\n");
    LOG_PRINT("Method 1:\n");
    LOG_PRINT("Test using internal timer\n");
    LOG_PRINT("\nMethod 2:\n");
    LOG_PRINT("Program controls GPIO level, external instruments test GPIO level change time\n\n");
    LOG_PRINT("-----------------------------------------\n");

    LOG_PRINT("Method 1:\n");
    LOG_PRINT("Record the current running time using 'HAL_BOARD_GetTime' and subtract to obtain the incremental running time.\n");
    LOG_PRINT("matters needing attention:\n");
    LOG_PRINT("1. Time with different accuracies can be obtained\n");
    LOG_PRINT("2. The time storage bit width is 32-bit. Pay attention to overflow\n");
    LOG_PRINT("3. The timing hardware clock source is the AHB clock, and the cycle unit timing can be used\n");
    LOG_PRINT("   in conjunction with obtaining the AHB clock frequency through HAL_BOARD_GetFreq.\n");
    LOG_PRINT("4. Please refer to the board configuration for the clock accuracy of AHB, which can be obtained from MSI, DCXO, and PLL\n");
    LOG_PRINT("-----------------------------------------\n");

    LOG_PRINT("Measurement method:\n");

    LOG_PRINT("Example 1\n");
    uint32_t timeA = 0;
    uint32_t timeB = 0;
    timeA = HAL_BOARD_GetTime(HAL_TIME_US);
    OSI_MSleep(SAMPLE_DELAY);
    timeB = HAL_BOARD_GetTime(HAL_TIME_US);
    LOG_PRINT("Calculate the usage time from A(%u us) to B(%u us) as %u us\n", timeA, timeB, timeB - timeA);

    LOG_PRINT("\nExample 2\n");
    timeA = HAL_BOARD_GetTime(HAL_TIME_CYCLE);
    OSI_MSleep(SAMPLE_DELAY);
    timeB = HAL_BOARD_GetTime(HAL_TIME_CYCLE);
    uint32_t timeFreq = HAL_BOARD_GetFreq(CLOCK_AHB);
    LOG_PRINT("Calculate the usage time from A(%u us) to B(%u us) as %u cycle, time freq %u\n",
            timeA, timeB, timeB - timeA, timeFreq);


    LOG_PRINT("\nMethod 2:\n");
    LOG_PRINT("matters needing attention:\n");
    LOG_PRINT("1. Define CONFIG_LOG_IO_TAG_MASK and CONFIG_LOG_IO_MASK in prj_comfig.h\n");
    LOG_PRINT("2. CONFIG_LOG_IO_TAG_MASK selects values that do not overlap");
    LOG_PRINT("   with the system or other applications to avoid interference\n");
    LOG_PRINT("3. CONFIG_LOG_IO_MASK Select an idle gpio that can be pulled up or down\n");
    LOG_PRINT("-----------------------------------------\n");

    LOG_PRINT("Measurement method:\n");
    LOG_IO(SAMPLE_LOG_IO_TAG, 1, 1, 0);
    OSI_MSleep(500);
    LOG_IO(SAMPLE_LOG_IO_TAG, 1, 0, 0);
    LOG_PRINT("Raise gpio x at point A\n");
    LOG_PRINT("Pull down gpio x at point B\n");
    LOG_PRINT("Using logic analysis instruments to measure GPIO high level time\n");

    LOG_PRINT("\nNote: \n");
    LOG_PRINT("There is no restriction on raising gpio at point A. For example, 3 pulses can also be pulled and recognized.\n");

    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
