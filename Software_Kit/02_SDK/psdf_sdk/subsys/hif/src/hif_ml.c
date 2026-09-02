/**
 ******************************************************************************
 * @file    hif_ml.c
 * @brief   hif_ml define.
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
#include "hif.h"
#include "hif_ml.h"
#include "hif_dll.h"
#include "hif_tl.h"

#include "hif_priv.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */

#if CONFIG_HIF_SPLIT_ENA
/**
 * @brief 
 */
typedef struct hif_ml_ctrl_t_ {
    uint8_t msgSeq;                     /*< Application message sequence number, recorded in HIF_Msg_Node_t->seq.
                                            Range: 0~63.*/
    OSI_Semaphore_t msgSeqSem;          /*< For protecting hif_ml_ctrl.msgSeq.
                                            Max value:  1;
                                            Init Value: 1;*/
    HIF_Msg_DoubleHead_t msgTrans;
    HIF_Msg_DoubleHead_t msgWaitAck;
    OSI_Timer_t waitAckTimer;
} hif_ml_ctrl_t;

static hif_ml_ctrl_t hif_ml_ctrl = {0};

/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static HIF_Frame_Node_t* hif_ml_get_frag_frame(HIF_Msg_Node_t *msgNode);

static void hif_ml_default_cb_inMsgItem(HIF_Msg_Node_t *msgNode);
static void hif_ml_default_cb_inFrame(HIF_Frame_Node_t *frame);
static bool hif_ml_isFragDone(HIF_Msg_Node_t *msgNode);

static uint16_t hif_ml_frag_ack_handler(hif_msg_tlv_t *msgTlv);
static void hif_ml_checkTimeoutAndRelease(HIF_Msg_DoubleHead_t *msgQueue);
static void hif_ml_ack_timer_callback(void *arg);

static int hif_ml_create_seq_sem(void);
static int hif_ml_del_seq_sem(void);
static int hif_ml_get_next_seq(uint8_t *seq);

static int hif_ml_create_wait_ack_timer(void);
static int hif_ml_del_wait_ack_timer(void);
static bool hif_ml_ack_timer_is_active(void);
static int hif_ml_ack_timer_start(uint32_t timeoutMs);
static int hif_ml_ack_timer_stop(void);

static HIF_Msg_Node_t* hif_find_ml_msg_item(uint8_t msgId, uint8_t msgSeq, HIF_Msg_DoubleHead_t *msgQueue);
static int hif_ml_checkAndRelease_msg_item(HIF_Msg_Node_t *msgNode, HIF_Msg_DoubleHead_t *msgQueue);
static void hif_ml_waitAckFrameQueue_add(HIF_Frame_Node_t *frame, HIF_Frame_DoubleHead_t *frameQueue);
static bool hif_ml_msg_isTimeout(HIF_Msg_Node_t *msgNode);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Call this function if length of user'data
 * @param msgHeader
 * @param dataQueue
 * @param appDataTotalLen
 * @param sendCb
 * @param timeoutMs
 * @return int
 */
__hif_sram_text int hif_ml_frag_send(HIF_MsgHdr_t *msgHeader,
                                    HIF_Data_DoubleHead_t *dataQueue,
                                    uint32_t appDataTotalLen,
                                    HIF_MsgTrans_Callback sendCb,
                                    uint32_t timeoutMs)
{
    if ((msgHeader == NULL) || (dataQueue == NULL) || (appDataTotalLen == 0)) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    int retVal = HIF_ERRCODE_SUCCESS;
    HIF_Msg_Node_t *msgNode = NULL;
    HIF_Frame_Node_t *frameLink = NULL;
    OSI_Semaphore_t *pSem = NULL;

    msgNode = hif_MEM_MsgNodeMalloc();

    if (msgNode == NULL) {
        HIF_LOG_DBG("msgNode no buffer\n");
        return HIF_ERRCODE_NO_BUFFER;
    } else {
        memset(msgNode, 0, sizeof(HIF_Msg_Node_t));

        if (hif_ml_get_next_seq(&(msgNode->msgSeq)) != HIF_ERRCODE_SUCCESS) {
            hif_MEM_MsgNodeFree(msgNode);
            HIF_LOG_DBG("msg seq blocked\n");
            return HIF_ERRCODE_FAIL;
        }

        msgNode->msgHeader = *msgHeader;
        msgNode->dataQueue = *dataQueue;
        msgNode->dataTotalLen = appDataTotalLen;
        msgNode->unFragDataNode = msgNode->dataQueue.head;
        msgNode->unfragDataNodeOffset = 0;
        msgNode->fragCnt = 0;;
        msgNode->toTL_frameCnt = 0;
        msgNode->fromTL_frameCnt = 0;
        memset(&(msgNode->waitAckFrameQueue), 0, sizeof(HIF_Frame_DoubleHead_t));

        if (hifCfg.splitRetryEna) {
#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
            msgNode->fragRetryCtrlFlag = HIF_ML_FRAG_RETRY_PACK_ACK_NOT_YET;
#else
            msgNode->fragRetryCtrlFlag = HIF_ML_FRAG_RETRY_NOT;
            hif_MEM_MsgNodeFree(msgNode);
            HIF_LOG_DBG("split no config but enable\n");
            return HIF_ERRCODE_FAIL;
#endif
        } else {
            msgNode->fragRetryCtrlFlag = HIF_ML_FRAG_RETRY_NOT;
        }

        msgNode->status = HIF_ERRCODE_SUCCESS;
        if (pdMS_TO_TICKS(timeoutMs) != 0) {
            msgNode->endTick = OSI_GetTicks() + pdMS_TO_TICKS(timeoutMs);
        } else {
            msgNode->endTick = OSI_GetTicks() + pdMS_TO_TICKS(CONFIG_HIF_ML_MSG_TIMEOUT_MS_DEFAULT);
        }

        msgNode->sendCb = (void *)sendCb;
        if (msgNode->sendCb == NULL) {
            pSem = hif_MEM_SemMalloc();
            if (pSem == NULL) {
                hif_MEM_MsgNodeFree(msgNode);
                return HIF_ERRCODE_NO_BUFFER;
            }
            msgNode->sem = pSem;
        }

        frameLink = hif_ml_get_frag_frame(msgNode);

        if (frameLink == NULL) {
            hif_MEM_SemFree(pSem);
            hif_MEM_MsgNodeFree(msgNode);
            return HIF_ERRCODE_NO_BUFFER;
        } else {
            hif_Msg_Append(&(hif_ml_ctrl.msgTrans), msgNode);

            uint8_t toTL_frameCnt = hif_tl_frame_trans(frameLink);

            msgNode->toTL_frameCnt += toTL_frameCnt;

            if (msgNode->sendCb == NULL) {
                OSI_SemaphoreWait(pSem, OSI_WAIT_FOREVER);
                hif_MEM_SemFree(pSem);

                if (msgNode->waitAckFrameQueue.head != NULL) {
                    hif_mem_free_frameQueue(&(msgNode->waitAckFrameQueue), 1);
                }
                retVal = msgNode->status;
                hif_MEM_MsgNodeFree(msgNode);
            }
        }
    }

    return retVal;
}

/**
 * @brief Fragment the unfragmented data in HIF_Msg_Node_t,
 *        and assemble fragment data into frames(frame link)
 * @param msgNode
 * @return HIF_Frame_Node_t* The frame link
 * @todo Avaliable memory pre-validataion for efficiency.
 */
static HIF_Frame_Node_t* hif_ml_get_frag_frame(HIF_Msg_Node_t *msgNode)
{
    if (msgNode == NULL) {
        return NULL;
    }

    HIF_MsgHdr_t fragMsgHeader = {0};
    hif_frag_hdr_t fragHdr = {0};

    HIF_Frame_DoubleHead_t frameQueue = {0};    // Including frames that have been fragmented and packeage
    HIF_Frame_Node_t *frame = NULL;
    HIF_Data_DoubleHead_t dataQueue = {0};      // For saving fragment data thag need to package into a frame
    HIF_Data_Node_t *dataNode = NULL;

    uint32_t appDataNodeCurr_startIdx = 0;
    HIF_Data_Node_t *appDataNodeCurr = NULL;

    fragMsgHeader = msgNode->msgHeader;
    fragMsgHeader.flag &= ~HAL_BIT(0);      // No ack
    fragMsgHeader.frag = 1;                 // Fragment data
    // fragMsgHeader.length =

    fragHdr.msgSeq = msgNode->msgSeq;
    fragHdr.totalLen = msgNode->dataTotalLen;
    // fragHdr.offset =

    do {
        /** + Get a fragment + **/
        fragHdr.offset = msgNode->fragCnt * HIF_ML_FRAG_REAL_PAYLOAD_LEN;
        fragMsgHeader.length = 0;
        memset(&(dataQueue), 0, sizeof(HIF_Data_DoubleHead_t));

        appDataNodeCurr = msgNode->unFragDataNode;
        appDataNodeCurr_startIdx = msgNode->unfragDataNodeOffset;
        while (appDataNodeCurr) {
            dataNode = hif_MEM_DataNodeMalloc();
            if (dataNode == NULL) {
                hif_MEM_DataListFree(&dataQueue);
                hif_mem_free_frameQueue(&frameQueue, 1);

                return NULL;
            } else {
                memset(dataNode, 0, sizeof(HIF_Data_Node_t));

                uint32_t appDataItemCurr_remainLen = appDataNodeCurr->len - appDataNodeCurr_startIdx;
                if ((fragMsgHeader.length + appDataItemCurr_remainLen) <= HIF_ML_FRAG_REAL_PAYLOAD_LEN) {
                    // dataNode->data = appDataNodeCurr->data + appDataNodeCurr_startIdx;
                    // dataNode->len = appDataItemCurr_remainLen;
                    // dataNode->next = NULL;

                    hif_DLL_SendDataNodeInit(appDataNodeCurr->data + appDataNodeCurr_startIdx,
                                             appDataItemCurr_remainLen,
                                             dataNode,
                                             0);

                    hif_DATA_Append(&dataQueue, dataNode);
                    fragMsgHeader.length += dataNode->len;

                    appDataNodeCurr_startIdx = 0;
                    appDataNodeCurr = appDataNodeCurr->next;
                } else {
                    // dataNode->data = appDataNodeCurr->data + appDataNodeCurr_startIdx;
                    // dataNode->len = HIF_ML_FRAG_REAL_PAYLOAD_LEN - fragMsgHeader.length;
                    // dataNode->next = NULL;

                    hif_DLL_SendDataNodeInit(appDataNodeCurr->data + appDataNodeCurr_startIdx,
                                             HIF_ML_FRAG_REAL_PAYLOAD_LEN - fragMsgHeader.length,
                                             dataNode,
                                             0);

                    hif_DATA_Append(&dataQueue, dataNode);
                    fragMsgHeader.length += dataNode->len;

                    appDataNodeCurr_startIdx += dataNode->len;       // Remaining after appDataNodeCurr_startIdx

                    break;                                          // Get a fragment done
                }
            }
        }
        /** = Get the fragment done = **/

        #if CONIFG_HIF_ENCRYPT_ENABLE
        1. payload = valid Payload + encrypy;
        2. encrypt as a data item inserted into newfragItem->dataQueue
        #endif

        /** + Package the fragment into a frame + **/
        if (dataQueue.head == NULL) {
            msgNode->unFragDataNode = appDataNodeCurr;
            break;
        } else {
            fragMsgHeader.length += sizeof(hif_frag_hdr_t);
            frame = hif_tl_pack_frame(&fragMsgHeader, &fragHdr, &dataQueue, hif_ml_default_cb_inFrame, msgNode->endTick);
            if (frame == NULL) {
                hif_MEM_DataListFree(&dataQueue);
                hif_mem_free_frameQueue(&frameQueue, 1);
                return NULL;
            } else {

                hif_FRAME_Append(&frameQueue, frame);
                msgNode->fragCnt += 1;
                msgNode->unFragDataNode = appDataNodeCurr;
                msgNode->unfragDataNodeOffset = appDataNodeCurr_startIdx;
                // Start next fragment...
            }
        }
        /** = Package the fragment into a frame = **/
    } while (msgNode->unFragDataNode);

#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
    if (msgNode->fragRetryCtrlFlag == HIF_ML_FRAG_RETRY_PACK_ACK_NOT_YET) {     // Ack fragment has not packaged into a frame;
        // fragMsgHeader = msgNode->msgHeader;
        fragMsgHeader.flag |= HAL_BIT(0);   // Ack
        // fragMsgHeader.frag = 1;          // Fragment data
        fragMsgHeader.length = sizeof(hif_frag_hdr_t);

        // fragHdr.msgSeq = msgNode->msgSeq;
        // fragHdr.totalLen = msgNode->dataTotalLen;
        fragHdr.offset = fragHdr.totalLen;

        frame = hif_tl_pack_frame(&fragMsgHeader, &fragHdr, NULL, hif_ml_default_cb_inFrame, msgNode->endTick);
        if (frame == NULL) {
            hif_MEM_DataListFree(&dataQueue);
            hif_mem_free_frameQueue(&frameQueue, 1);

            return NULL;
        } else {
            hif_FRAME_Append(&frameQueue, frame);
            msgNode->fragRetryCtrlFlag = HIF_ML_FRAG_RETRY_PACK_ACK_DONE;
        }
    }
#endif

    return frameQueue.head;
}

/**
 * @brief Default callback of application-layer to message-layer.
 *        Called by message-layer when sending message is done.
 * @param msgNode
 */
static void hif_ml_default_cb_inMsgItem(HIF_Msg_Node_t *msgNode)
{
    OSI_SemaphoreRelease(msgNode->sem);
}



/**
 * @brief Default callback of message-layer to transport-layer.
 *        Called by transport-layer when sending frame is done.
 * @param frame The frame
 */
static void hif_ml_default_cb_inFrame(HIF_Frame_Node_t *frame)
{
    if (frame == NULL) {
        return;
    }

    HIF_MsgHdr_t *msgHeader = NULL;
    hif_frag_hdr_t *fragHdr = NULL;
    HIF_Msg_Node_t *msgNode = NULL;

    msgHeader = (HIF_MsgHdr_t *)(&(frame->hdr[HIF_PHY_HEAD_LEN]));

    fragHdr = (hif_frag_hdr_t *)(&(frame->hdr[HIF_HDR_FRAG_HDR_IDX]));

#if  CONFIG_HIF_SEND_SPLIT_RETRY_ENA
    uint8_t isAckFrag = (msgHeader->flag & HAL_BIT(0)) && (msgHeader->frag);
#endif

    unsigned long key = __lock_irq();
    msgNode = hif_find_ml_msg_item(msgHeader->msg_id, fragHdr->msgSeq, &(hif_ml_ctrl.msgTrans));
    __unlock_irq((unsigned long)key);
    if (msgNode == NULL) {
        #if (CONFIG_HIF_SEND_SPLIT_RETRY_ENA)
            unsigned long key = __lock_irq();
            msgNode = hif_find_ml_msg_item(msgHeader->msg_id, fragHdr->msgSeq, &(hif_ml_ctrl.msgWaitAck));
            __unlock_irq((unsigned long)key);
            if (msgNode) {
                msgNode->fromTL_frameCnt += 1;
                hif_ml_waitAckFrameQueue_add(frame, &(msgNode->waitAckFrameQueue));     // msgNode in wait_ack queue
            } else {
                hif_mem_free_frame(frame, 1);   // msgNode not in both trans and wait_ack queue
            }
        #else
            hif_mem_free_frame(frame, 1);      // msgNode not in both trans and wait_ack queue
        #endif
        return;
    }

    /** Process below to end when msgNode in trans queue **/

    msgNode->fromTL_frameCnt += 1;

    if (hif_ml_msg_isTimeout(msgNode)) {
        msgNode->status = HIF_ERRCODE_TIMEOUT;
        if (msgNode->fragRetryCtrlFlag) {
            #if  (CONFIG_HIF_SEND_SPLIT_RETRY_ENA)
            hif_ml_waitAckFrameQueue_add(frame, &(msgNode->waitAckFrameQueue));
            #endif
        } else {
            hif_mem_free_frame(frame, 1);
        }
        hif_ml_checkAndRelease_msg_item(msgNode, &(hif_ml_ctrl.msgTrans));
        return;
    }

    /** Process below to end when msgNode is not timeout **/

    if (msgNode->fragRetryCtrlFlag) {
#if (CONFIG_HIF_SEND_SPLIT_RETRY_ENA)
        if ((msgNode->fragRetryCtrlFlag == HIF_ML_FRAG_RETRY_ACK_GOT_RETRY) ||
            (msgNode->fragRetryCtrlFlag == HIF_ML_FRAG_RETRY_ACK_TRANS_FAIL)) {
            if (frame->status != HIF_ERRCODE_SUCCESS) {
                msgNode->status = HIF_ERRCODE_IO_ERROR;
            }

            hif_mem_free_frame(frame, 1);
            hif_ml_checkAndRelease_msg_item(msgNode, &(hif_ml_ctrl.msgTrans));
        } else {
            hif_ml_waitAckFrameQueue_add(frame, &(msgNode->waitAckFrameQueue));

            if (isAckFrag) {
                if (frame->status != HIF_ERRCODE_SUCCESS) {
                    msgNode->status = HIF_ERRCODE_IO_ERROR;

                    int ret = HIF_ERRCODE_SUCCESS;
                    ret = hif_ml_checkAndRelease_msg_item(msgNode, &(hif_ml_ctrl.msgTrans));
                    if (ret != HIF_ERRCODE_SUCCESS) {
                        msgNode->fragRetryCtrlFlag = HIF_ML_FRAG_RETRY_ACK_TRANS_FAIL;

                        if (ret == HIF_ERRCODE_NO_BUFFER) {
                            HIF_LOG_DBG("Should not %d\n", ret);
                        }
                    }
                } else {
                    hif_ml_msgQueue_remove_item(msgNode, &(hif_ml_ctrl.msgTrans));
                    hif_Msg_Append(&(hif_ml_ctrl.msgWaitAck), msgNode);

                    if (!hif_ml_ack_timer_is_active()) {
                        int errCode = HIF_ERRCODE_SUCCESS;
                        hif_ml_ack_timer_stop();
                        errCode = hif_ml_ack_timer_start(CONFIG_HIF_ML_ACK_TIMEOUT_MS);
                        if (errCode) {
                            HIF_LOG_DBG("hif_ml_default_cb_inFrame start ack timer %d\n\r", errCode);
                        }
                    }
                }
            }
        }
#endif
    } else {
        if (frame->status != HIF_ERRCODE_SUCCESS) {
            msgNode->status = HIF_ERRCODE_IO_ERROR;
        }

        hif_mem_free_frame(frame, 1);
        hif_ml_checkAndRelease_msg_item(msgNode, &(hif_ml_ctrl.msgTrans));
    }
}

/**
 * @brief 
 * @param msgNode
 * @return true
 * @return false
 */
static bool hif_ml_isFragDone(HIF_Msg_Node_t *msgNode)
{
    if (msgNode == NULL) {
        return 0;
    }

    return (msgNode->unFragDataNode == NULL);   // All data has been fragmented and packaged into frame
}

/*** ========================= Associated with Message Layer Sending ========================= ***/


/*** ++++++++++++++++++++++++++ Associated with ack handler ++++++++++++++++++++++++++ ***/
#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA

/**
 * @brief Handler of ack to message layer, called in hif_host_ack_tvl_handler().
 * @param msgTlv
 * @return uint16_t The number of frames that have been delivered to the transport-layer
 */
static uint16_t hif_ml_frag_ack_handler(hif_msg_tlv_t *msgTlv)
{
    if ((msgTlv == NULL) || (msgTlv->length < sizeof(hif_ml_frag_ack_t))) {
        return 0;
    }

    hif_ml_ack_timer_stop();

    uint8_t msgId = 0;
    uint8_t fragAckNum = 0;
    hif_ml_frag_ack_t *fragAck = NULL;
    HIF_Msg_Node_t *msgNode = NULL;
    uint8_t toTL_frameCnt = 0;

    msgId = msgTlv->msgid;
    fragAckNum = msgTlv->length / sizeof(hif_ml_frag_ack_t);
    fragAck = (hif_ml_frag_ack_t *)(msgTlv->data);

    unsigned long key = __lock_irq();
    msgNode = hif_find_ml_msg_item(msgId, fragAck->msgSeq, &(hif_ml_ctrl.msgWaitAck));
    msgNode = hif_ml_msgQueue_remove_item(msgNode, &(hif_ml_ctrl.msgWaitAck));
    __unlock_irq((unsigned long)key);

    if (msgNode != NULL) {
        if (fragAck->blank_len == 0) {  // No need to re-transimit, means that transmission is done.
            if (msgNode->sendCb != NULL) {
                if (msgNode->dataQueue.head->rsv2 & HIF_DATA_NODE_FLAG_MSG_REPORT) {    // HIF_MsgReport
                    HIF_Data_DoubleHead_t dataQueue_tmp = {0};

                    msgNode->dataQueue.head->rsv2 &= ~((uint16_t)HIF_DATA_NODE_FLAG_MSG_REPORT | (uint16_t)HIF_DATA_NODE_FLAG_NOT_DLL_INIT);
                    hif_DATA_Append(&dataQueue_tmp, hif_DATA_PopHead(&(msgNode->dataQueue)));   // dataNode from HIF_MsgReport malloc

                    ((HIF_MsgTrans_Callback)(msgNode->sendCb))(msgNode->msgHeader.msg_id,
                                                                &(dataQueue_tmp),
                                                                msgNode->status);      // callback to application

                    if (dataQueue_tmp.head != NULL) {
                        hif_MEM_DataListFree(&(dataQueue_tmp));
                    }
                } else {                                                                // HIF_MsgReport_ListStart
                    ((HIF_MsgTrans_Callback)(msgNode->sendCb))(msgNode->msgHeader.msg_id,
                                                                &(msgNode->dataQueue),
                                                                msgNode->status);      // callback to application
                }

                if (msgNode->dataQueue.head != NULL) {
                    HIF_Data_Node_t *dataNode = NULL;
                    dataNode = msgNode->dataQueue.head;
                    while (dataNode) {
                        dataNode->rsv2 &= ~((uint16_t)HIF_DATA_NODE_FLAG_MSG_REPORT | (uint16_t)HIF_DATA_NODE_FLAG_NOT_DLL_INIT);
                        dataNode = dataNode->next;
                    }
                    hif_MEM_DataListFree(&(msgNode->dataQueue));
                }

                if (msgNode->waitAckFrameQueue.head != NULL) {
                    hif_mem_free_frameQueue(&(msgNode->waitAckFrameQueue), 1);
                }

                hif_MEM_MsgNodeFree(msgNode);
            } else {
                hif_ml_default_cb_inMsgItem(msgNode);
                if (msgNode->waitAckFrameQueue.head != NULL) {
                    hif_mem_free_frameQueue(&(msgNode->waitAckFrameQueue), 1);
                }
            }
        } else {
            if (hif_ml_msg_isTimeout(msgNode)) {
                msgNode->status = HIF_ERRCODE_TIMEOUT;
                hif_ml_checkAndRelease_msg_item(msgNode, NULL);
            } else {
                uint8_t fragAck_i = 0;
                HIF_Frame_Node_t *frame = NULL;
                HIF_Frame_Node_t *frameFree = NULL;
                HIF_Frame_DoubleHead_t frameQueue = {0};
                hif_frag_hdr_t *fragHdr = NULL;

                frame = msgNode->waitAckFrameQueue.head;
                while ((frame != NULL) && (fragAck_i < fragAckNum)) {
                    fragHdr = (hif_frag_hdr_t *)(&(frame->hdr[HIF_HDR_FRAG_HDR_IDX]));
                    if (fragHdr->offset == fragAck[fragAck_i].offset) {
                        uint32_t blank_len_remain = 0;

                        blank_len_remain = fragAck[fragAck_i].blank_len;
                        while ((blank_len_remain > 0) && (frame != NULL)) {
                            uint32_t currFragOffset = 0;
                            uint32_t NextFragOffset = 0;
                            uint32_t currFlagDataLen = 0;
                            currFragOffset = ((hif_frag_hdr_t *)(&(frame->hdr[HIF_HDR_FRAG_HDR_IDX])))->offset;
                            if (frame->next != NULL) {
                                NextFragOffset = ((hif_frag_hdr_t *)(&(frame->next->hdr[HIF_HDR_FRAG_HDR_IDX])))->offset;
                                currFlagDataLen = NextFragOffset - currFragOffset;
                                if (blank_len_remain >= currFlagDataLen) {
                                    blank_len_remain -= currFlagDataLen;
                                } else {
                                    HIF_LOG_DBG("hif_ml_frag_ack_handler unexpected result\n\r");
                                    break;
                                }
                            }

                            frameFree = frame;
                            frame = frame->next;
                            hif_frameQueue_remove_frame(frameFree, &(msgNode->waitAckFrameQueue));
                            hif_TL_RefreshFrame(frameFree);
                            hif_FRAME_Append(&frameQueue, frameFree);
                        }

                        fragAck_i += 1;
                    } else {
                        frame = frame->next;
                    }
                }

                msgNode->fragRetryCtrlFlag = HIF_ML_FRAG_RETRY_ACK_GOT_RETRY;
                toTL_frameCnt = hif_tl_frame_trans(frameQueue.head);
                if (toTL_frameCnt > 0) {
                    msgNode->toTL_frameCnt += toTL_frameCnt;
                    hif_Msg_Append(&(hif_ml_ctrl.msgTrans), msgNode);
                } else {
                    if (hif_ml_checkAndRelease_msg_item(msgNode, NULL) == HIF_ERRCODE_NOT_READY) {
                        hif_Msg_Append(&(hif_ml_ctrl.msgTrans), msgNode);
                    }

                }
            }
        }
    }

    hif_ml_checkTimeoutAndRelease(&(hif_ml_ctrl.msgWaitAck));

    if (hif_ml_ctrl.msgWaitAck.head != NULL) {
        int errCode = HIF_ERRCODE_SUCCESS;
        if (!hif_ml_ack_timer_is_active()) {
            hif_ml_ack_timer_stop();
            errCode = hif_ml_ack_timer_start(CONFIG_HIF_ML_ACK_TIMEOUT_MS);
            if (errCode) {
                HIF_LOG_DBG("hif_ml_frag_ack_handler start ack timer %d\n\r", errCode);
            }
        }
    }

    return toTL_frameCnt;
}
#endif

