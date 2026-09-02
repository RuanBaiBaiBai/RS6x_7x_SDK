/**
 ******************************************************************************
 * @file    hif_phy.c
 * @brief   hif phy define.
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
#include "hif_checksum.h"

#include "hif_phy.h"
#include "hif_phy_uart.h"
#include "hif_phy_i2c.h"
#include "hif_phy_spi.h"
#include "hif_phy_can.h"

#include "hif_log.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */

#define HIF_PHY_PARSE_STATE_IDLE        0
#define HIF_PHY_PARSE_STATE_BUSY        1
#define HIF_PHY_PARSE_STATE_PART        2
#define HIF_PHY_PARSE_STATE_FILL        3
#define HIF_PHY_PARSE_STATE_DONE        4


typedef struct {
    uint16_t            frameStart;
    uint16_t            frameSize;
    uint16_t            frameOffset;

    uint16_t            recvOffset;
    uint16_t            recvSize;
    uint8_t             recvState;
    int8_t              recvStatus;
} HIF_PHY_RecvCtrl_t;


typedef struct {
    HIF_Data_Node_t *   pSendNode;
    uint16_t            sendOffset;
    uint8_t             sendState;
    int8_t              sendStatus;
    uint32_t            flag;
} HIF_PHY_SendCtrl_t;


#if ((CONFIG_HIF_PHY_SEND_DMA == 1) || (CONFIG_HIF_PHY_SEND_INT == 1))
static HIF_PHY_RecvCtrl_t hifPHYRecvCtrl;
#endif

#if ((CONFIG_HIF_PHY_SEND_DMA == 1) || (CONFIG_HIF_PHY_SEND_INT == 1))
static HIF_PHY_SendCtrl_t hifPHYSendCtrl;
#endif

HIF_PHY_Ctrl_t hifPHYCtrl;

/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private variables.
 * ----------------------------------------------------------------------------
 */

/* Private macros.
 * ----------------------------------------------------------------------------
 */
#if ((CONFIG_HIF_PHY_RECV_DMA == 1) || (CONFIG_HIF_PHY_RECV_INT == 1))

#define hif_PHY_ReadRecvData(readSize, readStatus)      \
        do { \
            if ((hifPHYRecvCtrl.recvOffset + (readSize)) <= hifPHYRecvCtrl.recvSize) { \
                hifPHYRecvCtrl.recvOffset += (readSize);   \
                readStatus = (readSize); \
            } else { \
                readStatus = HIF_ERRCODE_IO_ERROR; \
            } \
        } while(0)

#define hif_PHY_InitRecvParse()    \
        do {\
            hifPHYRecvCtrl.recvSize = 0; \
            hifPHYRecvCtrl.frameStart = 0; \
            hifPHYRecvCtrl.recvOffset = 0; \
            hifPHYRecvCtrl.frameSize = 0; \
            hifPHYRecvCtrl.frameOffset = 0; \
            hifPHYRecvCtrl.recvStatus = HIF_ERRCODE_SUCCESS; \
            hifPHYRecvCtrl.recvState = HIF_PHY_PARSE_STATE_IDLE; \
        } while (0)
#endif

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static uint32_t hif_PHY_RecvParse(uint8_t *pBuff);
static uint32_t hif_PHY_SendParse(uint32_t ctrl);

extern void hif_phy_io_set(void);
extern void hif_phy_io_clr(void);


