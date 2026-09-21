/*
 **************************************************************************************************
 *          Copyright (c) 2022 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */
#include <hal_types.h>
#include "mmw_ctrl.h"
#include "mmw_report.h"
#include "mmw_alg_debug.h"

#include "hif_checksum.h"
#include "log.h"
#include "util.h"

#include "mmw_app_pointcloud_config.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_app_micro_pointcloud.h"
#include "mmw_point_cloud_psic_lib.h"
#include <math.h>


#ifndef MIN
#define MIN(x, y)					((x) < (y) ? (x) : (y))
#endif

extern uint8_t g_presence_point_en;

/* cfar method select: 0/1 */
extern uint8_t g_cfar_method;

int get_tx_rx_num(uint8_t *tx, uint8_t *rx)
{
	int ret = 0;
	uint8_t work = 0;
	uint8_t txrx = 0;

	ret = mmw_mode_get(&txrx, &work);
	if(ret) {
		printk("mmw mode get fail ! ret = %d \r\n", ret);
		return ret;
	}

	switch(txrx) {
		case MMW_MIMO_1T1R:
			*tx = 1;
			*rx = 1;
			break;
		case MMW_MIMO_1T3R:
			*tx = 1;
			*rx = 3;
			break;
		case MMW_MIMO_2T3R:
			*tx = 2;
			*rx = 3;
			break;
		case MMW_MIMO_1T4R:
			*tx = 1;
			*rx = 4;
			break;
		case MMW_MIMO_2T4R:
			*tx = 2;
			*rx = 4;
			break;
		case MMW_MIMO_1T2R:
			*tx = 1;
			*rx = 2;
			break;
		default:
			ret = -1;
			break;
	}
	return ret;
}

struct cube_report_info_t{
	MMW_FRAME_UPLOAD report_hdr;
	uint32_t report_time_ms;
	uint32_t next_frame_time;
	uint32_t frame_cb_cnt;
	uint16_t range_fft_num;
	uint16_t doppler_fft_num;
	uint16_t range_idx;
	uint16_t dop_idx;
	uint16_t block_cnt;
    uint16_t block_idx;
    uint16_t finish_idx;
	uint8_t  aborted;
	uint8_t  reserve;
	uint8_t tx_cnt;
	uint8_t rx_cnt;
	uint8_t tx_idx;
	uint8_t rx_idx;
} cube_report_info;

struct cube_report_info_t cube_report_info_backup;		// for backup cube_report_info

struct cube_retry_info_t{
    HOST_POLL_APP_CUBE *retry_cfg;
    uint16_t retry_block_idx;
    uint16_t retry_offset;
    uint16_t retry_arr_idx;
} cube_retry_info;

#define CUBE_CACHE_BLOCK    (8)
OSI_Semaphore_t cube_report_sem;
bool cube_access = false;

Detection3D_Data D_data_test = {0x00};
Detection3D_State D_State_test = {0x00};

static uint32_t g_cube_report_cache_block_size = CUBE_DATA_SIZE; /* Buffer size per data block for datacube reporting, in bytes */
static uint32_t g_cube_report_cache_block_num = CUBE_CACHE_BLOCK; /* Number of data blocks to buffer for datacube reporting */
/* Each block carries TL infor along with the datacube.This variable specifies the actual buffer size per block when reporting */
static uint32_t g_cube_report_cache_block_size_max = CUBE_SIZE_MAX;

void cube_report_frame_reset(void)
{
	cube_report_info.report_hdr.offset = 0;
	cube_report_info.range_idx = 0;
	cube_report_info.dop_idx   = 0;
	cube_report_info.tx_idx = 0;
	cube_report_info.rx_idx = 0;

	cube_report_info.block_idx = 0;
	cube_report_info.finish_idx = 0;
	cube_report_info.aborted   = 0;
}

static bool is_cube_report_frame_finish(void)
{
	return (cube_report_info.block_idx && cube_report_info.finish_idx >= cube_report_info.block_idx);
}


static void cube_access_ctrl(bool enable)
{
    if(enable == cube_access) {
        return ;
    } else {
        cube_access = enable;
        if(enable) {
            mmw_motion_cube_access_open();
        } else {
            mmw_motion_cube_access_close();
        }
    }
}

