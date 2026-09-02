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

#define LOG_MODULE                      "RADAR_FRAMEWORK"
#define LOG_LEVEL                       1

#include "common.h"
#include "radar_framework.h"
#include "mmw_ctrl.h"
#include "hif.h"
#include "log.h"
#include "mmw_app_pointcloud.h"
#include "mmw_point_cloud_psic_lib.h"
#include "mmw_app_pointcloud_config.h"
#include "radar_framework_report.h"
#include "radar_framework_hif_handler.h"
#include "radar_signal_process_2d.h"
#include "radar_signal_process_1d.h"
#include <math.h>
#if CONFIG_RADAR_FRAMEWORK_FT
#include "factory_test.h"
#endif
#if CONFIG_MMW_PRESENCE_POINT_CLOUD
#include "mmw_alg_doa.h"
#include "mmw_app_micro_pointcloud.h"
#endif

#if (CONFIG_MMW_DRIVER_TEMP_MGMT)
#include "mmw_temp_mgmt_api.h"
#endif
#if CONFIG_MMW_LOOP_TASK_MONITOR
#include "mmw.h"
#endif
#if (CONFIG_MMW_WATCHDOG)
#include "hal_wdg.h"
#endif

/*
 * In 1D frame mode, interval time has no meaning, but due to frame period time must be integer multiple of interval time,
 * so default DEFAULT_1D_FFT_INTERVAL_TIME_US is 100us, making period precision 0.1ms.
 */
#define DEFAULT_1D_FFT_INTERVAL_TIME_US (100)

/*
 * DC Remove frames, when convergencing, no targets should be withn 40cm of radar.
 * Decrease or increase this parameter can reduce or enhance dc remove performance.
 * */
#define RADAR_FRAMEWORK_DEFAULT_DC_REMOVAL_CONVERGENCE_LEN		(40)

/**
 * Too small maximumn detection range will cause RF work in unsupportted condition,
 * minimun max range configured in mmw_range_cfg() must be larger than 5m. 
 **/
#define MIN_RANGE_SUPPORTTED_MM			(5000)

static uint32_t s_frame_idx = 0;

/*
 * Frame number wait for clutter convergence, when clutter_rm_method is set to MMW_CLUTTER_REMOVAL_DC,
 * radar framework use fix 'RADAR_FRAMEWORK_DEFAULT_DC_REMOVAL_CONVERGENCE_LEN' frames to convergence,
 * when clutter_rm_method is set to MMW_CLUTTER_REMOVAL_ALL, radar framework use 2<<clutter_factor frames to convergence,
 */
static uint8_t s_clutter_removal_count;

RadarConfig_t *s_ptr_global_config;
mmw_application_ops_t *s_ptr_mmw_app_ops;

int radar_framework_register_callbacks(RadarConfig_t* ptr_global_config, mmw_application_ops_t* ptr_mmw_app_ops)
{
	int status = 0;
	s_ptr_global_config = ptr_global_config;
	s_ptr_mmw_app_ops = ptr_mmw_app_ops;

	/* hif config callback register */
	if (s_ptr_mmw_app_ops) {
		status = radar_framework_regist_hif_config_callback(s_ptr_mmw_app_ops->mmw_hif_config_handler, s_ptr_mmw_app_ops->mmw_hif_report_handler);
	} else {
		status = radar_framework_regist_hif_config_callback(0, 0);
	}
	if(status) {
		LOG_ERR("\r\nradar_framework_regist_hif_config_callback() failed %d \r\n", status);
	}
	return status;
}

__sram_text mmw_application_ops_t* radar_framework_get_application_ops(void)
{
	return s_ptr_mmw_app_ops;
}

__sram_text RadarConfig_t* radar_framework_get_config(void)
{
	return s_ptr_global_config;
}

__sram_text const RadarConfig_t* radar_framework_get_config_const(void)
{
	return s_ptr_global_config;
}

static void radar_framework_calculate_coverage_frame_num(MmwPointClutterRemoval_e clutter_rm_method, uint8_t clutter_factor) {
	if (clutter_rm_method == MMW_CLUTTER_REMOVAL_DC) {
		LOG_PRINT("clutter removal is set to MMW_CLUTTER_REMOVAL_DC.\r\n");
		s_clutter_removal_count = RADAR_FRAMEWORK_DEFAULT_DC_REMOVAL_CONVERGENCE_LEN;			/* fix coverage 20 frames for dc removal */
	} else if (clutter_rm_method == MMW_CLUTTER_REMOVAL_ALL) {
		s_clutter_removal_count = 2 << (clutter_factor);
		LOG_PRINT("clutter removal is set to MMW_CLUTTER_REMOVAL_ALL.\r\n");
	} else {
		LOG_PRINT("clutter removal is disabled.\r\n");
		s_clutter_removal_count = 0;
	}
}

static int check_freq_support(uint32_t start_freq_mhz)
{

#if CONFIG_SOC_SERIES_RS624X
	return (start_freq_mhz < 57000) || (start_freq_mhz > 66000);
#elif CONFIG_SOC_SERIES_RS613X
	return (start_freq_mhz < 58000) || (start_freq_mhz > 65000);
#else
	#error "Please Select Proper SOC!"
#endif
}

static int radar_frame_para_config(void)
{
	int ret = MMW_ERR_CODE_UNSUPPORT;
	if(s_ptr_global_config == 0) {
		ret = MMW_ERR_CODE_INVALID_PARAM;
		LOG_ERR("Empty confiure pointer: %d\n", ret);
		goto CONFIG_ERR;
	}

	uint8_t mimo_mode = s_ptr_global_config->radar_frame_config.mimo_mode;
#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
    if(s_ptr_global_config->radar_frame_config.frame_type == 0) {
        ret = mmw_mode_cfg(mimo_mode, MMW_WORK_MODE_1DFFT); /* config 1d work mode */
		if (ret) {
			LOG_ERR("mode cfg error: %d\n", ret);
			goto CONFIG_ERR;
		}
	}
#endif

#if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN
    if(s_ptr_global_config->radar_frame_config.frame_type == 1) {
        ret = mmw_mode_cfg(mimo_mode, MMW_WORK_MODE_2DFFT); /* config 2d work mode */
		if (ret) {
			LOG_ERR("call mmw_mode_cfg() error: %d\n", ret);
			goto CONFIG_ERR;
		}

		if (s_ptr_global_config->radar_frame_config.fft_mode != RADAR_FRAMEWORK_2DFFT_MODE) {
			ret = mmw_dop_fft_set(false);
		}  else {
			ret = mmw_dop_fft_set(true);
		}
		if (ret) {
			LOG_ERR("call mmw_dop_fft_set() failed! %d\n", ret);
			return ret;
		}
    }
#endif
	if (ret == MMW_ERR_CODE_UNSUPPORT) {
        LOG_ERR("NO proper frame type is select, please check RadarFrameConfig_t::frame_type, current is: %d\n", ret);
		goto CONFIG_ERR;
	}

	if (s_ptr_global_config->radar_frame_config.fft_mode == RADAR_FRAMEWORK_ADC_MODE) {
		mmw_time_domain_switch_set(true);
	} else {
		mmw_time_domain_switch_set(false);
	}

	/* RS6240 Support 57G~66G, RS6130 Support 58G~64G */
	uint32_t end_freq = s_ptr_global_config->radar_frame_config.start_freq_mhz + 150000 / s_ptr_global_config->radar_frame_config.range_resolution_mm;
	if (check_freq_support(s_ptr_global_config->radar_frame_config.start_freq_mhz)) {
		ret = MMW_ERR_CODE_UNSUPPORT;
		LOG_ERR("start frequency overflow!\n");
		goto CONFIG_ERR;
	}
	if (check_freq_support(end_freq)) {
		ret = MMW_ERR_CODE_UNSUPPORT;
		LOG_ERR("end frequency overflow!\n");
		goto CONFIG_ERR;
	}

    ret = mmw_freq_cfg(s_ptr_global_config->radar_frame_config.start_freq_mhz, 0);
    if (ret) {
        LOG_ERR("call mmw_freq_cfg() error: %d\n", ret);
		goto CONFIG_ERR;
    }

    /* config max det range and range res */
	/* re-calcuate range/doppler fft number */
	uint32_t max_range_meas, max_velocity_meas;
	/* Check range_fft_length and dop_fft_length */
	if ((s_ptr_global_config->radar_frame_config.range_fft_len_log2 != UINT16_MAX) && (s_ptr_global_config->radar_frame_config.dopper_fft_len_log2 != UINT16_MAX)) {
		if (s_ptr_global_config->radar_frame_config.range_fft_len_log2 + s_ptr_global_config->radar_frame_config.dopper_fft_len_log2 > 14) {
			LOG_ERR("range fft length(log2) add doppler fft length(log2) must less than 15!\n");
			goto CONFIG_ERR;
		}
	}

	if (s_ptr_global_config->radar_frame_config.range_fft_len_log2 == UINT16_MAX) {
		/* If 'max_det_range_mm' has been configured, use max_det_range_mm instead of range_fft_len_log2 */
		max_range_meas = s_ptr_global_config->radar_frame_config.max_range_mm;
	} else {
		if ((s_ptr_global_config->radar_frame_config.range_fft_len_log2 > 9) && mmw_point_cloud_get_user_cfg()->auto_gain_flag){
			ret = MMW_ERR_CODE_UNSUPPORT;
			LOG_ERR("range fft length must less than 10 when auto gain is enabled!\n");
			goto CONFIG_ERR;
		}
		if ((s_ptr_global_config->radar_frame_config.range_fft_len_log2 > 10) && (!mmw_point_cloud_get_user_cfg()->auto_gain_flag)){
			ret = MMW_ERR_CODE_UNSUPPORT;
			LOG_ERR("range fft length must less than 11 when auto gain is not enabled!\n");
			goto CONFIG_ERR;
		}
		max_range_meas = (1 << s_ptr_global_config->radar_frame_config.range_fft_len_log2) * s_ptr_global_config->radar_frame_config.range_resolution_mm;
	}
	if (max_range_meas < MIN_RANGE_SUPPORTTED_MM) {
		ret = MMW_ERR_CODE_UNSUPPORT;
        LOG_ERR("Max range measurement must over 5m!\n");
		goto CONFIG_ERR;		
	}
	
	if (s_ptr_global_config->radar_frame_config.dopper_fft_len_log2 == UINT16_MAX) {
		max_velocity_meas = s_ptr_global_config->radar_frame_config.max_unambigous_vel_mm;
	} else {
		max_velocity_meas = (1 << (s_ptr_global_config->radar_frame_config.dopper_fft_len_log2 - 1)) * s_ptr_global_config->radar_frame_config.vel_resolution_mm;
	}
	
    ret = mmw_range_cfg(max_range_meas, s_ptr_global_config->radar_frame_config.range_resolution_mm);
    if (ret) {
        LOG_ERR("call mmw_range_cfg() error: %d\n", ret);
		goto CONFIG_ERR;
    }
    if(s_ptr_global_config->radar_frame_config.frame_type == 1) {
        /* config max velocity and velocity res */
        ret = mmw_velocity_cfg(max_velocity_meas, s_ptr_global_config->radar_frame_config.vel_resolution_mm);
        if (ret) {
            LOG_ERR("call mmw_velocity_cfg() error: %d\n", ret);
		goto CONFIG_ERR;
        }
    } else {
        ret = mmw_interval_cfg(DEFAULT_1D_FFT_INTERVAL_TIME_US, 1);
        if (ret) {
            LOG_ERR("call mmw_interval_cfg() error: %d\n", ret);
		    goto CONFIG_ERR;
        }
    }

	/* Configure ACC number in each interval */
	if (s_ptr_global_config->radar_frame_config.acc_num_log2 > 3) {
        LOG_PRINT("acc_num_log2 is recommand less than 3\n");
	}
	ret = mmw_chirp_num_cfg(1 << s_ptr_global_config->radar_frame_config.acc_num_log2);
	if (ret) {
        LOG_ERR("call mmw_chirp_num_cfg() error: %d\n", ret);
		goto CONFIG_ERR;
    }

    /* config frame period */
    ret = mmw_frame_cfg(s_ptr_global_config->radar_frame_config.frame_period_ms, 0);
    if (ret) {
        LOG_ERR("call mmw_frame_cfg() error: %d\n", ret);
		goto CONFIG_ERR;
    }
    radar_framework_report_frame_config();
CONFIG_ERR:
    return ret;
}

static int radar_framework_app_init(void)
{
	int ret = 0;

#if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN
    if (s_ptr_global_config->radar_frame_config.frame_type == 1) {
        ret = radar_signal_process_2d_app_init();
        if (ret) {
            LOG_ERR("call radar_signal_process_2d_app_init() error: %d\n", ret);
            goto ERROR;
        }
    }
#endif

#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
    if (s_ptr_global_config->radar_frame_config.frame_type == 0) {
        ret = radar_signal_process_1d_app_init();
        if (ret) {
            LOG_ERR("call radar_signal_process_1d_app_init() error: %d\n", ret);
            goto ERROR;
        }
    }
#endif

	LOG_PRINT("Autogain is %d\n", mmw_fft_autogain_get());

#if CONFIG_MMW_MOTION_POINT_CLOUD == 0
	if ((s_ptr_global_config->radar_app_config.clutter_halt_method == CLUTTER_AUTO_UPDATE) && (mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_rm_method != MMW_CLUTTER_REMOVAL_NONE)) {
		LOG_PRINT("CLUTTER_AUTO_UPDATE will work well when not enable motion point cloud.\n");
	}
#endif

	if ((mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_rm_method != MMW_CLUTTER_REMOVAL_NONE) &&
		(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_cover_time < 4) &&
		(s_ptr_global_config->radar_app_config.clutter_halt_method == CLUTTER_AUTO_UPDATE)
		)
	 {
		LOG_PRINT("clutter cover time in mmw_app_point_cloud_config is recommanded to be larger than 4 when clutter removal method is 'CLUTTER_AUTO_UPDATE'\n");
	}

	radar_framework_calculate_coverage_frame_num(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_rm_method,
												mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_cover_time);
    s_frame_idx = 0;

	/*
	 * According to frame params, set HIF message queue timeout.
	 * HIF message queue timeout must satify:
	 * 	1. less than actural HIF data upload period;
	 *  2. larger than upload time.
	*/
    uint32_t hif_upload_period = 0;
#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN && CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE
	if (s_ptr_global_config->radar_frame_config.frame_type == 0) {	/* Use 1D frame*/
		hif_upload_period = 40;
	} else
#else
	{
		uint32_t frame_num = 0;
		ret = mmw_frame_get(&hif_upload_period, &frame_num);
		if (ret) {
			LOG_ERR("call mmw_frame_get() error: %d\n", ret);
			goto ERROR;
		}
		hif_upload_period = hif_upload_period < 110 ? hif_upload_period : 110;
		hif_upload_period = hif_upload_period > 30 ? hif_upload_period : 30;
		hif_upload_period -= 10;
	}
#endif
    HIF_ExtControl(HIF_SET_SEND_FRAME_TO, (void *)&hif_upload_period, sizeof(hif_upload_period));

ERROR:
	return ret;
}


int radar_framework_firmware_config(void)
{
    int ret = 0;
	if (s_ptr_global_config == 0){
		LOG_ERR("global config has not been registered!\n");
		ret = MMW_ERR_CODE_INVALID_PARAM;
		goto CONFIG_ERR;
	}

	/* configure frame params */
    ret = radar_frame_para_config();
    if (ret) {
        LOG_ERR("call radar_frame_para_config() error: %d\n", ret);
		goto CONFIG_ERR;
    }

	/* Run user hardware init function, which has higher priority than mmw_point_cloud_bb_config()*/
	if(s_ptr_mmw_app_ops->mmw_hw_init_cb) {
		ret = s_ptr_mmw_app_ops->mmw_hw_init_cb();
		if (ret) {
			LOG_ERR("s_ptr_mmw_app_ops->call mmw_hw_init_cb() error: %d\n", ret);
			goto CONFIG_ERR;
		}
	}
	/*
	 * This function is independent from any application it must be called.
	 * Thif function initial baseband configures according to params in mmw_point_cloud_get_user_cfg_const().
	*/
	ret = mmw_point_cloud_bb_config();
    if (ret) {
        LOG_ERR("call mmw_point_cloud_bb_config() error: %d\n", ret);
		goto CONFIG_ERR;
    }

	/** Step 3: Run software initialize functions;
	 * Software initialize functions usually allocate memories and initialize hardawre-independent variables.
	 * software may use frame struct params to determine memory usage, so software initialize functions should run
	 * after frame and interval params have been set by 'mmw_frame_cfg()' function. After 'mmw_frame_cfg()' i
	 * */
	if(s_ptr_mmw_app_ops->mmw_app_init_cb) {
		ret = s_ptr_mmw_app_ops->mmw_app_init_cb();
		if (ret) {
			LOG_ERR("s_ptr_mmw_app_ops->call mmw_app_init_cb() error: %d\n", ret);
			goto CONFIG_ERR;
		}
	}
	ret = radar_framework_app_init();
	if (ret) {
		LOG_ERR("call radar_framework_app_init() error: %d\n", ret);
		goto CONFIG_ERR;
	}

    ret = MMW_ERR_CODE_UNSUPPORT;
#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
        if (s_ptr_global_config->radar_frame_config.frame_type == 0) {
            ret = mmw_ctrl_callback_cfg(radar_framework_1d_frame_cb, MMW_DATA_TYPE_1DFFT, NULL);
            if (ret) {
                LOG_ERR("register 1d frame failed %d\n", ret);
                goto CONFIG_ERR;
            }
        }
#endif

#if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN
        if (s_ptr_global_config->radar_frame_config.frame_type == 1) {
            ret = mmw_ctrl_callback_cfg(radar_framework_2d_frame_cb, MMW_DATA_TYPE_2DFFT, NULL);
            if (ret) {
                LOG_ERR("register 2d frame failed %d\n", ret);
                goto CONFIG_ERR;
            }
        }
#endif	/* #if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN == 0*/

    /* if no mmw_ctrl_callback_cfg() is called, ret is MMW_ERR_CODE_UNSUPPORT here. */
	if (ret) {
        LOG_ERR("NO proper frame type is select, please check RadarFrameConfig_t::frame_type, current is: %d\n", ret);
		goto CONFIG_ERR;
    }

	/* check doppler fft number is not 2 when doppler fft is enabled */
	uint16_t range_fft_num, doppler_fft_num;
	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);

	uint8_t dop_fft_enable_flag = mmw_dop_fft_get();
	if (dop_fft_enable_flag && doppler_fft_num == 2) {
		LOG_ERR("doppler fft length must not be 2 when doppler fft is enabled!\n");
		ret = MMW_ERR_CODE_UNSUPPORT;
		goto CONFIG_ERR;
	}

CONFIG_ERR:
	return ret;
}

__sram_text inline uint32_t radar_framework_increase_frame_idx(void) {
    return (s_frame_idx++);
}

__sram_text inline uint32_t radar_framework_frame_idx_get(void) {
    return s_frame_idx;
}

__sram_text static uint8_t radar_framework_test_clutter_coverage(void) {
	if (s_clutter_removal_count > 0) {
		s_clutter_removal_count--;
	} else {
		s_clutter_removal_count = 0;
	}
	return !s_clutter_removal_count;
}

__sram_text static uint16_t radar_framework_get_clutter_coverage_remaining_frame(void) {
	return s_clutter_removal_count;
}

__sram_text int radar_framework_clutter_convergence_process(uint32_t frame_idx, MmwPointClutterRemoval_e clutter_rm_method) {
	int ret = 0;

	ret = radar_framework_test_clutter_coverage();
	if (!ret) { /* return vaule of 0 indicates that the invalid data frames has skipped */
		if (clutter_rm_method == MMW_CLUTTER_REMOVAL_DC) {
			/*
			 * If clutter rm method is CLTTER_REMOVAL_DC, framework must re-enable mmw_clutter_remove at 1st frame.
			*/
			if (frame_idx == 0) {
				LOG_PRINT("re-enable clutter remove\r\n" );
				mmw_clutter_remove(MMW_CLUTTER_MODE_ENABLE);
			}
			if (mmw_psic_dc_suppression_update()) {
				LOG_ERR("dc_suppression_update FAILED, Please Check CLUTTER CONFIG\r\n");
			}
		}

		/* When clutter convergence at the last frame, according to clutter_halt_method to continue or halt clutter update  */
		if (1 == radar_framework_get_clutter_coverage_remaining_frame()) {
			if ((clutter_rm_method == MMW_CLUTTER_REMOVAL_ALL) &&
				(radar_framework_get_config_const()->radar_app_config.clutter_halt_method == CLUTTER_HALT_AFTER_CONVERGENCE)) {
				mmw_clutter_halt_set(MMW_HALT_CLUTTER_UPDATE_ENABLE);
			}
		}
	}
	return ret;
}

#if (CONFIG_MMW_DRIVER_TEMP_MGMT)
int mmw_app_start(void *arg)
{
	mmw_ctrl_start();
#if (CONFIG_MMW_LOOP_TASK_MONITOR)
	mmw_loop_task_time_reset(NULL);
#endif
	return 0;
}
int mmw_app_stop(void *arg)
{
	mmw_ctrl_stop();
	OSI_MSleep(20);
	return 0;
}
int mmw_temp_triger_event_handler(void *arg)
{
	temp_mgmt_set_triger_calib_event(true);

	return 0;
}
#endif

#if (CONFIG_MMW_WATCHDOG)
static HAL_Dev_t *wdg_dev = NULL;
static int watchdog_init(void)
{
    WDG_InitParam_t initParam = {
        .mode = WDG_MODE_RESET_SYS,
        .callback = NULL,
        .timeout = CONFIG_WDG_TIMEOUT_MS,
    };

    wdg_dev = HAL_WDG_Init(0);
    if (wdg_dev == NULL) {
         LOG_PRINT("watchdog init fail!\n");
         return 0;
    }
    HAL_WDG_Open(wdg_dev, &initParam);

    HAL_WDG_Feed(wdg_dev);

	return 0;
}
__sram_text void mmw_watchdog_feed(void)
{
	HAL_WDG_Feed(wdg_dev);
}
void mmw_watchdog_stop(void)
{
	HAL_WDG_Close(wdg_dev);
}
int mmw_watchdog_open(void)
{
    if (wdg_dev == NULL) {
         LOG_ERR("watchdog not initialized!\n");
         return 0;
    }
    HAL_WDG_Open(wdg_dev, NULL);
    HAL_WDG_Feed(wdg_dev);
	return 0;
}

#endif

void radar_framework_wait_for_mmw_callback_returns(void)
{
	int mmw_state = 0;
	int to = 0;
	do {
		/* get mmw state, 4: runing */
		mmw_state = can_mmw_configured();
		if ((mmw_state != 4) && (mmw_event_process_completed())) {
			/* alreay stop */
			break;
		} else {
			/* prevent freezing, max 200ms */
			if (to >= 200) {
				break;
			}
			OSI_MSleep(1);
		}
		to++;
	} while (1);
}

#if CONFIG_RADAR_FRAMEWORK_FT
/** @brief this function is called when entering or exiting factory test mode
 *  @param event is FT_EVT_EXIT or FT_EVT_ENTER which is defined in factory_test.h.
 * 		   the value of event is used to judge current status is enter or exit factory test mode.
 */
int factory_test_callback_handle(uint8_t event)
{
	int ret = 0;
	if (event == FT_EVENT_ENTER) {
		/* add user operation in here.
		 * when entering factory-test mode, user should stop any other ongoing work.
		 * e.g. stop wave, deinit main idle */
		ret = radar_framework_stop_process();
	} else if (event == FT_EVENT_EXIT) {
		/* add user operation in here.
		 * when exiting  factory-test mode, user can execute other work.*/
		LOG_PRINT("exit factory-test mode, user can execute other work\n");
		ret = radar_framework_restart_process();
	}
	return ret;
}
#endif

int radar_framework_sys_init(void)
{
	int status = 0;

	/* MRS6130_P1806 set UART default
	 * MRS6130_P1812、MRS624x series set SPI default
	 * */
    HIF_CfgParam_t hifInitParam;
#if ((CONFIG_HIF_PHY_TYPE & 4) != 0)    /* spi */
	HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_SPI, 56000000);

#if (CONFIG_HIF_APP_SPEC_POOL)
    hifInitParam.comParam = 2;//spi quad mode
#endif
#elif ((CONFIG_HIF_PHY_TYPE & 1) != 0)  /* uart */
	HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_UART, CONFIG_HIF_PHY_UART_DEF_RATE);
#elif ((CONFIG_HIF_PHY_TYPE & 2) != 0)  /* iic */
	HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_IIC, 400000);
#else
	#error "not support hif type"
#endif

    LOG_PRINT("Init HIF hardware params......");
    status = HIF_Init(&hifInitParam);
	if(status) {
		LOG_ERR("\r\nHIF_Init() fail %d \r\n", status);
		goto EXIT;
	}
    LOG_PRINT("SUCCESS!\r\n");

    /* open mmw ctrl */
    LOG_PRINT("Open mmw......\r\n");
    status = mmw_ctrl_open(true, false, true);
    if (status != 0) {
        LOG_PRINT("mmw_ctrl_open() fail %d\n", status);
        goto EXIT;
    }

#if CONFIG_RADAR_FRAMEWORK_FT
	int ft_status = FactoryTest_Init(factory_test_callback_handle);
	if (ft_status == FACTORY_TEST_MODE_VALUE_ENTER) {
		/* if enter ft mode, user should not start mmw wave again */
		LOG_PRINT("current mode is factory-test mode\n");
#if CONFIG_MMW_LOOP_TASK_MONITOR
		mmw_loop_task_time_stop();
#endif
		status = MMW_ERR_CODE_RUNNING;
		goto EXIT;
	} else if (ft_status == NORMAL_MODE_VALUE) {
		/* if quit ft mode or current mode is normal mode, user can start mmw wave or execute other work */
		LOG_PRINT("current mode is normal mode\n");
	}
#endif

#if (CONFIG_MMW_DRIVER_TEMP_MGMT)
	mmw_temp_mgmt_init(TEMP_MGMT_HOST_CTRL, TEMP_MGMT_DEF_DETECT_PERIOD * 1000 / 250,
						TEMP_MGMT_DEF_TEMP_THR, (TEMP_MGMT_CLK_CALIB_MASK | TEMP_MGMT_ALL_FM_CALIB_MASK),
						mmw_temp_triger_event_handler);
	mmw_temp_mgmt_app_process_init(mmw_app_start, mmw_app_stop);
#endif
#if CONFIG_MMW_LOOP_TASK_MONITOR
    mmw_loop_task_time_reset(NULL);
#endif
#if (CONFIG_MMW_WATCHDOG)
    watchdog_init();
#endif

EXIT:
	return status;
}

int radar_framework_monitor_restart(void)
{
#if CONFIG_MMW_LOOP_TASK_MONITOR
    mmw_loop_task_time_reset(NULL);
#endif
#if (CONFIG_MMW_WATCHDOG)
	mmw_watchdog_open();
#endif
	return 0;
}

int radar_framework_monitor_stop(void)
{
#if (CONFIG_MMW_LOOP_TASK_MONITOR)
	mmw_loop_task_time_stop();
#endif
#if CONFIG_MMW_WATCHDOG
	mmw_watchdog_stop();
#endif
	return 0;
}

int radar_framework_app_deinit(void)
{
	mmw_application_ops_t* ptr_mmw_app_ops = radar_framework_get_application_ops();
	int ret = 0;

	do {
		if (ptr_mmw_app_ops) {
			int callback_ret;
			if (ptr_mmw_app_ops->mmw_hw_deinit_cb) {
				callback_ret = ptr_mmw_app_ops->mmw_hw_deinit_cb();
				if (callback_ret) {
					LOG_ERR("mmw_hw_deinit_cb() failed: %d", callback_ret);
					ret = callback_ret;
					break;
				}
			}
			if (ptr_mmw_app_ops->mmw_app_deinit_cb) {
				callback_ret = ptr_mmw_app_ops->mmw_app_deinit_cb();
				if (callback_ret) {
					LOG_ERR("mmw_app_deinit_cb() failed: %d", callback_ret);
					ret = callback_ret;
					break;
				}
			}
#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
			ret = radar_signal_process_1d_app_deinit();
			if (ret) {
				LOG_ERR("radar_signal_process_1d_app_deinit() failed: %d", callback_ret);
				break;
			}
#endif
#if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN
			ret = radar_signal_process_2d_app_deinit();
			if (ret) {
				LOG_ERR("radar_signal_process_2d_app_deinit() failed: %d", callback_ret);
				break;
			}
#endif
		}
	} while(0);
	return ret;
}


int radar_framework_restart_process(void)
{
	int ret = 0;
	ret = radar_framework_firmware_config();
	if (ret) {
		LOG_ERR("radar_framework_firmware_config() failed: %d", ret);
	} else {
		ret = mmw_ctrl_start();
		if (ret) {
			LOG_ERR("mmw_ctrl_start() failed: %d", ret);
		}
		radar_framework_monitor_restart();
	}
	return ret;
}

int radar_framework_stop_process(void)
{
	int ret = 0;

	HIF_MsgReport_Lock();
	HIF_MsgReport_Flush(1);
	do {
		ret = mmw_ctrl_stop();
		if (ret) {
			LOG_ERR("mmw_ctrl_stop() failed: %d", ret);
			break;
		}
		/* wait for last mmw callback function returns, then we can release memories */
		radar_framework_wait_for_mmw_callback_returns();
		/* Flush HIF queue, then enter configuration process */
		HIF_MsgReport_Flush(1);
		ret = radar_framework_monitor_stop();
		if (ret) {
			LOG_ERR("radar_framework_monitor_stop() failed: %d", ret);
			break;
		}
		ret = radar_framework_app_deinit();
		if (ret) {
			LOG_ERR("radar_framework_app_deinit() failed: %d", ret);
			break;
		}
	} while(0);

	HIF_MsgReport_Unlock();

	return ret;
}