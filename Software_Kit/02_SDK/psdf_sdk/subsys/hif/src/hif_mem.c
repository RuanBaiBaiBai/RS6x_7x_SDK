/**
 ******************************************************************************
 * @file    hif_mem.c
 * @brief   hif mem define.
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
#include "hif_mem.h"

#if (CONFIG_HIF == 1)

#include "ll_utils.h"
#include "hif_log.h"

/* Private defines.
 * ----------------------------------------------------------------------------
 */
#ifndef CONFIG_HIF_MEM_LOG_EN
#define CONFIG_HIF_MEM_LOG_EN                       0
#endif

#ifndef CONFIG_HIF_MEM_FRAME_NODE_CACHE
#define CONFIG_HIF_MEM_FRAME_NODE_CACHE             0
#endif

#ifndef CONFIG_HIF_MEM_MSG_NODE_CACHE
#define CONFIG_HIF_MEM_MSG_NODE_CACHE               0
#endif

#ifndef CONFIG_HIF_MEM_DATA_NODE_CACHE
#define CONFIG_HIF_MEM_DATA_NODE_CACHE              0
#endif

#ifndef CONFIG_HIF_MEM_SEM_NODE_CACHE
#define CONFIG_HIF_MEM_SEM_NODE_CACHE               1
#endif

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    void                    *buff;
    HIF_Data_SingleHead_t   listFree;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    uint16_t                numFree;
    uint16_t                minFree;
#endif
} HIF_MEM_DataCtrl_t;

typedef struct {
    void                    *buff;
    HIF_Frame_SingleHead_t  listFree;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    uint16_t                numFree;
    uint16_t                minFree;
#endif
} HIF_MEM_FrameCtrl_t;

#if CONFIG_HIF_SPLIT_ENA
typedef struct {
    void                    *buff;
    HIF_Msg_SingleHead_t    listFree;
    uint16_t                nodeCnt;
#if (CONFIG_HIF_MEM_DBG_EN == 1)
    uint16_t                numFree;
    uint16_t                minFree;
#endif
} HIF_MEM_MsgCtrl_t;
#endif

typedef struct {
    void                    *buff;
    HIF_Sem_SingleHead_t    listFree;
    uint16_t                nodeCnt;
#if (CONFIG_HIF_MEM_DBG_EN == 1)
    uint16_t                numFree;
    uint16_t                minFree;
#endif
} HIF_MEM_SemCtrl_t;

#if (CONFIG_HIF_APP_DATA_POOL)

typedef enum{
    MALLOC_INT_HEAP,
    MALLOC_INT_PHY,
}HIF_MEM_DataBuffer_t;

typedef struct {
    struct App_Node    *head;
} HIF_App_SingleHead_t;

typedef struct App_Node {
    uint8_t             *data;
    struct App_Node     *next;
} HIF_App_Node_t;


typedef struct {
    HIF_App_Node_t          *nodeBuff;
    HIF_App_Node_t          *alignNodeBuff;
    uint8_t                 *dataBuff;
    uint8_t                 *alignDataBuff;
    uint8_t                 *dataBuffEnd;
    HIF_App_SingleHead_t    listFree;

    uint32_t                nodeSize;
    uint16_t                numFree;
    uint16_t                totalFree;

    uint8_t                 type;
} HIF_MEM_AppCtrl_t;

static HIF_MEM_AppCtrl_t *hifMemAppCtrl = NULL;
static uint8_t hifMemCacheCnt = 0;

#endif

static HIF_MEM_DataCtrl_t hifMemDataCtrl;

static HIF_MEM_FrameCtrl_t hifMemFrameCtrl;

#if CONFIG_HIF_SPLIT_ENA
static HIF_MEM_MsgCtrl_t hifMemMsgCtrl;
#endif

static HIF_MEM_SemCtrl_t hifMemSemCtrl;


/* Private macros.
 * ----------------------------------------------------------------------------
 */

#define SLIST_OFFSET_OF(type, member)  ((size_t)&((type *)0)->member)

#define SLIST_CONTAINER_OF(ptr, type, member)  \
    ((type *)((uint8_t *)(ptr) - SLIST_OFFSET_OF(type, member)))


/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

