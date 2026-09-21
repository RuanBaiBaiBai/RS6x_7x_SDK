
#define LOG_MODULE                      "RADAR_SIGNAL_2D_PROCESS"
#define LOG_LEVEL                       1

#include "common.h"
#include "mmw_ctrl.h"
#include "radar_framework_report.h"
#include "mmw_point_cloud_psic_lib.h"
#include "mmw_app_pointcloud.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_alg_debug.h"
#include "mmw_type.h"
#include "log.h"
#include "radar_signal_process_2d.h"
#include "radar_framework.h"
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

#include "hal_board.h"

#if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN

static complex16_cube *g_1d_fft_data_buff = 0; /* store 1d fft data */
int static radar_framework_obtain_1d_fft_data_init(void)
{
	int8_t ret = 0;
	MmwPsicMimoRxNum_t mimo_rx_info;
	uint16_t range_fft_num, doppler_fft_num;

	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	mmw_psic_lib_sdk_get_tx_rx_num(&mimo_rx_info);

	ret = mmw_2dfft_obtain_1dfft_cfg(0, range_fft_num, 0, 0, MMW_ANT_ID_ALL, MMW_ANT_ID_ALL);
	if (ret) {
		LOG_ERR("obtain_1dfft_cfg FAILED: %d\n", ret);
		goto ERROR;
	}

	/* allocate the memory size according mmw_2dfft_obtain_1dfft_cfg */
	if (g_1d_fft_data_buff) {
		OSI_Free(g_1d_fft_data_buff);
	}
	/* For PING-PONG 2x memory should be used */
	g_1d_fft_data_buff = OSI_Malloc(sizeof(*g_1d_fft_data_buff) * mimo_rx_info.ant_tx_num * mimo_rx_info.ant_rx_num * range_fft_num * 2);
	if (g_1d_fft_data_buff == 0) {
		ret = MMW_ERR_CODE_NO_MEMORY;
		LOG_ERR("g_1d_fft_data_buff malloc FAILED!\n");
		goto ERROR;
	}
	ret = mmw_2dfft_set_1dfft_buffer(g_1d_fft_data_buff); /* regist pointer */
	if (ret) {
		LOG_ERR("set_1dfft_buffer FAILED: %d\n", ret);
		goto ERROR;
	}

ERROR:
	return ret;
}

void static radar_framework_obtain_1d_fft_data_deinit(void)
{
	mmw_2dfft_set_1dfft_buffer(0);
	mmw_process_mem_free((void**)&g_1d_fft_data_buff);
}

__sram_text static uint16_t interval0_rfft_buffer_set(uint32_t frame_idx)
{
	uint16_t range_fft_num, doppler_fft_num;
	MmwPsicMimoRxNum_t mimo_rx_info;
	uint16_t ping_pong_idx_offset = 0;
	uint16_t ping_pong_idx_offfset_next = 0;

	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	mmw_psic_lib_sdk_get_tx_rx_num(&mimo_rx_info);


	ping_pong_idx_offset = (frame_idx & 0x01) * (mimo_rx_info.ant_tx_num * mimo_rx_info.ant_rx_num * range_fft_num);
	ping_pong_idx_offfset_next = (!(frame_idx & 0x01)) * (mimo_rx_info.ant_tx_num * mimo_rx_info.ant_rx_num * range_fft_num);

	mmw_2dfft_set_1dfft_buffer(g_1d_fft_data_buff + ping_pong_idx_offfset_next); /* regist pointer */

	return ping_pong_idx_offset;

}

__sram_text static void radar_framework_motion_point_deinit(PointCloudBuffer_t** ptr_point_cloud_buffer) {
	if(ptr_point_cloud_buffer && *ptr_point_cloud_buffer) {
		mmw_process_mem_free((void**) &(*ptr_point_cloud_buffer)->ptr_motion_point_cloud_data);
		mmw_process_mem_free((void**) ptr_point_cloud_buffer);
	}
}

__sram_text static int radar_framework_motion_point_process(uint64_t frame_start_timestamp, PointCloudBuffer_t** ptr_point_cloud_buffer) {
#if CONFIG_MMW_SW_CFAR_ENABLE
	if (0 == mmw_fft_autogain_get()) {
		*ptr_point_cloud_buffer = mmw_point_cloud_process_sw_cfar();
	} else {
		LOG_ERR("SW CFAR IS NOT SUPPORT WHEN FFT AUGO GAIN IS ON\n");
		*ptr_point_cloud_buffer = 0;
		return MMW_ERR_CODE_UNSUPPORT;
	}
#else
	*ptr_point_cloud_buffer = mmw_point_cloud_process();
#endif	/* CONFIG_MMW_SW_CFAR_ENABLE */
	/* CheckPoint of calculate overtime, if over time, software may read error datacube or cfar list,
	 * so discard current frame */
	if (frame_start_timestamp != mmw_frame_start_timestamp()) {
		return MMW_ERR_CODE_TIMEOUT;
	}
	return MMW_ERR_CODE_SUCCESS;
}

__sram_text static void radar_framework_presence_point_deinit(PresencePointCloudBuffer_t** ptr_presence_point_cloud_buffer) {
	if(ptr_presence_point_cloud_buffer && *ptr_presence_point_cloud_buffer) {
		mmw_process_mem_free((void **) &(*ptr_presence_point_cloud_buffer)->ptr_presence_point_cloud_data);
		mmw_process_mem_free((void **) ptr_presence_point_cloud_buffer);
	}
}

