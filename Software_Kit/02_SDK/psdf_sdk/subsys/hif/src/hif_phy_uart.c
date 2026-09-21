/**
 ******************************************************************************
 * @file    hif_phy_uart.c
 * @brief   hif phy uart define.
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

#if ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_UART != 0))

#include "hal_uart.h"
#include "hal_dma.h"

#include "hif_dll.h"
#include "hif_phy_uart.h"
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
#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
static int hif_PHY_UART_DmaSendIrqCallback(void *pParam, uint16_t size, uint32_t flag);
static int hif_PHY_UART_DmaSendErrorIrqCallback(void *pParam, uint16_t size, uint32_t flag);
#endif

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
static int hif_PHY_UART_DmaRecvIrqCallback(void *pParam, uint16_t size, uint32_t flag);
#endif

#if (CONFIG_HIF_PHY_UART_SEND_MODE & (CONFIG_HIF_PHY_TRANS_MODE_INT | CONFIG_HIF_PHY_TRANS_MODE_DMA))
static void hif_PHY_UART_SendIrqCallback(HAL_Dev_t * pDevice, void *arg);
#endif

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
static void hif_PHY_UART_RecvIrqCallback(HAL_Dev_t * pDevice, void *arg);
#endif

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int hif_PHY_UART_Init(HIF_PHY_InitCfg_t *pInitCfg)
{
    HIF_LOG_DBG("hif_PHY_UART_Init");

    if (pInitCfg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    UART_InitParam_t uartCfg = {
        .baudRate       = pInitCfg->transFreq,
        .parity         = UART_PARITY_NONE,
        .stopBits       = UART_STOP_BIT_1,
        .dataBits       = UART_DATA_WIDTH_8,
        .autoFlowCtrl   = UART_FLOW_CTRL_NONE,
    };
    hifPHYCtrl.hifDev = HAL_UART_Init(hifPHYCtrl.devId, &uartCfg);
    if (hifPHYCtrl.hifDev == NULL) {
        HIF_LOG_ERR("HAL_UART_Init Fail");
        return HIF_ERRCODE_IO_ERROR;
    }


#if ((CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
    (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
    if ((hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) ||
        (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA)) {
        HIF_DMA_InitCfg_t dmaCfg = {
            .devId      = 0,
        };

#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
            dmaCfg.sendCb       = hif_PHY_UART_DmaSendIrqCallback;
            dmaCfg.errCb        = hif_PHY_UART_DmaSendErrorIrqCallback;

            if (hifPHYCtrl.devId == 0) {
                dmaCfg.sendShake    = DMA_HANDSHAKE_TXFIFO_UART0;
                dmaCfg.sendPhyAddr  = DMA_UART0_TX_ADDR;
            } else if (hifPHYCtrl.devId == 1) {
                dmaCfg.sendShake    = DMA_HANDSHAKE_TXFIFO_UART1;
                dmaCfg.sendPhyAddr  = DMA_UART1_TX_ADDR;
            } else {
                return HIF_ERRCODE_INVALID_PARAM;
            }
        } else {
            dmaCfg.sendShake    = 0xFF;
            dmaCfg.sendCb       = NULL;
            dmaCfg.errCb        = NULL;
            dmaCfg.sendPhyAddr  = 0;
        }
#endif

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
            dmaCfg.recvCb       = hif_PHY_UART_DmaRecvIrqCallback;

            if (hifPHYCtrl.devId == 0) {
                dmaCfg.recvShake    = DMA_HANDSHAKE_RXFIFO_UART0;
                dmaCfg.recvPhyAddr  = DMA_UART0_RX_ADDR;
            } else if (hifPHYCtrl.devId == 1) {
                dmaCfg.recvShake    = DMA_HANDSHAKE_RXFIFO_UART1;
                dmaCfg.recvPhyAddr  = DMA_UART1_RX_ADDR;
            } else {
                return HIF_ERRCODE_INVALID_PARAM;
            }
        } else {
            dmaCfg.recvShake    = 0xFF;
            dmaCfg.recvCb       = NULL;
            dmaCfg.recvPhyAddr  = 0;
        }
#endif

        int status = hif_PHY_DMA_Init(&dmaCfg);
        if (status != HIF_ERRCODE_SUCCESS) {
            return HIF_ERRCODE_IO_ERROR;
        }
    }
#endif


    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_UART_Deinit(void)
{
    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_UART_Open(void)
{
    HIF_LOG_DBG("hif_PHY_UART_Open");

    if (HAL_UART_Open(hifPHYCtrl.hifDev)) {
        HIF_LOG_ERR("HAL_UART_Open Fail");
        return HIF_ERRCODE_IO_ERROR;
    }

    /* 1> config spi send and recv */
    int status = HIF_ERRCODE_SUCCESS;

