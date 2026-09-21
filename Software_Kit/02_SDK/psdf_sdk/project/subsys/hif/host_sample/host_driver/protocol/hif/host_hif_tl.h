/**
 **************************************************************************************************
 * @file    host_hif_tl.h
 * @brief   Definition of HIF Transport Layer.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HOST_HIF_TL_H_
#define _HOST_HIF_TL_H_

/* Includes.*/
#include "../../porting/host_port.h"
#include "hif_type.h"
#include "host_hif.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */

/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef void* TL_HANDLE;
typedef void  (*MSG_PROC_CB)(hif_msg_hdr_t *msg, uint8_t *buffer, void *arg);
typedef void* (*MSG_BUFFER_GET_CB)(hif_msg_hdr_t *msg, uint32_t size, void *arg);
typedef void  (*MSG_BUFFER_PUT_CB)(hif_msg_hdr_t *msg, uint8_t *buffer, void *arg);


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */

/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
TL_HANDLE HIF_TL_Init(HifCfg_t *cfg, LLC_HANDLE llc_hdl);
void HIF_TL_DeInit(TL_HANDLE tl_hdl);
void HIF_TL_DevReset(TL_HANDLE tl_hdl);

void HIF_TL_WaitCmdResp_Abort(TL_HANDLE tl_hdl);

int HIF_TL_SendMsg(TL_HANDLE tl_hdl, hif_msg_hdr_t *msg, void *payload);
int HIF_TL_Disable_DevSleep(TL_HANDLE tl_hdl, uint8_t disable_sleep);

int HIF_TL_MsgHandle_Regist(TL_HANDLE tl_hdl, MSG_PROC_CB proc_cb, MSG_PROC_CB prev_cb,
			MSG_BUFFER_GET_CB buffer_get, MSG_BUFFER_PUT_CB buffer_put, void *cb_arg);
#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
int HIF_TL_PollAck_Set(TL_HANDLE tl_hdl, hif_msg_hdr_t *msg, void *ack_info, uint32_t len);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _HOST_HIF_TL_H_ */

