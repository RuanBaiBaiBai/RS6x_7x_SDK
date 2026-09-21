/**
 **************************************************************************************************
 * @file    host_mmw_report_api.c
 * @brief   Implement of MMW report APIs for Host.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.*/
#include "host_mmw_api.h"

typedef struct {
    uint32_t RetryOffset;  /* unit=DWORD(0xC1/0xC3) or BYTE(0xC2) */
	uint32_t RetryLen;     /* unit=DWORD(0xC1/0xC3) or BYTE(0xC2) */
} FrameHdr_Retry_t;

#define REPORT_FLAG_BUFFER_USER        BIT(0)
#define REPORT_FLAG_BUFFER_HOLD        BIT(1)
typedef struct {
	DEV_HANDLE DevHdl;
	Report_CB  report_cb;
	void      *cb_arg;
	uint8_t   *buffer;
    uint32_t   bufferLen;

	uint8_t    msgId;
	uint8_t    flag;
	uint8_t    drop;
#if (CFG_APP_RETRY_EN)
	uint8_t    retry_max;
	uint8_t    retry_start;
	uint8_t    retry_end;
	void      *retry_node;
#else
	uint8_t    reserved;
#endif
    uint32_t   frameIdx;
    uint32_t   frameLen;    /* unit=byte */
    uint32_t   frameOffset; /* unit=byte */
} Report_Local_t;

/* Common Handle for C1, C2, C3 */
static void Mmw_Report_DataPiece_Process(uint32_t msg_id, void *payload, uint32_t payload_len, void *arg)
{
	Report_Local_t *local = (Report_Local_t *)arg;
	FrameHdr_Data_t *frame = (FrameHdr_Data_t *)payload;
    uint32_t fFrameLen = 0;   /* unit=byte */
    uint32_t cur_offset; /* unit=byte */
	uint32_t total_len;  /* unit=byte */
	uint32_t data_len;   /* unit=byte */
	if (payload_len < sizeof(FrameHdr_Data_t) || NULL == frame) {
		return ; /* nothing to do */
	}

    if (msg_id == 0xC2) {
        fFrameLen = frame->frameLen;
        cur_offset = frame->dataOffset;
    } else {
        fFrameLen = frame->frameLen << 2;
        cur_offset = frame->dataOffset << 2;
    }

	/* Get data len by strip frame header len */
	data_len = payload_len - sizeof(FrameHdr_Data_t);
	total_len = fFrameLen + sizeof(FrameHdr_Data_t);

	HOST_LOG_DBG("Report_%02X FrameIdx=%d, total_len=%u (piece=%d, off=%u)\n",
				msg_id, frame->frameIdx, total_len, payload_len, cur_offset);

	/* Init a new frame */
	if ((data_len && 0 == local->frameLen) || local->frameIdx != frame->frameIdx) {
	#if (CFG_APP_RETRY_EN)
		HOST_LOG_DBG("New Frame, Last Idx=%u len=%u, off=%u retry%u %u\n", local->frameIdx,
				local->frameLen, local->frameOffset, local->retry_start, local->retry_end);
	#else
		HOST_LOG_DBG("New Frame, Last Idx=%u len=%u, off=%u\n", local->frameIdx,
				local->frameLen, local->frameOffset);
	#endif
		
		if (payload_len < total_len) { /* multi pieces, need alloc buffer */
			/* re-allocate buffer if need */
			if (local->bufferLen < total_len) {
				if (!(local->flag & REPORT_FLAG_BUFFER_USER)) {
					if (NULL != local->buffer) {
						host_os_free(local->buffer);
					}
					local->buffer = host_os_malloc(total_len);
					if (NULL != local->buffer) {
						local->bufferLen = total_len;
					} else {
						local->bufferLen = 0;
						HOST_LOG_ERR("Report_%02X malloc buffer failed!(%u)\n", msg_id, total_len);
						return ; /* no buffer, do nothing */
					}
				} else {
					HOST_LOG_ERR("Report_%02X User buffer(%u) too small!(need %u)\n",
						msg_id, local->bufferLen, total_len);
				}
			}

		#if (CFG_APP_RETRY_EN)
			/* init retry info */
			uint32_t piece_num = (fFrameLen + data_len - 1) / data_len;
			if (local->retry_max < piece_num) {
				uint32_t total_size = sizeof(FrameHdr_Retry_t) * piece_num;
				if (NULL != local->retry_node) {
					host_os_free(local->retry_node);
				}
				local->retry_node = host_os_malloc(total_size);
			}
			if (NULL != local->retry_node) {
				FrameHdr_Retry_t *retry = local->retry_node;
				local->retry_max = piece_num;
				local->retry_start = 0;
				local->retry_end   = 1;
				retry->RetryOffset = 0;
                retry->RetryLen    = frame->frameLen;
			} else {
				local->retry_max   = 0;
				local->retry_start = 0;
				local->retry_end   = 0;
			}
		#endif

			host_os_memcpy(local->buffer, frame, sizeof(FrameHdr_Data_t));
			local->frameLen    = fFrameLen;
			local->frameIdx    = frame->frameIdx;
			local->frameOffset = 0;
			local->drop        = 0;
		}else { /* only one piece */
			HOST_LOG_DBG("Report_%02X len=%u\n", msg_id, total_len);
			local->report_cb(local->msgId, (uint8_t *)frame, total_len, local->cb_arg);
			local->frameLen    = 0;
			return ;
		}
	}

	if (local->drop) { /* drop frame if something is wrong */
		if (NULL != local->buffer &&
			!(local->flag & (REPORT_FLAG_BUFFER_USER | REPORT_FLAG_BUFFER_HOLD))) {
			host_os_free(local->buffer);
			local->buffer = NULL;
			local->bufferLen = 0;
		}
		return ;
	}

	if (data_len) {
		/* Merge data pieces of frame */
		uint32_t buffer_need =  sizeof(FrameHdr_Data_t) + (cur_offset + data_len);
		if (buffer_need <= local->bufferLen) {
			uint8_t *data_buf = (uint8_t *)(local->buffer + sizeof(FrameHdr_Data_t));
			host_os_memcpy(&data_buf[cur_offset], &frame->data[0], data_len);
		} else {
			HOST_LOG_WRN("Report_%02X User buffer(%u) is small than frame len(%u)!!\n",
							msg_id, local->bufferLen, buffer_need);
		}

		if (local->frameOffset == cur_offset) {
			local->frameOffset += data_len;
		} else {
		#if (CFG_APP_RETRY_EN)
			if (NULL == local->retry_node) {
				local->drop = true;
				return ;
			}
		#else
			local->drop = true;
			return ;
		#endif
		}

	#if (CFG_APP_RETRY_EN)
		/* Update retry node */
		if (NULL != local->retry_node) {
			uint32_t retry_offset = frame->dataOffset;
			uint32_t retry_len = msg_id == 0xC2 ? data_len : data_len>>2;
			FrameHdr_Retry_t *node = local->retry_node;
			for (int i = local->retry_start; i < local->retry_end; ++i) {
				uint32_t blank_off = node[i].RetryOffset;
				uint32_t blank_end = blank_off + node[i].RetryLen;
				if (blank_end > retry_offset) { /* in the blank */
					if (blank_off == retry_offset) { /* at start */
						node[i].RetryOffset += retry_len;
						node[i].RetryLen -= retry_len;
					} else if (blank_end == retry_offset + retry_len) { /* at end */
						node[i].RetryLen -= retry_len;
					} else {
						/* in the middle, need add a node */
						if (local->retry_end < local->retry_max) {
							int j = local->retry_end++;
							int n = i + 1;
							while (j > n) {
								uint32_t *dst = (uint32_t *)&node[j];
								uint32_t *src = (uint32_t *)&node[j-1];
								*dst++ = *src++;
								*dst = *src;
								j--;
							}
							node[n].RetryOffset = retry_offset + retry_len;
							node[n].RetryLen    = blank_end - node[n].RetryOffset;
							node[i].RetryLen    = retry_offset - blank_off;
							HOST_LOG_DBG("Report_%02X Add a AckNode%d!(off=%u, len=%u)\n",
										msg_id, n, node[n].RetryOffset, node[n].RetryLen);
						} else {
							HOST_LOG_ERR("Report_%02X curoff=%u No AckNode!(max=%u)\n",
										msg_id, retry_offset, local->retry_max);
							for (int j = local->retry_start; j < local->retry_end; ++j) {
								HOST_LOG_ERR("AckNode%d: off=%u, len=%u\n",
									j, node[j].RetryOffset, node[j].RetryLen);
							}
						}
					}
		
					/* node is complete, remove it */
					if (node[i].RetryLen == 0) {
						HOST_LOG_DBG("Report_%02X Remove a AckNode%d!(off=%u, len=%u)\n",
									msg_id, i, node[i].RetryOffset, node[i].RetryLen);
						if (i == local->retry_start) {
							local->retry_start++;
						} else {
							local->retry_end--;
							if (i < local->retry_end) {
								uint32_t copy_size	= sizeof(fragment_ack_t);
								copy_size = copy_size * (local->retry_end - i);
								host_os_memcpy(&node[i], &node[i+1], copy_size);
							}
						}
					} else {
						HOST_LOG_DBG("Report_%02X Update a AckNode%d!(off=%u, blanklen=%u) start=%d end=%d\n",
							msg_id, i, node[i].RetryOffset, node[i].RetryLen,
							local->retry_start, local->retry_end);
					}
					break;
				}
			}
		}
	#endif
	}

#if (CFG_APP_RETRY_EN)
	if (NULL != local->retry_node) {
		/* handle retry */
		if (local->retry_start < local->retry_end) {
			if (cur_offset + data_len >= fFrameLen || 0 == data_len) { /* Last data piece Or AckRequest */
				HOST_LOG_DBG("Report_%02X *** Rerty ACK=%u %d ***\n", msg_id, local->retry_start, local->retry_end);
				uint32_t ack_size = sizeof(FrameHdr_Retry_t) * (local->retry_end - local->retry_start);
				FrameHdr_Retry_t *node = local->retry_node;
				HIF_Send_ReportAck(Host_HIF_Handle_Get(local->DevHdl), msg_id, &node[local->retry_start], ack_size);
			}
		} else { /* Data complete */
			HOST_LOG_DBG("Report_%02X len=%u\n", msg_id, total_len);
			local->report_cb(local->msgId, (uint8_t *)local->buffer, total_len, local->cb_arg);
			local->frameLen = 0;
		}
	} else
#endif
	if (local->frameOffset >= fFrameLen) {
		/* Data complete with no retry */
		HOST_LOG_DBG("Report_%02X len=%u\n", msg_id, total_len);
		local->report_cb(local->msgId, (uint8_t *)local->buffer, total_len, local->cb_arg);
		local->frameLen = 0;
	}

	if (0 == local->frameLen && !(local->flag & (REPORT_FLAG_BUFFER_USER | REPORT_FLAG_BUFFER_HOLD))) {
		if (NULL != local->buffer) {
			host_os_free(local->buffer);
			local->buffer = NULL;
			local->bufferLen = 0;
		}
	}
}

