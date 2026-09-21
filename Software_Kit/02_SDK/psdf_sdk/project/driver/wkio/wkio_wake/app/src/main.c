/**
 ******************************************************************************
 * @file    main.c
 * @brief   wkio_wake.
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
#include "hal_wkio.h"

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
#define TEST_WKIO_PORT                  WKIO_PORT_A
#define TEST_WKIO_PIN                   WKIO_PIN_6
#define TEST_WKIO_MODE                  HAL_WKIO_MODE_WAKE

/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    char port_s;
    uint8_t pin_s;
} test_t;

test_t gpio_test = {
    .port_s = 'A',
    .pin_s = TEST_WKIO_PIN
};

static void TEST_WKIO_Callback(void *arg)
{
    test_t *ptr = (test_t *)arg;
    (void)ptr;
    /* do */
    LOG_PRINT("wkio wakeup, wkio: P%c%d\n", ptr->port_s, ptr->pin_s);
}

int main(void)
{
    HAL_Dev_t *wkioDev;
    HAL_Status_t status = HAL_STATUS_OK;
    WKIO_WakeParam_t wkioIrqParam;

    LOG_PRINT("              WKIO-Wake Test\n");
    LOG_PRINT("-------------------------------------------\n\n");

    wkioDev = HAL_WKIO_Init(TEST_WKIO_PORT);
    if (wkioDev == NULL) {
        LOG_PRINT("Init wkio fail\n");
        return HAL_STATUS_INVALID_PARAM;
    }

    status = HAL_WKIO_SetPinMode(wkioDev, TEST_WKIO_PIN, TEST_WKIO_MODE);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set wkio mode fail %d\n", status);
        return status;
    }

    wkioIrqParam.event = WKIO_WAKE_EVT_FALLING_EDGE;
    wkioIrqParam.pull = HAL_WKIO_PULL_DOWN;
    wkioIrqParam.callback = TEST_WKIO_Callback;
    wkioIrqParam.arg =  &gpio_test;

    status = HAL_WKIO_SetWakeParam(wkioDev, TEST_WKIO_PIN, &wkioIrqParam);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set wkio wake init fail %d\n", status);
        return status;
    }

    status = HAL_WKIO_EnableWake(wkioDev);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set wkio wake enable fail %d\n", status);
        return status;
    }

    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
