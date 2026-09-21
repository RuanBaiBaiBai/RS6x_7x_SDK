/**
 ******************************************************************************
 * @file    cmd_timer.c
 * @brief   cmd timer define.
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
#include "irq.h"
#include "cmd_config.h"
#include "log.h"
#include "stdlib.h"
#include "hal_timer.h"

#if ((CONFIG_SHELL_CMD_TIMER == 1) && (CONFIG_SHELL == 1))
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

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static HAL_Dev_t *tempdev;
static HAL_Dev_t *tempdev1;
static HAL_Dev_t *tempdev2;
static HAL_Dev_t *tempdev3;
/* Exported functions.
 * ----------------------------------------------------------------------------
 */
//初始化timer
HAL_Status_t init_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	TIMER_ClockSrc_t clocksrc;
	TIMER_ClockDiv_t clockdiv;
	HAL_Status_t status = HAL_STATUS_OK;
	TIMER_InitParam_t timerParam;

	id = atoi(argv[1]);//id
	LOG_PRINT("id:%d\n", id);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		LOG_IO(0x1000, 2, 0, 0);    //拉低PA2
	} else if (id == TIMER1_ID) {
		LOG_IO(0x1000, 6, 0, 0);    //拉低PA6
	} else if (id == TIMER2_ID) {
		LOG_IO(0x1000, 4, 0, 0);    //拉低PA4
	} else if (id == TIMER3_ID) {
		LOG_IO(0x1000, 5, 0, 0);    //拉低PA5
	}

	clocksrc = atoi(argv[2]);//时钟源
	LOG_PRINT("clocksrc %d\n", clocksrc);
	if (clocksrc > TIMER_CLCOK_LSI_CLK || clocksrc < TIMER_CLCOK_APB1_PCLK) {
		return HAL_STATUS_INVALID_PARAM;
	}

	clockdiv = atoi(argv[3]);//分频系数
	LOG_PRINT("clockdiv %d\n", clockdiv);
	timerParam.clockSrc = clocksrc;
	timerParam.clockDiv = clockdiv;
	if (id == TIMER0_ID) {
		tempdev = HAL_TIMER_Init(id, &timerParam);
	} else if (id == TIMER1_ID) {
		tempdev1 = HAL_TIMER_Init(id, &timerParam);
	} else if (id == TIMER2_ID) {
		tempdev2 = HAL_TIMER_Init(id, &timerParam);
	} else if (id == TIMER3_ID) {
		tempdev3 = HAL_TIMER_Init(id, &timerParam);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("Init OK\n");
	}

	return status;
}

HAL_Status_t deinit_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		status = HAL_TIMER_DeInit(tempdev);
	} else if (id == TIMER1_ID) {
		status = HAL_TIMER_DeInit(tempdev1);
	} else if (id == TIMER2_ID) {
		status = HAL_TIMER_DeInit(tempdev2);
	} else if (id == TIMER3_ID) {
		status = HAL_TIMER_DeInit(tempdev3);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("DeInit OK\n");
	}

	return status;
}

HAL_Status_t open_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		status = HAL_TIMER_Open(tempdev);
	} else if (id == TIMER1_ID) {
		status = HAL_TIMER_Open(tempdev1);
	} else if (id == TIMER2_ID) {
		status = HAL_TIMER_Open(tempdev2);
	} else if (id == TIMER3_ID) {
		status = HAL_TIMER_Open(tempdev3);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("Open OK\n");
	}

	return status;
}

HAL_Status_t close_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		status = HAL_TIMER_Close(tempdev);
	} else if (id == TIMER1_ID) {
		status = HAL_TIMER_Close(tempdev1);
	} else if (id == TIMER2_ID) {
		status = HAL_TIMER_Close(tempdev2);
	} else if (id == TIMER3_ID) {
		status = HAL_TIMER_Close(tempdev3);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("Close OK\n");
	}

	return status;
}

HAL_Status_t set_loadcount_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;
	uint32_t reload = 0;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	reload = atoi(argv[2]);
	if (reload == 0) {
		LOG_PRINT("Set loadcount:%d\n", reload);
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		HAL_TIMER_SetLoadCount(tempdev, reload);
	} else if (id == TIMER1_ID) {
		HAL_TIMER_SetLoadCount(tempdev1, reload);
	} else if (id == TIMER2_ID) {
		HAL_TIMER_SetLoadCount(tempdev2, reload);
	} else if (id == TIMER3_ID) {
		HAL_TIMER_SetLoadCount(tempdev3, reload);
	}

	LOG_PRINT("Set loadcount:%d\n", reload);

	return status;
}

HAL_Status_t set_s_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;
	uint32_t s = 0;
	uint8_t mode = 0;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	s = atoi(argv[2]);
	if (s == 0) {
		LOG_PRINT("Set s:%d\n", s);
		return HAL_STATUS_INVALID_PARAM;
	}

	mode = atoi(argv[3]);
	if (id == TIMER0_ID) {
		HAL_TIMER_SetS(tempdev, s, mode);
	} else if (id == TIMER1_ID) {
		HAL_TIMER_SetS(tempdev1, s, mode);
	} else if (id == TIMER2_ID) {
		HAL_TIMER_SetS(tempdev2, s, mode);
	} else if (id == TIMER3_ID) {
		HAL_TIMER_SetS(tempdev3, s, mode);
	}

	LOG_PRINT("Set s:%d\n", s);

	return status;
}

HAL_Status_t set_ms_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;
	uint64_t ms = 0;
	uint8_t mode;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	ms = atoi(argv[2]);
	if (ms == 0) {
		LOG_PRINT("Set ms:%d\n", ms);
		return HAL_STATUS_INVALID_PARAM;
	}

	mode = atoi(argv[3]);
	if (id == TIMER0_ID) {
		HAL_TIMER_SetmS(tempdev, ms, mode);
	} else if (id == TIMER1_ID) {
		HAL_TIMER_SetmS(tempdev1, ms, mode);
	} else if (id == TIMER2_ID) {
		HAL_TIMER_SetmS(tempdev2, ms, mode);
	} else if (id == TIMER3_ID) {
		HAL_TIMER_SetmS(tempdev3, ms, mode);
	}

	LOG_PRINT("Set ms:%d\n", ms);

	return status;
}

HAL_Status_t set_us_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;
	uint64_t us = 0;
	uint8_t mode;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	us = atoi(argv[2]);
	if (us == 0) {
		LOG_PRINT("Set us:%d\n", us);
		return HAL_STATUS_INVALID_PARAM;
	}

	mode = atoi(argv[3]);
	if (id == TIMER0_ID) {
		HAL_TIMER_SetuS(tempdev, us, mode);
	} else if (id == TIMER1_ID) {
		HAL_TIMER_SetuS(tempdev1, us, mode);
	} else if (id == TIMER2_ID) {
		HAL_TIMER_SetuS(tempdev2, us, mode);
	} else if (id == TIMER3_ID) {
		HAL_TIMER_SetuS(tempdev3, us, mode);
	}

	LOG_PRINT("Set us:%d\n", us);

	return status;
}

HAL_Status_t enable_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		status = HAL_TIMER_Enable(tempdev);
		LOG_IO(0x1000, 2, 1, 0);    //拉高PA2
	} else if (id == TIMER1_ID) {
		status = HAL_TIMER_Enable(tempdev1);
		LOG_IO(0x1000, 6, 1, 0);    //拉高PA6
	} else if (id == TIMER2_ID) {
		status = HAL_TIMER_Enable(tempdev2);
		LOG_IO(0x1000, 4, 1, 0);    //拉高PA4
	} else if (id == TIMER3_ID) {
		status = HAL_TIMER_Enable(tempdev3);
		LOG_IO(0x1000, 5, 1, 0);    //拉高PA5
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("Enable OK\n");
	}

	return status;
}

