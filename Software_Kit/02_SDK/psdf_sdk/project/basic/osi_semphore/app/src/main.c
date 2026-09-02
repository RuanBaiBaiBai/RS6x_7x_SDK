/**
 ******************************************************************************
 * @file    osi_semphore.c
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
#include "hal_board.h"
#include "hal_os.h"


/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
#define TASK_STACK_SIZE                 512
#define TASK_TIMEOUT                    2000
#define CountSem_Initcnt                0
#define CountSem_Maxcnt                 10

/* Private variables.
 * ----------------------------------------------------------------------------
 */
static TaskHandle_t    task1_handle    = NULL;
static TaskHandle_t    task2_handle    = NULL;
static TaskHandle_t    task3_handle    = NULL;

HAL_Semaphore Task_BinarySemaphore;
HAL_Semaphore Task_CountSemaphore;

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
void task1(void *param);
void task2(void *param);
void task3(void *param);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int main(void)
{
    HAL_Status_t status = HAL_STATUS_OK;

    LOG_PRINT("Hello Word\n");
    LOG_PRINT("My name is possumic\n");
    LOG_PRINT("Nice to meet you\n\n");

    status = HAL_SemaphoreInitBinary(&Task_BinarySemaphore);
    if (status == HAL_STATUS_OK) {
        LOG_PRINT("SemaphoreInitBinary Create Success \n");
    } else {
        LOG_PRINT("SemaphoreInitBinary Create Fail \n");
    }

    status = HAL_SemaphoreInit(&Task_CountSemaphore, CountSem_Initcnt, CountSem_Maxcnt);
    if (status == HAL_STATUS_OK) {
        LOG_PRINT("CountSemaphore Create Success \n\n");
    } else {
        LOG_PRINT("CountSemaphore Create Fail \n");
    }

    xTaskCreate((TaskFunction_t)task1, "task1", TASK_STACK_SIZE, NULL, 2, &task1_handle);
    xTaskCreate((TaskFunction_t)task2, "task2", TASK_STACK_SIZE, NULL, 3, &task2_handle);
    xTaskCreate((TaskFunction_t)task3, "task3", TASK_STACK_SIZE, NULL, 4, &task3_handle);

    while (1) {
        OSI_Sleep(5);
    }

}

//BinarySemaphore & CountSemaphore Release
void task1(void *param)
{
    uint32_t cnt = 0;

    while(1) {
        if (HAL_SemaphoreIsValid(&Task_BinarySemaphore) != 0) {
            LOG_PRINT("task1:BinarySemaphore release %d \n", ++cnt);
            HAL_SemaphoreRelease(&Task_BinarySemaphore);
        } else {
            LOG_PRINT("task1:BinarySemaphore release fail \n");
        }

        if (HAL_SemaphoreIsValid(&Task_CountSemaphore) != 0) {
            LOG_PRINT("task1:CountSemaphore release %d \n", cnt);
            HAL_SemaphoreRelease(&Task_CountSemaphore);
        } else {
            LOG_PRINT("task1:CountSemaphore release fail \n");
        }

        if( cnt >= CountSem_Maxcnt) {
            LOG_PRINT("test Semaphore success \n");
            HAL_SemaphoreDeinit(&Task_BinarySemaphore);
            HAL_SemaphoreDeinit(&Task_CountSemaphore);
            HAL_SemaphoreSetInvalid(&Task_BinarySemaphore);
            HAL_SemaphoreSetInvalid(&Task_CountSemaphore);
            vTaskDelete(task3_handle);
            vTaskDelete(task2_handle);
            vTaskDelete(task1_handle);
        }
        vTaskDelay(1000);
    }
}

//BinarySemaphore take
void task2(void *param)
{
    HAL_Status_t status =  HAL_STATUS_ERROR;
    uint32_t cnt = 0;

    while(1) {
        status = HAL_SemaphoreWait(&Task_BinarySemaphore, TASK_TIMEOUT);
        if (HAL_STATUS_OK != status) {
            LOG_PRINT("task2:BinarySemaphore Timeout");
        } else {
            LOG_PRINT("task2:BinarySemaphore take %d \n\n", ++cnt);
        }
    }
    vTaskDelay(100);
}

//CountSemaphore take
void task3(void *param)
{
    HAL_Status_t status =  HAL_STATUS_ERROR;
    uint32_t cnt = 0;

    while(1) {
        status = HAL_SemaphoreWait(&Task_CountSemaphore, TASK_TIMEOUT);
        if (HAL_STATUS_OK != status) {
            LOG_PRINT("task3:Task_CountSemaphore Timeout");
        } else {
            LOG_PRINT("task3:Task_CountSemaphore take %d \n\n", ++cnt);
        }
    }
    vTaskDelay(100);
    }

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