int hif_MEM_DataPoolInit(uint32_t nodeCnt)
{
    if (hifMemDataCtrl.buff != NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    /* malloc pool */
    hifMemDataCtrl.buff = OSI_Malloc((sizeof(HIF_Data_Node_t) * nodeCnt) + HIF_MEM_ALIGN_NUM);
    if (hifMemDataCtrl.buff == NULL) {
        return HIF_ERRCODE_NO_BUFFER;
    }

    /* align cache line */
    uint32_t buffAddr = (uint32_t)hifMemDataCtrl.buff;
    buffAddr = HIF_MEM_ALIGN(buffAddr);

    /* init node, addr not cache */
    #if (CONFIG_HIF_MEM_DATA_NODE_CACHE == 1)
    HIF_Data_Node_t * pDataNode = (HIF_Data_Node_t *)buffAddr;
    #else
    HIF_Data_Node_t * pDataNode = (HIF_Data_Node_t *)LL_DeCodeAddr(buffAddr);

    csi_dcache_clean_range((unsigned long *)buffAddr, (sizeof(HIF_Data_Node_t) * nodeCnt));
    #endif

    /* init list head */
    hifMemDataCtrl.listFree.head = NULL;
    for (int32_t idx = 0; idx < nodeCnt; pDataNode++, idx++) {
        pDataNode->next = hifMemDataCtrl.listFree.head;
        hifMemDataCtrl.listFree.head = pDataNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Idx(%d), Node(%p), Next(%p)", idx, pDataNode, pDataNode->next);
#endif
#endif
    }

#if (CONFIG_HIF_MEM_DATA_NODE_CACHE == 0)
    csi_dcache_invalid_range((unsigned long *)buffAddr, (sizeof(HIF_Data_Node_t) * nodeCnt));
#endif

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemDataCtrl.numFree = nodeCnt;
    hifMemDataCtrl.minFree = nodeCnt;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Init Data Pool: Cnt(%d), Size(%d), Buff(%p), Head(%p)",
            nodeCnt, (sizeof(HIF_Data_Node_t) * nodeCnt), hifMemDataCtrl.buff, hifMemDataCtrl.listFree.head);
#endif
#endif

    return HIF_ERRCODE_SUCCESS;
}

int hif_MEM_DataPoolDeinit(void)
{
    if (hifMemDataCtrl.buff == NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    hifMemDataCtrl.listFree.head = NULL;
    OSI_Free(hifMemDataCtrl.buff);

    hifMemDataCtrl.buff = NULL;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemDataCtrl.numFree = 0;
    hifMemDataCtrl.minFree = 0;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Deinit Data Pool");
#endif
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_sram_text HIF_Data_Node_t *hif_MEM_DataNodeMalloc(void)
{
    unsigned long key = __lock_irq();

    HIF_Data_Node_t * mallocNode = NULL;

    if (hifMemDataCtrl.listFree.head != NULL) {
        mallocNode = hifMemDataCtrl.listFree.head;
        hifMemDataCtrl.listFree.head = mallocNode->next;
        mallocNode->next = NULL;
    }

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    if (mallocNode != NULL) {
        hifMemDataCtrl.numFree--;
        if (hifMemDataCtrl.minFree > hifMemDataCtrl.numFree) {
            hifMemDataCtrl.minFree = hifMemDataCtrl.numFree;
        }

#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Data Pool: Node(%p), Next(%p), Head(%p), Free(%d, %d)",
            mallocNode, mallocNode->next, hifMemDataCtrl.listFree.head, hifMemDataCtrl.numFree, hifMemDataCtrl.minFree);
#endif
    } else {
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Data Pool Fail");
#endif
    }
#endif

    __unlock_irq((unsigned long)key);

    return mallocNode;
}

__hif_isr_text void hif_MEM_DataNodeFree(HIF_Data_Node_t * pDataNode)
{
    if (pDataNode == NULL) {
        return;
    }

    unsigned long key = __lock_irq();

    pDataNode->next = hifMemDataCtrl.listFree.head;
    hifMemDataCtrl.listFree.head = pDataNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemDataCtrl.numFree++;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Free Data Pool: Node(%p), Next(%p), Head(%p), Cnt(%d, %d)",
        pDataNode, pDataNode->next, hifMemDataCtrl.listFree.head, hifMemDataCtrl.numFree, hifMemDataCtrl.minFree);
#endif
#endif

    __unlock_irq((unsigned long)key);
}

__hif_isr_text void hif_MEM_DataListFree(HIF_Data_DoubleHead_t * pDataList)
{
    if (pDataList == NULL) {
        return;
    }

    unsigned long key = __lock_irq();

    HIF_Data_Node_t *node;
    HIF_Data_Node_t *tmp;
    for (node = pDataList->head, tmp = (node != NULL) ? node->next : NULL;
        node != NULL;
        node = tmp, tmp = (node != NULL) ? node->next : NULL) {
        node->next = hifMemDataCtrl.listFree.head;
        hifMemDataCtrl.listFree.head = node;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
        hifMemDataCtrl.numFree++;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Node(%p), Next(%p)", node, node->next);
#endif
#endif
    }

    pDataList->head = NULL;
    pDataList->tail = NULL;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Free Data List: Head(%p), Cnt(%d, %d)",
            hifMemDataCtrl.listFree.head, hifMemDataCtrl.numFree, hifMemDataCtrl.minFree);
#endif
#endif

    __unlock_irq((unsigned long)key);
}