static void Mmw_Report_General_Process(uint32_t msg_id, void *payload, uint32_t payload_len, void *arg)
{
	Report_Local_t *local = (Report_Local_t *)arg;
	local->report_cb(local->msgId, payload, payload_len, local->cb_arg);
}

static void Mmw_Report_Handle_Free(Report_Local_t *local)
{
	if (NULL != local) {
#if (CFG_APP_RETRY_EN)
		if (NULL != local->retry_node) {
			host_os_free(local->retry_node);
		}
#endif
		if (NULL != local->buffer && !(local->flag & REPORT_FLAG_BUFFER_USER)) {
			host_os_free(local->buffer);
		}
		host_os_free(local);
	}
}

int Mmw_Report_Handle_Set_General(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg,
		void *buffer, uint32_t buffer_len, uint8_t msg_id, void *proc_handle)
{
	Report_Local_t *local = NULL;
	HIF_HANDLE hif_hdl = Host_HIF_Handle_Get(DevHdl);
	HIF_Report_Handle_Get(hif_hdl, msg_id, NULL, (void **)&local);
	if (NULL != cb) {
		if (NULL == local) {
			local = host_os_malloc(sizeof(Report_Local_t));
			if (NULL == local) {
				HOST_LOG_ERR("Device(%p) Report_%02X Alloc handle failed(%u)\n",
					DevHdl, msg_id, sizeof(Report_Local_t));
				return HOST_ERRCODE_NO_BUFFER;
			}
			host_os_memset(local, 0, sizeof(Report_Local_t));
		}
		if (NULL != buffer && buffer_len >= sizeof(FrameHdr_Data_t)) {
			if (NULL != local->buffer && !(local->flag & REPORT_FLAG_BUFFER_USER)) {
				host_os_free(local->buffer);
			}
			local->buffer    = buffer;
			local->bufferLen = buffer_len;
			local->flag      = REPORT_FLAG_BUFFER_USER;
		} else {
			local->flag      = REPORT_FLAG_BUFFER_HOLD;
		}
		local->report_cb = cb;
		local->cb_arg = cb_arg;
		local->msgId  = msg_id;
		local->DevHdl = DevHdl;
		int status = HIF_Report_Handle_Regist(hif_hdl, msg_id, proc_handle, local);
		if (status != HOST_ERRCODE_SUCCESS) {
			HOST_LOG_ERR("Device(%p) Report_%02X handle regist failed(%d)\n", DevHdl, msg_id, status);
			Mmw_Report_Handle_Free(local);
			return status;
		}
	}else {
		HIF_Report_Handle_Regist(hif_hdl, msg_id, NULL, NULL);
		Mmw_Report_Handle_Free(local);
	}
	return HOST_ERRCODE_SUCCESS;
}

extern bool Host_DevHdl_IsValid(DEV_HANDLE DevHdl);

void Mmw_Report_Handle_FreeAll(DEV_HANDLE DevHdl)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_CUBE_DATA, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_FFT_DATA, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_OBJECTS, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_MOTION_DATA, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_MOTION_SENSOR_PLOT_DATA, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA0, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA1, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA2, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_HEARTBEAT_DATA, NULL);
		Mmw_Report_Handle_Set_General(DevHdl, NULL, NULL, NULL, 0, HIF_MSG_ID_STARTUP, NULL);
	}
}

int Mmw_C1_CubeData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_CUBE_DATA, &Mmw_Report_DataPiece_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_C2_FFTData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_FFT_DATA, &Mmw_Report_DataPiece_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_C3_Objects_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_OBJECTS, &Mmw_Report_DataPiece_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_C6_GeneralData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_MOTION_DATA, &Mmw_Report_General_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_C7_PlotData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_MOTION_SENSOR_PLOT_DATA, &Mmw_Report_General_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_C8_MotionSensor_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA0, &Mmw_Report_General_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_C9_MRS2_Data_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	return HOST_ERRCODE_UNSUPPORT;
}

int Mmw_CA_MotionSensor_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA1, &Mmw_Report_General_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}
int Mmw_CB_MotionSensor_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
	if (Host_DevHdl_IsValid(DevHdl)) {
		return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
						HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA2, &Mmw_Report_General_Process);
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_FE_Heartbeat_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
    if (Host_DevHdl_IsValid(DevHdl)) {
        return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
                        HIF_MSG_ID_HEARTBEAT_DATA, &Mmw_Report_General_Process);
    }
    return HOST_ERRCODE_INVALID_HANDLE;
}

