/**
 **************************************************************************************************
 * @brief   project config define.
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
 **************************************************************************************************
 */
#define LOG_MODULE                      "RADAR_SIGNAL_HIF_HANDLER"
#define LOG_LEVEL                       1

#include "radar_framework.h"
#include "mmw_ctrl.h"
#include "hif.h"
#include "log.h"
#include "mmw_app_pointcloud.h"
#include "mmw_point_cloud_psic_lib.h"
#include "mmw_app_pointcloud_config.h"
#include "radar_framework_report.h"
#include "radar_signal_process_2d.h"
#include "radar_signal_process_1d.h"
#include <math.h>
#if CONFIG_MMW_PRESENCE_POINT_CLOUD
#include "mmw_alg_doa.h"
#include "mmw_app_micro_pointcloud.h"
#endif

/*
 * Configuration of HIF Message ID for parameter configuration and parameter reading.
 * */
#ifndef CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_CFG
#define CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_CFG        (0x60)
#endif

#ifndef CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_READ
#define CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_READ       (0x61)
#endif

// FRAME CONFIG
#define TLV_R3_DATA_BOX_CONFIG_MIMO_MODE			0x10
#define TLV_R3_DATA_BOX_CONFIG_FRAME_TYPE			0x11
#define TLV_R3_DATA_BOX_CONFIG_START_FREQ			0x12
#define TLV_R3_DATA_BOX_CONFIG_TRIGGER_RANGE		0x13
#define TLV_R3_DATA_BOX_CONFIG_RANGE_RESOLUTION		0x14
#define TLV_R3_DATA_BOX_CONFIG_MAX_VELOCITY			0x15
#define TLV_R3_DATA_BOX_CONFIG_VEL_RESOLUTION		0x16
#define TLV_R3_DATA_BOX_CONFIG_FRAME_PERIOD			0x17

#define TLV_R3_DATA_BOX_CONFIG_HEATBEAT_PERIOD		0x70

/*
 * In 1D frame mode, interval time has no meaning, but due to frame period time must be integer multiple of interval time,
 * so default DEFAULT_1D_FFT_INTERVAL_TIME_US is 100us, making period precision 0.1ms.
 */
#define DEFAULT_1D_FFT_INTERVAL_TIME_US (100)

static bool s_read_current_config_flag = false;

typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t value[1];
} strMsgTlv;

static int radar_framework_start_ctrl_handler(HIF_MsgHdr_t *msg)
{
    int ret = HIF_CMD_STATUS_SUCCESS;

    uint8_t start = *((uint8_t *)(msg + 1));

    if(start) {
       ret = radar_framework_restart_process();
    } else {
       ret = radar_framework_stop_process();
    }
    if(ret) {
        ret = HIF_CMD_STATUS_IO;
    }

    return HIF_MsgResp(msg, 0, ret);
}

static uint8_t para_check(uint32_t para, uint32_t min, uint32_t max)
{
    return (para >= min) && (para <= max);
}

/**
 * @brief Handler for single configuration messages related to R3 Data Box settings.
 *
 * This function processes TLV (Type-Length-Value) messages to configure various
 * data box parameters including MIMO mode, frame type, start frequency, trigger
 * range, range resolution, maximum velocity, velocity resolution, and frame period.
 * It validates the input parameters against a predefined function and updates the
 * global configuration structure if validation passes.
 *
 * @param msgTlv Pointer to the message structure containing the TLV type and value.
 *               The 'type' field determines which parameter to configure, and 'value'
 *               holds the raw data (often 16-bit values split across bytes).
 *
 * @return
 *   - 0 on successful configuration.
 *   - MMW_ERR_CODE_INVALID_PARAM if the global config is NULL or parameters are invalid.
 *   - HIF_CMD_STATUS_UNSUPPORT if the TLV type is not recognized.
 *
 * @note
 * - The function assumes 'ptr_global_config' is initialized before calling.
 * - Frequency and resolution values are typically 16-bit integers where 1 LSB = 1 MHz or 1 mm.
 * - Supports different SOC series (RS613X, RS624X) with specific MIMO mode constraints.
 */