int hif_MEM_FramePoolInit(uint32_t nodeCnt)
{
    if (hifMemFrameCtrl.buff != NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    /* malloc pool */
    hifMemFrameCtrl.buff = OSI_Malloc((sizeof(HIF_Frame_Node_t) * nodeCnt) + HIF_MEM_ALIGN_NUM);
    if (hifMemFrameCtrl.buff == NULL) {
        return HIF_ERRCODE_NO_BUFFER;
    }

    /* align cache line */
    uint32_t buffAddr = (uint32_t)hifMemFrameCtrl.buff;
    buffAddr = HIF_MEM_ALIGN(buffAddr);

    /* init node, addr not cache */
#if (CONFIG_HIF_MEM_FRAME_NODE_CACHE == 1)
    HIF_Frame_Node_t * pFrameNode = (HIF_Frame_Node_t *)buffAddr;
#else
    HIF_Frame_Node_t * pFrameNode = (HIF_Frame_Node_t *)LL_DeCodeAddr(buffAddr);

    csi_dcache_clean_range((unsigned long *)buffAddr, (sizeof(HIF_Frame_Node_t) * nodeCnt));
#endif

    /* init list head */
    hifMemFrameCtrl.listFree.head = NULL;
    for (int32_t idx = 0; idx < nodeCnt; pFrameNode++, idx++) {
        pFrameNode->next = hifMemFrameCtrl.listFree.head;
        hifMemFrameCtrl.listFree.head = pFrameNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Idx(%d), Node(%p), Next(%p)", idx, pFrameNode, pFrameNode->next);
#endif
#endif
    }

#if (CONFIG_HIF_MEM_FRAME_NODE_CACHE == 0)
    csi_dcache_invalid_range((unsigned long *)buffAddr, (sizeof(HIF_Frame_Node_t) * nodeCnt));
#endif


#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemFrameCtrl.numFree = nodeCnt;
    hifMemFrameCtrl.minFree = nodeCnt;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Init Frame Pool: Cnt(%d), Size(%d), Buff(%p), Head(%p)",
            nodeCnt, (sizeof(HIF_Frame_Node_t) * nodeCnt), hifMemFrameCtrl.buff, hifMemFrameCtrl.listFree.head);
#endif
#endif

    return HIF_ERRCODE_SUCCESS;
}


int hif_MEM_FramePoolDeinit(void)
{
    if (hifMemFrameCtrl.buff == NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    hifMemFrameCtrl.listFree.head = NULL;

    OSI_Free(hifMemFrameCtrl.buff);

    hifMemFrameCtrl.buff = NULL;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemFrameCtrl.numFree = 0;
    hifMemFrameCtrl.minFree = 0;
#endif

#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Deinit Frame Pool");
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_sram_text HIF_Frame_Node_t *hif_MEM_FrameNodeMalloc(void)
{
    unsigned long key = __lock_irq();

    HIF_Frame_Node_t * mallocNode = NULL;
    if (hifMemFrameCtrl.listFree.head != NULL) {
        mallocNode = hifMemFrameCtrl.listFree.head;
        hifMemFrameCtrl.listFree.head = mallocNode->next;
        mallocNode->next = NULL;
    }

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    if (mallocNode != NULL) {
        hifMemFrameCtrl.numFree--;
        if (hifMemFrameCtrl.minFree > hifMemFrameCtrl.numFree) {
            hifMemFrameCtrl.minFree = hifMemFrameCtrl.numFree;
        }

#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Frame Pool: Node(%p), Next(%p), Head(%p), Free(%d, %d)",
            mallocNode, mallocNode->next, hifMemFrameCtrl.listFree.head, hifMemFrameCtrl.numFree, hifMemFrameCtrl.minFree);
#endif
    } else {

#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Frame Pool Fail");
#endif
    }
#endif

    __unlock_irq((unsigned long)key);

    return mallocNode;
}


__hif_isr_text void hif_MEM_FrameNodeFree(HIF_Frame_Node_t * pFrameNode)
{
    if (pFrameNode == NULL) {
        return;
    }

    unsigned long key = __lock_irq();

    pFrameNode->next = hifMemFrameCtrl.listFree.head;
    hifMemFrameCtrl.listFree.head = pFrameNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemFrameCtrl.numFree++;

#if (CONFIG_HIF_MEM_LOG_EN == 1)

    HIF_LOG_DBG("Free Frame Pool: Node(%p), Next(%p), Head(%p), Cnt(%d, %d)",
        pFrameNode, pFrameNode->next, hifMemFrameCtrl.listFree.head, hifMemFrameCtrl.numFree, hifMemFrameCtrl.minFree);
#endif
#endif

    __unlock_irq((unsigned long)key);

}


__hif_sram_text void hif_MEM_FrameNodeInit(HIF_Frame_Node_t * pFrameNode)
{
    pFrameNode->next = NULL;
    pFrameNode->flag = 0;
    pFrameNode->status = HIF_ERRCODE_SUCCESS;
    pFrameNode->frameLen = 0;

    pFrameNode->listHead.head = NULL;
    pFrameNode->listHead.tail = NULL;

    pFrameNode->sem = NULL;
    pFrameNode->sendCb = NULL;

    /* remove */
//    pFrameNode->hdrLen = 0;
//    pFrameNode->tailLen = 0;
}


void hif_MEM_FrameNodeDeinit(HIF_Frame_Node_t * pFrameNode)
{
    if (pFrameNode == NULL) {
        return;
    }

    if (pFrameNode->sem != NULL) {
        hif_MEM_SemFree(pFrameNode->sem);
    }

    hif_MEM_DataListFree(&pFrameNode->listHead);

    pFrameNode->flag = 0;
    pFrameNode->status = HIF_ERRCODE_SUCCESS;
    pFrameNode->frameLen = 0;

    pFrameNode->sem = NULL;
    pFrameNode->sendCb = NULL;

    hif_MEM_FrameNodeFree(pFrameNode);
}


__hif_sram_text void hif_MEM_FrameListFree(HIF_Frame_DoubleHead_t * pFrameList)
{
    if (pFrameList == NULL) {
        return;
    }

    unsigned long key = __lock_irq();

    HIF_Frame_Node_t *node;
    HIF_Frame_Node_t *tmp;
    for (node = pFrameList->head, tmp = (node != NULL) ? node->next : NULL;
        node != NULL;
        node = tmp, tmp = (node != NULL) ? node->next : NULL) {

        node->next = hifMemFrameCtrl.listFree.head;
        hifMemFrameCtrl.listFree.head = node;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
        hifMemFrameCtrl.numFree++;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Node(%p), Next(%p)", node, node->next);
#endif
#endif
    }

    pFrameList->head = NULL;
    pFrameList->tail = NULL;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Free Frame List: Head(%p), Cnt(%d, %d)",
            hifMemFrameCtrl.listFree.head, hifMemFrameCtrl.numFree, hifMemFrameCtrl.minFree);
#endif
#endif

    __unlock_irq((unsigned long)key);
}

