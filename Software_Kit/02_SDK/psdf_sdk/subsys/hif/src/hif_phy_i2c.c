/**
 ******************************************************************************
 * @file    hif_phy_i2c.c
 * @brief   hif phy i2c define.
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
#include "hif_log.h"
#include "hif_dll.h"
#include "hif.h"
#include "hal_board.h"

#if ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_IIC != 0))

#include "hal_i2c.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */

/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private macros.
 * ----------------------------------------------------------------------------
 */
#define HIF_I2C_SEND_DATA_MIN               6
#define HIF_I2C_SEND_DATA_MAX               8

#define HIF_I2_FIFO_SIZE                    8
/* Private variables.
 * ----------------------------------------------------------------------------
 */

static uint8_t sendData[HIF_I2C_SEND_DATA_MAX];
static uint16_t sendLen;
static uint16_t skipLen;
static uint16_t skipOffset;
static uint8_t sendSkipEna;

static uint8_t recvFrame;
static uint8_t sendFrame;

static uint8_t hifI2cAddr;
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static void hif_PHY_IIC_IrqCallback(HAL_Dev_t * pDevice, void *arg);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int hif_PHY_IIC_Init(HIF_PHY_InitCfg_t *pInitCfg)
{
    HIF_LOG_DBG("hif_PHY_IIC_Init");

    if (pInitCfg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    I2C_InitParam_t iicCfg = {
        .mode      =  I2C_MODE_SLAVE,
        .speed     =  I2C_SPEED_FAST,
        .busErrCb  =  {
            .arg   =  NULL,
            .cb    =  NULL,
        },
    };

    hifPHYCtrl.hifDev = HAL_I2C_Init(hifPHYCtrl.devId, &iicCfg);
    if (hifPHYCtrl.hifDev == NULL) {
        HIF_LOG_ERR("HAL_IIC_Init Fail");
        return HIF_ERRCODE_IO_ERROR;
    }

    hifI2cAddr = (uint8_t)pInitCfg->transCfg;
#if ((CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
    (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
    if ((hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) ||
        (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA)) {
        return HIF_ERRCODE_INVALID_PARAM;
    }
#endif

    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_IIC_Deinit(void)
{
    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_IIC_Open(void)
{
    if (HAL_I2C_Open(hifPHYCtrl.hifDev, I2C_ADDR_WIDTH_7BIT, hifI2cAddr)) {
        HIF_LOG_ERR("HAL_IIC_Open Fail");
        return HIF_ERRCODE_IO_ERROR;
    }

    /* 1> config spi send and recv */
    int status = HIF_ERRCODE_SUCCESS;

#if ((CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
     (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
    if ((hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) ||
        (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA)) {
        return HIF_ERRCODE_INVALID_PARAM;
    }
#endif

#if ((CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
     (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT))
    if ((hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) ||
        (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT)) {
        HAL_Callback_t iicIrqCb = {
            .cb     = hif_PHY_IIC_IrqCallback,
            .arg    = NULL
        };

        HAL_I2C_RegisterIRQ(hifPHYCtrl.hifDev, &iicIrqCb);

        uint8_t level = 0;
        level = 3;
        HAL_I2C_ExtendControl(hifPHYCtrl.hifDev, I2C_EXTATTR_SET_TXFIFO_LEVEL, &level);
        level = 3;
        HAL_I2C_ExtendControl(hifPHYCtrl.hifDev, I2C_EXTATTR_SET_RXFIFO_LEVEL, &level);

        HAL_I2C_DisableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_ALL);

        HAL_I2C_Enable(hifPHYCtrl.hifDev);

        sendLen = 0;
        skipLen = 0;
        skipOffset = 0;
        sendSkipEna = 0;

    }
#endif

    return status;
}


__hif_sram_text int hif_PHY_IIC_Close(void)
{
    if (HAL_I2C_Close(hifPHYCtrl.hifDev) == HAL_STATUS_OK) {
        if(HAL_I2C_DeInit(hifPHYCtrl.hifDev) == HAL_STATUS_OK) {
            HAL_I2C_DisableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_ALL);
            return HIF_ERRCODE_SUCCESS;
        }
    }

    return HIF_ERRCODE_NOT_READY;
}


__hif_isr_text int hif_PHY_IIC_SendStart(HIF_Data_Node_t * pDataNode, uint32_t param)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {

    } else
#endif

#if (CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) {
        if (param == HIF_PHY_SEND_FLUSH) {
            hif_PHY_SendCallback(HIF_TRANS_STATUS_FLUSH);
        } else {


            sendLen = 0;
            HIF_Data_Node_t * pScanDataNode = pDataNode;
            uint16_t dataOffset = 0;
            while (pScanDataNode != NULL) {
                if ((pScanDataNode->data == NULL) || (pScanDataNode->len == 0)) {
                    break;
                }

                uint16_t reqSize = HIF_I2C_SEND_DATA_MAX - dataOffset;
                if (reqSize == 0) {
                    break;
                }

                if (reqSize > pScanDataNode->len) {
                    reqSize = pScanDataNode->len;
                }

                OSI_Memcpy(&sendData[dataOffset], pScanDataNode->data, reqSize);

                dataOffset += reqSize;

                if (pScanDataNode->rsv2 & (HIF_DATA_NODE_FLAG_HEAD | HIF_DATA_NODE_FLAG_LAST)) {
                    break;
                }

                pScanDataNode = pScanDataNode->next;
            }

            sendLen = dataOffset;

            HAL_I2C_RecoverBus(hifPHYCtrl.hifDev);
            HAL_I2C_EnableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_START |
                                                 I2C_IRQSTATUS_STOP |
                                                 I2C_IRQSTATUS_READ_REQ |
                                                 I2C_IRQSTATUS_RX_FIFO_FULL |
                                                 I2C_IRQSTATUS_SCL_LOW_TIMEOUT);

        }
    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }


    return status;
}