#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
/**
 * @brief Check the itmes of a queue if timeout or not. Release if timeout.
 * @param msgQueue
 */
static void hif_ml_checkTimeoutAndRelease(HIF_Msg_DoubleHead_t *msgQueue)
{
    if (msgQueue == NULL) {
        return;
    }

    HIF_Msg_Node_t *msgNode = NULL;
    HIF_Msg_Node_t *msgItemFree = NULL;

    unsigned long key = __lock_irq();
    msgNode = msgQueue->head;
    while (msgNode) {
        msgItemFree = msgNode;
        msgNode = msgNode->next;
        if (hif_ml_msg_isTimeout(msgItemFree)) {
            msgItemFree->status = HIF_ERRCODE_TIMEOUT;
            hif_ml_checkAndRelease_msg_item(msgItemFree, msgQueue);
        }
    }
    __unlock_irq((unsigned long)key);
}
#endif

#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
/**
 * @brief Wait-ack-timer's callback
 * @param arg
 */
static void hif_ml_ack_timer_callback(void *arg)
{
    hif_ml_ack_timer_stop();
    hif_ml_checkTimeoutAndRelease(&(hif_ml_ctrl.msgWaitAck));

    if (hif_ml_ctrl.msgWaitAck.head != NULL) {
        int errCode = HIF_ERRCODE_SUCCESS;
        if (!hif_ml_ack_timer_is_active()) {
            errCode = hif_ml_ack_timer_start(CONFIG_HIF_ML_ACK_TIMEOUT_MS);
            if (errCode) {
                HIF_LOG_DBG("hif_ml_ack_timer_callback start ack timer %d\n\r", errCode);
            }
        }
    }
}
#endif
/*** ========================== Associated with ack handler ========================= ***/


