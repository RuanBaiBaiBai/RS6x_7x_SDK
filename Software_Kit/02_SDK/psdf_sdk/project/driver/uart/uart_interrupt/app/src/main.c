/**
 ******************************************************************************
 * @file    main.c
 * @brief   uart interrupt test define.
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
#include "hal_uart.h"

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
#define TEST_UART_ID                            UART1_ID
#define TEST_UART_BAUDRATE                      115200
#define TEST_UART_RX_TIMEOUT                    600000
#define TEST_UART_TX_TIMEOUT                    50

/* Private variables.
 * ----------------------------------------------------------------------------
 */

#if CONFIG_UART_WUP_ENABLED
static struct wakelock uart_pm_lock;
static OSI_Timer_t uart_timer;
#endif

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

#if CONFIG_UART_WUP_ENABLED
static void uart_wuk_callback(HAL_Dev_t * pDevice, void *arg)
{
    pm_policy_wake_lock(&uart_pm_lock);
    OSI_TimerStart(&uart_timer);
}

static void uart_timeout(void *arg)
{
    pm_policy_wake_unlock(&uart_pm_lock);
}
#endif

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int main(void)
{
    HAL_Status_t status = HAL_STATUS_OK;
    int ret = 0;

    do {
        UART_InitParam_t uartParam = {
            .baudRate       = TEST_UART_BAUDRATE,
            .parity         = UART_PARITY_NONE,
            .stopBits       = UART_STOP_BIT_1,
            .dataBits       = UART_DATA_WIDTH_8,
            .autoFlowCtrl   = UART_FLOW_CTRL_NONE
        };

        HAL_Dev_t *pUartDev = HAL_UART_Init(TEST_UART_ID, &uartParam);
        if (pUartDev == NULL) {
            status = HAL_STATUS_ERROR;
            LOG_PRINT("uart init fail\n");
            break;
        }

        status = HAL_UART_Open(pUartDev);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("uart open fail\n");
            break;
        }

        status = HAL_UART_SetTransferMode(pUartDev, UART_DIR_TX, UART_TRANS_MODE_INTERRUPT);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("set transfer mode fail\n");
            break;
        }
        status = HAL_UART_SetTransferMode(pUartDev, UART_DIR_RX, UART_TRANS_MODE_INTERRUPT);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("set transfer mode fail\n");
            break;
        }

#if CONFIG_UART_WUP_ENABLED
        {
            OSI_Status_t status;
            OSI_TimerSetInvalid(&uart_timer);
            status = OSI_TimerCreate(&uart_timer, OSI_TIMER_ONCE, uart_timeout, NULL, 50);
            if (status != OSI_STATUS_OK) {
                return status;
            }
            
            uint8_t uart_wup_enable = 1;
            HAL_UART_ExtControl(pUartDev, UART_PARAM_WAKEUP_CTRL, (void *)&uart_wup_enable);
            HAL_Callback_t uart_wuk_cb = { .cb = uart_wuk_callback, .arg = NULL };
            HAL_UART_ExtControl(pUartDev, UART_PARAM_WAKEUP_CALLBACK, (void *)&uart_wuk_cb);
        }
#endif

        uint8_t testBuff[8];
        while (1) {
            ret = HAL_UART_Receive(pUartDev, testBuff, 1, TEST_UART_RX_TIMEOUT);
            if (ret == 1) {
#if CONFIG_UART_WUP_ENABLED
                OSI_TimerRestart(&uart_timer);
#endif
                HAL_UART_Transmit(pUartDev, testBuff, 1, TEST_UART_TX_TIMEOUT);
                if (testBuff[0] == '\n') {
                    break;
                }
            } else {
                LOG_PRINT("receive timeout\n");
                status = HAL_STATUS_ERROR;
                break;
            }
        }

    } while (0);

    if (status == HAL_STATUS_OK) {
        LOG_PRINT("sample run success\n");
    } else {
        LOG_PRINT("sample run fail\n");
    }

#if CONFIG_UART_WUP_ENABLED
    OSI_TimerDelete(&uart_timer);
#endif

    return 0;
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

