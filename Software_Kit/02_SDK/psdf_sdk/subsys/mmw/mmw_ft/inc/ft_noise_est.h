/**
 **************************************************************************************************
 * @brief   factory test noise estmation config define.
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
 
#ifndef __FT_NOISE_EST_H__
#define __FT_NOISE_EST_H__


#ifdef __cplusplus
extern "C" {
#endif

#include <common.h>
#include "mmw_type.h"
#include "mmw_ctrl.h"
#include "factory_test_config.h"
#include "mmw_alg_pointcloud_typedef.h"
#include "ft_type.h"
#include "hif.h"


#define FT_NOISE_EST_DBF_CASE_NUM		(4)	/* dbf degeree case: 0, 90, 30, -30 */
#define FT_NOISE_EST_DBF_DIFF_PWR_GAP	(6) /* after beamforming, gap of power diff between range bin is 6 dB */
#define FT_NOISE_EST_DIFF_PWR_GAP		(10)
#define FT_NOISE_EST_DBF_DIFF_STEP_NUM		(2)

#define FT_NOISE_EST_EACH_FREQ_BIN_GAP	(5)

#define FT_NOISE_EST_SPEED_CASE_NUM		(4) /* from 4 speed dim to excuting noise estimation */


enum {
	NOISE_PEAK_PWR_GAP_ID = 0,
	NOISE_SNR_GAP_ID,
	NOISE_MIN_GAP_ID,
	NOISE_AVGE_GAP_ID,
	NOISE_MAX_GAP_ID,
	NOISE_GAP_ID_NUM
};

enum {
	MMW_NOISE_RX_ANT_REPORT_EVT = 0,
	MMW_NOISE_FINAL_REPORT_EVT,
	MMW_NOISE_UNKNOW_REPORT_EVT
};

enum {
	PEAK_NOISE_VAILD = 0,
	PEAK_INVAILD,
	NOISE_INVALID
};


struct mmw_meas_pos_t {
	uint16_t rbin_max_pwr_idx;
	float  rbin_max_pwr;
};

struct ft_task_status_t {
	bool running;
	int16_t curr_frame_index;
};


void ft_noise_est_param_set(struct mmw_nosie_est_recive_param_t *param);

int ft_noise_factor_set(struct mmw_noise_factor_t *factor, struct mmw_noise_factor_t *factor_param);

int ft_noise_est_range_set(uint16_t range_min, uint16_t range_max, struct mmw_noise_usr_t *usr_param);

void mmw_noise_hw_init_set(uint32_t id);

int mmw_noise_meas_scope_set(uint16_t start_index, uint16_t end_index);

int mmw_noise_assess_gap_param_config(uint8_t gap_id, int16_t gap_val);

int mmw_noise_est_start(struct mmw_noise_est_frame_t *pFrame, struct radar_noise_est_t *mradar_est_data);

int ft_noise_est_init(struct mmw_noise_est_frame_t *pFrame);

void ft_noise_est_deinit(void);

int factory_test_mmw_perf_start(uint8_t *ft_cmd);

float noise_est_buf_update(Complexf32_RealImag *pBuf_in, uint16_t dop_num);

static void noise_rbin_buf_update(Complexf32_RealImag *pBuf, uint8_t rx_idx, uint16_t rbin_num, uint16_t rbin_off, struct mmw_noise_param_t  *noise_param);

int mmw_radar_find_rbin_postion(Complexf32_RealImag *cube, uint16_t start_offset, uint16_t rbin_num, uint8_t rx_ant_num, struct mmw_meas_pos_t *pos);

static bool mmw_radio_noise_est_pass(uint16_t rx_ant_id, float *avge_noise, struct radar_noise_est_t *mradar_est_data, struct mmw_noise_report_t *report);

void mmw_noise_est_report_data(int event, struct mmw_noise_report_t *report, struct mmw_noise_est_final_report_t *est_report);

struct ft_task_status_t* get_ft_task_status(void);

struct mmw_noise_est_final_report_t** get_noise_est_report_result(void);

/* max difference detect process function */

struct mmw_max_diff_detect_report_t** get_max_diff_detect_report_param(void);

struct mmw_max_diff_detect_param* get_detect_param(void);

int mmw_max_diff_detect_scope_set(uint16_t start_index, uint16_t end_index);

void mmw_max_diff_detect_gap_set(int16_t gap);

int mmw_max_diff_detect_start(struct mmw_noise_est_frame_t *pFrame);

void beamforming_vector_init(void);

void ft_max_diff_detect_deinit(void);

int factory_test_mmw_max_diff_detect_start(uint8_t *ft_cmd);

void ft_beamforming_2d(Complexf32_RealImag *ptr_ant_data, Complexf32_RealImag *dbf_result_buffer, uint16_t range_idx, uint16_t dop_idx, uint16_t dbf_case_idx);

void ft_multidim_noise_est_buf_update(Complexf32_RealImag *noise_buf, float *result_buffer, uint16_t dop_len, uint16_t static_dop_idx);

void mmw_max_diff_detect_process_finish(void);

bool ft_multidim_pass_verify(float *rbin_power_db, uint16_t *diff_step_buf, uint8_t step_size, float gap_diff_pwr, 
								uint32_t rbin_scop_start, uint32_t rbin_scop_end, float *diff_value_max, uint16_t *range_idx_max);

void ft_dbf_noise_est_process();

void ft_multidim_diff_process(uint16_t rx_ant_id, float* noise_diff_db_max);

void ft_max_diff_value_report(void);

struct mmw_max_diff_detect_result_t **get_diff_detect_result(void);

/* each frequency bin detect process function */
void ft_freq_bin_detect_frame_param_set(struct mmw_freq_bin_detect_recive_param_t *param);

int mmw_each_freq_bin_detect_scope_set(uint16_t start_index, uint16_t end_index);

int mmw_freq_bin_detect_start(struct mmw_noise_est_frame_t *pFrame, struct mmw_freq_bin_detect_recive_param_t *ptr_usr_param);

void ft_freq_bin_detect_deinit(void);

int factory_test_freq_bin_detect_start(uint8_t* ft_cmd);

struct mmw_freq_bin_detect_report_t* get_freq_bin_report_param(void);

#ifdef __cplusplus
}
#endif
#endif  