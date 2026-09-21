
#define LOG_MODULE                      "RADAR_FRAMEWORK_REPORT"
#define LOG_LEVEL                       1

#include "common.h"
#include <math.h>
#include "mmw_ctrl.h"
#include "mmw_point_cloud_psic_lib.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_app_pointcloud.h"
#include "mmw_alg_debug.h"
#include "log.h"
#include "radar_signal_process_2d.h"
#include "radar_framework_report.h"
#include "mmw_app_micro_pointcloud.h"
#include "radar_framework.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
#if ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0))
struct cube_report_info_t{
	MMW_FRAME_UPLOAD report_hdr;
    MMW_FRAME_TL report_tl;
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

struct cube_report_info_t cube_report_info_backup;      // for backup cube_report_info

struct cube_retry_info_t{
    HOST_POLL_APP_CUBE *retry_cfg;
    uint16_t retry_block_idx;
    uint16_t retry_arr_idx;
    uint32_t retry_offset;
} cube_retry_info;

uint32_t cube_report_cache_block_num = 32;
uint32_t cube_report_cache_block_size = 2048;
uint32_t cube_report_cache_block_size_max = 2048 + CUBE_HEAD_LEN;

#endif /*((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0)) */

/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private macros.
 * ----------------------------------------------------------------------------
 */
#ifndef MIN
#define MIN(x, y)					((x) < (y) ? (x) : (y))
#endif
/* Private variables.
 * ----------------------------------------------------------------------------
 */
Detection3D_State D_State_test = {0x00};

#if ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0))
OSI_Semaphore_t cube_report_sem;
static bool semRelease = false;
bool cube_access = false;
#endif

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
#if ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0))
__sram_text void cube_report_frame_reset(void)
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

__sram_text static void cube_access_ctrl(bool enable)
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
__sram_text static int hif_dma_cube_report_handler(uint8_t *cubeBuffer)
{
	uint8_t *cube_data = NULL;
    uint8_t cube_head_len = 0;
    uint16_t read_num = 0;
    uint32_t cube_size_max = 0;

    if(cube_report_info.report_hdr.offset == 0) {
        cube_head_len = CUBE_HEAD_LEN;
        cube_size_max = cube_report_cache_block_size_max;
    } else {
        cube_head_len = CUBE_HEAD_LEN - sizeof(MMW_FRAME_TL);
        cube_size_max = cube_report_cache_block_size_max - sizeof(MMW_FRAME_TL);
    }
    uint32_t offset = cube_head_len;
	uint16_t read_cnt = 0;
    int ret = 0;

	cube_data = cubeBuffer;

    for(uint32_t i=0; i<cube_report_info.tx_cnt*cube_report_info.rx_cnt*cube_report_info.doppler_fft_num;i++ ) {
        if((cube_size_max - offset) >= 4 * (cube_report_info.range_fft_num - cube_report_info.range_idx)) {
            read_num = cube_report_info.range_fft_num - cube_report_info.range_idx;
        } else {
            read_num = (cube_size_max - offset) / 4;
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
        read_cnt = (offset - cube_head_len) / 4;

        uint32_t remainData = cube_report_info.report_hdr.frame_len - cube_report_info.report_hdr.offset - 4 * read_cnt;
        if ((offset == cube_size_max) ||
            ((cube_report_info.report_hdr.offset > 0) && (remainData == 0)) ||
            ((cube_report_info.report_hdr.offset == 0) && (remainData == 12))) {

            memcpy(cube_data, &cube_report_info.report_hdr, sizeof(MMW_FRAME_UPLOAD));
            if(cube_report_info.report_hdr.offset == 0) {
                memcpy(&cube_data[12], &cube_report_info.report_tl, sizeof(MMW_FRAME_TL));
            }

            ret = HIF_MsgReport(0xC2, cube_data, offset, cube_report_callback);
            if (ret) {
                cube_report_info = cube_report_info_backup;
            } else {
                if (cube_report_info.report_hdr.offset == 0) {
                    cube_report_info.report_hdr.offset += sizeof(MMW_FRAME_TL);
                }
                cube_report_info.report_hdr.offset += (read_cnt * 4);

                cube_report_info_backup = cube_report_info;
            }
            return ret;
        }
    }
	return -1;
}

__sram_text void cube_report_callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
{
	uint32_t time_start;
	SET_TIME_START(time_start);

	cube_report_info.finish_idx++;

    uint8_t *pBuff = HIF_MsgReport_ListPopData(pDataListHead);
    hif_MEM_AppNodeFree(pBuff);

#if (CONFIG_HIF_APP_SPEC_POOL == 0)

	if (!cube_report_info.aborted) {
		/* there are blocks to fill. */
		if (status == 0 && (cube_report_info.block_idx >= cube_report_cache_block_num) && (cube_report_info.block_idx < cube_report_info.block_cnt)) {
            uint8_t *buffer = hif_MEM_AppNodeMalloc(0);
            if(buffer) {
                hif_dma_cube_report_handler(buffer);
                cube_report_info.block_idx++;
            } else {

            }
		} else if ((cube_report_info.finish_idx >= cube_report_info.block_idx) && (cube_report_cache_block_num < cube_report_info.block_cnt)) {
			/* All cube read done, release waitting of frame callback. */
            if(semRelease) {
                semRelease = false;
    			OSI_SemaphoreRelease(&cube_report_sem);
            }
		}
	}
#else
    if(status == 0) {
        uint8_t idx = 0;
        if(hif_MEM_AppNodeListIsFull(0)) {
            idx = 0;
        } else if(hif_MEM_AppNodeListIsFull(1)) {
            idx = 1;
        } else {
            return;
        }
        for(int i = 0; i < cube_report_cache_block_num; i++) {
            if(cube_report_info.block_idx < cube_report_info.block_cnt) {
                uint8_t *buffer = hif_MEM_AppNodeMalloc(idx);
                if(buffer) {
                    hif_dma_cube_report_handler(buffer);
                    cube_report_info.block_idx++;
                } else {
                    break;
                }
            }
        }
        if(cube_report_info.block_idx >= cube_report_info.block_cnt) {
            if(semRelease) {
                semRelease = false;
                OSI_SemaphoreRelease(&cube_report_sem);
            }

        }
    }


#endif
	SET_TIME_ESCAPE(time_start, time_start);
	UPDATE_TIME_DBG_INFO(time_start, "cube-bk", g_time_dbg_info.report_block);
}


#if (CONFIG_HIF_APP_SPEC_POOL == 0)
/* the following function is not used */
__sram_text static bool is_cube_report_frame_finish(void)
{
	return (cube_report_info.block_idx && cube_report_info.finish_idx >= cube_report_info.block_idx);
}

void cube_report_retry_callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status);
__sram_text static int hif_dma_cube_report_retry_send(uint32_t retry_offset, uint8_t* cubeBuffer)
{
    uint8_t isFirstFrag = 0;

    if (retry_offset == 0) {
        isFirstFrag = 1;
    }
    if (isFirstFrag == 0) {
        retry_offset -= (CUBE_HEAD_LEN - sizeof(MMW_FRAME_TL));
    }

    uint16_t retry_range_idx = (retry_offset / 4) % cube_report_info.range_fft_num;
    uint16_t retry_dop = ((retry_offset / 4) - retry_range_idx) / cube_report_info.range_fft_num;
    uint16_t retry_dop_idx = retry_dop % cube_report_info.doppler_fft_num;
    uint16_t retry_rx = (retry_dop - retry_dop_idx) / cube_report_info.doppler_fft_num;
    uint8_t retry_rx_idx = retry_rx % cube_report_info.rx_cnt;
    uint16_t retry_tx = (retry_rx - retry_rx_idx) / cube_report_info.rx_cnt;
    uint8_t retry_tx_idx = retry_tx % cube_report_info.tx_cnt;

    MMW_FRAME_UPLOAD retry_report_hdr;
    MMW_FRAME_TL retry_report_tl;
	uint8_t *cube_data = NULL;
    uint8_t cube_head_len = 0;
	uint16_t read_num = 0;
	uint32_t offset = 0;
	uint16_t read_cnt = 0;
    uint32_t cube_size_max = 0;

    if(isFirstFrag) {
        cube_head_len = CUBE_HEAD_LEN;
        cube_size_max = cube_report_cache_block_size_max;
    } else {
        cube_head_len = CUBE_HEAD_LEN - sizeof(MMW_FRAME_TL);
        cube_size_max = cube_report_cache_block_size_max - sizeof(MMW_FRAME_TL);
    }
    offset = cube_head_len;

	cube_data = cubeBuffer;

    for(uint32_t i = 0; i < cube_report_info.tx_cnt*cube_report_info.rx_cnt*cube_report_info.doppler_fft_num; i++) {
        if((cube_size_max - offset) >= 4 * (cube_report_info.range_fft_num - retry_range_idx)) {
            read_num = cube_report_info.range_fft_num - retry_range_idx;
        } else {
            read_num = (cube_size_max - offset) / 4;
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
        read_cnt = (offset - cube_head_len) / 4;
        retry_report_hdr.frame_len = cube_report_info.report_hdr.frame_len;
        if (isFirstFrag) {
            retry_report_hdr.offset = 0;
        } else {
            retry_report_hdr.offset = retry_offset + (CUBE_HEAD_LEN - sizeof(MMW_FRAME_TL));
        }
        retry_report_hdr.frame_idx = cube_report_info.report_hdr.frame_idx;
        if(offset == (cube_size_max) || (retry_report_hdr.frame_len - retry_report_hdr.offset - 4 * read_cnt) == 0) {
            memcpy(cube_data, &retry_report_hdr, sizeof(MMW_FRAME_UPLOAD));
            if(isFirstFrag) {
                retry_report_tl.type = 0;
                retry_report_tl.length = cube_report_info.report_tl.length;     // Keep total length
                retry_report_tl.rx_num = cube_report_info.rx_cnt;
                retry_report_tl.tx_num = cube_report_info.tx_cnt;
                retry_report_tl.range_bin_num = cube_report_info.range_fft_num;
                retry_report_tl.dop_bin_num = cube_report_info.doppler_fft_num;
                memcpy(&cube_data[12], &retry_report_tl, sizeof(MMW_FRAME_TL));
            }

            return HIF_MsgReport(0xC2, cube_data, offset, cube_report_retry_callback);
        }
    }
	return -1;
}

__sram_text void cube_report_retry_callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
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
		uint32_t read_num = MIN(cube_report_cache_block_size, cube_retry_info.retry_cfg->retry[retry_arr_idx].len);
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

__sram_text void cube_report_retry_handler(uint8_t *data, uint32_t retry_num)
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
        for(int i = 0; i < cube_report_cache_block_num; i++) {
            if (retry_num) {
				uint32_t offset = cube_retry_info.retry_cfg->retry[retry_arr_idx].offset + cube_retry_info.retry_offset;
                uint32_t dataLen = 0;
                if (offset == 0) {      // first frag
                    dataLen = cube_retry_info.retry_cfg->retry[retry_arr_idx].len - sizeof(MMW_FRAME_TL);
                } else {
                    dataLen = cube_retry_info.retry_cfg->retry[retry_arr_idx].len;
                }
				uint32_t read_num = MIN(cube_report_cache_block_size, dataLen);
				if (read_num && (offset + read_num <= frame_len)) {
                    uint8_t *buffer = hif_MEM_AppNodeMalloc(0);
                    if(buffer) {
                        hif_dma_cube_report_retry_send(offset, buffer);
                        cube_retry_info.retry_block_idx ++;
                        cube_retry_info.retry_offset += read_num;
                        if (cube_retry_info.retry_offset == dataLen) {
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

#endif

#endif /* ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0)) */

#if CONFIG_RADAR_FRAMEWORK_REPORT_HEART_BEAT
#define DEFAULT_HEARTBEAT_REPORT_PERIOD_S	(10 * 60)

uint32_t s_heartBeatPeriod_ms = DEFAULT_HEARTBEAT_REPORT_PERIOD_S * 1000;		/* Default 10 minutes per heartbeat message */
uint8_t s_pHeartBeat[HEART_BEAT_TYPE_MAX * 6];		/* MAX 6 Bytes for each heart beat types TLV supports uint32_t */

__sram_text static void radar_framework_heartBeat_sent_cb(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
{
}

__sram_text uint32_t radar_framework_heartBeat_period_set(uint32_t period_ms)
{
	uint32_t ret = s_heartBeatPeriod_ms;
    s_heartBeatPeriod_ms = period_ms;
	return ret;
}

__sram_text void radar_framework_report_heartBeat(uint16_t motion_pt_num, uint16_t presence_pt_num)
{
    uint32_t reportCnt = 0;
    uint16_t reportLen = 0;
    uint32_t framePeriod = 0;
    uint32_t frame_num = 0;
    static uint32_t count = 0;

    do {
        if(s_heartBeatPeriod_ms == 0) {
            count = 0;
            break;
        }

        mmw_frame_get(&framePeriod, &frame_num);
        if(framePeriod == 0) {
            break;
        }

        reportCnt = s_heartBeatPeriod_ms / framePeriod;

        if(reportCnt == 0) {
            break;
        }

        if(count >= reportCnt) {
            count = 0;
			uint32_t responseCnt = hif_msg_response_cnt_get();
			s_pHeartBeat[reportLen] = RESPONSE_CNT;
			reportLen += 1;
			s_pHeartBeat[reportLen] = 4;
			reportLen += 1;
			memcpy(&s_pHeartBeat[reportLen], &responseCnt, 4);
			reportLen += 4;

			uint32_t frameIndex = radar_framework_frame_idx_get();
			s_pHeartBeat[reportLen] = FRAME_INDEX;
			reportLen += 1;
			s_pHeartBeat[reportLen] = 4;
			reportLen += 1;
			memcpy(&s_pHeartBeat[reportLen], &frameIndex, 4);
			reportLen += 4;

			s_pHeartBeat[reportLen] = MOTION_PT_NUM;
			reportLen += 1;
			s_pHeartBeat[reportLen] = sizeof(motion_pt_num);
			reportLen += 1;
			memcpy(&s_pHeartBeat[reportLen], &motion_pt_num, sizeof(motion_pt_num));
			reportLen += sizeof(motion_pt_num);

			s_pHeartBeat[reportLen] = PRESENT_PT_NUM;
			reportLen += 1;
			s_pHeartBeat[reportLen] = sizeof(presence_pt_num);
			reportLen += 1;
			memcpy(&s_pHeartBeat[reportLen], &presence_pt_num, sizeof(presence_pt_num));
			reportLen += sizeof(presence_pt_num);

			int ret = HIF_MsgReport(0xFE, s_pHeartBeat, reportLen, radar_framework_heartBeat_sent_cb);
			if(ret) {
				LOG_ERR("Add for heartbeat buffer failed: %d\n", ret);
			}
        }

        count ++;
    } while(0);
}

#endif /* CONFIG_RADAR_FRAMEWORK_REPORT_HEART_BEAT */

__sram_text static void radar_framework_hif_sent_cb(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
{
	;
}

__sram_text static int radar_framework_debug_proto_report_add_payload_list(HIF_Data_ListHead_t *pDataListHead, char *ptr_channel_name, uint8_t dim, uint16_t len, PsicDbgProtoPayloadDataFormat_e data_format, uint8_t signed_flag, uint8_t align_mode, uint8_t q_value, uint8_t *header, void *ptr_data)
{
#define GEN_WIDTH_BITS(x)	((x) & 0x03)
#define GEN_SIGN_BITS(x)	(((x) & 0x01) << 2)
#define GEN_Q_BITS(x)		(((x) & 0x1f) << 3)
#define GEN_ALIGN_MODE(x)	((x) & 0x01)

	PsicDegProtoHeader_t payload_header;
	uint32_t total_data_size;
	if (ptr_channel_name) {
        payload_header.len = len;
        payload_header.dim = dim;
        payload_header.align_mode = align_mode;
        payload_header.payload_data_format = data_format;
        if (signed_flag) {
            payload_header.payload_data_sign = PSIC_DBG_PROTO_DATA_SIGNED;
        } else {
            payload_header.payload_data_sign = PSIC_DBG_PROTO_DATA_UNSIGNED;
        }
        payload_header.q_format = q_value;
		payload_header.dim = payload_header.dim ? payload_header.dim : 1;
        switch (data_format) {
            case PSIC_DBG_PROTO_DATA_FORMAT_BYTE:
                total_data_size = len;
                break;
            case PSIC_DBG_PROTO_DATA_FORMAT_SHORT:
                total_data_size = len * 2;
                break;
            case PSIC_DBG_PROTO_DATA_FORMAT_LONG:
                total_data_size = len * 4;
                break;
            case PSIC_DBG_PROTO_DATA_FORMAT_FLOATING:
                total_data_size = len * 4;
                break;
            default:
                printf("error: unsupported type\r\n");
                return -2;
                break;
        }
		total_data_size = total_data_size * payload_header.dim;
		header[0] = payload_header.dim;
		header[1] = GEN_WIDTH_BITS(payload_header.payload_data_format) |
						GEN_SIGN_BITS(payload_header.payload_data_sign) |
						GEN_Q_BITS(payload_header.q_format);
		header[2] = GEN_ALIGN_MODE(payload_header.align_mode);
		header[3] = payload_header.len & 0xff;
		header[4] = payload_header.len >> 8;
        int status = 0;
		status = HIF_MsgReport_ListPushData(pDataListHead, header, 5);
        if (status != 0) {
            return status;
        }
		status = HIF_MsgReport_ListPushData(pDataListHead, ptr_channel_name, strlen(ptr_channel_name) + 1);
        if (status != 0) {
            return status;
        }
        if(ptr_data) {
            status = HIF_MsgReport_ListPushData(pDataListHead, ptr_data, total_data_size);
            if (status != 0) {
                return status;
            }
        }

		return status;
	} else {
		return -1;
	}
}

__sram_text static int8_t radar_framework_report_debug_data(void *ptr_data, char *ptr_channel_name, uint8_t *header, uint8_t dim, uint16_t len, PsicDbgProtoPayloadDataFormat_e data_format, uint8_t signed_flag, uint8_t align_mode, uint8_t q_value, HIF_MsgTrans_Callback sentCb) {
	int8_t ret = HIF_ERRCODE_NOT_READY;

    HIF_Data_ListHead_t DataListHead = {NULL, NULL};

	if (ptr_channel_name)
	{
		ret = radar_framework_debug_proto_report_add_payload_list(&DataListHead, ptr_channel_name, dim, len, data_format, signed_flag, align_mode, q_value, header, ptr_data);
        if (ret == 0) {
            ret = HIF_MsgReport_ListStart(0xC6, &DataListHead, sentCb);	/* if need use non-blocking mode,should configure the callback function */
        }
	}
	if ((ret != 0) || (sentCb == NULL)) { /* return 0 indicates that the HIF task has been added successfully,otherwise the memory needs to be released */
        uint8_t *pBuff = NULL;
        do {
            pBuff =HIF_MsgReport_ListPopData(&DataListHead);
            if (pBuff == NULL) {
                break;
            }
        } while (1);
	}
	return ret;
}

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
#if ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0))
void radar_framework_report_param_get(void)
{
	int ret = 0;
	MmwPsicMimoRxNum_t mimo_info;

	D_State_test.proc_frame_num = 0;
    mmw_psic_lib_sdk_get_tx_rx_num(&mimo_info);
	cube_report_info.tx_cnt = mimo_info.ant_tx_num;
	cube_report_info.rx_cnt = mimo_info.ant_rx_num;

	ret = mmw_fft_num_get(&cube_report_info.range_fft_num, &cube_report_info.doppler_fft_num);
	if(ret) {
		LOG_PRINT("fft num get fail ! ret = %d \r\n", ret);
		return;
	}

	uint32_t frame_len = cube_report_info.tx_cnt * cube_report_info.rx_cnt *
	                     cube_report_info.range_fft_num * cube_report_info.doppler_fft_num;
	cube_report_info.block_cnt = (frame_len * 4 + cube_report_cache_block_size - 1) / cube_report_cache_block_size;
	cube_report_info.frame_cb_cnt = 0;
	cube_report_info.report_hdr.frame_len = frame_len * 4 + sizeof(MMW_FRAME_TL);
	cube_report_info.report_hdr.frame_idx = 0;
    cube_report_info.report_tl.type = 0;
    cube_report_info.report_tl.length = frame_len * 4 + 8;
    cube_report_info.report_tl.rx_num = cube_report_info.rx_cnt;
    cube_report_info.report_tl.tx_num = cube_report_info.tx_cnt;
    cube_report_info.report_tl.range_bin_num = cube_report_info.range_fft_num;
    cube_report_info.report_tl.dop_bin_num = cube_report_info.doppler_fft_num;
	cube_report_frame_reset();
}

__sram_text int radar_framework_ctrl_data_cube_report_cb(void *mmw_data, void *arg)
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

    OSI_SemaphoreWait(&cube_report_sem, 0);
	/* reset report status */
	cube_report_frame_reset();
    cube_access_ctrl(true);

    cube_report_info_backup = cube_report_info;

    uint32_t com_type = 0x00;
    HIF_ExtControl(HIF_GET_COM_TYPE, &com_type, sizeof(com_type));
    uint32_t sendNodeLevel = MIN(cube_report_info.block_cnt, cube_report_cache_block_num);
#if (CONFIG_HIF_APP_SPEC_POOL == 0)
    sendNodeLevel = MIN(4, sendNodeLevel);
#endif
    if (com_type == HIF_COM_TYPE_SPI) {
        HIF_ExtControl(HIF_SET_SEND_NODE_LEVEL, &sendNodeLevel, sizeof(sendNodeLevel));
    }

#if (CONFIG_HIF_APP_SPEC_POOL == 0)
    for(int i = 0; i < cube_report_cache_block_num; i++) {
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
#else
    if(hif_MEM_AppNodeListIsFull(0)) {
        for(int i = 0; i < cube_report_cache_block_num; i++) {
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
    }
#endif

    if (com_type == HIF_COM_TYPE_SPI) {
        sendNodeLevel = 0;
        HIF_ExtControl(HIF_SET_SEND_NODE_LEVEL, &sendNodeLevel, sizeof(sendNodeLevel));
    }

	/* 500us is margin */
	cube_report_info.next_frame_time = time_now + US_TO_CORET(cube_report_info.report_time_ms*1000 - 500);
	if (cube_report_cache_block_num < cube_report_info.block_cnt) {
        semRelease = true;
		ret = OSI_SemaphoreWait(&cube_report_sem, cube_report_info.report_time_ms);
		if(ret != OSI_STATUS_OK) {
            semRelease = false;
			cube_report_info.aborted = true;
		}
	}
	cube_access_ctrl(false);

	SET_TIME_ESCAPE(time_now, time_now);
	UPDATE_TIME_DBG_INFO(time_now, "report", g_time_dbg_info.report);

	return ret;
}


void radar_framework_data_report_init(uint32_t block_num, uint32_t block_size)
{
	cube_report_cache_block_num = block_num;
	cube_report_cache_block_size = block_size;
	cube_report_cache_block_size_max = block_size + CUBE_HEAD_LEN;
#if (CONFIG_HIF_APP_SPEC_POOL == 0)
    HIF_AppPool_Cfg_t appPoolCfg = {cube_report_cache_block_num, cube_report_cache_block_size_max, NULL};

    int ret = hif_MEM_AppPoolInit(&appPoolCfg, 1);
    if(ret) {
        LOG_ERR("cube app malloc fail! \r\n");
    } else {
        printf("cube pool malloc success \r\n");
    }
#else
    cube_report_cache_block_num = RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT;
	cube_report_cache_block_size_max = RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE + CUBE_HEAD_LEN;

    HIF_AppPool_Cfg_t appPoolCfg[2] = {{cube_report_cache_block_num, cube_report_cache_block_size_max, (uint8_t *)CONFIG_CUBE_BUFFER_A},
                                        {cube_report_cache_block_num, cube_report_cache_block_size_max, (uint8_t *)CONFIG_CUBE_BUFFER_B}};

    int ret = hif_MEM_AppPoolInit(appPoolCfg, 2);
    if(ret) {
        LOG_ERR("cube app malloc fail! \r\n");
    }

#endif
    OSI_SemaphoreCreate(&cube_report_sem, 0, 1);
}

void radar_framework_data_cube_report_deinit(void)
{
    cube_report_info.aborted = true;

    hif_MEM_AppPoolDeinit();

    OSI_SemaphoreDelete(&cube_report_sem);

    memset(&cube_report_info, 0, sizeof(struct cube_report_info_t));
}

#endif /* ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0)) */


void radar_framework_report_frame_config(void)
{
	int8_t ret = 0;
	LOG_PRINT("\r\n/--------Print Radar Frame configurs--------/\r\n");
	uint8_t txrx_mode, work_mode;
	ret = mmw_mode_get(&txrx_mode, &work_mode);
	if (ret == 0) {
		LOG_PRINT("txrx_mode:%d, work_mode:%d\n",txrx_mode, work_mode);
	} else {
		LOG_ERR("call mmw_mode_get() fail: %d\r\n", ret);
	}

	uint32_t range_mm, resol_mm;
	ret = mmw_range_get(&range_mm, &resol_mm);
	if (ret == 0) {
		LOG_PRINT("max_det_range_mm:%d,range_rel:%d\n",range_mm, resol_mm);
	} else {
		LOG_ERR("call mmw_range_get() fail: %d\r\n", ret);
	}

	uint32_t velocity_mm, veloc_resol;
	ret = mmw_velocity_get(&velocity_mm, &veloc_resol);
	if (ret == 0) {
		LOG_PRINT("velocity_mm:%d,veloc_resol:%d\n",velocity_mm, veloc_resol);
	} else {
		LOG_ERR("call mmw_velocity_get() fail: %d\r\n", ret);
	}

	uint32_t period_ms, frame_num;
	ret = mmw_frame_get(&period_ms, &frame_num);
	if (ret == 0) {
		LOG_PRINT("period_ms:%d,frame_num:%d\n",period_ms, frame_num);
	} else {
		LOG_ERR("call mmw_frame_get() fail: %d\r\n", ret);
	}

	uint32_t start_MHz, max_MHz;
	ret = mmw_freq_get(&start_MHz, &max_MHz);
	if (ret == 0) {
		LOG_PRINT("start_MHz:%d\n",start_MHz);
	} else {
		LOG_ERR("call mmw_freq_get() fail: %d\r\n", ret);
	}

	uint16_t rfft_num, dfft_num;
	ret = mmw_fft_num_get(&rfft_num, &dfft_num);
	if (ret == 0) {
		LOG_PRINT("range fft num:%d, doppler fft num: %d\n",rfft_num, dfft_num);
	} else {
		LOG_ERR("call mmw_fft_num_get() fail: %d\r\n", ret);
	}

	uint8_t dfft_en;
	dfft_en = mmw_dop_fft_get();
	LOG_PRINT("doppler fft enabled: %d\r\n", dfft_en);

	LOG_PRINT("HEAP size allocated: %dKB\r\n", CONFIG_HEAP_SIZE >> 10);
	LOG_PRINT("/--------Print Radar Frame Configures End--------/\r\n\r\n");

}

#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT) || (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
static uint8_t* g_point_cloud_report_buffer = 0;

int radar_framework_point_cloud_report_init(void) {
	uint8_t ret = MMW_ERR_CODE_SUCCESS;

	uint32_t max_report_buffer_len = 0; /* the totoal length of the data that needs to be repoerted, in units of bytes */
	max_report_buffer_len += sizeof(MMW_FRAME_UPLOAD);

#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT)
	max_report_buffer_len += (sizeof(PointCloud_Cart) * mmw_point_cloud_get_user_cfg_const()->mmw_motion_point_cloud_config.motion_point_cloud_max_cfar_num + sizeof(TL_HEADER));
#endif

#if (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
	max_report_buffer_len += (sizeof(PointCloud_Cart) * PRESENCE_POINT_MAX + sizeof(TL_HEADER));
#endif

	if (g_point_cloud_report_buffer) {
		OSI_Free(g_point_cloud_report_buffer);
	}
	g_point_cloud_report_buffer = OSI_Malloc(sizeof(*g_point_cloud_report_buffer) * max_report_buffer_len);
	if (g_point_cloud_report_buffer == 0) {
		ret = MMW_ERR_CODE_NO_MEMORY;
		LOG_ERR("g_point_cloud_report_buffer malloc FAILED!\r\n");
	}
	return ret;
}

void radar_framework_point_cloud_report_deinit(void) {
	if (g_point_cloud_report_buffer) {
		OSI_Free(g_point_cloud_report_buffer);
		g_point_cloud_report_buffer = 0;
	}
}
#endif
#if (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
/**
 * @brief Convert the presence point cloud computing results into the standard format for reporting.
 *
 * This function handles the conversion of presence-specific point cloud data,
 * which differs from motion point cloud data based on the detection algorithm used.
 *
 * @param ptr_report_buffer Pointer to the reporting format for storing standard motion point cloud.
 * @param ptr_presence_point_cloud_buffer Pointer to the source the results of presence point cloud.
 *
 * @note This function is only available if CONFIG_MMW_PRESENCE_POINT_CLOUD and CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT is enabled.
 *
 */
__sram_text static void radar_framework_add_presence_point_cloud_report_target(PointCloud_Cart* ptr_report_buffer, const PresencePointCloudBuffer_t *ptr_presence_point_cloud_buffer)
{
	float sin_y, sin_z, sin_x; /* x, y, z, vel, */
	uint32_t range_mm, range_reol_mm;
	uint16_t range_fft_num, doppler_fft_num;
	float range_bin_size_cm, micro_veloc_uint_cm;
	float range_cm, veloc_cm;

	float user_coordinate[3];

	uint32_t frame_period, dummy;

	mmw_range_get(&range_mm, &range_reol_mm);
	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);

	range_bin_size_cm = (float)range_mm / range_fft_num * 0.1f;
	

	uint32_t micro_dop_num = mmw_micro_doppler_num_get();
	int16_t doppler_idx_half = micro_dop_num >> 1;
	mmw_frame_get(&frame_period, &dummy);
	frame_period = frame_period * mmw_micro_frame_rate_get();
	micro_veloc_uint_cm = (float)(5 * 100) / (frame_period * micro_dop_num);

	uint32_t report_presence_point_cloud_num = 0;
	uint32_t presence_point_num = ptr_presence_point_cloud_buffer->presence_point_cloud_num;

	for (uint16_t presence_pc_idx = 0; presence_pc_idx < presence_point_num; presence_pc_idx++) {
		/*
		 * unit cm in D_data_test structure,
		 * x = range * sin(x)
		 * y = range * sqrt(1-sin(x)^2-sin(z)^2)
		 * z = range * sin(z)
		*/
		range_cm = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].range_idx * range_bin_size_cm;
		veloc_cm = (ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].doppler_idx - doppler_idx_half) * micro_veloc_uint_cm;
		sin_x = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].azi_phase;
		sin_z = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].ele_phase;
		sin_y = 1.f - sin_x * sin_x - sin_z * sin_z;
		sin_y = sqrtf(sin_y);
		mmw_point_cloud_trans_radar_coord_to_user_coord(range_cm * sin_x,
							   range_cm * sin_y,
							   range_cm * sin_z,
							   user_coordinate
							);

		ptr_report_buffer[report_presence_point_cloud_num].x = user_coordinate[0];
		ptr_report_buffer[report_presence_point_cloud_num].y = user_coordinate[1];
		ptr_report_buffer[report_presence_point_cloud_num].z = user_coordinate[2];
		ptr_report_buffer[report_presence_point_cloud_num].vel = veloc_cm;
		ptr_report_buffer[report_presence_point_cloud_num].snr = ptr_presence_point_cloud_buffer->ptr_presence_point_cloud_data[presence_pc_idx].sig_snr * 100;
		report_presence_point_cloud_num++;
	}
}
#endif

#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT)
/**
 * @brief Convert the motion point cloud computing results into the standard format for reporting.
 *
 * This function takes the point cloud buffer generated by the radar framework's
 * motion detection algorithms and convert it into the standard reporting format.
 *
 * @param ptr_report_buffer Pointer to the reporting format for storing standard motion point cloud.
 * @param ptr_point_cloud_buffer Pointer to the source the results of motion point cloud.
 *
 */
__sram_text static void radar_framework_add_motion_point_cloud_report_target(PointCloud_Cart* ptr_report_buffer, const PointCloudBuffer_t *ptr_point_cloud_buffer)
{
	float sin_y, sin_z, sin_x; /* x, y, z, vel, */
    float user_coordinate[3];
    uint32_t range_mm, range_reol_mm, velocity_mm, veloc_resol_mm;
    uint16_t range_fft_num, doppler_fft_num;
    float range_bin_size_cm, dop_bin_size_cm;
    int16_t doppler_idx_half;
	uint32_t report_motion_point_cloud_num = 0;
	MmwClusterDetectData_t ptr_point_cloud_data;
	uint32_t motion_point_num = ptr_point_cloud_buffer->point_cloud_num;

	mmw_range_get(&range_mm, &range_reol_mm);
	mmw_velocity_get(&velocity_mm, &veloc_resol_mm);
	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	range_bin_size_cm = (float)range_mm / range_fft_num * 0.1f;
	dop_bin_size_cm = (float)velocity_mm / doppler_fft_num * 0.2f; /* velocity_mm is single size */
	doppler_idx_half = doppler_fft_num >> 1;

	for (uint16_t point_cloud_idx = 0; point_cloud_idx < motion_point_num; point_cloud_idx++) {
		ptr_point_cloud_data = ptr_point_cloud_buffer->ptr_motion_point_cloud_data[point_cloud_idx];

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
		sin_x = ptr_point_cloud_data.azi_phase;
		sin_z = ptr_point_cloud_data.ele_phase;
		sin_y = 1.f - sin_x * sin_x - sin_z * sin_z;
		sin_y = sqrtf(sin_y);
		mmw_point_cloud_trans_radar_coord_to_user_coord(ptr_point_cloud_data.range_idx * sin_x * range_bin_size_cm,
							   ptr_point_cloud_data.range_idx * sin_y * range_bin_size_cm,
							   ptr_point_cloud_data.range_idx * sin_z * range_bin_size_cm,
							   user_coordinate
							);
		ptr_report_buffer[report_motion_point_cloud_num].x = user_coordinate[0];
		ptr_report_buffer[report_motion_point_cloud_num].y = user_coordinate[1];
		ptr_report_buffer[report_motion_point_cloud_num].z = user_coordinate[2];
		ptr_report_buffer[report_motion_point_cloud_num].vel = (ptr_point_cloud_data.doppler_idx - doppler_idx_half) * dop_bin_size_cm;
		ptr_report_buffer[report_motion_point_cloud_num].snr = ptr_point_cloud_data.sig_snr * 100;
		report_motion_point_cloud_num++;
	}
}
#endif

#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT) || (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
__sram_text int radar_framework_report_point_cloud_data(const PointCloudBuffer_t *ptr_point_cloud_buffer, const PresencePointCloudBuffer_t *ptr_presence_point_cloud_buffer, uint32_t frame_idx)
{
	uint32_t frame_len = 0; /* the total length of the current frame point cloud data, in units of DWORD(4 bytes) */
	uint32_t report_buffer_len = 0; /* the totoal length of the data that needs to be repoerted, in units of bytes */

#if !(CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT) /* only presence point cloud reporting */
	if (!ptr_presence_point_cloud_buffer) { /* if there are no presence point cloud calculated for the current frame, then ptr_presence_point_cloud_buffer equals 0 */
		/*
		 * to distinguish the current frame, for the case where there are not existing presence point cloud target and
		 * no calculated the presence point cloud, when the current frame does not calculate the presence point cloud,
		 * will not repoerts.
		 */
		return 0;
	}
#endif

	report_buffer_len += sizeof(MMW_FRAME_UPLOAD);

#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT)
	frame_len += ((sizeof(PointCloud_Cart) * ptr_point_cloud_buffer->point_cloud_num + sizeof(TL_HEADER))>>2);
	report_buffer_len += (sizeof(PointCloud_Cart) * ptr_point_cloud_buffer->point_cloud_num + sizeof(TL_HEADER));
#endif

#if (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
	if (ptr_presence_point_cloud_buffer) { /* if there are no presence point cloud calculated for the current frame, then ptr_presence_point_cloud_buffer equals 0 */
		frame_len += ((sizeof(PointCloud_Cart) * ptr_presence_point_cloud_buffer->presence_point_cloud_num + sizeof(TL_HEADER))>>2);
		report_buffer_len += (sizeof(PointCloud_Cart) * ptr_presence_point_cloud_buffer->presence_point_cloud_num + sizeof(TL_HEADER));
	}
#endif

	uint8_t *buffer = g_point_cloud_report_buffer;
	MMW_FRAME_UPLOAD frame_hdr;
	/* according to the HIF protocol, set frame_hdr */
	frame_hdr.frame_idx = frame_idx;
	frame_hdr.frame_len = frame_len;
	frame_hdr.offset	 = 0;
	memcpy(buffer, &frame_hdr, sizeof(MMW_FRAME_UPLOAD));
	buffer += sizeof(MMW_FRAME_UPLOAD);

	/* according to the HIF protocol, set up the application layer for motion point cloud data */
#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT)
		TL_HEADER tl;
		tl.type    = MMW_TL_TYPE_POINTS;
		tl.flag    = MMW_TL_FLAG_CART;
		tl.length = sizeof(PointCloud_Cart) * ptr_point_cloud_buffer->point_cloud_num;
		memcpy(buffer, &tl, sizeof(TL_HEADER));
		buffer += sizeof(TL_HEADER);
		/* convert the motion point cloud computing results into the reporting format and store them in the reporting buffer */
		radar_framework_add_motion_point_cloud_report_target((PointCloud_Cart*)buffer, ptr_point_cloud_buffer);
		buffer += (sizeof(PointCloud_Cart) * ptr_point_cloud_buffer->point_cloud_num);
#endif

	/* according to the HIF protocol, set up the application layer for presence point cloud data */
#if (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
	if (ptr_presence_point_cloud_buffer) {
		TL_HEADER tl;
		tl.type    = MMW_TL_TYPE_POINTS;
		tl.flag    = MMW_TL_FLAG_CART|MMW_TL_FLAG_MICRO;
		tl.length  = sizeof(PointCloud_Cart) * ptr_presence_point_cloud_buffer->presence_point_cloud_num;
		memcpy(buffer, &tl, sizeof(TL_HEADER));
		buffer += sizeof(TL_HEADER);
		/* convert the presence point cloud computing results into the reporting format and store them in the reporting buffer */
		radar_framework_add_presence_point_cloud_report_target((PointCloud_Cart*)buffer, ptr_presence_point_cloud_buffer);
		buffer += (sizeof(PointCloud_Cart) * ptr_presence_point_cloud_buffer->presence_point_cloud_num);
	}
#endif

	int status = HIF_MsgReport(0xC3, g_point_cloud_report_buffer, sizeof(*g_point_cloud_report_buffer) * report_buffer_len, radar_framework_hif_sent_cb);
	if (status) {
		LOG_ERR("send 0xC3 data fail: %d\n", status);
		radar_framework_hif_sent_cb(0, 0, 0);
	}
	return status;
}
#endif


__sram_text static int radar_framework_report_cube_split(HIF_Data_ListHead_t* data_list_hdr, MMW_FRAME_UPLOAD *report_hdr, MMW_FRAME_TL *tl, uint8_t *payload, uint32_t len, uint32_t* offset)
{
    int status = 0;

    do {
        status = HIF_MsgReport_ListPushData(data_list_hdr, report_hdr, sizeof(MMW_FRAME_UPLOAD));
        if (status != 0) {
            break;
        }
    	if (report_hdr->offset == 0) {
    		status = HIF_MsgReport_ListPushData(data_list_hdr, tl, sizeof(MMW_FRAME_TL));
            if (status != 0) {
                break;
            }
        }
    	if (len) {
    		status = HIF_MsgReport_ListPushData(data_list_hdr, payload, len);
            if (status != 0) {
                break;
            }
        }
        status = HIF_MsgReport_ListStart(0xC2, data_list_hdr, radar_framework_hif_sent_cb);
        if (status != 0) {
            break;
        }
    	if (report_hdr->offset == 0) {
			*offset = report_hdr->offset + sizeof(MMW_FRAME_TL);
    	}
        if(len) {
            *offset += len;
        }
    } while (0);

	if (status) {
		uint8_t *pBuff = NULL;
		do {
			pBuff = HIF_MsgReport_ListPopData(data_list_hdr);
			if (pBuff == NULL) {
				break;
			}
		} while (1);
	}
	return status;
}

#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN || CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE || CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN
/* Buffer set to 25 */
#define RADAR_FRAMEWORK_REPORT_CUBE_CYCLE_BUF_LEN			(25)
#define RADAR_FRAMEWORK_REPORT_CUBE_TYPE_LEN				(3)
static MMW_FRAME_UPLOAD frame_hdr_list[RADAR_FRAMEWORK_REPORT_CUBE_CYCLE_BUF_LEN];
static HIF_Data_ListHead_t list_header[RADAR_FRAMEWORK_REPORT_CUBE_CYCLE_BUF_LEN];
static uint8_t s_report_cube_buffer_idx;

static MMW_FRAME_TL s_tl_cycle_buf[RADAR_FRAMEWORK_REPORT_CUBE_TYPE_LEN];
static uint8_t s_tl_idx;

__sram_text int radar_framework_report_cube(complex16_cube *data, uint32_t frame_idx, MMW_FRAME_TL* ptr_tl) {
	uint32_t frame_len;
	uint32_t offset = 0;
	uint8_t pack_num = 0;
	int ret = 0;

	if (ptr_tl) {
		s_tl_cycle_buf[s_tl_idx] = *ptr_tl;
	} else {
		uint16_t range_fft_num, doppler_fft_num;
		MmwPsicMimoRxNum_t mimo_rx_info;

		mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
		mmw_psic_lib_sdk_get_tx_rx_num(&mimo_rx_info);

		s_tl_cycle_buf[s_tl_idx].type = 3;
		s_tl_cycle_buf[s_tl_idx].rx_num = mimo_rx_info.ant_rx_num;
		s_tl_cycle_buf[s_tl_idx].tx_num = mimo_rx_info.ant_tx_num;
		s_tl_cycle_buf[s_tl_idx].range_bin_num = range_fft_num;
		s_tl_cycle_buf[s_tl_idx].dop_bin_num = 1;
	}
	frame_len = s_tl_cycle_buf[s_tl_idx].range_bin_num * s_tl_cycle_buf[s_tl_idx].dop_bin_num * s_tl_cycle_buf[s_tl_idx].tx_num * s_tl_cycle_buf[s_tl_idx].rx_num * sizeof(complex16_cube);
	s_tl_cycle_buf[s_tl_idx].length = frame_len + 8;
	ptr_tl = &s_tl_cycle_buf[s_tl_idx];
	s_tl_idx++;
	if (RADAR_FRAMEWORK_REPORT_CUBE_TYPE_LEN <= s_tl_idx) {
		s_tl_idx = 0;
	}

	/* Setup frame header & data */
	frame_hdr_list[s_report_cube_buffer_idx].frame_idx = frame_idx;
	frame_hdr_list[s_report_cube_buffer_idx].frame_len = frame_len + sizeof(MMW_FRAME_TL);
	frame_hdr_list[s_report_cube_buffer_idx].offset	 = 0;
	uint8_t* payload = (uint8_t *)data;

	do {
		uint32_t packet_len = MIN(frame_len, POINT_DATA_SIZE);
		pack_num++;
		frame_len -= packet_len;
		list_header[s_report_cube_buffer_idx].head = 0;
		list_header[s_report_cube_buffer_idx].tail = 0;
		ret = radar_framework_report_cube_split(&list_header[s_report_cube_buffer_idx], &frame_hdr_list[s_report_cube_buffer_idx], ptr_tl, payload, packet_len, &offset);
		if (ret) {
			LOG_ERR("ADD HIF LIST FAILED!\r\n");
			break;
		}

		/* Cycling move to next buffer */
		s_report_cube_buffer_idx++;
		if (s_report_cube_buffer_idx < RADAR_FRAMEWORK_REPORT_CUBE_CYCLE_BUF_LEN) {
			memcpy(&frame_hdr_list[s_report_cube_buffer_idx], &frame_hdr_list[s_report_cube_buffer_idx - 1], sizeof(MMW_FRAME_UPLOAD));
		} else {
			s_report_cube_buffer_idx = 0;
			memcpy(&frame_hdr_list[0], &frame_hdr_list[RADAR_FRAMEWORK_REPORT_CUBE_CYCLE_BUF_LEN - 1], sizeof(MMW_FRAME_UPLOAD));
		}
		frame_hdr_list[s_report_cube_buffer_idx].offset = offset;
		payload   += packet_len;
		ptr_tl = NULL;
   } while (frame_len > 0);
	return ret;
}
#endif

#if CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO
static uint16_t g_frame_info[11];
char g_frame_info_name[] = "frame_info";
uint8_t g_frame_info_header[5] = {0};
__sram_text void radar_framework_report_frameInfo(uint16_t frame_idx, uint32_t during_time, uint16_t motion_pt_num, uint16_t presence_pt_num)
{
    HeapStats_t heapStats;
    vPortGetHeapStats(&heapStats);

	int8_t ret = 0;
	uint8_t txrx_mode, work_mode;
	ret = mmw_mode_get(&txrx_mode, &work_mode);
	if (ret) {
		LOG_ERR("call mmw_mode_get() fail: %d\r\n", ret);
	}

	uint32_t range_mm, resol_mm;
	ret = mmw_range_get(&range_mm, &resol_mm);
	if (ret) {
		LOG_ERR("call mmw_range_get() fail: %d\r\n", ret);
	}

	uint32_t velocity_mm, veloc_resol;
	ret = mmw_velocity_get(&velocity_mm, &veloc_resol);
	if (ret) {
		LOG_ERR("call mmw_velocity_get() fail: %d\r\n", ret);
	}

	uint32_t period_ms, frame_num;
	ret = mmw_frame_get(&period_ms, &frame_num);
	if (ret) {
		LOG_ERR("call mmw_frame_get() fail: %d\r\n", ret);
	}

	uint16_t rfft_num, dfft_num;
	ret = mmw_fft_num_get(&rfft_num, &dfft_num);
	if (ret) {
		LOG_ERR("call mmw_fft_num_get() fail: %d\r\n", ret);
	}

	g_frame_info[0] = frame_idx;
	g_frame_info[1] = txrx_mode;
	g_frame_info[2] = period_ms;
	g_frame_info[3] = range_mm;
	g_frame_info[4] = rfft_num;
	g_frame_info[5] = velocity_mm;
	g_frame_info[6] = dfft_num;
	g_frame_info[7] = during_time * 0.01f;		/* convert to 1LSB = 0.1ms */
	g_frame_info[8] = heapStats.xMinimumEverFreeBytesRemaining >> 10;	/* Convert to 1LSB = 1K*/
	g_frame_info[9] = motion_pt_num;
	g_frame_info[10] = presence_pt_num;

	ret = radar_framework_report_debug_data((void*)g_frame_info, g_frame_info_name, g_frame_info_header, 1, 11, PSIC_DBG_PROTO_DATA_FORMAT_SHORT, 0, 0, 0, radar_framework_hif_sent_cb);
	if (ret) {
		LOG_ERR("frame info report fail: %d\r\n", ret);
	}
}
#endif