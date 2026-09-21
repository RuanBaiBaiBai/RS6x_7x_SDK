/**
 ******************************************************************************
 * @file    hif_msg_0x25_wirteMem.c
 * @brief   hif_msg_0x25_wirteMem function define.
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

#if ((CONFIG_HIF_MSG_WRITE_MEM == 1) && (CONFIG_HIF_CMD == 1))

/**
 * @brief Data(received from host) type
 */
typedef struct hif_cmd_writeMem_t_ {
    uint32_t addr;
    uint16_t len;
    uint16_t flag;      /*< bit0: 1 - read register; 0 - not register; other reserved */
    uint8_t data[1];
} hif_cmd_writeMem_t;

/**
 * @brief readMem msg handler
 * @param msg 
 * @return int 
 */
int hif_msghdl_writeMem(HIF_MsgHdr_t *msg)
{
    uint8_t cmd_status = HIF_CMD_STATUS_SUCCESS;
    uint8_t *payloadAddr = NULL;
    hif_cmd_writeMem_t *writeMem = NULL;

    if (msg->length < sizeof(hif_cmd_writeMem_t)) {
        cmd_status = HIF_CMD_STATUS_PARAM;
    } else {
        payloadAddr = (uint8_t *)(msg + 1);
        if (msg->flag & HIF_MSG_FLAG_EXTEND_BIT) {
            payloadAddr += sizeof(HIF_MsgExtHdr_t);
        }

        writeMem = (hif_cmd_writeMem_t *)payloadAddr;

        HIF_LOG_DBG("hif msg: %s, addr: 0x%08x, len: %u, flag: 0x%04X\n", __func__, writeMem->addr, writeMem->len, writeMem->flag);
        
        memcpy((uint8_t *)(writeMem->addr), &(writeMem->data[0]), writeMem->len);
    }

    return HIF_MsgResp(msg, 0, cmd_status);
}

/**
 * @brief Init hif msghdl writeMem
 */
int hif_msghdl_writeMem_init(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HIF_MsgHdl_Regist(HIF_MSG_ID_WRITE_MEM, hif_msghdl_writeMem);

    return status;
}

#endif

