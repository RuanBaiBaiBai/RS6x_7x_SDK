/**
 **************************************************************************************************
 * @file    host_hif_tl.c
 * @brief   Implementation of HIF Transport Layer.
 * @attention
 *        Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes. */
#include "../llc/host_llc.h"
#include "host_hif_tl.h"

/* Config */
#define CPU_ADDR_UNALIGN_SUPPORT        1
#define HIF_TL_REPORT_PROC_THREAD_EN    1    /* enable report message process thread */
#ifndef HIF_TL_REPORT_QUEUE_MAX_DEF
#define HIF_TL_REPORT_QUEUE_MAX_DEF     16     /* default max num of report queue */
#endif
#ifndef HIF_TL_REPORT_TASK_STACK_SIZE
#define HIF_TL_REPORT_TASK_STACK_SIZE   2048
#endif

#define HIF_TL_BURST_NUM_MAX_DEF        256    /* default max burst num of rx message */
#if (HIF_TL_REPORT_PROC_THREAD_EN)
#define HIF_TL_RECV_QUEUE_MAX_DEF       4      /* default max num of receive queue */
#else
#define HIF_TL_RECV_QUEUE_MAX_DEF       32     /* default max num of receive queue */
#endif

#ifndef HIF_TL_TASK_STACK_SIZE
#define HIF_TL_TASK_STACK_SIZE          1024
#endif
#define HIF_TL_CMD_BUF_LEN              1024 /* command buffer for config and response */
#define HIF_TL_POLL_BUF_LEN             512  /* poll buffer for message tlv ack */


#if !IS_POW2(HIF_TL_REPORT_QUEUE_MAX_DEF)
#error "HIF_TL_REPORT_QUEUE_MAX_DEF is not pow of 2!"
#endif

/* Types.
 * ------------------------------------------------------------------------------------------------
 */
#define HIF_TL_BUS_TIMEOUT_MS            1000  /* default timeout of bus take */
#define HIF_TL_CMD_TIMEOUT_MS            1000  /* default timeout of cmd response */
#define HIF_TL_POLL_TIMEOUT_MS           100   /* default timeout of poll cmd */
#define HIF_TL_WAKEUP_TIMEOUT_MS         20    /* default timeout of wake up device */
#define HIF_TL_RETRY_MAX_BURST_NUM       8    /* !!!Cannot change. Max burst num when TL retry enable. */
#define HIF_TL_RX_SPEC_BUF_LEN           256  /* rx buffer for sepcial case */
#define HIF_TL_LOCAL_CMD_BUF_LEN         12   /* rx buffer for sepcial cmd response */

#define HIF_TL_LOCAL_ERR_RETRY_NUM       3    /* retry times when error in tl task */


#define HIF_TL_RX_HDR_BUF_OFF           (ROUND_UP4(HIF_PROTO_HEAD_LEN) - HIF_PROTO_HEAD_LEN)

#if !IS_POW2(HIF_TL_RETRY_MAX_BURST_NUM)
#error "HIF_TL_RETRY_MAX_BURST_NUM is not pow of 2!"
#endif
#if (HIF_TL_RX_SPEC_BUF_LEN < ROUND_UP4(HIF_PROTO_HEAD_LEN))
#error "HIF_TL_SPECIAL_BUF_LEN is too small!"
#endif

#define HIF_TL_TRAN_STATE_IDLE           0
#define HIF_TL_TRAN_STATE_WAIT_RX        1
#define HIF_TL_TRAN_STATE_RX             2

#define HIF_TL_DEV_STATE_SLEEP           0
#define HIF_TL_DEV_STATE_WAIT_ACTIVE     1
#define HIF_TL_DEV_STATE_ACTIVE          2

typedef union {
	hif_msg_hdr_t hdr;
	uint32_t      val32;
} hif_msg_hdr_u;


#define TL_MSG_BUF_SRC_NONE              0
#define TL_MSG_BUF_SRC_LLC               1  /* Do nothing  */
#define TL_MSG_BUF_SRC_TL_FIX            2  /* Do nothing  */
#define TL_MSG_BUF_SRC_TL_ALLOC          3  /* Released by TL os_free */
#define TL_MSG_BUF_SRC_MSG               4  /* Released by MSG Layer throught msg_cb */

#define TL_MSG_PROC_TYPE_NONE            0
#define TL_MSG_PROC_TYPE_REPORT          1
#define TL_MSG_PROC_TYPE_LOCAL           2
typedef struct {
	hif_msg_hdr_u u;
	void    *payload;
	uint8_t  payload_src;
	uint8_t  proc_type;
} msg_item_t;

typedef struct {
	/* From upper layer */
	HifCfg_t               *hif_cfg;
	LLC_HANDLE              llc_hdl;
	MSG_PROC_CB             msgprev_cb;
	MSG_PROC_CB             msgproc_cb;
	MSG_BUFFER_GET_CB       msgbuf_get;
	MSG_BUFFER_PUT_CB       msgbuf_put;
	void                   *msg_cb_arg;
	/* Device param */
	uint32_t                tx_rx_delay_us;
	uint32_t                rx_tx_delay_us;
	uint32_t                rx_rx_delay_us;
	host_os_timeout_t       data_max_delay;
	host_os_timeout_t       polling_time;
	host_os_timeout_t       llc_rx_timeout;
	/* Device state manage */
	uint8_t                 rx_polling;
	uint8_t                 dev_sleep_disable;
	uint8_t                 dev_state;
	uint8_t                 data_ready;
	uint8_t                 tx_seq;
	uint32_t                burst_num;

	/* Command send and receive */
	host_os_sem_t           cmd_sem;
	uint8_t                *cmd_buf;
	uint16_t                cmd_len;  /* command data len */
	uint8_t                 cmd_waitresp;

	uint8_t 				poll_enable;
	host_os_sem_t           poll_sem;
	uint8_t                *poll_buf;
	uint16_t                poll_len;

	/* Receive buffer for LLC  */
	uint32_t               *spec_buf;
	uint32_t               *tl_cmdresp;
	uint8_t                 recv_hrd_valid;
	uint8_t                 recv_buf_src;
	uint16_t                recv_buf_len;
	uint8_t                *recv_buf;

	/* Report message queue */
	uint8_t                 burst_first;
	uint8_t                 msg_less_buffer;
	uint8_t                 msg_report;
	uint8_t                 msg_queue_mask;
	uint16_t                msg_queue_get;
	uint16_t                msg_queue_put;
	msg_item_t             *msg_order_queue;

#if (HIF_TL_REPORT_PROC_THREAD_EN)
	volatile int            msg_proc_get;
	volatile int            msg_proc_put;
	msg_item_t             *msg_proc_queue;
	uint8_t                 msg_proc_wait;
	host_os_sem_t           proc_finish_sem;
	host_os_sem_t           proc_task_sem;
	host_os_thread_handle_t proc_task_hdl;
#endif

#if (CFG_TL_RETRY_EN)
	/*** Retransmit of transport layer ****/
	uint8_t                 tl_retry_enable;
	uint8_t                 tl_need_ack;
	uint16_t                tl_ack_seq_win;
	uint8_t                 tl_retry_flush;
#endif

#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
	/*** Ack info from msg layer ***/
	uint8_t                 msg_ack_tlv_num;
	uint16_t                msg_ack_len;
#endif

	host_os_sem_t           task_sem;
	host_os_thread_handle_t task_hdl;
} Hif_TL_Local_t;


#define HOST_POLL_TYPE_NONE            0
#define HOST_POLL_TYPE_TL_ACK          1
#define HOST_POLL_TYPE_ACK_CUBE        2
#define HOST_POLL_TYPE_ACK_TLV         3
typedef struct {
	uint8_t  poll_type;
	uint8_t  ack_err_cnt;  //unit=byte
	uint16_t burst_num;
	uint8_t  tl_err_seq[0];
} poll_ack_t;

typedef struct {
	uint8_t  poll_type;
	uint8_t  cube_err_num;  //unit=8byte
	uint16_t burst_num;
	uint8_t  cube_data[0];
} poll_ack_cube_t;

#define HOST_POLL_TLV_ACK_MSG          0
#define HOST_POLL_TLV_ACK_MSG_FRAG     1
#define HOST_POLL_TLV_ACK_TL           2
typedef struct {
	uint8_t  msg_id;
	uint8_t  type;
	uint16_t length;
	uint8_t  data[0];
} msg_tlv_ack_t;

typedef struct {
	uint8_t  poll_type;
	uint8_t  tlv_num;  //unit=8byte
	uint16_t burst_num;
	uint8_t  tlv_data[0];
} poll_ack_tlv_t;

const uint8_t poll_template[] = {0xA5,0x0,0x15,0xC,0x0,0x0};

/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
static uint8_t HIF_TL_CheckSum8(uint8_t initValue, uint8_t* data, int dataLen)
{
    register uint8_t checksum8 = initValue;
    while (dataLen--) {
        checksum8 += *data++;
    }
    return checksum8;
}

static uint32_t HIF_TL_Checksum32(uint8_t *data, uint32_t len, uint32_t *offset)
{
    register uint32_t result = 0;
    register uint32_t off = *offset & 0x1F;
    register uint32_t dword_len;

#if (CPU_ADDR_UNALIGN_SUPPORT)
    /* offset is not aligned4 */
    while (off && len) {
        result += ((uint32_t)(*data++)) << off;
        off = (off + 8) & 0x1F;
        len--;
    }

#else
    /* address or offset is not aligned4 */
    while (((data & 0x3) | off) && len) {
        result += (*data++) << off;
        off = (off + 8) & 0x1F;
        len--;
    }
#endif

    /* address and len is aligned4 */
    dword_len = (len >> 2);
    while (dword_len--) {
        result += *(uint32_t *)data;
        data += 4;
    }

    /* len not aligned4 */
    len = (len & 0x3);
    if (len) {
        result += *(uint32_t *)data & ((0x1U<<(len<<3)) - 1);
        off += (len<<3);
    }

    *offset = off;
    return result;
}

#if (CFG_HOST_PORT_PM_EN)
static int hif_tl_pm_lock_ref = 0;
static void HIF_TL_PM_Lock(void)
{
    bool need_lock = false;
    host_os_critical_flag_t critical_flag = host_os_enter_critical();
    if (hif_tl_pm_lock_ref == 0) {
        need_lock = true;
    } else if (hif_tl_pm_lock_ref < 0) {
        host_os_exit_critical(critical_flag);
        HOST_LOG_ERR("%s:pm lock ref status is error, hif_tl_pm_lock_ref=%d\n", __func__, hif_tl_pm_lock_ref);
        return ;
    }
    hif_tl_pm_lock_ref += 1;
    host_os_exit_critical(critical_flag);

    if (need_lock) {
        host_os_pm_lock();
    }
}

static void HIF_TL_PM_UnLock(void)
{
    bool need_unlock = false;
    host_os_critical_flag_t critical_flag = host_os_enter_critical();
    if (hif_tl_pm_lock_ref <= 0) {
        host_os_exit_critical(critical_flag);
//        HOST_LOG_ERR("%s:pm lock ref status is error, hif_tl_pm_lock_ref=%d\n", __func__, hif_tl_pm_lock_ref);
        return ;
    }
    hif_tl_pm_lock_ref -= 1;
    if (hif_tl_pm_lock_ref == 0) {
        need_unlock = true;
    }
    host_os_exit_critical(critical_flag);

    if (need_unlock) {
        host_os_pm_unlock();
    }
}
#endif

static void HIF_TL_PayloadLen_Set(uint8_t *phyhdr, uint32_t msgLen)
{
#if (CPU_ADDR_UNALIGN_SUPPORT)
	hif_msg_hdr_t *msghdr = (hif_msg_hdr_t *)&phyhdr[2];
	msghdr->length = msgLen & HIF_MSG_LENGTH_MASK;;
#else
	phyhdr[4] = (uint8_t)msgLen;
	phyhdr[5] = (phyhdr[5] & 0xF0) | ((msgLen>>8) & 0x0F);
#endif
}

static uint32_t HIF_TL_MsgLen_Get(uint8_t *phyhdr)
{
#if (CPU_ADDR_UNALIGN_SUPPORT)
	hif_msg_hdr_t *msghdr = (hif_msg_hdr_t *)&phyhdr[2];
	return msghdr->length + (msghdr->flag & BIT(2));
#else
	return (((phyhdr[5] & 0x0F)<<8) | phyhdr[4]) + ((phyhdr[2] & BIT(4))>>2);
#endif
}

static uint8_t HIF_TL_MsgCheckLen_Get(uint8_t *phyhdr)
{
	return (uint8_t)((phyhdr[2] & BIT(4))>>2);
}

static uint8_t HIF_TL_MsgSeq_Get(uint8_t *phyhdr)
{
	return (uint8_t)((phyhdr[5]>>4)& HIF_MSG_SEQ_MASK);
}

static void* HIF_TL_BufferGet(Hif_TL_Local_t *local, hif_msg_hdr_t *hdr, uint32_t size, uint8_t *buf_src)
{
	void *payload_buf = NULL;
	/* Try to get buffer from message layer for zero-copy */
	if (hdr->msg_id >= HIF_MSG_ID_REPORT_START) { /* report */
		if (local->msgbuf_get) {
			payload_buf = local->msgbuf_get((hif_msg_hdr_t *)hdr, size, local->msg_cb_arg);
		}
		if (payload_buf == NULL) {
			payload_buf = host_os_malloc(size);
			*buf_src = TL_MSG_BUF_SRC_TL_ALLOC;
		} else {
			*buf_src = TL_MSG_BUF_SRC_MSG;
		}
	} else { /* command response */
		uint32_t fix_buffer_len;
		if (local->cmd_waitresp == true) { /* user cmd */
			payload_buf = local->cmd_buf;
			fix_buffer_len = local->hif_cfg->cmd_buf_len;
		} else { /* tl local cmd */
			payload_buf = local->tl_cmdresp;
			fix_buffer_len = HIF_TL_LOCAL_CMD_BUF_LEN;
		}
		if (fix_buffer_len >= size) {
			*buf_src = TL_MSG_BUF_SRC_TL_FIX;
		} else {
			HOST_LOG_INF("Cmd_%02X: too long cmd response %d!\n", hdr->msg_id, size);
			*buf_src = TL_MSG_BUF_SRC_TL_ALLOC;
			payload_buf = host_os_malloc(size);
		}
	}
	return payload_buf;
}

static void HIF_TL_BufferPut(Hif_TL_Local_t *local, hif_msg_hdr_t *hdr, void *buffer, uint8_t buf_src)
{
	if (buffer != NULL) {
		if (buf_src == TL_MSG_BUF_SRC_TL_ALLOC) {
			host_os_free(buffer);
		} else if (buf_src == TL_MSG_BUF_SRC_MSG && local->msgbuf_put) {
			local->msgbuf_put(hdr, buffer, local->msg_cb_arg);
		}
	}
}

#if (HIF_TL_REPORT_PROC_THREAD_EN)
#define report_item_cnt(get, put)       ((int)((put) - (get)))
#define report_empty(get, put)          (report_item_cnt(get, put) <= 0)
#define report_full(get, put, mask)     (report_item_cnt(get, put) > (mask))

static int HIF_TL_ReportQueuePut(Hif_TL_Local_t *local, msg_item_t *item)
{
	unsigned int get  = local->msg_proc_get;
	unsigned int put  = local->msg_proc_put;
	while (report_full(get, put, (HIF_TL_REPORT_QUEUE_MAX_DEF-1))) {
		local->msg_proc_wait = 1;
		HOST_LOG_INF("proc queue is full. get=%d put=%d!\n", get, put);
		host_os_sem_give(local->proc_task_sem);
		host_os_sem_take(local->proc_finish_sem, HOST_OS_TIMEOUT_FOREVER);
		get = local->msg_proc_get;
	}
	msg_item_t *report_item = &local->msg_proc_queue[put & (HIF_TL_REPORT_QUEUE_MAX_DEF-1)];
	host_os_memcpy(report_item, item, sizeof(msg_item_t));
	local->msg_proc_put = ++put;
	host_os_sem_give(local->proc_task_sem);
	return 0;
}

static void HIF_TL_Report_Task(void *arg)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)arg;
	while (1) {
		unsigned int get = local->msg_proc_get;
		unsigned int put = local->msg_proc_put;
		if (!report_empty(get, put)) {
			msg_item_t *item = &local->msg_proc_queue[get & (HIF_TL_REPORT_QUEUE_MAX_DEF-1)];
			if (local->msgproc_cb != NULL) {
				local->msgproc_cb(&item->u.hdr, item->payload, local->msg_cb_arg);
			}
			if (item->payload != NULL) {
				HIF_TL_BufferPut(local, &item->u.hdr, item->payload, item->payload_src);
			}
			local->msg_proc_get = ++get;
		} else {
			if (local->msg_proc_wait) {
				local->msg_proc_wait = 0;
				host_os_sem_give(local->proc_finish_sem);
			}
			host_os_sem_take(local->proc_task_sem, HOST_OS_TIMEOUT_FOREVER);
		}
	}
	HOST_LOG_DBG("%s exit\n", __func__);
}
#endif

#define queue_item_cnt(get, put)       ((int16_t)((put) - (get)))
#define queue_empty(get, put)          (queue_item_cnt(get, put) <= 0)
#define queue_full(get, put, mask)     (queue_item_cnt(get, put) > (mask))

static int HIF_TL_ReportMsg(Hif_TL_Local_t *local)
{
	uint16_t mask = local->msg_queue_mask;
	uint16_t get  = local->msg_queue_get;
	uint16_t put  = local->msg_queue_put;
#if (CFG_TL_RETRY_EN)
	uint8_t flush = local->tl_retry_flush;
#else
	uint8_t flush = false;
#endif
	while (!queue_empty(get, put)) {
		msg_item_t *item = &local->msg_order_queue[get & mask];
		if (item->proc_type == TL_MSG_PROC_TYPE_REPORT) {
		#if (HIF_TL_REPORT_PROC_THREAD_EN)
			HIF_TL_ReportQueuePut(local, item);
		#else
			if (local->msgproc_cb != NULL) {
				local->msgproc_cb(&item->u.hdr, item->payload, local->msg_cb_arg);
			}
			if (item->payload != NULL) {
				HIF_TL_BufferPut(local, &item->u.hdr, item->payload, item->payload_src);
			}
		#endif
		} else if (item->proc_type == TL_MSG_PROC_TYPE_NONE && !flush) {
			break;
		}
		item->proc_type = TL_MSG_PROC_TYPE_NONE;
		++get;
	}
	local->msg_queue_get  = get;
	local->msg_report     = false;
#if (CFG_TL_RETRY_EN)
	local->tl_retry_flush = false;
#endif
	return 0;
}

#if (CFG_TL_RETRY_EN)
static int HIF_TL_AckInfo_Get(Hif_TL_Local_t *local, uint8_t *ack_info, uint8_t *ack_num)
{
	int tl_retry_num = 0;
	uint16_t mask = local->msg_queue_mask;
	uint16_t get  = local->msg_queue_get;
	uint16_t seq_win = local->tl_ack_seq_win;
	while (!queue_empty(get, seq_win)) {
		uint32_t seq_idx = get++ & mask;
		msg_item_t *item = &local->msg_order_queue[seq_idx];
		if (item->proc_type == TL_MSG_PROC_TYPE_NONE) { /* update TL ack info */
			ack_info[tl_retry_num++] = seq_idx;
		}
	}
	*ack_num = tl_retry_num;
	return 0;
}

static void HIF_TL_Retry_Update(Hif_TL_Local_t *local, hif_msg_hdr_u *msghdr)
{
	if (msghdr->hdr.msg_id == HIF_MSG_ID_HOST_POLL_ACK||
		msghdr->hdr.msg_id >= HIF_MSG_ID_REPORT_START) {
		uint16_t cur_seq = msghdr->hdr.seq;
		uint16_t report_seq_start = local->msg_queue_get & HIF_MSG_SEQ_MASK;
		uint16_t report_seq_end;
		if (cur_seq >= report_seq_start) {
			report_seq_end = local->msg_queue_get + (cur_seq - report_seq_start);
		} else {
			report_seq_end = local->msg_queue_get + (cur_seq + HIF_MSG_SEQ_MASK + 1 - report_seq_start);
		}
		if (msghdr->hdr.msg_id == HIF_MSG_ID_HOST_POLL_ACK) { /* Null-tail do not add seq */
			if (local->burst_first) {
				/* Flush timeout frames while TL retry if first frame is null-tail */
				local->msg_queue_put  = report_seq_end;
				local->tl_retry_flush = true;
				local->msg_report     = true;
			}
		} else {
			report_seq_end += 1; /* msg_id >= HIF_MSG_ID_REPORT_START */
		}
		local->tl_ack_seq_win = report_seq_end;
	}
}
#endif  /* CFG_TL_RETRY_EN */

static int HIF_TL_HandleMsg(Hif_TL_Local_t *local, hif_msg_hdr_u *msghdr, void *payload_buf, uint8_t buf_src)
{
	int report = false;
	if (msghdr->hdr.msg_id >= HIF_MSG_ID_REPORT_START) { /* report, push to msg queue */
		msg_item_t *msg_item;
		uint16_t put;

		if (local->msgprev_cb) {
			local->msgprev_cb((hif_msg_hdr_t *)msghdr, payload_buf, local->msg_cb_arg);
		}

		put = local->msg_queue_put;
#if (CFG_TL_RETRY_EN)
		if (local->tl_retry_enable) {
			msg_item = &local->msg_order_queue[msghdr->hdr.seq];
		} else
#endif
		if (!queue_full(local->msg_queue_get, put, local->msg_queue_mask)) {
			uint32_t idx = put & local->msg_queue_mask;
			msg_item = &local->msg_order_queue[idx];
		} else {
			HOST_LOG_ERR("%s: Queue is fulled, Drop report, Cmd ID=%02x, len=%d!\n",
					__func__, msghdr->hdr.msg_id, msghdr->hdr.length);
			HIF_TL_BufferPut(local, (hif_msg_hdr_t *)msghdr, payload_buf, buf_src);
			return true;
		}

		local->msg_queue_put = put + 1;
		if (msg_item->proc_type == TL_MSG_PROC_TYPE_NONE) {
			msg_item->proc_type = TL_MSG_PROC_TYPE_REPORT;
			msg_item->u.val32 = msghdr->val32;
			msg_item->payload = payload_buf;
			msg_item->payload_src = buf_src;
			local->msg_report  = true;
			report = true;
			host_os_sem_give(local->task_sem);
		} else {
			HOST_LOG_ERR("%s: replay? msg=%02x(%02x) len=%d(%d) seq=%d(%d)\n",
					__func__, msg_item->u.hdr.msg_id, msghdr->hdr.msg_id,
					msg_item->u.hdr.length, msghdr->hdr.length,
					msg_item->u.hdr.seq, msghdr->hdr.seq);
			/* TODO:Flush rx queue */
			HIF_TL_BufferPut(local, (hif_msg_hdr_t *)msghdr, payload_buf, buf_src);
		}
	} else {
		if (msghdr->hdr.msg_id == HIF_MSG_ID_HOST_POLL_ACK) {
			/* TODO: Handle it in TL */
		} else if (msghdr->hdr.msg_id == HIF_MSG_ID_PWR_CTRL) {
			/* TODO: Handle it in TL */
		} else if (local->cmd_waitresp) { /* command response, do not push to queue */
			if (local->msgproc_cb) {
				local->msgproc_cb((hif_msg_hdr_t *)msghdr, payload_buf, local->msg_cb_arg);
			} else {
				HOST_LOG_WRN("%s: No MsgHandle, Cmd ID=%02x, len=%d!\n",
						__func__, msghdr->hdr.msg_id, msghdr->hdr.length);
			}
			local->cmd_waitresp = false;
		}
		HIF_TL_BufferPut(local, (hif_msg_hdr_t *)msghdr, payload_buf, buf_src);
	}
	return report;
}


static void HIF_TL_ClearRxData(Hif_TL_Local_t *local, uint32_t data_len)
{
	while (data_len) {
		uint32_t read_len = MIN(data_len, HIF_TL_RX_SPEC_BUF_LEN);
		read_len = LLC_Recv(local->llc_hdl, (uint8_t *)local->spec_buf, read_len, local->llc_rx_timeout);
		data_len -= read_len;
	}
}

/* @return > 0: recv len(include header), = 0: header is not valid, < 0: system error */
static int HIF_TL_RecvHeader(Hif_TL_Local_t *local, hif_msg_hdr_u *msghdr, uint8_t **data_buf)
{
	int dummy_num = HIF_PROTO_HEAD_LEN;
	int burst_max_size = 4096 * local->burst_num;
	uint8_t *recv_hdr  = (uint8_t *)local->spec_buf + HIF_TL_RX_HDR_BUF_OFF;
	uint32_t read_len  = HIF_PROTO_HEAD_LEN;
	do {
		int recv_len = (int)LLC_Recv(local->llc_hdl, recv_hdr, read_len, local->llc_rx_timeout);
		if (recv_len < HIF_PROTO_HEAD_LEN) {
			return HOST_ERRCODE_IO_ERROR;
		} else {
			burst_max_size -= recv_len;
		}

		/* Magic Search and verify check8 */
		for (int i = 0; i < recv_len; ++i) {
			if (recv_hdr[i] == HIF_PHY_MSG_MAGIC) {

 				/* handle data tail */
				if (i + HIF_PROTO_HEAD_LEN > recv_len) {
					read_len = i + HIF_PROTO_HEAD_LEN - recv_len;
					int ret = (int)LLC_Recv(local->llc_hdl, &recv_hdr[recv_len],
											read_len, local->llc_rx_timeout);
					if (ret != read_len) {
						return HOST_ERRCODE_IO_ERROR;
					}
                    recv_len += ret;
				}

				if (0xFF == HIF_TL_CheckSum8(0, &recv_hdr[i], HIF_PROTO_HEAD_LEN)) {
					host_os_memcpy(msghdr, &recv_hdr[i+sizeof(hif_phy_hdr_t)], sizeof(hif_msg_hdr_t));
					*data_buf = &recv_hdr[i + HIF_PROTO_HEAD_LEN];
					return recv_len - i;
				}
			}

			if (recv_hdr[i] == 0xFF || recv_hdr[i] == 0x00) {
				if (--dummy_num <= 0) {
					return 0;
				}
			} else if (recv_hdr[i] != HIF_PHY_ACK_MAGIC) {
				local->burst_first = false;
				dummy_num = 4096;
				read_len = HIF_TL_RX_SPEC_BUF_LEN - HIF_PROTO_HEAD_LEN;
			}
		}
	} while (burst_max_size > 0);
	return 0; /* not found */
}

/* @return =0: recv payload sucess, > 0: check32 error < 0: error code */
static int HIF_TL_RecvPayload(Hif_TL_Local_t *local, hif_msg_hdr_u *msghdr,
									uint8_t *data_buf, uint32_t data_len,
                                    uint8_t **pp_payload, uint8_t *pbuf_src)
{
	uint8_t	*payload_buf  = NULL;
	uint32_t payload_len  = msghdr->hdr.length;

	/* Checksum32 = check32_data + payload + msghdr = 0xFFFFFFFF */
	uint32_t check_len	  = (msghdr->hdr.flag & BIT(2));
	uint32_t check32_data = 0;

	/* There is left payload to read */
	if (payload_len) {
		uint32_t left_len  = payload_len + check_len;
		payload_buf = HIF_TL_BufferGet(local, (hif_msg_hdr_t *)msghdr, left_len, pbuf_src);

		/* Handle the case no buffer */
		if (NULL == payload_buf) {
			local->msg_less_buffer = true; /* Less buffers in system */
			if (!queue_empty(local->msg_queue_get, local->msg_queue_put)) {
				/* Do report_task to release some buffers and try again */
				HIF_TL_ReportMsg(local);
				payload_buf = HIF_TL_BufferGet(local, (hif_msg_hdr_t *)msghdr, left_len, pbuf_src);
			}
			if (NULL == payload_buf) {
				/* Clear left data if no buffer  */
				if (left_len > data_len) {
					HIF_TL_ClearRxData(local, left_len - data_len);
				}
				return 1; /* no buffer, message is not valid */
			}
		}

		/* Copy front part of payload from spec_buf if need */
		if (data_len) {
			data_len = MIN(left_len, data_len);
			host_os_memcpy(payload_buf, data_buf, data_len);
			left_len -= data_len;
		}

		/* Read left data */
		if (left_len) {
			uint32_t recv_len = LLC_Recv(local->llc_hdl, &payload_buf[data_len],
											left_len, local->llc_rx_timeout);
			if (recv_len != left_len) { /* llc error */
				HOST_LOG_ERR("%s: payload(%d) recv error %d!\n",
							__func__, left_len, recv_len);
				HIF_TL_BufferPut(local, (hif_msg_hdr_t *)msghdr, (void *)payload_buf, *pbuf_src);
				return HOST_ERRCODE_IO_ERROR; /* device no data to read, just stop the burst period */
			}
		}

		/* calc checksum32  */
		if (check_len) {
			uint32_t check32_off = 0;
			/* Get the check32_data for compatible CPU unaligned-addr */
			host_os_memcpy(&check32_data, &payload_buf[payload_len], check_len);
			check32_data += HIF_TL_Checksum32(payload_buf, payload_len, &check32_off);
			check32_data += msghdr->val32;
		}
	} else if (check_len) { /* check32 only, no need to get buffer */
		int recv_len = LLC_Recv(local->llc_hdl, (uint8_t *)&check32_data,
							sizeof(check32_data), local->llc_rx_timeout);
		if (recv_len != sizeof(check32_data)) { /* llc error */
			HOST_LOG_ERR("%s: check32(%d) recv error %d!\n",
						__func__, sizeof(check32_data), recv_len);
			return HOST_ERRCODE_IO_ERROR; /* device no data to read, just stop the burst period */
		}
		check32_data += msghdr->val32;
	}

	/* Verify checksum32  */
	if (check_len && check32_data != 0xFFFFFFFF) {
		HOST_LOG_ERR("%s: payload(%d) check32 error 0x%0x!\n",
				__func__, payload_len, check32_data);
		HIF_TL_BufferPut(local, (hif_msg_hdr_t *)msghdr, payload_buf, *pbuf_src);
		return 1; /* checksum32 err, message is not valid */
	}

	/* Message is OK, return payload */
	*pp_payload = payload_buf;
	return HOST_ERRCODE_SUCCESS;
}

/* @return true: continue to read next msg, False: stop the burst */
static int HIF_TL_RecvMsg(Hif_TL_Local_t *local)
{
	uint8_t	*payload_buf = NULL;
	uint8_t  buf_src  = TL_MSG_BUF_SRC_NONE;
	uint8_t	*data_buf = NULL;
	hif_msg_hdr_u msghdr;
    uint8_t data_ready = 0;

	/* 1. Receive Message Data and check */
	int recv_len = HIF_TL_RecvHeader(local, &msghdr, &data_buf);
	if (recv_len >= HIF_PROTO_HEAD_LEN) { /* Header is Ok */

		data_ready = msghdr.hdr.flag & BIT(3);
		local->data_ready  = data_ready;
#if (CFG_TL_RETRY_EN)
		if (!data_ready && local->tl_retry_enable) {
			HIF_TL_Retry_Update(local, &msghdr);
		}
#endif
		local->burst_first = false;

        if (local->rx_rx_delay_us) {
            host_os_delayus(local->rx_rx_delay_us);
        }
		int ret = HIF_TL_RecvPayload(local, &msghdr, data_buf,
						recv_len - HIF_PROTO_HEAD_LEN, &payload_buf, &buf_src);
		if (ret != 0) {
			return ret > 0 ? data_ready : ret;
		}
	} else {
		local->data_ready = 0; /* reset data more */
		return 0; /* No more data, stop burst period */
	}

	/* 2. Handle message */
	if (HIF_TL_HandleMsg(local, &msghdr, payload_buf, buf_src)) {
		/* Do report task when msg queue is full or no buffer enough */
		if (local->msg_less_buffer ||
			queue_full(local->msg_queue_get, local->msg_queue_put,
						local->msg_queue_mask)) {
			HIF_TL_ReportMsg(local);
		}
	}
	return (int)data_ready; /* continue to recv next msg  */
}

static void HIF_TL_UpdateMsgHdr(Hif_TL_Local_t *local, hif_phy_hdr_t *phyhdr)
{
	uint8_t *msghdr = (uint8_t*)(phyhdr + 1);
    /* set magic */
    phyhdr->magic = HIF_PHY_MSG_MAGIC;
	/* update tx seq */
	msghdr[sizeof(hif_msg_hdr_t)-1] &= ~(HIF_MSG_SEQ_MASK << 4);
	msghdr[sizeof(hif_msg_hdr_t)-1] |= ((local->tx_seq++ & HIF_MSG_SEQ_MASK) << 4);
	/* update check8 */
	phyhdr->checksum = ~ HIF_TL_CheckSum8(HIF_PHY_MSG_MAGIC, msghdr, sizeof(hif_msg_hdr_t));
}

static int HIF_TL_Device_WakeUp(Hif_TL_Local_t *local, uint32_t timeout_ms)
{
	int ret;
	_ALIGNAS_VARIABLE uint8_t wakeup_magic[] = { HIF_PHY_WKP_MAGIC, 0xFF, HIF_PHY_WKP_MAGIC, 0xFF };
	_ALIGNAS_VARIABLE uint8_t wakeup_resp[2] = { 0 };
	do {
		ret = LLC_Send(local->llc_hdl, &wakeup_magic[0], sizeof(wakeup_magic), timeout_ms);
		if (ret == sizeof(wakeup_magic)) {
			host_os_delayms(1);
			ret = LLC_Recv(local->llc_hdl, &wakeup_resp[0], sizeof(wakeup_resp), timeout_ms);
			if (ret == sizeof(wakeup_resp)) {
				if (wakeup_resp[0] == HIF_PHY_ACK_MAGIC) {
					return HOST_ERRCODE_SUCCESS;
				}
			} else {
//				break;
			}
		} else {
			break;
		}
	} while (timeout_ms--);
	
	return timeout_ms ? ret : HOST_ERRCODE_TIMEOUT;
}


static int HIF_TL_Device_Sleep(Hif_TL_Local_t *local)
{
	uint32_t check32;
	uint32_t check_off = 0;
	uint32_t data_len  = 5;
	uint8_t device_sleep_cmd[] = {
		0xA5, 0x0,							 /* phy header */
		0x15, HIF_MSG_ID_PWR_CTRL, 0x1, 0x0, /* msg header */
		0x1,								 /* powerctrl mode, sleep = 0x1 */
		0x0, 0x0, 0x0, 0x0                   /* check32 */
	};				 

	HIF_TL_UpdateMsgHdr(local, (hif_phy_hdr_t *)&device_sleep_cmd[0]);
	/* calc data checksum32 */
	check32 = ~ HIF_TL_Checksum32(&device_sleep_cmd[sizeof(hif_phy_hdr_t)], data_len, &check_off);
	host_os_memcpy(&device_sleep_cmd[sizeof(hif_phy_hdr_t) + data_len], &check32, sizeof(check32));

	/* send data */
	data_len = LLC_Send(local->llc_hdl, &device_sleep_cmd[0], sizeof(device_sleep_cmd), HOST_OS_TIMEOUT_FOREVER);
	return data_len == sizeof(device_sleep_cmd) ? HOST_ERRCODE_SUCCESS : HOST_ERRCODE_IO_ERROR;
}

int HIF_TL_Disable_DevSleep(TL_HANDLE tl_hdl, uint8_t disable_sleep)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)tl_hdl;
	if (NULL != local) {
		local->dev_sleep_disable = disable_sleep;
		if (disable_sleep == 0) {
			host_os_sem_give(local->task_sem); /* set command event */
		}
		return HOST_ERRCODE_SUCCESS;
	}
	return HOST_ERRCODE_INVALID_HANDLE;
}

static void HIF_TL_SendCmd_Abort(Hif_TL_Local_t *local)
{
	host_os_sem_take(local->cmd_sem, HOST_OS_TIMEOUT_FOREVER);
	local->cmd_len = 0;
	host_os_sem_give(local->cmd_sem);
}

void HIF_TL_WaitCmdResp_Abort(TL_HANDLE tl_hdl)
{
    Hif_TL_Local_t *local = (Hif_TL_Local_t *)tl_hdl;

    if (NULL != tl_hdl) {
        host_os_sem_take(local->cmd_sem, HOST_OS_TIMEOUT_FOREVER);
        local->cmd_waitresp = false;
        host_os_sem_give(local->cmd_sem);
    }
}

static int HIF_TL_SendCmd(Hif_TL_Local_t *local)
{
	uint32_t check32;
	uint32_t check_off = 0;
	uint32_t data_len;
	uint32_t sent_len;

	host_os_sem_take(local->cmd_sem, HOST_OS_TIMEOUT_FOREVER);
	HIF_TL_UpdateMsgHdr(local, (hif_phy_hdr_t *)local->cmd_buf);
	/* calc data checksum32 */
	check32 = ~ HIF_TL_Checksum32(local->cmd_buf + sizeof(hif_phy_hdr_t), 
								local->cmd_len, &check_off);
	data_len = sizeof(hif_phy_hdr_t) + local->cmd_len;
	host_os_memcpy(local->cmd_buf + data_len, &check32, sizeof(check32));
	data_len += sizeof(check32);

	/* send data */
	sent_len = LLC_Send(local->llc_hdl, local->cmd_buf, data_len, HOST_OS_TIMEOUT_FOREVER);
	if (data_len == sent_len) {
		local->cmd_waitresp = true;
	}
	local->cmd_len = 0;
	host_os_sem_give(local->cmd_sem);
	return data_len == sent_len ? HOST_ERRCODE_SUCCESS : HOST_ERRCODE_IO_ERROR;
}

static int HIF_TL_SendPoll(Hif_TL_Local_t *local, uint32_t *burst_read)
{
	int ret = HOST_ERRCODE_IO_ERROR;
	uint32_t off = 0, check32;
	uint32_t data_len = 0;
	uint8_t  err_num  = 0;
	uint16_t burst_num = *burst_read;

	host_os_sem_take(local->poll_sem, HOST_OS_TIMEOUT_FOREVER);
#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
	if (local->msg_ack_len) {
		poll_ack_tlv_t *poll_ack = (poll_ack_tlv_t *)(local->poll_buf + HIF_PROTO_HEAD_LEN);
		uint8_t tlv_num = local->msg_ack_tlv_num;
		data_len += sizeof(poll_ack_tlv_t) + local->msg_ack_len;

#if (CFG_TL_RETRY_EN)
		if (local->tl_need_ack) {
			uint8_t *tlv_data = (uint8_t *)(poll_ack + 1);
			msg_tlv_ack_t *tl_ack = (msg_tlv_ack_t *)(tlv_data + local->msg_ack_len);
			HIF_TL_AckInfo_Get(local, &tl_ack->data[0], &err_num);
			tl_ack->msg_id = HIF_MSG_ID_HOST_POLL_ACK;
			tl_ack->type = HOST_POLL_TLV_ACK_TL;
			tl_ack->length = err_num;
			data_len += err_num;
			tlv_num  += 1;
			if (err_num) {
				burst_num = MIN(burst_num, err_num);
			}
		}
#endif
		poll_ack->poll_type = HOST_POLL_TYPE_ACK_TLV;
		poll_ack->burst_num = burst_num;
		poll_ack->tlv_num   = tlv_num;
	} else
#endif

    if (
#if (CFG_TL_RETRY_EN)
        local->tl_need_ack ||
#endif
        burst_num > 1) {
		poll_ack_t *poll_ack = (poll_ack_t *)(local->poll_buf + HIF_PROTO_HEAD_LEN);
#if (CFG_TL_RETRY_EN)
		if (local->tl_need_ack) {
			HIF_TL_AckInfo_Get(local, &poll_ack->tl_err_seq[0], &err_num);
			if (err_num) {
				burst_num = MIN(burst_num, err_num);
			}
		}
#endif
		poll_ack->poll_type = HOST_POLL_TYPE_TL_ACK;
		poll_ack->burst_num = burst_num;
		poll_ack->ack_err_cnt = err_num;
		data_len += sizeof(poll_ack_t) + err_num;
	}

	/* Update header and calc checksum */
	HIF_TL_PayloadLen_Set(local->poll_buf, data_len);
	HIF_TL_UpdateMsgHdr(local, (hif_phy_hdr_t *)local->poll_buf);
	data_len += sizeof(hif_msg_hdr_t);
	check32 = ~ HIF_TL_Checksum32(local->poll_buf + sizeof(hif_phy_hdr_t), data_len, &off);
	data_len += sizeof(hif_phy_hdr_t);
	host_os_memcpy(local->poll_buf + data_len, &check32, sizeof(check32));
	data_len += sizeof(check32);

	if (LLC_Send(local->llc_hdl, local->poll_buf,
		         data_len, HIF_TL_POLL_TIMEOUT_MS) == data_len) {
		/* update state of ack info */
#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
		local->msg_ack_len = 0;
		local->msg_ack_tlv_num  = 0;
#endif
#if (CFG_TL_RETRY_EN)
		local->tl_need_ack = false;
		if (local->tl_retry_enable && err_num == 0) {
			local->tl_ack_seq_win = local->msg_queue_get + burst_num;
		}
#endif
		ret = HOST_ERRCODE_SUCCESS;
	}
	host_os_sem_give(local->poll_sem);
	*burst_read = burst_num;
	return ret;
}

static void HIF_TL_Transport_Task(void *arg)
{
	uint32_t burst_read = 0;
	uint32_t tran_state = HIF_TL_TRAN_STATE_IDLE;
    uint32_t host_delay = 0;
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)arg;
	DevHw_t *devHw = LLC_Device_Get_DevHwCfg(local->llc_hdl);
	uint8_t upload_type = (devHw != NULL ? devHw->upload_type : LLC_UPLOAD_TYPE_PASSIVE);
	uint8_t bus_req = 0;
	uint8_t err_retry = 0;

	/* task loop */
	while (1) {
		switch (tran_state) {
		case HIF_TL_TRAN_STATE_IDLE:
			if (bus_req) {
				LLC_Device_Release_Bus(local->llc_hdl);
				bus_req = 0;
			}
			if (local->msg_report) {
				HIF_TL_ReportMsg(local);
			}
			if (local->cmd_len) { /* Send command is high priority */
				if (LLC_Device_Take_Bus(local->llc_hdl, HIF_TL_BUS_TIMEOUT_MS) == HOST_ERRCODE_SUCCESS) {
					bus_req = 1;
				} else {
					break;
				}
				if (local->dev_state == HIF_TL_DEV_STATE_SLEEP) {
					int ret = HIF_TL_Device_WakeUp(local, HIF_TL_WAKEUP_TIMEOUT_MS);
                    if (upload_type != LLC_UPLOAD_TYPE_PASSIVE) {
                        local->data_ready = false;
                        LLC_Device_Recv_Finish(local->llc_hdl);
                    }
					if (ret == HOST_ERRCODE_SUCCESS) {
                        host_os_delayus((local->tx_rx_delay_us)*2);
						local->dev_state = HIF_TL_DEV_STATE_ACTIVE;
					} else {
						/*TODO: wakeup error */
						HOST_LOG_ERR("WakeUp Device Err(%d)!\n", ret);
						if (++err_retry >= HIF_TL_LOCAL_ERR_RETRY_NUM) {
							HIF_TL_SendCmd_Abort(local);
						}
						break;
					}
				}
				if (HIF_TL_SendCmd(local) == HOST_ERRCODE_SUCCESS) {
					burst_read = 1;
                    host_delay = HIF_TL_CMD_TIMEOUT_MS; 
					tran_state = HIF_TL_TRAN_STATE_WAIT_RX;
				}
			} else if (local->data_ready ||
#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
				local->msg_ack_len ||
#endif
				LLC_IO_Get_Notify(local->llc_hdl) == HOST_IO_VALUE_HIGH) {
				if (LLC_Device_Take_Bus(local->llc_hdl, HIF_TL_BUS_TIMEOUT_MS) == HOST_ERRCODE_SUCCESS) {
					bus_req = 1;
				} else {
					break;
				}

				if (local->poll_enable) {
					if (local->data_ready == 0 && local->dev_state == HIF_TL_DEV_STATE_SLEEP &&
						LLC_IO_Get_Notify(local->llc_hdl) == HOST_IO_VALUE_LOW) {
						int ret = HIF_TL_Device_WakeUp(local, HIF_TL_WAKEUP_TIMEOUT_MS);
						if (ret == HOST_ERRCODE_SUCCESS) {
	                        host_os_delayus((local->tx_rx_delay_us)*2);
							local->dev_state = HIF_TL_DEV_STATE_ACTIVE;
						} else {
							/*TODO: wakeup error */
							HOST_LOG_ERR("WakeUp Device Err(%d)!\n", ret);
							if (++err_retry >= HIF_TL_LOCAL_ERR_RETRY_NUM) {
								host_os_sem_take(local->poll_sem, HOST_OS_TIMEOUT_FOREVER);
#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
								local->msg_ack_len = 0;
#endif
								host_os_sem_give(local->poll_sem);
							}
							break;
						}
					}
					burst_read = local->burst_num;
					if (HIF_TL_SendPoll(local, &burst_read) == HOST_ERRCODE_SUCCESS) {
                        host_delay = local->data_max_delay; 
						tran_state = HIF_TL_TRAN_STATE_WAIT_RX;
					} else {
						burst_read = 0;
						if (++err_retry >= HIF_TL_LOCAL_ERR_RETRY_NUM) {
							local->data_ready = 0;
							HOST_LOG_ERR("Send Poll Err!\n");
						}
					}
				} else {
					burst_read = local->burst_num;
					tran_state = HIF_TL_TRAN_STATE_RX;
				}
			} else {
				err_retry = 0;
				/* Idle, wait for event */
				if (local->rx_polling) {
					host_os_sem_take(local->task_sem, local->polling_time);
					local->data_ready = true;
				} else if (!local->cmd_waitresp &&
                           local->dev_sleep_disable == 0 && local->dev_state == HIF_TL_DEV_STATE_ACTIVE) {
					if (LLC_Device_Take_Bus(local->llc_hdl, HIF_TL_BUS_TIMEOUT_MS) == HOST_ERRCODE_SUCCESS) {
						bus_req = 1;
					} else {
						break;
					}
					if (HIF_TL_Device_Sleep(local) == HOST_ERRCODE_SUCCESS) {
						burst_read = 1;
						host_delay = HIF_TL_CMD_TIMEOUT_MS; 
						tran_state = HIF_TL_TRAN_STATE_WAIT_RX;
					}
					/* Device would timeout, so needn't retry if failed */
					local->dev_state = HIF_TL_DEV_STATE_SLEEP;
				} else {
#if (CFG_HOST_PORT_PM_EN)
					HIF_TL_PM_UnLock();
#endif
					host_os_sem_take(local->task_sem, HOST_OS_TIMEOUT_FOREVER);
#if (CFG_HOST_PORT_PM_EN)
					HIF_TL_PM_Lock();
#endif
				}
			}
			break;
		case HIF_TL_TRAN_STATE_WAIT_RX:
			/* delay for tx->rx */
			if (upload_type != LLC_UPLOAD_TYPE_PASSIVE) {
				tran_state = HIF_TL_TRAN_STATE_IDLE;
				break;
			}
			if (local->tx_rx_delay_us) {
				host_os_delayus(local->tx_rx_delay_us);
			}
			/* check io or timeout */
			if (LLC_IO_Get_Notify(local->llc_hdl) != HOST_IO_VALUE_HIGH) {
				while (host_delay-- && LLC_IO_Get_Notify(local->llc_hdl) != HOST_IO_VALUE_HIGH) {
					host_os_delayms(1);
				}

				if (LLC_IO_Get_Notify(local->llc_hdl) == HOST_IO_VALUE_LOW) {
					tran_state = HIF_TL_TRAN_STATE_IDLE;
					break;
				}
			}
		case HIF_TL_TRAN_STATE_RX:
			local->burst_first = true;
			/* Recieve message if in burst period */
			while (burst_read) {
				int ret = HIF_TL_RecvMsg(local);
				if (!ret) {
					burst_read = 0;
					break;
				}
				burst_read--;

                if (burst_read && local->rx_rx_delay_us) {
                    host_os_delayus(local->rx_rx_delay_us);
                }
			}

            /* Tell LLC Recv Finish, if Notify Type is COM_IRQ_THREAD*/
            LLC_Device_Recv_Finish(local->llc_hdl);

			LLC_Device_Release_Bus(local->llc_hdl);
			bus_req = 0;

			/* Set ack or not */
#if (CFG_TL_RETRY_EN)
			local->tl_need_ack = local->tl_retry_enable;
#endif
			/* Return to IDLE State */
			tran_state = HIF_TL_TRAN_STATE_IDLE;
			/* rx->tx delay */
			if (local->rx_tx_delay_us) {
				host_os_delayus(local->rx_tx_delay_us);
			}
			break;
		}
	}
}

/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
static void* HIF_TL_BufferGet_CB(uint32_t *size, uint32_t *timeout, void *arg)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)arg;
	if (local != NULL) {
		uint8_t *recv_hdr = (uint8_t *)local->spec_buf + HIF_TL_RX_HDR_BUF_OFF;

		*timeout = 50; //50ms
		/* return buffer to recv header if it's invalid */
		if (local->recv_hrd_valid == false) {
			recv_hdr[0] = 0;
			recv_hdr[1] = 0;
			*size = HIF_PROTO_HEAD_LEN;
			return recv_hdr;
		} 

		if (local->recv_buf == NULL) {
			uint32_t recv_len = HIF_TL_MsgLen_Get(recv_hdr);
			if (recv_len) {
				hif_msg_hdr_t *msghdr = (hif_msg_hdr_t *)&recv_hdr[sizeof(hif_phy_hdr_t)];
				local->recv_buf = HIF_TL_BufferGet(local, msghdr, recv_len, &local->recv_buf_src);
				local->recv_buf_len = recv_len;
			}
			*size = recv_len;
			return local->recv_buf;
		} else {
			HOST_LOG_ERR("%s: only 2 buffers for LLC!\n", __func__);
		}
	}
	return NULL;
}

static void HIF_TL_DataNotify_CB(uint8_t *data, uint32_t len, void *arg)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)arg;
	if (local != NULL) {
		if (data != NULL) {
			uint8_t buf_src  = TL_MSG_BUF_SRC_NONE;
			uint32_t msg_len;
			uint8_t *recv_hdr = (uint8_t *)local->spec_buf + HIF_TL_RX_HDR_BUF_OFF;
			hif_msg_hdr_u *msghdr = (hif_msg_hdr_u *)&recv_hdr[sizeof(hif_phy_hdr_t)];

			/* Handle msg header */
			if (local->recv_hrd_valid == false) {
				if (data[0] != HIF_PHY_MSG_MAGIC ||
					HIF_TL_CheckSum8(0, data, HIF_PROTO_HEAD_LEN) != 0xFF) {
					return ;
				}

				msg_len = HIF_TL_MsgLen_Get(data);
				if (recv_hdr == data) {
					local->recv_hrd_valid = true;
					if (msg_len != 0) {
						/* this is header buffer, wait next payload cb */
						return ;
					}
				} else { /* buffer from LLC, need copy */
					host_os_memcpy(recv_hdr, data, HIF_PROTO_HEAD_LEN);
					if (len >= msg_len + HIF_PROTO_HEAD_LEN) {
						len -= HIF_PROTO_HEAD_LEN;
						data += HIF_PROTO_HEAD_LEN;
					} else {
						HOST_LOG_ERR("%s: LLC Data is less %d %d!\n", __func__, len, msg_len);
						return ;
					}
				}
			} else {
				msg_len = HIF_TL_MsgLen_Get(recv_hdr);
			}

			/* reset the header */
			local->recv_hrd_valid = false;
			recv_hdr[0] = 0;
			
			/* Handle payload and calc check32 */
			if (msg_len) {
				/* Select buffer source */
				if (local->recv_buf == data) {
					local->recv_buf = NULL;
					buf_src = local->recv_buf_src;
				} else {
					HOST_LOG_INF("LLC Buf%d 0x%p(%p)!\n", msg_len, data, local->recv_buf);
					if (len >= msg_len) {
						buf_src = TL_MSG_BUF_SRC_LLC;
					} else {
						HOST_LOG_ERR("%s: LLC Data is less %d %d!\n", __func__, len, msg_len);
						return ;
					}
				}

				/* Calc check32 */
				uint32_t check_len = HIF_TL_MsgCheckLen_Get(recv_hdr);
				if (check_len) {
					uint32_t check32;
					uint32_t check32_off = 0;
					uint32_t payload_len = msg_len - check_len;
					host_os_memcpy(&check32, &data[payload_len], check_len);
					check32 += msghdr->val32; /* Add header */
					check32 += HIF_TL_Checksum32(data, payload_len, &check32_off); /* add payload */
					if (check32 != 0xFFFFFFFF) { /* checksum32 err */
						HOST_LOG_ERR("%s: payload(%d) check32 error 0x%0x!\n",
								__func__, payload_len, check32);
						HIF_TL_BufferPut(local, (hif_msg_hdr_t *)msghdr, data, buf_src);
						return ;
					}
				}
			}else {
				data = NULL; /* no payload, set NULL */
			}

			/* Hanedle Message */
			HIF_TL_HandleMsg(local, (hif_msg_hdr_u *)msghdr, data, buf_src);
		} else {
			/* set report event */
			local->data_ready = true;
			host_os_sem_give(local->task_sem);
		}
	} else {
		HOST_LOG_ERR("%s: arg is NULL!\n", __func__);
	}
}

TL_HANDLE HIF_TL_Init(HifCfg_t *cfg, LLC_HANDLE llc_hdl)
{
	Hif_TL_Local_t *local = NULL;
	if (cfg != NULL && llc_hdl != NULL) {
		local = (Hif_TL_Local_t *)host_os_malloc(sizeof(Hif_TL_Local_t));
		if (local != NULL) {
			int ret;
			/* reset buffer */
			host_os_memset(local, 0, sizeof(Hif_TL_Local_t));
			/* save param data */
			local->hif_cfg = cfg;
			local->llc_hdl = llc_hdl;

			/* init spec buffers */
			local->spec_buf = host_os_malloc(HIF_TL_RX_SPEC_BUF_LEN);
			if (NULL == local->spec_buf) {
				HOST_LOG_ERR("%s(%d):malloc(%d) Failed!\n",
							__func__, __LINE__, HIF_TL_RX_SPEC_BUF_LEN);
				goto tl_init_err;
			}

			/* init tl_cmdresp buffers */
			local->tl_cmdresp = host_os_malloc(HIF_TL_LOCAL_CMD_BUF_LEN);
			if (NULL == local->tl_cmdresp) {
				HOST_LOG_ERR("%s(%d):malloc(%d) Failed!\n",
							__func__, __LINE__, HIF_TL_LOCAL_CMD_BUF_LEN);
				goto tl_init_err;
			}

			/* init cmd buffers */
			if (local->hif_cfg->cmd_buf_len < HIF_PROTO_HEAD_LEN) {
				local->hif_cfg->cmd_buf_len = HIF_TL_CMD_BUF_LEN;
			}
			local->cmd_buf = host_os_malloc(local->hif_cfg->cmd_buf_len);
			if (NULL == local->cmd_buf) {
				HOST_LOG_ERR("%s(%d):malloc(%d) Failed!\n",
							__func__, __LINE__, local->hif_cfg->cmd_buf_len);
				goto tl_init_err;
			}
			local->cmd_sem = host_os_sem_create(1, 1);
			if (!host_os_sem_is_valid(local->cmd_sem)) {
				HOST_LOG_ERR("%s:cmd_sem Invalid!\n", __func__);
				goto tl_init_err;
			}

			/* init poll buffers */
			if (local->hif_cfg->poll_enable
#if (CFG_TL_RETRY_EN)
				|| local->hif_cfg->tl_retry_enable
#endif
#if (CFG_MSG_FRAGMENT_RETRY_EN)
				|| local->hif_cfg->fragment_retry_enable
#endif
#if (CFG_APP_RETRY_EN)
				|| local->hif_cfg->app_retry_enable
#endif
			) {
				local->poll_enable = true;
				if (local->hif_cfg->poll_buf_len < HIF_PROTO_HEAD_LEN) {
					local->hif_cfg->poll_buf_len = HIF_TL_POLL_BUF_LEN;
				}
				local->poll_buf = host_os_malloc(local->hif_cfg->poll_buf_len);
				if (NULL == local->poll_buf) {
					HOST_LOG_ERR("%s(%d):malloc(%d) Failed!\n",
								__func__, __LINE__, local->hif_cfg->poll_buf_len);
					goto tl_init_err;
				}
				local->poll_sem = host_os_sem_create(1, 1);
				if (!host_os_sem_is_valid(local->poll_sem)) {
					HOST_LOG_ERR("%s:cmd_sem Invalid!\n", __func__);
					goto tl_init_err;
				}
				host_os_memcpy(local->poll_buf, &poll_template[0], HIF_PROTO_HEAD_LEN);
			}

			/* init burst and queue */
			local->burst_num = (local->hif_cfg->burst_num ? 
							    local->hif_cfg->burst_num : HIF_TL_BURST_NUM_MAX_DEF);
#if (CFG_TL_RETRY_EN)
			if (local->hif_cfg->tl_retry_enable) {
				local->tl_retry_enable = local->hif_cfg->tl_retry_enable;
				local->burst_num = MIN(local->burst_num, HIF_TL_RETRY_MAX_BURST_NUM);
				local->hif_cfg->burst_num = local->burst_num;
				local->msg_queue_mask = HIF_TL_RETRY_MAX_BURST_NUM - 1;
			} else
#endif
            {
				local->msg_queue_mask = HIF_TL_RECV_QUEUE_MAX_DEF - 1;
			}
			local->msg_order_queue = (msg_item_t *)host_os_malloc(sizeof(msg_item_t) * (local->msg_queue_mask + 1));
			if (NULL == local->msg_order_queue) {
				HOST_LOG_ERR("%s(%d):malloc(%d) Failed!\n", __func__, __LINE__,
					sizeof(msg_item_t) * (local->msg_queue_mask + 1));
				goto tl_init_err;
			}
			host_os_memset(local->msg_order_queue, 0, sizeof(msg_item_t) * (local->msg_queue_mask + 1));
			
		#if (HIF_TL_REPORT_PROC_THREAD_EN)
			local->msg_proc_queue = (msg_item_t *)host_os_malloc(sizeof(msg_item_t) * HIF_TL_REPORT_QUEUE_MAX_DEF);
			if (NULL == local->msg_proc_queue) {
				HOST_LOG_ERR("%s(%d):malloc(%d) Failed!\n", __func__, __LINE__,
						sizeof(msg_item_t) * HIF_TL_REPORT_QUEUE_MAX_DEF);
				goto tl_init_err;
			}
			host_os_memset(local->msg_proc_queue, 0, sizeof(msg_item_t) * HIF_TL_REPORT_QUEUE_MAX_DEF);

			local->proc_task_sem = host_os_sem_create(0, 1);
			if (!host_os_sem_is_valid(local->proc_task_sem)) {
				HOST_LOG_ERR("%s:proc_task_sem Invalid!\n", __func__);
				goto tl_init_err;
			}
			local->proc_finish_sem = host_os_sem_create(0, 1);
			if (!host_os_sem_is_valid(local->proc_finish_sem)) {
				HOST_LOG_ERR("%s:proc_finish_sem Invalid!\n", __func__);
				goto tl_init_err;
			}
			local->proc_task_hdl = host_os_thread_create("hif_tl_proc_task", HIF_TL_REPORT_TASK_STACK_SIZE,
											(host_os_thread_entry_t)(HIF_TL_Report_Task),
											 local, HOST_OS_THREAD_PRIO_NORMAL);
			if (!host_os_thread_is_valid(local->proc_task_hdl)) {
				HOST_LOG_ERR("%s:hif_tl_proc_task create Failed!\n", __func__);
				goto tl_init_err;
			}
		#endif

			/* regist LLC layer callback */
			LLC_Buffer_Get_Handle_Regist(local->llc_hdl, &HIF_TL_BufferGet_CB, local);
			ret = LLC_Notify_Handle_Regist(local->llc_hdl, &HIF_TL_DataNotify_CB,
				                               local, local->polling_time);
			if (HOST_ERRCODE_SUCCESS != ret) {
				HOST_LOG_ERR("%s(%d):LLC_Notify_Handle_Regist Failed(%d)!\n",
								__func__, __LINE__, ret);
				goto tl_init_err;
			}

			/* init semaphore */
			local->task_sem = host_os_sem_create(0, 1);
			if (!host_os_sem_is_valid(local->task_sem)) {
				HOST_LOG_ERR("%s:task_sem Invalid!\n", __func__);
				goto tl_init_err;
			}

			/* init task */
			local->task_hdl = host_os_thread_create("hif_tl_task", HIF_TL_TASK_STACK_SIZE,
											(host_os_thread_entry_t)(HIF_TL_Transport_Task),
											 local, HOST_OS_THREAD_PRIO_HIGH);
			if (!host_os_thread_is_valid(local->task_hdl)) {
				HOST_LOG_ERR("%s:hif_tl_task create Failed!\n", __func__);
				goto tl_init_err;
			}

			DevHw_t *dev_cfg = LLC_Device_Get_DevHwCfg(local->llc_hdl);
            if (NULL == dev_cfg) {
                HOST_LOG_ERR("%s:get device hw cfg fail\n", __func__);
                goto tl_init_err;
            }

			/* init default param  */
            if (dev_cfg->bus_type == LLC_BUS_TYPE_UART) {
    			local->tx_rx_delay_us = 500;  //500us
    			local->rx_tx_delay_us = 0;    //0us
    			local->data_max_delay = 5;    //5ms
    			local->polling_time   = 10;  //10ms
    			local->llc_rx_timeout = 500; //500ms
            } else {
    			local->tx_rx_delay_us = 500;  //500us
    			local->rx_tx_delay_us = 100;  //100us
    			local->data_max_delay = 5;    //5ms
    			local->polling_time   = 10;  //10ms
    			local->llc_rx_timeout = 500; //500ms
            }
			local->dev_state = HIF_TL_DEV_STATE_SLEEP;

			if (dev_cfg->bus_type == LLC_BUS_TYPE_I2C) {
				local->rx_rx_delay_us = 500;
			}
			HOST_LOG_DBG("%s: suceess(0x%p)!\n", __func__, local);
		} else {
			HOST_LOG_ERR("%s(%d):malloc(%d) Failed!\n",
						__func__, __LINE__, sizeof(Hif_TL_Local_t));
		}
		return local;
	}

tl_init_err:
	if (local) {
	#if (HIF_TL_REPORT_PROC_THREAD_EN)
		if (host_os_thread_is_valid(local->proc_task_hdl)) {
			host_os_thread_delete(local->proc_task_hdl);
		}
		if (host_os_sem_is_valid(local->proc_task_sem)) {
			host_os_sem_delete(local->proc_task_sem);
		}
		if (host_os_sem_is_valid(local->proc_finish_sem)) {
			host_os_sem_delete(local->proc_finish_sem);
		}
		if (local->msg_proc_queue) {
			host_os_free(local->msg_proc_queue);
			local->msg_proc_queue = NULL;
		}
	#endif
		if (local->spec_buf) {
			host_os_free(local->spec_buf);
			local->spec_buf = NULL;
		}
		if (local->tl_cmdresp) {
			host_os_free(local->tl_cmdresp);
			local->tl_cmdresp = NULL;
		}
		if (local->cmd_buf) {
			host_os_free(local->cmd_buf);
			local->cmd_buf = NULL;
		}
		if (local->poll_buf) {
			host_os_free(local->poll_buf);
			local->poll_buf = NULL;
		}
		if (local->msg_order_queue) {
			host_os_free(local->msg_order_queue);
			local->msg_order_queue = NULL;
		}
		if (host_os_sem_is_valid(local->task_sem)) {
			host_os_sem_delete(local->task_sem);
		}
		if (host_os_sem_is_valid(local->cmd_sem)) {
			host_os_sem_delete(local->cmd_sem);
		}
		if (host_os_sem_is_valid(local->poll_sem)) {
			host_os_sem_delete(local->poll_sem);
		}
		LLC_Buffer_Get_Handle_Regist(local->llc_hdl, NULL, NULL);
		LLC_Notify_Handle_Regist(local->llc_hdl, NULL, NULL, 0);
		local->llc_hdl = NULL;
		host_os_free(local);
	}
	return NULL;
}

void HIF_TL_DeInit(TL_HANDLE tl_hdl)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)tl_hdl;
	if (local != NULL && local->llc_hdl != NULL) {

	#if (HIF_TL_REPORT_PROC_THREAD_EN)
		host_os_thread_delete(local->proc_task_hdl);
		host_os_sem_delete(local->proc_task_sem);
		host_os_sem_delete(local->proc_finish_sem);
		if (local->msg_proc_queue) {
			host_os_free(local->msg_proc_queue);
			local->msg_proc_queue = NULL;
		}
	#endif

		host_os_thread_delete(local->task_hdl);
		host_os_sem_delete(local->task_sem);
		if (local->spec_buf) {
			host_os_free(local->spec_buf);
			local->spec_buf = NULL;
		}
		if (local->tl_cmdresp) {
			host_os_free(local->tl_cmdresp);
			local->tl_cmdresp = NULL;
		}
		if (local->cmd_buf) {
			host_os_free(local->cmd_buf);
			local->cmd_buf = NULL;
			host_os_sem_delete(local->cmd_sem);
		}
		if (local->poll_buf) {
			host_os_free(local->poll_buf);
			local->poll_buf = NULL;
			host_os_sem_delete(local->poll_sem);
		}
		if (local->recv_buf) {
			host_os_free(local->recv_buf);
			local->recv_buf = NULL;
		}
		if (local->msg_order_queue) {
			host_os_free(local->msg_order_queue);
			local->msg_order_queue = NULL;
		}
		LLC_Buffer_Get_Handle_Regist(local->llc_hdl, NULL, NULL);
		LLC_Notify_Handle_Regist(local->llc_hdl, NULL, NULL, 0);
		local->llc_hdl = NULL;
		host_os_free(local);
	}
}

void HIF_TL_DevReset(TL_HANDLE tl_hdl)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)tl_hdl;
	if (local != NULL) {
#if (CFG_TL_RETRY_EN)
		local->tl_need_ack = 0;
		local->tl_ack_seq_win = 0;
#endif

		local->dev_state   = HIF_TL_DEV_STATE_SLEEP;
		local->data_ready  = 0;
		local->tx_seq      = 0;
#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
		local->msg_ack_len = 0;
#endif
		local->recv_hrd_valid = 0;

		local->burst_first     = 0;
		local->msg_less_buffer = 0;
		if (!queue_empty(local->msg_queue_get, local->msg_queue_put)) {
			local->msg_report  = true;
#if (CFG_TL_RETRY_EN)
			local->tl_retry_flush = true;
#endif
			host_os_sem_give(local->task_sem);
			while (!queue_empty(local->msg_queue_get, local->msg_queue_put));
			local->msg_queue_get = 0;
			local->msg_queue_put = 0;
		}
#if (HIF_TL_REPORT_PROC_THREAD_EN)
		host_os_sem_give(local->proc_task_sem);
		while (!report_empty(local->msg_proc_get, local->msg_proc_put));
#endif
	}
}

int HIF_TL_MsgHandle_Regist(TL_HANDLE tl_hdl, MSG_PROC_CB proc_cb, MSG_PROC_CB prev_cb,
			MSG_BUFFER_GET_CB buffer_get, MSG_BUFFER_PUT_CB buffer_put, void *cb_arg)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)tl_hdl;
	if (local != NULL) {
		local->msg_cb_arg = cb_arg;
		local->msgprev_cb = prev_cb;
		local->msgproc_cb = proc_cb;
		if (proc_cb == NULL) {
			local->msgbuf_get = NULL;
			local->msgbuf_put = NULL;
		} else {
			local->msgbuf_get = buffer_get;
			local->msgbuf_put = buffer_put;
		}
		return HOST_ERRCODE_SUCCESS;
	}
	return HOST_ERRCODE_INVALID_PARAM;
}

#if (HOST_HIF_POLL_ACK_TLV_IS_VALID)
int HIF_TL_PollAck_Set(TL_HANDLE tl_hdl, hif_msg_hdr_t *msg, void *ack_info, uint32_t len)
{
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)tl_hdl;
	if (local != NULL && ack_info != NULL && len) {
		if (local->poll_enable) {
			host_os_sem_take(local->poll_sem, HOST_OS_TIMEOUT_FOREVER);
			uint32_t cur_len = HIF_PROTO_HEAD_LEN + sizeof(poll_ack_tlv_t)
				               + local->msg_ack_len;
			if (cur_len + len + HIF_PROTO_TAIL_LEN <= local->hif_cfg->poll_buf_len) {
				msg_tlv_ack_t *msg_ack = (msg_tlv_ack_t *)(local->poll_buf + cur_len);
				msg_ack->msg_id = msg->msg_id;
				msg_ack->type   = msg->frag ? HOST_POLL_TLV_ACK_MSG_FRAG : 0;
				msg_ack->length = len;
				host_os_memcpy(&msg_ack->data[0], ack_info, len);
				local->msg_ack_len += sizeof(msg_tlv_ack_t) + len;
				local->msg_ack_tlv_num++;
				host_os_sem_give(local->poll_sem);
				HOST_LOG_DBG_HEX(ack_info, len, "Poll-ACK type=%d:\n", msg_ack->type);
				host_os_sem_give(local->task_sem);
				return HOST_ERRCODE_SUCCESS;
			}
			host_os_sem_give(local->poll_sem);
			return HOST_ERRCODE_NO_BUFFER;
		}
		return HOST_ERRCODE_NOT_READY;
	}else {
		/* TODO: Device need support APP-ACK msg */
		#if (0)
		msg->type = HIF_MSG_TYPE_TO_DEVICE;
		msg->flag &= ~ BIT(0);
		msg->length = len;
		HIF_TL_SendMsg(tl_hdl, msg, ack_info);
		#endif
	}
	return HOST_ERRCODE_INVALID_PARAM;
}
#endif

int HIF_TL_SendMsg(TL_HANDLE tl_hdl, hif_msg_hdr_t *msg, void *payload)
{
	/* check param */
	Hif_TL_Local_t *local = (Hif_TL_Local_t *)tl_hdl;
	if (local == NULL || msg == NULL || (payload == NULL && msg->length != 0)) {
		return HOST_ERRCODE_INVALID_PARAM;
	}
	if (msg->length + HIF_PROTO_HEAD_TAL_LEN > local->hif_cfg->cmd_buf_len) {
		return HOST_ERRCODE_NO_BUFFER;
	}

	host_os_sem_take(local->cmd_sem, HOST_OS_TIMEOUT_FOREVER);
	if (local->cmd_len == 0) {
		/* copy data */
		uint8_t *msg_buffer = (uint8_t *)local->cmd_buf + sizeof(hif_phy_hdr_t);
		host_os_memcpy(msg_buffer, msg, sizeof(hif_msg_hdr_t));
		if (msg->length) {
			msg_buffer += sizeof(hif_msg_hdr_t);
			host_os_memcpy(msg_buffer, payload, msg->length);
		}
		local->cmd_len = sizeof(hif_msg_hdr_t) + msg->length;
		host_os_sem_give(local->cmd_sem);
		host_os_sem_give(local->task_sem); /* set command event */
		return HOST_ERRCODE_SUCCESS;
	} else {
		host_os_sem_give(local->cmd_sem);
		return HOST_ERRCODE_BUSY;
	}
}

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
