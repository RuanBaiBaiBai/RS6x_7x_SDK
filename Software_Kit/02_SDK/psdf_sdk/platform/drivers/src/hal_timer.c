/**
 ******************************************************************************
 * @file    hal_timer.c
 * @brief   hal timer define.
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
#include "hal_types.h"
#include "hal_dev.h"


#include "ll_timer.h"
#include "hal_timer.h"
#include "hw_tim.h"
#include "hal_board.h"


#ifndef CONFIG_TIMER_LOG_LEVEL
#define CONFIG_TIMER_LOG_LEVEL          LEVEL_ERR


#define LOG_MODULE                      "TIMER"
#define LOG_LEVEL                       CONFIG_TIMER_LOG_LEVEL

#include "log.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
#define DEVICE_ID_GET(ID)                                       (ID >> 8)
#define DEVICE_TYPE_GET(ID)                                     (ID & 0XFF)

#define TIMER0                               (TIMR0_DEV)
#define TIMER1                               (TIMR1_DEV)
#define TIMER2                               (TIMR2_DEV)
#define TIMER3                               (TIMR3_DEV)

static timer_dev_t *timer_instance[TIMER_NUM] = {TIMER0, TIMER1, TIMER2, TIMER3};

static uint32_t timer0_clock_conf = 0;
static uint32_t timer1_clock_conf = 0;
/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static void TIMER_IRQ_Handler(void *hdev);

__HW_STATIC_INLINE void timer_clock_enable(TIMER_ID_t TIMERID, uint32_t clk_src, uint32_t clk_div);
__HW_STATIC_INLINE void timer_clock_disable(TIMER_ID_t TIMERID);
/* Exported functions.
 * ----------------------------------------------------------------------------
 */

/** 定时器初始化
 *  @param TIMER设备id
 *  @param TIMER配置参数句柄
 */
HAL_Dev_t *HAL_TIMER_Init(TIMER_ID_t id, const TIMER_InitParam_t *param)
{
	timer_device_t *temp_device = (timer_device_t *)HAL_DEV_Find(HAL_DEV_TYPE_TMR, id);

	if (temp_device == NULL) {
		LOG_PRINT("temp_device = NULL\n");
	}

	HAL_Status_t status = -1;
	if (id >= TIMER_NUM) {
		LOG_PRINT("id = NULL\n");
		return NULL;
	}

	if (param == NULL) {
		LOG_PRINT("param = NULL\n");
		return NULL;
	}
	if (temp_device == NULL) {
		temp_device = (timer_device_t *)HAL_DEV_MemMalloc(sizeof(timer_device_t));
		if (temp_device == NULL) {
			LOG_PRINT("temp_device = NULL\n");
			return (HAL_Dev_t *)temp_device;
		}
		memset(temp_device, 0, sizeof(timer_device_t));
		temp_device->device.reg = timer_instance[id];
		if (id == TIMER0_ID) {
			temp_device->device.irqNum = TMR0_0_IRQn;
		} else if (id == TIMER1_ID) {
			temp_device->device.irqNum = TMR0_1_IRQn;
		} else if (id == TIMER2_ID) {
			temp_device->device.irqNum = TMR1_0_IRQn;
		} else if (id == TIMER3_ID) {
			temp_device->device.irqNum = TMR1_1_IRQn;
		}
		temp_device->device.irqHandler = TIMER_IRQ_Handler;
		IRQ_Disable(temp_device->device.irqNum);//关中断
		status = IRQ_Attach(temp_device->device.irqNum, temp_device->device.irqHandler);
		if (status != 0) {
			LOG_PRINT("IRQ_Attach error\n");
		}
		status = IRQ_AttachDevice(temp_device->device.irqNum, (HAL_Dev_t *)temp_device);
		if (status != 0) {
			LOG_PRINT("IRQ_AttachDevice error\n");
		}
		IRQ_Priority(temp_device->device.irqNum, 5);//中断优先级

		IRQ_Enable(temp_device->device.irqNum);//开中断

		temp_device->timer_id = id;
		status =  HAL_DEV_Register((HAL_Dev_t *)temp_device, HAL_DEV_TYPE_TMR, id);
		if (status != 0) {
			LOG_PRINT("HAL_DEV_Register error\n");
		}
	}

	memcpy(&temp_device->init_param, param, sizeof(TIMER_InitParam_t));

	return (HAL_Dev_t *)temp_device;
}