__sram_text int radar_framework_2d_frame_cb(void *mmw_data, void *arg) {
	uint64_t frame_start_timestamp = mmw_frame_start_timestamp();
	PointCloudBuffer_t *ptr_point_cloud_buffer = 0;
	PresencePointCloudBuffer_t *ptr_presence_point_cloud_buffer = 0;
	void *ptr_argv[3];
	int err_code = 0;
	uint32_t frame_idx;
	uint32_t time_cb_start, time_cb_end;
	uint8_t convergence_flag;
	uint16_t offset_0interval_rfft = 0;
	mmw_application_ops_t* ptr_application_cb = radar_framework_get_application_ops();
	const RadarConfig_t* ptr_radar_config = radar_framework_get_config_const();
	bool micro_update = 0;

	frame_idx = radar_framework_increase_frame_idx();
	(void)micro_update;
	(void)frame_idx;
	(void)frame_start_timestamp;
	(void)time_cb_start;
	(void)time_cb_end;
	(void)offset_0interval_rfft;
	if(0) goto ERR_RELEASE_ALL;

	time_cb_start = HAL_BOARD_GetTime(HAL_TIME_US);

	mmw_dsp_poweron();
	convergence_flag = radar_framework_clutter_convergence_process(frame_idx, mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_rm_method);

	/* Call user entry callback functions */
	if(ptr_application_cb && ptr_application_cb->mmw_entry_proc_cb){
		ptr_application_cb->mmw_entry_proc_cb(frame_idx);
	}

	/* Set the buffer before point process to prevent data errors caused by the computing process */
#if CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN
	offset_0interval_rfft = interval0_rfft_buffer_set(frame_idx);
#endif

#if CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE
    radar_framework_ctrl_data_cube_report_cb(mmw_data, arg);
#endif

	if (ptr_radar_config && (ptr_radar_config->radar_frame_config.fft_mode == RADAR_FRAMEWORK_2DFFT_MODE)) {
	#if CONFIG_MMW_MOTION_POINT_CLOUD
		err_code = radar_framework_motion_point_process(frame_start_timestamp, &ptr_point_cloud_buffer);
		if (err_code) {
			goto ERR_RELEASE_ALL;
		}
	#endif

	#if (CONFIG_MMW_PRESENCE_POINT_CLOUD)
		micro_update = mmw_micro_point_frame();
		/* CheckPoint of calculate overttime, if over time, software may read error datacube or cfar list,
		 * so discard current frame */
		if (frame_start_timestamp != mmw_frame_start_timestamp()) {
			err_code = MMW_ERR_CODE_TIMEOUT;
			goto ERR_RELEASE_ALL;
		}
	#endif
	}


	/* Auto gain function must be cleared manually as soon as gain factor is not used,
	* to make auto gain in next frame works well.
	*  Hardware reuired gain factor cleared before chirp transmit period.
	* In this application, only radar_framework_motion_point_process() and mmw_micro_point_frame() may access datacube,
	* together with auto gain factor, once these 2 functions are called, application better call this function,
	* thus, when next frame arrives during following process, hardawre may re-calculate right auto gain factor.
	*/
	if (mmw_fft_autogain_get()){
		mmw_fft_autogain_clear();
	}

#if (CONFIG_MMW_PRESENCE_POINT_CLOUD)
	/* mmw frames down sampling for micro frame.
	 * During down sampling period, MCU keeps storing micro frame, ‘micro_update’ will keep ‘0’,
	 * therefor process micro points when new micro frame is ready(‘micro_update’ = 1).
	 * */
	if (micro_update) {
		ptr_presence_point_cloud_buffer = mmw_presence_point_process();
	}
#endif

	/* Call user callback functions */
	if(ptr_application_cb && ptr_application_cb->mmw_post_proc_cb){
		ptr_argv[0] = &frame_idx;
		ptr_argv[1] = ptr_point_cloud_buffer;
		ptr_argv[2] = ptr_presence_point_cloud_buffer;
		ptr_application_cb->mmw_post_proc_cb(3, ptr_argv);
	}

	/* If convergenced, radar frame work will not auto update clutter, application must handle  */
	if (convergence_flag) {
		if (mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_rm_method == MMW_CLUTTER_REMOVAL_ALL) {
#if CONFIG_MMW_MOTION_POINT_CLOUD && (CONFIG_MMW_PRESENCE_POINT_CLOUD == 0)
			if (CLUTTER_AUTO_UPDATE == radar_framework_get_config_const()->radar_app_config.clutter_halt_method) {
				if (ptr_point_cloud_buffer->point_cloud_num) {
					mmw_clutter_halt_set(MMW_ENABLE);
				} else {
					mmw_clutter_halt_set(MMW_DISABLE);
				}
			}
#endif
		} else if (mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.clutter_rm_method == MMW_CLUTTER_REMOVAL_DC) {
			if (CLUTTER_HALT_AFTER_CONVERGENCE != radar_framework_get_config_const()->radar_app_config.clutter_halt_method) {
				mmw_psic_dc_suppression_update();
			}
		} else {
			;
		}
	}
	time_cb_end = HAL_BOARD_GetTime(HAL_TIME_US);

	/*
	 * To Avoid using the memory that has not been fully utilized,check whether there are still HIF tasks that have not been
	 * completed for the previous frame.If so,skip the data reporting for this frame.
	 */
#if (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE == 0)
	if (HIF_MsgReport_Flush(0)) {
		LOG_PRINT("The HIF task of the previous frame was not completed!\n");
		goto ERR_RELEASE_ALL;
	}
#endif

#if CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO
	uint16_t motion_pt_num = 0, presence_pt_num = 0;
	if(ptr_point_cloud_buffer) {
		motion_pt_num = ptr_point_cloud_buffer->point_cloud_num;
	}
	if (ptr_presence_point_cloud_buffer) {
		presence_pt_num = ptr_presence_point_cloud_buffer->presence_point_cloud_num;
	}

	radar_framework_report_frameInfo(frame_idx, time_cb_end - time_cb_start, motion_pt_num, presence_pt_num);
#endif

#if CONFIG_RADAR_FRAMWORK_REPORT_INTERVAL0_RFFT
	radar_framework_report_cube(g_1d_fft_data_buff + offset_0interval_rfft, frame_idx, NULL); /* upload range fft result in 2d frame mode */
#endif

#if (CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT) || (CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
	err_code = radar_framework_report_point_cloud_data(ptr_point_cloud_buffer, ptr_presence_point_cloud_buffer, frame_idx);
	if (err_code) {
		goto ERR_RELEASE_ALL;
	}
#endif

#if CONFIG_RADAR_FRAMEWORK_REPORT_HEART_BEAT
    uint16_t motion_num = 0, presence_num = 0;
    if(ptr_point_cloud_buffer) {
        motion_num = ptr_point_cloud_buffer->point_cloud_num;
    }
    if (ptr_presence_point_cloud_buffer) {
        presence_num = ptr_presence_point_cloud_buffer->presence_point_cloud_num;
    }
    radar_framework_report_heartBeat(motion_num, presence_num);
#endif

#if (CONFIG_MMW_PRESENCE_POINT_CLOUD)
	if (micro_update) {
		/* release the resources used for storing the motion point cloud computing results */
		radar_framework_presence_point_deinit(&ptr_presence_point_cloud_buffer);
	}
#endif

#if CONFIG_MMW_MOTION_POINT_CLOUD
	/* release the resources used for storing the motion point cloud computing results */
	radar_framework_motion_point_deinit(&ptr_point_cloud_buffer);
#endif

ERR_RELEASE_ALL:
	if (ptr_point_cloud_buffer) {
		radar_framework_motion_point_deinit(&ptr_point_cloud_buffer);
	}

	if (ptr_presence_point_cloud_buffer) {
		radar_framework_presence_point_deinit(&ptr_presence_point_cloud_buffer);
	}

	if (err_code) {
		if(err_code == MMW_ERR_CODE_TIMEOUT){
			LOG_ERR("Calculate TIMEOUT!\n");
		}
		/* Auto gain function must be cleared manually as soon as gain factor is not used,
		* to make auto gain in next frame works well.
		*  Hardware reuired gain factor cleared before chirp transmit period.
		* In this application, only radar_framework_motion_point_process() and mmw_micro_point_frame() may access datacube,
		* together with auto gain factor, once these 2 functions are called, application better call this function,
		* thus, when next frame arrives during following process, hardawre may re-calculate right auto gain factor.
		*/
		if (mmw_fft_autogain_get()){
			mmw_fft_autogain_clear();
		}
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

#if CONFIG_MMW_PRESENCE_POINT_CLOUD
int check_presence_point_fft_len_support(void)
{
	int ret = MMW_ERR_CODE_INVALID_PARAM;
	MMWPresencePointCloudUserCfg_t *ptr_cfg = mmw_presence_point_cloud_get_user_cfg();

	if (ptr_cfg) {
		if (ptr_cfg->micro_dop_fft_len == 16 || ptr_cfg->micro_dop_fft_len == 32) {
			ret = MMW_ERR_CODE_SUCCESS;
		} else {
			ret = MMW_ERR_CODE_INVALID_PARAM;
			LOG_ERR("Presence pointcloud fft len support 16 and 32 only!, current is %d\n", ptr_cfg->micro_dop_fft_len);
		}
		if (ptr_cfg->micro_chirp_num > ptr_cfg->micro_dop_fft_len) {
			ret = MMW_ERR_CODE_INVALID_PARAM;
			LOG_ERR("micro_chirp_num(%d) must be smaller than micro dop fft len(%d)\n", ptr_cfg->micro_chirp_num, ptr_cfg->micro_dop_fft_len);
		}
	}
	return ret;
}
#endif

int radar_signal_process_2d_app_init(void)
{
	int ret = 0;
	uint16_t range_fft_num, doppler_fft_num;
	const RadarConfig_t* ptr_radar_config = radar_framework_get_config_const();

	if (!ptr_radar_config) {
		ret = MMW_ERR_CODE_INVALID_PARAM;
		LOG_ERR("Radar config is not set!\n");
		goto CONFIG_ERR;
	}

	/******************** Parameter verifications ********************/
	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	/* Check heap configuration is valid */
	if (CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN < range_fft_num) {
		ret = MMW_ERR_CODE_NO_MEMORY;
		LOG_ERR("CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN set less than range fft number! Please check configurations!\n");
		goto CONFIG_ERR;
	} else if (CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN > range_fft_num) {
		LOG_PRINT("Heap size can be reduceed by setting CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN equal to max-used range_fft_num.\n");
	} else {
		;
	}
	if (ptr_radar_config->radar_frame_config.fft_mode != RADAR_FRAMEWORK_2DFFT_MODE) {
		uint32_t range_mm, range_reol_mm;
#if CONFIG_MMW_MOTION_POINT_CLOUD || CONFIG_MMW_PRESENCE_POINT_CLOUD
		LOG_PRINT("Point cloud WILL NOT be calculated if fft_mode is not in 2D-FFT mode!\n");
#endif		
		mmw_range_get(&range_mm, &range_reol_mm);
		/* If adc output is enabled, range fft number and doppler fft nunmbe should be power of 2 */
		if (range_mm != range_fft_num * range_reol_mm) {
			ret = MMW_ERR_CODE_UNSUPPORT;
			LOG_ERR("When ADC output is enabled, please set range fft length equal to adc sample number!\n");
			goto CONFIG_ERR;
		}
		mmw_velocity_get(&range_mm, &range_reol_mm);
		/* If adc output is enabled, range fft number and doppler fft nunmbe should be power of 2 */
		if (range_mm * 2 != doppler_fft_num * range_reol_mm) {
			ret = MMW_ERR_CODE_UNSUPPORT;
			LOG_ERR("When ADC output is enabled, please set doppler fft length equal to adc sample number!\n");
			goto CONFIG_ERR;
		}
	}

	/******************** Configuration for each module ********************/
#if CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE
	radar_framework_data_report_init(RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT, RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE);
	/* Configure param for datacube report */
	radar_framework_report_param_get();
#endif

#if CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN
	ret = radar_framework_obtain_1d_fft_data_init();	//init range fft result data that need to be reported in 2d frame mode
	if (ret) {
		LOG_ERR("obtain_1d_fft_data_init fail %d\n", ret);
		goto CONFIG_ERR;
	}
#endif

#if CONFIG_MMW_MOTION_POINT_CLOUD
	mmw_point_cloud_init(); /* init the para of point cloud process */
#endif

#if CONFIG_MMW_PRESENCE_POINT_CLOUD
	ret = check_presence_point_fft_len_support();
	if (ret != MMW_ERR_CODE_SUCCESS) {
		goto CONFIG_ERR;
	}
	mmw_angle_mount_type_set(mmw_point_cloud_get_user_cfg_const()->chip_mount_type);
	mmw_presence_point_init();
	mmw_presence_point_restart();
#endif

#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT) || (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
	ret = radar_framework_point_cloud_report_init();
	if (ret != MMW_ERR_CODE_SUCCESS) {
		goto CONFIG_ERR;
	}
#endif

CONFIG_ERR:
	return ret;
}

int radar_signal_process_2d_app_deinit(void)
{
#if CONFIG_MMW_MOTION_POINT_CLOUD
	mmw_point_cloud_deinit();
#endif
#if CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN
	radar_framework_obtain_1d_fft_data_deinit();
#endif
#if CONFIG_MMW_PRESENCE_POINT_CLOUD
	mmw_presence_point_deinit();
#endif
#if CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE
	radar_framework_data_cube_report_deinit();
#endif
#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT) || (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
	radar_framework_point_cloud_report_deinit();
#endif
	return 0;
}

#endif	/* #if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN */