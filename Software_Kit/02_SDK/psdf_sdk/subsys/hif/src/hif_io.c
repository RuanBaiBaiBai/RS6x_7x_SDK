/**
 ******************************************************************************
 * @file    hif_io.c
 * @brief   hif_io define.
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
#include "hif.h"
#include "hif_config.h"
#include "hif_io.h"
#include "hal_dev.h"
#include "hal_wkio.h"
#include "hal_gpio.h"
#include "hif_pm.h"

#if (CONFIG_HIF == 1)

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
#if (CONFIG_HIF_PM)
HAL_Dev_t *pWkioDev;
#else
HAL_Dev_t *pGpioDev;
#endif
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int hif_IO_Init(uint32_t num)
{
    HAL_Status_t status = HAL_STATUS_OK;

#if (CONFIG_HIF_PM)
    uint8_t port = num / CONFIG_SOC_WKIOA_PIN_NUM;
    uint8_t pinIdx = num % CONFIG_SOC_WKIOA_PIN_NUM;

    pWkioDev = HAL_WKIO_Init(port);
    if (pWkioDev == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    status = HAL_WKIO_SetPinMode(pWkioDev, pinIdx, HAL_WKIO_MODE_HOLD);
    if (status != HAL_STATUS_OK) {
        return status;
    }
#else
    GPIO_PinParam_t gpioParam;

    uint8_t port = num / CONFIG_SOC_GPIOA_PIN_NUM;
    uint8_t pinIdx = num % CONFIG_SOC_GPIOA_PIN_NUM;


    pGpioDev = HAL_GPIO_Init(port);
    if (pGpioDev == NULL) {
        return HAL_STATUS_INVALID_PARAM;
    }

    gpioParam.mode = GPIOx_Pn_F1_OUTPUT;
    gpioParam.driving = GPIO_DRIVING_LEVEL_1;
    gpioParam.push = GPIO_PUSH_PULL;
    gpioParam.pull = GPIO_PULL_FLOATING;

    status = HAL_GPIO_SetPinParam(pGpioDev, pinIdx, &gpioParam);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    status = HAL_GPIO_WritePin(pGpioDev, pinIdx, GPIO_PIN_RESET);
    if (status != HAL_STATUS_OK) {
        return status;
    }


#endif

    return status;
}

int hif_IO_Deinit(uint32_t num)
{
    HAL_Status_t status = HAL_STATUS_OK;

#if (CONFIG_HIF_PM)
    uint8_t pinIdx = num % CONFIG_SOC_WKIOA_PIN_NUM;

    status = HAL_WKIO_SetPinMode(pWkioDev, pinIdx, HAL_WKIO_MODE_DISA);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    pWkioDev = NULL;
#else
    uint8_t pinIdx = num % CONFIG_SOC_GPIOA_PIN_NUM;

    status = HAL_GPIO_SetPinMode(pGpioDev, pinIdx, GPIOx_Pn_F15_DIS);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    pGpioDev = NULL;
#endif

    return status;
}

__hif_isr_text void hif_IO_SetOutput(uint8_t num, uint8_t val)
{
#if (CONFIG_HIF_PM)
    if (pWkioDev != NULL) {
        uint8_t pinIdx = num % CONFIG_SOC_WKIOA_PIN_NUM;
        uint8_t mode = (val) ? WKIO_PIN_SET : WKIO_PIN_RESET;

        HAL_WKIO_WritePin(pWkioDev, pinIdx, mode);
    }
#else
    if (pGpioDev != NULL) {
        uint8_t pinIdx = num % CONFIG_SOC_GPIOA_PIN_NUM;
        uint8_t mode = (val) ? GPIO_PIN_SET : GPIO_PIN_RESET;

        HAL_GPIO_WritePin(pGpioDev, pinIdx, mode);
    }
#endif
}


int hif_IO_GetInput(uint8_t num)
{
    return 0;
}



#endif /* CONFIG_HIF */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