__hif_isr_text static inline int hif_iic_send(uint8_t *pData, uint16_t len)
{
    int sendIdx = 0;
    for (sendIdx = 0; sendIdx < len; sendIdx++) {
        if (HAL_I2C_IsTxReady(hifPHYCtrl.hifDev) == 0) {
            break;
        }

        HAL_I2C_PutTxData(hifPHYCtrl.hifDev, pData[sendIdx], I2C_CMD_WRITE);
    }

    return sendIdx;
}


__hif_isr_text int hif_PHY_IIC_Send(void *pData, uint16_t len)
{
    if (pData == NULL) {
         return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t *pDataBuff = (uint8_t *)pData;

    uint16_t skipCnt = 0;
    if (sendSkipEna) {
        skipCnt = skipLen - skipOffset;
        if (skipCnt > len) {
            skipCnt = len;
        }

        skipOffset += skipCnt;
        if (skipOffset >= skipLen) {
            sendSkipEna = 0;
        }
        len -= skipCnt;
        pDataBuff += skipCnt;
    }

    int status = hif_iic_send(pDataBuff, len);
    if (status >= 0) {
        status += skipCnt;
    }

    return status;
}


__hif_isr_text int hif_PHY_IIC_SendStop(void)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {

    } else
#endif

#if (CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) {
        HAL_I2C_DisableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_START |
                                             I2C_IRQSTATUS_READ_REQ |
                                             I2C_IRQSTATUS_STOP |
                                             I2C_IRQSTATUS_RX_FIFO_FULL |
                                             I2C_IRQSTATUS_TX_FIFO_EMPTY |
                                             I2C_IRQSTATUS_SCL_LOW_TIMEOUT);

    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }


    return status;
}

__hif_sram_text int hif_PHY_IIC_SendIsDone(void)
{

    return HIF_ERRCODE_SUCCESS;

}


__hif_isr_text int hif_PHY_IIC_RecvStart(HIF_Data_Node_t * pDataList)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {

    } else
#endif

#if (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT) {
        HAL_I2C_RecoverBus(hifPHYCtrl.hifDev);
        HAL_I2C_EnableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_STOP |
                                             I2C_IRQSTATUS_READ_REQ |
                                             I2C_IRQSTATUS_RX_FIFO_FULL |
                                             I2C_IRQSTATUS_SCL_LOW_TIMEOUT);
    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }

    return status;
}

__hif_isr_text int hif_PHY_IIC_Recv(void *pData, uint16_t len)
{
    if (pData == NULL) {
         return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t *pDataBuff = (uint8_t *)pData;
    int recvIdx = 0;
    for (recvIdx = 0; recvIdx < len; recvIdx++) {
        if (HAL_I2C_IsRxReady(hifPHYCtrl.hifDev) == 0) {
            break;
        }
        pDataBuff[recvIdx] = HAL_I2C_GetRxData(hifPHYCtrl.hifDev);
    }

    return recvIdx;
}


__hif_isr_text int hif_PHY_IIC_RecvStop(void)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {

    } else
#endif

#if (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT) {
        HAL_I2C_DisableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_START |
                                             I2C_IRQSTATUS_READ_REQ |
                                             I2C_IRQSTATUS_STOP |
                                             I2C_IRQSTATUS_RX_FIFO_FULL |
                                             I2C_IRQSTATUS_TX_FIFO_EMPTY |
                                             I2C_IRQSTATUS_SCL_LOW_TIMEOUT);

    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }


    return status;
}


#if ((CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
    (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT))

__hif_isr_text static void hif_PHY_IIC_FillData(void)
{
    for (uint8_t idx = 0; idx < HIF_I2_FIFO_SIZE; idx++) {
        if (HAL_I2C_IsTxReady(hifPHYCtrl.hifDev) == 0) {
            break;
        }
        HAL_I2C_PutTxData(hifPHYCtrl.hifDev, 0xFF, I2C_CMD_WRITE);
    }
}


__hif_isr_text static void hif_PHY_IIC_DiscardData(void)
{
    for (uint8_t idx = 0; idx < HIF_I2_FIFO_SIZE; idx++) {
        if (HAL_I2C_IsRxReady(hifPHYCtrl.hifDev) == 0) {
            break;
        }
        HAL_I2C_GetRxData(hifPHYCtrl.hifDev);
    }
}


__hif_isr_text static void hif_PHY_IIC_IrqCallback(HAL_Dev_t * pDevice, void *arg)
{
    uint32_t iicStatus = HAL_I2C_GetIRQStatus(pDevice);
    uint32_t iicStatusUpdate = 0;
    HAL_I2C_ClearIRQ(pDevice, iicStatus);

    /* SCL: Pull the SCL pin low for more than 5ms
     * 1. reset I2C
     * 2. restart send or recvive
     */
    if (iicStatus & I2C_IRQSTATUS_SCL_LOW_TIMEOUT) {
        HAL_I2C_RecoverBus(hifPHYCtrl.hifDev);

        if(hifPHYCtrl.sendState == HIF_TRANS_STATE_BUSY) {
            hif_PHY_SendCallback(HIF_TRANS_STATUS_FLUSH);

        } else if (hifPHYCtrl.recvState == HIF_TRANS_STATE_BUSY) {
            hif_PHY_RecvStart(NULL);

        }

        sendFrame = 0;
        recvFrame = 0;

        HAL_I2C_DisableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_SCL_LOW_TIMEOUT);

        return;
    }


    /*
     * I2C Read (Masser <-  slave): Enable start interrupt (RX_FIFO_FULL, START, STOP, STUCK)
     * I2C Write (Master -> Slave): Disable start interrupt (RX_FIFO_FULL, STOP, STUCK)
     */
    if (iicStatus & I2C_IRQSTATUS_START) {
        /* wait status: READ_REQ or RX_FIFO_FULL or timeout
         * timeout: 2000 > 1ms
         *     cpu freq: 256MHz, once loop 0.5us
         */
        uint32_t iicStatusScan = 0;
        uint32_t timeout = 2000;
        iicStatusScan = iicStatus;
        while ((iicStatusScan & (I2C_IRQSTATUS_READ_REQ | I2C_IRQSTATUS_RX_FIFO_FULL)) == 0) {
            iicStatusScan = HAL_I2C_GetStatus(pDevice);
            if((timeout--) == 0) {
                break;
            }
        }

        /* 1>. Timeout
         * 1. address not match
         * 2. bus stuck (return, and wait I2C_IRQSTATUS_SCL_LOW_TIMEOUT irq)
         */
        if ((iicStatusScan & (I2C_IRQSTATUS_READ_REQ | I2C_IRQSTATUS_RX_FIFO_FULL)) == 0) {
            sendFrame = 0;
            recvFrame = 0;
            return;
        }

        /* Expect I2C Read
         */
        if (hifPHYCtrl.sendState == HIF_TRANS_STATE_BUSY) {
            /* 2>. I2C Read
             * Write data quickly
             * Enable Tx FIFO empty
             */
            if ((iicStatusScan & I2C_IRQSTATUS_READ_REQ)) {
                skipLen = 0;
                for (uint8_t idx = 0; idx < sendLen; idx++) {
                    HAL_I2C_PutTxData(hifPHYCtrl.hifDev, sendData[idx], I2C_CMD_WRITE);
                    skipLen++;
                    if (HAL_I2C_IsTxReady(hifPHYCtrl.hifDev) == 0) {
                        break;
                    }
                }
                skipOffset = 0;
                sendSkipEna = 1;

                sendFrame = 1;
                recvFrame = 0;

                hif_PHY_SendCallback(HIF_TRANS_STATUS_HEAD);
                HAL_I2C_EnableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_TX_FIFO_EMPTY);


            } else if ((iicStatusScan & I2C_IRQSTATUS_RX_FIFO_FULL)) {
            /* 3>. I2C Write
             * Read and discad data
             * Rx FIFO full interrupt default enable
             */             
                hif_PHY_IIC_DiscardData();

                sendFrame = 0;
                recvFrame = 0;
            }

        }
        else if (hifPHYCtrl.recvState == HIF_TRANS_STATE_BUSY) {
        /* Expect I2C Write
         */

            /* 2>. I2C Write
             * recvstate not match I2C RecvStart
             * Read and discad data
             * Rx FIFO full interrupt default enable
             */
            if ((iicStatusScan & I2C_IRQSTATUS_RX_FIFO_FULL)) {
                hif_PHY_IIC_DiscardData();

            } else if ((iicStatusScan & I2C_IRQSTATUS_READ_REQ)) {
            /* 2>. I2C Read
             * Write fill data
             * Enable Tx FIFO empty
             */
                hif_PHY_IIC_FillData();

                HAL_I2C_EnableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_TX_FIFO_EMPTY);
            }

            sendFrame = 0;
            recvFrame = 0;

        }
    }


    iicStatusUpdate = HAL_I2C_GetIRQStatus(pDevice);
    iicStatus |= iicStatusUpdate;
    HAL_I2C_ClearIRQ(pDevice, iicStatusUpdate);
    if (iicStatus & I2C_IRQSTATUS_READ_REQ) {
        if (hifPHYCtrl.recvState == HIF_TRANS_STATE_BUSY) {
            /* 2>. I2C Read
             * Write fill data
             */
            hif_PHY_IIC_FillData();

        } else if (hifPHYCtrl.sendState == HIF_TRANS_STATE_BUSY) {
            hif_PHY_RecvCallback(HIF_TRANS_STATUS_PART, 0);
            recvFrame = 0;
        }
    } else if (iicStatus & I2C_IRQSTATUS_RX_FIFO_FULL) {
        if (hifPHYCtrl.recvState == HIF_TRANS_STATE_BUSY) {
            hif_PHY_RecvCallback(HIF_TRANS_STATUS_PART, 0);
            recvFrame = 1;

        } else if (hifPHYCtrl.sendState == HIF_TRANS_STATE_BUSY) {
            /* I2C Write
             * Read and discad data
             */
            hif_PHY_IIC_DiscardData();

            sendFrame = 0;
            recvFrame = 0;

        }

    } else if (iicStatus & I2C_IRQSTATUS_TX_FIFO_EMPTY) {
        if (hifPHYCtrl.sendState == HIF_TRANS_STATE_BUSY) {
            hif_PHY_SendCallback(HIF_TRANS_STATUS_PART);
            recvFrame = 0;

        } else if (hifPHYCtrl.recvState == HIF_TRANS_STATE_BUSY) {
            /* 2>. I2C Read
             * Write fill data
             */
            hif_PHY_IIC_FillData();

            sendFrame = 0;
            recvFrame = 0;
        }
    }


    iicStatusUpdate = HAL_I2C_GetIRQStatus(pDevice);
    iicStatus |= iicStatusUpdate;
    HAL_I2C_ClearIRQ(pDevice, iicStatusUpdate);
    if (iicStatus & I2C_IRQSTATUS_STOP) {
        if (hifPHYCtrl.sendState == HIF_TRANS_STATE_BUSY) {
            if (sendFrame) {
                hif_PHY_SendCallback(HIF_TRANS_STATUS_DONE);
            }

        } else if (hifPHYCtrl.recvState == HIF_TRANS_STATE_BUSY) {
            if (recvFrame) {
                hif_PHY_RecvCallback(HIF_TRANS_STATUS_DONE, 0);
            }

        }

        HAL_I2C_DisableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_TX_FIFO_EMPTY);
        HAL_I2C_EnableIRQ(hifPHYCtrl.hifDev, I2C_IRQSTATUS_SCL_LOW_TIMEOUT);

        sendFrame = 0;
        recvFrame = 0;
    }

}

