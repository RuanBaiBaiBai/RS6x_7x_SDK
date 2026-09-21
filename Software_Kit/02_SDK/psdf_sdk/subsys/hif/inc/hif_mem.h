/**
 ******************************************************************************
 * @file    hif_mem.h
 * @brief   hif mem define.
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



#ifndef _HIF_MEM_H_
#define _HIF_MEM_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "hal_types.h"
#include "hif_msg.h"
/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */

#define CONFIG_HIF_DATA_NODE_EXT    1

#if (CONFIG_HIF_DATA_NODE_EXT == 1)

typedef struct Data_Node {
    uint8_t             *data;
    uint32_t            rsv0;
    struct Data_Node    *next;
    uint32_t            rsv1;
    uint16_t            len;
    uint16_t            rsv2;
} HIF_Data_Node_t;

#else

typedef struct Data_Node {
    struct Data_Node    *next;
    uint8_t             *data;
    uint16_t            len;
} HIF_Data_Node_t;

#endif  /* CONFIG_HIF_PHY_DMA */

typedef struct {
    struct Data_Node    *head;
} HIF_Data_SingleHead_t;

typedef struct {
    struct Data_Node    *head;
    struct Data_Node    *tail;
} HIF_Data_DoubleHead_t;

#if (CONFIG_HIF_APP_DATA_POOL)

typedef struct {
    uint32_t nodeCnt;
    uint32_t nodeSize;
    uint8_t *buffer;
}HIF_AppPool_Cfg_t;

#endif

/**
 * 6(phy head and msgHeader) + 1(dummy) + 8(frag header)
 */
#define HIF_HDR_LEN_MAX         15
#define HIF_HDR_CHECK8_IDX      1
#define HIF_HDR_DUMMY_IDX       6
#define HIF_HDR_FRAG_HDR_IDX    7

typedef struct Frame_Node{
    struct Frame_Node       *next;

#if CONFIG_HIF_SPLIT_ENA
    uint8_t                 hdr[HIF_HDR_LEN_MAX];
#else
    uint8_t                 hdr[8];
#endif

    uint8_t                 tail[8];

    uint8_t                 flag;
    int8_t                  status;
    uint16_t                frameLen;

    HIF_Data_DoubleHead_t   listHead;
    void                    *sendCb;
    OSI_Semaphore_t         *sem;
#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
    TickType_t              endTick;
#endif
} HIF_Frame_Node_t;

typedef struct {
    struct Frame_Node   *head;
} HIF_Frame_SingleHead_t;

typedef struct {
    struct Frame_Node   *head;
    struct Frame_Node   *tail;
} HIF_Frame_DoubleHead_t;


#if CONFIG_HIF_SPLIT_ENA
/**
 * @brief Message(application) managing type
 */
typedef struct _HIF_Msg_Node_t_ {
    struct _HIF_Msg_Node_t_ *next;
    HIF_MsgHdr_t msgHeader;
    void *sendCb;                               /*< callback of application-layer */
    HIF_Data_DoubleHead_t dataQueue;            /*< Data (item link) that application needs to send */
    uint32_t dataTotalLen;                      /*< Length(byte) of the data (item link) */
    HIF_Data_Node_t *unFragDataNode;           /*< Current data item in the link, that is fragmenting */
    uint32_t unfragDataNodeOffset;              /*< Offset of Current data item, after that data is needed to fragment */
    HIF_Frame_DoubleHead_t waitAckFrameQueue;   /*< The queue consists of frames that are waiting host's ack */
    OSI_Semaphore_t *sem;                       /*< Semaphore for blocked transmitting */
    TickType_t endTick;                         /*< OS tick, after that the message sending is timeout */
    uint8_t msgSeq;                             /*< Sequence of the message */
    uint8_t fragCnt;                            /*< Counter of fragments that have been fragmented */
    uint8_t toTL_frameCnt;                      /*< Counter of frames that have been sent to transport-layer */
    uint8_t fromTL_frameCnt;                    /*< Counter of frames that have been transmitted done by transport-layer */
    int8_t status;                              /*< Status of transmitting the message */

    #define HIF_ML_FRAG_RETRY_NOT               0
    #define HIF_ML_FRAG_RETRY_PACK_ACK_NOT_YET  1
    #define HIF_ML_FRAG_RETRY_PACK_ACK_DONE     2
    #define HIF_ML_FRAG_RETRY_ACK_TRANS_FAIL    3
    #define HIF_ML_FRAG_RETRY_ACK_GOT_RETRY     4
    uint8_t fragRetryCtrlFlag;              /*< 0 - The message is not needed to wait host's ack; !0 - Need to wait.
                                                2 - Ack fragment has packaged into a frame; !2 - has not packaged */
} HIF_Msg_Node_t;

typedef struct _hif_msg_singleHead_t_{
    HIF_Msg_Node_t   *head;
} HIF_Msg_SingleHead_t;

typedef struct _HIF_Msg_DoubleHead_t_ {
    HIF_Msg_Node_t *head;
    HIF_Msg_Node_t *tail;
} HIF_Msg_DoubleHead_t;
#endif

typedef struct Sem_Node {
    struct Sem_Node     *next;

    OSI_Semaphore_t     sem;
} HIF_Sem_Node_t;

typedef struct {
    struct Sem_Node   *head;
} HIF_Sem_SingleHead_t;

typedef struct {
    struct Sem_Node   *head;
    struct Sem_Node   *tail;
} HIF_Sem_DoubleHead_t;

/* Exported macro.
 * ----------------------------------------------------------------------------
 */

#define HIF_MEM_TYPE_DATA_NODE          1
#define HIF_MEM_TYPE_FRAME_NODE         2
#define HIF_MEM_TYPE_SEM_NODE           3

/* cache line size 32 bytes */
#define HIF_MEM_ALIGN_NUM               32
#define HIF_MEM_ALIGN(addr)             (((addr) + 31) & (~31))

#define HIF_MemMalloc(size)             OSI_Malloc(size)

#define HIF_MemRealloc(mem, size)       OSI_Realloc(mem, size)

#define HIF_MemFree(mem)                OSI_Free(mem)


/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int hif_MEM_DataPoolInit(uint32_t nodeCnt);

int hif_MEM_DataPoolDeinit(void);

HIF_Data_Node_t *hif_MEM_DataNodeMalloc(void);

void hif_MEM_DataNodeFree(HIF_Data_Node_t * dataNode);

void hif_MEM_DataListFree(HIF_Data_DoubleHead_t * dataList);


int hif_MEM_FramePoolInit(uint32_t nodeCnt);

int hif_MEM_FramePoolDeinit(void);

HIF_Frame_Node_t *hif_MEM_FrameNodeMalloc(void);

void hif_MEM_FrameNodeFree(HIF_Frame_Node_t * frameNode);

void hif_MEM_FrameListFree(HIF_Frame_DoubleHead_t * frameList);

void hif_MEM_FrameNodeInit(HIF_Frame_Node_t * pFrameNode);

void hif_MEM_FrameNodeDeinit(HIF_Frame_Node_t * pFrameNode);

#if (CONFIG_HIF_APP_DATA_POOL)

int hif_MEM_AppPoolInit(HIF_AppPool_Cfg_t *poolCfg, uint8_t poolCnt);

int hif_MEM_AppPoolDeinit(void);

uint8_t *hif_MEM_AppNodeMalloc(uint8_t idx);

int hif_MEM_AppNodeFree(uint8_t *buffer);

bool hif_MEM_AppNodeListIsFull(uint8_t idx);

#endif

#if CONFIG_HIF_SPLIT_ENA
void hif_mem_free_frame(HIF_Frame_Node_t *frame, bool isFreeResInFrame);

void hif_mem_free_frameQueue(HIF_Frame_DoubleHead_t *frameQueue, bool isFreeResInFrame);
#endif

#if CONFIG_HIF_SPLIT_ENA
int hif_MEM_MsgPoolInit(uint32_t nodeCnt);

int hif_MEM_MsgPoolDeinit(void);

HIF_Msg_Node_t *hif_MEM_MsgNodeMalloc(void);

void hif_MEM_MsgNodeFree(HIF_Msg_Node_t * pMsgNode);

void hif_MEM_MsgNodeInit(HIF_Msg_Node_t * pMsgNode);

void hif_MEM_MsgNodeDeinit(HIF_Msg_Node_t * pMsgNode);