#if ((CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
     (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
    if ((hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) ||
        (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA)) {

        status = hif_PHY_DMA_Open();
        if (status != HIF_ERRCODE_SUCCESS) {
            return HIF_ERRCODE_IO_ERROR;
        }

        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
            HAL_UART_SetTxFifoLevel(hifPHYCtrl.hifDev, UART_TX_FIFO_LEVEL_2_CHAR);

            HAL_Callback_t uartIrqCb = {NULL, NULL};
            uartIrqCb.cb = hif_PHY_UART_SendIrqCallback;
            HAL_UART_RegisterIRQ(hifPHYCtrl.hifDev, UART_DIR_TX, &uartIrqCb);
        }

        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
            return HIF_ERRCODE_INVALID_PARAM;
        }
    }
#endif

#if ((CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
     (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT))
    if ((hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) ||
        (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT)) {
        HAL_Callback_t uartIrqCb = {NULL, NULL};

#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
        if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) {
            uartIrqCb.cb = hif_PHY_UART_SendIrqCallback;
            HAL_UART_RegisterIRQ(hifPHYCtrl.hifDev, UART_DIR_TX, &uartIrqCb);
        }
#endif

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
        if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT) {
            uartIrqCb.cb = hif_PHY_UART_RecvIrqCallback;
            HAL_UART_RegisterIRQ(hifPHYCtrl.hifDev, UART_DIR_RX, &uartIrqCb);
        }
#endif
    }
#endif


    return status;
}


int hif_PHY_UART_Close(void)
{

    return HIF_ERRCODE_SUCCESS;
}


__hif_isr_text int hif_PHY_UART_SendStart(HIF_Data_Node_t * pDataNode, uint32_t param)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
        if (param == HIF_PHY_SEND_FLUSH) {
            hif_PHY_SendCallback(HIF_TRANS_STATUS_FLUSH);
        } else {
            while (!HAL_UART_IsTxEmpty(hifPHYCtrl.hifDev)) {
            }

            if (param == HIF_PHY_SEND_NOMAL) {
                hif_PHY_DMA_Send(pDataNode);
                HAL_UART_EnableDma(hifPHYCtrl.hifDev);
                hif_PHY_DMA_SendStart();

            } else if (param == HIF_PHY_SEND_SPEC) {
                HAL_UART_EnableTxReadyIRQ(hifPHYCtrl.hifDev);

            }
        }

    } else
#endif

#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) {
        if (param == HIF_PHY_SEND_FLUSH) {
            HAL_UART_DisableTxReadyIRQ(hifPHYCtrl.hifDev);
            HAL_UART_SetTxFifoLevel(hifPHYCtrl.hifDev, UART_TX_FIFO_LEVEL_1_2);
        } else {
            status = HAL_UART_EnableTxReadyIRQ(hifPHYCtrl.hifDev);
        }
    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }

    return status;
}

__hif_isr_text int hif_PHY_UART_Send(void *pData, uint16_t len)
{
    if (pData == NULL) {
         return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t *pDataBuff = (uint8_t *)pData;
    int sendIdx = 0;
    for (sendIdx = 0; sendIdx < len; sendIdx++) {
        if (HAL_UART_IsTxReady(hifPHYCtrl.hifDev) == 0) {
            break;
        }
        HAL_UART_PutTxData(hifPHYCtrl.hifDev, pDataBuff[sendIdx]);
    }

    return sendIdx;
}


__hif_isr_text int hif_PHY_UART_SendStop(void)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
        hif_PHY_DMA_SendStop();
        HAL_UART_Reset(hifPHYCtrl.hifDev, UART_RST_TYPE_TX_FIFO);
    } else
#endif

#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) {

        while (!HAL_UART_IsTxEmpty(hifPHYCtrl.hifDev)) {
        }
        HAL_UART_DisableTxReadyIRQ(hifPHYCtrl.hifDev);
    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }

    return status;
}


__hif_sram_text int hif_PHY_UART_SendIsDone(void)
{
    if (HAL_UART_IsTxEmpty(hifPHYCtrl.hifDev)) {
        return 1;
    } else {
        return 0;
    }
}


__hif_isr_text int hif_PHY_UART_RecvStart(HIF_Data_Node_t * pDataList)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {
        return status;
    } else
#endif

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT) {
        HAL_UART_EnableRxReadyIRQ(hifPHYCtrl.hifDev);
    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }

    return status;
}

__hif_isr_text int hif_PHY_UART_Recv(void *pData, uint16_t len)
{
    if (pData == NULL) {
         return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t *pDataBuff = (uint8_t *)pData;
    int recvIdx = 0;
    for (recvIdx = 0; recvIdx < len; recvIdx++) {
        if (HAL_UART_IsRxReady(hifPHYCtrl.hifDev) == 0) {
            break;
        }
        pDataBuff[recvIdx] = HAL_UART_GetRxData(hifPHYCtrl.hifDev);
    }

    return recvIdx;
}


__hif_isr_text int hif_PHY_UART_RecvStop(void)
{
    int status = HIF_ERRCODE_SUCCESS;

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_DMA) {

    } else
#endif

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
    if (hifPHYCtrl.recvMode == HIF_PHY_TRANS_MODE_INT) {
        HAL_UART_DisableRxReadyIRQ(hifPHYCtrl.hifDev);
    } else
#endif

    {
        status = HIF_ERRCODE_NOT_READY;
    }

    return status;
}


#if (CONFIG_HIF_PHY_UART_SEND_MODE & (CONFIG_HIF_PHY_TRANS_MODE_INT | CONFIG_HIF_PHY_TRANS_MODE_DMA))
__hif_isr_text static void hif_PHY_UART_SendIrqCallback(HAL_Dev_t * pDevice, void *arg)
{
#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
     if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
         if(HAL_UART_IsTxEmpty(hifPHYCtrl.hifDev)) {
             HAL_UART_DisableTxReadyIRQ(hifPHYCtrl.hifDev);
             hif_PHY_SendCallback(HIF_TRANS_STATUS_DONE);
         }

     } else
#endif

#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
     if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_INT) {
         hif_PHY_SendCallback(HIF_TRANS_STATUS_PART);
     } else
#endif
    {

    }
}
#endif

#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT)
__hif_sram_text static void hif_PHY_UART_RecvIrqCallback(HAL_Dev_t * pDevice, void *arg)
{
    hif_PHY_RecvCallback(HIF_TRANS_STATUS_DONE, 0);
}
#endif


#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
__hif_isr_text static int hif_PHY_UART_DmaSendIrqCallback(void *pParam, uint16_t size, uint32_t flag)
{
#if (CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
     if (hifPHYCtrl.sendMode == HIF_PHY_TRANS_MODE_DMA) {
         hif_PHY_SendCallback(HIF_TRANS_STATUS_PART);
         HAL_UART_EnableTxReadyIRQ(hifPHYCtrl.hifDev);
     }
#endif

    return 0;
}


__hif_sram_text static int hif_PHY_UART_DmaSendErrorIrqCallback(void *pParam, uint16_t size, uint32_t flag)
{
    return 0;
}
#endif


#if (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA)
__hif_sram_text static int hif_PHY_UART_DmaRecvIrqCallback(void *pParam, uint16_t size, uint32_t flag)
{
    return 0;
}
#endif


#if (CONFIG_HIF_PM == 1)

int hif_PHY_UART_PmInit(HIF_PHY_PMCallback_t pmCb, uint8_t pCfg)
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
        status = HAL_UART_ExtControl(hifPHYCtrl.hifDev, UART_PARAM_WAKEUP_THRESHOLD, &thresh);
        if (status != HAL_STATUS_OK) {
            break;;
        }

        status = HAL_UART_ExtControl(hifPHYCtrl.hifDev, UART_PARAM_WAKEUP_CALLBACK, &wakeCb);
        if (status != HAL_STATUS_OK) {
            break;;
        }

        status = HAL_UART_ExtControl(hifPHYCtrl.hifDev, UART_PARAM_WAKEUP_CTRL, &enable);
    } while (0);

    return status;
}

int hif_PHY_UART_PmDeinit(void)
{
    if (hifPHYCtrl.hifDev == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    uint8_t enable = 0;

    HAL_Status_t status = HAL_STATUS_OK;
    do {
        status = HAL_UART_ExtControl(hifPHYCtrl.hifDev, UART_PARAM_WAKEUP_CALLBACK, NULL);
        if (status != HAL_STATUS_OK) {
            break;;
        }
        status = HAL_UART_ExtControl(hifPHYCtrl.hifDev, UART_PARAM_WAKEUP_CTRL, &enable);
    } while (0);


    return status;
}

#endif

#endif /* ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_UART != 0)) */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
