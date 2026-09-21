/**
 ******************************************************************************
 * @file    hif_dll.h
 * @brief   hif data link layer define.
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

#ifndef _HIF_DLL_H_
#define _HIF_DLL_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "hif_types.h"
#include "hif_mem.h"
#include "hif_phy.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ----------------------------------------------------------------------------
 */

typedef int (* HIF_DLL_TransCallback_t)(HIF_Frame_Node_t * pFrame, uint32_t flag);


/**
 * @brief Initialization configuration structure for HIF DLL module
 * Contains callbacks, buffers, and physical layer configuration
 */
typedef struct {
    HIF_DLL_TransCallback_t     sendCb;
    HIF_DLL_TransCallback_t     recvCb;

    uint32_t                    cascadeThresh;

    HIF_PHY_InitCfg_t           *pPHYCfg;
} HIF_DLL_InitCfg_t;

/* Exported constants.
 * ----------------------------------------------------------------------------
 */
#define HIF_TRANS_FLAG_LAST_FRAME               0x01
#define HIF_FRAME_FLAG_CHECKSUME                0x02
#define HIF_FRAME_FLAG_REFRESH_DATA             0x04
#define HIF_FRAME_FLAG_CMD_FRAME                0x10
#define HIF_FRAME_FLAG_END_FRAME                0x20
#define HIF_FRAME_FLAG_FRAME_HEAD               0x40
#define HIF_FRAME_FLAG_FRAME_TAIL               0x80


#define HIF_DLL_FLAG_RECV_FRAME                 0x01
#define HIF_DLL_FLAG_RECV_FAIL                  0x02
#define HIF_DLL_FLAG_RECV_WAKE                  0x03

#define HIF_DLL_FLAG_SEND_FRAME                 0x10
#define HIF_DLL_FLAG_SEND_RSP_DONE              0x20
#define HIF_DLL_FLAG_SEND_RPT_DONE              0x40
#define HIF_DLL_FLAG_SEND_WAKE                  0x80
#define HIF_DLL_FLAG_SEND_FLUSH                 0x100


/* Exported macro.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int hif_DLL_Init(HIF_DLL_InitCfg_t *pInitCfg);

int hif_DLL_Deinit(void);

int hif_DLL_Open(void);

int hif_DLL_Close(void);

int hif_DLL_Send(HIF_Frame_Node_t *pFrameNode);

int hif_DLL_SendFlush(void);

int hif_DLL_Recv(HIF_Frame_Node_t * pFrameNode);

#define hif_DLL_GetTransDummy()     hif_PHY_GetTransDummy()

#define hif_DLL_SendDataNodeInit(data, len, node, flag)     hif_PHY_SendDataNodeInit((data), (len), (node), (flag))

#define hif_DLL_SendDataNodeDeinit(dataNode)                hif_PHY_SendDataNodeDeinit(dataNode)

#define hif_DLL_SendDataNodeRefresh(dataNode)               hif_PHY_SendDataNodeRefresh(dataNode)

#define hif_DLL_SendIsDone()                                hif_PHY_SendIsDone()

void hif_DLL_SendWakeupResp(HIF_Data_Node_t * pDataNode);

int hif_DLL_EndFrameIsBusy(void);

void hif_DLL_EndFrameRemove(void);

#ifdef __cplusplus
}
#endif

#endif /* _HIF_DLL_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
