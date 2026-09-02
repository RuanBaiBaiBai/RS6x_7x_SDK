/**
 ******************************************************************************
 * @file    hif_dll.c
 * @brief   hif data link layer define.
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

#include "hif_dll.h"
#include "hif_phy.h"
#include "hif_mem.h"
#include "ll_utils.h"

#include "hif_log.h"


/* Private typedef.
 * ----------------------------------------------------------------------------
 */

typedef struct {
    HIF_DLL_TransCallback_t     tlSendCb;
    HIF_DLL_TransCallback_t     tlRecvCb;

    HIF_Frame_DoubleHead_t      sendFrameList;
    HIF_Frame_Node_t            *pRecvFrame;

    int32_t                     remLen;
    int32_t                     cascadeThresh;

        #define HIF_DLL_SEND_TYPE_NULL         0
        #define HIF_DLL_SEND_TYPE_WAKEUP       1
        #define HIF_DLL_SEND_TYPE_CMDRSP       2
        #define HIF_DLL_SEND_TYPE_REPORT       3
    uint8_t                     sendType;
} HIF_DLL_t;


static HIF_DLL_t hifDLLCtrl;

/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private macros.
 * ----------------------------------------------------------------------------
 */
static int hif_DLL_AddSendFrameAllow(HIF_Frame_Node_t * pNewFrameNode);
static int hif_DLL_SendCb(void *pData, uint16_t size, uint32_t flag);
static int hif_DLL_RecvCb(void *pData, uint16_t size, uint32_t flag);
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

int hif_DLL_Init(HIF_DLL_InitCfg_t *pInitCfg)
{
    int status = HIF_ERRCODE_SUCCESS;

    if (pInitCfg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if ((pInitCfg->sendCb == NULL) || (pInitCfg->recvCb == NULL)) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    hifDLLCtrl.tlSendCb     = pInitCfg->sendCb;
    hifDLLCtrl.tlRecvCb     = pInitCfg->recvCb;


    hif_FRAME_Init(&hifDLLCtrl.sendFrameList);
    hifDLLCtrl.pRecvFrame = NULL;

    status = hif_PHY_Init(pInitCfg->pPHYCfg);
    if (status) {
        return HIF_ERRCODE_IO_ERROR;
    }

    hif_PHY_RegisterCallback(HIF_TRANS_SEND_CB_ID, hif_DLL_SendCb);
    hif_PHY_RegisterCallback(HIF_TRANS_RECV_CB_ID, hif_DLL_RecvCb);

    hifDLLCtrl.cascadeThresh = pInitCfg->cascadeThresh;

    hifDLLCtrl.sendType = HIF_DLL_SEND_TYPE_NULL;

    return HIF_ERRCODE_SUCCESS;
}


int hif_DLL_Deinit(void)
{
    /* add dll deinit process */

    return hif_PHY_Deinit();
}


int hif_DLL_Open(void)
{
    return hif_PHY_Open();
}


int hif_DLL_Close(void)
{
    return hif_PHY_Close();
}


__hif_sram_text int hif_DLL_Send(HIF_Frame_Node_t * pNewFrameNode)
{
#if (CONFIG_HIF_SEND == 1)

    int status = HIF_ERRCODE_SUCCESS;

    unsigned long key = __lock_irq();

    /* append frame node to send list */
    do {
        HIF_Frame_Node_t *pLastFrameNode = hif_FRAME_PeekTail(&hifDLLCtrl.sendFrameList);

        if (hif_DLL_AddSendFrameAllow(pNewFrameNode) != HIF_ERRCODE_SUCCESS) {
            if (hif_PHY_GetPhyType() == HIF_PHY_TYPE_UART) {
                status = HIF_ERRCODE_NOT_READY_TMP;
            } else {
                status = HIF_ERRCODE_NOT_READY;
            }
            break;
        }


        if ((pNewFrameNode->flag & HIF_FRAME_FLAG_END_FRAME) == 0) {
            /* new frame is data frame
             *
             * if last frame is NULL, append
             * if last frame is data frame, append
             * if last frame is End_Frame, insert new frame(data frame) to End_Frame prev
             */

            /* if last frame is NULL, append */
            if (pLastFrameNode == NULL) {
                hif_FRAME_Append(&hifDLLCtrl.sendFrameList, pNewFrameNode);

            /* if last frame is End_Frame, insert new frame(data frame) to End_Frame prev */
            } else if (pLastFrameNode->flag & HIF_FRAME_FLAG_END_FRAME) {

                /* pop End_Frame */
                HIF_Frame_Node_t *pEndFrame = hif_FRAME_PopTail(&hifDLLCtrl.sendFrameList);
                /* get new last frame node */
                pLastFrameNode = hifDLLCtrl.sendFrameList.tail;

                if (pLastFrameNode != NULL) {
                    /* append new frame node */
                    pLastFrameNode->listHead.tail->next = pNewFrameNode->listHead.head;
                    hif_FRAME_Append(&hifDLLCtrl.sendFrameList, pNewFrameNode);

                    /* data node link End_Frame data node */
                    pNewFrameNode->listHead.tail->next = pEndFrame->listHead.head;

                    /* link End_Frame node */
                    hif_FRAME_Append(&hifDLLCtrl.sendFrameList, pEndFrame);
                } else {
                    /* link End_Frame node */
                    hif_FRAME_Append(&hifDLLCtrl.sendFrameList, pEndFrame);

                    if (hif_PHY_GetPhyType() == HIF_PHY_TYPE_UART) {
                        status = HIF_ERRCODE_NOT_READY_TMP;
                    } else {
                        status = HIF_ERRCODE_NOT_READY;
                    }
                    break;
                }
            /* if last frame is data frame, append */
            } else {

                pLastFrameNode->listHead.tail->next = pNewFrameNode->listHead.head;
                hif_FRAME_Append(&hifDLLCtrl.sendFrameList, pNewFrameNode);
            }

            hifDLLCtrl.remLen += pNewFrameNode->frameLen;


        } else {
            /* new frame node is End_Frame
             *
             * if last frame is NULL, append
             * if last frame is End_Frame, nothing to do
             * if last frame is data frame, append (if add frame allow)
             */
            /* if last frame is NULL, append */
            if (pLastFrameNode == NULL) {
                hif_FRAME_Append(&hifDLLCtrl.sendFrameList, pNewFrameNode);

            } else if (pLastFrameNode->flag & HIF_FRAME_FLAG_END_FRAME) {
                /* There's nothing to do */
                break;

            /* if last frame is data frame, append */
            } else {
                pLastFrameNode->listHead.tail->next = pNewFrameNode->listHead.head;
                hif_FRAME_Append(&hifDLLCtrl.sendFrameList, pNewFrameNode);
            }

            hifDLLCtrl.remLen += pNewFrameNode->frameLen;
        }

    } while (0);


    __unlock_irq((unsigned long)key);

    /* start transmit */
    if (status != HIF_ERRCODE_SUCCESS) {
        HIF_LOG_DBG("send fail: %d", status);
        return status;
    }


    if (pNewFrameNode->flag & (HIF_FRAME_FLAG_END_FRAME | HIF_FRAME_FLAG_CMD_FRAME | HIF_TRANS_FLAG_LAST_FRAME)) {
        if (hif_PHY_TransIsIdle()) {
            HIF_Frame_Node_t *pFirstFrameNode = hif_FRAME_PeekHead(&hifDLLCtrl.sendFrameList);
            HIF_Data_Node_t * pDataNode = hif_DATA_PeekHead(&pFirstFrameNode->listHead);

            if (pFirstFrameNode->flag & HIF_FRAME_FLAG_CMD_FRAME) {
                hifDLLCtrl.sendType = HIF_DLL_SEND_TYPE_CMDRSP;
            } else {
                hifDLLCtrl.sendType = HIF_DLL_SEND_TYPE_REPORT;
            }

            hif_PHY_SendStart(pDataNode);

        }
    }

    return HIF_ERRCODE_SUCCESS;

#else

    return HIF_ERRCODE_NOT_READY;

#endif
}


__hif_isr_text int hif_DLL_SendFlush(void)
{

#if (CONFIG_HIF_SEND == 1)
    HIF_Frame_Node_t * pFrameNode;

    unsigned long key = __lock_irq();

    pFrameNode = hif_FRAME_PeekHead(&hifDLLCtrl.sendFrameList);
    if (pFrameNode == NULL) {
        __unlock_irq((unsigned long)key);
        return HIF_ERRCODE_SUCCESS;
    }


    int status = hif_PHY_SendFlush();
    if (status != HIF_ERRCODE_SUCCESS) {
        __unlock_irq((unsigned long)key);
        return HIF_ERRCODE_NOT_READY;
    }

    do {
        pFrameNode = hif_FRAME_PopHead(&hifDLLCtrl.sendFrameList);
        if (pFrameNode == NULL) {
            break;
        }

        pFrameNode->status = HIF_ERRCODE_TIMEOUT;

        if ((pFrameNode->flag & (HIF_FRAME_FLAG_END_FRAME | HIF_FRAME_FLAG_CMD_FRAME)) == 0) {
            hifDLLCtrl.tlSendCb(pFrameNode, HIF_DLL_FLAG_SEND_FLUSH);
        }
    } while (1);

    hifDLLCtrl.remLen = 0;

    __unlock_irq((unsigned long)key);

    return HIF_ERRCODE_SUCCESS;
#endif
}


__hif_isr_text void hif_DLL_SendWakeupResp(HIF_Data_Node_t * pDataNode)
{
    hifDLLCtrl.sendType = HIF_DLL_SEND_TYPE_WAKEUP;
    hif_PHY_SendWakeupResp(pDataNode);
}

__hif_isr_text int hif_DLL_Recv(HIF_Frame_Node_t * pFrameNode)
{
#if (CONFIG_HIF_RECV == 1)

    HIF_Data_Node_t * pDataNode = pFrameNode->listHead.head;

    hifDLLCtrl.pRecvFrame = pFrameNode;

    return hif_PHY_RecvStart(pDataNode);

#else

    return HIF_ERRCODE_NOT_READY;

#endif
}

__hif_sram_text int hif_DLL_EndFrameIsBusy(void)
{
    int status = 0;

    unsigned long key = __lock_irq();

    do {
        HIF_Frame_Node_t *pLastFrameNode = hif_FRAME_PeekTail(&hifDLLCtrl.sendFrameList);
        if (pLastFrameNode == NULL) {
            break;
        }

        if ((pLastFrameNode->flag & HIF_FRAME_FLAG_END_FRAME) != 0) {
            status = 1;
        }
    } while (0);

    __unlock_irq((unsigned long)key);

    return status;
}

__hif_sram_text void hif_DLL_EndFrameRemove(void)
{
    unsigned long key = __lock_irq();

    do {
        HIF_Frame_Node_t *pLastFrameNode = hif_FRAME_PeekTail(&hifDLLCtrl.sendFrameList);
        if (pLastFrameNode == NULL) {
            break;
        }

        if ((pLastFrameNode->flag & HIF_FRAME_FLAG_END_FRAME) != 0) {
            hif_FRAME_PopTail(&hifDLLCtrl.sendFrameList);
            hifDLLCtrl.remLen -= pLastFrameNode->frameLen;
            pLastFrameNode = hif_FRAME_PeekTail(&hifDLLCtrl.sendFrameList);
            pLastFrameNode->listHead.tail->next = NULL;
        }
    } while (0);

    __unlock_irq((unsigned long)key);
}

__hif_sram_text static int hif_DLL_AddSendFrameAllow(HIF_Frame_Node_t * pNewFrameNode)
{
#if (CONFIG_HIF_SEND == 1)

    if (((pNewFrameNode->flag & HIF_FRAME_FLAG_CMD_FRAME)
            && (hifDLLCtrl.sendType == HIF_DLL_SEND_TYPE_REPORT)) ||
        (((pNewFrameNode->flag & HIF_FRAME_FLAG_CMD_FRAME) == 0)
            && (hifDLLCtrl.sendType == HIF_DLL_SEND_TYPE_CMDRSP))) {
        return HIF_ERRCODE_NOT_READY;
    }

    /* if trans done */
    if (hif_PHY_TransIsIdle()) {
        return HIF_ERRCODE_SUCCESS;
    } else {
        HIF_Frame_Node_t *pFrameNode = hifDLLCtrl.sendFrameList.head;
        if (pFrameNode == NULL) {
            return HIF_ERRCODE_NOT_READY;
        }

        int32_t remTransLen = hifDLLCtrl.remLen - pFrameNode->frameLen;
        if (remTransLen > hifDLLCtrl.cascadeThresh) {
            return HIF_ERRCODE_SUCCESS;
        } else {
            return HIF_ERRCODE_NOT_READY;
        }
    }

#else

    return HIF_ERRCODE_NOT_READY;

#endif
}


__hif_isr_text static int hif_DLL_SendCb(void *pData, uint16_t size, uint32_t flag)
{
#if (CONFIG_HIF_SEND == 1)

    int status = HIF_ERRCODE_SUCCESS;
    HIF_Frame_Node_t *pFrameNode = NULL;
    HIF_Data_Node_t *pDataNode = NULL;


    if (hifDLLCtrl.sendType == HIF_DLL_SEND_TYPE_WAKEUP) {
        if (flag & HIF_PHY_FLAG_SEND_DONE) {
            hifDLLCtrl.sendType = HIF_DLL_SEND_TYPE_NULL;
            hif_PHY_SendStop();
            hifDLLCtrl.tlSendCb(NULL, HIF_DLL_FLAG_SEND_WAKE);
        }
        return status;
    }

    if (flag & HIF_PHY_FLAG_SEND_PART) {
        do {
            pFrameNode = hif_FRAME_PeekHead(&hifDLLCtrl.sendFrameList);
            if (pFrameNode == NULL) {
                break;
            }

            /* check data node trans done */
            pDataNode = hif_DATA_PeekTail(&pFrameNode->listHead);
            if (pDataNode == NULL) {
                break;
            }

            if ((pDataNode->len & HIF_PHY_TRANS_DONE_MSK) == 0 && (flag & HIF_PHY_FLAG_SEND_FLUSH) == 0) {
                /* not trans done */
                break;
            }

            /* Remove the currently completed frame from the send frame list */
            hif_FRAME_PopHead(&hifDLLCtrl.sendFrameList);
            pDataNode->next = NULL;
            hifDLLCtrl.remLen -= pFrameNode->frameLen;
            if ((pFrameNode->flag & (HIF_FRAME_FLAG_END_FRAME | HIF_FRAME_FLAG_CMD_FRAME)) == 0) {
                if (flag & HIF_PHY_FLAG_SEND_FLUSH) {
                    hifDLLCtrl.tlSendCb(pFrameNode, HIF_DLL_FLAG_SEND_FLUSH);
                } else {
                    hifDLLCtrl.tlSendCb(pFrameNode, HIF_DLL_FLAG_SEND_FRAME);
                }
            }
        } while (1);
    }

    if (flag & HIF_PHY_FLAG_SEND_DONE) {

        if (hifDLLCtrl.sendType == HIF_DLL_SEND_TYPE_REPORT) {
            hifDLLCtrl.tlSendCb(NULL, HIF_DLL_FLAG_SEND_RPT_DONE);
        } else {
            hifDLLCtrl.tlSendCb(NULL, HIF_DLL_FLAG_SEND_RSP_DONE);
        }

        hifDLLCtrl.sendType = HIF_DLL_SEND_TYPE_NULL;
    }

    return status;

#else

    return HIF_ERRCODE_NOT_READY;

#endif
}

__hif_isr_text  static int hif_DLL_RecvCb(void *pData, uint16_t size, uint32_t flag)
{
#if (CONFIG_HIF_RECV == 1)


    if (flag == HIF_PHY_FLAG_RECV_DONE) {
        if (hifDLLCtrl.pRecvFrame != NULL) {
            hifDLLCtrl.pRecvFrame->status = HIF_ERRCODE_SUCCESS;
            HIF_Data_Node_t * pDataNode = hifDLLCtrl.pRecvFrame->listHead.head;
            if (pDataNode != NULL) {
                pDataNode->data = pData;
                pDataNode->len = size;
            }
        }

        hifDLLCtrl.tlRecvCb(hifDLLCtrl.pRecvFrame, HIF_DLL_FLAG_RECV_FRAME);

    } else if (flag == HIF_PHY_FLAG_RECV_WAKE) {
        hifDLLCtrl.tlRecvCb(NULL, HIF_DLL_FLAG_RECV_WAKE);

    } else if (flag == HIF_PHY_FLAG_RECV_FAIL) {
        uint8_t *pStatus = (uint8_t *)pData;
        hifDLLCtrl.pRecvFrame->status = pStatus[0];
        hifDLLCtrl.tlRecvCb(hifDLLCtrl.pRecvFrame, HIF_DLL_FLAG_RECV_FAIL);
    }

    return HIF_ERRCODE_SUCCESS;

#else

    return HIF_ERRCODE_NOT_READY;

#endif
}


#endif /* CONFIG_HIF */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
