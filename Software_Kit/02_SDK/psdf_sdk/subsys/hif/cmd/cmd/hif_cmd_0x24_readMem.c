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

#if ((CONFIG_HIF_MSG_READ_MEM == 1) && (CONFIG_HIF_CMD == 1))

/**
 * @brief Data(received from host) type
 */
typedef struct hif_cmd_readMem_t_ {
    uint32_t addr;
    uint16_t len;
    uint16_t flag;      /*< bit0: 1 - read register; 0 - not register; other reserved */
} hif_cmd_readMem_t;


/**
 * @brief readMem msg handler
 * @param msg 
 * @return int 
 */
int hif_msghdl_readMem(HIF_MsgHdr_t *msg)
{
    uint8_t cmd_status = HIF_CMD_STATUS_SUCCESS;
    int readLen = 0;
    HIF_MsgGenAck_t *ack = NULL;
    hif_cmd_readMem_t *readMem = NULL;
    uint8_t *payloadAddr = NULL;

    if (msg->length < sizeof(hif_cmd_readMem_t)) {
        cmd_status = HIF_CMD_STATUS_PARAM;
    } else {
        payloadAddr = (uint8_t *)(msg + 1);
        if (msg->flag & HIF_MSG_FLAG_EXTEND_BIT) {
            payloadAddr += sizeof(HIF_MsgExtHdr_t);
        }

        readMem = (hif_cmd_readMem_t *)payloadAddr;
        ack = (HIF_MsgGenAck_t *)payloadAddr;
        readLen = readMem->len;

        HIF_LOG_DBG("hif msg cmd: %s, addr: 0x%08x, len: %u, flag: 0x%04x\n", __func__, readMem->addr, readMem->len, readMem->flag);

        if (readLen > CONFIG_HIF_CMD_PAYLOAD_MAX_SIZE) {
            cmd_status  = HIF_CMD_STATUS_PARAM;
            readLen = 0;
        } else {
            if (readMem->flag & HW_BIT(0)) {        // read register
                readLen = ALIGN_UP(readLen, 4);
            }
            // 读取的数据拷贝在 HIF 消息 payload 的 sizeof(HIF_MsgGenAck_t) 之后
            memcpy((uint8_t *)ack + sizeof(HIF_MsgGenAck_t), (uint8_t *)(readMem->addr), readLen);
        }
    }

    return HIF_MsgResp(msg, readLen, cmd_status);
}

/**
 * @brief Init hif msghdl readMem
 */
int hif_msghdl_readMem_init(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HIF_MsgHdl_Regist(HIF_MSG_ID_READ_MEM, hif_msghdl_readMem);

    return status;
}

#endif

