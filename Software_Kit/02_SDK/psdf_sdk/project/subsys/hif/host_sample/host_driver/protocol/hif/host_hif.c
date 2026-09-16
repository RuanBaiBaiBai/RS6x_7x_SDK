/**
 **************************************************************************************************
 * @file    host_hif.c
 * @brief   host hostinterface function file.
 * @attention
 *          Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../llc/host_llc.h"
#include "host_hif.h"
#include "host_hif_tl.h"

#define HIF_MSG_GROUP_REPORT_START              (HIF_MSG_ID_REPORT_START>>4)
#define HIF_MSG_GROUP_REPORT_NUM                (0x10 - HIF_MSG_GROUP_REPORT_START) /* 0xA0~0xFF, report */
#define HIF_MSG_GROUP_INDEX_CNT                 16

/* Local Variable.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_MSG_FRAGMENT_EN)
#define HIF_REPORT_FRAG_RETRY_IDX_INVALID       0xFFFF
#define HIF_REPORT_FRAG_FLOW_SEQ_INVALID        0xFF
typedef struct _fragment_t_ {
	uint32_t		total_len;
	uint32_t        data_offset;
	uint8_t         flow_seq;
#if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT == 1)
    uint8_t         abnormal_burst_num;
#endif
	uint8_t        *buffer;
	uint32_t        buffer_len;

	/* backup data for zero-copy of refragment */
	uint32_t        buf_offset;
	uint32_t        bkup_offset;
	fragment_hdr_t  hdr_backup;

#if (CFG_MSG_FRAGMENT_RETRY_EN)
	/* fragment retry */
	fragment_ack_t *ack_node;
	uint16_t        ack_idx_max;
	uint16_t        ack_idx_start;
	uint16_t        ack_idx_end;
	uint16_t        retry_idx;
#endif

	/* fragment flow list */
	struct _fragment_t_ *next;
	struct _fragment_t_ *prev;
} fragment_t;
#endif  /* CFG_MSG_FRAGMENT_EN */

#define HIF_REPORT_FLAG_USER_BUF                BIT(0)
#define HIF_REPORT_FLAG_CB_BUF_GET              BIT(1)
#define HIF_REPORT_FLAG_BUF_REUSE               BIT(2)
#define HIF_REPORT_FLAG_FRAG_BUF                BIT(3)
#define HIF_REPORT_FLAG_FRAG_RETRY              BIT(4)
#define HIF_REPORT_FLAG_TO_FREE                 BIT(7)

typedef struct {
	Msg_Report_CB msg_cb;
	void         *msg_arg;
	uint8_t      *buffer;
	uint32_t      buffer_len;
	uint32_t      hdl_ref_cnt;
	uint8_t       flag;
	uint8_t       msg_id;
#if (CFG_MSG_FRAGMENT_EN)
	uint8_t       last_flow_seq;
	uint8_t       reserved;
	fragment_t   *list_pending;
    fragment_t   *list_abnormal;
	fragment_t   *list_free;
    uint32_t      list_pending_len;
    uint32_t      list_abnormal_len;
    uint32_t      list_free_len;
#endif
} Hif_MsgReportHdl_t;

typedef struct {
	HifCfg_t   cfg;
	HifState_t state;
	HifCount_t count;
	LLC_HANDLE llc_hdl; /* link layer controller */
	TL_HANDLE  tl_hdl;  /* transport layer */

	uint8_t            wait_cmd_enable;
	uint8_t            wait_cmd_id;
	uint8_t           *cmd_resp_buf;
	uint32_t           cmd_resp_buf_len;
    uint32_t           cmd_resp_data_len;
	host_os_sem_t      wait_cmd_sem;
	host_os_timeout_t  wait_cmd_timeout;

	/* Handle and Buffer Regist */
	host_os_sem_t      msghdl_sem;
	Hif_MsgReportHdl_t *hif_msghdl[HIF_MSG_GROUP_REPORT_NUM];
} HifLocal_t;

static const HifCfg_t def_cfg;
/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
static Hif_MsgReportHdl_t * HIF_MSG_GetReportHdl(HifLocal_t *local, uint8_t msg_id)
{
	uint8_t msg_gp = (msg_id >> 4);
	if (msg_gp >= HIF_MSG_GROUP_REPORT_START) {
		uint8_t msg_idx = (msg_id & 0xF);
		msg_gp -= HIF_MSG_GROUP_REPORT_START;
		if (msg_gp < HIF_MSG_GROUP_REPORT_NUM && msg_idx < HIF_MSG_GROUP_INDEX_CNT) {
			host_os_sem_take(local->msghdl_sem, HOST_OS_TIMEOUT_FOREVER);
			if (local->hif_msghdl[msg_gp] != NULL) {
				Hif_MsgReportHdl_t * report_hdl = &local->hif_msghdl[msg_gp][msg_idx];
				if (report_hdl->msg_cb == NULL || (report_hdl->flag & HIF_REPORT_FLAG_TO_FREE)) {
					report_hdl = NULL;
				} else {
					report_hdl->hdl_ref_cnt++;
				}
                host_os_sem_give(local->msghdl_sem);
				return report_hdl;
			} else {
				HOST_LOG_DBG("%s:MsgID 0x%02X CallBack is not Set!\n", __func__, msg_id);
			}
			host_os_sem_give(local->msghdl_sem);
		} else {
			HOST_LOG_ERR("%s:Unsupport Report MsgID 0x%02X\n", __func__, msg_id);
		}
	} else {
		HOST_LOG_ERR("%s:Unexpected CmdID=0x%02X\n", __func__, msg_id);
	}
	return NULL;
}

static void HIF_MSG_PutReportHdl(HifLocal_t *local, Hif_MsgReportHdl_t *report_hdl)
{
	host_os_sem_take(local->msghdl_sem, HOST_OS_TIMEOUT_FOREVER);
	if (NULL != report_hdl && report_hdl->hdl_ref_cnt) {
		report_hdl->hdl_ref_cnt--;
	}
	host_os_sem_give(local->msghdl_sem);
}

#if (CFG_MSG_FRAGMENT_EN)
static void HIF_MSG_FragInfo_Put(Hif_MsgReportHdl_t *report_hdl, fragment_t *frag_info);

static fragment_t * HIF_MSG_FragInfo_Find(Hif_MsgReportHdl_t *report_hdl, uint8_t flow_seq)
{
	fragment_t *frag_info = report_hdl->list_pending;
	while (NULL != frag_info && frag_info->flow_seq != flow_seq) {
		frag_info = frag_info->next;
	}
	HOST_LOG_DBG("Fraginfo Find %p(flow%u)\n", frag_info, flow_seq);
	return frag_info;
}

static uint32_t HIF_MSG_FragInfo_Pending_List_Length(Hif_MsgReportHdl_t *report_hdl)
{
    if (NULL != report_hdl) {
        return report_hdl->list_pending_len;
    } else {
        return 0;
    }
}

static uint32_t HIF_MSG_FragInfo_Free_List_Length(Hif_MsgReportHdl_t *report_hdl)
{
    if (NULL != report_hdl) {
        return report_hdl->list_free_len;
    } else {
        return 0;
    }
}

static uint32_t HIF_MSG_FragInfo_Abnormal_List_Length(Hif_MsgReportHdl_t *report_hdl)
{
    if (NULL != report_hdl) {
        return report_hdl->list_abnormal_len;
    } else {
        return 0;
    }
}

