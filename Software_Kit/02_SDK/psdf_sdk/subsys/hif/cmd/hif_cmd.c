/**
 ******************************************************************************
 * @file    hif_cmd.h
 * @brief   hif_cmd.
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


#include "hif_cmd.h"
#include "hif_log.h"

#if CONFIG_HIF_CMD

/**
 * @brief Init hif commands.
 * @return int: The number of msg that init failed.
 * @attention This function should be called after HIF_Init.
 */
int hif_cmd_init(void)
{
    int ret = 0;
    int status = 0;

#if (CONFIG_HIF_MSG_FEXEC == 1)
    status = hif_msghdl_fexec_init();
    if (status != HIF_ERRCODE_SUCCESS) {
        HIF_LOG_WRN("Init hif msghdl fexec fail, errCode %d\n", status);
        ret++;
    }
#endif

#if (CONFIG_HIF_MSG_READ_MEM == 1)
    status = hif_msghdl_readMem_init();
    if (status != HIF_ERRCODE_SUCCESS) {
        HIF_LOG_WRN("Init hif msghdl readMem fail, errCode %d\n", status);
        ret++;
    }
#endif

#if (CONFIG_HIF_MSG_WRITE_MEM == 1)
    status = hif_msghdl_writeMem_init();
    if (status != HIF_ERRCODE_SUCCESS) {
        HIF_LOG_WRN("Init hif msghdl writeMem fail, errCode %d\n", status);
        ret++;
    }
#endif

    return ret;
}

#endif