void cube_report_callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status);
static int hif_dma_cube_report_handler(uint8_t *cubeBuffer)
{
	uint8_t *cube_data = NULL;
	uint16_t read_num = 0;
	uint16_t offset = CUBE_HEAD_LEN;
	uint16_t read_cnt = 0;
    int ret = 0;

	cube_data = cubeBuffer;

    for(uint32_t i=0; i<cube_report_info.tx_cnt*cube_report_info.rx_cnt*cube_report_info.doppler_fft_num;i++ ) {
        if((g_cube_report_cache_block_size_max - offset) >= 4 * (cube_report_info.range_fft_num - cube_report_info.range_idx)) {
            read_num = cube_report_info.range_fft_num - cube_report_info.range_idx;
        } else {
            read_num = (g_cube_report_cache_block_size_max - offset) / 4;
        }
        __mmw_fft_range((complex16_cube *)&cube_data[offset], cube_report_info.range_idx, read_num, cube_report_info.tx_idx, cube_report_info.rx_idx, cube_report_info.dop_idx);
        offset = offset + (4*read_num);
        cube_report_info.range_idx += read_num;
        if(cube_report_info.range_idx >= cube_report_info.range_fft_num) {
            cube_report_info.range_idx = 0;
            cube_report_info.dop_idx ++;
            if(cube_report_info.dop_idx == cube_report_info.doppler_fft_num) {
                cube_report_info.dop_idx = 0;
                cube_report_info.rx_idx ++;
                if(cube_report_info.rx_idx == cube_report_info.rx_cnt) {
                    cube_report_info.rx_idx = 0;
                    cube_report_info.tx_idx ++;
                    if(cube_report_info.tx_idx == cube_report_info.tx_cnt) {
                        cube_report_info.tx_idx = 0;
                    }
                }
            }
        }
        read_cnt = (offset - CUBE_HEAD_LEN) / 4;
        if(offset == (g_cube_report_cache_block_size_max) || (cube_report_info.report_hdr.frame_len - cube_report_info.report_hdr.offset - read_cnt) == 0) {
            memcpy(cube_data, &cube_report_info.report_hdr, sizeof(MMW_FRAME_UPLOAD));
            ret = HIF_MsgReport(0xC1, cube_data, offset, cube_report_callback);
            if (ret) {
                cube_report_info = cube_report_info_backup;     // Revert to the previous cube_report_info
                printf("Send 0xC1, %d\n", ret);
            } else {
                cube_report_info.report_hdr.offset += read_cnt;
                cube_report_info_backup = cube_report_info;
            }

            return ret;
        }
    }
	return -1;
}

void cube_report_retry_callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status);
static int hif_dma_cube_report_retry_send(uint32_t retry_offset, uint8_t* cubeBuffer)
{
    uint16_t retry_range_idx = retry_offset % cube_report_info.range_fft_num;
    uint16_t retry_dop = (retry_offset - retry_range_idx) / cube_report_info.range_fft_num;
    uint16_t retry_dop_idx = retry_dop % cube_report_info.doppler_fft_num;
    uint16_t retry_rx = (retry_dop - retry_dop_idx) / cube_report_info.doppler_fft_num;
    uint8_t retry_rx_idx = retry_rx % cube_report_info.rx_cnt;
    uint16_t retry_tx = (retry_rx - retry_rx_idx) / cube_report_info.rx_cnt;
    uint8_t retry_tx_idx = retry_tx % cube_report_info.tx_cnt;

    MMW_FRAME_UPLOAD retry_report_hdr;
	uint8_t *cube_data = NULL;
	uint16_t read_num = 0;
	uint16_t offset = CUBE_HEAD_LEN;
	uint16_t read_cnt = 0;

	cube_data = cubeBuffer;

    for(uint32_t i = 0; i < cube_report_info.tx_cnt*cube_report_info.rx_cnt*cube_report_info.doppler_fft_num; i++) {
        if((g_cube_report_cache_block_size_max - offset) >= 4 * (cube_report_info.range_fft_num - retry_range_idx)) {
            read_num = cube_report_info.range_fft_num - retry_range_idx;
        } else {
            read_num = (g_cube_report_cache_block_size_max - offset) / 4;
        }
        __mmw_fft_range((complex16_cube *)&cube_data[offset], retry_range_idx, read_num, retry_tx_idx, retry_rx_idx, retry_dop_idx);
        offset = offset + (4*read_num);
        retry_range_idx += read_num;
        if(retry_range_idx >= cube_report_info.range_fft_num) {
            retry_range_idx = 0;
            retry_dop_idx ++;
            if(retry_dop_idx == cube_report_info.doppler_fft_num) {
                retry_dop_idx = 0;
                retry_rx_idx ++;
                if(retry_rx_idx == cube_report_info.rx_cnt) {
                    retry_rx_idx = 0;
                    retry_tx_idx ++;
                    if(retry_tx_idx == cube_report_info.tx_cnt) {
                        retry_tx_idx = 0;
                    }
                }
            }
        }
        read_cnt = (offset - CUBE_HEAD_LEN) / 4;
        retry_report_hdr.frame_len = cube_report_info.report_hdr.frame_len;
        retry_report_hdr.offset = retry_offset;
        retry_report_hdr.frame_idx = cube_report_info.report_hdr.frame_idx;
        if(offset == (g_cube_report_cache_block_size_max) || (retry_report_hdr.frame_len - retry_report_hdr.offset - read_cnt) == 0) {
            memcpy(cube_data, &retry_report_hdr, sizeof(MMW_FRAME_UPLOAD));

            return HIF_MsgReport(0xC1, cube_data, offset, cube_report_retry_callback);
        }
    }
	return -1;
}

