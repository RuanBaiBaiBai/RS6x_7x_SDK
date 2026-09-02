/**
 **************************************************************************************************
 * @brief   factory test function define.
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
 
#include <math.h>
#include "ft_angle_est.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_point_cloud_psic_lib.h"
#include "csi_math.h"
#include "log.h"

extern void fft_autogain_restore_f32();
extern int mmw_ant_calib_data_load(void);
extern  int KVF_Load(uint16_t id, void *pdata, uint16_t size);
extern int KVF_Save(uint16_t id, void *pdata, uint32_t size);

extern void FT_RspSemRelease(void);

#if CONFIG_SOC_RS6240
    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t ft_azi_geometry[4] = {0, 1, 4, 5};
    uint8_t ft_elev_geometry[4] = {3, 2, 1};
    MmwPointAntGeometry_t ft_azi_geometry_struct = {
        .ptr_geometry = ft_azi_geometry,
        .geometry_len = 4
    };
    MmwPointAntGeometry_t ft_elev_geometry_struct = {
        .ptr_geometry = ft_elev_geometry,
        .geometry_len = 3
    };
#elif CONFIG_SOC_RS6130
    uint8_t ft_azi_geometry[2] = {2, 1};
    uint8_t ft_elev_geometry[2] = {0, 1};
    MmwPointAntGeometry_t ft_azi_geometry_struct = {
        .ptr_geometry = ft_azi_geometry,
        .geometry_len = 2
    };
    MmwPointAntGeometry_t ft_elev_geometry_struct = {
        .ptr_geometry = ft_elev_geometry,
        .geometry_len = 2
    };
#elif CONFIG_BOARD_MRS7241_P2840_CPUF
    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t ft_azi_geometry[8] = {4, 5, 6, 7, 0, 1, 2, 3};
    uint8_t ft_elev_geometry[2] = {0};
    MmwPointAntGeometry_t ft_azi_geometry_struct = {
        .ptr_geometry = ft_azi_geometry,
        .geometry_len = 8
    };
    MmwPointAntGeometry_t ft_elev_geometry_struct = {
        .ptr_geometry = ft_elev_geometry,
        .geometry_len = 0
    };
#elif CONFIG_BOARD_MRS7241_P2828_CPUF
    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t ft_azi_geometry[6] = {4, 0, 6, 2, 7, 3};
    uint8_t ft_elev_geometry[6] = {5, 0};
    MmwPointAntGeometry_t ft_azi_geometry_struct = {
        .ptr_geometry = ft_azi_geometry,
        .geometry_len = 6
    };
    MmwPointAntGeometry_t ft_elev_geometry_struct = {
        .ptr_geometry = ft_elev_geometry,
        .geometry_len = 2
    };
#elif CONFIG_BOARD_MRS6241_P2840_M81_CPUF
    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t ft_azi_geometry[8] = {4, 5, 6, 7, 0, 1, 2, 3};
    uint8_t ft_elev_geometry[2] = {0};
    MmwPointAntGeometry_t ft_azi_geometry_struct = {
        .ptr_geometry = ft_azi_geometry,
        .geometry_len = 8
    };
    MmwPointAntGeometry_t ft_elev_geometry_struct = {
        .ptr_geometry = ft_elev_geometry,
        .geometry_len = 0
    };
#elif CONFIG_BOARD_MRS6241_P2828_M62_CPUF
	/* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t ft_azi_geometry[6] = {4, 0, 6, 2, 7, 3};
    uint8_t ft_elev_geometry[6] = {5, 0};
    MmwPointAntGeometry_t ft_azi_geometry_struct = {
        .ptr_geometry = ft_azi_geometry,
        .geometry_len = 6
    };
    MmwPointAntGeometry_t ft_elev_geometry_struct = {
        .ptr_geometry = ft_elev_geometry,
        .geometry_len = 2
    };
#else
    #error "Please Set User-Defined Ant Geometry!"
#endif

extern int mmw_ant_calib_data_get(uint32_t *data, uint32_t len);
extern int mmw_ant_calib_data_set(uint32_t *data, uint32_t len);

extern struct mmw_angle_est_frame_t angle_frame;

struct mmw_angle_est_task_status_t ft_angle_est_task_status_obj;

struct radar_angle_est_t *angle_est_data = 0;

struct mmw_angle_est_report_t *angle_est_report = 0;

FT_AngleCalibState_t g_ft_cailb_state = FT_CALIB_STATE_FAIL;

FT_AngleCalculateData_t g_ft_cal_data = FT_CAL_DATA_FLASH;

/* this buffer use for storage iq data when angle calib is excuted successfully */
complex16_cube *ft_angle_pos_iq_data = 0;
/* this buffer use for storage angle calib data */
complex16_cube *ft_angle_calib_data = 0;

complex16_cube** get_ft_calib_ant_iq_data(void){
	return &ft_angle_pos_iq_data;
}

complex16_cube** get_ft_calib_data(void){
	return &ft_angle_calib_data;
}

struct radar_angle_est_t** get_radar_angle_est_param(void){
	return &angle_est_data;
}

struct mmw_angle_est_report_t** get_radar_angle_est_report(void){
	return &angle_est_report;
}

struct mmw_angle_est_task_status_t* get_angle_est_task_status(void) {
	return &ft_angle_est_task_status_obj;
}

FT_AngleCalibState_t* get_ft_angle_calib_status(void) {
	return &g_ft_cailb_state;
}

void set_ft_angle_calib_status(FT_AngleCalibState_t state) {
	g_ft_cailb_state = state;
}

void set_ft_angle_calculate_data(FT_AngleCalculateData_t data) {
	g_ft_cal_data = data;
}	

/**  * @brief Fourth-order Taylor series expansion formula for arcsine function
     * arcsin(x) is similar to x + x3/6 + 3*x5/40 + 5*x7/112
     * Convert all division operations to constant multiplication for embedded high-speed calculation
     * 1/6 = 0.166667    3/40 = 0.075    5/112 = 0.0446429
	 * @param sin_value input sin value.
	 * @return return rad value. 
	 */
float asin_poly3(float sin_value)
{
	float rad = 0.f;
	if (sin_value < -1) {
		return -3.1514926f * 0.5f;	// -pi / 2
	}
	if (sin_value > 1) {
		return 3.1514926f * 0.5f;	// pi / 2
	}
	
	float x = sin_value;
	float x2 = x * x;
	float x3 = x2 * x;
	float x5 = x3 * x2;
	float x7 = x5 * x2;
	
	rad = x + x3 * 0.166667f + x5 * 0.075f + x7 * 0.446429f;
	return  rad;
}

/** @brief set angle estimation param */
void ft_angle_est_param_set(struct mmw_angle_est_recive_param_t *param)
{
	if (param == NULL) {
		return;
	}
	
	if (param->start_freq == 0) {
		angle_frame.start_freq = FT_DEFAULT_START_FREQ;
	} else {
		angle_frame.start_freq = param->start_freq;
	}
	
	angle_frame.mount_type = param->mount_type;

	if (param->range_mm == 0) {
		angle_frame.range_mm = FT_DEFAULT_RANGE;
	} else {
		angle_frame.range_mm = param->range_mm;
	}

	if (param->range_res_mm == 0) {
		angle_frame.range_res_mm = FT_DEFAULT_RANGE_RES;
	} else {
		angle_frame.range_res_mm = param->range_res_mm;
	}
}

/** @brief 
 *  1. config mmw wave use user param or default param 
 *  2. enable fft autogain */
int mmw_wave_angle_est_frame_config(struct mmw_angle_est_frame_t *pFrame)
{
	int ret = 0;
	uint8_t trx_mimo = FT_ANGLE_EST_CONFIG_MIMO_MODE, work_mode = FT_ANGLE_EST_CONFIG_WORK_MODE;
	uint32_t end_freq = FT_DEFAULT_END_FREQ;
	uint32_t velocity_mm = FT_ANGLE_EST_CONFIG_VELOC, veloc_resol = FT_ANGLE_EST_CONFIG_VELOC_RES;
	uint32_t frame_period_ms = ANGLE_EST_DEFAULT_FRAME_PERIOD, frame_num = ANGLE_CALIB_DEFAULT_FRAME_NUM;
	uint8_t clutter_mode = MMW_CLUTTER_MODE_DISABLE;
	uint32_t chirp_num = ANGLE_EST_DEFAULT_CHIRP_NUM;

	if (pFrame == NULL) {
		return -1;
	}
	
	
	do {
		/* config for mmw chirp */
		ret = mmw_mode_cfg (trx_mimo, work_mode);
		if (ret) {
			LOG_PRINT("mmw_mode_cfg error\n");
			break;
		}

		ret = mmw_freq_cfg (pFrame->start_freq, end_freq);
		if (ret) {
			LOG_PRINT("mmw_freq_cfg error\n");
			break;
		}

		ret = mmw_range_cfg(pFrame->range_mm, pFrame->range_res_mm);
		if (ret) {
			LOG_PRINT("mmw_range_cfg error\n");
			break;
		}

		ret = mmw_velocity_cfg(velocity_mm, veloc_resol);
		if (ret) {
			LOG_PRINT("mmw_velocity_cfg error\n");
			break;
		}

		ret = mmw_frame_cfg(frame_period_ms, frame_num);
		if (ret) {
			LOG_PRINT("mmw_frame_cfg error: %d\n", ret);
			break;
		}

		ret = mmw_clutter_remove(clutter_mode);
		if (ret) {
			LOG_PRINT("mmw_clutter_remove error\n");
			break;
		}
	
		ret = mmw_chirp_num_cfg(chirp_num);
		if (ret) {
			LOG_PRINT("mmw_chirp_num_cfg error\n");
			break;
		}
	
		FT_LOG_PRINT("mimo: %d, work_mode: %d, start_freq: %d, end_freq: %d, range_mm: %d, resol_mm: %d, vel_mm: %d, vel_res: %d, frame_period: %d, frame_num: %d, clutter_mode: %d, chirp_num:%d\r\n",
						trx_mimo, work_mode, pFrame->start_freq, end_freq, pFrame->range_mm, pFrame->range_res_mm, velocity_mm, veloc_resol,
						frame_period_ms, frame_num, clutter_mode, chirp_num);
		/* Enable autogain */
		mmw_fft_autogain_set(1);
	} while(0);
	
	return ret;	
}

int mmw_radar_angle_est_init(uint8_t angle_est_mode){
	
	uint8_t ret = 0;
	uint32_t range_mm, resol_mm;
	uint8_t txrx, work;
	uint16_t range_min_mm = ANGLE_EST_DEFAULT_RANGE_MIN_MM;
	uint16_t range_max_mm = ANGLE_EST_DEFAULT_RANGE_MAX_MM; 	/* min 0.9m, max 1.4m */
	
	if (ft_angle_est_task_status_obj.running == true) {
		ret = -1;
		goto exit;
	}	
	
	/* this buffer deinit when quit ft-mode */
	if (ft_angle_calib_data == NULL) {
		mmw_process_mem_alloc((void **)&ft_angle_calib_data, sizeof(*ft_angle_calib_data) * CALIB_DATA_NUM);
		if (ft_angle_calib_data == NULL) {
			ret = -1;
			goto exit;
		}
	}
	if (ft_angle_pos_iq_data == NULL) {
		mmw_process_mem_alloc((void **)&ft_angle_pos_iq_data, sizeof(*ft_angle_pos_iq_data) * CONFIG_MAX_MIMO_ANT);
		if (ft_angle_pos_iq_data == NULL) {
			ret = -1;
			goto exit;
		}
	}
	
	
	if (angle_est_data == NULL) {
		mmw_process_mem_alloc((void **)&angle_est_data, sizeof(*angle_est_data));
		if (angle_est_data == NULL) {
			mmw_radar_angle_est_deinit();
			ret = -1;
			goto exit;
		}
	}
	
	if (angle_est_report == NULL) {
		mmw_process_mem_alloc((void **)&angle_est_report, sizeof(*angle_est_report));
		if (angle_est_report == NULL) {
			mmw_radar_angle_est_deinit();
			ret = -1;
			goto exit;
		}
	}
	angle_est_report->fail_msk = 0;
	
	memset(angle_est_data->angle_est_data_buf, 0, sizeof(angle_est_data->angle_est_data_buf));
	mmw_fft_num_get(&angle_est_data->angle_est_usr_param.range_len, &angle_est_data->angle_est_usr_param.doppler_len);
	mmw_mode_get (&txrx, &work);
	if (txrx != FT_ANGLE_EST_CONFIG_MIMO_MODE) {
		return -1;
	}
	
	switch (txrx) {
		case MMW_MIMO_1T3R:
			angle_est_data->angle_est_usr_param.tx_num = 1;
			angle_est_data->angle_est_usr_param.rx_num = 3;
			break;
		case MMW_MIMO_2T4R:
			angle_est_data->angle_est_usr_param.tx_num = 2;
			angle_est_data->angle_est_usr_param.rx_num = 4;
			break;
		default:
			ret = -1;
			break;
	}
	
	mmw_range_get(&range_mm, &resol_mm);
	if (range_min_mm > range_mm ) {
		mmw_radar_angle_est_deinit();
		ret = -1;
		goto exit;
	}
	
	if (angle_est_mode == FT_ANGELE_CALIB_MODE) {
		g_ft_cailb_state = FT_CALIB_STATE_FAIL;
	} else if (angle_est_mode == FT_ANGELE_CALCULATE_MODE) {
		if (g_ft_cailb_state == FT_CALIB_STATE_SUCCESS) {
			g_ft_cal_data = FT_CAL_DATA_CALIBRATION;
		} else {
			g_ft_cal_data = FT_CAL_DATA_FLASH;
		}
	}
	
	angle_est_data->angle_est_usr_param.range_max_mm = range_max_mm;
	angle_est_data->angle_est_usr_param.range_min_mm = range_min_mm;
	angle_est_data->angle_est_usr_param.range_index_max = range_max_mm / resol_mm;
	angle_est_data->angle_est_usr_param.range_index_min = GET_ROUND_UP(range_min_mm, resol_mm);
	if (angle_est_data->angle_est_usr_param.range_index_max > angle_est_data->angle_est_usr_param.range_len) {
		angle_est_data->angle_est_usr_param.range_index_max = angle_est_data->angle_est_usr_param.range_len - 1;
	}
	
	FT_LOG_PRINT("range_len: %d, resol_mm:%d, range_index_min: %d, range_index_max: %d, dop_len: %d\n", 
				angle_est_data->angle_est_usr_param.range_len, resol_mm, angle_est_data->angle_est_usr_param.range_index_min, 
				angle_est_data->angle_est_usr_param.range_index_max, angle_est_data->angle_est_usr_param.doppler_len);

	ft_angle_est_task_status_obj.curr_frame_index = 0;
	ft_angle_est_task_status_obj.running = true;

exit:	
	return ret;
}

void mmw_radar_angle_est_deinit(void)
{
	if (angle_est_data != NULL) {
		mmw_process_mem_free((void **)&angle_est_data);
		angle_est_data = NULL;
	}
	if (angle_est_report != NULL) {
		mmw_process_mem_free((void **)&angle_est_report);
		angle_est_report = NULL;
	}
	
	mmw_ctrl_callback_cfg(NULL, MMW_DATA_TYPE_DISABLE, NULL);
	
	ft_angle_est_task_status_obj.running = false;
	ft_angle_est_task_status_obj.curr_frame_index = 0;
}

/** @brief find max power and max power index in each ant. */
uint16_t find_max_pwr_rbin_postion(Complexf32_RealImag *pBuf, uint16_t rbin_num, uint16_t start_offset, uint16_t ant_mimo_num)
{
	Complexf32_RealImag *rbin;
	float power = 0.f, max_power = 0.f;
	uint16_t rbinoff, max_pwr_rbin_idx = 0;
	
	for (uint16_t range_idx = start_offset; range_idx < rbin_num; range_idx++) {
		power = 0.f;
		for (uint8_t rx_idx = 0; rx_idx < ant_mimo_num; rx_idx++) {
			rbinoff = rx_idx * rbin_num;
			rbin = &pBuf[rbinoff + range_idx];
			power = power + rbin->real * rbin->real + rbin->imag * rbin->imag;
		}
		if (max_power < power) {
			max_power = power;
			max_pwr_rbin_idx = range_idx;
		}
	}

	return  max_pwr_rbin_idx;
}

/** @brief get 0 dop data, calculate power and find max power index */
int16_t ft_angle_calib_max_pwr_rbin_postion(uint16_t *rbin_max_pwr_idx)
{
	int16_t ret = 0;
	uint16_t rbin_num = angle_est_data->angle_est_usr_param.range_index_max + 1;
	uint16_t tx_num = angle_est_data->angle_est_usr_param.tx_num;
	uint16_t rx_num = angle_est_data->angle_est_usr_param.rx_num;
	uint16_t cube_num = rbin_num * rx_num * tx_num;
	uint16_t dop_len = angle_est_data->angle_est_usr_param.doppler_len;
	uint16_t start_offset = angle_est_data->angle_est_usr_param.range_index_min;
	
	complex16_cube *angle_range_buffer = 0;
	Complexf32_RealImag *angle_range_buffer_f32 = 0;
	uint16_t cube_offset = 0;

	mmw_process_mem_alloc((void **)&angle_range_buffer, cube_num * sizeof(complex16_cube));
	if (angle_range_buffer == NULL) {
		ret = -1;
		goto exit;
	}
	
	mmw_process_mem_alloc((void **)&angle_range_buffer_f32, cube_num * sizeof(Complexf32_RealImag));
	if (angle_range_buffer_f32 == NULL) {
		ret = -1;
		goto exit;
	}
	
	for (uint16_t tx_id = 0; tx_id < tx_num; tx_id++) {
		for (uint16_t rx_id = 0; rx_id < rx_num; rx_id++) {
			/* get 0 dop data */
			mmw_process_fft_range_f32(&angle_range_buffer_f32[cube_offset], angle_range_buffer, rbin_num, tx_id, rx_id, dop_len >> 1);
			cube_offset += rbin_num;
		}
	}
	
	/* find max power and max power index in each ant. */
	*rbin_max_pwr_idx = find_max_pwr_rbin_postion(angle_range_buffer_f32, rbin_num, start_offset, tx_num * rx_num);

exit:
	if (angle_range_buffer) {
		mmw_process_mem_free((void **)&angle_range_buffer);
	}
	if (angle_range_buffer_f32) {
		mmw_process_mem_free((void **)&angle_range_buffer_f32);
	}
	return ret;
}

/** @brief storage sum of single bin data, for each ant in each frame 
 * 		   single bin data is from datacube in max power index, 0 doppler bin.
 *  @param max_pwr_idx: max power index
 *  @note single bin data should be restored fft autogain
 * */ 
void ft_angle_est_data_process(uint16_t max_pwr_idx)
{
	uint16_t ant_mimo_id = 0;
	uint16_t dop_len = angle_est_data->angle_est_usr_param.doppler_len;
	complex16_cube buf;
	Complexf32_RealImag buf_f32;
	
	for (uint16_t tx_id = 0; tx_id < angle_est_data->angle_est_usr_param.tx_num; tx_id++) {
		for (uint16_t rx_id = 0; rx_id < angle_est_data->angle_est_usr_param.rx_num; rx_id++) {
			mmw_fft_single_bin(&buf, tx_id, rx_id, dop_len >> 1, max_pwr_idx);
			fft_autogain_restore_f32(&buf, &buf_f32, tx_id, max_pwr_idx, 1);
			angle_est_data->angle_est_data_buf[ant_mimo_id].real += buf_f32.real;
			angle_est_data->angle_est_data_buf[ant_mimo_id].imag += buf_f32.imag;
			ant_mimo_id++;
		}
	}	
}

/** @brief angle calib process 
 *  @note for 624x, phase align at Rx0.
 * 		  for 6130, phase align at Rx1.
 * */
int mmw_range_angle_calib(struct radar_angle_est_t *pdata){
	
	uint16_t tx_num = pdata->angle_est_usr_param.tx_num;
	uint16_t rx_num = pdata->angle_est_usr_param.rx_num;
	
	Complexf32_RealImag *rx_ant_calib_data_f32 = 0;
	complex16_cube *rx_ant_calib_data_q = &pdata->rx_ant_calib_data[0];
	float pwr = 0.0f;
	float abs_tmp, abs_max = 0.f, amp_scale_factor;

	mmw_process_mem_alloc((void **)&rx_ant_calib_data_f32, CONFIG_MAX_MIMO_ANT * sizeof(Complexf32_RealImag));
	if (rx_ant_calib_data_f32 == NULL) {
		return -1;
	}
	
	/* for 624x, phase align at Rx0.
	 * for 6130, phase align at Rx1. */
#if CONFIG_SOC_SERIES_RS624X  || CONFIG_SOC_SERIES_RS724X
	uint16_t  align_ant_mimo_id = 0;
#elif CONFIG_SOC_SERIES_RS613X
	uint16_t  align_ant_mimo_id = 1;
#else
	#error "not support chip!"
#endif
	
	/* take 6130 as an example, phase align process as below:  
	 * calib[0] * Rx[0] = Rx[1]
	 * --> calib[0] = Rx[1] / Rx[0]
	 * */
	for (uint16_t ant_id = 0; ant_id < tx_num * rx_num; ant_id++) {
		/* (a+bi) / (c+di) 
		 * --> ( (ac + bd) + (bc - ad)i )/ (c * c + d * d)
		 * real: (ac + bd) / (c * c + d * d)
		 * imag: (bc - ad) / (c * c + d * d)  */
		pwr = pdata->angle_est_data_buf[ant_id].real * pdata->angle_est_data_buf[ant_id].real
			+ pdata->angle_est_data_buf[ant_id].imag * pdata->angle_est_data_buf[ant_id].imag;
		rx_ant_calib_data_f32[ant_id].real = (pdata->angle_est_data_buf[align_ant_mimo_id].real * pdata->angle_est_data_buf[ant_id].real
										   + pdata->angle_est_data_buf[align_ant_mimo_id].imag * pdata->angle_est_data_buf[ant_id].imag) / pwr;
		rx_ant_calib_data_f32[ant_id].imag = (pdata->angle_est_data_buf[align_ant_mimo_id].imag * pdata->angle_est_data_buf[ant_id].real
										   - pdata->angle_est_data_buf[align_ant_mimo_id].real * pdata->angle_est_data_buf[ant_id].imag) / pwr;
		
		abs_tmp = rx_ant_calib_data_f32[ant_id].real > 0 ? rx_ant_calib_data_f32[ant_id].real : - rx_ant_calib_data_f32[ant_id].real;
		abs_max = (abs_max > abs_tmp) ? abs_max: abs_tmp;
		abs_tmp = rx_ant_calib_data_f32[ant_id].imag > 0 ? rx_ant_calib_data_f32[ant_id].imag : - rx_ant_calib_data_f32[ant_id].imag;
		abs_max = (abs_max > abs_tmp) ? abs_max: abs_tmp;
		FT_LOG_PRINT("[%d] abs_max:%f\n", ant_id, abs_max);
		FT_LOG_PRINT("phase calib data: [%d] %f, %f\n", ant_id, rx_ant_calib_data_f32[ant_id].real, rx_ant_calib_data_f32[ant_id].imag);
	}
	
	/* scale real/imag close to q15(6240)/q11(6130) max */
	amp_scale_factor = 1 / abs_max;
	FT_LOG_PRINT("amp_scale_factor=%f\n",amp_scale_factor);
	for (uint16_t ant_id = 0; ant_id < tx_num * rx_num; ant_id++) {
		rx_ant_calib_data_f32[ant_id].real *= amp_scale_factor;
		rx_ant_calib_data_f32[ant_id].imag *= amp_scale_factor;
		FT_LOG_PRINT("amp calib data: [%d] %f, %f\n", ant_id, rx_ant_calib_data_f32[ant_id].real, rx_ant_calib_data_f32[ant_id].imag);
#if CONFIG_SOC_SERIES_RS624X  || CONFIG_SOC_SERIES_RS724X
		rx_ant_calib_data_q[ant_id].imag = (int16_t)(rx_ant_calib_data_f32[ant_id].imag * 32768);
		rx_ant_calib_data_q[ant_id].real = (int16_t)(rx_ant_calib_data_f32[ant_id].real * 32768);
#elif CONFIG_SOC_SERIES_RS613X
		rx_ant_calib_data_q[ant_id].imag = (int16_t)(rx_ant_calib_data_f32[ant_id].imag * 2048);
		rx_ant_calib_data_q[ant_id].real = (int16_t)(rx_ant_calib_data_f32[ant_id].real * 2048);
#else
		#error "please select valid board!"
#endif
	}
	
	mmw_process_mem_free((void **)&rx_ant_calib_data_f32);
	
	/* storage calib data */
	memcpy(ft_angle_calib_data, rx_ant_calib_data_q, sizeof(*ft_angle_calib_data) * CONFIG_MAX_MIMO_ANT);
#if CONFIG_SOC_SERIES_RS613X
	ft_angle_calib_data[3].real = 2048;
	ft_angle_calib_data[3].imag = 0;
	ft_angle_calib_data[4].real = 1 << 12; 
	ft_angle_calib_data[4].imag = 15561;
#endif	

	set_ft_angle_calib_status(FT_CALIB_STATE_SUCCESS);
	return 0;
}