/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int hif_PHY_Init(HIF_PHY_InitCfg_t *pInitCfg)
{
    int status = HIF_ERRCODE_SUCCESS;

    if (pInitCfg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    hifPHYCtrl.phyType  = pInitCfg->phyType;
    hifPHYCtrl.devId    = pInitCfg->devId;
    hifPHYCtrl.sendMode = pInitCfg->sendMode;
    hifPHYCtrl.recvMode = pInitCfg->recvMode;

    hifPHYCtrl.sendType = HIF_PHY_SEND_TYPE_OTHER;

    hifPHYCtrl.sendDummy = 0;
    HIF_LOG_DBG("HIF PHY: type(%d), id(%d)", hifPHYCtrl.phyType, hifPHYCtrl.devId);

    switch (pInitCfg->phyType) {
    case HIF_PHY_TYPE_DIS:
        break;

#if (CONFIG_HIF_PHY_UART != 0)
    case HIF_PHY_TYPE_UART:
        hifPHYCtrl.phyInit      = hif_PHY_UART_Init;
        hifPHYCtrl.phyDeinit    = hif_PHY_UART_Deinit;
        hifPHYCtrl.phyOpen      = hif_PHY_UART_Open;
        hifPHYCtrl.phyClose     = hif_PHY_UART_Close;
        hifPHYCtrl.phySend      = hif_PHY_UART_Send;
        hifPHYCtrl.phyRecv      = hif_PHY_UART_Recv;
        hifPHYCtrl.phySendStart = hif_PHY_UART_SendStart;
        hifPHYCtrl.phySendStop  = hif_PHY_UART_SendStop;
        hifPHYCtrl.phySendIsDone = hif_PHY_UART_SendIsDone;
        hifPHYCtrl.phyRecvStart = hif_PHY_UART_RecvStart;
        hifPHYCtrl.phyRecvStop  = hif_PHY_UART_RecvStop;

#if (CONFIG_HIF_PM == 1)
        hifPHYCtrl.phyPmInit    = hif_PHY_UART_PmInit;
        hifPHYCtrl.phyPmDeinit  = hif_PHY_UART_PmDeinit;
#endif
        break;
#endif /* CONFIG_HIF_PHY_UART */


#if (CONFIG_HIF_PHY_IIC != 0)
    case HIF_PHY_TYPE_IIC:
        hifPHYCtrl.phyInit      = hif_PHY_IIC_Init;
        hifPHYCtrl.phyDeinit    = hif_PHY_IIC_Deinit;
        hifPHYCtrl.phyOpen      = hif_PHY_IIC_Open;
        hifPHYCtrl.phyClose     = hif_PHY_IIC_Close;
        hifPHYCtrl.phySend      = hif_PHY_IIC_Send;
        hifPHYCtrl.phyRecv      = hif_PHY_IIC_Recv;
        hifPHYCtrl.phySendStart = hif_PHY_IIC_SendStart;
        hifPHYCtrl.phySendStop  = hif_PHY_IIC_SendStop;
        hifPHYCtrl.phySendIsDone = NULL;
        hifPHYCtrl.phyRecvStart = hif_PHY_IIC_RecvStart;
        hifPHYCtrl.phyRecvStop  = hif_PHY_IIC_RecvStop;

#if (CONFIG_HIF_PM == 1)
        hifPHYCtrl.phyPmInit    = hif_PHY_IIC_PmInit;
        hifPHYCtrl.phyPmDeinit  = hif_PHY_IIC_PmDeinit;
#endif
        break;
#endif /* CONFIG_HIF_PHY_IIC */


#if (CONFIG_HIF_PHY_SPI != 0)
    case HIF_PHY_TYPE_SPI:
        hifPHYCtrl.phyInit      = hif_PHY_SPI_Init;
        hifPHYCtrl.phyDeinit    = hif_PHY_SPI_Deinit;
        hifPHYCtrl.phyOpen      = hif_PHY_SPI_Open;
        hifPHYCtrl.phyClose     = hif_PHY_SPI_Close;
        hifPHYCtrl.phySend      = hif_PHY_SPI_Send;
        hifPHYCtrl.phyRecv      = hif_PHY_SPI_Recv;
        hifPHYCtrl.phySendStart = hif_PHY_SPI_SendStart;
        hifPHYCtrl.phySendStop  = hif_PHY_SPI_SendStop;
        hifPHYCtrl.phySendIsDone = NULL;
        hifPHYCtrl.phyRecvStart = hif_PHY_SPI_RecvStart;
        hifPHYCtrl.phyRecvStop  = hif_PHY_SPI_RecvStop;
#if (CONFIG_HIF_PM == 1)
        hifPHYCtrl.phyPmInit    = hif_PHY_SPI_PmInit;
        hifPHYCtrl.phyPmDeinit  = hif_PHY_SPI_PmDeinit;
#endif
        break;
#endif /* CONFIG_HIF_PHY_SPI */


#if (CONFIG_HIF_PHY_CAN != 0)
    case HIF_PHY_TYPE_CAN:
        hifPHYCtrl.phyInit      = hif_PHY_CAN_Init;
        hifPHYCtrl.phyDeinit    = hif_PHY_CAN_Deinit;
        hifPHYCtrl.phyOpen      = hif_PHY_CAN_Open;
        hifPHYCtrl.phyClose     = hif_PHY_CAN_Close;
        hifPHYCtrl.phySend      = hif_PHY_CAN_Send;
        hifPHYCtrl.phyRecv      = hif_PHY_CAN_Recv;
        hifPHYCtrl.phySendStart = hif_PHY_CAN_SendStart;
        hifPHYCtrl.phySendStop  = hif_PHY_CAN_SendStop;
        hifPHYCtrl.phySendIsDone = NULL;
        hifPHYCtrl.phyRecvStart = hif_PHY_CAN_RecvStart;
        hifPHYCtrl.phyRecvStop  = hif_PHY_CAN_RecvStop;
#if (CONFIG_HIF_PM == 1)
        hifPHYCtrl.phyPmInit    = hif_PHY_CAN_PmInit;
        hifPHYCtrl.phyPmDeinit  = hif_PHY_CAN_PmDeinit;
#endif
        break;
#endif /* CONFIG_HIF_PHY_CAN */

    default:
        status = HIF_ERRCODE_INVALID_PARAM;
    }

    if (status != HIF_ERRCODE_SUCCESS) {
        return status;
    }

    if (hifPHYCtrl.phyInit != NULL) {
        status = hifPHYCtrl.phyInit(pInitCfg);
    }

    hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
    hifPHYCtrl.recvState = HIF_TRANS_STATE_IDLE;

    return status;
}


int hif_PHY_Deinit(void)
{
    int status = HIF_ERRCODE_INVALID_PARAM;

    if (hifPHYCtrl.phyInit != NULL) {
        status = hifPHYCtrl.phyDeinit();
    }

    return status;
}


__hif_sram_text int hif_PHY_SendStart(HIF_Data_Node_t * pDataList)
{
    if(hifPHYCtrl.phyType == HIF_PHY_TYPE_UART) {//uart passive report need delay
        hif_phy_io_set();
    }

    unsigned long key = __lock_irq();

    int status = hifPHYCtrl.phySendStart(pDataList, HIF_PHY_SEND_NOMAL);
    if (status == HIF_ERRCODE_SUCCESS) {
        hifPHYSendCtrl.pSendNode = pDataList;
        hifPHYSendCtrl.sendOffset = 0;
        hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
        hifPHYCtrl.sendState = HIF_TRANS_STATE_BUSY;
    }

    __unlock_irq((unsigned long)key);

    if(hifPHYCtrl.phyType != HIF_PHY_TYPE_UART) {
        hif_phy_io_set();
    }

    return status;
}


__hif_isr_text int hif_PHY_SendFlush(void)
{
    hifPHYCtrl.phySendStart(NULL, HIF_PHY_SEND_FLUSH);
    hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
    hifPHYSendCtrl.pSendNode = NULL;
    hifPHYSendCtrl.sendOffset = 0;
    hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;

    return HIF_ERRCODE_SUCCESS;
}

__hif_isr_text int hif_PHY_RecvStart(HIF_Data_Node_t * pDataList)
{
    int status = HIF_ERRCODE_SUCCESS;

    unsigned long key = __lock_irq();

    if (pDataList != NULL) {
        status = hifPHYCtrl.phyRecvStart(pDataList);
        if (status == HIF_ERRCODE_SUCCESS) {
            hifPHYCtrl.pRecvNode = pDataList;
            hifPHYCtrl.recvState = HIF_TRANS_STATE_BUSY;
            hif_PHY_InitRecvParse();
        }
    } else {
        if (hifPHYCtrl.pRecvNode != NULL) {
            status = hifPHYCtrl.phyRecvStart(hifPHYCtrl.pRecvNode);
            if (status == HIF_ERRCODE_SUCCESS) {
                hifPHYCtrl.recvState = HIF_TRANS_STATE_BUSY;
                hif_PHY_InitRecvParse();
            }
        } else {
            status = HIF_ERRCODE_INVALID_PARAM;
        }
    }

    __unlock_irq((unsigned long)key);

    return status;
}


__hif_sram_text int hif_PHY_TransIsIdle(void)
{
#if (CONFIG_HIF_PHY_UART != 0)
    if (hifPHYCtrl.phyType == HIF_PHY_TYPE_UART) {
        if (hifPHYCtrl.sendState == HIF_TRANS_STATE_IDLE) {
            return 1;
        } else {
            return 0;
        }
    } else
#endif

    {
        if ((hifPHYCtrl.recvState == HIF_TRANS_STATE_IDLE) &&
            (hifPHYCtrl.sendState == HIF_TRANS_STATE_IDLE)) {
            return 1;
        } else {
            return 0;
        }
    }
}

int hif_PHY_RegisterCallback(uint8_t cbType, HIF_PHY_TransCallback_t cb)
{
    if ((cb == NULL)) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if (cbType == HIF_TRANS_SEND_CB_ID) {
        hifPHYCtrl.sendCb = cb;
    } else if (cbType == HIF_TRANS_RECV_CB_ID) {
        hifPHYCtrl.recvCb = cb;
    } else {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    return HIF_ERRCODE_SUCCESS;
}


__hif_sram_text uint8_t hif_PHY_GetTransDummy(void)
{
    return hifPHYCtrl.sendDummy;
}


__hif_isr_text void hif_PHY_SendDataNodeInit(uint8_t *pData, uint16_t len, HIF_Data_Node_t *pDataNode, uint32_t flag)
{
    if (flag & HIF_DATA_NODE_FLAG_NOT_DLL_INIT) {
        pDataNode->len      = len;
        pDataNode->data     = pData;
        pDataNode->next     = NULL;
        pDataNode->rsv2     = (uint16_t)flag & HIF_DATA_NODE_FLAG_MSK;
    } else {
#if (CONFIG_HIF_PHY_DMA == 1)
        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
            hif_PHY_DMA_SendDataNodeInit(pData, len, pDataNode, flag);
        } else
#endif
        {
#if (CONFIG_HIF_DATA_NODE_EXT == 1)
            pDataNode->rsv0     = 0;
            pDataNode->rsv1     = flag;
            pDataNode->len      = len & 0xFFF;
            pDataNode->data     = pData;
            pDataNode->next     = NULL;
            pDataNode->rsv2     = (uint16_t)flag & HIF_DATA_NODE_FLAG_MSK;
#else
            pDataNode->len      = len & 0xFFF;
            pDataNode->data     = pData;
            pDataNode->next     = NULL;
#endif
        }
    }
}


__hif_sram_text uint8_t * hif_PHY_SendDataNodeDeinit(HIF_Data_Node_t *pDataNode)
{
    uint8_t *pData = NULL;
    if (pDataNode->rsv2 & HIF_DATA_NODE_FLAG_NOT_DLL_INIT) {
        pData = pDataNode->data;
    } else {
#if (CONFIG_HIF_PHY_DMA == 1)
        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
            pData = hif_PHY_DMA_SendDataNodeDeinit(pDataNode);
        } else
#endif
        {
            pData = pDataNode->data;
        }
    }

    pDataNode->rsv2 &= ~((uint16_t)HIF_DATA_NODE_FLAG_MSK);

    hif_MEM_DataNodeFree(pDataNode);

    return pData;
}

/**
 * @brief Refresh a data node
 *        1. Clear send-complete flag(set by phyical-layer) in the pDataNode
 *        2. 
 * @param pDataNode
 */
void hif_PHY_SendDataNodeRefresh(HIF_Data_Node_t *pDataNode)
{
    if (pDataNode == NULL) {
        return;
    }

    /** 1. */
    pDataNode->len &= 0xFFFu;

    /** 2. */
}


__hif_isr_text void hif_PHY_RecvCallback(uint32_t rcvStatus, uint32_t rcvSize)
{
    uint8_t parseEna = 0;

    if (hifPHYCtrl.pRecvNode == NULL) {
        return;
    }
    uint8_t *pBuff = hifPHYCtrl.pRecvNode->data;

    /* 1>. receive data
     */
    switch (rcvStatus) {
    case HIF_TRANS_STATUS_NULL:
        /* DMA + SPI receive error
         * IIC receive error
         */
        hifPHYCtrl.phyRecvStop();
        hifPHYCtrl.phyRecvStart(hifPHYCtrl.pRecvNode);
        hifPHYCtrl.recvState = HIF_TRANS_STATE_BUSY;
        break;


    case HIF_TRANS_STATUS_PART:
        /* UART interrupt receive
         * IIC  interrupt receive
         */
#if (CONFIG_HIF_PHY_RECV_INT == 1)
        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT) {
            int recvStatus = 0;
            recvStatus = hifPHYCtrl.phyRecv(&pBuff[hifPHYRecvCtrl.recvSize], 0xFFFF);
            if (recvStatus >= 0) {
                hifPHYRecvCtrl.recvSize += recvStatus;
            } else {
                hifPHYCtrl.phyRecvStop();
                hifPHYCtrl.phyRecvStart(hifPHYCtrl.pRecvNode);
                hifPHYCtrl.recvState = HIF_TRANS_STATE_BUSY;
            }
        }
#endif
        break;


    case HIF_TRANS_STATUS_DONE:
#if (CONFIG_HIF_PHY_RECV_INT == 1)
        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT) {
            int recvStatus = 0;
            recvStatus = hifPHYCtrl.phyRecv(&pBuff[hifPHYRecvCtrl.recvSize], 0xFFFF);
            if (recvStatus >= 0) {
                hifPHYRecvCtrl.recvSize += recvStatus;
            } else {
                hifPHYCtrl.phyRecvStop();
                hifPHYCtrl.phyRecvStart(hifPHYCtrl.pRecvNode);
                hifPHYCtrl.recvState = HIF_TRANS_STATE_BUSY;
            }
        } else
