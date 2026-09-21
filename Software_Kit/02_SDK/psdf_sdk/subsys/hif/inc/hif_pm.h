/**
 ******************************************************************************
 * @file    hif_pm.h
 * @brief   hif power manage define.
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


#ifndef _HIF_PM_H
#define _HIF_PM_H

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "hif_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types.
 * ----------------------------------------------------------------------------
 */

typedef struct {
        #define HIF_PM_WAKE_DIS         0
        #define HIF_PM_WAKE_COM         1
        #define HIF_PM_WAKE_IO          2
    uint8_t     wakeType;

        #define HIF_WAKE_IO_L           0
        #define HIF_WAKE_IO_H           1
    uint8_t     wakeIoLevel;
    uint8_t     wakeIoNum;
    uint8_t     wakeComParam;

} HIF_PM_InitCfg_t;


/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int hif_PM_Init(HIF_PM_InitCfg_t * pInitCfg);

int hif_PM_Deinit(void);

void HIF_PM_Lock(void);

void HIF_PM_Unlock(void);


#ifdef __cplusplus
}
#endif

#endif //_HIF_PM_H


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
