/**
 ******************************************************************************
 * @file    hif_phy_spi.c
 * @brief   hif phy spi define.
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

#if ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_SPI != 0))

#include "hal_spi.h"
#include "hal_dma.h"

#include "hif_dll.h"
#include "hif_phy_spi.h"
#include "hif_phy_dma.h"

#include "hif_log.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */

/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private macros.
 * ----------------------------------------------------------------------------
 */

/* Private variables.
 * ----------------------------------------------------------------------------
 */

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

static void hif_PHY_SPI_CsIrqCallback(HAL_Dev_t * pDevice, void *arg);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int hif_PHY_SPI_Init(HIF_PHY_InitCfg_t *pInitCfg)
{
    HIF_LOG_DBG("hif_PHY_SPI_Init");

    if (pInitCfg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    SPI_InitParam_t spiCfg = {
        .mode       = SPI_MODE_SLAVE,
        .sclkMode   = SPI_SCLK_MODE0,
        .ioMode     = pInitCfg->transCfg,
        .csMode     = SPI_CS_HARD,
        .dataWidth  = SPI_DATAWIDTH_8BIT,
        .sclk       = pInitCfg->transFreq,
    };
    hifPHYCtrl.hifDev = HAL_SPI_Init(hifPHYCtrl.devId, &spiCfg, 1);
    if (hifPHYCtrl.hifDev == NULL) {
        return HIF_ERRCODE_IO_ERROR;
    }

#if ((CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
    (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
    if ((hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) ||
        (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA)) {
        HIF_DMA_InitCfg_t dmaCfg = {
            .devId      = 0,
        };

        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
            dmaCfg.sendShake    = DMA_HANDSHAKE_TXFIFO_SPI0;
            dmaCfg.sendCb       = NULL;
            dmaCfg.errCb        = NULL;
            dmaCfg.sendPhyAddr  = DMA_QSPI_TXRX_ADDR;
        } else {
            dmaCfg.sendShake    = 0xFF;
            dmaCfg.sendCb       = NULL;
            dmaCfg.errCb        = NULL;
            dmaCfg.sendPhyAddr  = 0;
        }

        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
            dmaCfg.recvShake    = DMA_HANDSHAKE_RXFIFO_SPI0;
            dmaCfg.recvCb       = NULL;
            dmaCfg.recvPhyAddr  = DMA_QSPI_TXRX_ADDR;
        } else {
            dmaCfg.recvShake    = 0xFF;
            dmaCfg.recvCb       = NULL;
            dmaCfg.recvPhyAddr  = 0;
        }

        int status = hif_PHY_DMA_Init(&dmaCfg);
        if (status != HIF_ERRCODE_SUCCESS) {
            return HIF_ERRCODE_IO_ERROR;
        }
    }
    #endif


    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_SPI_Deinit(void)
{

    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_SPI_Open(void)
{
    if (HAL_SPI_Open(hifPHYCtrl.hifDev, 0)) {
        return HIF_ERRCODE_IO_ERROR;
    }

    /* 1> config spi send and recv */
#if ((CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
    (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
    int status = HIF_ERRCODE_SUCCESS;
    HAL_Callback_t devCb;
    uint16_t fifoLevel;

    status = hif_PHY_DMA_Open();
    if (status != HIF_ERRCODE_SUCCESS) {
        return HIF_ERRCODE_IO_ERROR;
    }

    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
        fifoLevel = 23; // (24-1)

        HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_TX_DMA_LEVEL, &fifoLevel);
        HAL_SPI_EnableDMA(hifPHYCtrl.hifDev, SPI_DIR_TX);

        uint32_t hwVersion = 0;
        HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_GET_HW_VERSION, &hwVersion);

        if (hwVersion == 1) {
            hifPHYCtrl.sendDummy = 1;
        }
    }

    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
        fifoLevel = 15; // (16-1)

        HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_RX_DMA_LEVEL, &fifoLevel);
        HAL_SPI_EnableDMA(hifPHYCtrl.hifDev, SPI_DIR_RX);
    }

    devCb.cb    = hif_PHY_SPI_CsIrqCallback;
    devCb.arg   = NULL;
    HAL_SPI_RegisterIRQ(hifPHYCtrl.hifDev, SPI_IRQ_CS, &devCb);

    HAL_SPI_EnableIRQ(hifPHYCtrl.hifDev, SPI_IRQ_CS);

#endif


    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_SPI_Close(void)
{

    return HIF_ERRCODE_SUCCESS;
}


__hif_isr_text int hif_PHY_SPI_SendStart(HIF_Data_Node_t * pDataNode, uint32_t param)
{
#if (CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
        /* reset SPI FIFO */
        //HAL_SPI_StopTrans(hifPHYCtrl.hifDev);
        if (param == HIF_PHY_SEND_FLUSH) {
            hif_PHY_SendCallback(HIF_TRANS_STATUS_FLUSH);
        } else {
            HAL_SPI_StartTrans(hifPHYCtrl.hifDev, SPI_DIR_TX);
            if (param == HIF_PHY_SEND_NOMAL) {
                hif_PHY_DMA_Send(pDataNode);
                hif_PHY_DMA_SendStart();
            }
        }
    }
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_isr_text int hif_PHY_SPI_Send(void *pData, uint16_t len)
{
    if (pData == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t *pDataBuff = (uint8_t *)pData;
    int sendIdx = 0;
    for (sendIdx = 0; sendIdx < len; sendIdx++) {
        if (HAL_SPI_IsTxReady(hifPHYCtrl.hifDev) == 0) {
            break;
        }

        HAL_SPI_PutTxData(hifPHYCtrl.hifDev, *(uint32_t *)&pDataBuff[sendIdx]);
    }

    return sendIdx;
}


__hif_isr_text int hif_PHY_SPI_SendStop(void)
{
#if (CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
        HAL_SPI_StopTrans(hifPHYCtrl.hifDev);
        hif_PHY_DMA_SendStop();
    }
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_isr_text int hif_PHY_SPI_RecvStart(HIF_Data_Node_t * pDataList)
{
#if (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
        /* reset SPI FIFO */
        HAL_SPI_StopTrans(hifPHYCtrl.hifDev);
        hif_PHY_DMA_RecvStop();
        HAL_SPI_StartTrans(hifPHYCtrl.hifDev, SPI_DIR_RX);
        hif_PHY_DMA_Recv(pDataList);
        hif_PHY_DMA_RecvStart();
    }
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_isr_text int hif_PHY_SPI_Recv(void *pData, uint16_t len)
{

    return HIF_ERRCODE_SUCCESS;
}


__hif_isr_text int hif_PHY_SPI_RecvStop(void)
{
#if (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
        HAL_SPI_StopTrans(hifPHYCtrl.hifDev);
        hif_PHY_DMA_RecvStop();
    }
#endif

    return HIF_ERRCODE_SUCCESS;
}


__hif_isr_text static void hif_PHY_SPI_CsIrqCallback(HAL_Dev_t * pDevice, void *arg)
{
    hif_LOG_TimeRecord();
#if ((CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
    uint32_t transSize = 0;

    if (hifPHYCtrl.sendState == HIF_TRANS_STATE_BUSY) {
        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
            transSize = HAL_SPI_GetFifoLevel(hifPHYCtrl.hifDev, SPI_FIFO_TX);
            if (transSize == 0) {
                hif_PHY_SendCallback(HIF_TRANS_STATUS_DONE);
            } else {
                hif_PHY_SendCallback(HIF_TRANS_STATUS_PART);
            }
        }
    }
    else if (hifPHYCtrl.recvState == HIF_TRANS_STATE_BUSY) {
        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
            transSize = hif_PHY_DMA_GetRecvSize();
            if (transSize == hifPHYCtrl.pRecvNode->len) {
                hif_PHY_RecvCallback(HIF_TRANS_STATUS_NULL, 0);
            } else {
                uint32_t recvFifoSize = HAL_SPI_GetFifoLevel(hifPHYCtrl.hifDev, SPI_FIFO_RX);
                for (uint32_t idx = 0; idx < recvFifoSize; idx++) {
                    hifPHYCtrl.pRecvNode->data[transSize + idx] = (uint8_t)HAL_SPI_GetRxData(hifPHYCtrl.hifDev);
                }
                hif_PHY_RecvCallback(HIF_TRANS_STATUS_DONE, transSize + recvFifoSize);
            }
        }
    }
#endif

}


#if (CONFIG_HIF_PM == 1)

int hif_PHY_SPI_PmInit(HIF_PHY_PMCallback_t pmCb, uint8_t pCfg)
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
        status = HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_WAKEUP_THRESHOLD, &thresh);
        if (status != HAL_STATUS_OK) {
            break;
        }

        status = HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_WAKEUP_CALLBACK, &wakeCb);
        if (status != HAL_STATUS_OK) {
            break;
        }

        status = HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_WAKEUP_CTRL, &enable);
    } while (0);


    return status;
}

int hif_PHY_SPI_PmDeinit(void)
{
    if (hifPHYCtrl.hifDev == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t enable = 0;

    HAL_Status_t status = HAL_STATUS_OK;
    do {
        status = HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_WAKEUP_CALLBACK, NULL);
        if (status != HAL_STATUS_OK) {
            break;
        }
        status = HAL_SPI_ExtControl(hifPHYCtrl.hifDev, SPI_PARAM_WAKEUP_CTRL, &enable);
    } while (0);


    return status;
}

#endif

#endif /* ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_SPI != 0)) */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