#endif

#if (CONFIG_HIF_PHY_RECV_DMA == 1)
        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
            //uint32_t recvSize = hif_PHY_DMA_GetRecvSize();
            hif_PHY_InitRecvParse();
            hifPHYRecvCtrl.recvSize = rcvSize;
        } else
#endif
        {

        }
        parseEna = 1;
        break;


    default:
        break;
    }



    /* 2>. parse data
     */
    if (parseEna == 0) {
        return;
    }


    uint32_t parseState = hif_PHY_RecvParse(pBuff);

    if (parseState == HIF_PHY_PARSE_STATE_DONE) {
        hifPHYCtrl.phyRecvStop();
        hifPHYCtrl.recvState = HIF_TRANS_STATE_IDLE;

        /* recv success, report frame */
        if (hifPHYRecvCtrl.recvStatus == HIF_ERRCODE_SUCCESS) {
            hif_phy_io_clr();
            hifPHYCtrl.recvCb(&pBuff[hifPHYRecvCtrl.frameStart],
                hifPHYRecvCtrl.frameSize,
                HIF_PHY_FLAG_RECV_DONE);

        /* wakeup frame, report event */
        } else if (hifPHYRecvCtrl.recvStatus == HIF_ERRCODE_NOT_READY) {
            hif_phy_io_clr();
            hifPHYCtrl.recvCb(NULL,
                0,
                HIF_PHY_FLAG_RECV_WAKE);

        /* recv error, report status */
        } else {
            hifPHYCtrl.recvCb(&hifPHYRecvCtrl.recvStatus,
                1,
                HIF_PHY_FLAG_RECV_FAIL);
        }

    }        else {

        if ((rcvStatus == HIF_TRANS_STATUS_DONE) &&
            (parseState != HIF_PHY_PARSE_STATE_IDLE) &&
            (hifPHYCtrl.phyType != HIF_PHY_TYPE_UART)) {
            hifPHYCtrl.phyRecvStop();
            hifPHYCtrl.recvState = HIF_TRANS_STATE_IDLE;

            hifPHYRecvCtrl.recvStatus = HIF_ERRCODE_IO_ERROR;
            hifPHYCtrl.recvCb(&hifPHYRecvCtrl.recvStatus,
                1,
                HIF_PHY_FLAG_RECV_FAIL);
        }
    }

}


__hif_isr_text void hif_PHY_SendCallback(uint32_t sendStatus)
{
    uint32_t sendFlag = 0;


    if (hifPHYCtrl.phyType == HIF_PHY_TYPE_UART) {
        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) {
            sendFlag = hif_PHY_SendParse(HIF_TRANS_STATUS_NULL);

            if (sendFlag & HIF_PHY_FLAG_SEND_DONE) {
                hifPHYCtrl.phySendStop();
                hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
                hif_phy_io_clr();
            }

            if(sendFlag & (HIF_PHY_FLAG_SEND_DONE | HIF_PHY_FLAG_SEND_PART) ) {
                hifPHYCtrl.sendCb(NULL, 0, sendFlag);
            }

        } else if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
            if (sendStatus == HIF_TRANS_STATUS_DONE || sendStatus == HIF_TRANS_STATUS_PART) {
                if (sendStatus == HIF_TRANS_STATUS_DONE) {
                    sendFlag |= HIF_PHY_FLAG_SEND_DONE;
                    hifPHYCtrl.phySendStop();
                    hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
                    hif_phy_io_clr();
                }
                sendFlag |= HIF_PHY_FLAG_SEND_PART;

                hifPHYCtrl.sendCb(NULL, 0, sendFlag);
            } else if (sendStatus == HIF_TRANS_STATUS_FLUSH) {
                hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
                hifPHYSendCtrl.pSendNode = NULL;
                hifPHYSendCtrl.sendOffset = 0;
                hifPHYCtrl.phySendStop();
                hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
                hif_PHY_RecvStart(NULL);
                hifPHYCtrl.sendCb(NULL, 0, HIF_PHY_FLAG_SEND_FLUSH | HIF_PHY_FLAG_SEND_PART | HIF_PHY_FLAG_SEND_DONE);
            }
        }

    } else if (hifPHYCtrl.phyType == HIF_PHY_TYPE_IIC) {
        if (sendStatus == HIF_TRANS_STATUS_HEAD) {
            hif_PHY_SendParse(HIF_TRANS_STATUS_HEAD);
        } else if (sendStatus == HIF_TRANS_STATUS_PART) {
            sendFlag = hif_PHY_SendParse(HIF_TRANS_STATUS_PART);
            if(sendFlag & (HIF_PHY_FLAG_SEND_DONE | HIF_PHY_FLAG_SEND_PART) ) {
                hifPHYCtrl.sendCb(NULL, 0, HIF_PHY_FLAG_SEND_PART);
            }

        } else if (sendStatus == HIF_TRANS_STATUS_DONE) {
            sendFlag = hif_PHY_SendParse(HIF_TRANS_STATUS_DONE);
            hifPHYCtrl.phySendStop();
            if (sendFlag == HIF_PHY_FLAG_SEND_DONE) {
                hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
                hif_phy_io_clr();
                hifPHYCtrl.sendCb(NULL, 0, HIF_PHY_FLAG_SEND_DONE | HIF_PHY_FLAG_SEND_PART);
            } else if (sendFlag == HIF_PHY_FLAG_SEND_FLUSH) {
            hif_phy_io_clr();
                hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
                hifPHYSendCtrl.pSendNode = NULL;
                hifPHYSendCtrl.sendOffset = 0;
                hifPHYCtrl.phyRecvStop();
                hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
                hif_PHY_RecvStart(NULL);
                hifPHYCtrl.sendCb(NULL, 0, HIF_PHY_FLAG_SEND_FLUSH | HIF_PHY_FLAG_SEND_PART | HIF_PHY_FLAG_SEND_DONE);
            } else {
                hifPHYCtrl.phySendStart(hifPHYSendCtrl.pSendNode, HIF_PHY_SEND_NOMAL);
                hifPHYCtrl.sendCb(NULL, 0, HIF_PHY_FLAG_SEND_PART);
            }
        } else if(sendStatus == HIF_TRANS_STATUS_FLUSH) {
            hif_phy_io_clr();
            hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
            hifPHYSendCtrl.pSendNode = NULL;
            hifPHYSendCtrl.sendOffset = 0;
            hifPHYCtrl.phyRecvStop();
            hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
            hif_PHY_RecvStart(NULL);
            hifPHYCtrl.sendCb(NULL, 0, HIF_PHY_FLAG_SEND_FLUSH | HIF_PHY_FLAG_SEND_PART | HIF_PHY_FLAG_SEND_DONE);
        }

    } else if (hifPHYCtrl.phyType == HIF_PHY_TYPE_SPI) {
        if (sendStatus == HIF_TRANS_STATUS_FLUSH) {
            hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
            hifPHYSendCtrl.pSendNode = NULL;
            hifPHYSendCtrl.sendOffset = 0;
            hifPHYCtrl.phySendStop();
            hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
            hif_PHY_RecvStart(NULL);
            hifPHYCtrl.sendCb(NULL, 0, HIF_PHY_FLAG_SEND_FLUSH | HIF_PHY_FLAG_SEND_PART | HIF_PHY_FLAG_SEND_DONE);
        } else {
            if (hifPHYCtrl.sendType == HIF_PHY_SEND_TYPE_WAKEUP) {
                hifPHYCtrl.sendType = HIF_PHY_SEND_TYPE_OTHER;
                sendStatus = HIF_TRANS_STATUS_DONE;
            }

            if (sendStatus == HIF_TRANS_STATUS_DONE) {
                sendFlag |= HIF_PHY_FLAG_SEND_DONE;
                hifPHYCtrl.phySendStop();
                hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
                hif_phy_io_clr();
            }
            sendFlag |= HIF_PHY_FLAG_SEND_PART;

            hifPHYCtrl.sendCb(NULL, 0, sendFlag);
        }
    }
}



__hif_isr_text void hif_PHY_SendWakeupResp(HIF_Data_Node_t * pDataNode)
{
    hifPHYCtrl.sendType = HIF_PHY_SEND_TYPE_WAKEUP;
    hifPHYCtrl.sendState = HIF_TRANS_STATE_BUSY;

    hifPHYCtrl.phySendStop();
    hifPHYCtrl.phySendStart(pDataNode, HIF_PHY_SEND_SPEC);

    if (hifPHYCtrl.phyType == HIF_PHY_TYPE_IIC) {
        hifPHYSendCtrl.pSendNode = pDataNode;
        hifPHYSendCtrl.sendOffset = 0;
        hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
        hifPHYCtrl.sendState = HIF_TRANS_STATE_BUSY;
    } else {
        hifPHYCtrl.phySend(pDataNode->data, pDataNode->len);
    }
}

#if ((CONFIG_HIF_PHY_RECV_DMA == 1) || (CONFIG_HIF_PHY_RECV_INT == 1))
__hif_isr_text static uint32_t hif_PHY_RecvParse(uint8_t *pBuff)
{
    int readStatus = HIF_ERRCODE_SUCCESS;

    do {
        switch (hifPHYRecvCtrl.recvState)
        {
        case HIF_PHY_PARSE_STATE_IDLE:
            hif_PHY_ReadRecvData(1, readStatus);
            if (readStatus < 0) {
                break;
            }
            hifPHYRecvCtrl.frameOffset += 1;

            if (hifPHYRecvCtrl.frameOffset <= 2) {
                if (pBuff[hifPHYRecvCtrl.frameStart + 0] != HIF_PHY_MSG_MAGIC) {
                    if (hifPHYRecvCtrl.frameOffset == 1) {
                         if ((pBuff[hifPHYRecvCtrl.frameStart + 0] != HIF_PHY_WKP_MAGIC)
                            && (pBuff[hifPHYRecvCtrl.frameStart + 0] != HIF_PHY_SYNC_MAGIC)) {
                             hifPHYRecvCtrl.frameOffset = 0;
                             hifPHYRecvCtrl.frameStart = hifPHYRecvCtrl.recvOffset;
                         }
                     } else if (hifPHYRecvCtrl.frameOffset == 2) {
                         if (pBuff[hifPHYRecvCtrl.frameStart + 1] == 0xFF) {
                             /* wakeup process */
                             hifPHYRecvCtrl.recvStatus = HIF_ERRCODE_NOT_READY;
                             hifPHYRecvCtrl.recvState = HIF_PHY_PARSE_STATE_DONE;
                         }
                         hifPHYRecvCtrl.frameOffset = 0;
                         hifPHYRecvCtrl.frameStart = hifPHYRecvCtrl.recvOffset;
                     }

                } else {
                    /* do nothing */
                }

            } else if (hifPHYRecvCtrl.frameOffset == HIF_HEAD_LEN) {
                uint8_t check8 = ~(HIF_CheckSum8(0, &pBuff[hifPHYRecvCtrl.frameStart], HIF_HEAD_LEN));
                if (check8 == 0) {
                    HIF_MsgHdr_t *msg = (HIF_MsgHdr_t *)&pBuff[hifPHYRecvCtrl.frameStart + HIF_PHY_HEAD_LEN];
                    if (msg->type != HIF_MSG_TYPE_TO_DEVICE ||
                        (msg->flag & (HIF_MSG_FLAG_MAC32_BIT|HIF_MSG_FLAG_EXTEND_BIT))) {
                        hifPHYRecvCtrl.frameSize = hifPHYRecvCtrl.frameOffset;
                        hifPHYRecvCtrl.recvStatus = HIF_CMD_STATUS_VERSION;
                        hifPHYRecvCtrl.recvState = HIF_PHY_PARSE_STATE_DONE;
                    } else if (msg->length > CONFIG_HIF_CMD_PAYLOAD_MAX_SIZE) {
                        hifPHYRecvCtrl.frameSize = hifPHYRecvCtrl.frameOffset;
                        hifPHYRecvCtrl.recvStatus = HIF_CMD_STATUS_TOOLONG;
                        hifPHYRecvCtrl.recvState = HIF_PHY_PARSE_STATE_DONE;
                    } else {
                        hifPHYRecvCtrl.frameSize = hifPHYRecvCtrl.frameOffset + msg->length;
                        if ((msg->flag & HIF_MSG_FLAG_CHECK_BIT)) {
                            hifPHYRecvCtrl.frameSize += HIF_CHKEC_LEN;
                        }

                        if (hifPHYRecvCtrl.frameOffset < hifPHYRecvCtrl.frameSize) {
                            hifPHYRecvCtrl.recvState = HIF_PHY_PARSE_STATE_BUSY;
                        } else {
                            hifPHYRecvCtrl.recvState = HIF_PHY_PARSE_STATE_DONE;
                        }
                    }
                } else { /* head check error */
                    hifPHYRecvCtrl.frameOffset = 0;
                    hifPHYRecvCtrl.frameStart = hifPHYRecvCtrl.recvOffset;
                }
            }
            break;


        case HIF_PHY_PARSE_STATE_BUSY:
            if (hifPHYRecvCtrl.frameOffset >= hifPHYRecvCtrl.frameSize) {
                HIF_MsgHdr_t *msg = (HIF_MsgHdr_t *)&pBuff[hifPHYRecvCtrl.frameStart + HIF_PHY_HEAD_LEN];
                if ((msg->flag & HIF_MSG_FLAG_CHECK_BIT)) {
                    uint32_t check_calc = ~(HIF_CheckSum32(0, (uint32_t *)&pBuff[hifPHYRecvCtrl.frameStart + HIF_PHY_HEAD_LEN], hifPHYRecvCtrl.frameSize - HIF_PHY_HEAD_LEN - HIF_CHKEC_LEN));
                    uint32_t check_sum;
                    memcpy(&check_sum, &pBuff[hifPHYRecvCtrl.frameStart + hifPHYRecvCtrl.frameSize - HIF_CHKEC_LEN], sizeof(check_sum));
                    if (check_calc == check_sum) {
                        hifPHYRecvCtrl.recvStatus = HIF_CMD_STATUS_SUCCESS;
                    } else {
                        hifPHYRecvCtrl.recvStatus = HIF_CMD_STATUS_CHECK;
                    }
                }
                hifPHYRecvCtrl.recvState = HIF_PHY_PARSE_STATE_DONE;
            } else {
                hif_PHY_ReadRecvData((hifPHYRecvCtrl.frameSize - hifPHYRecvCtrl.frameOffset), readStatus);
                if (readStatus < 0) {
                    break;
                }

                hifPHYRecvCtrl.frameOffset += readStatus;
            }

            break;

        case HIF_PHY_PARSE_STATE_DONE:
            readStatus = HIF_ERRCODE_ABORTED;
            break;
        default:
            readStatus = HIF_ERRCODE_ABORTED;
            break;
        }

    } while (readStatus >= 0);


    return hifPHYRecvCtrl.recvState;
}
#endif


