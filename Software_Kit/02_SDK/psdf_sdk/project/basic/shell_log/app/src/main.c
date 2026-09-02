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


/*
 *
 */
#ifndef CONFIG_SAMPLE_LOG_LEVEL
#define CONFIG_SAMPLE_LOG_LEVEL          LEVEL_INF
#endif

#define LOG_MODULE                      "SAMPLE"
#define LOG_LEVEL                       CONFIG_SAMPLE_LOG_LEVEL

#include "log.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define SAMPLE_DATA_SIZE                    64

/* Private macros.
 * ----------------------------------------------------------------------------
 */

char * strLogLevel[] = {
    "LOG_LEVEL_DIS",
    "LEVEL_ERR",
    "LEVEL_WRN",
    "LEVEL_INF",
    "LEVEL_DBG",
};

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
    LOG_PRINT("\tLOG and shell sample\n");
    LOG_PRINT("=========================================\n\n");


    LOG_PRINT("\nUse the log\n");
    LOG_PRINT("1. Define the '#define CONFIG_PRINTF 1' enable LOG in prj_config.h\n");
    LOG_PRINT("2. Define the '#define CONFIG_PRINTF_UART_NUM 0' select UART 0 in prj_config.h\n");
    LOG_PRINT("3. #include \"conmmon.h\"\n");
    LOG_PRINT("4. #defiene LOG_MODULE\n");
    LOG_PRINT("5. #define LOG_LEVEL\n");
    LOG_PRINT("6. #include \"log.h\"\n");
    LOG_PRINT("-----------------------------------------\n");

    if (CONFIG_PM) {
        LOG_PRINT("\nUse the log pm\n");
        LOG_PRINT("1. Define the '#define CONFIG_PM 1' enable PM in prj_config.h\n");
        LOG_PRINT("2. Define the '#define CONFIG_SHELL_PM 1' enable shell PM in prj_config.h\n");
        LOG_PRINT("3. Define the '#define CONFIG_UART_WUP_ENABLED 1' enable uart wakeup in prj_config.h\n");
        LOG_PRINT("-----------------------------------------\n");
    }

    uint8_t logLevel = LOG_LEVEL;
    uint8_t sampleData[SAMPLE_DATA_SIZE];

    for (uint32_t idx = 0; idx < SAMPLE_DATA_SIZE; idx++) {
        sampleData[idx] = idx;
    }


    LOG_PRINT("Current 'LOG_LEVEL' is '%s'\n", strLogLevel[logLevel]);
    LOG_PRINT("Current 'LOG_MODULE' is '%s'\n\n", LOG_MODULE);

    LOG_PRINT("1> This is 'LOG_PRINT' print\n");
    LOG_HEX(sampleData, SAMPLE_DATA_SIZE, "2> This is 'LOG_HEX' print");
    LOG_DBG("3> This is 'LOG_DBG' print\n");
    LOG_DBG_HEX(sampleData, SAMPLE_DATA_SIZE, "2> This is 'LOG_DBG' print");
    LOG_INF("1> This is 'LOG_INF' print\n");
    LOG_INF_HEX(sampleData, SAMPLE_DATA_SIZE, "2> This is 'LOG_INF_HEX' print");
    LOG_WRN("1> This is 'LOG_WRN' print\n");
    LOG_WRN_HEX(sampleData, SAMPLE_DATA_SIZE, "2> This is 'LOG_WRN_HEX' print");
    LOG_ERR("1> This is 'LOG_ERR' print\n");
    LOG_ERR_HEX(sampleData, SAMPLE_DATA_SIZE, "2> This is 'LOG_ERR_HEX' print");


    LOG_PRINT("\nUse the shell\n");
    LOG_PRINT("1. Define the '#define CONFIG_SHELL 1' enable shell in prj_config.h\n");
    LOG_PRINT("2. Define the '#define CONFIG_SHELL_UART_NUM 1' select UART 1 in prj_config.h\n");
    LOG_PRINT("-----------------------------------------\n");


    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
