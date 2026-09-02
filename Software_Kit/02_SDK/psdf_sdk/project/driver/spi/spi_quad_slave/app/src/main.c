/**
 ******************************************************************************
 * @file    main.c
 * @brief   spi quad test define.
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
#include "hal_spi.h"
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
#define TEST_SPI_FREQUENCE                      400000
#define TEST_SPI_RX_TIMEOUT                     3000
#define TEST_SPI_TX_TIMEOUT                     3000

#define TEST_SPI_DATA_PRINT                     1

/* Private variables.
 * ----------------------------------------------------------------------------
 */
DEFINE_PINMUX(spi0, 6,
    { GPIO_PORT_A, GPIO_PIN_0,  {GPIOA_P0_F2_SPI_CS,   GPIO_DRIVING_LEVEL_3, GPIO_PUSH_PULL, GPIO_PULL_UP } },
    { GPIO_PORT_A, GPIO_PIN_1,  {GPIOA_P1_F2_SPI_CLK,   GPIO_DRIVING_LEVEL_2, GPIO_PUSH_PULL, GPIO_PULL_UP } },
    { GPIO_PORT_A, GPIO_PIN_2,  {GPIOA_P2_F2_SPI_MOSI,   GPIO_DRIVING_LEVEL_2, GPIO_PUSH_PULL, GPIO_PULL_UP } },
    { GPIO_PORT_A, GPIO_PIN_3,  {GPIOA_P3_F2_SPI_MISO,   GPIO_DRIVING_LEVEL_2, GPIO_PUSH_PULL, GPIO_PULL_UP } },
    { GPIO_PORT_A, GPIO_PIN_4,  {GPIOA_P4_F2_SPI_HOLD,   GPIO_DRIVING_LEVEL_2, GPIO_PUSH_PULL, GPIO_PULL_UP } },
    { GPIO_PORT_A, GPIO_PIN_5,  {GPIOA_P5_F2_SPI_WP,   GPIO_DRIVING_LEVEL_2, GPIO_PUSH_PULL, GPIO_PULL_UP } },
);
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


int main(void)
{
    HAL_Status_t status = HAL_STATUS_OK;
    int ret = 0;
    LOG_PRINT("spi quad slave test\n");

    SPI_InitParam_t spiParam = {
        .mode = SPI_MODE_SLAVE,
        .sclkMode = SPI_SCLK_MODE0,
        .ioMode = SPI_IO_MODE_QUAD,
        .csMode = SPI_CS_HARD,
        .dataWidth = SPI_DATAWIDTH_8BIT,
        .sclk = TEST_SPI_FREQUENCE,
    };

    HAL_Dev_t *pSpiDev = HAL_SPI_Init(SPI0_ID, &spiParam, 1);
    if (pSpiDev == NULL) {
        status = HAL_STATUS_ERROR;
        LOG_PRINT("spi init fail\n");
    }

    status = HAL_SPI_Open(pSpiDev, 0);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("spi open fail\n");
    }

    status = HAL_SPI_SetTransferMode(pSpiDev, SPI_DIR_RX, SPI_TRANS_MODE_NOMA);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("set transfer mode fail\n");
    }
    status = HAL_SPI_SetTransferMode(pSpiDev, SPI_DIR_TX, SPI_TRANS_MODE_NOMA);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("set transfer mode fail\n");
    }

    uint8_t testBuff[256];
    uint8_t rxbuf[256];
    uint16_t i;
    for (i = 0; i < sizeof(testBuff); i++) {
        testBuff[i] = i;
    }
    LOG_HEX(testBuff, sizeof(testBuff), "testBuff:");

    while (1) {
        ret = HAL_SPI_Receive(pSpiDev, rxbuf, sizeof(rxbuf), TEST_SPI_RX_TIMEOUT);
        if (ret > 0) {
            if (!memcmp(testBuff, rxbuf, sizeof(testBuff))) {
                LOG_PRINT("data correct\n");
                status = HAL_STATUS_OK;
            }
#if TEST_SPI_DATA_PRINT
        LOG_HEX(rxbuf, sizeof(rxbuf), "receive:");
#endif
        } else {
            LOG_PRINT("receive timeout\n");
            status = HAL_STATUS_ERROR;
            break;
        }
        HAL_SPI_Transmit(pSpiDev, (uint8_t *)testBuff, sizeof(testBuff), TEST_SPI_TX_TIMEOUT);
        break;
    }

    if (status == HAL_STATUS_OK) {
        LOG_PRINT("spi quad slave test run success\n");
    } else {
        LOG_PRINT("spi quad slave test run fail\n");
    }

    return 0;
}