/**@brief this function is use to get calib data in flash */
int mmw_ant_calib_data_get_complex16(complex16_cube* calib_data_complex16){
    /* 
     * Ant calib data save format in flash:
	 * RS6130: 
     * complex16_reim ant_0,
     * complex16_reim ant_1,
     * complex16_reim -ant_2.
     * RS6240:
     * complex16_reim ant_0,
     * complex16_reim ant_1,
     * complex16_reim ant_2,
     * complex16_reim ant_3,
     * complex16_reim ant_4,
     * complex16_reim ant_5,
     * complex16_reim ant_6,
     * complex16_reim ant_7,
     * complex16_reim ant_8,
	 * ant calib data save format in storage is: imag, real.
	 * complex16_cube save format in storage is: real, imag.
	 * therefore conver complex16_reim to complex16_cube.
     * */
	
	uint32_t chip_info[CALIB_DATA_NUM] = { 0 };
	int calib_data_len_byte = CALIB_DATA_NUM * 4;
	
#if CONFIG_SOC_SERIES_RS624X  || CONFIG_SOC_SERIES_RS724X
	int ret_len = KVF_Load(HWINFO_ID_NVM_ANT_CALIB_2T4R, (uint8_t *)&chip_info[0], calib_data_len_byte);
#elif CONFIG_SOC_SERIES_RS613X
	int ret_len = KVF_Load(HWINFO_ID_NVM_ANT_CALIB_1T3R, (uint8_t *)&chip_info[0], calib_data_len_byte);
#else
	#error "not support chip!"
#endif
	
	LOG_PRINT("MIMO Calib Load %d\n", ret_len);
	if (ret_len != calib_data_len_byte) {
		return -1;
	}
	for (int calib_idx = 0; calib_idx < CALIB_DATA_NUM; calib_idx++) {
		calib_data_complex16[calib_idx].real = chip_info[calib_idx] & 0xFFFF;
		calib_data_complex16[calib_idx].imag = chip_info[calib_idx] >> 16;
	}
#if CONFIG_SOC_SERIES_RS613X
	calib_data_complex16[2].real = -calib_data_complex16[2].real;
	calib_data_complex16[2].imag = -calib_data_complex16[2].imag;
#endif
	return 0;
}

/** @brief angle calculate process 
 *  @note Channel calibration should be manualy done for all mimo_rx,
 * 		  RS6130 use hardware calibration data, and is auto-calibrated when generate DataCube
 * */
int16_t mmw_ft_angle_calculate(void){

	int16_t status = 0;
	float sin_azi, sin_elev;
		
	Complexf32_RealImag *ant_aligned_buf = 0;
	
	mmw_process_mem_alloc((void **)&ant_aligned_buf, CONFIG_MAX_MIMO_ANT * sizeof(Complexf32_RealImag));
	if (ant_aligned_buf == NULL) {
		angle_est_data->azimuth_deg = 0; 
		angle_est_data->elevation_deg = 0;
		status = -1;
		goto exit;
	}
	
	/* 
	* Channel calibration should be manualy done for all mimo_rx,
	* RS6130 use hardware calibration data, and is auto-calibrated when generate DataCube
	*/
#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
		uint16_t tx_num = angle_est_data->angle_est_usr_param.tx_num;
		uint16_t rx_num = angle_est_data->angle_est_usr_param.rx_num;
		Complexf32_RealImag *calib_data_f32 = 0;
		
		mmw_process_mem_alloc((void **)&calib_data_f32, CONFIG_MAX_MIMO_ANT * sizeof(Complexf32_RealImag));
		if (calib_data_f32 == NULL) {
			angle_est_data->azimuth_deg = 0; 
			angle_est_data->elevation_deg = 0;
			status = -1;
			goto exit;
		}
		
		if (g_ft_cal_data == FT_CAL_DATA_FLASH) { // use calib data in flash
			int ret = mmw_ant_calib_data_get_complex16(angle_est_data->rx_ant_calib_data);
			if (ret < 0) {
				angle_est_data->azimuth_deg = 0; 
				angle_est_data->elevation_deg = 0;
				status = -1;
				goto exit;
			}
		}
		for (uint16_t ant_id = 0; ant_id < tx_num * rx_num; ant_id++) {
			calib_data_f32[ant_id].real = angle_est_data->rx_ant_calib_data[ant_id].real * 0.000030517578125f;	// q15 --> float
			calib_data_f32[ant_id].imag = angle_est_data->rx_ant_calib_data[ant_id].imag * 0.000030517578125f;
			FT_LOG_PRINT("[%d] 0x%x %x\n", ant_id, angle_est_data->rx_ant_calib_data[ant_id].real, angle_est_data->rx_ant_calib_data[ant_id].imag);
		}
		mmw_process_ant_data_calibrate_f32(angle_est_data->angle_est_data_buf, ant_aligned_buf, &calib_data_f32[0], tx_num * rx_num);
#elif CONFIG_SOC_SERIES_RS613X
		memcpy(ant_aligned_buf, angle_est_data->angle_est_data_buf, sizeof(angle_est_data->angle_est_data_buf));
		uint16_t tx_num = angle_est_data->angle_est_usr_param.tx_num;
		uint16_t rx_num = angle_est_data->angle_est_usr_param.rx_num;
		for (uint16_t ant_id = 0; ant_id < tx_num * rx_num; ant_id++) {
			FT_LOG_PRINT("[%d] 0x%x %x\n", ant_id, angle_est_data->rx_ant_calib_data[ant_id].real, angle_est_data->rx_ant_calib_data[ant_id].imag);
		}
#else
		#error "not support chip!"
#endif

	uint8_t ret = mmw_process_angle_process(ant_aligned_buf, &ft_azi_geometry_struct, &ft_elev_geometry_struct, &sin_azi, &sin_elev, 0, 0);
	if (ret < 0) {
		LOG_PRINT("error angle process\n");
		status = -1;
		goto exit;
	}

#if CONFIG_BOARD_MRS7241_P2840_CPUF
	/* MRS7241_P2840 only support calculate azimuth */
	/* deg = rad * 180 / pi = rad * 180 / 3.1415926 = rad * 180 * 0.31830988618379067153803535746773 */
	float azi_deg = asin_poly3(sin_azi) * 180.f * 0.31830988618f;
	float ele_deg = 0.f;
#elif CONFIG_SOC_SERIES_RS613X || CONFIG_SOC_SERIES_RS624X || CONFIG_BOARD_MRS7241_P2828_CPUF
	float ele_rad = asin_poly3(sin_elev);
	float azi_rad = sin_azi / psic_lib_cosf(ele_rad);
	if ( azi_rad < -1 || azi_rad > 1) {
		angle_est_data->azimuth_deg = 0; 
		angle_est_data->elevation_deg = 0;
		LOG_PRINT("error angle\n");
		status = -1;
		goto exit;
	}
	/* deg = rad * 180 / pi = rad * 180 / 3.1415926 = rad * 180 * 0.31830988618379067153803535746773 */
	float azi_deg = asin_poly3(azi_rad) * 180.f * 0.31830988618f;
	float ele_deg = ele_rad * 180.f * 0.31830988618f;
#else
	#error "please select valid board!"
#endif
	FT_LOG_PRINT("deg: %f, %f\n", azi_deg, ele_deg);
	
	angle_est_data->azimuth_deg = (int)(azi_deg * 1024); // float --> q10
	angle_est_data->elevation_deg = (int)(ele_deg * 1024);
	
exit:	
	if (ant_aligned_buf != NULL) {
		mmw_process_mem_free((void **)&ant_aligned_buf);
	}
#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
	if (calib_data_f32 != NULL) {
		mmw_process_mem_free((void **)&calib_data_f32);
	}
#endif	
	return status;
}

