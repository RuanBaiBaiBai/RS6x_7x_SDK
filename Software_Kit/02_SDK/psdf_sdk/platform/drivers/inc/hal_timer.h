/**
 ******************************************************************************
 * @file    hal_timer.h
 * @brief   hal timer define.
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

#ifndef _HAL_TIMER_H
#define _HAL_TIMER_H

/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "hal_types.h"

#include "hal_dev.h"
#include "hal_board.h"
#include "hal_os.h"
#include "ll_timer.h"


#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ----------------------------------------------------------------------------
 */
/* Exported constants.
 * ----------------------------------------------------------------------------
 */
//设备ID
typedef enum {
	TIMER0_ID = 0U,
	TIMER1_ID = 1U,
	TIMER2_ID = 2U,
	TIMER3_ID = 3U,
	TIMER_NUM
} TIMER_ID_t;

//时钟源
typedef enum {
	TIMER_CLCOK_APB1_PCLK   = 0,
	TIMER_CLCOK_MSI_CLK     = 1,
	TIMER_CLCOK_DCXO_CLK    = 2,
	TIMER_CLCOK_LSI_CLK     = 3,
} TIMER_ClockSrc_t;

//分频系数
typedef enum {
	TIMER_CLCOK_DIV1    = 0,
	TIMER_CLCOK_DIV2    = 1,
	TIMER_CLCOK_DIV4    = 2,
	TIMER_CLCOK_DIV8    = 3,
	TIMER_CLCOK_DIV16   = 4,
	TIMER_CLCOK_DIV32   = 5,
	TIMER_CLCOK_DIV64   = 6,
	TIMER_CLCOK_DIV128  = 7
} TIMER_ClockDiv_t;

//触发模式
typedef enum {
	auto_trigger_irq    = 0,//重复触发中断
	single_trigger_irq  = 1 //单触发中断
} TIMER_Mode_t;

//Timer初始化配置参数
//计数方式默认向下计数
typedef struct {
	TIMER_ClockSrc_t clockSrc;//时钟源
	TIMER_ClockDiv_t clockDiv;//分频系数
} TIMER_InitParam_t;

typedef void (*timer_callback_t) (void *arg);

typedef struct {
	HAL_Dev_t          device;
	TIMER_ID_t         timer_id;        //ID
	TIMER_InitParam_t  init_param;
	uint32_t           reload_value;    //重装载值
	uint8_t            Timer_Mode;      //触发模式
	timer_callback_t   callback;        //回调函数指针
	void               *callback_arg;    //回调函数参数
} timer_device_t;
/* Exported macro.GA
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */
/** 定时器初始化
 *  @param TIMER设备id
 *  @param TIMER配置参数句柄
 */
HAL_Dev_t *HAL_TIMER_Init(TIMER_ID_t id, const TIMER_InitParam_t *param);

/** 定时器反初始化
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_DeInit(HAL_Dev_t *timer);

/** 打开定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Open(HAL_Dev_t *timer);

/** 关闭定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Close(HAL_Dev_t *timer);

/** 设置重装载值
 *  @param TIMER设备
 *  @param 设置重装载值
 */
HAL_Status_t HAL_TIMER_SetLoadCount(HAL_Dev_t *timer, uint32_t timerloadcount);

/** 设置微秒
 *  @param TIMER设备
 *  @param 设置微秒
 *  @param 重复触发/单次触发
 */
HAL_Status_t HAL_TIMER_SetuS(HAL_Dev_t *timer, uint64_t us, TIMER_Mode_t mode);

/** 设置毫秒
 *  @param TIMER设备
 *  @param 设置毫秒
 *  @param 重复触发/单次触发
 */
HAL_Status_t HAL_TIMER_SetmS(HAL_Dev_t *timer, uint64_t ms, TIMER_Mode_t mode);

/** 设置秒
 *  @param TIMER设备
 *  @param 设置秒
 *  @param 重复触发/单次触发
 */
HAL_Status_t HAL_TIMER_SetS(HAL_Dev_t *timer, uint32_t s, TIMER_Mode_t mode);

/** 使能定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Enable(const HAL_Dev_t *timer);

/** 失能定时器
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Disable(const HAL_Dev_t *timer);

/** 注册回调函数
 *  @param TIMER设备
 *  @param 回调函数指针
 *  @param 回调函数参数
 */
void HAL_TIMER_RegisterIRQ(HAL_Dev_t *timer, timer_callback_t user_cb, void *arg);

/** 获取当前计数值
 *  @param TIMER设备
 */
uint32_t HAL_TIMER_GetCurrentValue(HAL_Dev_t *timer);

/** 阻塞延迟(us)
 *  @param TIMER设备
 *  @param 阻塞时间
 */
void HAL_TIMER_BusyWait(HAL_Dev_t *timer, uint64_t us);

/** 使能定时器中断
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Enable_Irq(const HAL_Dev_t *timer);

/** 失能定时器中断
 *  @param TIMER设备
 */
HAL_Status_t HAL_TIMER_Disable_Irq(const HAL_Dev_t *timer);

#ifdef __cplusplus
}
#endif

#endif /* (CONFIG_DRIVER_VERSION == 12) */


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */


