/**
 **************************************************************************************************
 * @file    port_io.c
 * @brief   adaptation for gpio file.
 * @attention
 *        Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"

#if (CFG_HOST_PORT_IO_EN == 1)
#include "hal_wkio.h"
#include "ll_gpio.h"
#include "hal_gpio.h"
#endif  /* CFG_HOST_PORT_IO_EN == 1 */


/* Local Variable.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_IO_EN == 1)
static HAL_Dev_t *gpio_dev;
#endif  /* CFG_HOST_PORT_IO_EN == 1 */


/* Local Function Declaration.
 * ------------------------------------------------------------------------------------------------
 */
/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_IO_EN == 1)
static HAL_Dev_t * wkio_dev;
int host_io_mode_set(uint32_t io, host_io_mode_t mode, host_io_pull_t pull, bool is_wkio)
{
    uint32_t io_mode;
    uint32_t io_pull;

    if (is_wkio) {
		HAL_Status_t status;
		if (wkio_dev == NULL) {
			wkio_dev = HAL_WKIO_Init(WKIO_PORT_A);
			if (wkio_dev == NULL) {
			 return HOST_ERRCODE_IO_ERROR;
			}
		}
		WKIO_PinMode_t pin_mode;
		switch (mode) {
		case HOST_IO_MODE_INTERRUPT:
			pin_mode = HAL_WKIO_MODE_WAKE;
			break;
		case HOST_IO_MODE_DISABLE:
			pin_mode = HAL_WKIO_MODE_DISA;
			break;
		case HOST_IO_MODE_OUTPUT:
			pin_mode = HAL_WKIO_MODE_HOLD;
			break;
		case HOST_IO_MODE_INPUT:
		default:
			pin_mode = HAL_WKIO_MODE_GPIO;
			break;
		}
		status = HAL_WKIO_SetPinMode(wkio_dev, io, pin_mode);
		status |= HAL_WKIO_SetPinPull(wkio_dev, io, (WKIO_PinPull_t)pull);
		return (status != HAL_STATUS_OK) ? status : HOST_ERRCODE_SUCCESS;
    }

    if (mode == HOST_IO_MODE_DISABLE) {
        io_mode = LL_GPIOx_Pn_F15_DIS;
    } else if (mode == HOST_IO_MODE_OUTPUT) {
        io_mode = LL_GPIOx_Pn_F1_OUTPUT;
    } else if (mode == HOST_IO_MODE_INPUT) {
        io_mode = LL_GPIOx_Pn_F0_INPUT;
    } else if (mode == HOST_IO_MODE_INTERRUPT) {
        io_mode = LL_GPIOx_Pn_F14_EINT;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (pull == HOST_IO_PULL_NO) {
        io_pull = LL_GPIO_PULL_NO;
    } else if (pull == HOST_IO_PULL_UP) {
        io_pull = LL_GPIO_PULL_UP;
    } else if (pull == HOST_IO_PULL_DOWN) {
        io_pull = LL_GPIO_PULL_DOWN;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (io < 16) {
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, io, io_mode);
        LL_GPIO_SetPinPull(GPIOA_DEV, io, io_pull);
        if (io_mode == LL_GPIOx_Pn_F14_EINT) {
            LL_GPIO_SetPinStreng(GPIOA_DEV, io, LL_GPIO_DRIVING_LVL_1);
        }
    } else {
        io = io - 16;
        LL_GPIO_SetPinFuncMode(GPIOB_DEV, io, io_mode);
        LL_GPIO_SetPinPull(GPIOB_DEV, io, io_pull);
        if (io_mode == LL_GPIOx_Pn_F14_EINT) {
            LL_GPIO_SetPinStreng(GPIOB_DEV, io, LL_GPIO_DRIVING_LVL_1);
        }
    }
    return HOST_ERRCODE_SUCCESS;
}


void host_io_write(uint32_t io, host_io_value_t value, bool is_wkio)
{
    if (is_wkio) {
		HAL_WKIO_WritePin(wkio_dev, io, (WKIO_PinState_t)value);
        return ;
    }

    if (io < 16) {
        if (value == HOST_IO_VALUE_LOW) {
            LL_GPIO_ResetOutputPin(GPIOA_DEV, (1 << io));
        } else if (value == HOST_IO_VALUE_HIGH) {
            LL_GPIO_SetOutputPin(GPIOA_DEV, (1 << io));
        }
    } else {
        io = io - 16;
        if (value == HOST_IO_VALUE_LOW) {
            LL_GPIO_ResetOutputPin(GPIOB_DEV, (1 << io));
        } else if (value == HOST_IO_VALUE_HIGH) {
            LL_GPIO_SetOutputPin(GPIOB_DEV, (1 << io));
        }
    }
}


host_io_value_t host_io_read(uint32_t io, bool is_wkio)
{
    if (is_wkio) {
		HAL_Status_t status = HAL_WKIO_ReadWakePin(wkio_dev, io);
        return status < 0 ? HOST_IO_VALUE_LOW : (host_io_value_t)status;
    }

    if (io < 16) {
        if (LL_GPIO_IsInputPinSet(GPIOA_DEV, (1 << io))) {
            return HOST_IO_VALUE_HIGH;
        } else {
            return HOST_IO_VALUE_LOW;
        }
    } else {
        io = io - 16;
        if (LL_GPIO_IsInputPinSet(GPIOB_DEV, (1 << io))) {
            return HOST_IO_VALUE_HIGH;
        } else {
            return HOST_IO_VALUE_LOW;
        }
    }
}


int host_io_irq_cfg(uint32_t io, host_io_irq_trig_t trig, host_io_irq_callback_t pcallback, void *arg, bool is_wkio)
{
    int status = 0;
    uint8_t idx;

    if (pcallback == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (is_wkio) {
		HAL_Status_t status;
		WKIO_WakeParam_t wkioIrq;
	    wkioIrq.pull     = HAL_WKIO_PULL_DOWN;
		wkioIrq.event    = (WKIO_WakeEvent_t)trig;
    	wkioIrq.callback = (WKIO_WakeCallback_t)pcallback;
    	wkioIrq.arg      =  arg;
    	status = HAL_WKIO_SetWakeParam(wkio_dev, io, &wkioIrq);
        return (status != HAL_STATUS_OK) ? status : HOST_ERRCODE_SUCCESS;
    }

    uint8_t port = 0;

    if(io > 15) {
        port = 1;
        idx = io % 16;
    } else {
        port = 0;
        idx = io;
    }

    GPIO_IrqParam_t gpioIrqParam;

    gpio_dev = HAL_GPIO_Init(port);
    if (gpio_dev == NULL) {
        return HOST_ERRCODE_IO_ERROR;
    }

    gpioIrqParam.event    = (GPIO_IrqEvent_t)trig;
    gpioIrqParam.callback = pcallback;
    gpioIrqParam.arg      = arg;

    status = HAL_GPIO_SetIRQParam(gpio_dev, idx, &gpioIrqParam);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    status = HAL_GPIO_EnableIRQ(gpio_dev, idx);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int host_io_irq_enable(uint32_t io, bool is_wkio)
{
    uint8_t idx;
    uint8_t port = 0;
    HAL_Dev_t *dev = NULL;

    if (is_wkio) {
		HAL_Status_t status = HAL_WKIO_SetPinMode(wkio_dev, io, HAL_WKIO_MODE_WAKE);
        return (status != HAL_STATUS_OK) ? status : HOST_ERRCODE_SUCCESS;
    }

    if(io > 15) {
        port = 1;
        idx = io % 16;
    } else {
        port = 0;
        idx = io;
    }

    dev = HAL_GPIO_Init(port);
    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HAL_GPIO_EnableIRQ(dev, idx);
}


int host_io_irq_disable(uint32_t io, bool is_wkio)
{
    uint8_t idx;
    uint8_t port = 0;
    HAL_Dev_t *dev = NULL;

    if (is_wkio) {
		HAL_Status_t status = HAL_WKIO_SetPinMode(wkio_dev, io, HAL_WKIO_MODE_GPIO);
        return (status != HAL_STATUS_OK) ? status : HOST_ERRCODE_SUCCESS;
    }

    if(io > 15) {
        port = 1;
        idx = io % 16;
    } else {
        port = 0;
        idx = io;
    }

    dev = HAL_GPIO_Init(port);
    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HAL_GPIO_DisableIRQ(dev, idx, true);
}
#endif  /* CFG_HOST_PORT_IO_EN == 1 */


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
