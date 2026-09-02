/**
 * @file hif_msg_0x23_fexec.c
 * @brief
 * @version 1.0
 * @date 2025-12-30 17:28:18
 * @author xxx (xxx@possumic.com)
 * @changes:
 * 
 * @attention
 * 
 * Copyright (C) 2025 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of POSSUMIC TECHNOLOGY CO., LTD. nor the names of
 *      its contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "hif_cmd.h"
#include "hif_log.h"

#if ((CONFIG_HIF_MSG_FEXEC == 1) && (CONFIG_HIF_CMD == 1))

/**
 * @brief Function type that fexec executes
 */
typedef int (*hif_cmd_fexec_func_t) (uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                      uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

/**
 * @brief Data(received from host) type
 */
typedef struct hif_cmd_fexec_t_ {
    uint32_t addr;          /**< Function address */
    uint16_t len;           /**< Param lenght(unit: byte) */
    uint16_t flag;          /**< Param type; Bitwise Parse; \
                                 bitx: 0 - The x-th param is unsigned int, 1 - The x-th param is string */
    uint32_t param[1];      /**< The 1st param */
} hif_cmd_fexec_t;

/**
 * @brief fexec msg handler
 * @param msg 
 * @return int 
 */
int hif_msghdl_fexec(HIF_MsgHdr_t *msg)
{
    uint8_t cmd_status = HIF_CMD_STATUS_SUCCESS;
    uint32_t funcRetVal = 0;
    hif_cmd_fexec_t *fexec = NULL;
    int paramNum = 0;
    uint32_t param[HIF_MSG_FEXEC_PARAM_MAX_NUM] = {0};
    hif_cmd_fexec_func_t func  = NULL;
    uint8_t *payloadAddr = NULL;
    HIF_MsgGenAck_t *ack = NULL;

    payloadAddr = (uint8_t *)(msg + 1);
    if (msg->flag & HIF_MSG_FLAG_EXTEND_BIT) {
        payloadAddr += sizeof(HIF_MsgExtHdr_t);
    }

    fexec = (hif_cmd_fexec_t *)payloadAddr;
    func  = (hif_cmd_fexec_func_t)(fexec->addr);
    ack = (HIF_MsgGenAck_t *)payloadAddr;

    if ((msg->length >= 9) || (msg->length == 4)) {
        if (msg->length >= 9) {
            int len        = fexec->len;
            uint16_t flag  = fexec->flag;
            uint32_t *argv = &(fexec->param[0]);                        // Address of 1st param

            if ((len < 4) && likely(!(flag & 0x1))) {    // fexec->len < 4 and type of param is uint32_t, tha is invalid
                HIF_LOG_ERR("hif mgs fexec 0x23, invalid param\n");
                cmd_status = HIF_CMD_STATUS_PARAM;

                goto hif_msghdl_fexec_return;                           // end
            } else {
                while (len > 0) {
                    if (likely(!(flag & 0x1))) {                        // unsigned int param
                        param[paramNum] = *argv;
                        argv++;
                        len -= 4;
                    } else {                                            // string param
                        param[paramNum] = (uint32_t)argv;               // Get address of string

                        int str_len = strlen((char *)argv) + 1;         // str end with '\0'
                        argv = (uint32_t *)((uint32_t)argv + str_len);

                        len -= str_len;
                    }

                    if (++paramNum >= HIF_MSG_FEXEC_PARAM_MAX_NUM) {
                        HIF_LOG_ERR("hif mgs fexec 0x23, too many param\n");
                        cmd_status = HIF_CMD_STATUS_TOOLONG;
                        break;
                    }
                    flag = flag>>1;
                }
            }
        }
        funcRetVal = (uint32_t)func(param[0], param[1], param[2], param[3], param[4],
            param[5], param[6], param[7], param[8], param[9]);          // Execute function

        HIF_LOG_DBG("fexec: func: 0x%08x; argvs: %d; funcRetVal: 0x%08X\n", (uint32_t)func, paramNum, funcRetVal);
    } else {
        HIF_LOG_ERR("hif mgs fexec 0x23, invalid param\n");
        cmd_status = HIF_CMD_STATUS_PARAM;
    }

hif_msghdl_fexec_return:
    // funcRetVal as response to host
    memcpy((uint8_t *)ack + sizeof(HIF_MsgGenAck_t), (uint8_t *)&funcRetVal, sizeof(funcRetVal));
    return HIF_MsgResp(msg, sizeof(funcRetVal), cmd_status);
}

/**
 * @brief Init hif msghdl fexec
 */
int hif_msghdl_fexec_init(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HIF_MsgHdl_Regist(HIF_MSG_ID_FEXEC, hif_msghdl_fexec);

    return status;
}

#endif

