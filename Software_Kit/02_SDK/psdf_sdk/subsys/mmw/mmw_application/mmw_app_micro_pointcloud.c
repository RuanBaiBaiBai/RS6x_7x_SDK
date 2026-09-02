/*
 **************************************************************************************************
 *          Copyright (c) 2022 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/************** Include Files **************/
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "mmw_point_cloud_psic_lib.h"

#include "csi_math.h"
#include "csi_math.h"
#include "csi_const_structs.h"
#include "mmw_alg_ant_calibration.h"
#include "mmw_alg_micro_cube.h"
#include "mmw_app_micro_pointcloud.h"
#include "mmw_alg_debug.h"
#include "mmw_app_pointcloud_config.h"
#include "mmw_alg_pointcloud.h"

#define MPC_SHOW_DEBUG_PRINT_ENABLE     0 /* MPC debug print */
#if (MPC_SHOW_DEBUG_PRINT_ENABLE)
#define mpc_printf(fmt, arg...)         printk("[MPC]"fmt, ##arg)
#else
#define mpc_printf(fmt, arg...)
#endif  //MPC_SHOW_DEBUG_PRINT_ENABLE

/* USE RECTANGLE Window in micro doppler to increase peak in doppler */
#define MICRO_DOP_WIN_TYPE_RECT			(1) //hanning win would lost 6dB.

#if CONFIG_SOC_SERIES_RS613X
#define MICRO_CFAR_ANT_DETECT_IDX	1
#elif CONFIG_SOC_SERIES_RS624X
#define MICRO_CFAR_ANT_DETECT_IDX	0
#else
	#error "not support chip!"
#endif

/* Micro ca-cfar snr linear threshold param define */
#define MICRO_CFAR_LINEAR_TH_SLOPE			(-1.75f)
#define MICRO_CFAR_LINEAR_TH_INTERCEPT		(28.5f)

extern MMWPresencePointCloudUserCfg_t g_mmw_presence_det_3d_user_cfg;
MPC_CTRL        g_mpc_ctrl;

static uint16_t mpc_snr_trans_db(uint32_t snr_lin) //20*log10
{
	uint16_t snr_db = 0;
	while (snr_lin >= 4) {
		snr_db += 6;
		snr_lin = snr_lin>>1;
	}
	return snr_db + (snr_lin == 3 ? 10 : (snr_lin == 2 ? 6 : 0));
}

uint8_t log2_num_get(uint32_t num){
	uint8_t num_log2 = 0;
	while (num > 1) {
		num >>= 1;
		num_log2++;
	}
	return num_log2;
}

float db_snr_trans_linear(float snr_db){
	
	float result = 0.0f;
	
	int16_t int_part = (int16_t)snr_db;
	float frac_part = snr_db - (float)int_part;
	
	if (frac_part < 0) {
		frac_part += 1.0f;
		int_part -= 1;
	}
	
	uint16_t exp = (int_part < 0) ? -int_part : int_part;
	
	float int_result = 1.0f;
	/* linear = 10^(snr_db / 20) = 10^(0.05 * snr_db) 
	 * 10^0.05 = 1.1220184543019634355910389464779
	 * */
	float int_base = 1.1220184543019634355910389464779f;

	while (exp) {
		if (exp & 1) {
			int_result *= int_base;
		}
		int_base *= int_base;
		exp >>= 1;
	}
	
	if (int_part < 0) {
		int_result = 1.0f / int_result;
	}
	
	float ln10_20 = 0.1151292546497023f;	/* (ln(10)) / 20 = 0.11512925464970228420089957273422 */
	float x = frac_part * ln10_20;
	float frac_result = 1.0f + x + x * x * 0.5f + x * x * x * 0.16667f;
	
	result = int_result * frac_result;
	
	return result;
}

static int micro_cfar_snr_linear_th_init(void)
{
	if (g_mpc_ctrl.proc_cfar_th_bufs == NULL) {
		return -1;
	}
	
	int range_cnt = 0;
	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
	float k = MICRO_CFAR_LINEAR_TH_SLOPE, b = MICRO_CFAR_LINEAR_TH_INTERCEPT, range_m = 0.f, snr_th_db = 0.f, snr_th_lin = 0.f;
	uint32_t range_mm, range_res_mm;
	uint16_t range_fft_num, dop_fft_num;
	mmw_range_get(&range_mm, &range_res_mm);
	mmw_fft_num_get(&range_fft_num, &dop_fft_num);
	
	uint32_t linear_snr = mmw_micro_cfar_snr_get();
	for (int range_idx = 0; range_idx < range_fft_num;) {
		range_m = (range_idx + 1) * range_res_mm * 0.001f;
		snr_th_db = k * range_m + b;
		snr_th_lin = db_snr_trans_linear(snr_th_db);
		if (snr_th_lin <= linear_snr) {
			snr_th_lin = linear_snr;
		} 
		g_mpc_ctrl.proc_cfar_th_bufs[range_cnt] = (uint16_t)(snr_th_lin * 16.f);		// float to U16Q4

		range_idx += range_extract_frequency;
		range_cnt++;
	}
	return 0;
}

static void micro_clutter_remove(complex16_cube *data_in, uint32_t chirp_num)
{
	int32_t clutter[2] = { 0 };
	/* calc average of chirps */
	for (int i = 0; i < chirp_num; ++i) {
		clutter[0] += data_in[i].real;
		clutter[1] += data_in[i].imag;
	}
	clutter[0] = clutter[0]/chirp_num;
	clutter[1] = clutter[1]/chirp_num;
	for (int i = 0; i < chirp_num; ++i) {
		data_in[i].real = data_in[i].real - clutter[0];
		data_in[i].imag = data_in[i].imag - clutter[1];
	}
}

