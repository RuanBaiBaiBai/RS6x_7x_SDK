/**
 ******************************************************************************
 * @file    osi_task.c
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
#include "board_config.h"
#include "log.h"
#include "hal_gpio.h"

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
#define TEST_GPIO_PIN                   GPIO_PIN_11
#define TEST_GPIO_PIN_VALUE             GPIO_PIN_SET

/* Private variables.
 * ----------------------------------------------------------------------------
 */
static TaskHandle_t    led_task_handle    = NULL;
HAL_Dev_t *gpioDev;

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
void led_task(void *param);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int main(void)
{
    HAL_Status_t status = HAL_STATUS_OK;
    GPIO_PinParam_t gpioParam;

    LOG_PRINT("Hello Word\n");
    LOG_PRINT("My name is possumic\n");
    LOG_PRINT("Nice to meet you\n\n");

    gpioDev = HAL_GPIO_Init(TEST_GPIO_PORT);
    if (gpioDev == NULL) {
        LOG_PRINT("Init gpio fail\n");
        return HAL_STATUS_INVALID_PARAM;
    }

    gpioParam.mode = GPIOx_Pn_F1_OUTPUT;
    gpioParam.driving = GPIO_DRIVING_LEVEL_1;
    gpioParam.push = GPIO_PUSH_PULL;
    gpioParam.pull = GPIO_PULL_FLOATING;

    status = HAL_GPIO_SetPinParam(gpioDev, TEST_GPIO_PIN, &gpioParam);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("Set config gpio fail %d\n", status);
        return status;
    }

    if (pdPASS != xTaskCreate(
        (TaskFunction_t)led_task,
        "led task",
        512,
        NULL,
        4,
        &led_task_handle)) {
        LOG_PRINT("led task create failed\n");
    } else {
        LOG_PRINT("led task create success\n");
    }

    while (1) {
        /* sleep 1s */
        OSI_Sleep(1);
    }

}

void led_task(void *param)
{
    uint8_t cnt = 0;

    while(1) {
    cnt++;
    HAL_GPIO_TogglePin(gpioDev, TEST_GPIO_PIN);
    LOG_PRINT("led toggle \n");

    if (cnt == 10) {
        LOG_PRINT("led task is delete success \n");
        cnt = 0;
        vTaskDelete(led_task_handle);
    }

    vTaskDelay(500);
    }
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