void cube_report_retry_callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
{
	/* 1. frame is aborted.
     * 2. time is not enough */

    uint8_t *pBuff = HIF_MsgReport_ListPopData(pDataListHead);
    hif_MEM_AppNodeFree(pBuff);

    if (cube_report_info.aborted || time_after(csi_coret_get_value(), cube_report_info.next_frame_time)) {
		if (cube_retry_info.retry_cfg) {
			OSI_Free(cube_retry_info.retry_cfg);
			cube_retry_info.retry_cfg = NULL;
			cube_access_ctrl(false);
		}
        return ;
    }

    if(cube_retry_info.retry_cfg && cube_retry_info.retry_cfg->retry_num) {
		uint32_t frame_len = cube_report_info.report_hdr.frame_len;
		uint32_t retry_arr_idx = cube_retry_info.retry_arr_idx;
		uint32_t offset = cube_retry_info.retry_cfg->retry[retry_arr_idx].offset + cube_retry_info.retry_offset;
		uint32_t read_num = MIN(g_cube_report_cache_block_size / 4, cube_retry_info.retry_cfg->retry[retry_arr_idx].len);
		if (read_num && (offset + read_num <= frame_len)) {
            uint8_t *buffer = hif_MEM_AppNodeMalloc(0);
            if(buffer) {
                hif_dma_cube_report_retry_send(offset, buffer);
                cube_retry_info.retry_block_idx ++;
                cube_retry_info.retry_offset += read_num;
                if (cube_retry_info.retry_offset == cube_retry_info.retry_cfg->retry[retry_arr_idx].len) {
                    cube_retry_info.retry_cfg->retry_num--;
                    cube_retry_info.retry_arr_idx += 1;
                    cube_retry_info.retry_offset = 0;
                }
            } else {

            }
		} else {
			cube_retry_info.retry_cfg->retry_num = 0;
		}
		if (cube_retry_info.retry_cfg->retry_num == 0) {
			cube_access_ctrl(false);
			OSI_Free(cube_retry_info.retry_cfg);
			cube_retry_info.retry_cfg = NULL;
		}
    }
}

void cube_report_retry_handler(uint8_t *data, uint32_t retry_num)
{
    if (retry_num) {
		/* 1. frame report is not finish
		 * 2. last retry is in progress
		 * 3. new frame is coming */
		if (cube_report_info.aborted || !is_cube_report_frame_finish() || cube_retry_info.retry_cfg ||
			time_after(csi_coret_get_value(), cube_report_info.next_frame_time)) {
			return ;
		}
	    cube_retry_info.retry_block_idx = 0;
	    cube_retry_info.retry_offset = 0;
	    cube_retry_info.retry_arr_idx = 0;

		uint32_t size = sizeof(HOST_POLL_APP_CUBE) + retry_num * sizeof(HOST_CUBE_RETRY);
        cube_retry_info.retry_cfg = (HOST_POLL_APP_CUBE *)OSI_Malloc(size);
        if(cube_retry_info.retry_cfg == NULL) {
            return ;
        }
		cube_retry_info.retry_cfg->retry_num = retry_num;
        memcpy(&cube_retry_info.retry_cfg->retry[0], (uint8_t *)data, retry_num * sizeof(HOST_CUBE_RETRY));

		uint32_t retry_arr_idx = cube_retry_info.retry_arr_idx;
		uint32_t frame_len = cube_report_info.report_hdr.frame_len;
        cube_access_ctrl(true);
        for(int i = 0; i < CUBE_CACHE_BLOCK; i++) {
            if (retry_num) {
				uint32_t offset = cube_retry_info.retry_cfg->retry[retry_arr_idx].offset + cube_retry_info.retry_offset;
				uint32_t read_num = MIN(g_cube_report_cache_block_size / 4, cube_retry_info.retry_cfg->retry[retry_arr_idx].len);
				if (read_num && (offset + read_num <= frame_len)) {
                    uint8_t *buffer = hif_MEM_AppNodeMalloc(0);
                    if(buffer) {
                        hif_dma_cube_report_retry_send(offset, buffer);
                        cube_retry_info.retry_block_idx ++;
                        cube_retry_info.retry_offset += read_num;
                        if (cube_retry_info.retry_offset == cube_retry_info.retry_cfg->retry[retry_arr_idx].len) {
                            retry_num--;
                            retry_arr_idx += 1;
                            cube_retry_info.retry_offset = 0;
                        }
                    } else {
                        break;
                    }
				} else {
					retry_num = 0;
				}
                if (retry_num == 0) {
                    cube_access_ctrl(false);
                    OSI_Free(cube_retry_info.retry_cfg);
                    cube_retry_info.retry_cfg = NULL;
                    break;
                }
            }
        }
		cube_retry_info.retry_cfg->retry_num = retry_num;
		cube_retry_info.retry_arr_idx = retry_arr_idx;
    }
}

