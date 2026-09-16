/**
 ******************************************************************************
 * @file    main.c
 * @brief   i2c PM wakeup sample.
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
 * ------------------------------------------------------------------------------------------------
 */
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

/* Constants.
 * ------------------------------------------------------------------------------------------------
 */

 /* Private variables.
 * ----------------------------------------------------------------------------
 */
static HAL_Dev_t *pi2cDev = NULL;

I2C_InitParam_t i2cInitParam ={
    .mode      =  I2C_MODE_SLAVE,
    .speed     =  I2C_SPEED_FAST_PLUS,
    .busErrCb  =  {
        .arg   =  NULL,
        .cb    =  NULL,
    },
};

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static void i2c_wakeup_cb(HAL_Dev_t *i2c, void  *param)
{
    LOG_PRINT("\ti2c Resume CB\n");
}


int main(void)
{
    uint32_t     ret         =   0U;
    HAL_Status_t status      =   HAL_STATUS_OK;

    LOG_PRINT("              I2C Slave PM Test\n");
    LOG_PRINT("-------------------------------------------\n\n");

    pi2cDev = HAL_I2C_Init(I2C0_ID, &i2cInitParam);
    if (!pi2cDev) {
        LOG_PRINT("i2c slave init fail\n");
        ret |= HAL_BIT(0);
    }

    status = HAL_I2C_Open(pi2cDev, I2C_ADDR_WIDTH_7BIT, 0x3A);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c open (0x%02X) fail (%d)\n", 0x3A, status);
        ret |= HAL_BIT(1);
    }

    bool able  = true;
    status = HAL_I2C_ExtendControl(pi2cDev, I2C_EXTATTR_WAKEUP_CTRL_ABLE, (void *)&able);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c enable function fail (%d)\n", status);
        ret |= HAL_BIT(2);
    }

    HAL_Callback_t wakeupCb;

    wakeupCb.cb   =   i2c_wakeup_cb;
    wakeupCb.arg  =   NULL;

    status = HAL_I2C_ExtendControl(pi2cDev, I2C_EXTATTR_WAKEUP_CALLBACK, (void *)&wakeupCb);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c set wakeup callback fail (%d)\n", status);
        ret |= HAL_BIT(3);
    }

    uint32_t wakeAddr  =  0x3A;
    status = HAL_I2C_ExtendControl(pi2cDev, I2C_EXTATTR_WAKEUP_THRESHOLD, (void *)&wakeAddr);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("i2c config wakeup addr fail (%d)\n", status);
        ret |= HAL_BIT(4);
    }

    return 0;
}



