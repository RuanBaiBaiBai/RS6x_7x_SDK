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

/* skip num of micro dop bin beside of velocity0 */
#define DOP_BIN_SKIP_0             0 //0 or 1
#define DOP_BIN_SKIP_BESIDE        0 //0~15
/* peak detect on 4 antannes */
#define DETECT_ANT_NUM_TH          ((MMW_ANT_CHANNEL_NUM + 1)>>1)

#if CONFIG_BOARD_MRS6130_P1806
PSIC_Board_e g_psic_board = BOARD_MRS6130_P1806;
#elif CONFIG_BOARD_MRS6130_P1812
PSIC_Board_e g_psic_board = BOARD_MRS6130_P1812;
#elif CONFIG_BOARD_MRS6240_P2512_CPUF || CONFIG_BOARD_MRS6240_P2512_CPUS
PSIC_Board_e g_psic_board = BOARD_MRS6240_P2512;
#elif CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2828_M62_CPUS
PSIC_Board_e g_psic_board = BOARD_MRS6241_P2828_M62;
#elif CONFIG_BOARD_MRS6241_P2840_M81_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUS
PSIC_Board_e g_psic_board = BOARD_MRS6241_P2840_M81;
#elif CONFIG_BOARD_MRS7241_P2828_M62_CPUF || CONFIG_BOARD_MRS7241_P2828_M62_CPUS
PSIC_Board_e g_psic_board = BOARD_MRS7241_P2828_M62;
#elif CONFIG_BOARD_MRS7241_P2840_M81_CPUF || CONFIG_BOARD_MRS7241_P2840_M81_CPUS
PSIC_Board_e g_psic_board = BOARD_MRS7241_P2840_M81;
#else
#error "not support board"
#endif
MPC_CTRL        g_mpc_ctrl;

uint16_t mpc_snr_trans_db(uint32_t snr_lin) //20*log10
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

/**
 * @brief This function converts the value from the dB scale (magnitude domain) to a linear magnitude.
 * 		  Formula: linear = 10^(snr_db / 20)
 * @param snr_db, input data in dB(float)
 * @return linear amplitude value after conversion(float)
 * */
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


void micro_clutter_remove(complex16_cube *data_in, uint32_t chirp_num)
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

void micro_ant_data_align_2t4r(complex16_mdsp *fft_out, complex16_mdsp *ant_aligned, uint32_t ant_step)
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



#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_SNR_THRESHOLD_METHOD
int micro_dop_cfar_doa(complex16_mdsp *fft_out, uint32_t range_idx, MmwMicroDetectData_t* ptr_mpc_buffer)
{
	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
	uint8_t  range_station = g_mpc_ctrl.cfar_stationary & 0x1;
	uint8_t  dop_station   = (g_mpc_ctrl.cfar_stationary >> 1) & 0x1;
	uint32_t abs_th_linear = mmw_micro_cfar_snr_get();
	uint32_t dop_cfar_th   = abs_th_linear * abs_th_linear;
	
	uint16_t MDOP_FFT_BUF_NUM = g_mpc_ctrl.mprocess_param.mdop_fft_buf_num;
	uint16_t micro_dop_num = mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
	uint8_t MICRO_CHIRP_NUM_LOG2 = g_mpc_ctrl.mprocess_param.micro_chirp_num_log2;
	uint8_t MICRO_DOP_GAIN_LOG2 = g_mpc_ctrl.mprocess_param.micro_dop_gain_log2;

	Noisetype *noiseth = micro_cube_noise_get(range_idx);

	/* 1. Get left and right data for range stationary detection */
  	complex16_mdsp *fft_range_left = NULL;
	complex16_mdsp *fft_range_right = NULL;
	if (range_station) {
		uint32_t buff_idx = range_idx % 3;
		if (buff_idx == 0) {
			fft_range_left	= fft_out + 2 * MDOP_FFT_BUF_NUM;
			fft_range_right	= fft_out + 1 * MDOP_FFT_BUF_NUM;
		} else if (buff_idx == 2) {
			fft_range_left	= fft_out + 1 * MDOP_FFT_BUF_NUM;
			fft_range_right	= fft_out;
			fft_out = fft_out + 2 * MDOP_FFT_BUF_NUM;
		} else { /* 1 */
			fft_range_left	= fft_out;
			fft_range_right	= fft_out + 2 * MDOP_FFT_BUF_NUM;
			fft_out = fft_out + 1 * MDOP_FFT_BUF_NUM;
		}
	}

	/* 2. CFAR on Doppler FFT result and perform DoA on CFAR result */
	for (int micro_dop_idx = (DOP_BIN_SKIP_0 + DOP_BIN_SKIP_BESIDE); micro_dop_idx < (micro_dop_num - DOP_BIN_SKIP_BESIDE); ++micro_dop_idx) {
		uint32_t is_peak = 0;
		uint32_t peak_power = 0;
		uint32_t peak_noise = 0;
		
		if (g_mpc_ctrl.presence_points_num >= CONFIG_MMW_PRESENCE_POINT_MAX) {
			break;
		}
		
		/* Calc left and right index for doppler stationary detection */
		uint32_t dop_prev = 0, dop_next = 0;
		if (dop_station) {
			dop_prev = (((uint32_t)micro_dop_idx - 1) & (micro_dop_num - 1));
			dop_next = (((uint32_t)micro_dop_idx + 1) & (micro_dop_num - 1));
		}

		/* Peak detection on each MIMO-RX. */
		for (int j = 0; j < MMW_ANT_CHANNEL_NUM; ++j) {

			/* noise threshold */
			uint32_t ant_base = j*micro_dop_num;
			uint32_t data_idx = ant_base + micro_dop_idx;
			uint32_t power_pre, power_next;

			/* Power compsation to get real noise level.
			 * Note: we do scale on fft_out because noise is too small and would be cut-off to 0 */
			uint32_t noise_threshold = ((noiseth[j] * dop_cfar_th)>>MICRO_CHIRP_NUM_LOG2);
			uint32_t power = COMPLEX16_POWER(&fft_out[data_idx])<<MICRO_DOP_GAIN_LOG2;

			/* Compare with noise threshold */
			if (power <= noise_threshold) {
				continue ;
			}

			/* Do doppler stationary detection */
			if (dop_station) {
				power_pre  = COMPLEX16_POWER(&fft_out[ant_base + dop_prev]);
				power_next = COMPLEX16_POWER(&fft_out[ant_base + dop_next]);
				power_pre  = (power_pre<<MICRO_DOP_GAIN_LOG2);
				power_next = (power_next<<MICRO_DOP_GAIN_LOG2);
				if (power < power_next || power < power_pre) {
					continue ;
				}
			}

			/* Record max peak power for calc SNR */
			if (power > peak_power) {
				peak_power = power;
				peak_noise = noise_threshold;
			}

			/* Peak filter out by half of MIMO num */
			if (++is_peak >= DETECT_ANT_NUM_TH) {
				if (range_station) {
					/* range stationary detect */
					power_pre  = COMPLEX16_POWER(&fft_range_left[data_idx]);
					power_next = COMPLEX16_POWER(&fft_range_right[data_idx]);
					power_pre  = (power_pre<<MICRO_DOP_GAIN_LOG2);
					power_next = (power_next<<MICRO_DOP_GAIN_LOG2);
					if (power <= power_next || power <= power_pre) {
						is_peak = 0;
					}
				}
				break;
			}
			(void)power_next; (void)power_pre;
		}

		/* DoA on CFAR peak. */
		if (is_peak >= DETECT_ANT_NUM_TH) {
			uint32_t ant_aligned[MMW_ANT_CHANNEL_NUM];
			
			/* Antenna aligned and Anlge DBF */
		#if (CONFIG_SOC_RS6130)
			for (int j = 0; j < MMW_ANT_CHANNEL_NUM; ++j) {
				ant_aligned[j] = *(uint32_t *)&fft_out[micro_dop_idx + j*micro_dop_num];
			}
		#elif (CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X)
			micro_ant_data_align_2t4r(&fft_out[micro_dop_idx], (complex16_mdsp *)&ant_aligned[0], micro_dop_num);
		#else
			#error "not support board"	
		#endif

			/* psic_2nd_pass_filter */
			uint8_t psic_2nd_pass_check = 0;
		#if (CONFIG_SOC_SERIES_RS613X) 
			psic_2nd_pass_check = 1;		/* RS6130 force pass threshold */
		#elif CONFIG_SOC_RS6240
			Complexf32_RealImag ptr_ant_aligned[MMW_ANT_CHANNEL_NUM];
			csi_q15_to_float((const q15_t*)ant_aligned, (float*)ptr_ant_aligned, MMW_ANT_CHANNEL_NUM * 2);
			psic_2nd_pass_check = (mmw_psic_2nd_pass_filter((MmwPsicComplexf32_reim*)ptr_ant_aligned, PSIC_2ND_PASS_FILTER_PATH_0) <  mmw_point_cloud_get_user_cfg_const()->mmw_motion_point_cloud_config.psic_2nd_pass_thres);
			psic_2nd_pass_check &= (mmw_psic_2nd_pass_filter((MmwPsicComplexf32_reim*)ptr_ant_aligned, PSIC_2ND_PASS_FILTER_PATH_1) <  mmw_point_cloud_get_user_cfg_const()->mmw_motion_point_cloud_config.psic_2nd_pass_thres);
		#elif CONFIG_SOC_RS6241 || CONFIG_SOC_SERIES_RS724X
		
			#if (CONFIG_BOARD_MRS6241_P2828_M62_CPUF) || (CONFIG_BOARD_MRS7241_P2828_CPUF)
			PsicAntPattern ant_pattern = PSIC_2ND_PASS_ANT_PATTERN_M62;
			#elif (CONFIG_BOARD_MRS6241_P2840_M81_CPUF) || (CONFIG_BOARD_MRS7241_P2840_CPUF)
			PsicAntPattern ant_pattern = PSIC_2ND_PASS_ANT_PATTERN_M81;
			#endif
		
			Complexf32_RealImag ptr_ant_aligned[MMW_ANT_CHANNEL_NUM];
			csi_q15_to_float((const q15_t*)ant_aligned, (float*)ptr_ant_aligned, MMW_ANT_CHANNEL_NUM * 2);
			psic_2nd_pass_check = (mmw_psic_2nd_pass_filter_ant_pcb((MmwPsicComplexf32_reim*)ptr_ant_aligned, PSIC_2ND_PASS_FILTER_PATH_0, ant_pattern) <  mmw_point_cloud_get_user_cfg_const()->mmw_motion_point_cloud_config.psic_2nd_pass_thres);
			psic_2nd_pass_check &= (mmw_psic_2nd_pass_filter_ant_pcb((MmwPsicComplexf32_reim*)ptr_ant_aligned, PSIC_2ND_PASS_FILTER_PATH_1, ant_pattern) <  mmw_point_cloud_get_user_cfg_const()->mmw_motion_point_cloud_config.psic_2nd_pass_thres);
		#else
			#error "not support board"
		#endif		

			if (!psic_2nd_pass_check) {
				continue;
			}

			float sin_azi;
			float sin_elev;
			uint8_t ret_status = mmw_process_presence_pointcloud_angle_process((complex16_mdsp *)&ant_aligned[0], mmw_get_g_azi_geometry_struct_const(), mmw_get_g_elev_geometry_struct_const(), 
															&sin_azi, &sin_elev, 0, 0);
			if (ret_status != MMW_ERR_CODE_SUCCESS) {
				continue;
			}
			
			/* SNR calculate, 'peak_noise+1' avoid peak_noise is 0 */
			uint32_t snr_linear = (((uint64_t)peak_power) * 9 * dop_cfar_th)/(peak_noise+1);
			
			ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].azi_phase = sin_azi;
			ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].ele_phase = sin_elev;
			ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].doppler_idx = micro_dop_idx;
			ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].range_idx = range_idx * range_extract_frequency;
			ptr_mpc_buffer[g_mpc_ctrl.presence_points_num].sig_snr = mpc_snr_trans_db(snr_linear) >> 1;
			g_mpc_ctrl.presence_points_num++;	

			/* It's optimization, skip the next point to peak if stationary detetion. */
			micro_dop_idx += dop_station;
		}
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

#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_SNR_THRESHOLD_METHOD
int mmw_presence_point_SNR_threshold_process(MPC_CTRL* g_mpc_ctrl, uint16_t range_idx_start, uint16_t range_idx_end,  MmwMicroDetectData_t* ptr_mpc_buffer)
{
	uint8_t  range_station = g_mpc_ctrl->cfar_stationary & 0x1;
	
	uint32_t fft_range_idx = 0, fft_range_pre;
	uint16_t range_idx = range_idx_start;
	
	uint16_t MDOP_FFT_BUF_NUM = g_mpc_ctrl->mprocess_param.mdop_fft_buf_num;
	
	uint8_t chirp_num = mmw_presence_point_cloud_get_user_cfg()->micro_chirp_num;
	uint8_t micro_dop_num = mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
#if (MDSP_FFT_CALC_ENABLE)
	uint8_t micro_dop_num_log2 = log2_num_get(micro_dop_num);
#endif
	
	if (unlikely(g_mpc_ctrl->proc_bufs == NULL)) {
		return MMW_ERR_CODE_NOT_READY;
	}
	
	complex16_mdsp *micro_dop_buf = (complex16_mdsp *)g_mpc_ctrl->proc_bufs;
	
	
	/* calc left idx of the first range bin */
	if (range_station) {
		fft_range_idx = (range_idx > 0 ? range_idx - 1 : 0);
		fft_range_pre = fft_range_idx + NOISE_SEC_NUM;
	} else {
		fft_range_idx = range_idx;
	}
	
	
	do { /* for each range bin */

		if (fft_range_idx < range_idx_end) {

			complex16_mdsp *fft_out;
			if (range_station) {
				/* three ring buffers for range stationary detection */
				fft_out = micro_dop_buf + (fft_range_idx % 3) * MDOP_FFT_BUF_NUM;
			} else {
				fft_out = micro_dop_buf;
			}

			/* For each MIMO-RX */
			uint32_t ant_idx = 0;
			complex16_cube *data_chirps = micro_cube_chirps_get(fft_range_idx);
			do {
				/* 
				 * static clutter remove.
				 * even if rectangle window is used, clutter remove is needed, because 0s are padding
				*/
				micro_clutter_remove(data_chirps, chirp_num);

				/* apply hanning window and padding 0 */
				micro_dop_window(data_chirps, chirp_num, fft_out, micro_dop_num);

				/* micro doppler fft */
			#if (MDSP_FFT_CALC_ENABLE)
				RunComplexFFT((uint32_t *)fft_out, micro_dop_num_log2, (uint32_t *)fft_out);
			#else
				/* scale iq data to int16_max */
				uint32_t scale_factor = 0;
				int16_t iq_data_max = 0;
				for (int idx = 0; idx < chirp_num; idx++) {
					if (ABS(fft_out[idx].real) > iq_data_max) {
						iq_data_max = ABS(fft_out[idx].real);
				}
					if (ABS(fft_out[idx].imag) > iq_data_max) {
						iq_data_max = ABS(fft_out[idx].imag);
					}
				}

				while (iq_data_max << (scale_factor + 1) <= INT16_MAX && (scale_factor + 1) < 15) {
					scale_factor++;
				}

				for (int idx = 0; idx < chirp_num; idx++) {
					fft_out[idx].real = (int16_t)(fft_out[idx].real << scale_factor);
					fft_out[idx].imag = (int16_t)(fft_out[idx].imag << scale_factor);
				}
	
				if (micro_dop_num == 16) {
					csi_cfft_q15(&csi_cfft_sR_q15_len16, (q15_t *)fft_out, 0, 1);
				} else if (micro_dop_num == 32) {
					csi_cfft_q15(&csi_cfft_sR_q15_len32, (q15_t *)fft_out, 0, 1);
				}
				uint8_t fft_compensation_gain = micro_dop_num / chirp_num;
				for (int idx = 0; idx < micro_dop_num; idx++) {
					fft_out[idx].real = (int16_t)((fft_out[idx].real * fft_compensation_gain) >> scale_factor);
					fft_out[idx].imag = (int16_t)((fft_out[idx].imag * fft_compensation_gain) >> scale_factor);
				}
			#endif

				/* update pointers of antanna data */
				fft_out     += micro_dop_num;
				data_chirps += chirp_num;
			} while (++ant_idx < MMW_ANT_CHANNEL_NUM);

			/* Calc left bins of range stationary */
			fft_range_idx++;
			if (range_station && unlikely(fft_range_idx < fft_range_pre)) {
				continue;
			}
		}

		/* micro point cloud detection */
		micro_dop_cfar_doa(micro_dop_buf, range_idx, ptr_mpc_buffer);
		if (g_mpc_ctrl->presence_points_num >= CONFIG_MMW_PRESENCE_POINT_MAX) {
			break;
		}
		
		range_idx++;

	} while (range_idx < range_idx_end);

	mpc_printf("F%d cfar=%d point=%d!\n", frame_num, g_mpc_ctrl->dop_cfar_num, g_mpc_ctrl->points_num);
	return MMW_ERR_CODE_SUCCESS;
	
}
#endif

#if CONFIG_MMW_PRESENCE_POINT_CLOUD
PresencePointCloudBuffer_t* mmw_presence_point_process(void)
{
	uint32_t frame_num;
	uint16_t range_start, range_max;
	uint16_t min_range_report_idx , max_range_report_idx;

	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;
	
	PresencePointCloudBuffer_t *ptr_presence_point_cloud_3d = 0;
	
	mmw_process_mem_alloc((void**)&ptr_presence_point_cloud_3d, sizeof(*ptr_presence_point_cloud_3d));
    memset(ptr_presence_point_cloud_3d, 0, sizeof(*ptr_presence_point_cloud_3d));
	
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

	micro_cube_range_get(&range_start, &range_max);
	min_range_report_idx = MAX(range_start, g_mpc_ctrl.range_bin_skip);
	min_range_report_idx = MAX(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_filter_config.range_threshold_idx[0]/ range_extract_frequency, min_range_report_idx);
	range_max = MIN(range_max, g_mpc_ctrl.range_bin_max);
	range_max = range_max / range_extract_frequency;
	max_range_report_idx = MIN((uint16_t)(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_filter_config.range_threshold_idx[1]/ range_extract_frequency), range_max * 0.8f);
	
	g_mpc_ctrl.presence_points_num = 0;

	mmw_process_mem_alloc((void **)&ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data, sizeof(*ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data) * CONFIG_MMW_PRESENCE_POINT_MAX);

#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_SNR_THRESHOLD_METHOD
	mmw_presence_point_SNR_threshold_process(&g_mpc_ctrl, min_range_report_idx, max_range_report_idx, ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data);
#elif CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_CFAR_METHOD
#if CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_BOTH_SIDE
	mmw_psic_micro_cfar_doa_both_side(&g_mpc_ctrl, min_range_report_idx, max_range_report_idx, ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data);
#elif CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == MICRO_CFAR_MODE_SINGLE_SIDE
	mmw_psic_micro_cfar_doa_single_side(&g_mpc_ctrl, min_range_report_idx, max_range_report_idx, ptr_presence_point_cloud_3d->ptr_presence_point_cloud_data);
#endif /* #if CFAR_MODE */
#endif /* #if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD */

	ptr_presence_point_cloud_3d->presence_point_cloud_num = g_mpc_ctrl.presence_points_num;
	
	mpc_printf("F%d cfar=%d point=%d!\n", frame_num, g_mpc_ctrl.presence_points_num);
	
	return ptr_presence_point_cloud_3d;
}
#endif	/* #if PRESENCE POINT CLOUD */

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
	
#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_SNR_THRESHOLD_METHOD
	g_mpc_ctrl.cfar_stationary = 0x0 | (0x1<<1); /* enable and doppler stationary, disable range stationary */
#endif
	
	int ret = 0;
	/* 1.Initial default parameters of presence point cloud process. */
	uint8_t micro_chirp_num = mmw_presence_point_cloud_get_user_cfg()->micro_chirp_num;
	uint8_t micro_dop_num = mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len;
	
	uint16_t range_fft_num, doppler_fft_num;
	uint32_t range_mm, range_reol_mm;
	mmw_range_get(&range_mm, &range_reol_mm);
	
	/* dop fft len must greater than or equal to chirp num */
	if (micro_dop_num < micro_chirp_num) {
		micro_dop_num = micro_chirp_num;
		mmw_presence_point_cloud_get_user_cfg()->micro_dop_fft_len = micro_chirp_num;
	}
	
	/* API param init */
	mmw_fft_num_get(&range_fft_num, &doppler_fft_num);
	g_mpc_ctrl.range_bin_max  = MIN(CONFIG_MMW_PRESENCE_RANGE_BIN_NUM, range_fft_num);
	g_mpc_ctrl.range_bin_skip = MICRO_RANGE_BIN_SKIP_NUM;
	g_mpc_ctrl.board_type = g_psic_board;

	/* process param init */
	g_mpc_ctrl.mprocess_param.micro_chirp_num_log2 = log2_num_get(micro_chirp_num);
	g_mpc_ctrl.mprocess_param.micro_dop_num_log2 = log2_num_get(micro_dop_num);
	g_mpc_ctrl.mprocess_param.mdop_fft_buf_num = micro_dop_num * MMW_ANT_CHANNEL_NUM;
	g_mpc_ctrl.mprocess_param.micro_dop_fft_gain_log2 = (g_mpc_ctrl.mprocess_param.micro_dop_num_log2 / 2 - 1); //16chirp = 12dB for fft gain compsation
	g_mpc_ctrl.mprocess_param.micro_dop_gain_log2 = (2 + (g_mpc_ctrl.mprocess_param.micro_dop_num_log2 & 0x1)); //16chirp = 12dB for fft gain compsation
	g_mpc_ctrl.frame_div_cnt = mmw_presence_point_cloud_get_user_cfg()->micro_frame_div_factor;
	/* ca-cfar param init */
	if (g_mpc_ctrl.mcfar_Param.linear_snr_q4 == 0) {
		float snr_th_db = mmw_presence_point_cloud_get_user_cfg()->micro_ca_cfar_snr_th + mmw_presence_point_cloud_get_user_cfg()->micro_ca_cfar_snr_linear_th_offest;
		float linear_snr = db_snr_trans_linear(snr_th_db);
		uint16_t linear_snr_q4 = (uint16_t)(linear_snr * 16.f);		// float --> q4
		g_mpc_ctrl.mcfar_Param.linear_snr_q4 = linear_snr_q4;
	}

#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_CFAR_METHOD
	uint8_t range_extract_frequency = mmw_presence_point_cloud_get_user_cfg()->micro_cube_range_extract_frequency;

	uint16_t range_num = g_mpc_ctrl.range_bin_max / range_extract_frequency;

	g_mpc_ctrl.mcfar_Param.hw_hpf_suppressed_range_bin_len =
	hw_hpf_suppressed_range_bin_len_get(mmw_point_cloud_get_user_cfg_const()->mmw_point_cloud_detection_config.hpf_bandwith_config, range_num);
	
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
#endif /* #if CFAR_MODE */
#endif /* #if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD */

#if (MICRO_DOP_WIN_TYPE_RECT == 0)
	micro_dop_win_init(micro_chirp_num);
#endif
	
	
#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_SNR_THRESHOLD_METHOD
	/* 2. Alloc memory for micro doppler fft buffer, 11K @ fft16, mimo8, single cfar mode; 21K @ fft16, mimo8, both cfar mode. */
	if (g_mpc_ctrl.proc_bufs == NULL) {
		uint32_t buff_size = sizeof(complex16_mdsp) * (3 * g_mpc_ctrl.mprocess_param.mdop_fft_buf_num);
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
#elif CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_CFAR_METHOD
	ret = mmw_psic_micro_cfar_mem_init(&g_mpc_ctrl);
	if (ret != MMW_ERR_CODE_SUCCESS) {
		mmw_presence_point_deinit();
		ret = -1;
		goto exit;
	}
#endif

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
#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_SNR_THRESHOLD_METHOD	
	if (g_mpc_ctrl.proc_bufs) {
		mmw_process_mem_free((void **) &g_mpc_ctrl.proc_bufs);
		g_mpc_ctrl.proc_bufs = NULL;
	}
#elif CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_CFAR_METHOD
	mmw_psic_micro_cfar_mem_deinit(&g_mpc_ctrl);
#endif	
	
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
	mmw_presence_point_cloud_get_user_cfg()->micro_frame_div_factor = mmw_frame_div_cnt;
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
