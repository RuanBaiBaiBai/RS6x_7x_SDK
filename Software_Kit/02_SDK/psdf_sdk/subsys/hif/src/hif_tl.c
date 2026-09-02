/**
 ******************************************************************************
 * @file    hif_tl.c
 * @brief   hif_tl define.
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
#include "hif_tl.h"
#include "hif_priv.h"
#include "hif_dll.h"


/* Private typedef.
 * ----------------------------------------------------------------------------
 */
#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
typedef struct _hif_tl_ctrl_t_ {
    OSI_Timer_t waitAckTimer;
} hif_tl_ctrl_t;

static hif_tl_ctrl_t hif_tl_ctrl = {0};

/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static OSI_Status_t hif_tl_create_wait_ack_timer(void);
static OSI_Status_t hif_tl_del_wait_ack_timer(void);
#endif      /* CONFIG_HIF_TL_ACK_TIMER_ENABLE */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

/*** ++++++++++++++++++++++++++ Init and Deinit of transport-layer ++++++++++++++++++++++++++ ***/
#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
/**
 * @brief Transpost-layer init
 * @return int8_t
 */
int8_t hif_tl_init(void)
{
    int8_t retVal = HIF_ERRCODE_SUCCESS;

    if (hif_tl_create_wait_ack_timer() != HIF_ERRCODE_SUCCESS) {
        retVal = HIF_ERRCODE_FAIL;
    }

    return retVal;
}

int8_t hif_tl_deInit(void)
{
    int8_t retVal = HIF_ERRCODE_SUCCESS;

    if (hif_tl_del_wait_ack_timer() != HIF_ERRCODE_SUCCESS) {
        retVal = HIF_ERRCODE_FAIL;
    }

    return retVal;
}
#endif      /* CONFIG_HIF_TL_ACK_TIMER_ENABLE */
/*** ========================== Init and Deinit of transport-layer ========================= ***/


/*** ++++++++++++++++++++++++++ Associated with waiting-ack timer ++++++++++++++++++++++++++ ***/
#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
/**
 * @brief Wait-ack-timer's callback
 * @param arg
 */
static void hif_tl_ack_timer_callback(void *arg)
{
    hif_tl_ack_timer_stop();

    HIF_Frame_Node_t *frame = NULL;
    HIF_Frame_Node_t *frameFree = NULL;

    unsigned long key = __lock_irq();

    frame = hifCtrl.ackPending.head;
    while (frame) {
        if (hif_tl_frame_isTimeout(frame)) {
            frameFree = frame;
            frame = frame->next;
            frameFree->status = HIF_ERRCODE_TIMEOUT;
            frameFree = hif_frameQueue_remove_frame(frameFree, &(hifCtrl.ackPending));
            hif_FRAME_Append(&(hifCtrl.freePending), frameFree);
        } else {
            frame = frame->next;
        }
    }
    __unlock_irq((unsigned long)key);

    if (hifCtrl.freePending.head != NULL) {
        OSI_SemaphoreRelease(&hifCtrl.semProc);
    }

    if (hifCtrl.ackPending.head != NULL) {
        hif_tl_ack_timer_start(CONFIG_HIF_TL_ACK_TIMEOUT_MS);
    }
}

/**
 * @brief Create timer for message-layer to wait ack
 * @return OSI_Status_t
 */
static OSI_Status_t hif_tl_create_wait_ack_timer(void)
{
    return OSI_TimerCreate(&(hif_tl_ctrl.waitAckTimer),
                           OSI_TIMER_ONCE,
                           hif_tl_ack_timer_callback,
                           NULL,
                           CONFIG_HIF_TL_ACK_TIMEOUT_MS);
}

/**
 * @brief Create timer for message-layer to wait ack
 * @return OSI_Status_t
 */
static OSI_Status_t hif_tl_del_wait_ack_timer(void)
{
    return OSI_TimerDelete(&(hif_tl_ctrl.waitAckTimer));
}


/**
 * @brief Start (hif_tl_ctrl.waitAckTimer)
 * @param timeoutMs The expired time(ms)
 * @return OSI_Status_t
 */
OSI_Status_t hif_tl_ack_timer_start(uint32_t timeoutMs)
{
    OSI_Status_t retVal = OSI_STATUS_OK;

    if (OSI_TimerIsActive(&(hif_tl_ctrl.waitAckTimer))) {
        return retVal;
    }

    retVal = OSI_TimerChangePeriod(&(hif_tl_ctrl.waitAckTimer), timeoutMs);

    if (retVal != OSI_STATUS_OK) {
        return retVal;
    }

    return OSI_TimerStart(&(hif_tl_ctrl.waitAckTimer));
}

OSI_Status_t hif_tl_ack_timer_stop(void)
{
    return OSI_TimerStop(&(hif_tl_ctrl.waitAckTimer));
}
#endif      /* CONFIG_HIF_TL_ACK_TIMER_ENABLE */


/*** ========================== Associated with waiting-ack timer ========================= ***/

#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
/**
 * @brief Check if a frame is timeout or not.
 * @param frame
 * @return true: Timeout
 * @return false: note timeout
 */
bool hif_tl_frame_isTimeout(HIF_Frame_Node_t *frame)
{
    if (frame == NULL) {
        return 0;
    }

    return ((wrap32_before(frame->endTick, OSI_GetTicks())) || (frame->status == HIF_ERRCODE_TIMEOUT));
}
#endif      /* CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE */




/*** ++++++++++++++++++++++++++ APIs provided to message-layer ++++++++++++++++++++++++++ ***/
#if CONFIG_HIF_SPLIT_ENA
/**
 * @brief Packages a frame
 * @param msgHeader
 * @param fragHdr
 * @param dataQueue
 * @param sendCb
 * @param endTick
 * @return HIF_Frame_Node_t*
 */
__hif_sram_text HIF_Frame_Node_t* hif_tl_pack_frame(HIF_MsgHdr_t *msgHeader,
                                    hif_frag_hdr_t *fragHdr,
                                    HIF_Data_DoubleHead_t *dataQueue,
                                    void *sendCb,
                                    TickType_t endTick)
{
    if (msgHeader == NULL) {
        return NULL;
    }

    HIF_Frame_Node_t *frame = NULL;
    HIF_Data_Node_t *headDataNode = NULL;
    HIF_Data_Node_t *tailDataNode = NULL;
    HIF_Data_Node_t *fragHdrDataNode = NULL;
    HIF_MsgHdr_t *msgH = NULL;

    uint8_t hdrLen = 0;
    uint8_t tailLen = 0;

    frame = hif_MEM_FrameNodeMalloc();
    headDataNode = hif_MEM_DataNodeMalloc();
    tailDataNode = hif_MEM_DataNodeMalloc();

    if (fragHdr != NULL) {
        fragHdrDataNode = hif_MEM_DataNodeMalloc();
    }

    if ((frame == NULL) ||
        (headDataNode == NULL) ||
        (tailDataNode == NULL) ||
        ((fragHdr != NULL) && (fragHdrDataNode == NULL)))
    {
        HIF_LOG_DBG("no buffer, frame: %p, headDataNode: %p, tailDataNode: %p, fragHdrDataNode: %p\n",
                    frame, headDataNode, tailDataNode, fragHdrDataNode);
        hif_MEM_FrameNodeFree(frame);
        hif_MEM_DataNodeFree(headDataNode);
        hif_MEM_DataNodeFree(tailDataNode);
        hif_MEM_DataNodeFree(fragHdrDataNode);
        return NULL;
    }

    memset(frame, 0, sizeof(HIF_Frame_Node_t));
    memset(headDataNode, 0, sizeof(HIF_Data_Node_t));
    memset(tailDataNode, 0, sizeof(HIF_Data_Node_t));
    if (fragHdrDataNode != NULL) {
        memset(fragHdrDataNode, 0, sizeof(HIF_Data_Node_t));
    }

    if (frame) {
        /* Message Header processing */
        frame->hdr[0] = HIF_PHY_MSG_MAGIC;
        msgH = (HIF_MsgHdr_t *)(&(frame->hdr[HIF_PHY_HEAD_LEN]));
        *msgH = *msgHeader;
        msgH->flag |= HIF_MSG_FLAG_MORE_DATA_BIT;
        frame->hdr[HIF_HDR_CHECK8_IDX] = ~HIF_CheckSum8(HIF_PHY_MSG_MAGIC,
                                                                    &(frame->hdr[HIF_PHY_HEAD_LEN]),
                                                                    HIF_MSG_HEAD_LEN);
        hdrLen = HIF_HEAD_LEN + hif_DLL_GetTransDummy();

        /* Fragmengt header processing */
        if (fragHdr != NULL) {
            *(hif_frag_hdr_t *)(&frame->hdr[HIF_HDR_FRAG_HDR_IDX]) = *fragHdr;
            // msgH.flag = HAL_BIT(0);     // Ack, has set before calling this function
            // msgH.frag = 1;              // Fragment data, has set before calling this function
        }

        /* User data(real payload) */
        if (dataQueue != NULL) {
            frame->listHead.head = dataQueue->head;
            frame->listHead.tail = dataQueue->tail;
        } else {
            frame->listHead.head = NULL;
            frame->listHead.tail = NULL;
        }

        /* Tail(Cheaksum32) processing */
        hif_tl_frame_checksum32(frame);     // Calculate check32 here for transmission efficiency
        tailLen = HIF_CHKEC_LEN + hif_DLL_GetTransDummy();

        /* fragHdr as a data node inserted into frame->listHead */
        if (fragHdr != NULL) {
            hif_DLL_SendDataNodeInit(&(frame->hdr[HIF_HDR_FRAG_HDR_IDX]), sizeof(hif_frag_hdr_t), fragHdrDataNode, 0);
            hif_DATA_Prepend(&(frame->listHead),  fragHdrDataNode);
        }

        /*  msgHdr as a data node inserted into frame->listHead */
        hif_DLL_SendDataNodeInit(frame->hdr, hdrLen, headDataNode, HIF_DATA_NODE_FLAG_HEAD);
        hif_DATA_Prepend(&(frame->listHead),  headDataNode);
        frame->flag |= HIF_FRAME_FLAG_FRAME_HEAD;

        /* frame->tail[] as a data node inserted into frame->listHead */
        hif_DLL_SendDataNodeInit(frame->tail, tailLen, tailDataNode, HIF_DATA_NODE_FLAG_LAST);
        hif_DATA_Append(&(frame->listHead), tailDataNode);
        frame->flag |= HIF_FRAME_FLAG_FRAME_TAIL;

        /* Other data nodes have been processed through calling hif_DLL_SendDataNodeInit() before this function */
#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
        if (endTick == 0) {
            frame->endTick = OSI_GetTicks() + pdMS_TO_TICKS(CONFIG_HIF_TL_FRAME_TIMEOUT_MS_DEFAULT);
        } else {
            frame->endTick = endTick;
        }
#endif

        if (sendCb) {
            frame->sendCb = sendCb;
        } else {
            if (sendCb == NULL) {
                frame->sem = hif_MEM_SemMalloc();
                if (frame->sem == NULL) {
                    hif_mem_free_frame(frame, 1);
                    return NULL;
                }
            } else {
                frame->sendCb = sendCb;
            }
        }
    }

    return frame;
}

/**
 * @brief Pass a frame link list to tansport-layer
 * @param frameLink
 * @return uint8_t The number of frames that have been accepted by transport-layer.
 */
__hif_sram_text uint16_t hif_tl_frame_trans(HIF_Frame_Node_t *frameLink)
{
    if (frameLink == NULL) {
        return 0;
    }

    uint16_t frameCnt = 1;
    HIF_Frame_DoubleHead_t frameQueue = {0};
    HIF_Frame_Node_t *frameTail = NULL;

    frameTail = frameLink;
    while (frameTail->next) {
        frameCnt++;
        frameTail = frameTail->next;
    }
    frameQueue.head = frameLink;
    frameQueue.tail = frameTail;

    frameQueue_merge_tail(&(hifCtrl.put), &frameQueue);

    hifCtrl.putNodeCnt += frameCnt;

#if CONFIG_HIF_TL_DEBUG_EN
    unsigned long key = __lock_irq();
    hif_tl_debug_info.frameCnt_from_ml += frameCnt;
    __unlock_irq((unsigned long)key);
#endif

    if (hifCtrl.putNodeCnt >= hifCfg.sendNodeLevel) {
        extern void hif_EVENT_Set(uint32_t event);
        hif_EVENT_Set(HIF_EVENT_REPORT_REFRESH);
    }

    return frameCnt;
}

/**
 * @brief Calculate the checksum(uint32, at tail of hif msg) of frame
 * @param frame
 */
__hif_sram_text void hif_tl_frame_checksum32(HIF_Frame_Node_t *frame)
{
    HIF_MsgHdr_t *msgHeader = NULL;

    msgHeader = (HIF_MsgHdr_t *)&frame->hdr[HIF_PHY_HEAD_LEN];

    if(msgHeader->flag & HIF_MSG_FLAG_CHECK_BIT) {
        uint32_t offset = 0;
        HIF_Data_Node_t *dataNode = NULL;
        uint32_t checksum32 = 0;

        checksum32 = *(uint32_t *)msgHeader;

        if (msgHeader->frag) {
            checksum32 += *(uint32_t *)(&frame->hdr[HIF_HDR_FRAG_HDR_IDX]);
            checksum32 += *(uint32_t *)(&frame->hdr[HIF_HDR_FRAG_HDR_IDX + 4]);
        }

        dataNode =  frame->listHead.head;
        while (dataNode) {
            checksum32 += checksum32_calc(dataNode->data, dataNode->len, &offset);
            dataNode = dataNode->next;
        }
        checksum32 = ~checksum32;
        memcpy(frame->tail, &checksum32, HIF_CHKEC_LEN);
    }
}

/**
 * @brief Incremental updating of checksum(uint8_t)
 * @param oldCheck
 * @param oldVal
 * @param newVal
 * @return uint8_t
 */
uint8_t check8_incremental_update(uint8_t oldCheck, uint8_t oldVal, uint8_t newVal)
{
    uint16_t sum = ~(uint16_t)oldCheck;     // old sum
    sum = (sum - oldVal) + newVal;
    return (uint8_t)(~sum);
}

/**
 * @brief Incremental updating of checksum(uint32_t)
 * @param oldCheck
 * @param oldVal
 * @param newVal
 * @return uint32_t
 */
uint32_t check32_incremental_update(uint32_t oldCheck, uint32_t oldVal, uint32_t newVal)
{
    uint64_t sum = ~(uint64_t)oldCheck;     // old sum
    sum = (sum - oldVal) + newVal;
    return (uint32_t)(~sum);
}


void hif_tl_frame_update_seq_checksum(HIF_Frame_Node_t *pFrame)
{
    if (pFrame == NULL) {
        return;
    }

    if ((pFrame->flag & HIF_FRAME_FLAG_CHECKSUME) == 0) {
        HIF_MsgHdr_t *msgHdr = (HIF_MsgHdr_t *)(&(pFrame->hdr[HIF_PHY_HEAD_LEN]));
        uint32_t oldVal_u32 = *(uint32_t *)(&(pFrame->hdr[HIF_PHY_HEAD_LEN]));

        msgHdr->seq = hifCtrl.txSeq & 0x07;

        pFrame->hdr[0] = HIF_PHY_MSG_MAGIC;
        /* update head checksum */
        uint8_t check8 = HIF_CheckSum8(HIF_PHY_MSG_MAGIC, &pFrame->hdr[HIF_PHY_HEAD_LEN], HIF_MSG_HEAD_LEN);
        pFrame->hdr[1] = ~check8;

        *(uint32_t *)(&(pFrame->tail[0])) = check32_incremental_update(*(uint32_t *)(&(pFrame->tail[0])),
                                                                    oldVal_u32,
                                                                    *(uint32_t *)(&(pFrame->hdr[HIF_PHY_HEAD_LEN])));

        pFrame->flag |= HIF_FRAME_FLAG_CHECKSUME;
    }
}

#endif      // CONFIG_HIF_SPLIT_ENA
/*** ========================== APIs provided to message-layer ========================== ***/

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