/*** ++++++++++++++++++++++++++ Associated Application message sequence number ++++++++++++++++++++++++++ ***/
#define HIF_ML_MSG_ITEM_SEQ_SEM_WAIT_MS     5UL

static int hif_ml_create_seq_sem(void)
{
    return OSI_SemaphoreCreate(&(hif_ml_ctrl.msgSeqSem), 1, 1);
}

static int hif_ml_del_seq_sem(void)
{
    return OSI_SemaphoreDelete(&(hif_ml_ctrl.msgSeqSem));
}

/**
 * @brief Increase Application message sequence number and Get the latest number
 * @param seq
 * @return int
 */
static int hif_ml_get_next_seq(uint8_t *seq)
{
    if (OSI_SemaphoreWait(&(hif_ml_ctrl.msgSeqSem), HIF_ML_MSG_ITEM_SEQ_SEM_WAIT_MS) != OSI_STATUS_OK) {
        return HIF_ERRCODE_FAIL;
    }

    if (hif_ml_ctrl.msgSeq == 63) {
        hif_ml_ctrl.msgSeq = 0;
    } else {
        hif_ml_ctrl.msgSeq += 1;
    }
    *seq = hif_ml_ctrl.msgSeq;

    OSI_SemaphoreRelease(&(hif_ml_ctrl.msgSeqSem));

    return HIF_ERRCODE_SUCCESS;
}
/*** ========================== Associated Application message sequence number ========================= ***/


/*** ++++++++++++++++++++++++++ Associated with waiting-ack timer ++++++++++++++++++++++++++ ***/
#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
/**
 * @brief Create timer for message-layer to wait ack
 * @return int
 */
static int hif_ml_create_wait_ack_timer(void)
{
    return OSI_TimerCreate(&(hif_ml_ctrl.waitAckTimer),
                           OSI_TIMER_ONCE,
                           hif_ml_ack_timer_callback,
                           NULL,
                           CONFIG_HIF_ML_ACK_TIMEOUT_MS);
}

static int hif_ml_del_wait_ack_timer(void)
{
    return OSI_TimerDelete(&(hif_ml_ctrl.waitAckTimer));
}

/**
 * @brief Check if (hif_ml_ctrl.waitAckTimer) is on active or inactive
 * @return true - Active
 * @return false - Inactive
 */
static bool hif_ml_ack_timer_is_active(void)
{
    return OSI_TimerIsActive(&(hif_ml_ctrl.waitAckTimer));
}

/**
 * @brief Start (hif_ml_ctrl.waitAckTimer)
 * @param timeoutMs The expired time(ms)
 * @return int
 */
static int hif_ml_ack_timer_start(uint32_t timeoutMs)
{
    OSI_Status_t retVal = OSI_STATUS_OK;

    retVal = OSI_TimerChangePeriod(&(hif_ml_ctrl.waitAckTimer), timeoutMs);

    if (retVal != OSI_STATUS_OK) {
        return retVal;
    }

    return OSI_TimerStart(&(hif_ml_ctrl.waitAckTimer));
}

static int hif_ml_ack_timer_stop(void)
{
    return OSI_TimerStop(&(hif_ml_ctrl.waitAckTimer));
}
#endif
/*** ========================== Associated with waiting-ack timer ========================= ***/

/**
 * @brief
 */
int hif_ml_init(void)
{
    int retVal = 0;

    retVal = hif_ml_create_seq_sem();
    if (retVal != HIF_ERRCODE_SUCCESS) {
        return retVal;
    }

#if (CONFIG_HIF_SEND_SPLIT_RETRY_ENA)
    retVal = hif_ml_create_wait_ack_timer();
    if (retVal != HIF_ERRCODE_SUCCESS) {
        return retVal;
    }
#endif

    return retVal;
}

int hif_ml_deInit(void)
{
    int status = HIF_ERRCODE_SUCCESS;
    int ret = HIF_ERRCODE_SUCCESS;

    status = hif_ml_del_seq_sem();
    if (status != HIF_ERRCODE_SUCCESS) {
        ret = status;
    }

#if (CONFIG_HIF_SEND_SPLIT_RETRY_ENA)
    status = hif_ml_del_wait_ack_timer();
    if (status != HIF_ERRCODE_SUCCESS) {
        ret = status;
    }
#endif

    return ret;
}

/**
 * @brief Find a msgNode in a HIF_Msg_DoubleHead_t according to message ID
 * @param msgId The message ID.
 * @param msgSeq The application message sequence number(HIF_Msg_DoubleHead_t->msgSeq)
 * @param msgQueue The HIF_Msg_DoubleHead_t
 * @return HIF_Msg_Node_t*: NULL - Not found; !NULL - The msgNode needed
 */
static HIF_Msg_Node_t* hif_find_ml_msg_item(uint8_t msgId, uint8_t msgSeq, HIF_Msg_DoubleHead_t *msgQueue)
{
    if (msgQueue == NULL) {
        return NULL;
    }

    HIF_Msg_Node_t *found = msgQueue->head;

    while (found) {
        if ((found->msgHeader.msg_id == msgId) && (found->msgSeq == msgSeq)) {
            break;
        }
        found = found->next;
    }

    return found;
}

/**
 * @brief Check if could release a msg_item;
 *        Release if allowed(msgNode->fromTL_frameCnt == msgNode->toTL_frameCnt).
 * @param msgNode
 * @param msgQueue If not NULL, remove the item from the queue
 */
static int hif_ml_checkAndRelease_msg_item(HIF_Msg_Node_t *msgNode, HIF_Msg_DoubleHead_t *msgQueue)
{
    if (msgNode == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    int ret = HIF_ERRCODE_SUCCESS;
    HIF_Msg_Node_t *pMsgNode = NULL;

    if (msgNode->fromTL_frameCnt == msgNode->toTL_frameCnt) {
        unsigned long key = __lock_irq();
        if (msgQueue != NULL) {
            pMsgNode = hif_ml_msgQueue_remove_item(msgNode, msgQueue);
        } else {
            pMsgNode = msgNode;
        }
        __unlock_irq((unsigned long)key);

        if (pMsgNode != NULL) {
            if (pMsgNode->sendCb != NULL) {
                if (msgNode->dataQueue.head->rsv2 & HIF_DATA_NODE_FLAG_MSG_REPORT) {    // HIF_MsgReport
                    HIF_Data_DoubleHead_t dataQueue_tmp = {0};

                    msgNode->dataQueue.head->rsv2 &= ~((uint16_t)HIF_DATA_NODE_FLAG_MSG_REPORT | (uint16_t)HIF_DATA_NODE_FLAG_NOT_DLL_INIT);
                    hif_DATA_Append(&dataQueue_tmp, hif_DATA_PopHead(&(msgNode->dataQueue)));   // dataNode from HIF_MsgReport malloc

                    ((HIF_MsgTrans_Callback)(msgNode->sendCb))(msgNode->msgHeader.msg_id,
                                                                &(dataQueue_tmp),
                                                                msgNode->status);      // callback to application

                    if (dataQueue_tmp.head != NULL) {
                        hif_MEM_DataListFree(&(dataQueue_tmp));
                    }
                } else {                                                                // HIF_MsgReport_ListStart
                    ((HIF_MsgTrans_Callback)(msgNode->sendCb))(msgNode->msgHeader.msg_id,
                                                                &(msgNode->dataQueue),
                                                                msgNode->status);      // callback to application
                }

                if (msgNode->dataQueue.head != NULL) {
                    HIF_Data_Node_t *dataNode = NULL;
                    dataNode = msgNode->dataQueue.head;
                    while (dataNode) {
                        dataNode->rsv2 &= ~((uint16_t)HIF_DATA_NODE_FLAG_MSG_REPORT | (uint16_t)HIF_DATA_NODE_FLAG_NOT_DLL_INIT);
                        dataNode = dataNode->next;
                    }
                    hif_MEM_DataListFree(&(msgNode->dataQueue));
                }

                if (msgNode->waitAckFrameQueue.head != NULL) {
                    hif_mem_free_frameQueue(&(msgNode->waitAckFrameQueue), 1);
                }

                hif_MEM_MsgNodeFree(msgNode);
            } else {
                hif_ml_default_cb_inMsgItem(msgNode);
                if (msgNode->waitAckFrameQueue.head != NULL) {
                    hif_mem_free_frameQueue(&(msgNode->waitAckFrameQueue), 1);
                }
            }
        } else {
            ret = HIF_ERRCODE_NO_BUFFER;
        }
    } else {
        ret = HIF_ERRCODE_NOT_READY;
    }

    return ret;
}

#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
/**
 * @brief Add a  frame item into a frame queue in the order indicated by hif_frag_hdr_t->offset.
 * @param frame
 * @param frameQueue
 */
static void hif_ml_waitAckFrameQueue_add(HIF_Frame_Node_t *frame, HIF_Frame_DoubleHead_t *frameQueue)
{
    if ((frame == NULL) || (frameQueue == NULL)) {
        return;
    }

    hif_frag_hdr_t *fragHdr = NULL;
    uint32_t frameItemOffset = 0;

    fragHdr = (hif_frag_hdr_t *)(&(frame->hdr[HIF_HDR_FRAG_HDR_IDX]));
    frameItemOffset =fragHdr->offset;

    HIF_Frame_Node_t *preNode = NULL;
    HIF_Frame_Node_t *nextNode = NULL;

    preNode = frameQueue->head;
    nextNode = preNode;
    while (nextNode) {
        fragHdr = (hif_frag_hdr_t *)(&(nextNode->hdr[HIF_HDR_FRAG_HDR_IDX]));
        if (frameItemOffset < fragHdr->offset) {
            break;
        }
        preNode = nextNode;
        nextNode = nextNode->next;
    }

    if (preNode == nextNode) {  // frameQueue->head == NULL or nextNode == frameQueue->head
        hif_FRAME_Prepend(frameQueue, frame);
    } else {    // preNode != NULL
        if (nextNode == NULL) {
            hif_FRAME_Append(frameQueue, frame);
        } else {
            hif_frame_item_link_add(frame, preNode);
        }
    }
}
#endif

/**
 * @brief Check if a msg item is timeout or not.
 * @param msgNode
 * @return true: Timeout
 * @return false: note timeout
 */
static bool hif_ml_msg_isTimeout(HIF_Msg_Node_t *msgNode)
{
    if (msgNode == NULL) {
        return 0;
    }

    return (wrap32_before(msgNode->endTick, OSI_GetTicks()) || (msgNode->status == HIF_ERRCODE_TIMEOUT));
}

/**
 * @brief Calculate the total length(bytes) of a data item link
 * @param link
 * @return uint32_t
 */
__hif_sram_text uint32_t hif_data_item_link_calDataLen(HIF_Data_Node_t *link)
{
    if (link == NULL) {
        return 0;
    }

    uint32_t dataLen = 0;
    HIF_Data_Node_t *dataNode = NULL;

    unsigned long key = __lock_irq();
    dataNode = link;
    while (dataNode) {
        dataLen += dataNode->len;
        dataNode = dataNode->next;
    }
    __unlock_irq((unsigned long)key);

    return dataLen;
}

#endif      /* CONFIG_HIF_SPLIT_ENA */

/**
 * @brief Ack(TLV) handler, called in msghdl_host_poll().
 * @param host_ack_tlv
 * @return uint16_t The number of frames that have been delivered to the transport-layer
 */
uint16_t hif_host_ack_tvl_handler(hif_host_poll_ack_tlv_t *host_ack_tlv)
{
    if (host_ack_tlv == NULL) {
        return 0;
    }

    hif_msg_tlv_t *msgTlv = NULL;
    uint8_t *pbuf = NULL;
    uint16_t toTL_frameCnt = 0;

    pbuf = (uint8_t *)(host_ack_tlv->msgTlv);
    for (uint8_t i = 0; i < host_ack_tlv->tlvNum; i++) {
        msgTlv = (hif_msg_tlv_t *)pbuf;
        switch (msgTlv->type) {
            case HIF_MSG_TLV_APP_ACK: { /*< Application-layer's ack */
                if ((msgTlv->msgid == 0xC1) || (msgTlv->msgid == 0xC2)) {
                    uint32_t retry_num = msgTlv->length/sizeof(HOST_CUBE_RETRY);
                    cube_report_retry_handler(&msgTlv->data[0], retry_num);
                    toTL_frameCnt += retry_num;
                }
                break;
            }
            case HIF_MSG_TLV_ML_ACK: {  /*< Fragment ack of message-layer */
                #if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
                toTL_frameCnt += hif_ml_frag_ack_handler(msgTlv);
                #endif
                break;
            }
            case HIF_MSG_TLV_TL_ACK: {
                hif_tl_retry(msgTlv->length, msgTlv->data);
                break;
            }
            default:
                HIF_LOG_DBG("Undefined ack_tlv type\n\r");
                break;
        }
        pbuf += sizeof(hif_msg_tlv_t) + msgTlv->length;
    }

    return toTL_frameCnt;
}

__attribute__((weak)) void cube_report_retry_handler(uint8_t *data, uint32_t retry_num)
{ }

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

