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

#include "hal_flash.h"
#include "board_config.h"

#include "log.h"
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define TEST_FLASH_BUFF_SIZE                256
#define TEST_FLASH_CNT                      256

/* Private macros.
 * ----------------------------------------------------------------------------
 */
/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
uint32_t testBuff[TEST_FLASH_BUFF_SIZE];

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int main(void)
{
    LOG_PRINT("flash basic test\n");
    LOG_PRINT("-----------------------------\n");

    do {
        HAL_Status_t status = HAL_STATUS_OK;
        uint32_t flashSize = 0;
        uint32_t blockSize = 0;
        uint32_t testAddr = 0;
        uint32_t testSeq = 0;

        HAL_Dev_t *pFlashDev = HAL_DEV_Find(HAL_DEV_TYPE_FLASH, 0);
        if (pFlashDev == NULL) {
            LOG_PRINT("flash not init\n");
            break;
        }

        status = HAL_FLASH_ExtControl(pFlashDev, FLASH_GET_FLASH_SIZE, &flashSize, 4);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("get flash size fail (%d)\n", status);
            break;
        }

        status = HAL_FLASH_ExtControl(pFlashDev, FLASH_GET_BLOCK_SIZE, &blockSize, 4);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("get block size fail (%d)\n", status);
            break;
        }

        LOG_PRINT("flash size: 0x%08X (%dKB)\n", flashSize, flashSize / 1024);
        LOG_PRINT("flash block size 0x%08X (%dKB)\n", blockSize, blockSize / 1024);

        testAddr = flashSize - 2 * blockSize;
        status = HAL_FLASH_Read(pFlashDev, testAddr, (uint8_t *)testBuff, 4);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("flash read fail (%d)\n", status);
            break;
        }
        LOG_PRINT("read success\n");

        testSeq = testBuff[0] + TEST_FLASH_BUFF_SIZE;
        LOG_PRINT("base seq %08X\n", testSeq);
        status = HAL_FLASH_Erase(pFlashDev, testAddr, TEST_FLASH_BUFF_SIZE * 4);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("flash erase fail (%d)\n", status);
            break;
        }
        LOG_PRINT("erase success\n");

        for (uint32_t idx = 0; idx < TEST_FLASH_BUFF_SIZE; idx++) {
            testBuff[idx] = idx + testSeq;
        }

        status = HAL_FLASH_Write(pFlashDev, testAddr, (uint8_t *)testBuff, TEST_FLASH_BUFF_SIZE * 4);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("flash write fail (%d)\n", status);
            break;
        }
        LOG_PRINT("write success\n");
        for (uint32_t idx = 0; idx < TEST_FLASH_BUFF_SIZE; idx++) {
            testBuff[idx] = 0;
        }

        status = HAL_FLASH_Read(pFlashDev, testAddr, (uint8_t *)testBuff, TEST_FLASH_BUFF_SIZE * 4);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("flash read fail (%d)\n", status);
            break;
        }
        LOG_PRINT("read success\n");
        for (uint32_t idx = 0; idx < TEST_FLASH_BUFF_SIZE; idx++) {
            LOG_PRINT("%08X ", testBuff[idx]);
            if ((idx % 16) == 15) {
                LOG_PRINT("\n");
            }
        }

        LOG_PRINT("\ntest done\n");
    } while (0);


    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
