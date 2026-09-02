/**
 ******************************************************************************
 * @file    hif_phy.h
 * @brief   hif phy layer define.
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

#ifndef _HIF_PHY_H_
#define _HIF_PHY_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "hif_types.h"
#include "hif_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

/* Physical layer type definitions */
#define HIF_PHY_TYPE_DIS            0x00   /*!< Disabled/disconnected type */
#define HIF_PHY_TYPE_UART           0x01    /*!< UART physical layer type */
#define HIF_PHY_TYPE_IIC            0x02    /*!< I2C physical layer type */
#define HIF_PHY_TYPE_SPI            0x03    /*!< SPI physical layer type */
#define HIF_PHY_TYPE_CAN            0x04    /*!< CAN physical layer type */

/* Physical layer transmission mode definitions */
#define HIF_PHY_TRANS_MODE_INT      0x01    /*!< Interrupt-based transmission mode */
#define HIF_PHY_TRANS_MODE_POL      0x02    /*!< Polling-based transmission mode */
#define HIF_PHY_TRANS_MODE_DMA      0x03    /*!< DMA-based transmission mode */


#define HIF_PHY_TRANS_DONE_MSK      0x1000
#define HIF_PHY_TRANS_LEN_MSK       0xFFF


#define HIF_PHY_FLAG_RECV_DONE      0x01
#define HIF_PHY_FLAG_RECV_FAIL      0x02
#define HIF_PHY_FLAG_RECV_WAKE      0x03

#define HIF_PHY_FLAG_SEND_PART      0x10
#define HIF_PHY_FLAG_SEND_DONE      0x20
#define HIF_PHY_FLAG_SEND_FLUSH     0x40


#define HIF_TRANS_SEND_CB_ID        1
#define HIF_TRANS_RECV_CB_ID        2
#define HIF_TRANS_ERR_CB_ID         3


#define HIF_PHY_REFRESH_FLAG_WRITE  1
#define HIF_PHY_REFRESH_FLAG_READ   2

#define HIF_DATA_NODE_FLAG_LAST             0x0100      /*< Indicate a node is the last node of a node list */
#define HIF_DATA_NODE_FLAG_HEAD             0x0200      /*< Indicate a node is the head node of a node list */
#define HIF_DATA_NODE_FLAG_MSG_REPORT       0x1000      /*< Indicate a node is allocated by HIF, not application */
#define HIF_DATA_NODE_FLAG_NOT_DLL_INIT     0x2000      /*< Indicate a node should not be inited by DLL */
#define HIF_DATA_NODE_FLAG_MSK              0xFF00

#define HIF_TRANS_STATE_IDLE        0
#define HIF_TRANS_STATE_BUSY        1


#define HIF_TRANS_STATUS_NULL       0
#define HIF_TRANS_STATUS_HEAD       1
#define HIF_TRANS_STATUS_PART       2
#define HIF_TRANS_STATUS_DONE       3
#define HIF_TRANS_STATUS_FLUSH      4


/* Exported types.
 * ----------------------------------------------------------------------------
 */

typedef struct {
    uint8_t             phyType;
    uint8_t             devId;
    uint8_t             sendMode;
    uint8_t             recvMode;

    uint32_t            transFreq;
    uint32_t            transCfg;
} HIF_PHY_InitCfg_t;


/**
 * @brief Function type for initializing the physical layer
 * @param pInitCfg Pointer to initialization configuration
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_init_t)(HIF_PHY_InitCfg_t *pInitCfg);

/**
 * @brief Function type for deinitializing the physical layer
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_deinit_t)(void);


/**
 * @brief Function type for opening the physical layer connection
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_open_t)(void);

/**
 * @brief Function type for closing the physical layer connection
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_close_t)(void);


/**
 * @brief Function type for starting a send operation
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_send_start_t)(HIF_Data_Node_t * pDataList, uint32_t param);

/**
 * @brief Function type for sending data through physical layer
 * @param pdata Pointer to data to be sent
 * @param size Size of data to be sent
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_send_t)(void *pdata, uint16_t size);

/**
 * @brief Function type for ending a send operation
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_send_stop_t)(void);

typedef int (* phy_send_IsDone_t)(void);

/**
 * @brief Function type for starting a receive operation
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_recv_start_t)(HIF_Data_Node_t * pDataList);

/**
 * @brief Function type for receiving data from physical layer
 * @param pData Pointer to buffer where received data will be stored
 * @param size Size of data to receive
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_recv_t)(void *pData, uint16_t size);

/**
 * @brief Function type for ending a receive operation
 * @return Operation status (0 for success, negative for error)
 */
typedef int (* phy_recv_stop_t)(void);


typedef int (* HIF_PHY_TransCallback_t)(void *pParam, uint16_t size, uint32_t flag);


