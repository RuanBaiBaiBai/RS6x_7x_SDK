/**
 ******************************************************************************
 * @file    hif_pm.c
 * @brief   hif power manage define.
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
#include "hif_types.h"
#include "hif_config.h"

#if (CONFIG_HIF == 1)
#include "hif_mem.h"
#include "hif_phy.h"
#include "hif_priv.h"
#include "hif_pm.h"

#include "policy.h"


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

#if (CONFIG_HIF_PM == 1)
static uint8_t hifPmWakeType = 0;
static uint8_t hifPmIsLock = 0;
static struct wakelock hifPmDev = {0x00};
#endif

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

static int hif_PM_Callback(void *pDevice, void *arg);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int hif_PM_Init(HIF_PM_InitCfg_t * pInitCfg)
{
    if (pInitCfg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PM == 1)
    hifPmWakeType = pInitCfg->wakeType;

    if (hifPmWakeType == HIF_PM_WAKE_COM) {
        if (hifPHYCtrl.phyPmInit == NULL) {
            return HIF_ERRCODE_INVALID_PARAM;
        }

        status = hifPHYCtrl.phyPmInit(hif_PM_Callback, pInitCfg->wakeComParam);

    } else if (hifPmWakeType == HIF_PM_WAKE_IO) {
        /* add wakeup io */
        status = HIF_ERRCODE_INVALID_PARAM;
    } else {
        status = HIF_ERRCODE_INVALID_PARAM;
    }
#endif


    return status;
}


int hif_PM_Deinit(void)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PM == 1)
    if (hifPmWakeType == HIF_PM_WAKE_COM) {
        if (hifPHYCtrl.phyPmDeinit == NULL) {
            return HIF_ERRCODE_INVALID_PARAM;
        }

        status = hifPHYCtrl.phyPmDeinit();
        hifPmIsLock = 0;
        pm_policy_wake_unlock(&hifPmDev);
    } else if (hifPmWakeType == HIF_PM_WAKE_IO) {
        /* add wakeup io */
        status = HIF_ERRCODE_INVALID_PARAM;
    } else {
        status = HIF_ERRCODE_INVALID_PARAM;
    }
#endif

    return status;
}


__hif_sram_text void HIF_PM_Lock(void)
{
#if (CONFIG_HIF_PM == 1)
    if (hifPmWakeType != HIF_PM_WAKE_DIS) {
        if (!hifPmIsLock) {
            hifPmIsLock = true;
            pm_policy_wake_lock(&hifPmDev);
        }
    }
#endif
}


__hif_sram_text void HIF_PM_Unlock(void)
{
#if (CONFIG_HIF_PM == 1)
    if (hifPmWakeType != HIF_PM_WAKE_DIS) {
        if (hifPmIsLock) {
            hifPmIsLock = false;
            pm_policy_wake_unlock(&hifPmDev);
        }
    }
#endif
}


#if (CONFIG_HIF_PM == 1)
__hif_sram_text static int hif_PM_Callback(void *pDevice, void *arg)
{
    HIF_PM_Lock();

    hif_EVENT_Set(HIF_EVENT_WAKEUP);

    return 0;
}
#endif

#endif

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