void hif_MEM_MsgListFree(HIF_Msg_DoubleHead_t * pMsgList);
#endif

int hif_MEM_SemPoolInit(uint32_t nodeCnt);

int hif_MEM_SemPoolDeinit(void);

OSI_Semaphore_t *hif_MEM_SemMalloc(void);

void hif_MEM_SemFree(OSI_Semaphore_t * pSem);


#if (CONFIG_HIF_MEM_DBG_EN == 1)
uint16_t hif_MEM_GetFreeSize(uint32_t memType);

uint16_t hif_MEM_GetMinFreeSize(uint32_t memType);

#else

#define hif_MEM_GetFreeSize(memType)
#define hif_MEM_GetMinFreeSize(memType)

#endif

/*
 * -----------------------------------------------------
 */

static inline void hif_DATA_Init(HIF_Data_DoubleHead_t *pList)
{
    unsigned long key = __lock_irq();
    pList->head = NULL;
    pList->tail = NULL;
    __unlock_irq((unsigned long)key);
}

static inline int hif_DATA_IsEmpty(HIF_Data_DoubleHead_t *pList)
{
    return ((pList != NULL) && (pList->head == NULL));
}

static inline HIF_Data_Node_t * hif_DATA_PeekHead(HIF_Data_DoubleHead_t *pList)
{
    if (pList != NULL) {
        return pList->head;
    } else {
        return NULL;
    }
}

static inline HIF_Data_Node_t * hif_DATA_PeekTail(HIF_Data_DoubleHead_t *pList)
{
    if (pList != NULL) {
        return pList->tail;
    } else {
        return NULL;
    }
}

__hif_isr_text static inline HIF_Data_Node_t * hif_DATA_PopHead(HIF_Data_DoubleHead_t *pList)
{
    HIF_Data_Node_t *pNode = NULL;

    unsigned long key = __lock_irq();
    if ((pList != NULL) && (pList->head != NULL)) {
        pNode = pList->head;
        pList->head = pNode->next;
        pNode->next = NULL;
        if (pList->head == NULL) {
            pList->tail = NULL;
        }
    }
    __unlock_irq((unsigned long)key);

    return pNode;
}

static inline HIF_Data_Node_t * hif_DATA_PopTail(HIF_Data_DoubleHead_t *pList)
{
    HIF_Data_Node_t *pNode = NULL;

    unsigned long key = __lock_irq();

    do {
        HIF_Data_Node_t *tail_node = NULL;
        HIF_Data_Node_t *prev_node = NULL;

        if (pList == NULL || (pList->head == NULL)) {
            break;
        }

        tail_node = pList->tail;
        /* Only one node in the list, pop head directly (head-tail linkage) */
        if (pList->head == pList->tail) {
            pNode = hif_DATA_PopHead(pList);
            break;
        }

        /* Traverse to find the predecessor of the tail node (only O(n) step for double-head tail pop) */
        for (prev_node = pList->head;
            prev_node != NULL && prev_node->next != tail_node;
            prev_node = prev_node->next);
        if (prev_node == NULL) {
            break; /* Theoretically unreachable, for exception prevention */
        }

        /* Set predecessor's next to NULL, move tail pointer to predecessor */
        prev_node->next = NULL;
        pList->tail = prev_node;
        tail_node->next = NULL; /* Unlink node to avoid wild pointer */

        pNode = tail_node;

    } while (0);

    __unlock_irq((unsigned long)key);


    return pNode;
}

__hif_isr_text static inline void hif_DATA_Append(HIF_Data_DoubleHead_t *pList, HIF_Data_Node_t *pNode)
{
    unsigned long key = __lock_irq();

    if ((pList != NULL) && (pNode != NULL)) {
        pNode->next = NULL;
        if (pList->head == NULL) {
            pList->head = pNode;
            pList->tail = pNode;
        } else {
            pList->tail->next = pNode;
            pList->tail = pNode;
        }
    }

    __unlock_irq((unsigned long)key);
}

__hif_sram_text static inline void hif_DATA_Prepend(HIF_Data_DoubleHead_t *pList, HIF_Data_Node_t *pNode)
{
    unsigned long key = __lock_irq();

    if ((pList != NULL) && (pNode != NULL)) {
        pNode->next = pList->head;
        pList->head = pNode;
        if (pList->tail == NULL) {
            pList->tail = pNode;
        }
    }
    __unlock_irq((unsigned long)key);

}