/** @brief main angle process function
 * 1.find max power range bin index
 * 2.storage 0 dop data in max power range bin index for each ant
 * mode: angle calibrate
 * 		(1) phase align
 * 		(2) scale real/imag of calib data to q11/q15 max
 * 		(3) set calib data into registers
 * 		(4) execute angle calculate.
 * */
static uint16_t angle_est_rbin_max_pwr_idx;
static int mmw_ctrl_angle_calib_frame_cb(void *mmw_data, void *arg) {
	
	ft_angle_est_task_status_obj.curr_frame_index++;
	FT_LOG_PRINT("Radar callback frame: %d\n", ft_angle_est_task_status_obj.curr_frame_index);
	if (ft_angle_est_task_status_obj.curr_frame_index <= CONFIG_MMW_VENDOR_FILTER_FRAME_NUM) {
		return 0;
	}
	mmw_motion_cube_access_open();
	
	if (ft_angle_est_task_status_obj.curr_frame_index == CONFIG_MMW_VENDOR_FILTER_FRAME_NUM + 1) {
		/* in first frame, find max power rbin index */
		int ret = ft_angle_calib_max_pwr_rbin_postion(&angle_est_rbin_max_pwr_idx);
		if (ret < 0) {
			LOG_PRINT("find max pwr rbin postion fail\n");
			return -1;
		}
		uint32_t range_mm, resol_mm;
		mmw_range_get(&range_mm, &resol_mm);
	
		angle_est_data->range = angle_est_rbin_max_pwr_idx * resol_mm;
		FT_LOG_PRINT("max_idx: %d, max_distence: %d\n",angle_est_rbin_max_pwr_idx, angle_est_rbin_max_pwr_idx * resol_mm);
	} 
	
	ft_angle_est_data_process(angle_est_rbin_max_pwr_idx);	
	mmw_fft_autogain_clear();	
	mmw_motion_cube_access_close();
	
	if (ft_angle_est_task_status_obj.curr_frame_index == ANGLE_CALIB_DEFAULT_FRAME_NUM) {
		uint16_t tx_num = angle_est_data->angle_est_usr_param.tx_num;
		uint16_t rx_num = angle_est_data->angle_est_usr_param.rx_num;

		/* get avge of ANGLE_CALIB_DEFAULT_FRAME_NUM */
		for (uint16_t ant_id = 0; ant_id < tx_num * rx_num; ant_id++) {
			angle_est_data->angle_est_data_buf[ant_id].real /=  (ANGLE_CALIB_DEFAULT_FRAME_NUM - CONFIG_MMW_VENDOR_FILTER_FRAME_NUM);
			angle_est_data->angle_est_data_buf[ant_id].imag /=  (ANGLE_CALIB_DEFAULT_FRAME_NUM - CONFIG_MMW_VENDOR_FILTER_FRAME_NUM);
		}
		/* in last frame, execute angle calib */
		
		/* set iq data which use for calculate calib data */
		for (int calib_idx = 0; calib_idx < tx_num * rx_num; calib_idx++) {
			ft_angle_pos_iq_data[calib_idx].imag = (int16_t)(angle_est_data->angle_est_data_buf[calib_idx].imag * 32768);
			ft_angle_pos_iq_data[calib_idx].real = (int16_t)(angle_est_data->angle_est_data_buf[calib_idx].real * 32768);
		}
		/* execute angle calib */
		uint8_t ret = mmw_range_angle_calib(angle_est_data);
		if (ret < 0) {
			/* calib fail, not set data to registers, get calib data from flash to calculate angle */
			int ret_status = mmw_ant_calib_data_get_complex16(angle_est_data->rx_ant_calib_data);
			if (ret_status < 0) {
				return -1;
			}
		} else {
#if CONFIG_SOC_SERIES_RS613X
			/* for RS6130, calib success, set calib data into registers */
			mmwc_set_angle_calib_data(angle_est_data->rx_ant_calib_data, tx_num * rx_num);
#elif CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
			/* for RS6240, can not set calib data into registers. 
			 * in function: mmw_range_angle_calib(), calib data has storage in angle_est_data.rx_ant_calib_data.
			 * before calculate angle, in channel calibration, datacube will multiply calib data */
			;
#else
			#error "not support chip!"
#endif
		}
		
		/* calib finish */
		ft_angle_est_finish(angle_est_data, angle_est_report, FT_ANGELE_CALIB_MODE);
	
	}

	return 0;
}

/** @brief main angle process function
 * 1.find max power range bin index
 * 2.storage 0 dop data in max power range bin index for each ant
 * 3.angle calculate 
 * 		for RS6240, get calib data from calibration result or flash, and execute ant align.
 * 		for RS6130, use hardware calibration data, and is auto-calibrated when generate DataCube
 * 		after ant align, execute angle caculate process.
 * */
