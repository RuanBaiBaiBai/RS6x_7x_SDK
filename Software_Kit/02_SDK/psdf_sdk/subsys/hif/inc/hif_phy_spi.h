/**
 ******************************************************************************
 * @file    hif_phy_spi.h
 * @brief   hif phy spi define.
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

#ifndef _HIF_PHY_SPI_H_
#define _HIF_PHY_SPI_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "hif_types.h"
#include "hif_config.h"

#if (CONFIG_HIF_PHY_SPI != 0)
#include "hif_phy.h"

#include "hal_spi.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ----------------------------------------------------------------------------
 */

typedef struct {
    uint32_t        spiFreq;
    uint8_t         spiClkMode;
    uint8_t         spiIoMode;
} HIF_PHY_SPI_Cfg_t;

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

/* Exported macro.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int hif_PHY_SPI_Init(HIF_PHY_InitCfg_t *pInitCfg);

int hif_PHY_SPI_Deinit(void);

int hif_PHY_SPI_Open(void);

int hif_PHY_SPI_Close(void);


int hif_PHY_SPI_SendStart(HIF_Data_Node_t * pDataList, uint32_t param);

int hif_PHY_SPI_Send(void *pData, uint16_t len);

int hif_PHY_SPI_SendStop(void);


int hif_PHY_SPI_RecvStart(HIF_Data_Node_t * pDataList);

int hif_PHY_SPI_Recv(void *pData, uint16_t len);

int hif_PHY_SPI_RecvStop(void);

#if (CONFIG_HIF_PM == 1)
int hif_PHY_SPI_PmInit(HIF_PHY_PMCallback_t pmCb, uint8_t pCfg);

int hif_PHY_SPI_PmDeinit(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* (CONFIG_HIF_PHY_SPI != 0) */


#endif /* _HIF_PHY_SPI_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