static int radar_framework_single_msg_handler(strMsgTlv *msgTlv)
{
    int ret = MMW_ERR_CODE_INVALID_PARAM;
    uint32_t tmp;
	RadarConfig_t* ptr_global_config = radar_framework_get_config();
	if (ptr_global_config == 0) {
		return MMW_ERR_CODE_INVALID_PARAM;
	}
    switch(msgTlv->type) {
        case TLV_R3_DATA_BOX_CONFIG_MIMO_MODE:
#if CONFIG_SOC_SERIES_RS613X
            if (para_check(msgTlv->value[0], MMW_MIMO_1T3R, MMW_MIMO_1T3R)) {
                ptr_global_config->radar_frame_config.mimo_mode = msgTlv->value[0];
                ret = 0;
            }
#elif  CONFIG_SOC_SERIES_RS624X
            if (para_check(msgTlv->value[0], MMW_MIMO_2T4R, MMW_MIMO_2T4R)) {
                ptr_global_config->radar_frame_config.mimo_mode = msgTlv->value[0];
                ret = 0;
            }
#else
#error "Please Choose Valid SOC Series"
#endif
            break;

        case TLV_R3_DATA_BOX_CONFIG_START_FREQ:
            /* 1LSB = 1MHz */
            tmp = (msgTlv->value[1] << 8) + msgTlv->value[0];
            if (para_check(tmp, 57000, 64000)) {
                ptr_global_config->radar_frame_config.start_freq_mhz = tmp;
                ret = 0;
            }
            break;
        case TLV_R3_DATA_BOX_CONFIG_FRAME_TYPE:
#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
            if (msgTlv->value[0] == 0) {
                ret = 0;
			}
#endif
#if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN
            if (msgTlv->value[0] == 1) {
                ret = 0;
			}
#endif
			if (!ret) {
				ptr_global_config->radar_frame_config.frame_type = msgTlv->value[0];
			}
            break;
        case TLV_R3_DATA_BOX_CONFIG_TRIGGER_RANGE:
            ptr_global_config->radar_frame_config.max_range_mm = (msgTlv->value[1] << 8) + msgTlv->value[0];
			ptr_global_config->radar_frame_config.range_fft_len_log2 = UINT16_MAX;
            ret = 0;
            break;
        case TLV_R3_DATA_BOX_CONFIG_RANGE_RESOLUTION:
            ptr_global_config->radar_frame_config.range_resolution_mm = (msgTlv->value[1] << 8) + msgTlv->value[0];
            ret = 0;
            break;
        case TLV_R3_DATA_BOX_CONFIG_MAX_VELOCITY:
            ptr_global_config->radar_frame_config.max_unambigous_vel_mm = (msgTlv->value[1] << 8) + msgTlv->value[0];
			ptr_global_config->radar_frame_config.dopper_fft_len_log2 = UINT16_MAX;
            ret = 0;
            break;
        case TLV_R3_DATA_BOX_CONFIG_VEL_RESOLUTION:
            ptr_global_config->radar_frame_config.vel_resolution_mm = (msgTlv->value[1] << 8) + msgTlv->value[0];
            ret = 0;
            break;
        case TLV_R3_DATA_BOX_CONFIG_FRAME_PERIOD:
            ptr_global_config->radar_frame_config.frame_period_ms = (msgTlv->value[1] << 8) + msgTlv->value[0];
            ret = 0;
            break;
#if (CONFIG_RADAR_FRAMEWORK_REPORT_HEART_BEAT)
        case TLV_R3_DATA_BOX_CONFIG_HEATBEAT_PERIOD:
            uint32_t heartBeatPeriod = (msgTlv->value[3] << 24) + (msgTlv->value[2] << 16) + (msgTlv->value[1] << 8) + msgTlv->value[0];
            radar_framework_heartBeat_period_set(heartBeatPeriod * 1000);
            ret = 0;
            break;
#endif
        default:
            ret = HIF_CMD_STATUS_UNSUPPORT;
            break;
    }

    return ret;
}

static int radar_framework_config_handler(HIF_MsgHdr_t *msg)
{
    int ret = 0;
    uint16_t offset = 0;
    uint8_t *payload = (uint8_t *)(msg + 1);
    strMsgTlv *msgTlv = NULL;

    do{
        msgTlv = (strMsgTlv *)(&payload[offset]);
        ret = radar_framework_single_msg_handler(msgTlv);
        if (ret != 0) {
            break;
        }
        offset += (msgTlv->len + 2);
    }while(offset < msg->length);

	/* After all hif config finished, calculate range_fft and doppler_ffft number satisfy power of 2 */


    return HIF_MsgResp(msg, 0, ret);
}

static int radar_framework_report_handler(HIF_MsgHdr_t *msg)
{
    int ret = 0;
    strMsgTlv *msgTlv = (strMsgTlv *)(msg + 1);

    s_read_current_config_flag = true;
    ret = radar_framework_single_msg_handler(msgTlv);
    s_read_current_config_flag = false;

    return HIF_MsgResp(msg, msgTlv->len+2 , ret);
}

int radar_framework_regist_hif_config_callback(MMW_HIF_CONFIG hif_config_handler, MMW_HIF_CONFIG hif_report_handler)
{
    int status = 0;

    status = HIF_MsgHdl_Regist(HIF_MSG_ID_START_CTRL, radar_framework_start_ctrl_handler);
    if (status) {
        LOG_ERR("register HIF_MSG_ID_START_CTRL failed\r\n");
        goto EXIT;
    }
	if (hif_config_handler) {
		status = HIF_MsgHdl_Regist(CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_CFG, hif_config_handler);
	} else {
		status = HIF_MsgHdl_Regist(CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_CFG, radar_framework_config_handler);
	}
    if (status) {
        LOG_ERR("register CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_CFG failed\r\n");
        goto EXIT;
    }

	if (hif_report_handler) {
		status = HIF_MsgHdl_Regist(CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_READ, hif_report_handler);
	} else {
		status = HIF_MsgHdl_Regist(CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_READ, radar_framework_report_handler);
	}
    if (status) {
        LOG_ERR("register CONFIG_HIF_MSG_ID_RADAR_FRAMEWORK_PARA_READ failed\r\n");
        goto EXIT;
    }

EXIT:
    return status;
}