#if CONFIG_HIF_SPLIT_ENA
/**
 * @brief Free a frame
 * @param frame
 * @param isFreeResInFrame true(1/!0) - Free all resource in frame, including sem, dataNode etc.
 *                         0 - No free
 */
__hif_sram_text void hif_mem_free_frame(HIF_Frame_Node_t *frame, bool isFreeResInFrame)
{
    if (frame == NULL) {
        return;
    }

    if (isFreeResInFrame) {
        if (frame->listHead.head != NULL) {
            hif_MEM_DataListFree(&(frame->listHead));
        }

        if (frame->sem != NULL) {
            hif_MEM_SemFree(frame->sem);
        }
    }

    memset(frame, 0, sizeof(HIF_Frame_Node_t));
    hif_MEM_FrameNodeFree(frame);

    frame = NULL;
}

/**
 * @brief Free all frames in a frameQueue
 * @param frameQueue
 * @param isFreeResInFrame true(1/!0) - Free all resource in frame; include sem, dataNode etc.
 *                       0 - No free
 */
__hif_sram_text void hif_mem_free_frameQueue(HIF_Frame_DoubleHead_t *frameQueue, bool isFreeResInFrame)
{
    if (frameQueue == NULL) {
        return;
    }

    if (frameQueue->head == NULL) {
        return;
    }

    if (isFreeResInFrame) {
        HIF_Frame_Node_t *frame = frameQueue->head;
        while (frame) {
            if (frame->listHead.head != NULL) {
                hif_MEM_DataListFree(&(frame->listHead));
                memset(&(frame->listHead), 0, sizeof(HIF_Data_DoubleHead_t));
            }

            if (frame->sem != NULL) {
                hif_MEM_SemFree(frame->sem);
                frame->sem = NULL;
            }

            frame = frame->next;
        }
    }

    hif_MEM_FrameListFree(frameQueue);

    memset(frameQueue, 0, sizeof(HIF_Frame_DoubleHead_t));
}
#endif

#if CONFIG_HIF_SPLIT_ENA
int hif_MEM_MsgPoolInit(uint32_t nodeCnt)
{
    if (hifMemMsgCtrl.buff != NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    /* malloc pool */
    hifMemMsgCtrl.buff = OSI_Malloc((sizeof(HIF_Msg_Node_t) * nodeCnt) + HIF_MEM_ALIGN_NUM);
    if (hifMemMsgCtrl.buff == NULL) {
        return HIF_ERRCODE_NO_BUFFER;
    }

    /* align cache line */
    uint32_t buffAddr = (uint32_t)hifMemMsgCtrl.buff;
    buffAddr = HIF_MEM_ALIGN(buffAddr);

    /* init node, addr not cache */
#if (CONFIG_HIF_MEM_MSG_NODE_CACHE == 1)
    HIF_Msg_Node_t * pMsgNode = (HIF_Msg_Node_t *)buffAddr;
#else
    HIF_Msg_Node_t * pMsgNode = (HIF_Msg_Node_t *)LL_DeCodeAddr(buffAddr);
#endif

    /* init list head */
    hifMemMsgCtrl.listFree.head = NULL;
    for (int32_t idx = 0; idx < nodeCnt; pMsgNode++, idx++) {
        pMsgNode->next = hifMemMsgCtrl.listFree.head;
        hifMemMsgCtrl.listFree.head = pMsgNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Idx(%d), Node(%p), Next(%p)", idx, pMsgNode, pMsgNode->next);
#endif
#endif
    }

#if (CONFIG_HIF_MEM_MSG_NODE_CACHE == 1)
    csi_dcache_clean_range((unsigned long *)buffAddr, (sizeof(HIF_Msg_Node_t) * nodeCnt));
#else
    csi_dcache_invalid_range((unsigned long *)buffAddr, (sizeof(HIF_Msg_Node_t) * nodeCnt));
#endif


#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemMsgCtrl.numFree = nodeCnt;
    hifMemMsgCtrl.minFree = nodeCnt;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Init Msg Pool: Cnt(%d), Size(%d), Buff(%p), Head(%p)",
            nodeCnt, (sizeof(HIF_Msg_Node_t) * nodeCnt), hifMemMsgCtrl.buff, hifMemMsgCtrl.listFree.head);
#endif
#endif

    return HIF_ERRCODE_SUCCESS;
}


int hif_MEM_MsgPoolDeinit(void)
{
    if (hifMemMsgCtrl.buff == NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    hifMemMsgCtrl.listFree.head = NULL;

    OSI_Free(hifMemMsgCtrl.buff);

    hifMemMsgCtrl.buff = NULL;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemMsgCtrl.numFree = 0;
    hifMemMsgCtrl.minFree = 0;
#endif

#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Deinit Msg Pool");
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_sram_text HIF_Msg_Node_t *hif_MEM_MsgNodeMalloc(void)
{
    unsigned long key = __lock_irq();

    HIF_Msg_Node_t * mallocNode = NULL;
    if (hifMemMsgCtrl.listFree.head != NULL) {
        mallocNode = hifMemMsgCtrl.listFree.head;
        hifMemMsgCtrl.listFree.head = mallocNode->next;
        mallocNode->next = NULL;
    }

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    if (mallocNode != NULL) {
        hifMemMsgCtrl.numFree--;
        if (hifMemMsgCtrl.minFree > hifMemMsgCtrl.numFree) {
            hifMemMsgCtrl.minFree = hifMemMsgCtrl.numFree;
        }

#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Msg Pool: Node(%p), Next(%p), Head(%p), Free(%d, %d)",
            mallocNode, mallocNode->next, hifMemMsgCtrl.listFree.head, hifMemMsgCtrl.numFree, hifMemMsgCtrl.minFree);
#endif
    } else {

#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Msg Pool Fail");
#endif
    }
#endif

    __unlock_irq((unsigned long)key);

    return mallocNode;
}


__hif_sram_text void hif_MEM_MsgNodeFree(HIF_Msg_Node_t * pMsgNode)
{
    if (pMsgNode == NULL) {
        return;
    }

    unsigned long key = __lock_irq();

    pMsgNode->next = hifMemMsgCtrl.listFree.head;
    hifMemMsgCtrl.listFree.head = pMsgNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemMsgCtrl.numFree++;

#if (CONFIG_HIF_MEM_LOG_EN == 1)

    HIF_LOG_DBG("Free Msg Pool: Node(%p), Next(%p), Head(%p), Cnt(%d, %d)",
        pMsgNode, pMsgNode->next, hifMemMsgCtrl.listFree.head, hifMemMsgCtrl.numFree, hifMemMsgCtrl.minFree);
#endif
#endif

    __unlock_irq((unsigned long)key);

}


void hif_MEM_MsgNodeInit(HIF_Msg_Node_t * pMsgNode)
{
    if (pMsgNode == NULL) {
        return;
    }

    memset(pMsgNode, 0, sizeof(HIF_Msg_Node_t));
}


void hif_MEM_MsgNodeDeinit(HIF_Msg_Node_t * pMsgNode)
{
    if (pMsgNode == NULL) {
        return;
    }

	memset(pMsgNode, 0, sizeof(HIF_Msg_Node_t));
}


__hif_sram_text void hif_MEM_MsgListFree(HIF_Msg_DoubleHead_t * pMsgList)
{
    if (pMsgList == NULL) {
        return;
    }

    unsigned long key = __lock_irq();

    HIF_Msg_Node_t *node;
    HIF_Msg_Node_t *tmp;
    for (node = pMsgList->head, tmp = (node != NULL) ? node->next : NULL;
        node != NULL;
        node = tmp, tmp = (node != NULL) ? node->next : NULL) {

        node->next = hifMemMsgCtrl.listFree.head;
        hifMemMsgCtrl.listFree.head = node;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
        hifMemMsgCtrl.numFree++;
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Node(%p), Next(%p)", node, node->next);
#endif
#endif
    }

    pMsgList->head = NULL;
    pMsgList->tail = NULL;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Free Msg List: Head(%p), Cnt(%d, %d)",
            hifMemMsgCtrl.listFree.head, hifMemMsgCtrl.numFree, hifMemMsgCtrl.minFree);
#endif
#endif

    __unlock_irq((unsigned long)key);
}
#endif


int hif_MEM_SemPoolInit(uint32_t nodeCnt)
{
    if (hifMemSemCtrl.buff != NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    /* malloc pool */
    hifMemSemCtrl.buff = OSI_Malloc((sizeof(HIF_Sem_Node_t) * nodeCnt) + HIF_MEM_ALIGN_NUM);
    if (hifMemSemCtrl.buff == NULL) {
        return HIF_ERRCODE_NO_BUFFER;
    }


    /* align cache line */
    uint32_t buffAddr = (uint32_t)hifMemSemCtrl.buff;
    buffAddr = HIF_MEM_ALIGN(buffAddr);

    /* init node, addr cache */
#if (CONFIG_HIF_MEM_SEM_NODE_CACHE == 1)
    HIF_Sem_Node_t * pSemNode = (HIF_Sem_Node_t *)buffAddr;
#else
    HIF_Sem_Node_t * pSemNode = (HIF_Sem_Node_t *)LL_DeCodeAddr(buffAddr);

    csi_dcache_clean_range((unsigned long *)buffAddr, (sizeof(HIF_Sem_Node_t) * nodeCnt));
#endif

    OSI_Status_t osStatus = OSI_STATUS_OK;
    hifMemSemCtrl.listFree.head = NULL;
    for (int32_t idx = 0; idx < nodeCnt; pSemNode++, idx++) {
        OSI_SemaphoreSetInvalid(&pSemNode->sem);
        osStatus = OSI_SemaphoreCreateBinary(&pSemNode->sem);
        if (osStatus != OSI_STATUS_OK){
            return HIF_ERRCODE_NO_BUFFER;
        }

        pSemNode->next = hifMemSemCtrl.listFree.head;
        hifMemSemCtrl.listFree.head = pSemNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Idx(%d), Node(%p), Next(%p)", idx, pSemNode, pSemNode->next);
#endif
#endif
    }

    hifMemSemCtrl.nodeCnt = nodeCnt;

#if (CONFIG_HIF_MEM_SEM_NODE_CACHE == 0)
    csi_dcache_invalid_range((unsigned long *)buffAddr, (sizeof(HIF_Sem_Node_t) * nodeCnt));
#endif


#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemSemCtrl.numFree = nodeCnt;
    hifMemSemCtrl.minFree = nodeCnt;

#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Init Sem Pool: Cnt(%d), Size(%d), Buff(%p), Head(%p)",
            nodeCnt, (sizeof(HIF_Frame_Node_t) * nodeCnt), hifMemSemCtrl.buff, hifMemSemCtrl.listFree.head);
#endif
#endif

    return HIF_ERRCODE_SUCCESS;
}


int hif_MEM_SemPoolDeinit(void)
{
    if (hifMemSemCtrl.buff == NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    HIF_Sem_Node_t * node = (HIF_Sem_Node_t *)HIF_MEM_ALIGN((uint32_t)hifMemSemCtrl.buff);
    for (uint32_t idx = 0; idx < hifMemSemCtrl.nodeCnt; idx++) {
        OSI_SemaphoreDelete(&(node[idx].sem));
    }

    hifMemSemCtrl.listFree.head = NULL;

    OSI_Free(hifMemSemCtrl.buff);

    hifMemSemCtrl.buff = NULL;
    hifMemSemCtrl.nodeCnt = 0;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
    hifMemSemCtrl.numFree = 0;
    hifMemSemCtrl.minFree = 0;
#endif

#if (CONFIG_HIF_MEM_LOG_EN == 1)
    HIF_LOG_DBG("Deinit Sem Pool");
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_sram_text OSI_Semaphore_t *hif_MEM_SemMalloc(void)
{
    unsigned long key = __lock_irq();

    HIF_Sem_SingleHead_t * pHifSemFree = (HIF_Sem_SingleHead_t *)(&hifMemSemCtrl.listFree);

    HIF_Sem_Node_t * mallocNode = NULL;

    if (pHifSemFree->head != NULL) {
        mallocNode = pHifSemFree->head;
        pHifSemFree->head = mallocNode->next;
        mallocNode->next = NULL;
    }

    __unlock_irq((unsigned long)key);

    if (mallocNode != NULL) {

#if (CONFIG_HIF_MEM_DBG_EN == 1)
        hifMemSemCtrl.numFree--;
        if (hifMemSemCtrl.minFree > hifMemSemCtrl.numFree) {
            hifMemSemCtrl.minFree = hifMemSemCtrl.numFree;
        }
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Sem Pool: Node(%p), Next(%p), Head(%p), Free(%d, %d)",
            mallocNode, mallocNode->next, hifMemSemCtrl.listFree.head, hifMemSemCtrl.numFree, hifMemSemCtrl.minFree);
#endif
#endif

        return &mallocNode->sem;
    } else {
#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
        HIF_LOG_DBG("Malloc Sem Pool Fail");
#endif
#endif
        return NULL;
    }
}


__hif_sram_text void hif_MEM_SemFree(OSI_Semaphore_t * pSem)
{
    if (pSem == NULL) {
        return;
    }

    unsigned long key = __lock_irq();

    HIF_Sem_SingleHead_t * pHifSemFree = (HIF_Sem_SingleHead_t *)(&hifMemSemCtrl.listFree);
    HIF_Sem_Node_t *semNode = SLIST_CONTAINER_OF(pSem, HIF_Sem_Node_t, sem);

    semNode->next = pHifSemFree->head;
    pHifSemFree->head = semNode;

#if (CONFIG_HIF_MEM_DBG_EN == 1)
#if (CONFIG_HIF_MEM_LOG_EN == 1)
    hifMemSemCtrl.numFree++;

    HIF_LOG_DBG("Free Frame Pool: Node(%p), Next(%p), Head(%p), Cnt(%d, %d)",
        semNode, semNode->next, hifMemSemCtrl.listFree.head, hifMemSemCtrl.numFree, hifMemSemCtrl.minFree);
#endif
#endif

    __unlock_irq((unsigned long)key);
}

#if (CONFIG_HIF_APP_DATA_POOL)

int hif_MEM_AppPoolInit(HIF_AppPool_Cfg_t *poolCfg, uint8_t poolCnt)
{
    int ret = HIF_ERRCODE_SUCCESS;

    if(poolCfg == NULL || poolCnt == 0) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    for(uint8_t i=0; i< poolCnt; i++) {
        if(poolCfg[i].nodeCnt == 0 || poolCfg[i].nodeSize == 0) {
            return HIF_ERRCODE_INVALID_PARAM;
        }
    }

    hifMemCacheCnt = 0;
    do {
        hifMemAppCtrl = (HIF_MEM_AppCtrl_t *)OSI_Malloc(poolCnt * sizeof(HIF_MEM_AppCtrl_t));
        if(hifMemAppCtrl == NULL) {
            ret = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        memset(hifMemAppCtrl, 0, poolCnt * sizeof(HIF_MEM_AppCtrl_t));
        hifMemCacheCnt = poolCnt;

        uint32_t buffAddr = 0;
        for(uint8_t i=0; i< poolCnt; i++) {
            hifMemAppCtrl[i].nodeBuff = (HIF_App_Node_t *)OSI_Malloc(sizeof(HIF_App_Node_t) * poolCfg[i].nodeCnt + HIF_MEM_ALIGN_NUM);
            if (hifMemAppCtrl[i].nodeBuff == NULL) {
                ret = HIF_ERRCODE_NO_BUFFER;
                break;
            }

            buffAddr = (uint32_t)hifMemAppCtrl[i].nodeBuff;
            buffAddr = HIF_MEM_ALIGN(buffAddr);
            hifMemAppCtrl[i].alignNodeBuff = (HIF_App_Node_t *)buffAddr;

            uint32_t blockSize = poolCfg[i].nodeSize;

            /* malloc pool */
            if(poolCfg[i].buffer == NULL) {
                hifMemAppCtrl[i].dataBuff = OSI_Malloc((blockSize * poolCfg[i].nodeCnt) + HIF_MEM_ALIGN_NUM);
                if (hifMemAppCtrl[i].dataBuff == NULL) {
                    ret = HIF_ERRCODE_NO_BUFFER;
                    break;
                }

                buffAddr = (uint32_t)hifMemAppCtrl[i].dataBuff;
                buffAddr = HIF_MEM_ALIGN(buffAddr);
                hifMemAppCtrl[i].alignDataBuff = (uint8_t *)buffAddr;
                /* init node, addr not cache */
#if (CONFIG_HIF_MEM_DATA_NODE_CACHE == 0)
                csi_dcache_clean_range((unsigned long *)hifMemAppCtrl[i].alignDataBuff, (blockSize * poolCfg[i].nodeCnt));
#endif
                hifMemAppCtrl[i].type = MALLOC_INT_HEAP;
            } else {
                hifMemAppCtrl[i].dataBuff = poolCfg[i].buffer;
                hifMemAppCtrl[i].alignDataBuff = (uint8_t *)hifMemAppCtrl[i].dataBuff;
                hifMemAppCtrl[i].type = MALLOC_INT_PHY;
            }

            hifMemAppCtrl[i].nodeSize = poolCfg[i].nodeSize;
            hifMemAppCtrl[i].numFree = poolCfg[i].nodeCnt;
            hifMemAppCtrl[i].totalFree = poolCfg[i].nodeCnt;
            hifMemAppCtrl[i].dataBuffEnd = (uint8_t *)(hifMemAppCtrl[i].alignDataBuff + blockSize * poolCfg[i].nodeCnt);

            /* init list head */
            hifMemAppCtrl[i].listFree.head = NULL;

            HIF_App_Node_t *pDataNode = hifMemAppCtrl[i].alignNodeBuff;

            for (uint32_t idx = 0; idx < poolCfg[i].nodeCnt; idx++) {
                pDataNode->data = (uint8_t *)(&hifMemAppCtrl[i].alignDataBuff[blockSize*idx]);
                pDataNode->next = hifMemAppCtrl[i].listFree.head;
                hifMemAppCtrl[i].listFree.head = pDataNode;
                pDataNode++;
            }

            if(poolCfg[i].buffer == NULL) {
#if (CONFIG_HIF_MEM_DATA_NODE_CACHE == 0)
                csi_dcache_invalid_range((unsigned long *)hifMemAppCtrl[i].alignDataBuff, (blockSize * poolCfg[i].nodeCnt));
#endif
            }
        }
    } while(0);

    if(ret) {
        hif_MEM_AppPoolDeinit();
    }

    return ret;
}

int hif_MEM_AppPoolDeinit(void)
{
    int ret = HIF_ERRCODE_SUCCESS;

    if(hifMemAppCtrl) {
        for (uint8_t i=0; i<hifMemCacheCnt; i++)  {
            hifMemAppCtrl[i].listFree.head = NULL;
            if(hifMemAppCtrl[i].nodeBuff) {
                OSI_Free(hifMemAppCtrl[i].nodeBuff);
                hifMemAppCtrl[i].nodeBuff = NULL;
                hifMemAppCtrl[i].alignNodeBuff = NULL;
            }

            if (hifMemAppCtrl[i].type == MALLOC_INT_HEAP) {
                if(hifMemAppCtrl[i].dataBuff) {
                    OSI_Free(hifMemAppCtrl[i].dataBuff);
                    hifMemAppCtrl[i].dataBuff = NULL;
                    hifMemAppCtrl[i].alignDataBuff = NULL;
                }
            }
        }
        memset(hifMemAppCtrl, 0, hifMemCacheCnt * sizeof(HIF_MEM_AppCtrl_t));
        OSI_Free(hifMemAppCtrl);
        hifMemAppCtrl = NULL;
    }

    hifMemCacheCnt = 0;

    return ret;
}


__hif_sram_text uint8_t *hif_MEM_AppNodeMalloc(uint8_t idx)
{
    uint8_t *pBuffer = NULL;

    if(idx >= hifMemCacheCnt) {
        return NULL;
    }

    if(hifMemAppCtrl == NULL) {
        return NULL;
    }

    unsigned long key = __lock_irq();

    HIF_App_Node_t * mallocNode = NULL;

    if (hifMemAppCtrl[idx].listFree.head != NULL) {
        mallocNode = hifMemAppCtrl[idx].listFree.head;
        hifMemAppCtrl[idx].listFree.head = mallocNode->next;
        mallocNode->next = NULL;
    }

    if (mallocNode != NULL) {
        pBuffer = mallocNode->data;
        hifMemAppCtrl[idx].numFree--;
    }

    __unlock_irq((unsigned long)key);

    return pBuffer;
}

__hif_sram_text int hif_MEM_AppNodeFree(uint8_t *buffer)
{
    int ret = HIF_ERRCODE_SUCCESS;
    uint16_t nodeIdx = 0;

    if (buffer == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    unsigned long key = __lock_irq();

    for(uint8_t i=0; i<hifMemCacheCnt; i++) {
        if(buffer >= hifMemAppCtrl[i].alignDataBuff && buffer < (hifMemAppCtrl[i].dataBuffEnd)) {
            nodeIdx = (buffer - hifMemAppCtrl[i].alignDataBuff) / hifMemAppCtrl[i].nodeSize;
            HIF_App_Node_t *pDataNode = &hifMemAppCtrl[i].alignNodeBuff[nodeIdx];

            pDataNode->next = hifMemAppCtrl[i].listFree.head;
            hifMemAppCtrl[i].listFree.head = pDataNode;

            hifMemAppCtrl[i].numFree++;

            break;
        }
    }
    __unlock_irq((unsigned long)key);

    return ret;
}

__hif_sram_text bool hif_MEM_AppNodeListIsFull(uint8_t idx)
{
    if(idx < hifMemCacheCnt) {
        return (hifMemAppCtrl[idx].numFree == hifMemAppCtrl[idx].totalFree);
    } else {
        return false;
    }
}

#endif

#if (CONFIG_HIF_MEM_DBG_EN == 1)
uint16_t hif_MEM_GetFreeSize(uint32_t memType)
{
    uint16_t Num = 0;

    unsigned long key = __lock_irq();

    if (memType == HIF_MEM_TYPE_DATA_NODE) {
        Num = hifMemDataCtrl.numFree;
    } else if (memType == HIF_MEM_TYPE_FRAME_NODE) {
        Num = hifMemFrameCtrl.numFree;
    } else if (memType == HIF_MEM_TYPE_SEM_NODE) {
        Num = hifMemSemCtrl.numFree;
    } else {
        Num = 0xFFFF;
    }
    __unlock_irq((unsigned long)key);

    return Num;
}


uint16_t hif_MEM_GetMinFreeSize(uint32_t memType)
{
    uint16_t Num = 0;

    unsigned long key = __lock_irq();

    if (memType == HIF_MEM_TYPE_DATA_NODE) {
        Num = hifMemDataCtrl.minFree;
    } else if (memType == HIF_MEM_TYPE_FRAME_NODE) {
        Num = hifMemFrameCtrl.minFree;
    } else if (memType == HIF_MEM_TYPE_SEM_NODE) {
        Num = hifMemSemCtrl.minFree;
    } else {
        Num = 0xFFFF;
    }

    __unlock_irq((unsigned long)key);

    return Num;
}

#endif  /* CONFIG_HIF_MEM_DBG_EN */





#endif /* CONFIG_HIF */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