static inline void hif_FRAME_Init(HIF_Frame_DoubleHead_t *pList)
{
    unsigned long key = __lock_irq();
    pList->head = NULL;
    pList->tail = NULL;
    __unlock_irq((unsigned long)key);

}

static inline int hif_FRAME_IsEmpty(HIF_Frame_DoubleHead_t *pList)
{
    return ((pList != NULL) && (pList->head == NULL));
}

__hif_isr_text static inline HIF_Frame_Node_t * hif_FRAME_PopHead(HIF_Frame_DoubleHead_t *pList)
{
    HIF_Frame_Node_t *pNode = NULL;

    unsigned long key = __lock_irq();

    if ((pList != NULL) && (pList->head != NULL)) {
        pNode = pList->head;
        pList->head = pNode->next;
        pNode->next = NULL;
        if (pList->head == NULL) {
            pList->tail = NULL;
        }
    }

    __unlock_irq((unsigned long)key);


    return pNode;
}

static inline HIF_Frame_Node_t * hif_FRAME_PopTail(HIF_Frame_DoubleHead_t *pList)
{
    HIF_Frame_Node_t *pNode = NULL;

    unsigned long key = __lock_irq();

    do {
        HIF_Frame_Node_t *tail_node = NULL;
        HIF_Frame_Node_t *prev_node = NULL;

        if (pList == NULL || (pList->head == NULL)) {
            break;
        }

        tail_node = pList->tail;
        /* Only one node in the list, pop head directly (head-tail linkage) */
        if (pList->head == pList->tail) {
            pNode = hif_FRAME_PopHead(pList);
            break;
        }

        /* Traverse to find the predecessor of the tail node (only O(n) step for double-head tail pop) */
        for (prev_node = pList->head;
            prev_node != NULL && prev_node->next != tail_node;
            prev_node = prev_node->next);
        if (prev_node == NULL) {
            break; /* Theoretically unreachable, for exception prevention */
        }

        /* Set predecessor's next to NULL, move tail pointer to predecessor */
        prev_node->next = NULL;
        pList->tail = prev_node;
        tail_node->next = NULL; /* Unlink node to avoid wild pointer */

        pNode = tail_node;

    } while (0);

    __unlock_irq((unsigned long)key);


    return pNode;
}

static inline HIF_Frame_Node_t * hif_FRAME_PeekHead(HIF_Frame_DoubleHead_t *pList)
{
    if (pList != NULL) {
        return pList->head;
    } else {
        return NULL;
    }
}

static inline HIF_Frame_Node_t * hif_FRAME_PeekTail(HIF_Frame_DoubleHead_t *pList)
{
    if (pList != NULL) {
        return pList->tail;
    } else {
        return NULL;
    }
}

__hif_isr_text static inline void hif_FRAME_Append(HIF_Frame_DoubleHead_t *pList, HIF_Frame_Node_t *pNode)
{
    unsigned long key = __lock_irq();

    if ((pList != NULL) && (pNode != NULL)) {
        pNode->next = NULL;
        if (pList->head == NULL) {
            pList->head = pNode;
            pList->tail = pNode;
        } else {
            pList->tail->next = pNode;
            pList->tail = pNode;
        }
    }

    __unlock_irq((unsigned long)key);
}

static inline void hif_FRAME_Prepend(HIF_Frame_DoubleHead_t *pList, HIF_Frame_Node_t *pNode)
{
    unsigned long key = __lock_irq();

    if ((pList != NULL) && (pNode != NULL)) {
        pNode->next = pList->head;
        pList->head = pNode;
        if (pList->tail == NULL) {
            pList->tail = pNode;
        }
    }
    __unlock_irq((unsigned long)key);

}

/**
 * @brief Find and Remove a frame from a queue
 * @param frame
 * @param frameQueue
 */