void cube_report_callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
{
	uint32_t time_start;
	SET_TIME_START(time_start);
	cube_report_info.finish_idx++;

    uint8_t *pBuff = HIF_MsgReport_ListPopData(pDataListHead);
    hif_MEM_AppNodeFree(pBuff);

	if (!cube_report_info.aborted) {
		/* there are blocks to fill. */
		if (status == 0 && (cube_report_info.block_idx >= CUBE_CACHE_BLOCK) && (cube_report_info.block_idx < cube_report_info.block_cnt)) {
            uint8_t *buffer = hif_MEM_AppNodeMalloc(0);
            if(buffer) {
                hif_dma_cube_report_handler(buffer);
                cube_report_info.block_idx++;
            } else {

            }
		} else if ((cube_report_info.finish_idx >= cube_report_info.block_idx) && (CUBE_CACHE_BLOCK < cube_report_info.block_cnt)) {
			/* All cube read done, release waitting of frame callback. */
			OSI_SemaphoreRelease(&cube_report_sem);
		}
	}
	SET_TIME_ESCAPE(time_start, time_start);
	UPDATE_TIME_DBG_INFO(time_start, "cube-bk", g_time_dbg_info.report_block);
}

extern uint8_t g_dc_suppression_flag;
int mmw_ctrl_data_cube_report_cb(void *mmw_data, void *arg)
{
	int ret = 0;

	uint32_t time_now = csi_coret_get_value();
	cube_report_info.report_hdr.frame_idx = cube_report_info.frame_cb_cnt++;

	if (cube_report_info.report_hdr.frame_idx == 0) {
		uint32_t period_ms = 0;
		uint32_t frame_num = 0;
		mmw_frame_get(&period_ms,  &frame_num);
		uint32_t interval_period = 0;
		uint32_t interval_num = 0;;
		mmw_interval_get(&interval_period,  &interval_num);
		interval_period = (interval_period * interval_num) / 1000;
		if(period_ms > interval_period) {
			cube_report_info.report_time_ms = period_ms - interval_period;
		} else {
			return ret; /* no time to report */
		}
	}

	if (g_dc_suppression_flag) {
		if (MMW_ERR_CODE_SUCCESS != mmw_psic_dc_suppression_update()) {
			printf("condition error!\r\n");
		}
	}

	/* reset report status */
	cube_report_frame_reset();
	cube_access_ctrl(true);

 	cube_report_info_backup = cube_report_info;

     uint32_t com_type = 0x00;
     HIF_ExtControl(HIF_GET_COM_TYPE, &com_type, sizeof(com_type));
     uint32_t sendNodeLevel = MIN(cube_report_info.block_cnt, CUBE_CACHE_BLOCK);
     sendNodeLevel = MIN(sendNodeLevel, 4);
     if (com_type == HIF_COM_TYPE_SPI) {
         HIF_ExtControl(HIF_SET_SEND_NODE_LEVEL, &sendNodeLevel, sizeof(sendNodeLevel));
     }

	for(int i = 0; i < CUBE_CACHE_BLOCK; i++) {
		if(cube_report_info.block_idx < cube_report_info.block_cnt) {
            uint8_t *buffer = hif_MEM_AppNodeMalloc(0);
            if(buffer) {
                hif_dma_cube_report_handler(buffer);
                cube_report_info.block_idx++;
            } else {
                break;
            }
		}
	}

    if (com_type == HIF_COM_TYPE_SPI) {
        sendNodeLevel = 0;
        HIF_ExtControl(HIF_SET_SEND_NODE_LEVEL, &sendNodeLevel, sizeof(sendNodeLevel));
    }

	/* 500us is margin */
	cube_report_info.next_frame_time = time_now + US_TO_CORET(cube_report_info.report_time_ms*1000 - 500);

    if (CUBE_CACHE_BLOCK < cube_report_info.block_cnt) {
    	ret = OSI_SemaphoreWait(&cube_report_sem, cube_report_info.report_time_ms);
    	if(ret != OSI_STATUS_OK) {
    		cube_report_info.aborted = true;
    	}
    }

	cube_access_ctrl(false);

	SET_TIME_ESCAPE(time_now, time_now);
	UPDATE_TIME_DBG_INFO(time_now, "report", g_time_dbg_info.report);

	return ret;
}


void mmw_report_param_get(void)
{
	int ret = 0;

	D_State_test.proc_frame_num = 0;

	ret = get_tx_rx_num(&cube_report_info.tx_cnt, &cube_report_info.rx_cnt);
	if(ret) {
		printf("mmw mode get fail ! ret = %d \r\n", ret);
		return;
	}
	ret = mmw_fft_num_get(&cube_report_info.range_fft_num, &cube_report_info.doppler_fft_num);
	if(ret) {
		printk("fft num get fail ! ret = %d \r\n", ret);
		return;
	}

	uint32_t frame_len = cube_report_info.tx_cnt * cube_report_info.rx_cnt *
	                     cube_report_info.range_fft_num * cube_report_info.doppler_fft_num;
	uint32_t data_cube_size = frame_len * 4;
	
	/* Set the datacube data size per block.
	 * A block that is too large wastes memory;
	 * a block that is too small degrades efficiency
	 */
	if (data_cube_size <= (1 * 1024)) {
		g_cube_report_cache_block_size = 1024;
	} else if (data_cube_size <= (8 * 1024)) {
		g_cube_report_cache_block_size = 2048;
	} else {
		g_cube_report_cache_block_size = CUBE_DATA_SIZE;
	}
	g_cube_report_cache_block_size_max = g_cube_report_cache_block_size + CUBE_HEAD_LEN;
	
	cube_report_info.block_cnt = (frame_len * 4 + g_cube_report_cache_block_size - 1) / g_cube_report_cache_block_size;
	cube_report_info.frame_cb_cnt = 0;
	cube_report_info.report_hdr.frame_len = frame_len;
	cube_report_info.report_hdr.frame_idx = 0;
	cube_report_frame_reset();
	
	/* Maximum number of blocks per frame is limited to 16 to pervent sram overflow.
	 * Developers can reconfigure this threshold based on the remaining SRAM budget.
	 */
	g_cube_report_cache_block_num = MIN(16, cube_report_info.block_cnt);
	
	/* Set the number of datacube frames to buffer.
	 * A larger buffer depth uses more sram.
	 * Ensure adequate resources are available before increasing.
	 */
	uint8_t datacube_latency_report_frame_cnt = 0;
	if (cube_report_info.doppler_fft_num == 1) {
		if (cube_report_info.range_fft_num <= 256) {
			datacube_latency_report_frame_cnt = 8;
		} else {
			datacube_latency_report_frame_cnt = 6;
		}
	} else {
		datacube_latency_report_frame_cnt = 1;
	}
	
	g_cube_report_cache_block_num = g_cube_report_cache_block_num * datacube_latency_report_frame_cnt;
}


void mmw_data_cube_report_init(void)
{
    HIF_AppPool_Cfg_t appPoolCfg = {g_cube_report_cache_block_num, g_cube_report_cache_block_size_max, NULL};

    int ret = hif_MEM_AppPoolInit(&appPoolCfg, 1);
    if(ret) {
        LOG_ERR("cube app malloc fail! \r\n");
    } else {
        printf("cube pool malloc success \r\n");
    }

    OSI_SemaphoreCreate(&cube_report_sem, 0, 1);
}

void mmw_data_cube_report_deinit(void)
{
    cube_report_info.aborted = true;

    hif_MEM_AppPoolDeinit();

    OSI_SemaphoreDelete(&cube_report_sem);

	memset(&cube_report_info, 0, sizeof(struct cube_report_info_t));
}

void mmw_presence_point_report_init(void) {
	if ((D_data_test.presence_points == NULL) && g_presence_point_en) {
		D_data_test.presence_points = OSI_Malloc(sizeof(PointCloud3D)*PRESENCE_POINT_MAX);
	}
}

void mmw_presence_point_report_deinit(void) {
	if ((D_data_test.presence_points != NULL) && g_presence_point_en) {
		OSI_Free(D_data_test.presence_points);
		D_data_test.presence_points = NULL;
	}
}

