/**
 **************************************************************************************************
 * @file    host_hif.h
 * @brief   Definition of HIF Type and APIs.
 * @attention
 *          Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.*/
#ifndef __HOST_HIF_H__
#define __HOST_HIF_H__

/* Includes.*/
#include "../../porting/host_port.h"
#include "../llc/host_llc.h"
#include "hif_type.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef void*  HIF_HANDLE;

typedef struct {
	uint8_t  poll_enable;
	uint8_t  app_retry_enable;
	uint8_t  tl_retry_enable;
	uint8_t  fragment_enable;
	uint8_t  fragment_retry_enable;

	uint16_t burst_num; /* default burst number */
	uint16_t cmd_buf_len;   /* length of TL buffer to send a command */
	uint16_t poll_buf_len;  /* length of TL buffer to send a command */
} HifCfg_t;

typedef struct {
	uint32_t cmd_cnt;
	uint32_t cmd_err_cnt;

	uint32_t report_cnt;
	uint32_t report_max_size;
	uint32_t report_min_size;

	uint32_t rx_head_err_cnt;
	uint32_t rx_data_err_cnt;
} HifCount_t;

typedef struct {
	/* process state of HIF */
	#define DEV_STATE_IDLE                0
	#define DEV_STATE_IN_SLEEP            1
	#define DEV_STATE_IN_BURNING          2
	#define DEV_STATE_IN_CMD              3
	#define DEV_STATE_IN_REPORTING        4
	uint8_t  state;
	/* last error code */
	uint8_t  last_cmd;
	uint8_t  last_report;
	uint8_t  last_error;
} HifState_t;

typedef void (*Msg_Report_CB)(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg);
typedef void (*Crypter)(uint32_t offset, uint8_t *data, uint32_t len);

/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
HIF_HANDLE  HIF_Entity_Init(HifCfg_t *user_cfg, LLC_HANDLE llc_hdl);
void        HIF_Entity_DeInit(HIF_HANDLE hdl);
void        HIF_Entity_DevReset(HIF_HANDLE hdl);
int         HIF_Entity_DevSleep_Disable(HIF_HANDLE hdl, bool disable);
HifCfg_t*   HIF_Config_Get(HIF_HANDLE hdl);
HifCfg_t*   HIF_Config_Confirm(HIF_HANDLE hdl);
HifState_t* HIF_State_Get(HIF_HANDLE hdl);
HifCount_t* HIF_Counters_Get(HIF_HANDLE hdl);

int HIF_Fragment_State_Get(
    HIF_HANDLE hdl, uint32_t msg_id,
    uint32_t *hold_buffer_len,
    uint32_t *pending_len, uint32_t *abnormal_len, uint32_t *free_len
);

int HIF_Cmd_Request(HIF_HANDLE hdl, uint32_t msg_id, void *param,
					uint32_t param_len, void *resp_buff, uint32_t buffer_len, uint32_t *data_len);
#if (CFG_APP_RETRY_EN)
int HIF_Send_ReportAck(HIF_HANDLE hdl, uint32_t msg_id, void *ack, uint32_t ack_len);
#endif
int HIF_Report_Handle_Regist(HIF_HANDLE hdl, uint8_t msg_id, Msg_Report_CB cb, void *arg);
int HIF_Report_Handle_Get(HIF_HANDLE hdl, uint8_t msg_id, Msg_Report_CB *pCB, void **pArg);

int HIF_Report_Buffer_Set(HIF_HANDLE hdl, uint8_t msg_id, void *buffer, uint32_t buffer_len);
int HIF_Encrypt_Regist(HIF_HANDLE hdl, uint32_t enc_cfg, Crypter enc, Crypter dec);

#ifdef __cplusplus
}
#endif

#endif /* __HOST_HIF__ */