#if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT == 1)
static void HIF_MSG_FragInfo_Pending_List_Abnormal_Check(
    Hif_MsgReportHdl_t *report_hdl, hif_msg_hdr_t *msg, fragment_hdr_t *frag_hdr
)
{
    if (NULL != report_hdl && NULL != msg) {
        host_os_critical_flag_t critical_flag = host_os_enter_critical();
        fragment_t *frag_info = report_hdl->list_pending;
        fragment_t *next_frag_info = frag_info;
        while (NULL != frag_info) {
            next_frag_info = frag_info->next;
            if (frag_info->data_offset < frag_info->total_len) {
                if ((!msg->frag || (msg->flag & BIT(0)))  // not frag or is ACK
                    || (NULL == frag_hdr || frag_info->flow_seq != frag_hdr->flow_seq)) {
                    frag_info->abnormal_burst_num += 1;
                } else if (NULL != frag_hdr && frag_info->flow_seq == frag_hdr->flow_seq) {
                    frag_info->abnormal_burst_num = 0;
                }
                if (frag_info->abnormal_burst_num >= CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR) {
                    /* move out pending */
                    if (NULL != frag_info->prev) {
                        frag_info->prev->next = frag_info->next;
                    } else {
                        report_hdl->list_pending = frag_info->next;
                    }
                    if (NULL != frag_info->next) {
                        frag_info->next->prev = frag_info->prev;
                    }
                    report_hdl->list_pending_len -= 1;
                    /* move to abnormal list */
                    frag_info->prev = NULL;
                    frag_info->next = report_hdl->list_abnormal;
                    if (NULL != report_hdl->list_abnormal) {
                        report_hdl->list_abnormal->prev = frag_info;
                    }
                    report_hdl->list_abnormal = frag_info;
                    report_hdl->list_abnormal_len += 1;
                }
            }
            frag_info = next_frag_info;
        }
        host_os_exit_critical(critical_flag);
    }
}

static void HIF_MSG_FragInfo_Abnormal_List_Clean(Hif_MsgReportHdl_t *report_hdl)
{
    if (NULL != report_hdl && HIF_MSG_FragInfo_Abnormal_List_Length(report_hdl) > 0) {
        host_os_critical_flag_t critical_flag = host_os_enter_critical();
        fragment_t *frag_info = report_hdl->list_abnormal;
        report_hdl->list_abnormal = NULL;
        report_hdl->list_abnormal_len = 0;
        host_os_exit_critical(critical_flag);
        fragment_t *next_frag_info = frag_info;
        while (NULL != frag_info) {
            next_frag_info = frag_info->next;
            ABNORMAL_FRAGMENT_INFO_LOG(report_hdl,
                                       frag_info,
                                       CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR);
            HIF_MSG_FragInfo_Put(report_hdl, frag_info);
            frag_info = next_frag_info;
        }
    }
}
#endif

static fragment_t * HIF_MSG_FragInfo_Alloc(Hif_MsgReportHdl_t *report_hdl)
{
    host_os_critical_flag_t crit_flag;
    fragment_t *info;

    crit_flag = host_os_enter_critical();
    info = report_hdl->list_free;
	if (NULL != info) {
		report_hdl->list_free = info->next;
		if (NULL != report_hdl->list_free) {
			report_hdl->list_free->prev = NULL;
		}
        report_hdl->list_free_len -= 1;
		host_os_exit_critical(crit_flag);
		info->data_offset = 0;
		info->buf_offset  = 0;
		info->bkup_offset = 0;
#if (CFG_MSG_FRAGMENT_RETRY_EN)
		info->ack_idx_start = 0;
		info->ack_idx_end   = 0;
#endif
#if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT == 1)
        info->abnormal_burst_num = 0;
#endif
		info->prev = NULL;
		info->next = NULL;
	} else {
        host_os_exit_critical(crit_flag);
		info = host_os_malloc(sizeof(fragment_t));
		if (NULL != info) {
			host_os_memset(info, 0, sizeof(fragment_t));
		}
	}
	HOST_LOG_DBG("Fraginfo Alloc %p\n", info);
	return info;
}

static void HIF_MSG_FragInfo_Put(Hif_MsgReportHdl_t *report_hdl, fragment_t *frag_info)
{
	if (NULL != frag_info) {
		/* reset info */
		if (frag_info->buffer == report_hdl->buffer) {
			if (report_hdl->flag & HIF_REPORT_FLAG_CB_BUF_GET) {
				if (!(report_hdl->flag & HIF_REPORT_FLAG_BUF_REUSE)) {
					host_os_free(report_hdl->buffer);
					report_hdl->buffer = NULL;
				}
				host_os_critical_flag_t crit_flag = host_os_enter_critical();
				report_hdl->flag &= ~(HIF_REPORT_FLAG_CB_BUF_GET|HIF_REPORT_FLAG_FRAG_BUF);
				host_os_exit_critical(crit_flag);
			}
			HOST_LOG_DBG("Frag(flow%u) buffer(%p) return to hdl\n", frag_info->flow_seq, frag_info->buffer);
		} else {
			/* Update common buffer if it's not in used. */
			host_os_critical_flag_t crit_flag = host_os_enter_critical();
			if ((report_hdl->flag & (HIF_REPORT_FLAG_CB_BUF_GET|HIF_REPORT_FLAG_BUF_REUSE|HIF_REPORT_FLAG_USER_BUF))
				== HIF_REPORT_FLAG_BUF_REUSE && frag_info->buffer_len > report_hdl->buffer_len
#if (CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX > 0)
                && frag_info->buffer_len <= CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX
#endif
				) {
				void *last_buffer = report_hdl->buffer;
				report_hdl->buffer = frag_info->buffer;
				report_hdl->buffer_len = frag_info->buffer_len;
				host_os_exit_critical(crit_flag);
				if (NULL != last_buffer) {
					host_os_free(last_buffer);
				}
				HOST_LOG_DBG("Frag(flow%u) buffer(%p) update to hdl\n",
					frag_info->flow_seq, frag_info->buffer);
			} else {
				host_os_exit_critical(crit_flag);
				host_os_free(frag_info->buffer);
				HOST_LOG_DBG("Frag(flow%u) buffer free %p hdl_flag=0x%X\n",
					frag_info->flow_seq, frag_info->buffer, report_hdl->flag);
			}
		}
		frag_info->buffer      = NULL;
		frag_info->buffer_len  = 0;

		host_os_critical_flag_t crit_flag = host_os_enter_critical();
//		/* move out pending or abnormal */
//		if (NULL != frag_info->prev) {
//			frag_info->prev->next = frag_info->next;
//		} else {
//            if (from_pending) {  // frag_info is from pending list
//				report_hdl->list_pending = frag_info->next;
//            } else {  // frag_info is from abnormal list
//                report_hdl->list_abnormal = frag_info->next;
//            }
//		}
//		if (NULL != frag_info->next) {
//			frag_info->next->prev = frag_info->prev;
//		}
//        if (from_pending) {
//            report_hdl->list_pending_len -= 1;
//        } else {
//            report_hdl->list_abnormal_len -= 1;
//        }

		/* move to free list*/
#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX >= 0)
		fragment_t *frag_info_to_free = NULL;
#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX > 0)
        if (HIF_MSG_FragInfo_Free_List_Length(report_hdl) < CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX) {
#endif
#endif
#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX != 0)
    		frag_info->prev = NULL;
    		frag_info->next = report_hdl->list_free;
    		if (NULL != report_hdl->list_free) {
    			report_hdl->list_free->prev = frag_info;
    		}
    		report_hdl->list_free = frag_info;
            report_hdl->list_free_len += 1;
#endif
#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX > 0)
        } else {
#endif
#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX >= 0)
            frag_info_to_free = frag_info;
#endif
#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX > 0)
        }
#endif
		host_os_exit_critical(crit_flag);
#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX >= 0)
		if (NULL != frag_info_to_free) {
#if (CFG_MSG_FRAGMENT_RETRY_EN)
            if (NULL != frag_info_to_free->ack_node) {
                host_os_free(frag_info_to_free->ack_node);
//                frag_info_to_free->ack_node      = NULL;
            }
#endif
			host_os_free(frag_info_to_free);
		}
#endif
		HOST_LOG_DBG("Fraginfo Put%s %p\n", ((NULL != frag_info_to_free) ? "(free)" : ""), frag_info);
	}
}

static void HIF_MSG_FragInfo_Free(fragment_t *frag_info)
{
	if (NULL != frag_info) {
		if (NULL != frag_info->buffer) {
			host_os_free(frag_info->buffer);
			frag_info->buffer = NULL;
		}
#if (CFG_MSG_FRAGMENT_RETRY_EN)
		if (NULL != frag_info->ack_node) {
			host_os_free(frag_info->ack_node);
			frag_info->ack_node = NULL;
			frag_info->ack_idx_max = 0;
		}
#endif
		host_os_free(frag_info);
	}
}

static fragment_t * HIF_MSG_FragInfo_Get(Hif_MsgReportHdl_t *report_hdl, hif_msg_hdr_t *msg, fragment_hdr_t *frag_hdr)
{
	if (NULL != report_hdl) {
		uint32_t buffer_len_need;
		fragment_t *frag_info = HIF_MSG_FragInfo_Find(report_hdl, frag_hdr->flow_seq);
		if (NULL != frag_info) {
			return frag_info;
		} else {
			/* malloc fragment info if not find */
			frag_info = HIF_MSG_FragInfo_Alloc(report_hdl);
			if (NULL != frag_info) {
				frag_info->total_len  = frag_hdr->total_len;
				frag_info->flow_seq   = frag_hdr->flow_seq;
#if (CFG_MSG_FRAGMENT_RETRY_EN)
				frag_info->retry_idx  = HIF_REPORT_FRAG_RETRY_IDX_INVALID;
#endif
			} else {
				HOST_LOG_ERR("FragInfo_%02X(flow%u) Info alloc(%d) Failed!\n",
					msg->msg_id, frag_hdr->flow_seq, sizeof(fragment_t));
				return NULL;
			}
		}

		/* Prepare fragment buffer if need */
		host_os_critical_flag_t crit_flag = host_os_enter_critical();
		buffer_len_need = frag_hdr->total_len + sizeof(fragment_hdr_t) + HIF_PROTO_TAIL_LEN;
		if (((uint8_t *)frag_hdr == report_hdl->buffer || (report_hdl->flag & HIF_REPORT_FLAG_CB_BUF_GET) == 0)
			&& buffer_len_need <= report_hdl->buffer_len) {
			report_hdl->flag |= (HIF_REPORT_FLAG_CB_BUF_GET|HIF_REPORT_FLAG_FRAG_BUF);
			frag_info->buffer = report_hdl->buffer;
			frag_info->buffer_len = report_hdl->buffer_len;
			host_os_exit_critical(crit_flag);
		} else {
			host_os_exit_critical(crit_flag);
			HOST_LOG_DBG("FragInfo_%02X(flow%u) malloc(%u) buffer, hdr=%p hdl buffer(%p) len=%u flag=0x%X!\n",
						msg->msg_id, frag_hdr->flow_seq, buffer_len_need, frag_hdr, report_hdl->buffer,
						report_hdl->buffer_len, report_hdl->flag);
			frag_info->buffer = host_os_malloc(buffer_len_need);
			if (NULL != frag_info->buffer) {
				frag_info->buffer_len = buffer_len_need;
			} else {
				HOST_LOG_ERR("FragInfo_%02X(flow%u) buffer alloc(%u) failed!\n",
							msg->msg_id, frag_hdr->flow_seq, buffer_len_need);
				HIF_MSG_FragInfo_Free(frag_info);
				return NULL;
			}
		}

#if (CFG_MSG_FRAGMENT_RETRY_EN)
		/* Prepare fragment ACK node if need */
		if (report_hdl->flag & HIF_REPORT_FLAG_FRAG_RETRY) {
			uint32_t payload_len  = msg->length - sizeof(fragment_hdr_t);
			uint32_t ack_info_max = ((frag_info->total_len + payload_len - 1)/payload_len + 1)>>1;
			fragment_ack_t *ack_node = frag_info->ack_node;

			if (ack_info_max > frag_info->ack_idx_max) {
				uint32_t ack_info_size = sizeof(fragment_ack_t) * ack_info_max;
				if (NULL != ack_node) {
					frag_info->ack_node    = NULL;
					frag_info->ack_idx_max = 0;
					host_os_free(ack_node);
				}
				ack_node = host_os_malloc(ack_info_size);
				if (NULL == ack_node) {
						HOST_LOG_ERR("FragInfo_%02X(flow%u) AckNode alloc(%u) Failed!\n",
									msg->msg_id, frag_info->flow_seq, ack_info_size);
					HIF_MSG_FragInfo_Free(frag_info);
					return NULL;
				}
				//host_os_memset(ack_node, 0, ack_info_size);
				frag_info->ack_idx_max = ack_info_max;
				frag_info->ack_node    = ack_node;
			}
			/* Init the first node */
			frag_info->ack_idx_start = 0;
			frag_info->ack_idx_end	 = 1;
			ack_node->blank_len = frag_info->total_len;
			ack_node->blank_off = 0;
			ack_node->flow_seq  = frag_info->flow_seq;
            ack_node->reserved  = 0;
		} else {
			fragment_ack_t *ack_node = frag_info->ack_node;
			if (NULL != ack_node) {
				frag_info->ack_node = NULL;
				host_os_free(ack_node);
			}
		}
#endif

		/* push to list head */
		crit_flag = host_os_enter_critical();
		if (0 == (report_hdl->flag & HIF_REPORT_FLAG_TO_FREE)) {
			if (NULL != report_hdl->list_pending) {
				report_hdl->list_pending->prev = frag_info;
				frag_info->next = report_hdl->list_pending;
			}
			report_hdl->list_pending = frag_info;
			report_hdl->last_flow_seq = frag_info->flow_seq;
            report_hdl->list_pending_len += 1;
		}
		host_os_exit_critical(crit_flag);

		HOST_LOG_DBG("New FragInfo_%02X(flow%u) total=%u, info(%d)=%p buffer=%p ack=%p(%d)\n",
					msg->msg_id, frag_hdr->flow_seq, frag_hdr->total_len,
					sizeof(fragment_t), frag_info, frag_info->buffer,
#if (CFG_MSG_FRAGMENT_RETRY_EN)
					frag_info->ack_node, frag_info->ack_idx_max
#else
					0, 0
#endif
		);
		return frag_info;
	}
	return NULL;
}

static void* HIF_MSG_Refrag_Buffer(HifLocal_t *local, Hif_MsgReportHdl_t *report_hdl, hif_msg_hdr_t *msg, uint32_t size)
{
	fragment_t *frag_info = report_hdl->list_pending;
	while (NULL != frag_info && NULL != frag_info->buffer) {
		uint32_t buf_offset = frag_info->buf_offset;
		if (buf_offset + size <= frag_info->buffer_len) {
			/* Backup fragment_hdr_t data */
			if (buf_offset > 0 && buf_offset != frag_info->bkup_offset) {
				frag_info->bkup_offset = buf_offset;
				host_os_memcpy(&frag_info->hdr_backup, &frag_info->buffer[buf_offset], sizeof(fragment_hdr_t));
			}
			HOST_LOG_DBG("FragBuff_%02X buf_off=%u msglen=%u\n", msg->msg_id, buf_offset, msg->length);
			return &frag_info->buffer[buf_offset];
		} else {
			frag_info = frag_info->next;
		}
	}
	HOST_LOG_DBG("FragBuff_%02X NULL! list=%p\n", msg->msg_id, report_hdl->list_pending);
	return NULL;
}

static void HIF_MSG_Refrag_PrevCB(hif_msg_hdr_t *msg, uint8_t *buffer, void *arg)
{
	HifLocal_t *local = (HifLocal_t *)arg;
	fragment_hdr_t *frag_hdr = (fragment_hdr_t *)buffer;

    Hif_MsgReportHdl_t *report_hdl = HIF_MSG_GetReportHdl(local, msg->msg_id);

#if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT == 1)
    if (msg->frag && !(msg->flag & BIT(0))) {
        HIF_MSG_FragInfo_Pending_List_Abnormal_Check(report_hdl, msg, (fragment_hdr_t *)buffer);
    } else {
        HIF_MSG_FragInfo_Pending_List_Abnormal_Check(report_hdl, msg, NULL);
    }
#if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT_TIMELY == 1)
    HIF_MSG_FragInfo_Abnormal_List_Clean(report_hdl);
#endif
#endif

	/* Handle Fragment Data only */
	if (!msg->frag || (msg->flag & BIT(0)) ||
		NULL == local || local->cfg.fragment_enable == 0) {
		HIF_MSG_PutReportHdl(local, report_hdl);
		return ;
	}

	if (msg->length > sizeof(fragment_hdr_t) && frag_hdr->total_len >= msg->length) {
		fragment_t *frag_info = HIF_MSG_FragInfo_Get(report_hdl, msg, frag_hdr);
		if (NULL == frag_info || frag_info->flow_seq != frag_hdr->flow_seq) {
			HOST_LOG_ERR("FragPrev_%02X(flow%u) total=%u, Get frag_info failed!\n",
						msg->msg_id, frag_hdr->flow_seq, frag_hdr->total_len);
			HIF_MSG_PutReportHdl(local, report_hdl);
			return ;
		}
#if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT == 1)
        frag_info->abnormal_burst_num = 0;
#endif
		uint32_t cur_offset   = frag_hdr->offset;
		uint32_t payload_len  = msg->length - sizeof(fragment_hdr_t);
		
		/* Update data state for no retry case */
		if (frag_info->data_offset == cur_offset) {
			frag_info->data_offset += payload_len;
		}
		/* Update buffer offset for following fragment */
		if (frag_info->buf_offset < cur_offset + payload_len) {
			frag_info->buf_offset = cur_offset + payload_len;
		}
		/* Move data to target position if need */
		fragment_hdr_t *frag_buffer = (fragment_hdr_t *)(frag_info->buffer + cur_offset);
		if (frag_hdr != frag_buffer && cur_offset + msg->length <= frag_info->buffer_len) {
			host_os_memcpy(frag_buffer + 1, frag_hdr + 1, payload_len);
			HOST_LOG_DBG("FragPrev_%02X(flow%u) Copy off=%u len=%u\n",
					msg->msg_id, frag_info->flow_seq, cur_offset, payload_len);
			//HOST_LOG_DBG_HEX(frag_hdr, ((payload_len > 16) ? 16 : payload_len), "hdr(%p)\n", frag_hdr);
			//HOST_LOG_DBG_HEX(frag_buffer, ((payload_len > 16) ? 16 : payload_len), "buff(%p)\n", frag_buffer);
		} else {
			HOST_LOG_DBG("FragPrev_%02X(flow%u) ZeroCopy(%p) off=%u len=%u\n",
					msg->msg_id, frag_info->flow_seq, frag_hdr, cur_offset, payload_len);
		}
		/* Restore backup data of refrag_buffer */
		frag_buffer = (fragment_hdr_t *)(frag_info->buffer + frag_info->bkup_offset);
		if (frag_hdr == frag_buffer) {
			host_os_memcpy(frag_hdr, &frag_info->hdr_backup, sizeof(fragment_hdr_t));
		}

#if (CFG_MSG_FRAGMENT_RETRY_EN)
		/* Update ACK node */
		if (NULL != frag_info->ack_node) {
			fragment_ack_t *node = frag_info->ack_node;
			for (int i = frag_info->ack_idx_start; i < frag_info->ack_idx_end; ++i) {
				uint32_t blank_off = node[i].blank_off;
				uint32_t blank_end = blank_off + node[i].blank_len;
				if (blank_end > cur_offset) { /* in the blank */
					if (blank_off == cur_offset) { /* at start */
						node[i].blank_off += payload_len;
						node[i].blank_len -= payload_len;
					} else if (blank_end == cur_offset + payload_len) { /* at end */
						node[i].blank_len -= payload_len;
					} else {
						/* in the middle, need add a node */
						if (frag_info->ack_idx_end < frag_info->ack_idx_max) {
							int j = frag_info->ack_idx_end++;
							int n = i + 1;
							while (j > n) {
								uint32_t *dst = (uint32_t *)&node[j];
								uint32_t *src = (uint32_t *)&node[j-1];
								*dst++ = *src++;
								*dst = *src;
								j--;
							}
							node[n].blank_off = cur_offset + payload_len;
							node[n].blank_len = blank_end - node[n].blank_off;
							node[n].flow_seq  = node[i].flow_seq;
							node[i].blank_len = cur_offset - blank_off;
							HOST_LOG_DBG("FragPrev_%02X(flow%u) Add a AckNode%d!(off=%u, len=%u)\n",
								msg->msg_id, node[i].flow_seq, n, node[n].blank_off, node[n].blank_len);
						} else {
							HOST_LOG_ERR("FragPrev_%02X(flow%u) curoff=%u No AckNode!(max=%u)\n",
								msg->msg_id, frag_hdr->flow_seq, cur_offset, frag_info->ack_idx_max);
							for (int j = frag_info->ack_idx_start; j < frag_info->ack_idx_end; ++j) {
								HOST_LOG_ERR("AckNode%d: off=%u, len=%u\n",
									j, node[j].blank_off, node[j].blank_len);
							}
						}
					}

					/* node is complete, remove it */
					if (node[i].blank_len == 0) {
						HOST_LOG_DBG("FragPrev_%02X(flow%u) Remove a AckNode%d!(off=%u, len=%u)\n",
								msg->msg_id, node[i].flow_seq, i, node[i].blank_off, node[i].blank_len);
						if (i == frag_info->ack_idx_start) {
							frag_info->ack_idx_start++;
						} else {
							frag_info->ack_idx_end--;
							if (i < frag_info->ack_idx_end) {
								uint32_t copy_size	= sizeof(fragment_ack_t);
								copy_size = copy_size * (frag_info->ack_idx_end - i);
								host_os_memcpy(&node[i], &node[i+1], copy_size);
							}
						}
					}
					HOST_LOG_DBG("FragPrev_%02X(flow%u) Update a AckNode%d!(off=%u, blanklen=%u) start=%d end=%d\n",
							msg->msg_id, frag_hdr->flow_seq, i, node[i].blank_off, node[i].blank_len,
							frag_info->ack_idx_start, frag_info->ack_idx_end);
					break;
				}
			}
		}
#endif

		HIF_MSG_PutReportHdl(local, report_hdl);
	}else {
		HIF_MSG_PutReportHdl(local, report_hdl);
		HOST_LOG_ERR("FragPrev_%02X(flow%u), total=%u, payload len=%u error!\n",
						msg->msg_id, frag_hdr->flow_seq, frag_hdr->total_len, msg->length);
	}
}

/* @return  0:refragment not handle this message, !0: refragment have handled this message */
static int HIF_MSG_Refrag_Process(HifLocal_t *local, Hif_MsgReportHdl_t *report_hdl, hif_msg_hdr_t *msg, uint8_t *buffer)
{
	fragment_t *frag_info;
#if (CFG_MSG_FRAGMENT_RETRY_EN)
	if ((msg->flag & BIT(0))) { /* AckRequest */
		fragment_ack_t *ack_data = NULL;
		uint32_t ack_size = sizeof(fragment_ack_t);
		fragment_hdr_t *frag_hdr = (fragment_hdr_t *)buffer;
		frag_info  = HIF_MSG_FragInfo_Find(report_hdl, frag_hdr->flow_seq);
		if (NULL != frag_info && frag_info->ack_idx_start < frag_info->ack_idx_end) {
			frag_info->retry_idx = frag_info->ack_idx_start;
			ack_size = ack_size * (frag_info->ack_idx_end - frag_info->ack_idx_start);
			ack_data = &frag_info->ack_node[frag_info->ack_idx_start];
		} else {
			uint32_t seq_gap = ((uint32_t)report_hdl->last_flow_seq + BIT(6) - frag_hdr->flow_seq) & (BIT(6)-1);
			ack_data = (fragment_ack_t *)buffer;
			if (seq_gap < BIT(6-1)) {
				/* fragment is completed */
				ack_data->blank_off = frag_hdr->total_len;
				ack_data->blank_len = 0;
			} else {
				/* new flow seq, receive nothing yet */
				ack_data->blank_off = 0;
			}
		}
		HIF_TL_PollAck_Set(local->tl_hdl, msg, ack_data, ack_size);
	}
#endif

    fragment_t *frag_complete = NULL;
    fragment_t *frag_next = NULL;
    host_os_critical_flag_t critical_flag = host_os_enter_critical();
	frag_info = report_hdl->list_pending;
    // oldest fragment need to be placed on the first, FIFO
	while (NULL != frag_info) {
		if (frag_info->data_offset == frag_info->total_len
#if (CFG_MSG_FRAGMENT_RETRY_EN)
//			|| (frag_info->retry_idx != HIF_REPORT_FRAG_RETRY_IDX_INVALID &&
			|| (
			    frag_info->ack_idx_start == frag_info->ack_idx_end)
#endif
		) {
            /* mark next */
            frag_next = frag_info->next;
            /* move out pending */
            if (NULL != frag_info->prev) {
                frag_info->prev->next = frag_info->next;
            } else {
                report_hdl->list_pending = frag_info->next;
            }
            if (NULL != frag_info->next) {
                frag_info->next->prev = frag_info->prev;
            }
            report_hdl->list_pending_len -= 1;
            /* move to frag_complete */
            if (NULL != frag_complete) {
                frag_info->next = frag_complete;
            } else {
                frag_info->next = NULL;
            }
            frag_complete = frag_info;
            /* set frag_info */
            frag_info = frag_next;
		} else {
			frag_info = frag_info->next;
		}
	}
    host_os_exit_critical(critical_flag);
    frag_info = frag_complete;
    while (NULL != frag_info) {
        frag_next = frag_info->next;
        report_hdl->msg_cb(msg->msg_id, frag_info->buffer + sizeof(fragment_hdr_t),
                    frag_info->total_len, report_hdl->msg_arg);
        HIF_MSG_FragInfo_Put(report_hdl, frag_info);
        frag_info = frag_next;
    }
	return 1;
}
#endif  /* CFG_MSG_FRAGMENT_EN */

static void HIF_MSG_Process_CB(hif_msg_hdr_t *msg, uint8_t *buffer, void *arg)
{
	HifLocal_t *local = (HifLocal_t *)arg;
	if (local != NULL) {
		if (local->wait_cmd_enable && msg->msg_id == local->wait_cmd_id) {
			uint32_t data_len = MIN(local->cmd_resp_buf_len, msg->length);
			if (local->cmd_resp_buf && data_len) {
				host_os_memcpy(local->cmd_resp_buf, buffer, data_len);
                local->cmd_resp_data_len = msg->length;
			}
			host_os_sem_give(local->wait_cmd_sem);
		} else {
			Hif_MsgReportHdl_t *report_hdl = HIF_MSG_GetReportHdl(local, msg->msg_id);
			if (report_hdl != NULL) {
				uint32_t payload_len = msg->length;
			#if (CFG_MSG_FRAGMENT_EN)
            #if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT == 1)
            #if (SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT_TIMELY == 0)
                HIF_MSG_FragInfo_Abnormal_List_Clean(report_hdl);
            #endif
            #endif
				if (msg->frag && HIF_MSG_Refrag_Process(local, report_hdl, msg, buffer)) {
				} else
			#endif
				report_hdl->msg_cb(msg->msg_id, buffer, payload_len, report_hdl->msg_arg);
			} else {
				HOST_LOG_WRN("%s:Unsupport Report=0x%02X len=%d\n",
							__func__, msg->msg_id, msg->length);
			}
			HIF_MSG_PutReportHdl(local, report_hdl);
		}
	}
}

static void* HIF_MSG_Buffer_Get_CB(hif_msg_hdr_t *msg, uint32_t size, void *arg)
{
	void *payload_buffer = NULL;
	HifLocal_t *local = (HifLocal_t *)arg;
	if (local != NULL) {
		Hif_MsgReportHdl_t *report_hdl = HIF_MSG_GetReportHdl(local, msg->msg_id);
		if (NULL != report_hdl) {
        #if (CFG_MSG_FRAGMENT_EN)
			if (local->cfg.fragment_enable && msg->frag && !(msg->flag & BIT(0))) {
				payload_buffer = HIF_MSG_Refrag_Buffer(local, report_hdl, msg, size);
				if (NULL != payload_buffer) {
					HOST_LOG_DBG("MsgBuff_%02X fragbuf=%p size=%u\n",
						msg->msg_id, report_hdl->buffer, size);
					HIF_MSG_PutReportHdl(local, report_hdl);
					return payload_buffer;
				}
			}
		#endif

			host_os_critical_flag_t crit_flag = host_os_enter_critical();
			if (size <= report_hdl->buffer_len && 
				!(report_hdl->flag & HIF_REPORT_FLAG_CB_BUF_GET)) { /* buffer not put*/
				report_hdl->flag |= HIF_REPORT_FLAG_CB_BUF_GET;
				payload_buffer = report_hdl->buffer;
			}
			host_os_exit_critical(crit_flag);
			HOST_LOG_DBG("MsgBuff_%02X req_size=%u commonbuf=%p len=%u flag=0x%X\n",
					msg->msg_id, size, payload_buffer, report_hdl->buffer_len, report_hdl->flag);
			HIF_MSG_PutReportHdl(local, report_hdl);
		}
	}
	return payload_buffer;
}

static void HIF_MSG_Buffer_Put_CB(hif_msg_hdr_t *msg, uint8_t *buffer, void *arg)
{
	HifLocal_t *local = (HifLocal_t *)arg;
	if (NULL != local && NULL != buffer) {
		Hif_MsgReportHdl_t *report_hdl = HIF_MSG_GetReportHdl(local, msg->msg_id);
		if (report_hdl != NULL && buffer == report_hdl->buffer) {
			if ((report_hdl->flag & (HIF_REPORT_FLAG_CB_BUF_GET|HIF_REPORT_FLAG_FRAG_BUF))
				== HIF_REPORT_FLAG_CB_BUF_GET) {
				if (!(report_hdl->flag & HIF_REPORT_FLAG_BUF_REUSE)) {
					report_hdl->buffer = NULL;
					host_os_free(buffer);
				}
				host_os_critical_flag_t crit_flag = host_os_enter_critical();
				report_hdl->flag &= ~HIF_REPORT_FLAG_CB_BUF_GET;
				host_os_exit_critical(crit_flag);
			}
		} else if (msg->frag == 0) {
			HOST_LOG_ERR("%s:Error Buffer(0x%p) Source!\n", __func__, buffer);
		}
		HIF_MSG_PutReportHdl(local, report_hdl);
	}
}

static void HIF_Report_Handle_Free(Hif_MsgReportHdl_t *report_hdl)
{
#if (CFG_MSG_FRAGMENT_EN)
	/* release all fragment */
	host_os_critical_flag_t crit_flag = host_os_enter_critical();
	fragment_t *list_pending = report_hdl->list_pending;
	fragment_t *list_abnormal = report_hdl->list_abnormal;
	fragment_t *list_free = report_hdl->list_free;
	report_hdl->list_pending = NULL;
	report_hdl->list_abnormal = NULL;
	report_hdl->list_free = NULL;
	report_hdl->list_pending_len = 0;
	report_hdl->list_abnormal_len = 0;
	report_hdl->list_free_len = 0;
	report_hdl->flag |= HIF_REPORT_FLAG_TO_FREE;
	host_os_exit_critical(crit_flag);

	while (NULL != list_pending) {
		fragment_t *next_frag = list_pending->next;
		HIF_MSG_FragInfo_Free(list_pending);
		list_pending = next_frag;
	}
	while (NULL != list_abnormal) {
		fragment_t *next_frag = list_abnormal->next;
		HIF_MSG_FragInfo_Free(list_abnormal);
		list_abnormal = next_frag;
	}
	while (NULL != list_free) {
		fragment_t *next_frag = list_free->next;
		HIF_MSG_FragInfo_Free(list_free);
		list_free = next_frag;
	}
#endif

	if (NULL != report_hdl->buffer) {
		if (!(report_hdl->flag & HIF_REPORT_FLAG_USER_BUF)) {
			host_os_free(report_hdl->buffer);
			report_hdl->buffer = NULL;
			report_hdl->buffer_len = 0;
		}
	}
	report_hdl->flag = 0;
	report_hdl->msg_cb  = NULL;
	report_hdl->msg_arg = NULL;
}

int HIF_Report_Handle_Regist(HIF_HANDLE hdl, uint8_t msg_id, Msg_Report_CB cb, void *arg)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	if (local != NULL) {
		uint8_t msg_gp	= (msg_id >> 4) - HIF_MSG_GROUP_REPORT_START;
		uint8_t msg_idx = (msg_id & 0xF);
		if (msg_gp >= HIF_MSG_GROUP_REPORT_NUM || msg_idx >= HIF_MSG_GROUP_INDEX_CNT) {
			HOST_LOG_ERR("%s:Invalid MsgID=0x%0x gp=%u idx=%u\n",
					__func__, msg_id, HIF_MSG_GROUP_REPORT_NUM, HIF_MSG_GROUP_INDEX_CNT);
			return HOST_ERRCODE_INVALID_PARAM;
		}
		HOST_LOG_DBG("Report Handle Regist 0x%02X: cb=%p arg=0x%p\n", msg_id, cb, arg);

		host_os_sem_take(local->msghdl_sem, HOST_OS_TIMEOUT_FOREVER);
		if (NULL != cb) {
			if (local->hif_msghdl[msg_gp] == NULL) {
				int group_size = sizeof(Hif_MsgReportHdl_t)*HIF_MSG_GROUP_INDEX_CNT;
				local->hif_msghdl[msg_gp] = (Hif_MsgReportHdl_t *)host_os_malloc(group_size);
				if (local->hif_msghdl[msg_gp] == NULL) {
					HOST_LOG_ERR("%s: MsgID=0x%0x malloc failed %d!\n",
								__func__, msg_id, group_size);
					host_os_sem_give(local->msghdl_sem);
					return HOST_ERRCODE_NO_BUFFER;
				}
				HOST_LOG_DBG("Report Handle Alloc gp=%01X addr=0x%p\n",
						(HIF_MSG_GROUP_REPORT_START + msg_gp), local->hif_msghdl[msg_gp]);
				host_os_memset(local->hif_msghdl[msg_gp], 0, group_size);
			}
			local->hif_msghdl[msg_gp][msg_idx].msg_id  = msg_id;
			local->hif_msghdl[msg_gp][msg_idx].msg_arg = arg;
			local->hif_msghdl[msg_gp][msg_idx].msg_cb  = cb;
			local->hif_msghdl[msg_gp][msg_idx].flag |= HIF_REPORT_FLAG_BUF_REUSE;
			local->hif_msghdl[msg_gp][msg_idx].hdl_ref_cnt = 0;
#if (CFG_MSG_FRAGMENT_RETRY_EN)
			if (local->cfg.fragment_retry_enable) {
				local->hif_msghdl[msg_gp][msg_idx].flag |= HIF_REPORT_FLAG_FRAG_RETRY;
			}
#endif
		} else {
			if (local->hif_msghdl[msg_gp] != NULL) {
				Hif_MsgReportHdl_t *report_hdl = &local->hif_msghdl[msg_gp][msg_idx];
				if (NULL != report_hdl->msg_cb) {
					host_os_critical_flag_t crit_flag = host_os_enter_critical();
					report_hdl->flag |= HIF_REPORT_FLAG_TO_FREE;
					host_os_exit_critical(crit_flag);
					/* wait reference clear */
					while (report_hdl->hdl_ref_cnt) {
						host_os_delayms(10);
					}
					/* if cb is NULL, free the report handle */
					HIF_Report_Handle_Free(report_hdl);
				}
			}
		}
		host_os_sem_give(local->msghdl_sem);
		return HOST_ERRCODE_SUCCESS;
	}
	return HOST_ERRCODE_INVALID_PARAM;
}

int HIF_Report_Handle_Get(HIF_HANDLE hdl, uint8_t msg_id, Msg_Report_CB *pCB, void **pArg)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	if (local != NULL) {
		Hif_MsgReportHdl_t *report_hdl = HIF_MSG_GetReportHdl(local, msg_id);
		if (NULL != report_hdl) {
			if (NULL != pCB) {
				*pCB = report_hdl->msg_cb;
			}
			if (NULL != pArg) {
				*pArg = report_hdl->msg_arg;
			}
			HIF_MSG_PutReportHdl(local, report_hdl);
			return HOST_ERRCODE_SUCCESS;
		}
	}
	return HOST_ERRCODE_INVALID_PARAM;
}

int HIF_Report_Buffer_Set(HIF_HANDLE hdl, uint8_t msg_id, void *buffer, uint32_t buffer_len)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	if (local != NULL) {
		uint8_t msg_gp	= (msg_id>>4) - HIF_MSG_GROUP_REPORT_START;
		uint8_t msg_idx = (msg_id & 0xF);
		if (msg_gp >= HIF_MSG_GROUP_REPORT_NUM || msg_idx >= HIF_MSG_GROUP_INDEX_CNT) {
			HOST_LOG_ERR("%s:Invalid MsgID=0x%0x gp=%u idx=%u\n",
						__func__, msg_id, msg_gp, msg_idx);
			return HOST_ERRCODE_INVALID_PARAM;
		}

		host_os_sem_take(local->msghdl_sem, HOST_OS_TIMEOUT_FOREVER);
		if (local->hif_msghdl[msg_gp] == NULL ||
			local->hif_msghdl[msg_gp][msg_idx].msg_cb == NULL) {
			HOST_LOG_ERR("%s:MsgID 0x%02X Callback is NULL!\n", __func__, msg_id);
 			host_os_sem_give(local->msghdl_sem);
			return HOST_ERRCODE_INVALID_PARAM;
		}

		Hif_MsgReportHdl_t *report_hdl = &local->hif_msghdl[msg_gp][msg_idx];
		if (!(report_hdl->flag & HIF_REPORT_FLAG_CB_BUF_GET)) {
			host_os_critical_flag_t crit_flag = host_os_enter_critical();
			report_hdl->buffer = buffer;
			report_hdl->buffer_len = buffer_len;
			report_hdl->flag |= HIF_REPORT_FLAG_USER_BUF|HIF_REPORT_FLAG_BUF_REUSE;
			host_os_exit_critical(crit_flag);
			HOST_LOG_DBG("Report Buffer Regist 0x%02X: buf=0x%p len=%d\n", msg_id, buffer, buffer_len);
			host_os_sem_give(local->msghdl_sem);
			return HOST_ERRCODE_SUCCESS;
		} else {
			HOST_LOG_ERR("Report Buffer is busy! 0x%02X: buf=0x%p len=%d\n",
						msg_id, report_hdl->buffer, report_hdl->buffer_len);
		}
		host_os_sem_give(local->msghdl_sem);
	}
	return HOST_ERRCODE_INVALID_PARAM;
}

int HIF_Encrypt_Regist(HIF_HANDLE hdl, uint32_t enc_cfg, Crypter enc, Crypter dec)
{
	return HOST_ERRCODE_UNSUPPORT;
}


int HIF_Cmd_Request(HIF_HANDLE hdl, uint32_t msg_id, void *param,
					uint32_t param_len, void *resp_buff, uint32_t buffer_len, uint32_t *data_len)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	if (local != NULL) {
		int ret;
		hif_msg_hdr_t msg;
		msg.type = HIF_MSG_TYPE_TO_DEVICE;
		msg.flag = BIT(0)|BIT(2);
		msg.msg_id = msg_id;
		msg.length = param_len;

        if (data_len != NULL) {
            *data_len = 0;
        }

		local->cmd_resp_buf      = resp_buff;
		local->cmd_resp_buf_len  = buffer_len;
        local->cmd_resp_data_len = 0;
		local->wait_cmd_id       = msg_id;
		local->wait_cmd_enable   = true;
		ret = HIF_TL_SendMsg(local->tl_hdl, &msg, param);
		if (ret == HOST_ERRCODE_SUCCESS) {
			ret = host_os_sem_take(local->wait_cmd_sem, local->wait_cmd_timeout);
			if (ret != HOST_ERRCODE_SUCCESS) {
                HIF_TL_WaitCmdResp_Abort(local->tl_hdl);
				HOST_LOG_ERR("%s:Wait CmdResp=0x%02x Failed %d!\n", __func__, msg_id, ret);
			} else if (data_len != NULL) {
                *data_len = local->cmd_resp_data_len;
            }
		} else {
			HOST_LOG_ERR("%s:Send Cmd0x%02x len=%d Failed %d!\n",
						__func__, msg_id, param_len, ret);
		}
		local->wait_cmd_enable = false;
		local->wait_cmd_id  = 0xFF;
		local->cmd_resp_buf = NULL;
		return ret;
	}
	return HOST_ERRCODE_INVALID_PARAM;
}

#if (CFG_APP_RETRY_EN)
int HIF_Send_ReportAck(HIF_HANDLE hdl, uint32_t msg_id, void *ack, uint32_t ack_len)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	if (local != NULL && NULL != local->tl_hdl) {
		hif_msg_hdr_t msg = { 0 };
		msg.type   = HIF_MSG_TYPE_TO_DEVICE;
		msg.msg_id = (uint8_t)msg_id;
		msg.length = ack_len;
		return HIF_TL_PollAck_Set(local->tl_hdl, &msg, ack, ack_len);
	}
	return HOST_ERRCODE_INVALID_PARAM;
}
#endif



int HIF_Fragment_State_Get(
    HIF_HANDLE hdl, uint32_t msg_id,
    uint32_t *hold_buffer_len,
    uint32_t *pending_len, uint32_t *abnormal_len, uint32_t *free_len
)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
    if (NULL != local) {
        Hif_MsgReportHdl_t *report_hdl = HIF_MSG_GetReportHdl(local, msg_id);
        if (NULL != report_hdl) {
            if (NULL != hold_buffer_len) {
                *hold_buffer_len = report_hdl->buffer_len;
            }
#if (CFG_MSG_FRAGMENT_EN)
            if (NULL != pending_len) {
                *pending_len = HIF_MSG_FragInfo_Pending_List_Length(report_hdl);
            }
            if (NULL != abnormal_len) {
                *abnormal_len = HIF_MSG_FragInfo_Abnormal_List_Length(report_hdl);
            }
            if (NULL != free_len) {
                *free_len = HIF_MSG_FragInfo_Free_List_Length(report_hdl);
            }
#else
            if (NULL != pending_len) {
                *pending_len = 0;
            }
            if (NULL != abnormal_len) {
                *abnormal_len = 0;
            }
            if (NULL != free_len) {
                *free_len = 0;
            }
#endif
            HIF_MSG_PutReportHdl(local, report_hdl);
            return HOST_ERRCODE_SUCCESS;
        } else {
            return HOST_ERRCODE_INVALID_PARAM;
        }
    }
    return HOST_ERRCODE_INVALID_HANDLE;
}

HifState_t* HIF_State_Get(HIF_HANDLE hdl)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	return &local->state;
}

HifCount_t* HIF_Counters_Get(HIF_HANDLE hdl)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	return &local->count;
}

HifCfg_t* HIF_Config_Get(HIF_HANDLE hdl)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	return &local->cfg;
}

HifCfg_t* HIF_Config_Confirm(HIF_HANDLE hdl)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	HIF_TL_Disable_DevSleep(local->tl_hdl, 1);
//#if (CFG_TL_RETRY_EN)
//	if (local->cfg.tl_retry_enable) {
//		uint8_t status = 0xFF;
//		struct {
//			uint8_t  subid;
//			uint8_t  host_ack;
//			uint8_t  host_ack_mode;
//			uint8_t  device_ack;
//			uint8_t  device_ack_mode;
//		} param = {0x7, 0x1, 0x1, 0x0, 0x0};
//		HIF_Cmd_Request(hdl, HIF_MSG_ID_HIF_MODE_CFG, &param, sizeof(param), &status, 1, NULL);
//		if (status != HIF_CMD_STATUS_SUCCESS) {
//			local->cfg.tl_retry_enable = 0;
//		}
//	}
//#endif
//	if (local->cfg.fragment_enable) {
//		uint8_t status[2] = {0xFF, 0x0};
//		struct {
//			uint8_t  subid;
//			uint8_t  frag_mode;
//			uint16_t frag_thresh;
//			uint32_t max_total_size;
//		} frag_param = {0x1, 0x1, 3840, 0xFFFFFFFF};
//		frag_param.frag_mode = local->cfg.fragment_retry_enable ? 0x2 : 0x1;
//		HIF_Cmd_Request(hdl, HIF_MSG_ID_HIF_MODE_CFG, &frag_param, sizeof(frag_param), &status, 1, NULL);
//		if (status[0] != HIF_CMD_STATUS_SUCCESS) {
//			local->cfg.fragment_enable = 0;
//		} else {
//			local->cfg.fragment_retry_enable = (status[1] == 0x2);
//		}
//	}
	HIF_TL_Disable_DevSleep(local->tl_hdl, 0);
	return &local->cfg;
}

static void HIF_MSG_Buffers_Release(HifLocal_t *local)
{
	host_os_sem_take(local->msghdl_sem, HOST_OS_TIMEOUT_FOREVER);
	for (int msg_gp = 0; msg_gp < HIF_MSG_GROUP_REPORT_NUM; ++msg_gp) {
		Hif_MsgReportHdl_t *report_group = local->hif_msghdl[msg_gp];
		if (report_group == NULL) {
			continue;
		}
		local->hif_msghdl[msg_gp] = NULL;

		for (int idx = 0; idx < HIF_MSG_GROUP_INDEX_CNT; ++idx) {
			HIF_Report_Handle_Free(&report_group[idx]);
		}
		host_os_free(report_group);
	}
	host_os_sem_give(local->msghdl_sem);
}

HIF_HANDLE HIF_Entity_Init(HifCfg_t *user_cfg, LLC_HANDLE llc_hdl)
{
	HifLocal_t *local = NULL;
	if (llc_hdl != NULL) {
		local = (HifLocal_t *)host_os_malloc(sizeof(HifLocal_t));
		if (local != NULL) {
			host_os_memset(local, 0, sizeof(HifLocal_t));
			host_os_memcpy(&local->cfg, (user_cfg == NULL ? &def_cfg : user_cfg), sizeof(HifCfg_t));
			local->llc_hdl = llc_hdl;
			local->tl_hdl  = HIF_TL_Init(&local->cfg, llc_hdl);
			if (local->tl_hdl == NULL) {
				goto hif_init_err;
			}
		#if (CFG_MSG_FRAGMENT_EN)
			HIF_TL_MsgHandle_Regist(local->tl_hdl, &HIF_MSG_Process_CB, HIF_MSG_Refrag_PrevCB,
								&HIF_MSG_Buffer_Get_CB, &HIF_MSG_Buffer_Put_CB, local);
		#else
			HIF_TL_MsgHandle_Regist(local->tl_hdl, &HIF_MSG_Process_CB, NULL,
								&HIF_MSG_Buffer_Get_CB, &HIF_MSG_Buffer_Put_CB, local);
		#endif
			local->wait_cmd_sem = host_os_sem_create(0, 1);
			if (!host_os_sem_is_valid(local->wait_cmd_sem)) {
				HOST_LOG_ERR("%s:cmd_sem Invalid!\n", __func__);
				goto hif_init_err;
			}
			local->msghdl_sem = host_os_sem_create(1, 1);
			if (!host_os_sem_is_valid(local->msghdl_sem)) {
				HOST_LOG_ERR("%s:cmd_sem Invalid!\n", __func__);
				goto hif_init_err;
			}

			local->wait_cmd_timeout = 5000; //5s timeout
		}
		HOST_LOG_DBG("%s: suceess(0x%p)!\n", __func__, local);
		return local;
	}
	
hif_init_err:
	if (local) {
		if (host_os_sem_is_valid(local->wait_cmd_sem)) {
			host_os_sem_delete(local->wait_cmd_sem);
		}
		if (host_os_sem_is_valid(local->msghdl_sem)) {
			host_os_sem_delete(local->msghdl_sem);
		}
		if (local->tl_hdl) {
			HIF_TL_DeInit(local->tl_hdl);
			local->tl_hdl = NULL;
		}
		local->llc_hdl = NULL;
		host_os_free(local);
	}
	return NULL;

}

void HIF_Entity_DeInit(HIF_HANDLE hdl)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	if (local != NULL && local->tl_hdl != NULL) {
		if (host_os_sem_is_valid(local->wait_cmd_sem)) {
			host_os_sem_delete(local->wait_cmd_sem);
		}
		if (host_os_sem_is_valid(local->msghdl_sem)) {
			host_os_sem_delete(local->msghdl_sem);
		}
		HIF_TL_DeInit(local->tl_hdl);
		local->tl_hdl = NULL;
		local->llc_hdl = NULL;
		HIF_MSG_Buffers_Release(local);
		host_os_free(local);
	}
}

void HIF_Entity_DevReset(HIF_HANDLE hdl)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	if (local != NULL && local->tl_hdl != NULL) {
		HIF_TL_DevReset(local->tl_hdl);
		if (local->wait_cmd_enable) {
			host_os_sem_give(local->wait_cmd_sem);
		}
		host_os_memset(&local->state, 0, sizeof(HifState_t));
		host_os_memset(&local->count, 0, sizeof(HifCount_t));
	}
}

int HIF_Entity_DevSleep_Disable(HIF_HANDLE hdl, bool disable)
{
    HifLocal_t *local = (HifLocal_t *)hdl;
    if (local != NULL && local->tl_hdl != NULL) {
        return HIF_TL_Disable_DevSleep(local->tl_hdl, disable);
    }
    return HOST_ERRCODE_INVALID_PARAM;
}

#if 0

#define TEST_FRAG_RETRY    0
typedef struct frag_test_param_t {
	hif_msg_hdr_t msghdr;
	uint32_t total_len;
	uint32_t frag_size;
	uint8_t *buffer;
} FRAG_TEST_PARAM;

void HIF_MSG_SendFrag_Test(HifLocal_t *local, FRAG_TEST_PARAM *param,
			uint32_t flow_seq, uint32_t cnt_mask, uint32_t send_val)
{
	uint8_t  *paylaod_buf;
	uint32_t  offset = 0;
	uint32_t  data_value = 0;
	uint32_t  frag_cnt   = 0;

	/* send fragments */
	param->msghdr.flag = BIT(2);
	while (offset < param->total_len) {
		fragment_hdr_t *frag_hdr;
		uint32_t data_size = MIN(param->frag_size, param->total_len - offset);
		uint32_t payload_size = data_size + sizeof(fragment_hdr_t);
		param->msghdr.length = payload_size;
		payload_size += HIF_PROTO_TAIL_LEN;
		
		paylaod_buf = HIF_MSG_Buffer_Get_CB(&param->msghdr, payload_size, local);
		if (NULL == paylaod_buf) {
			if (NULL == param->buffer) {
				paylaod_buf = host_os_malloc(payload_size);
				if (NULL == paylaod_buf) {
					HOST_LOG_ERR("Refrag_Test(flow%u): malloc(%u) Failed!\n",
						flow_seq, payload_size);
					return ;
				} else {
					HOST_LOG_INF("Refrag_Test(flow%u): malloc(%u) %p!\n",
						flow_seq, payload_size, paylaod_buf);
					param->buffer = paylaod_buf; 
				}
			} else {
				paylaod_buf = param->buffer;
			}
		}
	
		/* set fragment header */
		frag_hdr = (fragment_hdr_t *)paylaod_buf;
		frag_hdr->total_len = param->total_len;
		frag_hdr->offset = offset;
		frag_hdr->flow_seq = (flow_seq & 0x3F);
	
#if (TEST_FRAG_RETRY)
		if ((frag_cnt & cnt_mask) == send_val) {
			/* set fragment data */
			for (int i = sizeof(fragment_hdr_t); i < data_size + sizeof(fragment_hdr_t); ++i) {
				paylaod_buf[i] = data_value++;
			}
			HOST_LOG_INF("Refrag_Test(flow%u): cnt=%u, off=%u buf=%p len=%u!\n",
					flow_seq, frag_cnt, offset, paylaod_buf, data_size);
			HIF_MSG_Refrag_PrevCB(&param->msghdr, paylaod_buf, local);
			HIF_MSG_Process_CB(&param->msghdr, paylaod_buf, local);
		} else {
			data_value += data_size;
		}
#else
		/* set fragment data */
		HOST_LOG_INF("Refrag_Test(flow%u): cnt=%u, off=%u buf=%p len=%u!\n",
					 flow_seq, frag_cnt, offset, paylaod_buf, data_size);
		for (int i = sizeof(fragment_hdr_t); i < data_size + sizeof(fragment_hdr_t); ++i) {
			paylaod_buf[i] = data_value++;
		}
		HIF_MSG_Refrag_PrevCB(&param->msghdr, paylaod_buf, local);
		HIF_MSG_Process_CB(&param->msghdr, paylaod_buf, local);
#endif
	
		HIF_MSG_Buffer_Put_CB(&param->msghdr, paylaod_buf, local);
		offset += data_size;
		frag_cnt++;
	}
	
	/* send ACK request */
	if (local->cfg.fragment_retry_enable) {
		fragment_hdr_t ack_request;
		param->msghdr.flag |= BIT(0);
		ack_request.total_len = param->total_len;
		ack_request.offset	  = 0;
		ack_request.flow_seq = flow_seq;
		HOST_LOG_INF("Refrag_Test: Ack Request(flow%u)!\n", flow_seq);
		HIF_MSG_Refrag_PrevCB(&param->msghdr, (uint8_t *)&ack_request, local);
		HIF_MSG_Process_CB(&param->msghdr, (uint8_t *)&ack_request, local);
	}
}

void HIF_MSG_Refrag_Test(HIF_HANDLE hdl, uint8_t msg_id, uint32_t total_len, uint32_t frag_size, uint32_t frame_num)
{
	HifLocal_t *local = (HifLocal_t *)hdl;
	uint32_t flow_seq  = 0;
	FRAG_TEST_PARAM param;
	param.msghdr.msg_id = msg_id;
	param.msghdr.type   = HIF_MSG_TYPE_TO_HOST;
	param.msghdr.frag   = 1;
	param.total_len     = total_len;
	param.frag_size     = frag_size;
	param.buffer        = NULL;

	while (flow_seq < frame_num) {
	#if (TEST_FRAG_RETRY)
		HOST_LOG_INF("--------Refrag_Test(flow%u): %02X Retry---------\n", flow_seq, msg_id);
		HIF_MSG_SendFrag_Test(local, &param, flow_seq, 3, 0);
		HIF_MSG_SendFrag_Test(local, &param, flow_seq, 3, 2);
		HIF_MSG_SendFrag_Test(local, &param, flow_seq, 3, 1);
		HIF_MSG_SendFrag_Test(local, &param, flow_seq, 3, 3);
	#else
		HOST_LOG_INF("--------Refrag_Test(flow%u): %02X ---------\n", flow_seq, msg_id);
		HIF_MSG_SendFrag_Test(local, &param, flow_seq, 0, 0);
	#endif
		flow_seq++;
		host_os_delayms(10);
	}
	if (param.buffer != NULL) {
		host_os_free(param.buffer);
	}
}
#endif

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
