/**
 ******************************************************************************
 * @file    hif_tl.h
 * @brief   hif_tl define.
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

#ifndef __HIF_TL_H__
#define __HIF_TL_H__

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "hif_types.h"
#include "hif_msg.h"
#include "hif_log.h"
#include "hif_mem.h"
#include "hif.h"
#include "hif_checksum.h"
#include "hif_ml.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported macro.
 * ----------------------------------------------------------------------------
 */
#ifndef wrap32_before
#define wrap32_before(a, b)           ((int32_t)((a) - (b)) < 0)
#define wrap32_after(a, b)            wrap32_before(b, a)
#define wrap32_diff(a, b)             ((int32_t)((a) - (b)))
#endif

/* Exported types.
 * ----------------------------------------------------------------------------
 */

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
#if CONFIG_HIF_TL_ACK_TIMER_ENABLE

int8_t hif_tl_init(void);
int8_t hif_tl_deInit(void);

OSI_Status_t hif_tl_ack_timer_start(uint32_t timeoutMs);
OSI_Status_t hif_tl_ack_timer_stop(void);
#endif

#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
bool hif_tl_frame_isTimeout(HIF_Frame_Node_t *frameItem);
#endif      /* CONFIG_HIF_TL_ACK_TIMER_ENABLE */


#if CONFIG_HIF_SPLIT_ENA
void hif_tl_frame_checksum32(HIF_Frame_Node_t *frame);

HIF_Frame_Node_t* hif_tl_pack_frame(HIF_MsgHdr_t *msgHeader,
    hif_frag_hdr_t *fragHdr,
    HIF_Data_DoubleHead_t *dataQueue,
    void *sendCb,
    TickType_t endTick);

uint16_t hif_tl_frame_trans(HIF_Frame_Node_t *frameLink);

void hif_tl_frame_update_seq_checksum(HIF_Frame_Node_t *pFrame);
#endif      /* CONFIG_HIF_SPLIT_ENA */

#ifdef __cplusplus
}
#endif

#endif

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