HAL_Status_t disable_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		status = HAL_TIMER_Disable(tempdev);
	} else if (id == TIMER1_ID) {
		status = HAL_TIMER_Disable(tempdev1);
	} else if (id == TIMER2_ID) {
		status = HAL_TIMER_Disable(tempdev2);
	} else if (id == TIMER3_ID) {
		status = HAL_TIMER_Disable(tempdev3);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("Disable OK\n");
	}

	return status;
}

__sram_text void TIMER0_Callback(void *arg)
{
	static bool trig_high = true;
	if (trig_high) {
		LOG_IO(0x1000, 2, 0, 0);    //拉低PA2
	} else {
		LOG_IO(0x1000, 2, 1, 0);    //拉高PA2
	}
	trig_high = !trig_high;
}

__sram_text void TIMER1_Callback(void *arg)
{
	static bool trig_high = true;
	if (trig_high) {
		LOG_IO(0x1000, 6, 0, 0);    //拉低PA6
	} else {
		LOG_IO(0x1000, 6, 1, 0);    //拉高PA6
	}
	trig_high = !trig_high;
}

__sram_text void TIMER2_Callback(void *arg)
{
	static bool trig_high = true;
	if (trig_high) {
		LOG_IO(0x1000, 4, 0, 0);    //拉低PA4
	} else {
		LOG_IO(0x1000, 4, 1, 0);    //拉高PA4
	}
	trig_high = !trig_high;
}

__sram_text void TIMER3_Callback(void *arg)
{
	static bool trig_high = true;
	if (trig_high) {
		LOG_IO(0x1000, 5, 0, 0);    //拉低PA5
	} else {
		LOG_IO(0x1000, 5, 1, 0);    //拉高PA5
	}
	trig_high = !trig_high;
}

HAL_Status_t registerirq_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		HAL_TIMER_RegisterIRQ(tempdev, (void *)TIMER0_Callback, NULL);
	} else if (id == TIMER1_ID) {
		HAL_TIMER_RegisterIRQ(tempdev1, (void *)TIMER1_Callback, NULL);
	} else if (id == TIMER2_ID) {
		HAL_TIMER_RegisterIRQ(tempdev2, (void *)TIMER2_Callback, NULL);
	} else if (id == TIMER3_ID) {
		HAL_TIMER_RegisterIRQ(tempdev3, (void *)TIMER3_Callback, NULL);
	}

	LOG_PRINT("TIMER%d RegisterIRQ OK\n", id);

	return status;
}

HAL_Status_t get_currentvalue_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;
	uint32_t currentvalue = 0;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		currentvalue = HAL_TIMER_GetCurrentValue(tempdev);
	} else if (id == TIMER1_ID) {
		currentvalue = HAL_TIMER_GetCurrentValue(tempdev1);
	} else if (id == TIMER2_ID) {
		currentvalue = HAL_TIMER_GetCurrentValue(tempdev2);
	} else if (id == TIMER3_ID) {
		currentvalue = HAL_TIMER_GetCurrentValue(tempdev3);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("currentvalue:%d\n", currentvalue);
	}

	return status;
}

HAL_Status_t busywait_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;
	uint64_t us = 0;

	LOG_IO(0x1000, 2, 0, 0);    //拉低PA2
	LOG_IO(0x1000, 6, 0, 0);    //拉低PA6
	LOG_IO(0x1000, 4, 0, 0);    //拉低PA4
	LOG_IO(0x1000, 5, 0, 0);    //拉低PA5

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	us = atoi(argv[2]);
	if (id == TIMER0_ID) {
		LOG_IO(0x1000, 2, 1, 0);    //拉高PA2
		HAL_TIMER_BusyWait(tempdev, us);
		LOG_IO(0x1000, 2, 0, 0);    //拉低PA2
	} else if (id == TIMER1_ID) {
		LOG_IO(0x1000, 6, 1, 0);    //拉高PA6
		HAL_TIMER_BusyWait(tempdev1, us);
		LOG_IO(0x1000, 6, 0, 0);    //拉低PA6
	} else if (id == TIMER2_ID) {
		LOG_IO(0x1000, 4, 1, 0);    //拉高PA4
		HAL_TIMER_BusyWait(tempdev2, us);
		LOG_IO(0x1000, 4, 0, 0);    //拉低PA4
	} else if (id == TIMER3_ID) {
		LOG_IO(0x1000, 5, 1, 0);    //拉高PA5
		HAL_TIMER_BusyWait(tempdev3, us);
		LOG_IO(0x1000, 5, 0, 0);    //拉低PA5
	}
	//LOG_PRINT("busywait us:%d\n", us);

	return status;
}

HAL_Status_t enable_irq_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		status = HAL_TIMER_Enable_Irq(tempdev);
	} else if (id == TIMER1_ID) {
		status = HAL_TIMER_Enable_Irq(tempdev1);
	} else if (id == TIMER2_ID) {
		status = HAL_TIMER_Enable_Irq(tempdev2);
	} else if (id == TIMER3_ID) {
		status = HAL_TIMER_Enable_Irq(tempdev3);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("Enable_irq OK\n");
	}

	return status;
}

HAL_Status_t disable_irq_cmd(Shell *shell, int argc, char *argv[])
{
	TIMER_ID_t id;
	HAL_Status_t status = HAL_STATUS_OK;

	id = atoi(argv[1]);
	if (id != TIMER0_ID && id != TIMER1_ID && id != TIMER2_ID && id != TIMER3_ID) {
		return HAL_STATUS_INVALID_PARAM;
	}

	if (id == TIMER0_ID) {
		status = HAL_TIMER_Disable_Irq(tempdev);
	} else if (id == TIMER1_ID) {
		status = HAL_TIMER_Disable_Irq(tempdev1);
	} else if (id == TIMER2_ID) {
		status = HAL_TIMER_Disable_Irq(tempdev2);
	} else if (id == TIMER3_ID) {
		status = HAL_TIMER_Disable_Irq(tempdev3);
	}

	if (status == HAL_STATUS_OK) {
		LOG_PRINT("Disable_irq OK\n");
	}

	return status;
}

ShellCommand timer_cmds[] = {
	SHELL_CMD_ITEM(init,             init_cmd,             "<id> <clocksrc> <clockdiv>"),
	SHELL_CMD_ITEM(deinit,           deinit_cmd,           "<id>"),
	SHELL_CMD_ITEM(open,             open_cmd,             "<id>"),
	SHELL_CMD_ITEM(close,            close_cmd,            "<id>"),
	SHELL_CMD_ITEM(set_loadcount,    set_loadcount_cmd,    "<id> <loadcount>"),
	SHELL_CMD_ITEM(set_s,            set_s_cmd,            "<id> <s>  <mode>"),
	SHELL_CMD_ITEM(set_ms,           set_ms_cmd,           "<id> <ms> <mode>"),
	SHELL_CMD_ITEM(set_us,           set_us_cmd,           "<id> <us> <mode>"),
	SHELL_CMD_ITEM(enable,           enable_cmd,           "<id>"),
	SHELL_CMD_ITEM(disable,          disable_cmd,          "<id>"),
	SHELL_CMD_ITEM(registerirq,      registerirq_cmd,      "<id>"),
	SHELL_CMD_ITEM(get_currentvalue, get_currentvalue_cmd, "<id>"),
	SHELL_CMD_ITEM(busywait,         busywait_cmd,         "<id> <us>"),
	SHELL_CMD_ITEM(enable_irq,       enable_irq_cmd,       "<id>"),
	SHELL_CMD_ITEM(disable_irq,      disable_irq_cmd,      "<id>"),
	SHELL_CMD_ITEM_END()
};

SHELL_CMD_GROUP(timer, timer_cmds, "timer command");

#endif

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

