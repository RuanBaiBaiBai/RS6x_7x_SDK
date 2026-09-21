/**
 ******************************************************************************
 * @file    cmd_sys.c
 * @brief   cmd system define.
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
#include "ll_utils.h"
#include "ll_rtctick.h"
#include "ll_rcc_dev.h"
#include "ll_rcc_bus.h"
#include "cmd_config.h"
#include "ll_wdg.h"

#if ((CONFIG_SHELL_CMD_SYS == 1) && (CONFIG_SHELL == 1))

#include "hal_board.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define SYS_REBOOT_WARM     0
#define SYS_REBOOT_COLD     1
#define SYS_UPGRADE         3

/* Private macros.
 * ----------------------------------------------------------------------------
 */
/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */
void sys_arch_reboot(int type)
{
    /*
     * 1> SYS_REBOOT_COLD:
     *        a.set sys reset flag
     *        b.reset all sys(clear first addr)
     *        c.wait for all sys reset
     *
     * 2> SYS_REBOOT_WARM(not support):
     *        a.set curr cpu reset flag
     *        b.reset curr cpu(not clear first addr)
     *        c.wait for curr cpu reset
     *
     * 3> SYS_UPGRADE:
     *        a.set sys upgrade flag
     *        b.reset all sys(clear first addr)
     *        c.into upgrade
     */

    uint32_t cpu_reset_flag = 0;

    LL_BOOT_SetFirstAddr(0);

    if ((type == SYS_REBOOT_COLD) || (type == SYS_REBOOT_WARM)) {
        cpu_reset_flag = LL_BOOT_SYS_FLAG_RESET;
    } else {
        cpu_reset_flag = LL_BOOT_SYS_FLAG_UPGRADE;
    }

#if (CONFIG_BOARD_MRS6240_P2512_CPUF || CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUF)
    LL_INTR_SetCPUSMsk(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
    LL_WKUP_DisableOnSys0(0xFFFFFFFF);
    LL_INTR_SetCPUFMsk(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
    LL_WKUP_DisableOnSys1(0xFFFFFFFF);

    LL_RTCTICK_CMP1_Disable();
    LL_RTCTICK_CMP0_Disable();

    LL_WDT_Disable(WDG0_DEV);
    LL_WDT_Disable(WDG1_DEV);

    LL_RCC_SYS_SetClockSource(0, LL_RCC_SYS_SRC_MSI);
    LL_RCC_SYS_SetClockSource(1, LL_RCC_SYS_SRC_MSI);
    LL_BOOT_SetCPUSFlag(cpu_reset_flag);
    LL_BOOT_SetFirstAddr(0);

    LL_SYS_ForceUpSys0();
    LL_SYS_ForceDownSys1();
    while(1);
#else
    LL_BOOT_SetCPUSFlag(cpu_reset_flag);
    LL_WDG0_SetResetPatition(LL_WDG_RESET_CTRL_SYS);
    /* disable wakeup timer */
    LL_RTCTICK_CMP1_Disable();
    LL_RTCTICK_CMP0_Disable();

    LL_BOOT_ResetEnable();

#endif
    return;
}


static int cmd_sys_reset(Shell *shell, int argc, char *argv[])
{
    HAL_BOARD_Reset(HAL_RESET_SYS);
    return HAL_STATUS_OK;
}

static int cmd_sys_upgrade(Shell *shell, int argc, char *argv[])
{
    sys_arch_reboot(SYS_UPGRADE);
    return HAL_STATUS_OK;
}

SHELL_CMD(reboot, cmd_sys_reset, "reset system");
SHELL_CMD(upgrade, cmd_sys_upgrade, "enter upgrade mode");

#endif /* CONFIG_SHELL_CMD_SYS */




/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