int Mmw_FE_Heartbeat_Data_Parse(Heartbeat_Data_t *heartbeat_out, uint8_t *data, uint32_t data_len)
{
    uint32_t offset = 0;
    strMsgTlv *tlv = NULL;

    if (heartbeat_out == NULL || data == NULL || data_len <= 0) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    do {
        tlv = (strMsgTlv *)&data[offset];
        switch (tlv->type) {
            case HOST_HIF_MSG_FE_TLV_TYPE_RESPONSE_CNT:
                {
                    if (sizeof(heartbeat_out->response_cnt) == tlv->len) {
                        host_os_memcpy(&heartbeat_out->response_cnt, tlv->value, tlv->len);
                    } else {
                        HOST_LOG_ERR("%s: type=%d, len=%d, but need len=%d",
                            __func__, tlv->type, tlv->len, (int)sizeof(heartbeat_out->response_cnt));
                        return HOST_ERRCODE_INVALID_PARAM;
                    }
                }
                break;
            case HOST_HIF_MSG_FE_TLV_TYPE_FRAME_INDEX:
                {
                    if (sizeof(heartbeat_out->frame_index) == tlv->len) {
                        host_os_memcpy(&heartbeat_out->frame_index, tlv->value, tlv->len);
                    } else {
                        HOST_LOG_ERR("%s: type=%d, len=%d, but need len=%d",
                            __func__, tlv->type, tlv->len, (int)sizeof(heartbeat_out->frame_index));
                        return HOST_ERRCODE_INVALID_PARAM;
                    }
                }
                break;
            case HOST_HIF_MSG_FE_TLV_TYPE_MOTION_PT_NUM:
                {
                    if (sizeof(heartbeat_out->motion_pt_num) == tlv->len) {
                        host_os_memcpy(&heartbeat_out->motion_pt_num, tlv->value, tlv->len);
                    } else {
                        HOST_LOG_ERR("%s: type=%d, len=%d, but need len=%d",
                            __func__, tlv->type, tlv->len, (int)sizeof(heartbeat_out->motion_pt_num));
                        return HOST_ERRCODE_INVALID_PARAM;
                    }
                }
                break;
            case HOST_HIF_MSG_FE_TLV_TYPE_PRESENT_PT_NUM:
                {
                    if (sizeof(heartbeat_out->present_pt_num) == tlv->len) {
                        host_os_memcpy(&heartbeat_out->present_pt_num, tlv->value, tlv->len);
                    } else {
                        HOST_LOG_ERR("%s: type=%d, len=%d, but need len=%d",
                            __func__, tlv->type, tlv->len, (int)sizeof(heartbeat_out->present_pt_num));
                        return HOST_ERRCODE_INVALID_PARAM;
                    }
                }
                break;
            default:
                host_os_memset(heartbeat_out, 0, sizeof(heartbeat_out));
                HOST_LOG_ERR("%s: type=%d error\n", __func__, tlv->type);
                return HOST_ERRCODE_INVALID_PARAM;
        }
        offset += (tlv->len + 2);
    } while (offset < data_len);

    return HOST_ERRCODE_SUCCESS;
}

int Mmw_FF_Startup_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg)
{
    if (Host_DevHdl_IsValid(DevHdl)) {
        return Mmw_Report_Handle_Set_General(DevHdl, cb, cb_arg, NULL, 0,
                        HIF_MSG_ID_STARTUP, &Mmw_Report_General_Process);
    }
    return HOST_ERRCODE_INVALID_HANDLE;
}



#if 0

#if (CFG_APP_RETRY_EN)
#define TEST_APP_RETRY    1
#endif
typedef struct peice_test_param_t {
	hif_msg_hdr_t msghdr;
	uint32_t total_len;
	uint32_t peice_size;
	uint8_t *buffer;
	uint8_t  app_retry_req;
} APP_PEICE_TEST_PARAM;

void MMW_SendPeice_Test(Report_Local_t *local, APP_PEICE_TEST_PARAM *param,
			uint32_t frame_idx, uint32_t cnt_mask, uint32_t send_val)
{
	uint8_t  *paylaod_buf;
	uint32_t  offset = 0;
	uint32_t  data_value = 0;
	uint32_t  frag_cnt   = 0;

	/* send fragments */
	while (offset < param->total_len) {
		FrameHdr_Data_t *frame_hdr;
		uint32_t data_size = MIN(param->peice_size, param->total_len - offset);
		uint32_t payload_size = data_size + sizeof(FrameHdr_Data_t);
		param->msghdr.length = payload_size;
		payload_size += HIF_PROTO_TAIL_LEN;

		if (NULL == param->buffer) {
			paylaod_buf = host_os_malloc(payload_size);
			if (NULL == paylaod_buf) {
				HOST_LOG_ERR("DataPiece_Test(frame_idx%u): malloc(%u) Failed!\n",
					frame_idx, payload_size);
				return ;
			} else {
				HOST_LOG_INF("DataPiece_Test(frame_idx%u): malloc(%u) %p!\n",
					frame_idx, payload_size, paylaod_buf);
				param->buffer = paylaod_buf; 
			}
		} else {
			paylaod_buf = param->buffer;
		}

		/* set fragment header */
		frame_hdr = (FrameHdr_Data_t *)paylaod_buf;
		frame_hdr->frameLen   = (param->msghdr.msg_id == 0xC2 ? param->total_len : (param->total_len>>2));
		frame_hdr->dataOffset = (param->msghdr.msg_id == 0xC2 ? offset : (offset>>2));
		frame_hdr->frameIdx   = frame_idx;
	
#if (TEST_APP_RETRY)
		if ((frag_cnt & cnt_mask) == send_val) {
			/* set fragment data */
			for (int i = sizeof(FrameHdr_Data_t); i < data_size + sizeof(FrameHdr_Data_t); ++i) {
				paylaod_buf[i] = data_value++;
			}
			HOST_LOG_INF("DataPiece_Test(frame_idx%u): cnt=%u, off=%u buf=%p len=%u!\n",
					frame_idx, frag_cnt, offset, paylaod_buf, data_size);
			Mmw_Report_DataPiece_Process(param->msghdr.msg_id, paylaod_buf, param->msghdr.length, local);
		} else {
			data_value += data_size;
		}
#else
		/* set fragment data */
		HOST_LOG_INF("DataPiece_Test(frame_idx%u): cnt=%u, off=%u buf=%p len=%u!\n",
					 frame_idx, frag_cnt, offset, paylaod_buf, data_size);
		for (int i = sizeof(FrameHdr_Data_t); i < data_size + sizeof(FrameHdr_Data_t); ++i) {
			paylaod_buf[i] = data_value++;
		}
		Mmw_Report_DataPiece_Process(param->msghdr.msg_id, paylaod_buf, param->msghdr.length, local);
#endif
		offset += data_size;
		frag_cnt++;
	}
	
	/* send ACK request */
	if (param->app_retry_req) {
		FrameHdr_Data_t ack_request;
		ack_request.frameIdx = frame_idx;
		ack_request.frameLen = (param->msghdr.msg_id == 0xC2 ? param->total_len : (param->total_len>>2));
		ack_request.dataOffset = 0;
		HOST_LOG_INF("DataPiece_Test: Ack Request(frame_idx%u)!\n", frame_idx);
		Mmw_Report_DataPiece_Process(param->msghdr.msg_id, &ack_request, sizeof(ack_request), local);
	}
}

void APP_DataPiece_Test(DEV_HANDLE DevHdl, uint8_t msg_id, uint32_t total_len, uint32_t peice_size, uint32_t frame_num)
{
	Report_Local_t *local = NULL;
	HIF_HANDLE hif_hdl = Host_HIF_Handle_Get(DevHdl);
	HIF_Report_Handle_Get(hif_hdl, msg_id, NULL, (void **)&local);

	if (NULL != local) {
		uint32_t frame_idx  = 0;
		APP_PEICE_TEST_PARAM param;
		param.msghdr.msg_id = msg_id;
		param.msghdr.type   = HIF_MSG_TYPE_TO_HOST;
		param.msghdr.frag   = 1;
		param.total_len     = total_len;
		param.peice_size    = peice_size;
		param.buffer        = NULL;
		param.app_retry_req = 0; 

		while (frame_idx < frame_num) {
#if (TEST_APP_RETRY)
			HOST_LOG_INF("--------DataPiece_Test(frame%u): %02X Retry---------\n", frame_idx, msg_id);
			MMW_SendPeice_Test(local, &param, frame_idx, 3, 0);
			MMW_SendPeice_Test(local, &param, frame_idx, 3, 2);
			MMW_SendPeice_Test(local, &param, frame_idx, 3, 1);
			MMW_SendPeice_Test(local, &param, frame_idx, 3, 3);
#else
			HOST_LOG_INF("--------DataPiece_Test(frame%u): %02X ---------\n", frame_idx, msg_id);
			MMW_SendPeice_Test(local, &param, frame_idx, 0, 0);
#endif
			frame_idx++;
			host_os_delayms(10);
		}
		if (param.buffer != NULL) {
			host_os_free(param.buffer);
		}
	} else {
		HOST_LOG_ERR("ERR: DataPiece_Test Not Regist: %02X\n", msg_id);
	}

}
#endif

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */

