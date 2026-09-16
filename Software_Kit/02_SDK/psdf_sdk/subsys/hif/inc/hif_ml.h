/**
 ******************************************************************************
 * @file    hif_ml.h
 * @brief   hif_ml define.
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

#ifndef __HIF_ML_H__
#define __HIF_ML_H__

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "hif_types.h"
#include "hif_msg.h"
#include "hif_log.h"
#include "hif_mem.h"
#include "hif.h"
#include "hif_checksum.h"

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
/**
 * @brief Fragment header in start of payload of each fragment frame
 */
typedef struct _hif_frag_hdr_t_ {
    uint32_t totalLen   : 32;   /*< Message Total length, including all fragment of the message */
    uint32_t offset     : 24;   /*< Offset(byte) of the fragment in the message */
    uint32_t msgSeq     : 6;    /*< Sequence of specified MsgID */
    uint32_t reserved   : 2;    /*< Reserved */
} hif_frag_hdr_t;

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

#define HIF_ML_FRAG_HDR_LEN                 sizeof(hif_frag_hdr_t)
#define HIF_ML_FRAG_REAL_PAYLOAD_LEN        (CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE - HIF_ML_FRAG_HDR_LEN)

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
#if CONFIG_HIF_SPLIT_ENA

#if CONFIG_HIF_SEND_SPLIT_RETRY_ENA
typedef struct _hif_ml_frag_ack_t_ {
    uint32_t blank_len  : 32;
    uint32_t offset     : 24;
    uint32_t msgSeq     : 6;
    uint32_t reserve    : 2;
} hif_ml_frag_ack_t;
// typedef hif_frag_hdr_t  hif_ml_frag_ack_t;
#endif

int hif_ml_init(void);
int hif_ml_deInit(void);

/**
 * @brief callback(default) type of message-layer
 */
typedef void (*hif_ml_sendCallback_t)(HIF_Frame_Node_t *);

uint32_t hif_data_item_link_calDataLen(HIF_Data_Node_t *link);

int hif_ml_frag_send(HIF_MsgHdr_t *msgHeader,
    HIF_Data_DoubleHead_t *dataQueue,
    uint32_t appDataTotalLen,
    HIF_MsgTrans_Callback appSentCb,
    uint32_t timeoutMs);

#endif      /* CONFIG_HIF_SPLIT_ENA */

typedef struct _hif_msg_tlv_t_ {
    uint8_t  msgid;
    #define HIF_MSG_TLV_APP_ACK     0   /*< Application-layer's ack */
    #define HIF_MSG_TLV_ML_ACK      1   /*< Fragment ack of message-layer */
    #define HIF_MSG_TLV_TL_ACK      2   /*< Ack of transport-layer retry */
    uint8_t  type;
    uint16_t length;                    /* Length of data[], unit: byte */
    uint8_t  data[0];                   /* The ack data of the msgid, parsed according to the type:
                                            type == 0:
                                            type == 1: Parsing data[] into struct, hif_ml_frag_ack_t
                                            type == 2: Parsing data[] into byte */
} hif_msg_tlv_t;

typedef struct _hif_host_poll_ack_tlv_t_ {
    uint8_t  poll_type;
    uint8_t  tlvNum;
    uint16_t burst_period;
    hif_msg_tlv_t msgTlv[0];
} hif_host_poll_ack_tlv_t;

uint16_t hif_host_ack_tvl_handler(hif_host_poll_ack_tlv_t *host_ack_tlv);
extern void cube_report_retry_handler(uint8_t *data, uint32_t retry_num);

#ifdef __cplusplus
}
#endif

#endif      /* __HIF_ML_H__ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