static inline HIF_Frame_Node_t* hif_frameQueue_remove_frame(HIF_Frame_Node_t *frame, HIF_Frame_DoubleHead_t *frameQueue)
{
    if ((frame == NULL) || (frameQueue == NULL)) {
        return NULL;
    }

    HIF_Frame_Node_t *preNode = NULL;
    HIF_Frame_Node_t *node = NULL;

    unsigned long key = __lock_irq();
    preNode = frameQueue->head;
    node = frameQueue->head;
    while (node) {
        if (node == frame) {
            if ((node == frameQueue->head) && (node == frameQueue->tail)) {
                frameQueue->head = NULL;
                frameQueue->tail = NULL;
            } else if (node == frameQueue->head) {
                frameQueue->head = frameQueue->head->next;
            } else if (node == frameQueue->tail) {
                frameQueue->tail = preNode;
                frameQueue->tail->next = NULL;
            } else {
                preNode->next = node->next;
            }
            node->next = NULL;
            break;
        }

        preNode = node;
        node = node->next;
    }
    __unlock_irq((unsigned long)key);

    return node;
}

/**
 * @brief Add a frame after preNode of a single link list
 * @param frame
 * @param preNode
 */
static inline void hif_frame_item_link_add(HIF_Frame_Node_t *frame, HIF_Frame_Node_t *preNode)
{
    if ((frame == NULL) || (preNode == NULL)){
        return;
    }

    unsigned long key = __lock_irq();
    frame->next = preNode->next;
    preNode->next = frame;
    __unlock_irq((unsigned long)key);
}

/**
 * @brief Insert/Merge a frame queue into the tail of another queue
 * @param dstFrameQueue
 * @param srcFrameQueue
 */
static inline void frameQueue_merge_tail(HIF_Frame_DoubleHead_t *dstFrameQueue, HIF_Frame_DoubleHead_t *srcFrameQueue)
{
    if ((dstFrameQueue == NULL) || (srcFrameQueue == NULL)) {
        return;
    }

    unsigned long key = __lock_irq();
    if (dstFrameQueue->head == NULL) {
        dstFrameQueue->head = srcFrameQueue->head;
        dstFrameQueue->tail = srcFrameQueue->tail;
    } else {
        dstFrameQueue->tail->next = srcFrameQueue->head;
        dstFrameQueue->tail = srcFrameQueue->tail;
    }
    __unlock_irq((unsigned long)key);
}

#if CONFIG_HIF_SPLIT_ENA
static inline void hif_Msg_Append(HIF_Msg_DoubleHead_t *pList, HIF_Msg_Node_t *pNode)
{
    unsigned long key = __lock_irq();

    if ((pList != NULL) && (pNode != NULL)) {
        pNode->next = NULL;
        if (pList->head == NULL) {
            pList->head = pNode;
            pList->tail = pNode;
        } else {
            pList->tail->next = pNode;
            pList->tail = pNode;
        }
    }

    __unlock_irq((unsigned long)key);
}

/**
 * @brief Find and Remove a msg node from a queue
 * @param msgNode
 * @param msgQueue
 * @return HIF_Msg_Node_t*: !NULL - The msg node is found in the queue and removed from the queue;
 *                          NULL - The msg node is not found in the queue
 */
static inline HIF_Msg_Node_t* hif_ml_msgQueue_remove_item(HIF_Msg_Node_t *msgNode, HIF_Msg_DoubleHead_t *msgQueue)
{
    if ((msgNode == NULL) || (msgQueue == NULL)) {
        return NULL;
    }

    HIF_Msg_Node_t *preNode = NULL;
    HIF_Msg_Node_t *node = NULL;

    unsigned long key = __lock_irq();
    preNode = msgQueue->head;
    node = msgQueue->head;
    while (node) {
        if (node == msgNode) {
            if ((node == msgQueue->head) && (node == msgQueue->tail)) {
                msgQueue->head = NULL;
                msgQueue->tail = NULL;
            } else if (node == msgQueue->head) {
                msgQueue->head = msgQueue->head->next;
            } else if (node == msgQueue->tail) {
                msgQueue->tail = preNode;
                msgQueue->tail->next = NULL;
            } else {
                preNode->next = node->next;
            }
            node->next = NULL;
            break;
        }

        preNode = node;
        node = node->next;
    }
    __unlock_irq((unsigned long)key);

    return node;
}
#endif

#ifdef __cplusplus
}
#endif

#endif /* _HIF_MEM_H_ */