/** 定时器反初始化
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_DeInit(HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if ((DEVICE_ID_GET(timer->id) > TIMER_NUM) || (DEVICE_TYPE_GET(timer->id) != HAL_DEV_TYPE_TMR)) {
		return HAL_STATUS_INVALID_PARAM;
	}

	timer_clock_disable(temp_dev->timer_id);
	HAL_TIMER_Disable(timer);

	HAL_DEV_Unregister(timer);
	HAL_DEV_MemFree(timer);

	return HAL_STATUS_OK;
}

/** 打开定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Open(HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;
	TIMER_InitParam_t *pinit = &temp_dev->init_param;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if ((DEVICE_ID_GET(timer->id) > TIMER_NUM) || (DEVICE_TYPE_GET(timer->id) != HAL_DEV_TYPE_TMR)) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (timer->state == HAL_DEV_STATE_UNINIT) {
		return HAL_STATUS_INVALID_STATE;
	}

//	LOG_PRINT("SF src%d SF div%d\n", pinit->clockSrc, pinit->clockDiv);
//	LOG_PRINT("timer id %d\n", temp_dev->timer_id);
	timer_clock_enable(temp_dev->timer_id, pinit->clockSrc, pinit->clockDiv);

	timer->state = HAL_DEV_STATE_OPEN;

	return HAL_STATUS_OK;
}

/** 关闭定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Close(HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	if ((DEVICE_ID_GET(timer->id) > TIMER_NUM) || (DEVICE_TYPE_GET(timer->id) != HAL_DEV_TYPE_TMR)) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (timer->state != HAL_DEV_STATE_UNINIT || timer->state != HAL_DEV_STATE_INIT) {
		timer->state = HAL_DEV_STATE_CLOSE;
	}

	/* unlock */
	timer_clock_disable(temp_dev->timer_id);

	timer->state = HAL_DEV_STATE_CLOSE;

	return HAL_STATUS_OK;
}

/** 设置重装载值
 *  @param TIMER设备
 *  @param 设置重装载值
 */
HAL_Status_t HAL_TIMER_SetLoadCount(HAL_Dev_t *timer, uint32_t timerloadcount)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	LL_TIM_SetLoadCounter(timer_instance[temp_dev->timer_id], timerloadcount);

	uint32_t reload = LL_TIM_GetLoadCounter(timer_instance[temp_dev->timer_id]);
	LOG_PRINT("timer%d currentvalue: %d\n", temp_dev->timer_id, reload);

	return HAL_STATUS_OK;
}

/** 设置微秒
 *  @param TIMER设备
 *  @param 设置微秒
 *  @param 重复触发/单次触发
 */
HAL_Status_t HAL_TIMER_SetuS(HAL_Dev_t *timer, uint64_t us, TIMER_Mode_t mode)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;
	TIMER_InitParam_t *pinit = &temp_dev->init_param;
	uint32_t reload_value = 0;
	uint64_t bus_freq = 0;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (us <= 0) {
		return HAL_STATUS_INVALID_PARAM;
	}

	LL_TIM_SetUserDefinedMode(timer_instance[temp_dev->timer_id]);//设置用户定义模式

	if (pinit->clockSrc == TIMER_CLCOK_APB1_PCLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_APB1);
	} else if (pinit->clockSrc == TIMER_CLCOK_MSI_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_MSI);
	} else if (pinit->clockSrc == TIMER_CLCOK_DCXO_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_DCXO);
	} else if (pinit->clockSrc == TIMER_CLCOK_LSI_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_LSI);
	}
	reload_value = ((us*bus_freq)/(1<<pinit->clockDiv)/1000000-1);