void send_obj_data_Callback(HIF_Frame_Node_t *hif_queue_item, int8_t status)
{
}
static void mmw_msg_send_obj_data(MMW_FRAME_UPLOAD *report_hdr, TL_HEADER *tl, uint8_t *payload, uint32_t len)
{
    int status = 0;
    HIF_Data_ListHead_t DataListHead = {NULL, NULL};

    do {
        status = HIF_MsgReport_ListPushData(&DataListHead, report_hdr, sizeof(MMW_FRAME_UPLOAD));
        if (status != 0) {
            break;
        }
        if (tl != NULL) {
    		status = HIF_MsgReport_ListPushData(&DataListHead, tl, sizeof(TL_HEADER));
            if (status != 0) {
                break;
            }
    	}
    	if (len) {
    		status = HIF_MsgReport_ListPushData(&DataListHead, payload, len);
            if (status != 0) {
                break;
            }
    	}

        status = HIF_MsgReport_ListStart(0xC3, &DataListHead, NULL);
        if (status != 0) {
            break;
        }

        if (tl != NULL) {
            report_hdr->offset += (sizeof(TL_HEADER)>>2);
        }
        if(len) {
            report_hdr->offset += (len>>2);
        }
    } while (0);

    // if (status != 0) {
        uint8_t *pBuff = NULL;
        do {
            pBuff = HIF_MsgReport_ListPopData(&DataListHead);
            if (pBuff == NULL) {
                break;
            }
        } while (1);
    // }
}

void mmw_pointcloud_upload(MMW_FRAME_UPLOAD *frame_hdr, TL_HEADER *tl, uint8_t *payload, uint32_t payload_len)
{
	do {
	 	uint32_t packet_len = MIN(payload_len, POINT_DATA_SIZE);
		payload_len -= packet_len;
		mmw_msg_send_obj_data(frame_hdr, tl, payload, packet_len);
		payload   += packet_len;
		tl = NULL;
	} while (payload_len > 0);
}

void tracker_targets_upload(MMW_FRAME_UPLOAD *frame_hdr, ClusterTrackElm *buff, uint32_t total_num)
{
	TL_HEADER tl;
	TL_HEADER *tl_ptr = &tl;
	tl.type   = 5;
	tl.flag   = 1;
	tl.length = total_num*sizeof(ClusterTrackElm);
	do {
		uint32_t packet_num = MIN(total_num, POINT_DATA_SIZE/sizeof(ClusterTrackElm));
		total_num -= packet_num;
		mmw_msg_send_obj_data(frame_hdr, tl_ptr, (uint8_t *)buff, packet_num*sizeof(ClusterTrackElm));
		buff += packet_num;
		tl_ptr = NULL;
		OSI_MSleep(1);
	} while (total_num > 0);
}

extern uint8_t  gui_data_report;
int mmw_detection3d_data_report(Detection3D_Data *data, Detection3D_State *state, void *arg)
{
	MMW_FRAME_UPLOAD frame_hdr;
	bool upload_point   = false;
	#if (CONFIG_MMW_APP_TRACK_ENABLE)
	bool upload_targets = false;
	#endif
	uint32_t frame_num  = state->proc_frame_num;

	switch (gui_data_report) {
	case GUI_DATA_TYPE_POINTS:
		upload_point = true;
		break;
	case GUI_DATA_TYPE_TRACKING:
	#if (CONFIG_MMW_APP_TRACK_ENABLE)
		upload_targets = true;
	#endif
		break;
	case GUI_DATA_TYPE_POINTS_TRACKING:
		upload_point   = true;
	#if (CONFIG_MMW_APP_TRACK_ENABLE)
		upload_targets = true;
	#endif
		break;
	default:
		return 0;
	}

	/* Setup frame header */
	frame_hdr.frame_idx = frame_num;
	frame_hdr.frame_len = 0;
	frame_hdr.offset	 = 0;
#if 1 /* (CONFIG_MMW_APP_POINT_CLOUD_ENABLE) */
	if (upload_point) {
		frame_hdr.frame_len += ((sizeof(PointCloud_Polar)*data->motion_points_num  + sizeof(TL_HEADER))>>2);
	}
#endif

#if (CONFIG_MMW_PRESENCE_POINT_CLOUD)
	if (upload_point && g_presence_point_en) {
		frame_hdr.frame_len += ((sizeof(PointCloud_Polar)*data->presence_points_num  + sizeof(TL_HEADER))>>2);
	}
#endif
#if (CONFIG_MMW_APP_TRACK_ENABLE)
	if (upload_targets) {
		frame_hdr.frame_len += ((sizeof(ClusterTrackElm)*data->tracker_objs_num  + sizeof(TL_HEADER))>>2);
	}
#endif

	/* Output data packets */
#if 1 /*CONFIG_MMW_APP_POINT_CLOUD_ENABLE) */
	if (upload_point) { /* motion points packets */
		TL_HEADER tl;
		uint32_t total_len = sizeof(PointCloud_Polar) * data->motion_points_num;
		tl.type    = MMW_TL_TYPE_POINTS;
		tl.flag    = MMW_TL_FLAG_CART;
		tl.length = total_len;
		mmw_pointcloud_upload(&frame_hdr, &tl, (uint8_t *)data->motion_points, tl.length);
	}
#endif

#if (CONFIG_MMW_PRESENCE_POINT_CLOUD)
	if (upload_point && g_presence_point_en) {
		TL_HEADER tl;
		tl.type    = MMW_TL_TYPE_POINTS;
		tl.flag    = MMW_TL_FLAG_CART|MMW_TL_FLAG_MICRO;
		tl.length  = sizeof(PointCloud_Polar) * data->presence_points_num;
		/* output presence points packets */
		mmw_pointcloud_upload(&frame_hdr, &tl, (uint8_t *)data->presence_points, tl.length);
	}
#endif

#if (CONFIG_MMW_APP_TRACK_ENABLE)
	if (upload_targets) {
		/* tracker targes packets */
		tracker_targets_upload(&frame_hdr, data->tracker_objs, data->tracker_objs_num);
	}
#endif
	(void)frame_hdr;
	return 0;
}

