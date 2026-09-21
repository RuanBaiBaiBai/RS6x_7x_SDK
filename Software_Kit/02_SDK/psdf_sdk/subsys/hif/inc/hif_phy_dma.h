/**
 ******************************************************************************
 * @file    hif_phy_dma.h
 * @brief   hif phy dma define.
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

#ifndef _HIF_PHY_DMA_H_
#define _HIF_PHY_DMA_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "hif_types.h"
#include "hif_phy.h"

#include "hal_dma.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

/* Exported types.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    uint8_t                 devId;
    uint8_t                 sendShake;
    uint8_t                 recvShake;

    uint32_t                sendPhyAddr;
    uint32_t                recvPhyAddr;

    HIF_PHY_TransCallback_t sendCb;
    HIF_PHY_TransCallback_t recvCb;
    HIF_PHY_TransCallback_t errCb;

} HIF_DMA_InitCfg_t;

/* Exported macro.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

#if (CONFIG_HIF_PHY_DMA == 1)
int hif_PHY_DMA_Init(HIF_DMA_InitCfg_t *pInitCfg);

int hif_PHY_DMA_Deinit(void);

int hif_PHY_DMA_Open(void);

int hif_PHY_DMA_Close(void);

int hif_PHY_DMA_Recv(void * pDmaNode);

int hif_PHY_DMA_Send(void * pDmaNode);

int hif_PHY_DMA_RecvStart(void);

int hif_PHY_DMA_RecvStop(void);

int hif_PHY_DMA_SendStart(void);

int hif_PHY_DMA_SendStop(void);

uint32_t hif_PHY_DMA_GetRecvSize(void);

void hif_PHY_DMA_SendDataNodeInit(uint8_t *pData, uint16_t len, HIF_Data_Node_t *pDataNode, uint32_t flag);

uint8_t * hif_PHY_DMA_SendDataNodeDeinit(HIF_Data_Node_t *pDataNode);

#endif




#ifdef __cplusplus
}
#endif

#endif /* _HIF_PHY_DMA_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