//	LOG_PRINT("reload_value: %d\n",reload_value);
////	LOG_PRINT("clock_Div: %d\n",Div);
//	LOG_PRINT("bus_freq %d\n",bus_freq);

	if (mode == 0 || mode == 1) {
		temp_dev->Timer_Mode = mode;
	}

	if (reload_value > 0xFFFFFFFE) {
		LOG_PRINT("reload_value > 0xFFFFFFFF\n");
		return HAL_STATUS_ERROR;
	}

	LL_TIM_SetLoadCounter(timer_instance[temp_dev->timer_id], (uint32_t)reload_value);

	return HAL_STATUS_OK;
}

/** 设置毫秒
 *  @param TIMER设备
 *  @param 设置毫秒
 *  @param 重复触发/单次触发
 */
HAL_Status_t HAL_TIMER_SetmS(HAL_Dev_t *timer, uint64_t ms, TIMER_Mode_t mode)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;
	TIMER_InitParam_t *pinit = &temp_dev->init_param;
	uint32_t reload_value = 0;
	uint32_t bus_freq = 0;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (ms <= 0) {
		return HAL_STATUS_INVALID_PARAM;
	}
	LL_TIM_SetUserDefinedMode(timer_instance[temp_dev->timer_id]);//设置用户定义模式

	if (pinit->clockSrc == TIMER_CLCOK_APB1_PCLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_APB1);
	} else if (pinit->clockSrc == TIMER_CLCOK_MSI_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_MSI);
	} else if (pinit->clockSrc == TIMER_CLCOK_DCXO_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_DCXO);
	} else if (pinit->clockSrc == TIMER_CLCOK_LSI_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_LSI);
	}
	reload_value = ((ms*bus_freq)/(1<<pinit->clockDiv)/1000-1);
//	LOG_PRINT("reload_value: %d\n",reload_value);
////	LOG_PRINT("clock_Div: %d\n",Div);
//	LOG_PRINT("bus_freq %d\n",bus_freq);

	if (mode == 0 || mode == 1) {
		temp_dev->Timer_Mode = mode;
	}

	if (reload_value > 0xFFFFFFFE) {
		LOG_PRINT("reload_value > 0xFFFFFFFF\n");
		return HAL_STATUS_ERROR;
	}

	LL_TIM_SetLoadCounter(timer_instance[temp_dev->timer_id], reload_value);

	return HAL_STATUS_OK;
}

/** 设置秒
 *  @param TIMER设备
 *  @param 设置秒
 *  @param 重复触发/单次触发
 */
HAL_Status_t HAL_TIMER_SetS(HAL_Dev_t *timer, uint32_t s, TIMER_Mode_t mode)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;
	TIMER_InitParam_t *pinit = &temp_dev->init_param;
	uint32_t reload_value = 0;
	uint32_t bus_freq = 0;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (s <= 0) {
		return HAL_STATUS_INVALID_PARAM;
	}
	LL_TIM_SetUserDefinedMode(timer_instance[temp_dev->timer_id]);//设置用户定义模式

	if (pinit->clockSrc == TIMER_CLCOK_APB1_PCLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_APB1);
	} else if (pinit->clockSrc == TIMER_CLCOK_MSI_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_MSI);
	} else if (pinit->clockSrc == TIMER_CLCOK_DCXO_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_DCXO);
	} else if (pinit->clockSrc == TIMER_CLCOK_LSI_CLK) {
		bus_freq = HAL_BOARD_GetFreq(CLOCK_LSI);
	}
	reload_value = ((s*bus_freq)/(1<<pinit->clockDiv)-1);
//	LOG_PRINT("reload_value: %d\n",reload_value);
////	LOG_PRINT("clock_Div: %d\n",Div);
//	LOG_PRINT("bus_freq %d\n",bus_freq);

	if (mode == 0 || mode == 1) {
		temp_dev->Timer_Mode = mode;
	}

	if (reload_value > 0xFFFFFFFF) {
		LOG_PRINT("reload_value > 0xFFFFFFFF\n");
		return HAL_STATUS_ERROR;
	}

	LL_TIM_SetLoadCounter(timer_instance[temp_dev->timer_id], reload_value);

	return HAL_STATUS_OK;
}

/** 使能定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Enable(const HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	LL_TIM_Enable(timer_instance[temp_dev->timer_id]);

	return HAL_STATUS_OK;
}

/** 失能定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Disable(const HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	LL_TIM_Disable(timer_instance[temp_dev->timer_id]);

	return HAL_STATUS_OK;
}

/** 注册回调函数
 *  @param TIMER设备
 *  @param 回调函数指针
 */
void HAL_TIMER_RegisterIRQ(HAL_Dev_t *timer, timer_callback_t user_cb, void *arg)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	if (timer == NULL) {
		return;
	}

	temp_dev->callback = user_cb;
	temp_dev->callback_arg = arg;

	HAL_TIMER_Enable_Irq(timer);
}

/** 获取当前计数值
 *  @param TIMER设备
 */
uint32_t HAL_TIMER_GetCurrentValue(HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;
	uint32_t current = 0;

	if (timer == NULL) {
		return HAL_STATUS_INVALID_PARAM;
	}

	current = LL_TIM_GetCurrentValue(timer_instance[temp_dev->timer_id]);
	return current;
}

/** 获取已过去秒
 *  @param TIMER设备
 *  @param 上一次计数值
 *  @param 当前计数值
 */
uint32_t HAL_TIMER_GetElapsedValue(HAL_Dev_t *timer, uint32_t LastValue, uint32_t CurrentValue)
{
	return 0;
}

/** 阻塞延迟(us)
 *  @param TIMER设备
 *  @param 阻塞时间
 */
void HAL_TIMER_BusyWait(HAL_Dev_t *timer, uint64_t us)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	IRQ_Disable(temp_dev->device.irqNum);

	LL_TIM_DisableInterruptMask(timer_instance[temp_dev->timer_id]);

	LL_TIM_SetUserDefinedMode(timer_instance[temp_dev->timer_id]);

	HAL_TIMER_SetuS(timer, us, 0);

	LL_TIM_Enable(timer_instance[temp_dev->timer_id]);

	while (!LL_TIM_GetInterruptStatus(timer_instance[temp_dev->timer_id]))
	{};
	LL_TIM_Disable(timer_instance[temp_dev->timer_id]);

	IRQ_Enable(temp_dev->device.irqNum);
}

/** 定时器中断
 *  @param TIMER设备
 */
__sram_text static void TIMER_IRQ_Handler(void *hdev)
{
	timer_device_t *temp_dev = (timer_device_t *)hdev;
	if (LL_TIM_GetInterruptStatus(timer_instance[temp_dev->timer_id])) {
		//清除中断标志位
		LL_TIM_ClearInterrupt(timer_instance[temp_dev->timer_id]);
		if (temp_dev && temp_dev->callback) {
			temp_dev->callback(temp_dev->callback_arg);
		}
		if (temp_dev->Timer_Mode == 1) {
			LL_TIM_Disable(timer_instance[temp_dev->timer_id]);
		}
	}
}

/**	开启时钟
 *  @param TIMER设备id
 *  @param 时钟源
 *  @param 分频系数
 */
__HW_STATIC_INLINE void timer_clock_enable(TIMER_ID_t TIMERID, uint32_t clk_src, uint32_t clk_div)
{
	if (TIMERID == TIMER0_ID) {
		HW_SET_MSK_VAL(timer0_clock_conf, 0x07 << 8, 0x08, clk_div);
		HW_SET_MSK_VAL(timer0_clock_conf, 0x03 << 0, 0x00, clk_src);

		LL_RCC_TMR0_SetClockSource(clk_src);

		LL_RCC_TMR0_SetPrescaler(clk_div);

		LL_RCC_TMR0_DisableBusClock();

		LL_RCC_TMR0_DisableClock();

		LL_RCC_TMR0_Reset();

		LL_RCC_TMR0_EnableBusClock();

		LL_RCC_TMR0_EnableClock();

	} else if (TIMERID == TIMER1_ID) {
		HW_SET_MSK_VAL(timer0_clock_conf, 0x07 << 24, 0x18, clk_div);
		HW_SET_MSK_VAL(timer0_clock_conf, 0x03 << 16, 0x10, clk_src);
		CCMU1_DEV->CCMU_TMR0_FCLK_SRC_SEL = timer0_clock_conf;

		LL_RCC_TMR1_DisableBusClock();

		LL_RCC_TMR1_DisableClock();

		LL_RCC_TMR1_Reset();

		LL_RCC_TMR1_EnableBusClock();

		LL_RCC_TMR1_EnableClock();
	} else if (TIMERID == TIMER2_ID) {
		HW_SET_MSK_VAL(timer1_clock_conf, 0x07 << 8, 0x08, clk_div);
		HW_SET_MSK_VAL(timer1_clock_conf, 0x03 << 0, 0x00, clk_src);

		LL_RCC_TMR2_SetClockSource(clk_src);

		LL_RCC_TMR2_SetPrescaler(clk_div);

		LL_RCC_TMR2_DisableBusClock();

		LL_RCC_TMR2_DisableClock();

		LL_RCC_TMR2_Reset();

		LL_RCC_TMR2_EnableBusClock();

		LL_RCC_TMR2_EnableClock();
	} else if (TIMERID == TIMER3_ID) {
		HW_SET_MSK_VAL(timer1_clock_conf, 0x07 << 24, 0x18, clk_div);
		HW_SET_MSK_VAL(timer1_clock_conf, 0x03 << 16, 0x10, clk_src);
		CCMU1_DEV->CCMU_TMR1_FCLK_SRC_SEL = timer1_clock_conf;

		LL_RCC_TMR3_DisableBusClock();

		LL_RCC_TMR3_DisableClock();

		LL_RCC_TMR3_Reset();

		LL_RCC_TMR3_EnableBusClock();

		LL_RCC_TMR3_EnableClock();
	}
}

/** 关闭时钟
 *  @param TIMER设备id
 */
__HW_STATIC_INLINE void timer_clock_disable(TIMER_ID_t TIMERID)
{
	if (TIMERID == TIMER0_ID) {
		LL_RCC_TMR0_DisableBusClock();

		LL_RCC_TMR0_DisableClock();

		LL_RCC_TMR0_Reset();
	} else if (TIMERID == TIMER1_ID) {
		LL_RCC_TMR1_DisableBusClock();

		LL_RCC_TMR1_DisableClock();

		LL_RCC_TMR1_Reset();
	} else if (TIMERID == TIMER2_ID) {
		LL_RCC_TMR2_DisableBusClock();

		LL_RCC_TMR2_DisableClock();

		LL_RCC_TMR2_Reset();
	} else if (TIMERID == TIMER3_ID) {
		LL_RCC_TMR3_DisableBusClock();

		LL_RCC_TMR3_DisableClock();

		LL_RCC_TMR3_Reset();
	}
}

/** 使能定时器中断
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Enable_Irq(const HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	LL_TIM_DisableInterruptMask(timer_instance[temp_dev->timer_id]);

	return HAL_STATUS_OK;
}

/** 失能定时器中断
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Disable_Irq(const HAL_Dev_t *timer)
{
	timer_device_t *temp_dev = (timer_device_t *)timer;

	LL_TIM_EnableInterruptMask(timer_instance[temp_dev->timer_id]);

	return HAL_STATUS_OK;
}

#endif /* (CONFIG_DRIVER_VERSION == 12) */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