static int mmw_ctrl_angle_calculate_frame_cb(void *mmw_data, void *arg) {
	
	ft_angle_est_task_status_obj.curr_frame_index++;
	FT_LOG_PRINT("Radar callback frame: %d\n", ft_angle_est_task_status_obj.curr_frame_index);
	if (ft_angle_est_task_status_obj.curr_frame_index <= CONFIG_MMW_VENDOR_FILTER_FRAME_NUM) {
		return 0;
	}
	mmw_motion_cube_access_open();
	
	if (ft_angle_est_task_status_obj.curr_frame_index == CONFIG_MMW_VENDOR_FILTER_FRAME_NUM + 1) {
		/* in first frame, find max power rbin index */
		int ret = ft_angle_calib_max_pwr_rbin_postion(&angle_est_rbin_max_pwr_idx);
		if (ret < 0) {
			LOG_PRINT("find max pwr rbin postion fail\n");
			return -1;
		}
		uint32_t range_mm, resol_mm;
		mmw_range_get(&range_mm, &resol_mm);
	
		angle_est_data->range = angle_est_rbin_max_pwr_idx * resol_mm;
		FT_LOG_PRINT("max_idx: %d, max_distence: %d\n",angle_est_rbin_max_pwr_idx, angle_est_rbin_max_pwr_idx * resol_mm);
	} 
	
	ft_angle_est_data_process(angle_est_rbin_max_pwr_idx);	
	mmw_fft_autogain_clear();	
	mmw_motion_cube_access_close();
	
	if (ft_angle_est_task_status_obj.curr_frame_index == ANGLE_CALIB_DEFAULT_FRAME_NUM) {
		/* in last frame, execute angle calculate */
		uint16_t tx_num = angle_est_data->angle_est_usr_param.tx_num;
		uint16_t rx_num = angle_est_data->angle_est_usr_param.rx_num;
		int16_t angle_calculate_ret;

		/* get avge of ANGLE_CALIB_DEFAULT_FRAME_NUM */
		for (uint16_t ant_id = 0; ant_id < tx_num * rx_num; ant_id++) {
			angle_est_data->angle_est_data_buf[ant_id].real /=  (ANGLE_CALIB_DEFAULT_FRAME_NUM - CONFIG_MMW_VENDOR_FILTER_FRAME_NUM);
			angle_est_data->angle_est_data_buf[ant_id].imag /=  (ANGLE_CALIB_DEFAULT_FRAME_NUM - CONFIG_MMW_VENDOR_FILTER_FRAME_NUM);
		}

		angle_calculate_ret = mmw_ft_angle_calculate();
		if (angle_calculate_ret < 0) {
			angle_est_report->fail_msk = 1;
			set_ft_angle_calib_status(FT_CALIB_STATE_FAIL);
		} else {
			set_ft_angle_calib_status(FT_CALIB_STATE_SUCCESS);
		}
		ft_angle_est_finish(angle_est_data, angle_est_report, FT_ANGELE_CALCULATE_MODE);

	}

	return 0;
}


/** @brief angle estimation finish, stop mmw wave and report result */
void ft_angle_est_finish(struct radar_angle_est_t *pdata,struct mmw_angle_est_report_t * report, uint8_t angle_est_mode){
	
	
	mmw_ctrl_stop();
	ft_angle_est_task_status_obj.running = false;
	ft_angle_est_task_status_obj.curr_frame_index = 0;
	
	if (angle_est_mode == FT_ANGELE_CALCULATE_MODE) {
		ft_angele_est_report(pdata, report);
	}
	
	FT_RspSemRelease();
}

/** @brief set ant geometry 
 *  @note the geometry is based on radar chip 
 * */
void ft_angle_est_set_ant_geometry(MmwMountType_e mount_type){
/* mount_type can be referred to enum define(in mmw_alg_pointcloud_typedef.h,line 12) */
#if CONFIG_SOC_RS6130
    ft_azi_geometry_struct.geometry_len = 2;
    ft_elev_geometry_struct.geometry_len = 2;

    if (mount_type == MMW_MOUNT_VERTICAL) {
        ft_azi_geometry[0] = 2;
        ft_azi_geometry[1] = 1;
        ft_elev_geometry[0] = 0;
        ft_elev_geometry[1] = 1;
    } else if (mount_type == MMW_MOUNT_HORIZONTAL) {
        ft_azi_geometry[0] = 1;
        ft_azi_geometry[1] = 0;
        ft_elev_geometry[0] = 2;
        ft_elev_geometry[1] = 1;        
    }
#elif CONFIG_SOC_RS6240
    if (mount_type == MMW_MOUNT_VERTICAL) {
        ft_azi_geometry[0] = 0;
        ft_azi_geometry[1] = 1;
        ft_azi_geometry[2] = 4;
        ft_azi_geometry[3] = 5;

        ft_elev_geometry[0] = 3;
        ft_elev_geometry[1] = 2;
        ft_elev_geometry[2] = 1;
        
        ft_azi_geometry_struct.geometry_len = 4;
        ft_elev_geometry_struct.geometry_len = 3;
    } else if (mount_type == MMW_MOUNT_HORIZONTAL) {
        ft_azi_geometry[0] = 1;
        ft_azi_geometry[1] = 2;
        ft_azi_geometry[2] = 3;
        ft_elev_geometry[0] = 0;
        ft_elev_geometry[1] = 1;
        ft_elev_geometry[2] = 4;
        ft_elev_geometry[3] = 5;
        ft_azi_geometry_struct.geometry_len = 3;
        ft_elev_geometry_struct.geometry_len = 4;
    }
#elif CONFIG_BOARD_MRS7241_P2840_CPUF
    ft_azi_geometry_struct.ptr_geometry[0] = 4;
    ft_azi_geometry_struct.ptr_geometry[1] = 5;
    ft_azi_geometry_struct.ptr_geometry[2] = 6;
    ft_azi_geometry_struct.ptr_geometry[3] = 7;
    ft_azi_geometry_struct.ptr_geometry[4] = 0;
    ft_azi_geometry_struct.ptr_geometry[5] = 1;
    ft_azi_geometry_struct.ptr_geometry[6] = 2;
    ft_azi_geometry_struct.ptr_geometry[7] = 3;

    ft_elev_geometry_struct.ptr_geometry[0] = 0;
    ft_elev_geometry_struct.ptr_geometry[1] = 0;

    ft_azi_geometry_struct.geometry_len = 8;
    ft_elev_geometry_struct.geometry_len = 0;
#elif CONFIG_BOARD_MRS7241_P2828_CPUF
    if (mount_type == MMW_MOUNT_VERTICAL) {
		ft_azi_geometry_struct.ptr_geometry[0] = 5;
        ft_azi_geometry_struct.ptr_geometry[1] = 0;

        ft_elev_geometry_struct.ptr_geometry[0] = 3;
        ft_elev_geometry_struct.ptr_geometry[1] = 7;
        ft_elev_geometry_struct.ptr_geometry[2] = 2;
        ft_elev_geometry_struct.ptr_geometry[3] = 6;
        ft_elev_geometry_struct.ptr_geometry[4] = 0;
        ft_elev_geometry_struct.ptr_geometry[5] = 4;
        ft_azi_geometry_struct.geometry_len = 2;
        ft_elev_geometry_struct.geometry_len = 6;
    } else if (mount_type == MMW_MOUNT_HORIZONTAL) {
		ft_azi_geometry_struct.ptr_geometry[0] = 4;
        ft_azi_geometry_struct.ptr_geometry[1] = 0;
        ft_azi_geometry_struct.ptr_geometry[2] = 6;
        ft_azi_geometry_struct.ptr_geometry[3] = 2;
        ft_azi_geometry_struct.ptr_geometry[4] = 7;
        ft_azi_geometry_struct.ptr_geometry[5] = 3;

        ft_elev_geometry_struct.ptr_geometry[0] = 5;
        ft_elev_geometry_struct.ptr_geometry[1] = 0;
        ft_azi_geometry_struct.geometry_len = 6;
        ft_elev_geometry_struct.geometry_len = 2;
    }
#elif CONFIG_BOARD_MRS6241_P2840_M81_CPUF
    ft_azi_geometry_struct.ptr_geometry[0] = 4;
    ft_azi_geometry_struct.ptr_geometry[1] = 5;
    ft_azi_geometry_struct.ptr_geometry[2] = 6;
    ft_azi_geometry_struct.ptr_geometry[3] = 7;
    ft_azi_geometry_struct.ptr_geometry[4] = 0;
    ft_azi_geometry_struct.ptr_geometry[5] = 1;
    ft_azi_geometry_struct.ptr_geometry[6] = 2;
    ft_azi_geometry_struct.ptr_geometry[7] = 3;

    ft_elev_geometry_struct.ptr_geometry[0] = 0;
    ft_elev_geometry_struct.ptr_geometry[1] = 0;

    ft_azi_geometry_struct.geometry_len = 8;
    ft_elev_geometry_struct.geometry_len = 0;
#elif CONFIG_BOARD_MRS6241_P2828_M62_CPUF
	if (mount_type == MMW_MOUNT_VERTICAL) {
		ft_azi_geometry_struct.ptr_geometry[0] = 5;
		ft_azi_geometry_struct.ptr_geometry[1] = 0;

		ft_elev_geometry_struct.ptr_geometry[0] = 3;
		ft_elev_geometry_struct.ptr_geometry[1] = 7;
		ft_elev_geometry_struct.ptr_geometry[2] = 2;
		ft_elev_geometry_struct.ptr_geometry[3] = 6;
		ft_elev_geometry_struct.ptr_geometry[4] = 0;
		ft_elev_geometry_struct.ptr_geometry[5] = 4;
		ft_azi_geometry_struct.geometry_len = 2;
		ft_elev_geometry_struct.geometry_len = 6;
	} else if (mount_type == MMW_MOUNT_HORIZONTAL) {
		ft_azi_geometry_struct.ptr_geometry[0] = 4;
		ft_azi_geometry_struct.ptr_geometry[1] = 0;
		ft_azi_geometry_struct.ptr_geometry[2] = 6;
		ft_azi_geometry_struct.ptr_geometry[3] = 2;
		ft_azi_geometry_struct.ptr_geometry[4] = 7;
		ft_azi_geometry_struct.ptr_geometry[5] = 3;

		ft_elev_geometry_struct.ptr_geometry[0] = 5;
		ft_elev_geometry_struct.ptr_geometry[1] = 0;
		ft_azi_geometry_struct.geometry_len = 6;
		ft_elev_geometry_struct.geometry_len = 2;
	}
#else
    #error "Please Set User-Defined Ant Geometry"
#endif
}

/** @brief angle estmation start
 *  1.config mmw wave
 *  2.init angle param
 *  3.config hardware ant align registers
 *  4.set ant geometry
 *  5.config mmw callback
 *  6.creat semaphore
 *  7.mmw wave start
 *  @return return 0 on success
 * */

int mmw_angle_est_start(struct mmw_angle_est_frame_t *pFrame, uint8_t angle_est_mode)
{
	int ret = 0;
	/* config mmw wave */
	ret = mmw_wave_angle_est_frame_config(pFrame);
	if (ret) {
		ret = -1;
		return ret;
	}
	/* init angle est param */
	ret = mmw_radar_angle_est_init(angle_est_mode);
	if (ret < 0) {
		ret = -1;
		return ret;
	}
	/* set ant align registers */
#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
	mmw_ant_hw_calib_cfg(false);
#elif CONFIG_SOC_SERIES_RS613X
	if (angle_est_mode == FT_ANGELE_CALIB_MODE) {
		mmw_ant_hw_calib_cfg(false);
	} else {
		mmw_ant_hw_calib_cfg(true);
	}
#else
	#error "not support chip!"
#endif

	ft_angle_est_task_status_obj.angle_est_mode = angle_est_mode;
	/* set ant geometry */
	ft_angle_est_set_ant_geometry(pFrame->mount_type);
	
	/* config mmw callback */
	if (angle_est_mode == FT_ANGELE_CALIB_MODE) {
		ret = mmw_ctrl_callback_cfg(mmw_ctrl_angle_calib_frame_cb, MMW_DATA_TYPE_2DFFT, NULL);
	} else if (angle_est_mode == FT_ANGELE_CALCULATE_MODE) {
		ret = mmw_ctrl_callback_cfg(mmw_ctrl_angle_calculate_frame_cb, MMW_DATA_TYPE_2DFFT, NULL);
	}
		
	if (ret) {
		mmw_radar_angle_est_deinit();
	}

	/* mmw wave start */
	ret = mmw_ctrl_start();
	if (ret) {
		mmw_radar_angle_est_deinit();
		return ret;
	}
	
	return ret;
}

int ft_angle_est_param_check(struct mmw_angle_est_recive_param_t *param)
{
	int ret = 0;
	if (param->work_mode != FT_ANGELE_CALIB_MODE && param->work_mode != FT_ANGELE_CALCULATE_MODE) {
		LOG_PRINT("angle est mode error %d\n", param->work_mode);
		ret = -1;
	}
	if (param->mount_type != MMW_MOUNT_VERTICAL && param->mount_type != MMW_MOUNT_HORIZONTAL) {
		LOG_PRINT("angle est mount type error %d\n", param->mount_type);
		ret = -1;
	}
	return ret;
}

int factory_test_angle_est_init(uint8_t *ft_cmd, uint8_t *angle_est_work_mode)
{
	int ret = 0;
	
	/* get receive HIF msg */
	struct mmw_angle_est_recive_param_t *angle_est_recive_param = (struct mmw_angle_est_recive_param_t *)(ft_cmd);
	/* set angle estmation param */
	ft_angle_est_param_set(angle_est_recive_param);

	*angle_est_work_mode = angle_est_recive_param->work_mode;
	
	ret = ft_angle_est_param_check(angle_est_recive_param);
	
	return ret;
}

/** @brief report result of angle estimation 
 *  @note if user do not excute angle calib, report calib data in flash */
void ft_angele_est_report(struct radar_angle_est_t *pdata,struct mmw_angle_est_report_t * report){
	
	report->range = pdata->range;
	report->azimuth_deg = pdata->azimuth_deg;
	report->elevation_deg = pdata->elevation_deg;

	if (ABS(pdata->azimuth_deg) > 1024 || ABS(pdata->elevation_deg) > 1024) {
		report->fail_msk = 1;
		set_ft_angle_calib_status(FT_CALIB_STATE_FAIL);
	} else {
		set_ft_angle_calib_status(FT_CALIB_STATE_SUCCESS);
	}

	if (g_ft_cal_data == FT_CAL_DATA_FLASH) {  /* use calib data in flash */
		mmw_ant_calib_data_get_complex16(pdata->rx_ant_calib_data);
	}
	
#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
	report->calib[0] = (uint16_t)pdata->rx_ant_calib_data[0].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[0].imag << 16);
	report->calib[1] = (uint16_t)pdata->rx_ant_calib_data[1].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[1].imag << 16);
	report->calib[2] = (uint16_t)pdata->rx_ant_calib_data[2].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[2].imag << 16);
	report->calib[3] = (uint16_t)pdata->rx_ant_calib_data[3].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[3].imag << 16);
	report->calib[4] = (uint16_t)pdata->rx_ant_calib_data[4].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[4].imag << 16);
	report->calib[5] = (uint16_t)pdata->rx_ant_calib_data[5].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[5].imag << 16);
	report->calib[6] = (uint16_t)pdata->rx_ant_calib_data[6].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[6].imag << 16);
	report->calib[7] = (uint16_t)pdata->rx_ant_calib_data[7].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[7].imag << 16);
#elif CONFIG_SOC_SERIES_RS613X
	report->calib[0] = (uint16_t)pdata->rx_ant_calib_data[0].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[0].imag << 16);
	report->calib[1] = (uint16_t)pdata->rx_ant_calib_data[1].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[1].imag << 16);
	report->calib[2] = (uint16_t)pdata->rx_ant_calib_data[2].real | ((uint32_t)(uint16_t)pdata->rx_ant_calib_data[2].imag << 16);
	report->calib[3] = 2048;
	report->calib[4] = (1 << 12) | (15561 << 16);
#else
	#error "not support chip!"
#endif
}

/** @brief save specified calib data into flash 
 *  @param calib_data: the point of specified calib data
 * 	@return return 0 on success
 * */
int mmw_angle_save_calib_data_custom(uint8_t *calib_data)
{
	int ret = 0;
#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
	int ret_len = KVF_Save(HWINFO_ID_NVM_ANT_CALIB_2T4R, calib_data, CONFIG_MAX_MIMO_ANT * 4);
	if (ret_len != CONFIG_MAX_MIMO_ANT * 4) {
		ret = -1;
	}
#elif CONFIG_SOC_SERIES_RS613X
	*(uint32_t *)&calib_data[8] = - *(uint32_t *)&calib_data[8];
	*(uint32_t *)&calib_data[12] = 2048;
	*(uint32_t *)&calib_data[16] = (1 << 12) | (15561 << 16);
	
	int ret_len = KVF_Save(HWINFO_ID_NVM_ANT_CALIB_1T3R, calib_data, HWINFO_SIZE_NVM_MMW_CALIB);
	if (ret_len != HWINFO_SIZE_NVM_MMW_CALIB) {
		ret = -1;
	}
#else
	#error "please select valid board!"
#endif
	return ret;
}

/** @brief save calib data into flash */
int mmw_angle_save_calib_data(void)
{
	int ret = 0;
	
#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
	uint8_t data[CONFIG_MAX_MIMO_ANT * 4] = {0};
	complex16_cube *calib = ft_angle_calib_data;
	
	*(uint32_t *)&data[0]  = (uint16_t)calib[0].real | ((uint32_t)(uint16_t)calib[0].imag << 16);
	*(uint32_t *)&data[4]  = (uint16_t)calib[1].real | ((uint32_t)(uint16_t)calib[1].imag << 16);
	*(uint32_t *)&data[8]  = (uint16_t)calib[2].real | ((uint32_t)(uint16_t)calib[2].imag << 16);
	*(uint32_t *)&data[12]  = (uint16_t)calib[3].real | ((uint32_t)(uint16_t)calib[3].imag << 16);
	*(uint32_t *)&data[16]  = (uint16_t)calib[4].real | ((uint32_t)(uint16_t)calib[4].imag << 16);
	*(uint32_t *)&data[20]  = (uint16_t)calib[5].real | ((uint32_t)(uint16_t)calib[5].imag << 16);
	*(uint32_t *)&data[24]  = (uint16_t)calib[6].real | ((uint32_t)(uint16_t)calib[6].imag << 16);
	*(uint32_t *)&data[28]  = (uint16_t)calib[7].real | ((uint32_t)(uint16_t)calib[7].imag << 16);
	
	FT_LOG_PRINT("save angle calib data: 0x%x, 0x%x ,0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n", 
		*(uint32_t *)&data[0], *(uint32_t *)&data[4], *(uint32_t *)&data[8], *(uint32_t *)&data[12], 
		*(uint32_t *)&data[16], *(uint32_t *)&data[20], *(uint32_t *)&data[24], *(uint32_t *)&data[28]);
	
	int ret_len = KVF_Save(HWINFO_ID_NVM_ANT_CALIB_2T4R, data, CONFIG_MAX_MIMO_ANT * 4);
	if (ret_len != CONFIG_MAX_MIMO_ANT * 4) {
		ret = -1;
	}
	
#elif CONFIG_SOC_SERIES_RS613X
	uint8_t data[HWINFO_SIZE_NVM_MMW_CALIB];
	complex16_cube *calib = ft_angle_calib_data;
	
	*(uint32_t *)&data[0]  = (uint16_t)calib[0].real | ((uint32_t)(uint16_t)calib[0].imag << 16);
	*(uint32_t *)&data[4]  = (uint16_t)calib[1].real | ((uint32_t)(uint16_t)calib[1].imag << 16);
	*(uint32_t *)&data[8]  = (uint16_t)-calib[2].real | ((uint32_t)(uint16_t)-calib[2].imag << 16);
	*(uint32_t *)&data[12] = 2048;
	*(uint32_t *)&data[16] = (1 << 12) | (15561 << 16);
	
	FT_LOG_PRINT("save angle calib data: 0x%x, 0x%x ,0x%x, 0x%x 0x%x\n", *(uint32_t *)&data[0], *(uint32_t *)&data[4], *(uint32_t *)&data[8], *(uint32_t *)&data[12], *(uint32_t *)&data[16]);
	
	int ret_len = KVF_Save(HWINFO_ID_NVM_ANT_CALIB_1T3R, data, HWINFO_SIZE_NVM_MMW_CALIB);
	if (ret_len != HWINFO_SIZE_NVM_MMW_CALIB) {
		ret = -1;
	}
#else
	#error "please select valid board!"
#endif

	return ret;
}

/** @brief get ant calib data from flash 
 *  @param calib_data: the point of calib data from flash
 *  @return retun 0 on success 
 * */
int mmw_angle_get_calib_data(uint32_t *calib_data)
{
#if (CONFIG_SOC_SERIES_RS613X)
    uint32_t chip_info[HWINFO_SIZE_NVM_MMW_CALIB] = { 0 };
	complex16_reim* ptr_calib_data = 0;
	
    /* 
     * Ant data save format in flash:
     * RS6130: 
     * complex16_reim ant_0,
     * complex16_reim ant_1,
     * complex16_reim -ant_2
     *
     * RS6240:
     * complex16_reim ant_0,
     * complex16_reim ant_1,
     * complex16_reim ant_2,
     * complex16_reim ant_3,
     * complex16_reim ant_4,
     * complex16_reim ant_5,
     * complex16_reim ant_6,
     * complex16_reim ant_7,
     * complex16_reim ant_8,
     * */
    int ret_len = KVF_Load(HWINFO_ID_NVM_ANT_CALIB_1T3R, (uint8_t *)&chip_info[0], HWINFO_SIZE_NVM_MMW_CALIB);
	ptr_calib_data = (complex16_reim*)&chip_info[2];
    /* ant2 is reversed for rs6130 */
    ptr_calib_data->imag = -ptr_calib_data->imag;
    ptr_calib_data->real = -ptr_calib_data->real;

    if (ret_len == HWINFO_SIZE_NVM_MMW_CALIB) {
		calib_data[0] = chip_info[0];
		calib_data[1] = chip_info[1];
		calib_data[2] = chip_info[2];
		calib_data[3] = 2048;
		calib_data[4] = (1 << 12) | (15561 << 16);
        LOG_PRINT("MIMO Calib Load %d\n", ret_len);
		FT_LOG_PRINT("calib data[0] 0x%x [1] 0x%x [2] 0x%x [3] 0x%x [4] 0x%x\n", calib_data[0], calib_data[1], calib_data[2], calib_data[3], calib_data[4]);
        return 0;
    } else {
        return -1;
	}
#elif CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
	/* KV-Calibration is only support for RS6130 */
	uint32_t chip_info[CONFIG_MAX_MIMO_ANT] = { 0 };
    int ret_len = KVF_Load(HWINFO_ID_NVM_ANT_CALIB_2T4R, (uint8_t *)&chip_info[0], CONFIG_MAX_MIMO_ANT * 4);
	if (ret_len == CONFIG_MAX_MIMO_ANT * 4) {
		calib_data[0] = chip_info[0];
		calib_data[1] = chip_info[1];
		calib_data[2] = chip_info[2];
		calib_data[3] = chip_info[3];
		calib_data[4] = chip_info[4];
		calib_data[5] = chip_info[5];
		calib_data[6] = chip_info[6];
		calib_data[7] = chip_info[7];
		LOG_PRINT("MIMO Calib Load %d!\n", ret_len);
		FT_LOG_PRINT("%x %x %x %x %x %x %x %x\n",
			calib_data[0], calib_data[1],
			calib_data[2], calib_data[3],
			calib_data[4], calib_data[5],
			calib_data[6], calib_data[7]);
		return 0;
	} else {
		return -1;
	}
#else
	#error "please select valid board!"
#endif
}

/** @brief get iq data which use for calibration */
int mmw_angle_calib_iq_data_get(uint8_t *iq_data, uint8_t len)
{
    int ret = 0;
	complex16_cube *angle_pos = ft_angle_pos_iq_data;

    memcpy(iq_data, angle_pos, len);

    return ret;
}