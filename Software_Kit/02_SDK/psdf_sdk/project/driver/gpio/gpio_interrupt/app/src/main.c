/**
 ******************************************************************************
 * @file    main.c
 * @brief   gpio_irq.
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
#include "hal_gpio.h"

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
#define TEST_GPIO_PORT                  GPIO_PORT_A
#define TEST_GPIO_PIN                   GPIO_PIN_6
#define TEST_GPIO_EXTI_Callback         HAL_GPIO_EXTI6_Callback
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
    .pin_s = TEST_GPIO_PIN
};

void TEST_GPIO_EXTI_Callback(void *arg)
{
    test_t *ptr = (test_t *)arg;
    (void)ptr;
    /* do */
    LOG_PRINT("port=%c,pin=%d\n", ptr->port_s, ptr->pin_s);
}


int main(void)
{
    HAL_Dev_t *gpioDev;
    HAL_Status_t status = HAL_STATUS_OK;
    GPIO_PinParam_t gpioParam;
    GPIO_IrqParam_t gpioIrqParam;


    LOG_PRINT("              GPIO-Irq Test\n");
    LOG_PRINT("-------------------------------------------\n\n");


    gpioDev = HAL_GPIO_Init(TEST_GPIO_PORT);
    if (gpioDev == NULL) {
        LOG_PRINT("Init gpio fail\n");
        return HAL_STATUS_INVALID_PARAM;
    }

    gpioParam.mode = GPIOx_Pn_F14_EINT;
    gpioParam.pull = GPIO_PULL_DOWN;
    gpioParam.driving = GPIO_DRIVING_LEVEL_1;

    status = HAL_GPIO_SetPinParam(gpioDev, TEST_GPIO_PIN, &gpioParam);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set config gpio fail %d\n", status);
        return status;
    }

    gpioIrqParam.event    = GPIO_IRQ_EVT_RISING_EDGE;
    gpioIrqParam.callback = TEST_GPIO_EXTI_Callback;  // TEST_GPIO_EXTI_Callback OR NULL
    gpioIrqParam.arg      = &gpio_test;

    status = HAL_GPIO_SetIRQParam(gpioDev, TEST_GPIO_PIN, &gpioIrqParam);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Init gpio irq fail %d\n", status);
        return status;
    }

    status = HAL_GPIO_EnableIRQ(gpioDev, TEST_GPIO_PIN);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Enable gpio irq fail %d\n", status);
        return status;
    }

    LOG_PRINT("Test done\n");

    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
