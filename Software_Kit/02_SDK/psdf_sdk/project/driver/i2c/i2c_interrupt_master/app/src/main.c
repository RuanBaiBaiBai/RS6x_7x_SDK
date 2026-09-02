/**
 ******************************************************************************
 * @file    main.c
 * @brief   i2c master intrrupt test.
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

#include "hal_i2c.h"

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
static HAL_Dev_t *pi2cDev = NULL;

I2C_InitParam_t i2cInitParam ={
    .mode      =  I2C_MODE_MASTER,
    .speed     =  I2C_SPEED_FAST_PLUS,
    .busErrCb  =  {
        .arg   =  NULL,
        .cb    =  NULL,
    },
};

uint8_t txBuff[30]  =  { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
                        0x0C, 0x0D, 0x0E, 0x0F, 0x0E, 0x0D,
                        0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07,
                        0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

uint8_t rxBuff[30];

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int main(void)
{
    uint32_t     ret         =   0U;
    HAL_Status_t status      =   HAL_STATUS_OK;
    int32_t      rx_num      =   0U;
    int32_t      tx_num      =   0U;


    LOG_PRINT("              I2C Master Intrrupt Test\n");
    LOG_PRINT("-------------------------------------------\n\n");

    pi2cDev = HAL_I2C_Init(I2C0_ID, &i2cInitParam);
    if (!pi2cDev) {
        LOG_PRINT("i2c master init fail\n");
        ret |= HAL_BIT(0);
    }

    status = HAL_I2C_Open(pi2cDev, I2C_ADDR_WIDTH_7BIT, 0x3A);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c open (0x%02X) fail (%d)\n", 0x3A, status);
        ret |= HAL_BIT(1);
    }

    status = HAL_I2C_SetTransferMode(pi2cDev, I2C_TRANSFDIR_RX, I2C_TRANSFMODE_INTERRUPT);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c set transfer mode fail %d (%d-%d)\n",
                status, I2C_TRANSFDIR_RX, I2C_TRANSFMODE_INTERRUPT);
        ret |= HAL_BIT(2);
    }

    status = HAL_I2C_SetTransferMode(pi2cDev, I2C_TRANSFDIR_TX, I2C_TRANSFMODE_INTERRUPT);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c set transfer mode fail %d (%d-%d)\n",
                status, I2C_TRANSFDIR_TX, I2C_TRANSFMODE_INTERRUPT);
        ret |= HAL_BIT(3);
    }

    tx_num = HAL_I2C_Transmit(pi2cDev, txBuff, sizeof(txBuff), 5000);
    if (tx_num <= 0) {
        LOG_PRINT("i2c Transmit Fail (%d)\n", tx_num);
        ret |= HAL_BIT(4);
    }

    rx_num  =  HAL_I2C_Receive(pi2cDev, rxBuff, sizeof(rxBuff), 5000);
    if (rx_num <= 0) {
        LOG_PRINT("i2c Receive Fail (%d)\n", rx_num);
        ret |= HAL_BIT(5);
    }

    if (OSI_Memcmp(txBuff, rxBuff, sizeof(rxBuff)) != 0) {
        LOG_PRINT("Cmp buff fail\n");
        ret |= HAL_BIT(6);
    }

    status = HAL_I2C_Close(pi2cDev);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c slave close fail (%d)\n", status);
        ret |= HAL_BIT(7);
    }

    status  =  HAL_I2C_DeInit(pi2cDev);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c slave deinit fail (%d)\n", status);
        ret |= HAL_BIT(8);
    }

    if (ret != 0) {
        LOG_PRINT("sample run fail (%08X)\n", ret);
        return -1;
    }

    LOG_PRINT("sample run success\n");

    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