#endif



#if (CONFIG_HIF_PM == 1)

int hif_PHY_IIC_PmInit(HIF_PHY_PMCallback_t pmCb, uint8_t pCfg)
{
    if (pmCb == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if (hifPHYCtrl.hifDev == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }


    uint8_t enable = 1;
    HAL_Callback_t wakeCb = {
        .cb = (HAL_CbFunc_t)pmCb,
        .arg = NULL,
    };

    uint32_t thresh = pCfg;

    HAL_Status_t status = HAL_STATUS_OK;
    do {
        status = HAL_I2C_ExtendControl(hifPHYCtrl.hifDev, I2C_EXTATTR_WAKEUP_THRESHOLD, &thresh);
        if (status != HAL_STATUS_OK) {
            break;;
        }

        status = HAL_I2C_ExtendControl(hifPHYCtrl.hifDev, I2C_EXTATTR_WAKEUP_CALLBACK, &wakeCb);
        if (status != HAL_STATUS_OK) {
            break;;
        }

        status = HAL_I2C_ExtendControl(hifPHYCtrl.hifDev, I2C_EXTATTR_WAKEUP_CTRL_ABLE, &enable);
    } while (0);

    return status;
}

int hif_PHY_IIC_PmDeinit(void)
{
    if (hifPHYCtrl.hifDev == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t enable = 0;

    HAL_Status_t status = HAL_STATUS_OK;
    do {
        status = HAL_I2C_ExtendControl(hifPHYCtrl.hifDev, I2C_EXTATTR_WAKEUP_CALLBACK, NULL);
        if (status != HAL_STATUS_OK) {
            break;;
        }
        status = HAL_I2C_ExtendControl(hifPHYCtrl.hifDev, I2C_EXTATTR_WAKEUP_CTRL_ABLE, &enable);
    } while (0);

    return status;
}

#endif


#endif /* ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_IIC == 1)) */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