#if (MICRO_DOP_WIN_TYPE_RECT == 0)
const int16_t hanning_win_4[] = {
	11321, 29639, 29639, 11321
};
const int16_t hanning_win_8[] = {
	3383, 13539, 24576, 31780, 31780, 24576, 13539, 3383
};
const int16_t hanning_win_16[] = {
	1106, 4276, 9081, 14872, 20868, 26258, 30314, 32489,
	32489, 30314, 26258, 20868, 14872, 9081, 4276, 1106
};
const int16_t hanning_win_32[] = {
	296, 1174, 2601, 4526, 6880, 9578, 12521, 15604,
	18716, 21743, 24576, 27113, 29263, 30947, 32104, 32694,
	32694, 32104, 30947, 29263, 27113, 24576, 21743, 18716,
	15604, 12521, 9578, 6880, 4526, 2601, 1174, 296
};
int micro_dop_win_init(uint32_t chirp_num)
{
	switch(chirp_num) {
	case 4:
		g_mpc_ctrl.hanning_win = &hanning_win_4[0];
		break;
	case 8:
		g_mpc_ctrl.hanning_win = &hanning_win_8[0];
		break;
	case 16:
		g_mpc_ctrl.hanning_win = &hanning_win_16[0];
		break;
	case 32:
		g_mpc_ctrl.hanning_win = &hanning_win_32[0];
		break;
	}
	return 0;
}
#endif

void micro_dop_window(complex16_cube *data_in, uint32_t chirp_num, complex16_mdsp *data_win, uint32_t mdop_num)
{
	int i = 0;
	const int16_t *win = g_mpc_ctrl.hanning_win;
	uint8_t MICRO_DOP_FFT_GAIN_LOG2 = g_mpc_ctrl.mprocess_param.micro_dop_fft_gain_log2;
	if (win == NULL) {
		while (i < chirp_num) {
			data_win[i].real = data_in[i].real << MICRO_DOP_FFT_GAIN_LOG2;
			data_win[i].imag = data_in[i].imag << MICRO_DOP_FFT_GAIN_LOG2;
			++i;
		}
	} else {
		while (i < chirp_num) { //"+1" for hanning win would lost 6dB.
			data_win[i].real = (((int32_t)data_in[i].real) * win[i])>>(15 - (MICRO_DOP_FFT_GAIN_LOG2 + 1));
			data_win[i].imag = (((int32_t)data_in[i].imag) * win[i])>>(15 - (MICRO_DOP_FFT_GAIN_LOG2 + 1));
			++i;
		}
	}
	while (i < mdop_num) {
		*(uint32_t *)&data_win[i++] = 0;
	}
}

int micro_dbf_cfar(complex16_mdsp *data_dbf, uint32_t dbf_num, Noisetype noise_level,
                       complex16_mdsp *ant_aligned, MmwAngleInfo_t *angles, uint32_t buf_max)
{
	uint32_t point_idx = 0;
	uint32_t angel_num = 0;
	uint32_t max_idx[MMW_DBF_MAX_PEAK + 1];
	uint32_t max_peak[MMW_DBF_MAX_PEAK + 1] = {0};
	uint32_t max_peak_th;

#if (MPC_SHOW_DBF_FFT_DEBUG)
	mpc_printf("dbf noise=%d\n", noise_level);
#endif

	for (int i = 0; i < MMW_DBF_FFT_NUM; ++i) {
		/* noise threshold */
		uint32_t power_pre, power_next;
		uint32_t power = COMPLEX16_POWER(&data_dbf[i]);
		if (power < noise_level) {
			continue ;
		}
		/* stationary detect */
		power_pre  = COMPLEX16_POWER(&data_dbf[(i - 1) & (MMW_DBF_FFT_NUM - 1)]);
		power_next = COMPLEX16_POWER(&data_dbf[(i + 1) & (MMW_DBF_FFT_NUM - 1)]);
		if (power < power_next || power < power_pre) {
			continue ;
		}
		/* save peaks */
		for (int j = 0; j < buf_max; ++j) {
			if (power > max_peak[j]) {
				max_peak[j + 1] = max_peak[j];
				max_idx[j + 1]  = max_idx[j];
				max_peak[j] = power;
				max_idx[j]  = i;
				break;
			}
		}
	}

	max_peak_th = max_peak[0]/MMW_DBF_PEAK_SFDR;  //10dB
	while (max_peak[point_idx] > max_peak_th && point_idx < MMW_DBF_MAX_PEAK) {
		complex16_mdsp azim_phase, elev_phase;
		mmw_angle_bin_phase(&data_dbf[0], max_idx[point_idx], &azim_phase);
		mmw_angle_elev_phase_m62(ant_aligned, (uint32_t *)&elev_phase);
		mmw_angle_calculate((uint32_t *)&azim_phase, (uint32_t *)&elev_phase, angles);
		if (angles->elevation < ANGLE_ELEV_INVALID) {
			if (++angel_num < buf_max) {
				angles++;
			} else {
				break;
			}
		}
		point_idx++;
	}
	return angel_num;
}

static void micro_ant_data_align_2t4r(complex16_mdsp *fft_out, complex16_mdsp *ant_aligned, uint32_t ant_step)
{
	const complex16_mdsp *ant_calibrate_data = (complex16_mdsp *)mmw_angle_calib_data_get();
	for (int i = 0; i < MMW_ANT_CHANNEL_NUM; ++i) {
		complex16mdsp_mul16(fft_out, &ant_calibrate_data[i], &ant_aligned[i]);
		fft_out += ant_step;
	}
}


/**@brief get the range bin len which is influenced by hw hpf */
uint16_t hw_hpf_suppressed_range_bin_len_get(int hpf_bandwidth, int range_num) {
	float bandwidth = -1;
	int suppressed_range_bin_len = 0;
	switch (hpf_bandwidth) {
		case HPF_BW_0P5M:
			bandwidth = 0.5f;
			break;
		case HPF_BW_1M:
			bandwidth = 1;
			break;
		case HPF_BW_2M:
			bandwidth = 2;
			break;
		case HPF_BW_4M:
			bandwidth = 4;
			break;
		case HPF_BW_8M:
			bandwidth = 8;
			break;
		case HPF_BW_16M:
			bandwidth = 16;
			break;
		default:
			break;
	}
	/* suppressed_range_bin_len = bandwidth / Fs * range_num, Fs is 10MHz */
	suppressed_range_bin_len = bandwidth / 10.f * range_num;
	return suppressed_range_bin_len;
}

#if CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_SINGLE_SIDE
int micro_dop_single_side_cfar_doa(complex16_mdsp *fft_out, uint32_t *fft_out_abs, uint32_t range_idx,  MmwMicroDetectData_t* ptr_mpc_buffer)
{
	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
	uint16_t linear_snr_q4 = 0;
	
	int16_t noise_len_segment1 = g_mpc_ctrl.mcfar_Param.noise_len_segment1;
	int16_t noise_len_segment2 = g_mpc_ctrl.mcfar_Param.noise_len_segment2;
	uint16_t guard_len = g_mpc_ctrl.mcfar_Param.guard_len;
	uint16_t win_len = g_mpc_ctrl.mcfar_Param.win_len;
	
	uint32_t range_mm, range_reol_mm;
	mmw_range_get(&range_mm, &range_reol_mm);
	int16_t noise_len_segment1_use = 0;
	uint16_t guard_len_use = 0;
	
	uint16_t MDOP_FFT_BUF_NUM = g_mpc_ctrl.mprocess_param.mdop_fft_buf_num;
	uint16_t micro_dop_num = mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
	uint16_t dop_bin_start_skip = g_mpc_ctrl.dop_bin_start_skip;
	uint16_t dop_bin_end_skip = g_mpc_ctrl.dop_bin_end_skip;
	
	uint32_t abs_thresh, abs_power;
	
	uint16_t linear_th_offest_db = mmw_presence_point_cloud_get_user_cfg()->micro_ca_cfar_snr_linear_th_offest; 
	uint16_t linear_th_offest_q4 = (uint16_t)(db_snr_trans_linear(linear_th_offest_db) * 16.f);
	linear_snr_q4 = g_mpc_ctrl.proc_cfar_th_bufs[range_idx] + linear_th_offest_q4;
	
	if (range_idx < (g_mpc_ctrl.mcfar_Param.hw_hpf_suppressed_range_bin_len + win_len)) {
		/* for the detection of range bins influenced by HPF,
		 * the snr threshold is increased by extra 0dB.
		 * */
		uint8_t extra_snr_th_db = MICRO_CFAR_EXTRA_SNR_TH_DB;
		linear_snr_q4 += (uint16_t)(db_snr_trans_linear(extra_snr_th_db) * 16.f);
	} 

	for (int micro_dop_idx = (dop_bin_start_skip + dop_bin_end_skip); micro_dop_idx < (micro_dop_num - dop_bin_end_skip); micro_dop_idx++) {
		/* point num limit */
		if (g_mpc_ctrl.presence_points_num >= CONFIG_MMW_PRESENCE_POINT_MAX) {
			break;
		}
		abs_thresh = 0;
		abs_power = 0;
		/* the first stage is detected in three sections to use a fixed noise length */
		if (range_idx < (win_len -1)) {
			/* ensure there are sufficient noise cells for CFAR detection */
			if (range_reol_mm < MICRO_CA_CFAR_RANGE_RES_MM_MIN_LIM) {
				guard_len_use = (guard_len + 1) >> 1;
				noise_len_segment1_use = (win_len - 1) - (guard_len_use << 1);
			} else {
				guard_len_use = guard_len;
				noise_len_segment1_use = noise_len_segment1;
			}
			
			if (range_idx <= guard_len_use) {
				uint32_t right_noise_start_idx = range_idx + guard_len_use + 1;
				for (int j = 0; j < noise_len_segment1_use; j++) {
					uint32_t ptr_offest = (right_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
					abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
					right_noise_start_idx++;
				}
			}
			else if ((range_idx > guard_len_use) && (range_idx < noise_len_segment1_use + guard_len_use)) {
				uint32_t left_noise_len = range_idx - guard_len_use;
				uint32_t right_noise_len = win_len  - (range_idx  + guard_len_use + 1);
				uint32_t left_noise_start_idx = 0;
				uint32_t right_noise_start_idx = range_idx + guard_len_use + 1;
				
				for (int j = 0; j < left_noise_len; j++) {
					uint32_t ptr_offest = (left_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
					abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
					left_noise_start_idx++;
				}
				for (int j = 0; j < right_noise_len; j++) {
					uint32_t ptr_offest = (right_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
					abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
					right_noise_start_idx++;
				}
			}
			else {
				uint32_t left_noise_start_idx = range_idx - guard_len_use - noise_len_segment1_use;

				for (int j = 0; j < noise_len_segment1_use; ++j) {
				 	uint32_t ptr_offest = (left_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
					abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
					left_noise_start_idx++;	
				}				
			}
			abs_thresh = abs_thresh * linear_snr_q4;
			
			uint32_t ptr_offest = (range_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
			abs_power = fft_out_abs[ptr_offest + micro_dop_idx];
			
			if (((abs_power << 4) * noise_len_segment1_use) <= abs_thresh) {
				continue;
			}
		} 
		/* the seconed stage uses noise data on only left side */
		else {
			uint32_t left_noise_start_idx = range_idx - (guard_len + noise_len_segment2);

			for (int j = 0; j < noise_len_segment2; j++) {
				uint32_t ptr_offest = (left_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
				abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
				left_noise_start_idx++;
			}
			
			abs_thresh = abs_thresh * linear_snr_q4;
			
			uint32_t ptr_offest = (range_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
			abs_power = fft_out_abs[ptr_offest + micro_dop_idx];
			

			if (((abs_power << 4) * noise_len_segment2) <= abs_thresh) {
				continue;
			}
		}
		
		/* Antenna aligned and Anlge calculate */
		uint32_t ant_aligned[MMW_ANT_CHANNEL_NUM];
		float sin_azi;
		float sin_elev;
		
		/* get ant iq data from ring buffer */
		uint16_t ptr_offest = micro_dop_idx + (range_idx % win_len) * MDOP_FFT_BUF_NUM;
#if (CONFIG_SOC_SERIES_RS613X)
		for (int ant_mimo_idx = 0; ant_mimo_idx < MMW_ANT_CHANNEL_NUM; ant_mimo_idx++) {
			ant_aligned[ant_mimo_idx] = *(uint32_t *)&fft_out[ptr_offest + ant_mimo_idx * micro_dop_num];
		}
#elif (CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X)
		micro_ant_data_align_2t4r(&fft_out[ptr_offest], (complex16_mdsp *)&ant_aligned[0], micro_dop_num);
#else
		#error "not support board"
#endif
		uint8_t ret_status = mmw_process_presence_pointcloud_angle_process((complex16_mdsp *)&ant_aligned[0], mmw_get_g_azi_geometry_struct_const(), mmw_get_g_elev_geometry_struct_const(), 
															&sin_azi, &sin_elev, 0, 0);
		if (ret_status != MMW_ERR_CODE_SUCCESS) {
			continue;
		}
			
		/* SNR calculate, use rx0(6240)/rx1(6130) to calculate noise */
		uint32_t* noise = micro_cube_noise_get(range_idx);
		
		uint16_t range_start, range_end;
		micro_cube_range_get(&range_start, &range_end);
		uint16_t range_num = range_end - range_start;
		uint16_t range_store_len = range_num / mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
		uint32_t offest = MICRO_CFAR_ANT_DETECT_IDX * range_store_len;
		
		/* use dop_idx = 0, 1, (dop_num - 1) to do noise estimation, and in each dop_idx, use range_idx and neighbouring two range_idx to do  noise estimation. 
		 * therefore, power should multiply 9(3*3) to align with noise. */
		uint32_t snr_linear = (((uint64_t)(abs_power * abs_power) << g_mpc_ctrl.mprocess_param.micro_dop_gain_log2) * 9) / (noise[offest] >> g_mpc_ctrl.mprocess_param.micro_chirp_num_log2);
		if (snr_linear == 0) {
			continue;
		}
		/* Convert to point cloud result data */
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].azi_phase = sin_azi;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].ele_phase = sin_elev;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].doppler_idx = micro_dop_idx;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].range_idx = range_idx * range_extract_frequency;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].sig_snr = mpc_snr_trans_db(snr_linear) >> 1;
		g_mpc_ctrl.presence_points_num++;	
	}

	return 0;
}
#endif

#if CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_BOTH_SIDE
uint8_t pre_store_isready(uint16_t store_fft_out_idx, uint16_t cut_range_idx, uint16_t range_bin_max, uint16_t half_win_len)
{
	/* ca-cfar detects whether the data of noise and protection units at different stages are pre-stored */
	if (cut_range_idx < half_win_len) {
		return store_fft_out_idx > (cut_range_idx + half_win_len);
	}
	else if ((cut_range_idx >= half_win_len) && (cut_range_idx < (range_bin_max - half_win_len))) {
		return store_fft_out_idx > (cut_range_idx + half_win_len);
	}
	else {
		return store_fft_out_idx >= range_bin_max;
	}
}

int micro_dop_both_side_cfar_doa(complex16_mdsp *fft_out, uint32_t *fft_out_abs, uint32_t range_idx,  MmwMicroDetectData_t* ptr_mpc_buffer)
{
	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
	uint16_t linear_snr_q4 = 0;
	
	uint16_t noise_len = g_mpc_ctrl.mcfar_Param.noise_len;
	uint16_t guard_len = g_mpc_ctrl.mcfar_Param.guard_len;
	uint16_t half_win_len = g_mpc_ctrl.mcfar_Param.half_win_len;
	uint16_t win_len = g_mpc_ctrl.mcfar_Param.win_len;
	
	uint16_t MDOP_FFT_BUF_NUM = g_mpc_ctrl.mprocess_param.mdop_fft_buf_num;
	uint16_t range_bin_max = g_mpc_ctrl.range_bin_max;
	uint16_t micro_dop_num = mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
	uint16_t dop_bin_start_skip = g_mpc_ctrl.dop_bin_start_skip;
	uint16_t dop_bin_end_skip = g_mpc_ctrl.dop_bin_end_skip;
	
	uint32_t abs_thresh, abs_power;
	
	uint16_t linear_th_offest_db = mmw_presence_point_cloud_get_user_cfg()->micro_ca_cfar_snr_linear_th_offest; 
	uint16_t linear_th_offest_q4 = (uint16_t)(db_snr_trans_linear(linear_th_offest_db) * 16.f);
	linear_snr_q4 = g_mpc_ctrl.proc_cfar_th_bufs[range_idx] + linear_th_offest_q4;
	
	if (range_idx < (g_mpc_ctrl.mcfar_Param.hw_hpf_suppressed_range_bin_len + win_len)) {
		/* for the detection of range bins influenced by HPF,
		 * the snr threshold is increased by extra 0dB.
		 * */
		uint8_t extra_snr_th_db = MICRO_CFAR_EXTRA_SNR_TH_DB;
		linear_snr_q4 += (uint16_t)(db_snr_trans_linear(extra_snr_th_db) * 16.f);
	}
	
	bool segment1 = (range_idx < half_win_len);
	bool segment2 = ((range_idx >= half_win_len) && (range_idx < (range_bin_max - half_win_len)));
	bool segment3 = ((range_idx >= (range_bin_max - half_win_len)) && (range_idx < range_bin_max));
	
	for (int micro_dop_idx = (dop_bin_start_skip + dop_bin_end_skip); micro_dop_idx < (micro_dop_num - dop_bin_end_skip); micro_dop_idx++) {
		/* point num limit */
		if (g_mpc_ctrl.presence_points_num >= CONFIG_MMW_PRESENCE_POINT_MAX) {
			break;
		}
		abs_thresh = 0;
		abs_power = 0;
		/* the first stage only uses the right noise data */
		if (segment1) {
			uint32_t right_noise_start_idx = range_idx + guard_len + 1;
			for (int j = 0; j < noise_len; j++) {
				uint32_t ptr_offest = (right_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
				abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
				right_noise_start_idx++;
			}
			abs_thresh = abs_thresh * linear_snr_q4;
			uint32_t ptr_offest = (range_idx % g_mpc_ctrl.mcfar_Param.win_len) * g_mpc_ctrl.mprocess_param.mdop_fft_buf_num;
			complex16_mdsp *ring_cut_range_ptr = fft_out + ptr_offest;
			abs_power = complex16_abs(ring_cut_range_ptr[micro_dop_idx].real, ring_cut_range_ptr[micro_dop_idx].imag);
			
			if (((abs_power << 4) * noise_len) <= abs_thresh) {
				continue;
			}
		} 
		/* the seconed(middle) stage  uses noise data on both sides */
		else if (segment2) {
			uint32_t left_noise_start_idx = range_idx - half_win_len;
			uint32_t right_noise_start_idx = range_idx + guard_len + 1;

			for (int j = 0; j < noise_len; ++j) {
				uint32_t ptr_offest = (left_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
				abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
				ptr_offest = (right_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
				abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
				left_noise_start_idx++;
				right_noise_start_idx++;		
			}
			abs_thresh = abs_thresh * linear_snr_q4;
			
			uint32_t ptr_offest = (range_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
			abs_power = fft_out_abs[ptr_offest + micro_dop_idx];

			if (((abs_power << 4) * 2 * noise_len) <= abs_thresh) {
				continue;
			}
		}
		/* the last stage only uses the left noise data */
		else if (segment3) {
			uint32_t left_noise_start_idx = range_idx - half_win_len;

			for (int j = 0; j < noise_len; ++j) {
				uint32_t ptr_offest = (left_noise_start_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
				abs_thresh += fft_out_abs[ptr_offest + micro_dop_idx];
				left_noise_start_idx++;
			}
			abs_thresh = abs_thresh * linear_snr_q4;
			
			uint32_t ptr_offest = (range_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
			abs_power = fft_out_abs[ptr_offest + micro_dop_idx];

			if (((abs_power << 4) * noise_len) <= abs_thresh) {
				continue;
			}
		}
		
		/* Antenna aligned and Anlge calculate */
		uint32_t ant_aligned[MMW_ANT_CHANNEL_NUM];
		float sin_azi;
		float sin_elev;
		
		/* get ant iq data from ring buffer */
		uint16_t ptr_offest = micro_dop_idx + (range_idx % win_len) * MDOP_FFT_BUF_NUM;
#if (CONFIG_SOC_SERIES_RS613X)
		for (int ant_mimo_idx = 0; ant_mimo_idx < MMW_ANT_CHANNEL_NUM; ant_mimo_idx++) {
			ant_aligned[ant_mimo_idx] = *(uint32_t *)&fft_out[ptr_offest + ant_mimo_idx * micro_dop_num];
		}
#elif (CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X)
		micro_ant_data_align_2t4r(&fft_out[ptr_offest], (complex16_mdsp *)&ant_aligned[0], micro_dop_num);
#else
		#error "not support board"
#endif
		uint8_t ret_status = mmw_process_presence_pointcloud_angle_process((complex16_mdsp *)&ant_aligned[0], mmw_get_g_azi_geometry_struct_const(), mmw_get_g_elev_geometry_struct_const(), 
															&sin_azi, &sin_elev, 0, 0);

		if (ret_status != MMW_ERR_CODE_SUCCESS) {
			continue;
		}
			
		/* SNR calculate, use rx0 to calculate noise */
		uint32_t* noise = micro_cube_noise_get(range_idx);
		uint16_t range_start, range_end;
		micro_cube_range_get(&range_start, &range_end);
		uint16_t range_num = range_end - range_start;
		uint16_t range_store_len = range_num / mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
		uint32_t offest = MICRO_CFAR_ANT_DETECT_IDX * range_store_len;
		
		/* use dop_idx = 0, 1, (dop_num - 1) to do noise estimation, and in each dop_idx, use range_idx and neighbouring two range_idx to do  noise estimation. 
		 * therefore, power should multiply 9(3*3) to align with noise. */
		uint32_t snr_linear = (((uint64_t)(abs_power * abs_power) << g_mpc_ctrl.mprocess_param.micro_dop_gain_log2) * 9) / (noise[offest] >> g_mpc_ctrl.mprocess_param.micro_chirp_num_log2);

		/* Convert to point cloud result data */
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].azi_phase = sin_azi;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].ele_phase = sin_elev;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].doppler_idx = micro_dop_idx;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].range_idx = range_idx * range_extract_frequency;
		ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].sig_snr = mpc_snr_trans_db(snr_linear) >> 1;
		g_mpc_ctrl.presence_points_num++;	
	}

	return 0;
}
#endif


/* Note: mmw frames down sampling for micro frame.
 * return: TRUE if new micro frame is ready to process micro points .
 *         FALSE if new micro frame is not ready to process micro points.
 */
bool mmw_micro_point_frame(void)
{
    if (g_mpc_ctrl.frame_num == 0){
        /* set if auto gain is enabled at 1st frame */
        micro_cube_auto_gain_set(mmw_fft_autogain_get());
    }
	/* set frame div cnt */
	if (unlikely(g_mpc_ctrl.frame_div_cnt == 0)) { /* init frame_div_cnt */
		uint8_t frame_div_cnt = mmw_presence_point_cloud_get_user_cfg()->micro_frame_div_factor;
		g_mpc_ctrl.frame_div_cnt = frame_div_cnt;
		mpc_printf("micro frame div %d!\n", g_mpc_ctrl.frame_div_cnt);
	}

	/* down sampling */
	uint8_t frame_div_idx = g_mpc_ctrl.frame_div_idx++;
	if (g_mpc_ctrl.frame_div_idx >= g_mpc_ctrl.frame_div_cnt) {
		g_mpc_ctrl.frame_div_idx = 0;
	}
	if (frame_div_idx) {
		mpc_printf("micro skip frame %d!\n", frame_div_idx);
		return false;
	}

	/* micro chirp store */
	micro_cube_data_store();
	if (unlikely(!micro_cube_ready())) {
		return false;
	}
	return true;
}

#if CONFIG_MMW_PRESENCE_POINT_CLOUD
PresencePointCloudBuffer_t* mmw_presence_point_process(void)
{
	uint32_t frame_num;
	uint32_t chirp_num;
	uint16_t range_idx, range_start, range_max;
	uint32_t fft_range_idx;
	uint32_t time_start, time_dop = 0, time_cfar = 0;
	uint16_t min_range_report_idx , max_range_report_idx;
	
	uint8_t micro_dop_num = mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
#if (MDSP_FFT_CALC_ENABLE)	
	uint8_t micro_dop_num_log2 = log2_num_get(micro_dop_num);
#endif
	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
	
	PresencePointCloudBuffer_t *ptr_presence_point_cloud_3d = 0;
	
	mmw_process_mem_alloc((void**)&ptr_presence_point_cloud_3d, sizeof(*ptr_presence_point_cloud_3d));
    memset(ptr_presence_point_cloud_3d, 0, sizeof(*ptr_presence_point_cloud_3d));
	
	uint32_t *micro_cfar_abs_buf = g_mpc_ctrl.proc_abs_bufs;
	if (unlikely(micro_cfar_abs_buf == NULL)) {
		mpc_printf("data_abs_bufs %p\n", micro_cfar_abs_buf);
		
	}
	complex16_mdsp *micro_dop_buf = (complex16_mdsp *)g_mpc_ctrl.proc_bufs;
	if (unlikely(micro_dop_buf == NULL)) {
		mpc_printf("data_bufs %p\n", micro_dop_buf);
		
	}
	
	/* Update micro point param */
	frame_num = g_mpc_ctrl.frame_num++;
	if (frame_num == 0) {
		uint16_t range_num, dop_num;
		uint32_t range_mm, range_bin_size;
		mmw_fft_num_get(&range_num, &dop_num);
		mmw_range_get(&range_mm, &range_bin_size);
		if (g_mpc_ctrl.range_min_mm || g_mpc_ctrl.range_max_mm) {
			g_mpc_ctrl.range_bin_skip = (uint32_t)range_num * g_mpc_ctrl.range_min_mm/range_mm;
			g_mpc_ctrl.range_bin_max  = (uint32_t)range_num * g_mpc_ctrl.range_max_mm/range_mm;
		}

		mpc_printf("[CFG] range=%d, skip=%d, max=%d!\n",
			range_num, g_mpc_ctrl.range_bin_skip, g_mpc_ctrl.range_bin_max);
	}

	fft_range_idx = 0;
	micro_cube_range_get(&range_start, &range_max);
	min_range_report_idx = MAX(range_start, g_mpc_ctrl.range_bin_skip);
	range_idx = MAX(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_filter_config.range_threshold_idx[0]/ range_extract_frequency, min_range_report_idx);
	range_max = MIN(range_max, g_mpc_ctrl.range_bin_max);
	range_max = range_max / range_extract_frequency;
	max_range_report_idx = MIN((uint16_t)(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_filter_config.range_threshold_idx[1]/ range_extract_frequency), range_max * 0.8f);
	
	g_mpc_ctrl.presence_points_num = 0;
	chirp_num = mmw_presence_point_cloud_get_user_cfg()->micro_chirp_num;

	mmw_process_mem_alloc((void **)&ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data, sizeof(*ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data) * CONFIG_MMW_PRESENCE_POINT_MAX);
	do { /* for each range bin */
		SET_TIME_START(time_start);
		
		complex16_mdsp *fft_out;
		
#if CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_BOTH_SIDE
		if (fft_range_idx < max_range_report_idx) {
			uint32_t ptr_offest = (fft_range_idx % g_mpc_ctrl.mcfar_Param.win_len) * g_mpc_ctrl.mprocess_param.mdop_fft_buf_num;
			fft_out = micro_dop_buf + ptr_offest;
			/* micro cube process */
			complex16_cube *data_chirps = micro_cube_chirps_get(fft_range_idx);
			/* For each MIMO-RX */
			for (int ant_id = 0; ant_id < MMW_ANT_CHANNEL_NUM; ant_id++) {
				/* 1.static clutter remove. 
				* even if rectangle window is used, clutter remove is needed, because 0s are padding
				*/
				micro_clutter_remove(data_chirps, chirp_num);
				/* 2.apply hanning window and padding 0 */
				micro_dop_window(data_chirps, chirp_num, fft_out, micro_dop_num);
				/* 3.micro dop fft */
			#if (MDSP_FFT_CALC_ENABLE)
				RunComplexFFT((uint32_t *)fft_out, micro_dop_num_log2, (uint32_t *)fft_out);
			#else
				if (micro_dop_num == 16) {
					csi_cfft_q15(&csi_cfft_sR_q15_len16, (q15_t *)fft_out, 0, 1);
				} else if (micro_dop_num == 32) {
					csi_cfft_q15(&csi_cfft_sR_q15_len32, (q15_t *)fft_out, 0, 1);
				} 
			#endif
				/* for RS6130, use Rx1 for cfar detection.
					 * for RS6240, use Rx0 for cfar detection.
					 * calculate the modulus of each doppler on Rx0(6240)/Rx1(6130) */
				if (ant_id == MICRO_CFAR_ANT_DETECT_IDX) {
					uint32_t offest = (fft_range_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
					for (int16_t micro_dop_idx = 0; micro_dop_idx < micro_dop_num; micro_dop_idx++) {
						micro_cfar_abs_buf[offest + micro_dop_idx] = complex16_abs(fft_out[micro_dop_idx].real, fft_out[micro_dop_idx].imag); 
					}
				}
				/* update pointers of antanna data */
				fft_out     += micro_dop_num;
				data_chirps += chirp_num;
			}
			fft_range_idx++;
			
			if (pre_store_isready(fft_range_idx, range_idx, g_mpc_ctrl.range_bin_max, g_mpc_ctrl.mcfar_Param.half_win_len)) {
				micro_dop_both_side_cfar_doa(micro_dop_buf, micro_cfar_abs_buf, range_idx, ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data);
				range_idx++;
			}
		} else {
			/* Data storage is completed,only the last segement data is detected */
			micro_dop_both_side_cfar_doa(micro_dop_buf, micro_cfar_abs_buf, range_idx, ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data);
			range_idx++;
		}
		
#elif CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_SINGLE_SIDE
		if (fft_range_idx < max_range_report_idx) {
			if (fft_range_idx < g_mpc_ctrl.mcfar_Param.win_len || range_idx >= g_mpc_ctrl.mcfar_Param.win_len) {
				uint32_t ptr_offest = (fft_range_idx % g_mpc_ctrl.mcfar_Param.win_len) * g_mpc_ctrl.mprocess_param.mdop_fft_buf_num;
				fft_out = micro_dop_buf + ptr_offest;
				/* micro cube process */
				complex16_cube *data_chirps = micro_cube_chirps_get(fft_range_idx);
				/* For each MIMO-RX */
				for (int ant_id = 0; ant_id < MMW_ANT_CHANNEL_NUM; ant_id++) {
					/* 1.static clutter remove. 
					* even if rectangle window is used, clutter remove is needed, because 0s are padding
					*/
					micro_clutter_remove(data_chirps, chirp_num);
					/* 2.apply hanning window and padding 0 */
					micro_dop_window(data_chirps, chirp_num, fft_out, micro_dop_num);
					/* 3.micro dop fft */
				#if (MDSP_FFT_CALC_ENABLE)
					RunComplexFFT((uint32_t *)fft_out, micro_dop_num_log2, (uint32_t *)fft_out);
				#else
					if (micro_dop_num == 16) {
						csi_cfft_q15(&csi_cfft_sR_q15_len16, (q15_t *)fft_out, 0, 1);
					} else if (micro_dop_num == 32) {
						csi_cfft_q15(&csi_cfft_sR_q15_len32, (q15_t *)fft_out, 0, 1);
					} 
				#endif
					/* for RS6130, use Rx1 for cfar detection.
					 * for RS6240, use Rx0 for cfar detection.
					 * calculate the modulus of each doppler on Rx0(6240)/Rx1(6130) */
					if (ant_id == MICRO_CFAR_ANT_DETECT_IDX) {
						uint32_t offest = (fft_range_idx % g_mpc_ctrl.mcfar_Param.win_len) * micro_dop_num;
						for (int16_t micro_dop_idx = 0; micro_dop_idx < micro_dop_num; micro_dop_idx++) {
							micro_cfar_abs_buf[offest + micro_dop_idx] = complex16_abs(fft_out[micro_dop_idx].real, fft_out[micro_dop_idx].imag); 
						}
					}
					/* update pointers of antanna data */
					fft_out     += micro_dop_num;
					data_chirps += chirp_num;
				}
				fft_range_idx++;
			}
			if (fft_range_idx > g_mpc_ctrl.mcfar_Param.noise_len_segment2) {
				micro_dop_single_side_cfar_doa(micro_dop_buf, micro_cfar_abs_buf, range_idx, ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data);
				range_idx++;
			}
			
		} 
#endif
		ADD_TIME_ESCAPE(time_cfar, time_start);
	} while (range_idx < max_range_report_idx);

	ptr_presence_point_cloud_3d->presence_point_cloud_num = g_mpc_ctrl.presence_points_num;
	UPDATE_TIME_DBG_INFO(time_dop, "mdop", g_time_dbg_info.micro_dop);
	UPDATE_TIME_DBG_INFO(time_cfar, "mcfar", g_time_dbg_info.micro_cfar);
	mpc_printf("F%d cfar=%d point=%d!\n", frame_num, g_mpc_ctrl.presence_points_num);
	return ptr_presence_point_cloud_3d;
}
#endif

uint32_t mmw_presence_point_num_get(void)
{
	return g_mpc_ctrl.presence_points_num;
}

uint32_t mmw_micro_doppler_num_get(void)
{
	return mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
}

/* Reset micro point cloud state at mmw restart. */
void mmw_presence_point_restart(void)
{
	g_mpc_ctrl.frame_div_idx = 0;
	g_mpc_ctrl.frame_div_cnt = mmw_presence_point_cloud_get_user_cfg()->micro_frame_div_factor;
	g_mpc_ctrl.frame_num  = 0;
	g_mpc_ctrl.presence_points_num = 0;
	micro_cube_reset();
}

int mmw_presence_point_init(void)
{
	int ret = 0;
	/* 1.Initial default parameters of presence point cloud process. */
	uint8_t micro_chirp_num = mmw_presence_point_cloud_get_user_cfg()->micro_chirp_num;
	uint8_t micro_dop_num = mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
	uint16_t range_fft_num, doppler_fft_num;
	uint32_t range_mm, range_reol_mm;
	mmw_range_get(&range_mm, &range_reol_mm);
	
	/* dop fft len must great than or equal to chirp num */
	if (micro_dop_num < micro_chirp_num) {
		micro_dop_num = micro_chirp_num;
		g_mmw_presence_det_3d_user_cfg.micro_dop_fft_len = micro_chirp_num;
	}
	
	/* API param init */
	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	g_mpc_ctrl.range_bin_max  = MIN(CONFIG_MMW_PRESENCE_RANGE_BIN_NUM, range_fft_num);
	g_mpc_ctrl.range_bin_skip = MICRO_RANGE_BIN_SKIP_NUM;

	/* process param init */
	g_mpc_ctrl.mprocess_param.micro_chirp_num_log2 = log2_num_get(micro_chirp_num);
	g_mpc_ctrl.mprocess_param.micro_dop_num_log2 = log2_num_get(micro_dop_num);
	g_mpc_ctrl.mprocess_param.mdop_fft_buf_num = micro_dop_num * MMW_ANT_CHANNEL_NUM;
	g_mpc_ctrl.mprocess_param.micro_dop_fft_gain_log2 = (g_mpc_ctrl.mprocess_param.micro_dop_num_log2 / 2 - 1); //16chirp = 12dB for fft gain compsation
	g_mpc_ctrl.mprocess_param.micro_dop_gain_log2 = (2 + (g_mpc_ctrl.mprocess_param.micro_dop_num_log2 & 0x1)); //16chirp = 12dB for fft gain compsation
	g_mpc_ctrl.frame_div_cnt = mmw_presence_point_cloud_get_user_cfg()->micro_frame_div_factor;
	/* ca-cfar param init */
	if (g_mpc_ctrl.mcfar_Param.linear_snr_q4 == 0) {
		uint8_t snr_th_db = mmw_presence_point_cloud_get_user_cfg()->micro_ca_cfar_snr_th;
		float linear_snr = db_snr_trans_linear((float)snr_th_db);
		uint16_t linear_snr_q4 = (uint16_t)(linear_snr * 16.f);		// float --> q4
		g_mpc_ctrl.mcfar_Param.linear_snr_q4 = linear_snr_q4;
	}
	
	g_mpc_ctrl.mcfar_Param.hw_hpf_suppressed_range_bin_len =
	hw_hpf_suppressed_range_bin_len_get(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.hpf_bandwith_config, 
										g_mpc_ctrl.range_bin_max / range_extract_frequency);
	
	/* cfar guard len = round_up(MICRO_CA_CFAR_GUARD_RANGE_MM / range_reol_mm / range_extract_frequency) */
	g_mpc_ctrl.mcfar_Param.guard_len = (MICRO_CA_CFAR_GUARD_RANGE_MM + (range_reol_mm * range_extract_frequency) - 1) / (range_reol_mm * range_extract_frequency);
	
#if CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_SINGLE_SIDE
	/* cfar noise len segment2 = round_up(MICRO_CA_CFAR_NOISE_LEN / range_extract_frequency) */
	g_mpc_ctrl.mcfar_Param.noise_len_segment2 = (MICRO_CA_CFAR_NOISE_LEN + range_extract_frequency - 1) / range_extract_frequency;
	g_mpc_ctrl.mcfar_Param.noise_len_segment1 = g_mpc_ctrl.mcfar_Param.noise_len_segment2 - g_mpc_ctrl.mcfar_Param.guard_len;
	g_mpc_ctrl.mcfar_Param.win_len = g_mpc_ctrl.mcfar_Param.noise_len_segment2 + g_mpc_ctrl.mcfar_Param.guard_len + 1;
#elif CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_BOTH_SIDE	
	/* cfar noise len = round_up(MICRO_CA_CFAR_NOISE_LEN / range_extract_frequency) */
	g_mpc_ctrl.mcfar_Param.noise_len = (MICRO_CA_CFAR_NOISE_LEN + range_extract_frequency - 1) / range_extract_frequency;
	g_mpc_ctrl.mcfar_Param.half_win_len = g_mpc_ctrl.mcfar_Param.noise_len + g_mpc_ctrl.mcfar_Param.guard_len;
	g_mpc_ctrl.mcfar_Param.win_len = 2 * g_mpc_ctrl.mcfar_Param.half_win_len + 1;
#endif


#if (MICRO_DOP_WIN_TYPE_RECT == 0)
	micro_dop_win_init(micro_chirp_num);
#endif
	
	/* 2. Alloc memory for micro doppler fft buffer, 11K @ fft16, mimo8, single cfar mode; 21K @ fft16, mimo8, both cfar mode. */
	if (g_mpc_ctrl.proc_bufs == NULL) {
		uint32_t buff_size = sizeof(complex16_mdsp) * (g_mpc_ctrl.mcfar_Param.win_len * g_mpc_ctrl.mprocess_param.mdop_fft_buf_num);
		mmw_process_mem_alloc((void **)&g_mpc_ctrl.proc_bufs, buff_size);
		if (g_mpc_ctrl.proc_bufs == NULL) {
			mpc_printf("proc_bufs malloc %d failed!\n", buff_size);
			mmw_presence_point_deinit();
			ret = -1;
			goto exit;
		}
	} else { /* inited already, just do nothing */
		;
	}
	/* Alloc memory to storage the modulus of Rx0(6240)/Rx1(6130) which uses for micro cfar.
       1.4K@ fft16, mimo8, single cfar mode; 
	   2.7K@ fft16, mimo8, both cfar mode. */
	if (g_mpc_ctrl.proc_abs_bufs == NULL) {
		uint32_t buff_size = sizeof(uint32_t) * (g_mpc_ctrl.mcfar_Param.win_len * micro_dop_num);
		mmw_process_mem_alloc((void **)&g_mpc_ctrl.proc_abs_bufs, buff_size);
		if (g_mpc_ctrl.proc_abs_bufs == NULL) {
			mpc_printf("proc_abs_bufs malloc %d failed!\n", buff_size);
			mmw_presence_point_deinit();
			ret = -1;
			goto exit;
		}
	} else { /* inited already, just do nothing */
		;
	}

	/* Alloc memory to storage ca-cfar linear threshold */
	if (g_mpc_ctrl.proc_cfar_th_bufs == NULL) {
		uint32_t buff_size = sizeof(uint16_t) * (range_fft_num / range_extract_frequency);
		mmw_process_mem_alloc((void **)&g_mpc_ctrl.proc_cfar_th_bufs, buff_size);
		if (g_mpc_ctrl.proc_cfar_th_bufs == NULL) {
			mpc_printf("proc_cfar_th_bufs malloc %d failed!\n", buff_size);
			mmw_presence_point_deinit();
			ret = -1;
			goto exit;
		}
	} else { /* inited already, just do nothing */
		;
	}

	/* init ca-cfar snr linear threshold */
	if(micro_cfar_snr_linear_th_init() != 0){
		mmw_presence_point_deinit();
		ret = -1;
		goto exit;
	}

	/* 3. Alloc memory for micro cube, 74K @ 8chirps, range_extract 1 */
	uint16_t micro_cube_range_start_bin = 0;
	/* ca-cfar need storage all range bin */
	if (micro_cube_init(micro_cube_range_start_bin, g_mpc_ctrl.range_bin_max, micro_chirp_num, MMW_ANT_CHANNEL_NUM)) {
		mpc_printf("micro_cube_init failed!\n");
		ret = -1;
		goto exit;
	}

	/* 4. Initial DoA calculate module. */
	mmw_angle_init();
	mpc_printf("mpc_init buf=%p\n", g_mpc_ctrl.proc_bufs);

exit:
	return ret;
}

void mmw_presence_point_deinit(void)
{
	/* 1. Free all all resources */
	micro_cube_deinit();
	if (g_mpc_ctrl.proc_bufs) {
		mmw_process_mem_free((void **) &g_mpc_ctrl.proc_bufs);
		g_mpc_ctrl.proc_bufs = NULL;
	}
	if (g_mpc_ctrl.proc_abs_bufs) {
		mmw_process_mem_free((void **) &g_mpc_ctrl.proc_abs_bufs);
		g_mpc_ctrl.proc_abs_bufs = NULL;
	}
	if (g_mpc_ctrl.proc_cfar_th_bufs) {
		mmw_process_mem_free((void **) &g_mpc_ctrl.proc_cfar_th_bufs);
		g_mpc_ctrl.proc_cfar_th_bufs = NULL;
	}
	/* 2. Reset all parameters and state */
	memset(&g_mpc_ctrl, 0, sizeof(g_mpc_ctrl));
}

int mmw_presence_point_range_set (uint32_t min_range_mm, uint32_t max_range_mm)
{
	g_mpc_ctrl.range_min_mm = min_range_mm;
	g_mpc_ctrl.range_max_mm = max_range_mm;
	return 0;
}

int mmw_presence_point_range_get (uint32_t *min_range_mm, uint32_t *max_range_mm)
{
	if (min_range_mm == NULL || max_range_mm == NULL)
		return -1;
	*min_range_mm = g_mpc_ctrl.range_min_mm;
	*max_range_mm = g_mpc_ctrl.range_max_mm;
	return 0;
}

int mmw_micro_cfar_snr_set(uint32_t snr_linear)
{
	g_mpc_ctrl.mcfar_Param.linear_snr_q4 = (snr_linear ? snr_linear : 1) << 4;	
	return 0;
}

uint32_t mmw_micro_cfar_snr_get(void)
{
	return g_mpc_ctrl.mcfar_Param.linear_snr_q4 >> 4;
}

int mmw_micro_frame_rate_set(uint8_t mmw_frame_div_cnt)
{
	g_mmw_presence_det_3d_user_cfg.micro_frame_div_factor = mmw_frame_div_cnt;
	g_mpc_ctrl.frame_div_cnt = mmw_frame_div_cnt;
	g_mpc_ctrl.frame_div_idx = 0;
	micro_cube_reset();
	return 0;
}

uint8_t mmw_micro_frame_rate_get(void)
{
	return g_mpc_ctrl.frame_div_cnt;
}

MPC_CTRL *mmw_presence_g_mpc_ctrl_get(void)
{
	return &g_mpc_ctrl;
}