#if ((CONFIG_HIF_PHY_SEND_DMA == 1) || (CONFIG_HIF_PHY_SEND_INT == 1))


__hif_isr_text static uint32_t hif_PHY_SendParse(uint32_t ctrl)
{
    uint32_t flag = 0;
    int writeStatus = HIF_ERRCODE_SUCCESS;
    int loopBreak = 0;

    do {
        switch (hifPHYSendCtrl.sendState) {
        case HIF_PHY_PARSE_STATE_IDLE:
            if (hifPHYSendCtrl.pSendNode == NULL) {
                hifPHYSendCtrl.sendStatus = HIF_ERRCODE_SUCCESS;
                hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_DONE;
            } else {
                if ((hifPHYSendCtrl.pSendNode->data == NULL) ||
                    (hifPHYSendCtrl.pSendNode->len == 0)) {
                    hifPHYSendCtrl.sendStatus = HIF_ERRCODE_NO_BUFFER;
                    hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_DONE;
                } else {
                    hifPHYSendCtrl.sendOffset = 0;
                    hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_BUSY;
                }
            }
            break;


        case HIF_PHY_PARSE_STATE_BUSY:
            if (ctrl == HIF_TRANS_STATUS_DONE) {
                flag = HIF_PHY_FLAG_SEND_FLUSH;
                hifPHYSendCtrl.flag = 0;
                hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
                loopBreak = 1;
                break;
            }

            if (hifPHYSendCtrl.pSendNode->len > hifPHYSendCtrl.sendOffset) {
                uint16_t sendLen = hifPHYSendCtrl.pSendNode->len - hifPHYSendCtrl.sendOffset;

                writeStatus = hifPHYCtrl.phySend(&hifPHYSendCtrl.pSendNode->data[hifPHYSendCtrl.sendOffset], sendLen);
                if (writeStatus < 0) {
                    hifPHYSendCtrl.sendStatus = HIF_ERRCODE_IO_ERROR;
                    hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_DONE;
                    loopBreak = 1;
                } else {
                    hifPHYSendCtrl.sendOffset += writeStatus;
                    if (writeStatus < sendLen) {
                        loopBreak = 1;
                    }
                }
            } else {
                flag |= HIF_PHY_FLAG_SEND_PART;
                hifPHYSendCtrl.pSendNode->len |= HIF_PHY_TRANS_DONE_MSK;
                if ((ctrl == HIF_TRANS_STATUS_HEAD) || (ctrl == HIF_TRANS_STATUS_PART)) {
                    if (hifPHYSendCtrl.pSendNode->rsv2 & (HIF_DATA_NODE_FLAG_HEAD | HIF_DATA_NODE_FLAG_LAST)) {
                        hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_FILL;
                        if (hifPHYSendCtrl.pSendNode->rsv2 & HIF_DATA_NODE_FLAG_LAST) {
                            if (hifPHYSendCtrl.pSendNode->next == NULL) {
                                hifPHYSendCtrl.flag = HIF_PHY_FLAG_SEND_DONE;
                            } else {
                                hifPHYSendCtrl.flag = HIF_PHY_FLAG_SEND_PART;
                            }
                        }
                        hifPHYSendCtrl.pSendNode = hifPHYSendCtrl.pSendNode->next;

                    } else {
                        hifPHYSendCtrl.pSendNode = hifPHYSendCtrl.pSendNode->next;
                        hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
                    }
                } else {
                    hifPHYSendCtrl.pSendNode = hifPHYSendCtrl.pSendNode->next;
                    hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
                }
            }
            break;


        case HIF_PHY_PARSE_STATE_FILL:
            if (ctrl == HIF_TRANS_STATUS_DONE) {
                flag |= hifPHYSendCtrl.flag;
                hifPHYSendCtrl.flag = 0;
                hifPHYSendCtrl.sendState = HIF_PHY_PARSE_STATE_IDLE;
                loopBreak = 1;
            }else if ((ctrl == HIF_TRANS_STATUS_HEAD) || (ctrl == HIF_TRANS_STATUS_PART)) {
                for (uint32_t idx = 0; idx < 6; idx++) {
                    uint8_t fillData = 0xFF;
                    writeStatus = hifPHYCtrl.phySend(&fillData, 1);
                    if (writeStatus <= 0) {
                        break;
                    }
                }
                loopBreak = 1;
            }
            break;

        case HIF_PHY_PARSE_STATE_PART:

            break;

        case HIF_PHY_PARSE_STATE_DONE:
            flag |= HIF_PHY_FLAG_SEND_DONE;
            loopBreak = 1;
            break;

        default:

        }
    } while (loopBreak == 0);

    return flag;
}

#endif

#endif /* (CONFIG_HIF == 1) */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