#if (CONFIG_HIF_PM == 1)
typedef int (* HIF_PHY_PMCallback_t)(void *pDevice, void *arg);

typedef int (* phy_pm_init_t)(HIF_PHY_PMCallback_t pmCb, uint8_t pCfg);

typedef int (* phy_pm_deinit_t)(void);
#endif


typedef struct {
    phy_init_t              phyInit;
    phy_deinit_t            phyDeinit;

    phy_open_t              phyOpen;
    phy_close_t             phyClose;

    phy_send_start_t        phySendStart;
    phy_send_t              phySend;
    phy_send_stop_t         phySendStop;
    phy_send_IsDone_t       phySendIsDone;

    phy_recv_start_t        phyRecvStart;
    phy_recv_t              phyRecv;
    phy_recv_stop_t         phyRecvStop;

#if (CONFIG_HIF_PM == 1)
    phy_pm_init_t           phyPmInit;
    phy_pm_deinit_t         phyPmDeinit;
#endif

    HIF_PHY_TransCallback_t sendCb;
    HIF_PHY_TransCallback_t recvCb;

    HAL_Dev_t               *hifDev;

    uint8_t                 phyType;
    uint8_t                 devId;
    uint8_t                 sendMode;
    uint8_t                 recvMode;

    HIF_Data_Node_t *       pRecvNode;

    uint8_t                 sendState;
    uint8_t                 recvState;

        #define HIF_PHY_SEND_TYPE_OTHER         0
        #define HIF_PHY_SEND_TYPE_WAKEUP        1
    uint8_t                 sendType;

    uint8_t                 sendDummy;
} HIF_PHY_Ctrl_t;

extern HIF_PHY_Ctrl_t hifPHYCtrl;

/* Exported macro.
 * ----------------------------------------------------------------------------
 */
#define HIF_PHY_SEND_NOMAL                  (0)
#define HIF_PHY_SEND_SPEC                   (1)
#define HIF_PHY_SEND_FLUSH                  (2)

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int hif_PHY_Init(HIF_PHY_InitCfg_t *pInitCfg);

int hif_PHY_Deinit(void);

static inline int hif_PHY_Open(void)
{
    return hifPHYCtrl.phyOpen();
}

static inline int hif_PHY_Close(void)
{
    return hifPHYCtrl.phyClose();
}


static inline int hif_PHY_SendStop(void)
{
    unsigned long key = __lock_irq();

    int status = hifPHYCtrl.phySendStop();
    if (status == HIF_ERRCODE_SUCCESS) {
        hifPHYCtrl.sendState = HIF_TRANS_STATE_IDLE;
    }

    __unlock_irq((unsigned long)key);

    return status;
}


static inline int hif_PHY_SendIsDone(void)
{
    if (hifPHYCtrl.phySendIsDone != NULL) {
        return (hifPHYCtrl.sendState == HIF_TRANS_STATE_IDLE && hifPHYCtrl.phySendIsDone());
    } else {
        return 1;
    }
}


static inline int hif_PHY_RecvStop(void)
{
    unsigned long key = __lock_irq();

    int status = hifPHYCtrl.phyRecvStop();
    if (status == HIF_ERRCODE_SUCCESS) {
        hifPHYCtrl.recvState = HIF_TRANS_STATE_IDLE;
    }

    __unlock_irq((unsigned long)key);

    return status;
}

static inline int hif_PHY_GetPhyType(void)
{
    return hifPHYCtrl.phyType;
}


int hif_PHY_TransIsIdle(void);

int hif_PHY_SendStart(HIF_Data_Node_t * pDataList);

int hif_PHY_RecvStart(HIF_Data_Node_t * pDataList);

int hif_PHY_SendFlush(void);

int hif_PHY_RegisterCallback(uint8_t cbType, HIF_PHY_TransCallback_t cb);

uint8_t hif_PHY_GetTransDummy(void);

void hif_PHY_SendWakeupResp(HIF_Data_Node_t * pDataNode);

void hif_PHY_SendDataNodeInit(uint8_t *pData, uint16_t len, HIF_Data_Node_t *pDataNode, uint32_t flag);

uint8_t * hif_PHY_SendDataNodeDeinit(HIF_Data_Node_t *pDataNode);

void hif_PHY_SendDataNodeRefresh(HIF_Data_Node_t *pDataNode);

void hif_PHY_RecvCallback(uint32_t param, uint32_t rcvSize);

void hif_PHY_SendCallback(uint32_t param);


/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "hif_phy_uart.h"
#include "hif_phy_i2c.h"
#include "hif_phy_spi.h"
#include "hif_phy_can.h"
#include "hif_phy_dma.h"


#ifdef __cplusplus
}
#endif

#endif /* _HIF_PHY_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