/* The callback function when 2D-FFT finished in point cloud application */
int mmw_ctrl_data_point_report_cb(void *mmw_data, void *arg)
{
	uint32_t time_start, time_motion = 0, time_micro = 0, time_report = 0;

	float sin_y, sin_z, sin_x; /* x, y, z, vel, */

	uint32_t range_mm, range_reol_mm, velocity_mm, veloc_resol_mm;
	uint16_t range_fft_num, doppler_fft_num;
	float range_bin_size_cm, dop_bin_size_cm;
	int16_t doppler_idx_half;
	float user_coordinate[3];
	mmw_range_get(&range_mm, &range_reol_mm);
	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);

	range_bin_size_cm = (float)range_mm / range_fft_num * 0.1f;
	SET_TIME_START(time_start);

	/*--------------------- MOTION POINT PROCESS --------------------- */
	PointCloudBuffer_t *ptr_point_cloud_buffer;
	if (g_cfar_method == 0) {
		ptr_point_cloud_buffer = mmw_point_cloud_process();
	} else if (g_cfar_method == 1) {
		ptr_point_cloud_buffer = mmw_point_cloud_process_sw_cfar();
	} else {
		ptr_point_cloud_buffer = 0;
	}

	SET_TIME_ESCAPE(time_motion, time_start);
	UPDATE_TIME_DBG_INFO(time_motion, "motion", g_time_dbg_info.motion_point);
	SET_TIME_START(time_start);

	/*---------------------  PRESENCE POINT PROCESS --------------------- */
	#if (CONFIG_MMW_PRESENCE_POINT_CLOUD)
	if (g_presence_point_en) {
		bool micro_update = mmw_micro_point_frame();
		/* auto gain function must be cleared manually as soon as gain factor is not used,
		   to make auto gain in next frame works well.
		   Hardware reuired gain factor cleared before chirp transmit period
		*/
		mmw_fft_autogain_clear();

		if (micro_update) {
			PresencePointCloudBuffer_t *ptr_presence_point_cloud_buffer;
			ptr_presence_point_cloud_buffer = mmw_presence_point_process();

			uint32_t frame_period, dummy;
			uint32_t micro_dop_num = mmw_micro_doppler_num_get();
			doppler_idx_half = micro_dop_num >> 1;
			mmw_frame_get(&frame_period, &dummy);
			frame_period = frame_period * mmw_micro_frame_rate_get();
			float micro_veloc_uint = (float)(5 * 100) / (frame_period * micro_dop_num);
			float range_cm, veloc;

			for (int presence_pc_idx = 0; presence_pc_idx < ptr_presence_point_cloud_buffer->presence_point_cloud_num; presence_pc_idx++) {
				/*
				* unit cm in D_data_test structure,
				* x = range * sin(x)
				* y = range * sqrt(1-sin(x)^2-sin(z)^2)
				* z = range * sin(z)
				*/
				range_cm = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].range_idx * range_bin_size_cm;
				veloc = (ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].doppler_idx - doppler_idx_half)  * micro_veloc_uint;
				sin_x = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].azi_phase;
				sin_z = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].ele_phase;
				sin_y = 1.f - sin_x * sin_x - sin_z * sin_z;
				sin_y = sqrtf(sin_y);
				mmw_point_cloud_trans_radar_coord_to_user_coord(range_cm * sin_x,
								range_cm * sin_y,
								range_cm * sin_z,
								user_coordinate
								);

				D_data_test.presence_points[presence_pc_idx].cart.x = user_coordinate[0];
				D_data_test.presence_points[presence_pc_idx].cart.y = user_coordinate[1];
				D_data_test.presence_points[presence_pc_idx].cart.z = user_coordinate[2];
				D_data_test.presence_points[presence_pc_idx].cart.vel = veloc;
				D_data_test.presence_points[presence_pc_idx].cart.snr = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].sig_snr * 100;
			}
			D_data_test.presence_points_num = ptr_presence_point_cloud_buffer->presence_point_cloud_num;
			mmw_process_mem_free((void **) &ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data);
			mmw_process_mem_free((void **) &ptr_presence_point_cloud_buffer);
		}
	} else {
		D_data_test.presence_points_num = 0;
		/* auto gain function must be cleared manually as soon as gain factor is not used,
		   to make auto gain in next frame works well.
		   Hardware reuired gain factor cleared before chirp transmit period
		*/
		mmw_fft_autogain_clear();
	}
	ADD_TIME_ESCAPE(time_micro, time_start);
	UPDATE_TIME_DBG_INFO(time_micro, "micro", g_time_dbg_info.micro_point);
	SET_TIME_START(time_start);
#else
	D_data_test.presence_points_num = 0;
	(void)time_micro;
	/* auto gain function must be cleared manually as soon as gain factor is not used,
	   to make auto gain in next frame works well.
	   Hardware reuired gain factor cleared before chirp transmit period
	*/
	mmw_fft_autogain_clear();
#endif

	/*---------------------  DATA CONVERT PROCESS --------------------- */
	mmw_velocity_get(&velocity_mm, &veloc_resol_mm);
	dop_bin_size_cm = (float)velocity_mm / doppler_fft_num * 0.2f; /* velocity_mm is single size */
	doppler_idx_half = doppler_fft_num >> 1;

	/* Convert to cartisian coordinated system, an extra memory copy will be applied */
	if (D_data_test.motion_points == 0){
		//memset(&D_data_test, 0, sizeof(D_data_test));
		mmw_process_mem_alloc((void**)&D_data_test.motion_points, sizeof(*D_data_test.motion_points) * ptr_point_cloud_buffer->point_cloud_num);
	}
	for (int point_cloud_idx = 0; point_cloud_idx < ptr_point_cloud_buffer->point_cloud_num; point_cloud_idx++) {
		/*
		 * unit cm in D_data_test structure,
		 * x = range * cos(phi) * sin(theta)
		 * y = range * cos(phi) * cos(theta)
		 * z = range * sin(phi)
		 * or
		 * x = range * sin(x)
		 * y = range * sqrt(1-sin(x)^2-sin(z)^2)
		 * z = range * sin(z)
		*/
		sin_x = ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].azi_phase;
		sin_z = ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].ele_phase;
		sin_y = 1.f - sin_x * sin_x - sin_z * sin_z;
		sin_y = sqrtf(sin_y);
			mmw_point_cloud_trans_radar_coord_to_user_coord(ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].range_idx * sin_x * range_bin_size_cm,
								   ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].range_idx * sin_y * range_bin_size_cm,
								   ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].range_idx * sin_z * range_bin_size_cm,
								   user_coordinate
								);

		D_data_test.motion_points[point_cloud_idx].cart.x = user_coordinate[0];
		D_data_test.motion_points[point_cloud_idx].cart.y = user_coordinate[1];
		D_data_test.motion_points[point_cloud_idx].cart.z = user_coordinate[2];
		D_data_test.motion_points[point_cloud_idx].cart.vel = (ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].doppler_idx - doppler_idx_half) * dop_bin_size_cm;
		D_data_test.motion_points[point_cloud_idx].cart.snr = ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx].sig_snr * 100;
	}

	D_data_test.motion_points_num = ptr_point_cloud_buffer->point_cloud_num;
	D_data_test.tracker_objs_num  = 0;

	mmw_process_mem_free((void**) &ptr_point_cloud_buffer->ptr_motion_point_cloud_data);
	mmw_process_mem_free((void**) &ptr_point_cloud_buffer);
	SET_TIME_ESCAPE(time_start, time_start);
	UPDATE_TIME_DBG_INFO(time_start, "convert", g_time_dbg_info.motion_convert);
	SET_TIME_START(time_start);

	/*---------------------  DATA REPORT PROCESS --------------------- */
	/* This is a block function, free memory after this functon returns */
	mmw_detection3d_data_report(&D_data_test, &D_State_test, NULL);
	ADD_TIME_ESCAPE(time_report, time_start);
	UPDATE_TIME_DBG_INFO(time_report, "report", g_time_dbg_info.report);

	mmw_process_mem_free((void**)&D_data_test.motion_points);
	D_State_test.proc_frame_num++;

	if (g_dc_suppression_flag) {
		if (MMW_ERR_CODE_SUCCESS != mmw_psic_dc_suppression_update()) {
			printf("condition error!\r\n");
		}
	}

	return 0;
}
