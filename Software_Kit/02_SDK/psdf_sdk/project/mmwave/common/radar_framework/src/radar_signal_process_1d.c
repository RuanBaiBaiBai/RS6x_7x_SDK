/**
 ******************************************************************************
 * @file    main.c
 * @brief   main define.
 * @verbatim    null
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
#define LOG_MODULE                      "RADAR_SIGNAL_1D_PROCESS"
#define LOG_LEVEL                       1

#include "common.h"
#include "mmw_ctrl.h"
#include "hal_board.h"
#include "radar_framework_report.h"
#include "mmw_point_cloud_psic_lib.h"
#include "mmw_app_pointcloud.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_alg_debug.h"
#include "mmw_type.h"
#include "log.h"
#include "radar_framework.h"
#include "mmw_point_cloud_psic_lib.h"
#if (CONFIG_MMW_DRIVER_TEMP_MGMT)
#include "mmw_temp_mgmt_api.h"
#endif
#if CONFIG_MMW_LOOP_TASK_MONITOR
#include "mmw.h"
#endif
#if (CONFIG_MMW_WATCHDOG)
#include "hal_wdg.h"
#endif

#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
static complex16_cube* s_ptr_1d_datacube;

int radar_signal_process_1d_app_init(void)
{
	int ret = 0;
	uint16_t range_fft_num, doppler_fft_num;
	MmwPsicMimoRxNum_t mimo_rx_info;
	const RadarConfig_t* ptr_radar_config = radar_framework_get_config_const();

	if (!ptr_radar_config) {
		ret = MMW_ERR_CODE_INVALID_PARAM;
		LOG_ERR("Radar config is not set!\n");
		goto ERROR;	
	}

	if(mmw_point_cloud_get_user_cfg()->auto_gain_flag) {
		ret = MMW_ERR_CODE_UNSUPPORT;
		LOG_ERR("Auto gain in 1D Frame is not support! Please check CONFIG_MMW_FFT_AUTOGAIN_EN!\r\n");
		goto ERROR;
	}

	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	mmw_psic_lib_sdk_get_tx_rx_num(&mimo_rx_info);

	/* Check heap configuration is valid */
	if (CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN < range_fft_num) {
		ret = MMW_ERR_CODE_NO_MEMORY;
		LOG_ERR("CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN set less than range fft number! Please check configurations!\r\n");
		goto ERROR;
	} else if (CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN > range_fft_num) {
		LOG_PRINT("Heap size can be reduceed by setting CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN equal to max-used range_fft_num.\r\n");
	} else {
		;
	}

	if (ptr_radar_config->radar_frame_config.fft_mode == RADAR_FRAMEWORK_ADC_MODE) {
		uint32_t range_mm, range_reol_mm;
		mmw_range_get(&range_mm, &range_reol_mm);
		/* If adc output is enabled, range fft number and doppler fft nunmbe should be power of 2 */
		if (range_mm != range_fft_num * range_reol_mm) {
			LOG_ERR("When ADC output is enabled, please set range fft length equal to adc sample number!\r\n");
			ret = MMW_ERR_CODE_UNSUPPORT;
			goto ERROR;
		}
	}

#if CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE
	radar_framework_data_report_init(RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT, RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE);
	/* Configure param for datacube report */
	radar_framework_report_param_get();
#else
	if (s_ptr_1d_datacube) {
		OSI_Free(s_ptr_1d_datacube);
	}
	s_ptr_1d_datacube = OSI_Malloc(sizeof(complex16_cube) * range_fft_num * mimo_rx_info.mimo_rx_num);
	if (s_ptr_1d_datacube == 0) {
		LOG_ERR("malloc rfft buffer failed!\n");
		ret = MMW_ERR_CODE_NO_MEMORY;
	}
#endif

ERROR:
	return ret;
}

int radar_signal_process_1d_app_deinit(void)
{
#if CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE
	radar_framework_data_cube_report_deinit();
#else
	if (s_ptr_1d_datacube) {
		OSI_Free(s_ptr_1d_datacube);
		s_ptr_1d_datacube = 0;
	}	
#endif
	return 0;
}

__sram_text int radar_framework_1d_frame_cb(void *mmw_data, void *arg) {
	uint32_t frame_idx = radar_framework_increase_frame_idx();
	const MmwPointCloudUserCfg_t* ptr_point_cloud_config = mmw_point_cloud_get_user_cfg_const();
	mmw_application_ops_t* ptr_application_cb = radar_framework_get_application_ops();

	if (radar_framework_clutter_convergence_process(frame_idx, ptr_point_cloud_config->mmw_point_cloud_detection_config.clutter_rm_method)){
		if (ptr_point_cloud_config->mmw_point_cloud_detection_config.clutter_rm_method == MMW_CLUTTER_REMOVAL_DC) {
			if (CLUTTER_HALT_AFTER_CONVERGENCE != radar_framework_get_config_const()->radar_app_config.clutter_halt_method) {
				mmw_psic_dc_suppression_update();
			}
		}
	}

	/* Call user entry callback functions */
	if(ptr_application_cb && ptr_application_cb->mmw_entry_proc_cb){
		ptr_application_cb->mmw_entry_proc_cb(frame_idx);
	}

#if CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE
    radar_framework_ctrl_data_cube_report_cb(mmw_data, arg);
#else
	/* A sample of how to read 1D-DataCube */
	complex16_cube* ptr_datacube = s_ptr_1d_datacube;
	uint16_t range_fft_num, doppler_fft_num;
	MmwPsicMimoRxNum_t mimo_rx_info;

	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	mmw_psic_lib_sdk_get_tx_rx_num(&mimo_rx_info);
	mmw_motion_cube_access_open();
	for (uint8_t tx_idx = 0; tx_idx < mimo_rx_info.ant_tx_num; tx_idx++) {
		for (uint8_t rx_idx = 0; rx_idx < mimo_rx_info.ant_rx_num; rx_idx++) {
			__mmw_fft_range(ptr_datacube, 0, range_fft_num, tx_idx, rx_idx, doppler_fft_num >> 1);
			ptr_datacube += range_fft_num;
        }
    }
	mmw_motion_cube_access_close();
	/* Data is saved in s_ptr_1d_datacube */
#endif

	/* Call user callback functions */
	if(ptr_application_cb && ptr_application_cb->mmw_post_proc_cb){
		ptr_application_cb->mmw_post_proc_cb(1, (void**)&s_ptr_1d_datacube);
	}

#if (CONFIG_MMW_WATCHDOG)
    mmw_watchdog_feed();
#endif
#if (CONFIG_MMW_LOOP_TASK_MONITOR)
	mmw_loop_task_time_reset(NULL);
#endif

#if (CONFIG_MMW_DRIVER_TEMP_MGMT)
	if (temp_mgmt_get_triger_calib_event()) {
		return 1;
	}
#endif

    return 0;
}

#endif 	/* #if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN */