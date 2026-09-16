/**
 **************************************************************************************************
 * *@brief   factory test function define.
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

#include "ft_noise_est.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_point_cloud_psic_lib.h"
#include "csi_math.h"
#include "log.h"

/* mmw radio filter DC index */
#define CONFIG_FILTER_DC_RANGE_MM				(400)
/* config peak range bin min range scope*/
#define CONFIG_MIN_SCOPE_RANGE_MM				(3000)

/*mmw software param config*/
#define CONFIG_NOISE_SEM_EN						(0)
#define CONFIG_NOISE_BUF_MAX_CHIRP_NUM 			(128)

#if CONFIG_SOC_SERIES_RS624X  || CONFIG_SOC_SERIES_RS724X
#define CONFIG_NOISE_EST_MIMO_MODE 	(MMW_MIMO_2T4R) 	/* MIMO mode config*/
#define CONFIG_MMW_RX_ANT_IO_MASK 	0xF					/* ant msk config*/
#elif CONFIG_SOC_SERIES_RS613X
#define CONFIG_NOISE_EST_MIMO_MODE 	(MMW_MIMO_1T3R) 
#define CONFIG_MMW_RX_ANT_IO_MASK 	0x7
#else
#error "please select valid board!"
#endif

#define RX_ANT_VAILD(rx_id) (CONFIG_MMW_RX_ANT_IO_MASK & BIT(rx_id))
#define RBIN_ANT_PWR(PTR_BUF, RBIN_NUM, RX_ANT, BIN) (PTR_BUF[(RBIN_NUM) * (RX_ANT) + (BIN)])

#define POWER_CAL(a, b)		(a * a + b * b)

/*mmw radar param config */
//#define CONFIG_NOISE_CHIRP_NUM					(128)

/* ft task status define */
struct ft_task_status_t ft_task_status_obj;

extern void FT_RspSemRelease(void);

#define RBIN_SCOPE_OF_START_ID	(0)
#define RBIN_SCOPE_OF_END_ID	(1)


static struct mmw_noise_gap_param_t massess_gap = {
	.peak_pwr 	= 400.f,		// unit 0.01db
	.rbin_gap_num 	= 2,	// rbin gap num
	.snr 		= 600.f,		// unit 0.01db
	.min 		= 500.f,		// unit 0.01db
	.avge 		= 500.f,		// unit 0.01db
	.max 		= 700.f		// unit 0.01db
};

static struct mmw_noise_est_param {
	uint32_t hw_init_id;
	uint16_t range_fft_len;
	uint16_t dop_fft_len;
	uint16_t rbin_scop[2];
	uint16_t rbin_scop_snr[2];
} g_rbin_noise_est_param = {
	.hw_init_id = 0,
	.range_fft_len = 128,
	.dop_fft_len = 32,
	.rbin_scop = {
		[0] = 0,
		[1] = 0
	},
	.rbin_scop_snr = {
		[0] = 0,
		[1] = 0
	}
};

struct radar_noise_est_t *mradar_est_data = 0;
struct mmw_noise_report_t *report = 0;
struct mmw_noise_est_final_report_t *noise_est_report = 0;
extern struct mmw_noise_est_frame_t g_noise_est_frame;

//struct mmw_noise_est_task_status_t ft_noise_est_task_status_obj;

extern uint8_t g_est_rx_msk;

extern int16_t fast_db_trans_f32(float lin_data);

struct ft_task_status_t* get_ft_task_status(void) {
	return &ft_task_status_obj;
}

struct radar_noise_est_t** get_radar_noise_est_param(void){
	return &mradar_est_data;
}

/** @brief set noise estimation param */
void ft_noise_est_param_set(struct mmw_nosie_est_recive_param_t *param)
{
	/* check validity of param */ 
	if (param == NULL)  {
		return ;
	}
	if (param->est_rx_msk == 0 || param->est_rx_msk > CONFIG_MMW_RX_ANT_IO_MASK) {
		return ;
	}
	g_est_rx_msk = param->est_rx_msk;
	
	mmw_noise_hw_init_set(param->hw_init_id);
	
	mmw_noise_assess_gap_param_config(NOISE_PEAK_PWR_GAP_ID, param->gap.peak_pwr);
	mmw_noise_assess_gap_param_config(NOISE_SNR_GAP_ID, param->gap.snr);
	mmw_noise_assess_gap_param_config(NOISE_MIN_GAP_ID, param->gap.min);
	mmw_noise_assess_gap_param_config(NOISE_AVGE_GAP_ID, param->gap.avge);
	mmw_noise_assess_gap_param_config(NOISE_MAX_GAP_ID, param->gap.max);
	
	/* config noise estimation param  */
	/* 
		If user don't config chirp & frame params, use default param:
		59GHz, 128fft, 8cm, +-10m/s, 16 dfft
	*/
	if (param->range_mm == 0) {
		g_noise_est_frame.range_mm = FT_DEFAULT_RANGE;
	} else {
		g_noise_est_frame.range_mm = param->range_mm;
	}

	if (param->range_resolut == 0) {
		g_noise_est_frame.resol_mm = FT_DEFAULT_RANGE_RES;
	} else {
		g_noise_est_frame.resol_mm = param->range_resolut;
	}

	if (param->start_freq == 0) {
		g_noise_est_frame.startFreq_MHz = FT_DEFAULT_START_FREQ;
	} else {
		g_noise_est_frame.startFreq_MHz = param->start_freq;
	}

	if (param->veloc_mm == 0) {
		g_noise_est_frame.velocity_mm = NOISE_FT_DEFAULT_VELOC;
	} else {
		g_noise_est_frame.velocity_mm = param->veloc_mm;
	}

	if (param->vel_resol == 0) {
		g_noise_est_frame.veloc_resol = NOISE_FT_DEFAULT_VEL_RES;
	} else {
		g_noise_est_frame.veloc_resol = param->vel_resol;
	}

	if (param->frame_num == 0) {
		g_noise_est_frame.frame_num = NOISE_FT_DEFAULT_FRAME_NUM + CONFIG_MMW_VENDOR_FILTER_FRAME_NUM;
	} else {
		g_noise_est_frame.frame_num = param->frame_num + CONFIG_MMW_VENDOR_FILTER_FRAME_NUM;
	}
}


/** @brief set noise est result to struct "est_report" */
void mmw_noise_est_report_data(int event, struct mmw_noise_report_t *report, struct mmw_noise_est_final_report_t *est_report){

	est_report->rx_msk = g_est_rx_msk;
	switch (event) {
		case MMW_NOISE_RX_ANT_REPORT_EVT:{
			uint8_t rx_ant_idx = report->rx_id;
			est_report->factor[rx_ant_idx].peak_pwr_avge = report->peak_pwr_db;
			est_report->factor[rx_ant_idx].pwr_high_avge = report->noise_high_db;
			est_report->factor[rx_ant_idx].pwr_low_avge = report->noise_low_db;
			est_report->factor[rx_ant_idx].pwr_avge = report->noise_avge_db;
			est_report->factor[rx_ant_idx].peak_snr_avge = report->snr_db;
			est_report->peak_distance = report->peak_distance;
			break;
		}
		case MMW_NOISE_FINAL_REPORT_EVT:{
			est_report->noise_db = report->noise_avge_db;
			est_report->fail_msk = report->test_fail_mask;
			est_report->invalid_msk = report->test_invalid_mask;
			break;
		}
		default:
			break;
	}
}

/** @brief set radar perfermance typical value */
int ft_noise_factor_set(struct mmw_noise_factor_t *factor_recive, struct mmw_noise_factor_t *factor_param)
{
	int ret = 0;
	if (factor_recive == NULL) {
		ret = -1;
		goto exit;
	}
	for (uint8_t mimo_rx_ant_id = 0; mimo_rx_ant_id < CONFIG_MAX_MIMO_ANT; mimo_rx_ant_id++) {
		factor_param[mimo_rx_ant_id].pwr_high_avge = factor_recive[mimo_rx_ant_id].pwr_high_avge;
		factor_param[mimo_rx_ant_id].pwr_low_avge = factor_recive[mimo_rx_ant_id].pwr_low_avge;
		factor_param[mimo_rx_ant_id].pwr_avge = factor_recive[mimo_rx_ant_id].pwr_avge;
		factor_param[mimo_rx_ant_id].peak_pwr_avge = factor_recive[mimo_rx_ant_id].peak_pwr_avge;
		factor_param[mimo_rx_ant_id].peak_snr_avge = factor_recive[mimo_rx_ant_id].peak_snr_avge;
	}
exit:	
	return ret;
}

/** @brief set noise est range  */
int ft_noise_est_range_set(uint16_t range_min, uint16_t range_max, struct mmw_noise_usr_t *usr_param)
{
	usr_param->range_min_mm 	= range_min;
	usr_param->range_max_mm 	= range_max;
	FT_LOG_PRINT("noise config range %dmm - %dmm\n", range_min, range_max);
	return 0;
}

void mmw_noise_hw_init_set(uint32_t id)
{
	g_rbin_noise_est_param.hw_init_id = id;
}

/** @brief set scope of noise est range index */
int mmw_noise_meas_scope_set(uint16_t start_index, uint16_t end_index)
{
	if ((start_index > end_index) || (end_index >= g_rbin_noise_est_param.range_fft_len)) {
	}
	g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID] 	= start_index;
	g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID] 	= end_index;
	return 0;
}

/** @brief config gap value of radar perfermance value  */
int mmw_noise_assess_gap_param_config(uint8_t gap_id, int16_t gap_val)
{
	int ret = 0;
	struct mmw_noise_gap_param_t	*assess = &massess_gap;

	if (gap_val < 0) {
		return -1;
	}

	switch (gap_id) {
	case NOISE_PEAK_PWR_GAP_ID:
		assess->peak_pwr = gap_val;
		break;
	case NOISE_SNR_GAP_ID:
		assess->snr = gap_val;
		break;
	case NOISE_MIN_GAP_ID:
		assess->min = gap_val;
		break;
	case NOISE_AVGE_GAP_ID:
		assess->avge = gap_val;
		break;
	case NOISE_MAX_GAP_ID:
		assess->max = gap_val;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

uint16_t round_2power(uint16_t fft_len)
{
	uint16_t power = 1;
	while (fft_len > power) {
		power <<= 1;
	}
	return power;
}

/** @brief init param before noise eatimation process
 * 1. init noise estimation param 
 * 2. memlloc buffer use for storage data which in noise est processing
 * */
int ft_noise_est_init(struct mmw_noise_est_frame_t *pFrame)
{
	int ret = 0;
	float *noise_est_buf = 0;
	
	if (ft_task_status_obj.running == true) {
		ret = -1;
		return ret;
	}
	
	
	if (report == NULL) {
		mmw_process_mem_alloc((void **)&report, sizeof(*report));
	}
	if (noise_est_report == NULL) {
		mmw_process_mem_alloc((void **)&noise_est_report, sizeof(*noise_est_report));
	}
	/* init g_rbin_noise_est_param struct */
	uint16_t range_fft_len = (pFrame->range_mm + pFrame->resol_mm - 1) / pFrame->resol_mm;
	range_fft_len = round_2power(range_fft_len);
	g_rbin_noise_est_param.range_fft_len = range_fft_len;
	
	uint16_t dop_fft_len = (NOISE_FT_DEFAULT_VELOC / NOISE_FT_DEFAULT_VEL_RES) * 2;
	g_rbin_noise_est_param.dop_fft_len = dop_fft_len;
	
	/* if user not set scope of range index, use default param as below:
	 * 		start range index: 	range_fft_len*3/10
	 * 		end range index:	(range_fft_len*9 + 9)/10 - 1
	 * else 
	 * 		check the scope is valid.
	*/
	if (g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID] == 0
		&& g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID] == 0) {
		g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID] = range_fft_len*3/10;
		g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID] = (range_fft_len*9 + 9)/10 - 1;
	} else {
		if (g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID] >= range_fft_len) {
			g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID] = range_fft_len -1;
		}
		if (g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID] >= range_fft_len) {
			g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID] = range_fft_len -1;
		}
	}

	/* set scope of range index use for calculate snr */
	if (g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] == 0
		&& g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] == 0) {
		g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] = range_fft_len*7/10;
		g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] = (range_fft_len*9 + 9)/10 - 1;
	} else {
		if (g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] >= range_fft_len) {
			g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] = range_fft_len -1;
		}
		if (g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] >= range_fft_len) {
			g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] = range_fft_len -1;
		}
	}

	FT_LOG_PRINT("start %d end %d frame period %d\n", g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID],
									g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID], pFrame->frame_period_ms);

	mmw_process_mem_alloc((void**)&noise_est_buf, CONFIG_MAX_MIMO_ANT * range_fft_len * sizeof(float));
	if (noise_est_buf == NULL) {
		ret = -1;
		ft_noise_est_deinit();
		return ret;
	}

	for (uint8_t rx_idx = 0; rx_idx < CONFIG_MAX_MIMO_ANT; rx_idx++) {
		mradar_est_data->noise_param.rbin_max_pwr_idx[rx_idx] = 0;
		mradar_est_data->noise_param.rbin_max_pwr[rx_idx] = 0;
	}
	
	mradar_est_data->noise_param.frame_num = pFrame->frame_num;
	ft_task_status_obj.curr_frame_index = 0;
	ft_task_status_obj.running = true;	
	
	memset(noise_est_buf, 0, CONFIG_MAX_MIMO_ANT * range_fft_len * sizeof(float));
	mradar_est_data->noise_est_buf = noise_est_buf;
	
	memset(report, 0, sizeof(*report));
	memset(noise_est_report, 0, sizeof(*noise_est_report));
	return ret;
}


void ft_noise_est_deinit(void) 
{
	if (mradar_est_data->noise_est_buf != NULL) {
		mmw_process_mem_free((void **)&mradar_est_data->noise_est_buf);
	}
	if (mradar_est_data != NULL) {
		mmw_process_mem_free((void**)&mradar_est_data);
		mradar_est_data = NULL;
	}
	if (report != NULL) {
		mmw_process_mem_free((void **)&report);
	}
	if (noise_est_report != NULL) {
		mmw_process_mem_free((void **)&noise_est_report);
	}
	
	ft_task_status_obj.running = false;
	ft_task_status_obj.curr_frame_index = 0;
}

int factory_test_mmw_perf_start(uint8_t *ft_cmd)
{
	int ret;
	/* malloc param buffer */
	if (mradar_est_data == NULL) {
		mmw_process_mem_alloc((void **)&mradar_est_data, sizeof(*mradar_est_data));
	}
	/* get receive HIF msg */
	struct mmw_nosie_est_recive_param_t *nosie_est_recive_param = (struct mmw_nosie_est_recive_param_t *)((uint8_t*)ft_cmd);
	/* set noise estmation param */
	ft_noise_est_param_set(nosie_est_recive_param);
	/* set typical value for each enabled ants */
	ft_noise_factor_set(nosie_est_recive_param->factor, mradar_est_data->factor_param);
	/* set detect range */
	ft_noise_est_range_set(NOISE_FT_DEFAULT_RANGE_MIN_MM, NOISE_FT_DEFAULT_RANGE_MAX_MM, &mradar_est_data->usr_param);			/* min 0.9m, max 1.4m */
	/* set frame period */
	g_noise_est_frame.frame_period_ms = NOISE_FT_DEFAULT_FRAME_PERIOD;
	/* set noise est scope */
	mmw_noise_meas_scope_set(nosie_est_recive_param->est_rbin_start_id, nosie_est_recive_param->est_rbin_end_id);
	/* start noise estimation */
	ret = mmw_noise_est_start(&g_noise_est_frame, mradar_est_data);
	return ret;
}

/** @brief calculate noise and set result in struct mmw_noise_report_t  */
void mmw_noise_process_finish(struct radar_noise_est_t *mradar_est_data, struct mmw_noise_report_t *report){
	
	uint32_t frame_num = ft_task_status_obj.curr_frame_index - CONFIG_MMW_VENDOR_FILTER_FRAME_NUM;
	float *rbin_max_pwr = &mradar_est_data->noise_param.rbin_max_pwr[0];
	float *rbin_power; 
	float noise_db = 0.f, all_noise_db = 0.f;
	bool pass = false;
	uint8_t tx_num = mradar_est_data->noise_param.tx_num;
	uint8_t rx_num = mradar_est_data->noise_param.rx_num;
	
	mmw_ctrl_stop();
	ft_task_status_obj.running = false;
	ft_task_status_obj.curr_frame_index = 0;
	
	if (mradar_est_data->noise_est_buf == NULL) {
		LOG_PRINT("mradar_est_data->noise_est_buf == NULL\n");
		return;
	}

	if (rx_num > CONFIG_MAX_MIMO_ANT ||
		mradar_est_data->noise_param.range_len > g_rbin_noise_est_param.range_fft_len) {
		return;
	}
	
	rbin_power = mradar_est_data->noise_est_buf;

	for (int rx_ant_idx = 0; rx_ant_idx < CONFIG_MAX_MIMO_ANT; rx_ant_idx++) {
		for (int range_idx = 0; range_idx < mradar_est_data->noise_param.range_len; range_idx++) {
			rbin_power[range_idx] = rbin_power[range_idx] / ((mradar_est_data->noise_param.doppler_len >> 1) * frame_num);
		}
		rbin_power += g_rbin_noise_est_param.range_fft_len;
		rbin_max_pwr[rx_ant_idx] = rbin_max_pwr[rx_ant_idx] / frame_num;
		
		pass = mmw_radio_noise_est_pass(rx_ant_idx, &noise_db, mradar_est_data, report);
		if (pass == true) {
			FT_LOG_PRINT("mmw noise est on rx ant %d test pass\n", rx_ant_idx);
		} else {
			report->test_fail_mask |= BIT(rx_ant_idx);
			FT_LOG_PRINT("mmw noise est on rx ant %d test fail %x\n", rx_ant_idx, report->test_fail_mask);
		}
		all_noise_db += noise_db;
	}
	report->noise_avge_db = all_noise_db / (tx_num * rx_num);
	mmw_noise_est_report_data(MMW_NOISE_FINAL_REPORT_EVT, report, noise_est_report);
	
	FT_RspSemRelease();
}

static int noise_est_frame_check(struct radar_noise_est_t *mradar_est_data)
{
	uint8_t txrx, work;
	uint16_t range_len, doppler_len;
	uint32_t range_mm, resol_mm;

	mmw_fft_num_get(&range_len, &doppler_len);
	if (range_len != g_rbin_noise_est_param.range_fft_len || doppler_len != g_rbin_noise_est_param.dop_fft_len){
		LOG_PRINT("Err: mmw_fft_num_get for range_len %d doppler_len %d\n", range_len, doppler_len);
		return -1;
	}

	mmw_mode_get (&txrx, &work);
	if ((txrx != CONFIG_NOISE_EST_MIMO_MODE) || (work != MMW_WORK_MODE_2DFFT)) {
		LOG_PRINT("Err: mmw_mode_get for txrx %d work %d\n", txrx, work);
		return -1;
	}

	if (CONFIG_NOISE_EST_MIMO_MODE == MMW_MIMO_2T4R) {
			mradar_est_data->noise_param.tx_num = 2;
			mradar_est_data->noise_param.rx_num = 4;
	} else if (CONFIG_NOISE_EST_MIMO_MODE == MMW_MIMO_1T3R) {
			mradar_est_data->noise_param.tx_num = 1;
			mradar_est_data->noise_param.rx_num = 3;
	} else {
			return -1;
	}

	mmw_range_get (&range_mm, &resol_mm);
	if (resol_mm) {
		mradar_est_data->noise_param.range_index_min = mradar_est_data->usr_param.range_min_mm / resol_mm;
		mradar_est_data->noise_param.range_index_max = mradar_est_data->usr_param.range_max_mm / resol_mm;
		mradar_est_data->noise_param.range_index_max = MAX(mradar_est_data->noise_param.range_index_max, (CONFIG_MIN_SCOPE_RANGE_MM/resol_mm));
		mradar_est_data->noise_param.range_index_dc = MAX(1, CONFIG_FILTER_DC_RANGE_MM / resol_mm);
	} else {
		mradar_est_data->noise_param.range_index_min = 0;
		mradar_est_data->noise_param.range_index_max = 0;
		mradar_est_data->noise_param.range_index_dc = 1;
	}

	if (range_len) {
		mradar_est_data->noise_param.resol_mm = range_mm / range_len;
	} else {
		mradar_est_data->noise_param.resol_mm = resol_mm;
	}
	if (range_len == g_rbin_noise_est_param.range_fft_len) {
		mradar_est_data->noise_param.range_len = range_len;
	} else {
		mradar_est_data->noise_param.range_len = g_rbin_noise_est_param.range_fft_len;
	}
	mradar_est_data->noise_param.doppler_len = doppler_len;
	return 0;
}

void fft_autogain_restore_f32(complex16_cube *buffer, Complexf32_RealImag *buffer_f32, uint8_t tx_idx, uint16_t range_idx, uint16_t buff_size) 
{
	
	uint8_t q_tar;  // q_tar is the total q value that shoud be recovered.
	int8_t shift_value;
	Complex32_RealImag *buffer_32 = 0;
	mmw_process_mem_alloc((void **)&buffer_32, buff_size * sizeof(Complex32_RealImag));
	
	/* get auto gain factor */
	q_tar = mmw_fft_autogain_base(tx_idx) + mmw_fft_autogain_range(range_idx, tx_idx);
	
	/* convert buffer to Q31 */
	shift_value = 16 - q_tar;  // shift_value is the value that will be left-shifted to get q31 data.
	for (uint16_t idx = 0; idx < buff_size; idx++) {
		buffer_32[idx].real = (int32_t)buffer[idx].real << shift_value;
		buffer_32[idx].imag = (int32_t)buffer[idx].imag << shift_value;
		
		/* Q31 to float */
		buffer_f32[idx].real = (float)buffer_32[idx].real / 2147483648;
		buffer_f32[idx].imag = (float)buffer_32[idx].imag / 2147483648;
		
	}

	mmw_process_mem_free((void **)&buffer_32);
}

/** @brief mmw wave callback
 * main noise estmation process function.
 * 1、check mmw frame is valid.
 * 2、get all dop data for each rbin .
 * 3、restore auto gain.
 * 4、calculate the sum of all low speed dop bin power.
 * 5、storage the sum value in mradar_est_data.noise_est_buf.
 * 6、get 0 dop data, then find max power and max power index in 0 dop for each ant.
 * */
static int mmw_ctrl_noise_est_frame_cb(void *mmw_data, void *arg) {
	
	FT_LOG_PRINT("Radar callback process noise data, frame: %d\n", ft_task_status_obj.curr_frame_index);
	int ret = 0;
	float power = 0.f;

	ret = noise_est_frame_check(mradar_est_data);
	if (ret < 0) {
		LOG_PRINT("err: frame param check error\n");
		return -1;
	}

	uint8_t ant_mimo_id = 0;
	uint16_t dop_len = mradar_est_data->noise_param.doppler_len;
	uint16_t range_len = g_rbin_noise_est_param.range_fft_len;
	
	complex16_cube *noise_dop_buf = 0;
	Complexf32_RealImag *noise_dop_buffer_f32 = 0;
	complex16_cube *noise_range_buf = 0;
	Complexf32_RealImag *noise_range_buffer_f32 = 0;

	uint16_t rbin_num = mradar_est_data->noise_param.range_index_max + 1;
	uint16_t rbin_off = mradar_est_data->noise_param.range_index_dc;
	ft_task_status_obj.curr_frame_index++;
	
	if (mradar_est_data->noise_est_buf == NULL) {
		return -1;
	}

	if (ft_task_status_obj.curr_frame_index <= CONFIG_MMW_VENDOR_FILTER_FRAME_NUM) {
		return 0;
	}
	
	mmw_process_mem_alloc((void **)&noise_dop_buf, dop_len * sizeof(complex16_cube));
	if (noise_dop_buf == NULL) {
		LOG_PRINT("noise_dop_buf malloc fail\n");
		goto exit;
	}
	mmw_process_mem_alloc((void **)&noise_dop_buffer_f32, dop_len * sizeof(Complexf32_RealImag));
	if (noise_dop_buffer_f32 == NULL) {
		LOG_PRINT("noise_dop_buffer_f32 malloc fail\n");
		goto exit;
	}
	mmw_process_mem_alloc((void **)&noise_range_buf, rbin_num * sizeof(complex16_cube));
	if (noise_range_buf == NULL) {
		LOG_PRINT("noise_range_buf malloc fail\n");
		goto exit;
	}
	mmw_process_mem_alloc((void **)&noise_range_buffer_f32, rbin_num * sizeof(Complexf32_RealImag));
	if (noise_range_buffer_f32 == NULL) {
		LOG_PRINT("noise_range_buffer_f32 malloc fail\n");
		goto exit;
	}
	mmw_motion_cube_access_open();
	FT_LOG_PRINT("ongoing collecting noise frame count %d rxnum %d rangelen %d doplen %d rbin_num %d\n", ft_task_status_obj.curr_frame_index,
		mradar_est_data->noise_param.rx_num, mradar_est_data->noise_param.range_len, mradar_est_data->noise_param.doppler_len, rbin_num);
	
	if (rbin_num >= range_len) {
		rbin_num = range_len;
	}

	for (uint8_t tx_idx = 0; tx_idx < mradar_est_data->noise_param.tx_num; tx_idx++) {
		for (uint8_t rx_idx = 0; rx_idx < mradar_est_data->noise_param.rx_num; rx_idx++) {
			if (!RX_ANT_VAILD(rx_idx)) {
				continue;
			}
		/* 1、get all dop data for each rbin 
		 * 2、restore auto gain
		 * 3、calculate sum of all low speed dop bin power
		 * 4、storage the sum in mradar_est_data.noise_est_buf
		 * */
			for (uint16_t range_idx = 0; range_idx < mradar_est_data->noise_param.range_len; range_idx++) {
				mmw_fft_doppler(noise_dop_buf, dop_len, tx_idx, rx_idx, range_idx);
				fft_autogain_restore_f32(noise_dop_buf, noise_dop_buffer_f32, tx_idx, range_idx, dop_len);
				power = noise_est_buf_update(noise_dop_buffer_f32, dop_len);
				mradar_est_data->noise_est_buf[(ant_mimo_id * range_len) + range_idx] += power;
			}
		
			/* get 0 dop data */
			mmw_process_fft_range_f32(noise_range_buffer_f32, noise_range_buf, rbin_num, tx_idx, rx_idx, dop_len >> 1);
		
			/* find max power and max power index in 0 dop for each ant.
			 * if max power index is different between each frame, print a waring message */
			noise_rbin_buf_update(noise_range_buffer_f32, ant_mimo_id, rbin_num, rbin_off, &mradar_est_data->noise_param);
			ant_mimo_id++;
		}
	}
	mmw_fft_autogain_clear();	
	mmw_motion_cube_access_close();

exit:
	if (noise_dop_buf) {
		mmw_process_mem_free((void **)&noise_dop_buf);
	}
	if (noise_dop_buffer_f32) {
		mmw_process_mem_free((void **)&noise_dop_buffer_f32);
	}
	if (noise_range_buf) {
		mmw_process_mem_free((void **)&noise_range_buf);
	}
	if (noise_range_buffer_f32) {
		mmw_process_mem_free((void **)&noise_range_buffer_f32);
	}

	/* process finish */
	if(ft_task_status_obj.curr_frame_index >= g_noise_est_frame.frame_num){
		mmw_noise_process_finish(mradar_est_data, report);
	}
	
	return 0;
}

float noise_est_buf_update(Complexf32_RealImag *pBuf_in, uint16_t dop_num)
{
	float power = 0.0f;
	uint16_t filter_start_index = dop_num >> 2; // 16/4 = 4
	uint16_t filter_end_index = filter_start_index + (dop_num >> 1) - 1; // 4 + 16/2 = 12

	for (int dp = 0; dp < dop_num; dp++) {
		if ((dp < filter_start_index || dp > filter_end_index) || dop_num == 1) {
			power += pBuf_in[dp].real * pBuf_in[dp].real + pBuf_in[dp].imag * pBuf_in[dp].imag;
		}
	}
	return power;
}

static void noise_rbin_buf_update(Complexf32_RealImag *pBuf, uint8_t rx_idx, uint16_t rbin_num, uint16_t rbin_off, struct mmw_noise_param_t  *noise_param)
{
	struct mmw_meas_pos_t pos;

	mmw_dsp_poweron();
	mmw_radar_find_rbin_postion(pBuf, rbin_off, rbin_num, 1, &pos);

	if (noise_param->rbin_max_pwr_idx[rx_idx] == 0) {
		noise_param->rbin_max_pwr_idx[rx_idx] = pos.rbin_max_pwr_idx;
	}
	
	if (noise_param->rbin_max_pwr_idx[rx_idx] != pos.rbin_max_pwr_idx) {
		if ((noise_param->rbin_max_pwr_idx[rx_idx] > (pos.rbin_max_pwr_idx + 1))
			|| ((noise_param->rbin_max_pwr_idx[rx_idx] + 1) < pos.rbin_max_pwr_idx)) {
			report->test_invalid_mask |= BIT(PEAK_INVAILD);
		}
		FT_LOG_PRINT("error rx idx %d pos index %d power %f\n", rx_idx, pos.rbin_max_pwr_idx, pos.rbin_max_pwr);
		pos.rbin_max_pwr_idx = noise_param->rbin_max_pwr_idx[rx_idx];
		pos.rbin_max_pwr = pBuf[pos.rbin_max_pwr_idx].real * pBuf[pos.rbin_max_pwr_idx].real 
							+ pBuf[pos.rbin_max_pwr_idx].imag * pBuf[pos.rbin_max_pwr_idx].imag;
	}
	noise_param->rbin_max_pwr[rx_idx] += pos.rbin_max_pwr;

}

int mmw_radar_find_rbin_postion(Complexf32_RealImag *cube, uint16_t start_offset, uint16_t rbin_num, uint8_t rx_ant_num, struct mmw_meas_pos_t *pos)
{
	Complexf32_RealImag *rbin;
	float power = 0.f, max_power = 0.f;
	uint16_t rbinoff, max_pwr_rbin_idx = 0;

	/* find out the max power from all rx ant cube rbin */
	for (uint8_t rx_idx = 0; rx_idx < rx_ant_num; rx_idx++) {
		rbinoff = rx_idx * rbin_num;
		for (uint16_t range_idx = start_offset; range_idx < rbin_num; range_idx++) {
			rbin = &cube[rbinoff + range_idx];
			power = rbin->real * (rbin->real) + (rbin->imag) * (rbin->imag);
			if (max_power < power) {
				max_power = power;
				max_pwr_rbin_idx = range_idx;
			}
		}
	}

	pos->rbin_max_pwr_idx = max_pwr_rbin_idx;
	pos->rbin_max_pwr = max_power;
	return 0;
}


/** @brief caculate noise min/max/avge value and snr.
 * compare the diff of result and typical value with gap value
 * */
static bool mmw_radio_noise_est_pass(uint16_t rx_ant_id, float *avge_noise, struct radar_noise_est_t *mradar_est_data, struct mmw_noise_report_t *report)
{
	struct mmw_noise_factor_t *pfactor;

	/* initial nosise est param */
	bool noise_est_pass = true;
	float noise_avge_snr_db = 0.f;
	float noise_avge_snr_pwr = 0;
	float noise_avge_pwr = 0;
	float noise_max_pwr = 0;
	float noise_min_pwr = INFINITY;

	/* temp variables */
	float noise_power = 0.f;

	if (mradar_est_data->noise_est_buf == NULL || rx_ant_id >= CONFIG_MAX_MIMO_ANT) {
		return false;
	}

	pfactor = &mradar_est_data->factor_param[rx_ant_id];

	for (uint16_t rbin_idx = g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID]; rbin_idx <= g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID]; rbin_idx++) {
		noise_power += RBIN_ANT_PWR(mradar_est_data->noise_est_buf, g_rbin_noise_est_param.range_fft_len, rx_ant_id, rbin_idx);
	}
	noise_power = noise_power / (g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] - g_rbin_noise_est_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] + 1);
	noise_avge_snr_pwr = noise_power;
	noise_avge_snr_db = (fast_db_trans_f32(noise_avge_snr_pwr)) * 0.0625f;	
	noise_avge_snr_db = noise_avge_snr_db * 100; // uint: 0.01dB

	/* init all */
	noise_power = 0.f;
	float noise_power_tmp = 0.f;

	for (uint16_t rbin_idx = g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID]; rbin_idx <= g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID]; rbin_idx++) {
		noise_power_tmp = RBIN_ANT_PWR(mradar_est_data->noise_est_buf, g_rbin_noise_est_param.range_fft_len, rx_ant_id, rbin_idx);
		noise_power += noise_power_tmp;
		if (noise_max_pwr < noise_power_tmp) {
			noise_max_pwr = noise_power_tmp;
		}
		if (noise_min_pwr > noise_power_tmp) {
			noise_min_pwr = noise_power_tmp;
		}
	}

	noise_power = noise_power / (g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID] - g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID] + 1);
	noise_avge_pwr = noise_power;
	report->noise_avge_db = (fast_db_trans_f32(noise_avge_pwr)) * 0.0625f;
	report->noise_avge_db = report->noise_avge_db * 100; // uint: 0.01dB

	report->noise_low_db = (fast_db_trans_f32(noise_min_pwr)) * 0.0625f;	
	report->noise_low_db = report->noise_low_db * 100; // uint: 0.01dB

	report->noise_high_db = (fast_db_trans_f32(noise_max_pwr)) * 0.0625f;	
	report->noise_high_db = report->noise_high_db * 100; // uint: 0.01dB

	/*max peak snr verify*/
	report->peak_pwr_db = (fast_db_trans_f32(mradar_est_data->noise_param.rbin_max_pwr[rx_ant_id])) * 0.0625f;	
	report->peak_pwr_db = report->peak_pwr_db * 100; // uint: 0.01dB

	report->snr_db = report->peak_pwr_db - noise_avge_snr_db;

	if (ABS(report->snr_db - pfactor->peak_snr_avge) > massess_gap.snr) {
		noise_est_pass = false;
	}

	/* max peak power verify */
	uint16_t resol_mm = mradar_est_data->noise_param.resol_mm;
	report->peak_distance = mradar_est_data->noise_param.rbin_max_pwr_idx[rx_ant_id] * resol_mm;

	if (report->peak_distance > mradar_est_data->usr_param.range_max_mm
		|| report->peak_distance < mradar_est_data->usr_param.range_min_mm
		|| ABS(pfactor->peak_pwr_avge - report->peak_pwr_db) > massess_gap.peak_pwr) {
		noise_est_pass = false;
	} 
							  
	FT_LOG_PRINT("ant [%d], distance %dmm, high avg: %f, low avg: %f, noise avg:%f, peak pwr avg:%f, peak snr avg: %f\n", 
		rx_ant_id, report->peak_distance, report->noise_high_db, report->noise_low_db, report->noise_avge_db, report->peak_pwr_db, report->snr_db);

	/* noise aver/max/min verify */
	if (ABS(report->noise_avge_db - pfactor->pwr_avge) > massess_gap.avge
		|| ABS(report->noise_low_db - pfactor->pwr_low_avge) > massess_gap.min
		|| ABS(report->noise_high_db - pfactor->pwr_high_avge) > massess_gap.max) {
		noise_est_pass = false;
	} 
	
	*avge_noise = report->noise_avge_db;
	report->rx_id = rx_ant_id;
	report->noise_start_index = g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_START_ID];
	report->noise_end_index = g_rbin_noise_est_param.rbin_scop[RBIN_SCOPE_OF_END_ID];
	if (noise_est_pass == true) {
		report->fail = 0;
	} else {
		report->fail = 1;
	}

	mmw_noise_est_report_data(MMW_NOISE_RX_ANT_REPORT_EVT, report, noise_est_report);

	return noise_est_pass;
}


int mmw_wave_noise_est_frame_config(struct mmw_noise_est_frame_t *pFrame)
{
	int ret = 0;

	if (pFrame == NULL) {
		return -1;
	}

	do {
		/* config for mmw chirp */
		ret = mmw_mode_cfg (pFrame->trx_mimo, pFrame->work_mode);
		if (ret) {
			LOG_PRINT("mmw_mode_cfg error\n");
			break;
		}

		ret = mmw_freq_cfg (pFrame->startFreq_MHz, pFrame->endFreq_MHz);
		if (ret) {
			LOG_PRINT("mmw_freq_cfg error\n");
			break;
		}

		ret = mmw_range_cfg(pFrame->range_mm, pFrame->resol_mm);
		if (ret) {
			LOG_PRINT("mmw_range_cfg error\n");
			break;
		}

		ret = mmw_velocity_cfg(pFrame->velocity_mm, pFrame->veloc_resol);
		if (ret) {
			LOG_PRINT("mmw_velocity_cfg error\n");
			break;
		}

		ret = mmw_chirp_num_cfg(pFrame->chirp_num);
		if (ret) {
			LOG_PRINT("mmw_chirp_num_cfg error\n");
			break;
		}

		ret = mmw_clutter_remove(pFrame->clutter_mode);
		if (ret) {
			LOG_PRINT("mmw_clutter_remove error\n");
			break;
		}
	
		ret = mmw_frame_cfg(pFrame->frame_period_ms,	pFrame->frame_num);
		if (ret) {
			LOG_PRINT("mmw_frame_cfg error: %d\n", ret);
			break;
		}
	
		FT_LOG_PRINT("mimo: %d, work_mode: %d, start_freq: %d, end_freq: %d, range_mm: %d, resol_mm: %d, vel_mm: %d, vel_res: %d, frame_period: %d, frame_num: %d, clutter_mode: %d, chirp_num:%d\r\n",
						pFrame->trx_mimo, pFrame->work_mode, pFrame->startFreq_MHz, pFrame->endFreq_MHz, pFrame->range_mm, pFrame->resol_mm, pFrame->velocity_mm, pFrame->veloc_resol,
						pFrame->frame_period_ms, pFrame->frame_num, pFrame->clutter_mode, pFrame->chirp_num);
	
		/* Enable autogain */
		mmw_fft_autogain_set(1);
	}while (0);
	
	return ret;
}

/** @brief noise estmation start
 *  1.config mmw wave
 *  2.init buffer struct
 *  3.config mmw callback
 *  4.creat semaphore
 *  5.mmw wave start
 *  @return return 0 on sucess
 * */
int mmw_noise_est_start(struct mmw_noise_est_frame_t *pFrame, struct radar_noise_est_t *mradar_est_data)
{
	int ret = 0;
	uint8_t int_type;	
	
	/* config mmw wave */
	ret = mmw_wave_noise_est_frame_config(pFrame);
	if (ret) {
		return -1;
	}
	/* init buffer struct */
	ret = ft_noise_est_init(pFrame);
	if (ret) {
		return -1;
	}
	
	/* config mmw callback */
	if (pFrame->work_mode == MMW_WORK_MODE_2DFFT) {
		int_type = MMW_DATA_TYPE_2DFFT;
	} else {
		int_type = MMW_DATA_TYPE_1DFFT;
	}
	ret = mmw_ctrl_callback_cfg(mmw_ctrl_noise_est_frame_cb, int_type, NULL);
	if (ret) {
		mmw_ctrl_callback_cfg(NULL, MMW_DATA_TYPE_DISABLE, NULL);
	}
	
	/* mmw wave start */
	ret = mmw_ctrl_start();
	if (ret) {
		return -1;
	}
	return ret;
}

struct mmw_noise_est_final_report_t** get_noise_est_report_result(void) 
{
	return &noise_est_report;
}

/********************************** max difference detect process function **********************************/

uint8_t g_max_diff_detect_pass_flag = 1;
uint16_t g_report_range_num = 0;

static struct mmw_max_diff_detect_param {
	uint16_t range_fft_len;
	uint16_t dop_fft_len;
	uint8_t report_type;
	uint16_t report_range_num;
	float diff_pwr_db_gap;
	uint16_t rbin_scop[2];			/* this scope use for find max/min value */
	uint16_t rbin_scop_snr[2];		/* this scope use for calculate snr */
	uint16_t rbin_scop_diff[2];		/* this scope use for dc-dc interference detect */

} g_rbin_max_diff_detect_param = {
	.range_fft_len = 0,
	.dop_fft_len = 0,
	.report_range_num = 0,
	.diff_pwr_db_gap = 0,
	.rbin_scop = {
		[0] = 0,
		[1] = 0
	},
	.rbin_scop_snr = {
		[0] = 0,
		[1] = 0
	},
	.rbin_scop_diff ={
		[0] = 0,
		[1] = 0
	}
};


struct radar_max_diff_detect_t *diff_detect_data = 0;

/* this buffer is used to storage max diff value for each range_idx.
 * it is malloc in ft_max_diff_detect_init(), and free when get_diff_value function is excuted or when quit ft-mode */
struct mmw_max_diff_detect_result_t *g_diff_detect_result = 0;

struct mmw_max_diff_detect_report_t *max_diff_detect_report = 0;

struct mmw_max_diff_detect_report_t** get_max_diff_detect_report_param(void) {
	return &max_diff_detect_report;
}

struct mmw_max_diff_detect_result_t **get_diff_detect_result(void) {
	return &g_diff_detect_result;
}

struct radar_max_diff_detect_t** get_radar_max_diff_detect_param(void){
	return &diff_detect_data;
}

struct mmw_max_diff_detect_param* get_detect_param(void)
{
	return &g_rbin_max_diff_detect_param;
}

/** @brief set scope of noise est range index */
int mmw_max_diff_detect_scope_set(uint16_t start_index, uint16_t end_index)
{
	if ((start_index > end_index) || (end_index >= g_rbin_max_diff_detect_param.range_fft_len)) {
	}
	g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] 	= start_index;
	g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] 	= end_index;
	return 0;
}

/** @brief set scope of noise est range index */
void mmw_max_diff_detect_gap_set(int16_t gap)
{
	if (gap == 0) {
		gap = FT_NOISE_EST_DIFF_PWR_GAP;
	} else {
		g_rbin_max_diff_detect_param.diff_pwr_db_gap = gap * 0.01f;	// gap uint is 0.01db, trans 0.01db to db
	}
}

/** @brief init param before max diff detect process
 * 1. init max diff detect param 
 * 2. memlloc buffer use for storage data which in noise est processing
 * */
int ft_max_diff_detect_init(struct mmw_noise_est_frame_t *pFrame)
{
	int ret = 0;
	uint16_t rbin_num;
	float *multi_speed_dim_buf = 0;
	float *dbf_power_buffer = 0;
	
	if (ft_task_status_obj.running == true) {
		ret = -1;
		return ret;
	}
	
	
	if (report == NULL) {
		mmw_process_mem_alloc((void **)&report, sizeof(*report));
	}
	if (max_diff_detect_report == NULL) {
		mmw_process_mem_alloc((void **)&max_diff_detect_report, sizeof(*max_diff_detect_report));
	}
	
	/* init g_rbin_noise_est_param struct */
	uint16_t range_fft_len = (pFrame->range_mm + pFrame->resol_mm - 1) / pFrame->resol_mm;
	range_fft_len = round_2power(range_fft_len);
	g_rbin_max_diff_detect_param.range_fft_len = range_fft_len;
	
	uint16_t dop_fft_len = (NOISE_FT_DEFAULT_VELOC / NOISE_FT_DEFAULT_VEL_RES) * 2;
	dop_fft_len =  round_2power(dop_fft_len);
	g_rbin_max_diff_detect_param.dop_fft_len = dop_fft_len;

	
	/* if user not set scope of range index, use default param as below:
	 * 		start range index: 	range_fft_len*3/10
	 * 		end range index:	(range_fft_len*9 + 9)/10 - 1
	 * else 
	 * 		check the scope is valid.
	*/
	if (g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] == 0
		&& g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] == 0) {
		g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] = range_fft_len*3/10;
		g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] = (range_fft_len*9 + 9)/10 - 1;
	} else {
		if (g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] >= range_fft_len) {
			g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] = range_fft_len -1;
		}
		if (g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] >= range_fft_len) {
			g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] = range_fft_len -1;
		}
	}

	/* set scope of range index use for calculate snr */
	if (g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] == 0
		&& g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] == 0) {
		g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] = range_fft_len*7/10;
		g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] = (range_fft_len*9 + 9)/10 - 1;
	} else {
		if (g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] >= range_fft_len) {
			g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] = range_fft_len -1;
		}
		if (g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] >= range_fft_len) {
			g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] = range_fft_len -1;
		}
	}
	
	/* set scope of range index use for dx-dc interference detect */
	g_rbin_max_diff_detect_param.rbin_scop_diff[RBIN_SCOPE_OF_START_ID] = range_fft_len*1/10;
	g_rbin_max_diff_detect_param.rbin_scop_diff[RBIN_SCOPE_OF_END_ID] = (range_fft_len*9 + 9)/10 - 1;
	
	rbin_num = g_rbin_max_diff_detect_param.rbin_scop_diff[1] - g_rbin_max_diff_detect_param.rbin_scop_diff[0] + 1;

	FT_LOG_PRINT("range fft len:%d, dop fft len:%d, range start:%d, range end:%d\n", 
				g_rbin_max_diff_detect_param.range_fft_len, g_rbin_max_diff_detect_param.dop_fft_len,
				g_rbin_max_diff_detect_param.rbin_scop_diff[RBIN_SCOPE_OF_START_ID], 
				g_rbin_max_diff_detect_param.rbin_scop_diff[RBIN_SCOPE_OF_END_ID]);

	mmw_process_mem_alloc((void**)&multi_speed_dim_buf, FT_NOISE_EST_SPEED_CASE_NUM * CONFIG_MAX_MIMO_ANT * range_fft_len * sizeof(float));
	
	mmw_process_mem_alloc((void **)&dbf_power_buffer, sizeof(float) * FT_NOISE_EST_SPEED_CASE_NUM * range_fft_len * FT_NOISE_EST_DBF_CASE_NUM);	
	
	/* report param init and malloc */
	if (g_diff_detect_result == NULL) {
		mmw_process_mem_alloc((void **)&g_diff_detect_result, sizeof(*g_diff_detect_result) * rbin_num);
	}
	
	if (multi_speed_dim_buf == NULL)  {
		ret = -1;
		ft_max_diff_detect_deinit();
		return ret;
	}
	
	if (dbf_power_buffer == NULL) {
		ret = -1;
		ft_max_diff_detect_deinit();
		return ret;
	}
	
	if (g_diff_detect_result == NULL) {
		ret = -1;
		ft_max_diff_detect_deinit();
		return ret;
	}
	
	beamforming_vector_init();
	
	/* init param */
	for (uint8_t rx_idx = 0; rx_idx < CONFIG_MAX_MIMO_ANT; rx_idx++) {
		diff_detect_data->noise_param.rbin_max_pwr[rx_idx] = 0;
		diff_detect_data->noise_param.rbin_max_pwr_idx[rx_idx] = 0;
	}
	
	diff_detect_data->noise_param.frame_num = pFrame->frame_num;
	ft_task_status_obj.curr_frame_index = 0;
	ft_task_status_obj.running = true;	
	
	memset(multi_speed_dim_buf, 0, FT_NOISE_EST_SPEED_CASE_NUM * CONFIG_MAX_MIMO_ANT * range_fft_len * sizeof(float));
	diff_detect_data->multi_speed_dim_buf = multi_speed_dim_buf;
	
	memset(dbf_power_buffer, 0, sizeof(float) * FT_NOISE_EST_SPEED_CASE_NUM * range_fft_len * FT_NOISE_EST_DBF_CASE_NUM);
	diff_detect_data->dbf_param.dbf_power_buffer = dbf_power_buffer;
	memset(g_diff_detect_result, 0, sizeof(*g_diff_detect_result) * rbin_num);
	
	memset(report, 0, sizeof(*report));
	memset(max_diff_detect_report, 0, sizeof(*max_diff_detect_report));
	
	return ret;
}

void ft_max_diff_detect_deinit(void)
{
	if (diff_detect_data->multi_speed_dim_buf) {
		mmw_process_mem_free((void **)&diff_detect_data->multi_speed_dim_buf);
	}
	if (diff_detect_data->dbf_param.dbf_power_buffer) {
		mmw_process_mem_free((void **)&diff_detect_data->dbf_param.dbf_power_buffer);
	}
	
	if (diff_detect_data != NULL) {
		mmw_process_mem_free((void **)&diff_detect_data);
	}
	if (report != NULL) {
		mmw_process_mem_free((void **)&report);
	}
	if (max_diff_detect_report != NULL) {
		mmw_process_mem_free((void **)&max_diff_detect_report);
	}
	
	ft_task_status_obj.running = false;
	ft_task_status_obj.curr_frame_index = 0;
}

static int max_diff_detect_frame_check(void)
{
	uint8_t txrx, work;
	uint16_t range_len, doppler_len;
	uint32_t range_mm, resol_mm;

	mmw_fft_num_get(&range_len, &doppler_len);
	if (range_len != g_rbin_max_diff_detect_param.range_fft_len || doppler_len != g_rbin_max_diff_detect_param.dop_fft_len){
		LOG_PRINT("Err: mmw_fft_num_get for range_len %d doppler_len %d\n", range_len, doppler_len);
		return -1;
	}

	mmw_mode_get (&txrx, &work);
	if ((txrx != CONFIG_NOISE_EST_MIMO_MODE) || (work != MMW_WORK_MODE_2DFFT)) {
		LOG_PRINT("Err: mmw_mode_get for txrx %d work %d\n", txrx, work);
		return -1;
	}

	if (CONFIG_NOISE_EST_MIMO_MODE == MMW_MIMO_2T4R) {
			diff_detect_data->noise_param.tx_num = 2;
			diff_detect_data->noise_param.rx_num = 4;
	} else if (CONFIG_NOISE_EST_MIMO_MODE == MMW_MIMO_1T3R) {
			diff_detect_data->noise_param.tx_num = 1;
			diff_detect_data->noise_param.rx_num = 3;
	} else {
			return -1;
	}

	mmw_range_get(&range_mm, &resol_mm);
	if (resol_mm) {
		diff_detect_data->noise_param.range_index_min = diff_detect_data->usr_param.range_min_mm / resol_mm;
		diff_detect_data->noise_param.range_index_max = diff_detect_data->usr_param.range_max_mm / resol_mm;
		diff_detect_data->noise_param.range_index_max = MAX(diff_detect_data->noise_param.range_index_max, (CONFIG_MIN_SCOPE_RANGE_MM/resol_mm));
		diff_detect_data->noise_param.range_index_dc = MAX(1, CONFIG_FILTER_DC_RANGE_MM / resol_mm);
	} else {
		diff_detect_data->noise_param.range_index_min = 0;
		diff_detect_data->noise_param.range_index_max = 0;
		diff_detect_data->noise_param.range_index_dc = 1;
	}

	if (range_len) {
		diff_detect_data->noise_param.resol_mm = range_mm / range_len;
	} else {
		diff_detect_data->noise_param.resol_mm = resol_mm;
	}

	if (range_len == g_rbin_max_diff_detect_param.range_fft_len) {
		diff_detect_data->noise_param.range_len = range_len;
	} else {
		diff_detect_data->noise_param.range_len = g_rbin_max_diff_detect_param.range_fft_len;
	}
	diff_detect_data->noise_param.doppler_len = doppler_len;
	return 0;
}



/** @brief mmw wave callback
 * main noise estmation process function.
 * 1、check mmw frame is valid.
 * 2、get all dop data for each rbin .
 * 3、restore auto gain.
 * 4、calculate the sum of all low speed dop bin power.
 * 5、storage the sum value in mradar_est_data.noise_est_buf.
 * 6、get 0 dop data, then find max power and max power index in 0 dop for each ant.
 * */
static int mmw_ctrl_ft_max_diff_detect_frame_cb(void *mmw_data, void *arg) {
	
	FT_LOG_PRINT("ongoing collecting noise frame count %d\n", ft_task_status_obj.curr_frame_index);
	int ret = 0;
	MmwPsicMimoRxNum_t mimo_rx_info;
	
	if (diff_detect_data->multi_speed_dim_buf == NULL) {
		LOG_PRINT("diff_detect_data->multi_speed_dim_buf == NULL");
		return -1;
	}

	ret = max_diff_detect_frame_check();
	if (ret < 0) {
		LOG_PRINT("err: frame param check error\n");
		return -1;
	}
	
	uint8_t ant_mimo_idx = 0;
	uint16_t dop_len = diff_detect_data->noise_param.doppler_len;
	uint16_t range_len = diff_detect_data->noise_param.range_len;
	
	complex16_cube *noise_dop_buf = 0;
	Complexf32_RealImag *noise_dop_buffer_f32 = 0;
	complex16_cube *noise_range_buf = 0;
	Complexf32_RealImag *noise_range_buffer_f32 = 0;
	
	uint16_t rbin_num = diff_detect_data->noise_param.range_index_max + 1;
	uint16_t rbin_off = diff_detect_data->noise_param.range_index_dc;
	ft_task_status_obj.curr_frame_index++;
	

	if (ft_task_status_obj.curr_frame_index <= CONFIG_MMW_VENDOR_FILTER_FRAME_NUM) {
		return 0;
	}
	
	mmw_process_mem_alloc((void **)&noise_dop_buf, dop_len * sizeof(complex16_cube));
	mmw_process_mem_alloc((void **)&noise_dop_buffer_f32, dop_len * sizeof(Complexf32_RealImag));
	mmw_process_mem_alloc((void **)&noise_range_buf, rbin_num * sizeof(complex16_cube));
	mmw_process_mem_alloc((void **)&noise_range_buffer_f32, rbin_num * sizeof(Complexf32_RealImag));


	if (noise_dop_buf == NULL || noise_dop_buffer_f32 == NULL || noise_range_buf == NULL || noise_range_buffer_f32 == NULL) {
		return -1;
	}
	mmw_motion_cube_access_open();
	
	mmw_psic_lib_sdk_get_tx_rx_num(&mimo_rx_info);
	Complexf32_RealImag ptr_ant_data[CONFIG_MAX_MIMO_ANT];
	Complexf32_RealImag *dbf_result_buf = 0;
	
	mmw_process_mem_alloc((void **)&dbf_result_buf, sizeof(Complexf32_RealImag) * dop_len);

	/* dbf noise est process */
	for (uint16_t dbf_case_idx = 0; dbf_case_idx < FT_NOISE_EST_DBF_CASE_NUM; dbf_case_idx++) {
		for (uint16_t range_idx = 0; range_idx < range_len; range_idx++) {
			memset(&dbf_result_buf[0], 0, sizeof(Complexf32_RealImag) * dop_len);
			for (uint16_t dop_idx = 0; dop_idx < dop_len; dop_idx++) {
				mmw_process_fft_ant_f32(ptr_ant_data, &mimo_rx_info, dop_idx, range_idx);
				ft_beamforming_2d(ptr_ant_data, &dbf_result_buf[dop_idx], range_idx, dop_idx, dbf_case_idx);
			}
			uint32_t noise_est_offest = dbf_case_idx * range_len * FT_NOISE_EST_SPEED_CASE_NUM + range_idx * FT_NOISE_EST_SPEED_CASE_NUM;
			float *result_power_buffer = &diff_detect_data->dbf_param.dbf_power_buffer[noise_est_offest];
			ft_multidim_noise_est_buf_update(dbf_result_buf, result_power_buffer, dop_len, dop_len >> 1);
			
		}
	}
	mmw_process_mem_free((void **)&dbf_result_buf);

	/* multi speed dim noise est  */
	for (uint8_t tx_idx = 0; tx_idx < diff_detect_data->noise_param.tx_num; tx_idx++) {
		for (uint8_t rx_idx = 0; rx_idx < diff_detect_data->noise_param.rx_num; rx_idx++) {
			ant_mimo_idx = tx_idx * diff_detect_data->noise_param.rx_num + rx_idx;
			if (!RX_ANT_VAILD(rx_idx)) {
				continue;
			}
			/* 1、get all dop data for each rbin 
			 * 2、restore auto gain
			 * 3、calculate sum of all high speed dop bin power
			 * 4、storage the sum in mradar_est_data.noise_est_buf
			 * */
			for (uint16_t range_idx = 0; range_idx < range_len; range_idx++) {
				mmw_fft_doppler(noise_dop_buf, dop_len, tx_idx, rx_idx, range_idx);
				fft_autogain_restore_f32(noise_dop_buf, noise_dop_buffer_f32, tx_idx, range_idx, dop_len);
				
				uint32_t offest = ant_mimo_idx * range_len * FT_NOISE_EST_DBF_CASE_NUM + range_idx * FT_NOISE_EST_DBF_CASE_NUM;
				float *ptr_result_buffer = &diff_detect_data->multi_speed_dim_buf[offest];
				ft_multidim_noise_est_buf_update(noise_dop_buffer_f32, ptr_result_buffer, dop_len, dop_len >> 1);
			}
			
			/* get 0 dop data */
			mmw_process_fft_range_f32(noise_range_buffer_f32, noise_range_buf, rbin_num, tx_idx, rx_idx, dop_len >> 1);
		
			/* find max power and max power index in 0 dop for each ant.
			 * if max power index is different between each frame, print a waring message */
			noise_rbin_buf_update(noise_range_buffer_f32, ant_mimo_idx, rbin_num, rbin_off, &diff_detect_data->noise_param);
		}
	}

	mmw_fft_autogain_clear();	
	mmw_motion_cube_access_close();
	
	mmw_process_mem_free((void **)&noise_dop_buf);
	mmw_process_mem_free((void **)&noise_dop_buffer_f32);
	mmw_process_mem_free((void **)&noise_range_buf);
	mmw_process_mem_free((void **)&noise_range_buffer_f32);
	
	/* process finish */
	if(ft_task_status_obj.curr_frame_index >= g_noise_est_frame.frame_num){
		mmw_max_diff_detect_process_finish();
	}
	
	return 0;
	
}


/** @brief max diff detect start, max diff detect use the same mmw wave as noise estmation.
 *  1.config mmw wave
 *  2.init buffer struct
 *  3.config mmw callback
 *  4.creat semaphore
 *  5.mmw wave start
 *  @return return 0 on sucess
 * */
int mmw_max_diff_detect_start(struct mmw_noise_est_frame_t *pFrame)
{
	int ret = 0;
	uint8_t int_type;	
	
	/* config mmw wave */
	mmw_wave_noise_est_frame_config(pFrame);
	/* init buffer struct */
	ret = ft_max_diff_detect_init(pFrame);
	if (ret < 0) {
		return -1;
	}
	
	/* config mmw callback */
	if (pFrame->work_mode == MMW_WORK_MODE_2DFFT) {
		int_type = MMW_DATA_TYPE_2DFFT;
	} else {
		int_type = MMW_DATA_TYPE_1DFFT;
	}
	ret = mmw_ctrl_callback_cfg(mmw_ctrl_ft_max_diff_detect_frame_cb, int_type, NULL);
	if (ret) {
		mmw_ctrl_callback_cfg(NULL, MMW_DATA_TYPE_DISABLE, NULL);
	}
	
	/* mmw wave start */
	ret = mmw_ctrl_start();
	if (ret) {
		return -1;
	}
	return ret;
}


int factory_test_mmw_max_diff_detect_start(uint8_t *ft_cmd)
{
	int ret = 0;
	
	/* get receive HIF msg */
	uint16_t noise_est_mode = *((uint8_t*)ft_cmd) | (*((uint8_t*)ft_cmd + 1) <<8);
	uint8_t noise_est_mode_v1 = noise_est_mode & 0x01;
	uint8_t noise_est_mode_v2 = (noise_est_mode & 0x02) >> 1; 
//	uint8_t noise_est_mode_clutter_remove = noise_est_mode & 0x04; // not supported yet
	
	
	if (noise_est_mode_v1 && noise_est_mode_v2) {
		ret = -1;
		goto exit;
	}
	
	if (noise_est_mode_v1) {
		/* malloc param buffer */
		if (mradar_est_data == NULL) {
			mmw_process_mem_alloc((void **)&mradar_est_data, sizeof(*mradar_est_data));
		}
		/* get receive HIF msg */
		struct mmw_nosie_est_recive_param_t *nosie_est_recive_param = (struct mmw_nosie_est_recive_param_t *)((uint8_t*)ft_cmd + 3);
		/* set noise estmation param */
		ft_noise_est_param_set(nosie_est_recive_param);
		/* set typical value for each enabled ants */
		ft_noise_factor_set(nosie_est_recive_param->factor, mradar_est_data->factor_param);
		/* set detect range */
		ft_noise_est_range_set(NOISE_FT_DEFAULT_RANGE_MIN_MM, NOISE_FT_DEFAULT_RANGE_MAX_MM, &mradar_est_data->usr_param);			/* min 0.9m, max 1.4m */
		/* set frame period */
		g_noise_est_frame.frame_period_ms = NOISE_FT_DEFAULT_FRAME_PERIOD;
		/* set detect scope */
		mmw_noise_meas_scope_set(nosie_est_recive_param->est_rbin_start_id, nosie_est_recive_param->est_rbin_end_id);
		/* start noise estimation */
		ret = mmw_noise_est_start(&g_noise_est_frame, mradar_est_data);

	} else if (noise_est_mode_v2) {
		/* malloc param buffer */
		if (diff_detect_data == NULL) {
			mmw_process_mem_alloc((void **)&diff_detect_data, sizeof(*diff_detect_data));
		}
		/* get receive HIF msg */
		struct mmw_nosie_est_recive_param_t *nosie_est_recive_param = (struct mmw_nosie_est_recive_param_t *)((uint8_t*)ft_cmd + 3);
		/* set noise estmation param */
		ft_noise_est_param_set(nosie_est_recive_param);
		/* set typical value for each enabled ants */
		ft_noise_factor_set(nosie_est_recive_param->factor, diff_detect_data->factor_param);
		/* set detect range */
		ft_noise_est_range_set(NOISE_FT_DEFAULT_RANGE_MIN_MM, NOISE_FT_DEFAULT_RANGE_MAX_MM, &diff_detect_data->usr_param);			/* min 0.9m, max 1.4m */
		/* set frame period */
		g_noise_est_frame.frame_period_ms = FT_MAX_DIFF_DETECT_DEFAULT_FRAME_PERIOD;
		/* set detect scope */
		mmw_max_diff_detect_scope_set(nosie_est_recive_param->est_rbin_start_id, nosie_est_recive_param->est_rbin_end_id);
		/* set diff pwr db gap */
		int16_t gap = *((uint8_t*)ft_cmd + 3 + sizeof(struct mmw_nosie_est_recive_param_t)) | (*((uint8_t*)ft_cmd + 3 + sizeof(struct mmw_nosie_est_recive_param_t) + 1) << 8);
		mmw_max_diff_detect_gap_set(gap);
		/* start noise estimation */
		ret = mmw_max_diff_detect_start(&g_noise_est_frame);
	}
	
exit:
	return ret;
}


/** @brief caculate noise min/max/avge value and snr.
 * compare the diff of result and typical value with gap value
 * */
static bool mmw_mulidim_noise_est_pass(uint16_t rx_ant_id, float *avge_noise, struct mmw_noise_report_t *report)
{
	struct mmw_noise_factor_t *pfactor;
	uint16_t range_len = diff_detect_data->noise_param.range_len;

	/* initial nosise est param */
	bool noise_est_pass = true;
	float noise_avge_snr_db = 0.f;
	float noise_avge_snr_pwr = 0;
	float noise_avge_pwr = 0;
	float noise_max_pwr = 0;
	float noise_min_pwr = INFINITY;

	/* temp variables */
	float noise_power = 0.f;

	if (diff_detect_data->multi_speed_dim_buf == NULL || rx_ant_id >= CONFIG_MAX_MIMO_ANT) {
		return false;
	}

	pfactor = &diff_detect_data->factor_param[rx_ant_id];

	for (uint16_t rbin_idx = g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID]; rbin_idx <= g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID]; rbin_idx++) {
		uint32_t offest = rx_ant_id * range_len * FT_NOISE_EST_SPEED_CASE_NUM + rbin_idx * FT_NOISE_EST_SPEED_CASE_NUM + 3;
		noise_power += diff_detect_data->multi_speed_dim_buf[offest];
	}
	noise_power = noise_power / (g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_END_ID] - g_rbin_max_diff_detect_param.rbin_scop_snr[RBIN_SCOPE_OF_START_ID] + 1);
	noise_avge_snr_pwr = noise_power;
	noise_avge_snr_db = (fast_db_trans_f32(noise_avge_snr_pwr)) * 0.0625f;	
	noise_avge_snr_db = noise_avge_snr_db * 100; // uint: 0.01dB

	/* init all */
	noise_power = 0.f;
	float noise_power_tmp = 0.f;

	for (uint16_t rbin_idx = g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID]; rbin_idx <= g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID]; rbin_idx++) {
		uint32_t offest = rx_ant_id * range_len * FT_NOISE_EST_SPEED_CASE_NUM + rbin_idx * FT_NOISE_EST_SPEED_CASE_NUM + 3;
		noise_power_tmp = diff_detect_data->multi_speed_dim_buf[offest];
		noise_power += noise_power_tmp;
		if (noise_max_pwr < noise_power_tmp) {
			noise_max_pwr = noise_power_tmp;
		}
		if (noise_min_pwr > noise_power_tmp) {
			noise_min_pwr = noise_power_tmp;
		}
	}

	noise_power = noise_power / (g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] - g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] + 1);
	noise_avge_pwr = noise_power;
	report->noise_avge_db = (fast_db_trans_f32(noise_avge_pwr)) * 0.0625f;
	report->noise_avge_db = report->noise_avge_db * 100; // uint: 0.01dB

	report->noise_low_db = (fast_db_trans_f32(noise_min_pwr)) * 0.0625f;	
	report->noise_low_db = report->noise_low_db * 100; // uint: 0.01dB

	report->noise_high_db = (fast_db_trans_f32(noise_max_pwr)) * 0.0625f;	
	report->noise_high_db = report->noise_high_db * 100; // uint: 0.01dB

	/*max peak snr verify*/
	report->peak_pwr_db = (fast_db_trans_f32(diff_detect_data->noise_param.rbin_max_pwr[rx_ant_id])) * 0.0625f;	
	report->peak_pwr_db = report->peak_pwr_db * 100; // uint: 0.01dB

	report->snr_db = report->peak_pwr_db - noise_avge_snr_db;

	if (ABS(report->snr_db - pfactor->peak_snr_avge) > massess_gap.snr) {
		noise_est_pass = false;
	}

	/* max peak power verify */
	uint16_t resol_mm = diff_detect_data->noise_param.resol_mm;
	report->peak_distance = diff_detect_data->noise_param.rbin_max_pwr_idx[rx_ant_id] * resol_mm;

	if (report->peak_distance > diff_detect_data->usr_param.range_max_mm
		|| report->peak_distance < diff_detect_data->usr_param.range_min_mm
		|| ABS(pfactor->peak_pwr_avge - report->peak_pwr_db) > massess_gap.peak_pwr) {
		noise_est_pass = false;
	} 
							  
	FT_LOG_PRINT("ant [%d], distance %dmm, high avg: %f, low avg: %f, noise avg:%f, peak pwr avg:%f, peak snr avg: %f\n", 
		rx_ant_id, report->peak_distance, report->noise_high_db, report->noise_low_db, report->noise_avge_db, report->peak_pwr_db, report->snr_db);

	/* noise aver/max/min verify */
	if (ABS(report->noise_avge_db - pfactor->pwr_avge) > massess_gap.avge
		|| ABS(report->noise_low_db - pfactor->pwr_low_avge) > massess_gap.min
		|| ABS(report->noise_high_db - pfactor->pwr_high_avge) > massess_gap.max) {
		noise_est_pass = false;
	} 
	
	*avge_noise = report->noise_avge_db;
	report->rx_id = rx_ant_id;
	report->noise_start_index = g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID];
	report->noise_end_index = g_rbin_max_diff_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID];
	if (noise_est_pass == true) {
		report->fail = 0;
	} else {
		report->fail = 1;
	}

	mmw_noise_est_report_data(MMW_NOISE_RX_ANT_REPORT_EVT, report, &max_diff_detect_report->noise_est_report);

	return noise_est_pass;
}



/** @brief calculate noise and set result in struct mmw_noise_report_t  */
void mmw_max_diff_detect_process_finish(void)
{
	uint32_t frame_num = ft_task_status_obj.curr_frame_index - CONFIG_MMW_VENDOR_FILTER_FRAME_NUM;
	float *rbin_max_pwr = &diff_detect_data->noise_param.rbin_max_pwr[0];
	float noise_db = 0.f, all_noise_db = 0.f;
	uint8_t tx_num = diff_detect_data->noise_param.tx_num;
	uint8_t rx_num = diff_detect_data->noise_param.rx_num;
	
	mmw_ctrl_stop();
	ft_task_status_obj.running = false;
	ft_task_status_obj.curr_frame_index = 0;
	
	uint16_t dbf_diff_step_buf[FT_NOISE_EST_DBF_DIFF_STEP_NUM] = {2, 3};
	uint16_t diff_step = 2;
	uint32_t rbin_scop_start = g_rbin_max_diff_detect_param.rbin_scop_diff[RBIN_SCOPE_OF_START_ID];
	uint32_t rbin_scop_end = g_rbin_max_diff_detect_param.rbin_scop_diff[RBIN_SCOPE_OF_END_ID];
	
	float diff_value_max = 0.f;
	uint16_t range_idx_max = 0;
	
	bool pass = false;
	
	float *rbin_power_db = 0;
	bool dbf_pass = false;

	uint16_t range_len = diff_detect_data->noise_param.range_len;

	if (diff_detect_data->multi_speed_dim_buf == NULL) {
		return;
	}
	
	/* ************ dbf ************ */
	mmw_process_mem_alloc((void **)&rbin_power_db, sizeof(float) * range_len * FT_NOISE_EST_SPEED_CASE_NUM);
	if (rbin_power_db == NULL) {
		return;
	}
	
	/* loop the range_idx */
	for (uint16_t range_idx = rbin_scop_start; range_idx < rbin_scop_end - diff_step; range_idx++) {
		g_diff_detect_result[range_idx - rbin_scop_start].range_idx = range_idx + diff_step;
	}
	
	for (int dbf_case_idx = 0; dbf_case_idx < FT_NOISE_EST_DBF_CASE_NUM; dbf_case_idx++) {
		uint32_t offest = dbf_case_idx * range_len * FT_NOISE_EST_SPEED_CASE_NUM;

		for (int i = 0; i < range_len * FT_NOISE_EST_SPEED_CASE_NUM; i++) {
			diff_detect_data->dbf_param.dbf_power_buffer[offest + i] /= (( diff_detect_data->noise_param.doppler_len >> 1) * frame_num);
			rbin_power_db[i] = (fast_db_trans_f32( diff_detect_data->dbf_param.dbf_power_buffer[offest + i])) * 0.0625f;
		}
		
		dbf_pass = ft_multidim_pass_verify(rbin_power_db, &dbf_diff_step_buf[0], FT_NOISE_EST_DBF_DIFF_STEP_NUM,
						FT_NOISE_EST_DBF_DIFF_PWR_GAP, rbin_scop_start, rbin_scop_end,
						&diff_value_max, &range_idx_max);
						

		if (ABS(diff_value_max * 100) > ABS(max_diff_detect_report->diff_value_report.diff_value)) {
			max_diff_detect_report->diff_value_report.range_idx = range_idx_max;
			max_diff_detect_report->diff_value_report.diff_value = (int16_t)(diff_value_max * 100); // uint: 0.01dB
		}				
		
		g_max_diff_detect_pass_flag &= dbf_pass;
	}
	/* ************ dbf end ************ */
	memset(rbin_power_db, 0,   sizeof(float) * range_len * FT_NOISE_EST_SPEED_CASE_NUM);
	
	
	/* multim speed dim pass verify */
	for (int ant_idx = 0; ant_idx < CONFIG_MAX_MIMO_ANT; ant_idx++) {
		uint32_t offest = ant_idx * diff_detect_data->noise_param.range_len * FT_NOISE_EST_SPEED_CASE_NUM;
			
		for (int i = 0; i < diff_detect_data->noise_param.range_len * FT_NOISE_EST_SPEED_CASE_NUM; i++) {
			diff_detect_data->multi_speed_dim_buf[offest + i] /= (( diff_detect_data->noise_param.doppler_len >> 1) * frame_num);
			rbin_power_db[i] = (fast_db_trans_f32( diff_detect_data->multi_speed_dim_buf[offest + i])) * 0.0625f;
		}

		pass = ft_multidim_pass_verify(rbin_power_db, &diff_step, 1, 
					g_rbin_max_diff_detect_param.diff_pwr_db_gap, rbin_scop_start, rbin_scop_end,
					&diff_value_max, &range_idx_max);
			
		if (ABS(diff_value_max * 100) > ABS(max_diff_detect_report->diff_value_report.diff_value)) {
			max_diff_detect_report->diff_value_report.range_idx = range_idx_max;
			max_diff_detect_report->diff_value_report.diff_value = diff_value_max * 100; // uint: 0.01dB
		}
		
		g_max_diff_detect_pass_flag &= pass;
	}
	
	mmw_process_mem_free((void **)&rbin_power_db);
	
	/* noise est param cal */
	for (int rx_ant_idx = 0; rx_ant_idx < tx_num * rx_num; rx_ant_idx++) {
		
		rbin_max_pwr[rx_ant_idx] = rbin_max_pwr[rx_ant_idx] / frame_num;
		
		pass = mmw_mulidim_noise_est_pass(rx_ant_idx, &noise_db, report);
		
		if (pass == true) {
			FT_LOG_PRINT("mmw noise est on rx ant %d test pass\n", rx_ant_idx);
		} else {
			report->test_fail_mask |= BIT(rx_ant_idx);
			FT_LOG_PRINT("mmw noise est on rx ant %d test fail %x\n", rx_ant_idx, report->test_fail_mask);
		}
		all_noise_db += noise_db;
	}
	report->noise_avge_db = all_noise_db / (tx_num * rx_num);
	mmw_noise_est_report_data(MMW_NOISE_FINAL_REPORT_EVT, report, &max_diff_detect_report->noise_est_report);

	FT_RspSemRelease();
}


Complexf32_RealImag steer_vector_conj_lut[4];

void steer_vector_conj_lut_init(void){
	/* [0, 90, 30, -30] --> [1, -1, j, -j] */
	steer_vector_conj_lut[0].real = 1;
	steer_vector_conj_lut[0].imag = 0;
	steer_vector_conj_lut[1].real = -1;
	steer_vector_conj_lut[1].imag = 0;
	steer_vector_conj_lut[2].real = 0;
	steer_vector_conj_lut[2].imag = 1;
	steer_vector_conj_lut[3].real = 0;
	steer_vector_conj_lut[3].imag = -1;
}

void beamforming_vector_init(void)
{
	Complexf32_RealImag dbf_steer_vector[FT_NOISE_EST_DBF_CASE_NUM * CONFIG_MAX_MIMO_ANT];
	steer_vector_conj_lut_init();
	/* [0, 90, 30, -30] --> [1, -1, j, -j] */
	uint8_t steer_vector_conj_idx[FT_NOISE_EST_DBF_CASE_NUM][CONFIG_MAX_MIMO_ANT];
#if CONFIG_SOC_RS6130
	/* steer vector:
	 * 0:  [1,1,1]
	 * 90: [1,1,-1]
	 * 30: [1,1,1j]
	 * -30:[1,1,-1j]
	 * for convenient beamforming, take conjugate of steer vector:
	 * conjugate of steer vector:
	 * 0:  [1,1,1]
	 * 90: [1,1,-1]
	 * 30: [1,1,-1j]
	 * -30:[1,1,1j] 
	 * the index is: {0, 0, 0}, {0, 0, 1}, {0, 0, 3}, {0, 0, 2}
	 */
	steer_vector_conj_idx[0][0] = 0;
	steer_vector_conj_idx[0][1] = 0;
	steer_vector_conj_idx[0][2] = 0;
	
	steer_vector_conj_idx[1][0] = 0;
	steer_vector_conj_idx[1][1] = 0;
	steer_vector_conj_idx[1][2] = 1;
	
	steer_vector_conj_idx[2][0] = 0;
	steer_vector_conj_idx[2][1] = 0;
	steer_vector_conj_idx[2][2] = 3;
	
	steer_vector_conj_idx[3][0] = 0;
	steer_vector_conj_idx[3][1] = 0;
	steer_vector_conj_idx[3][2] = 2;
	
#elif CONFIG_SOC_RS6240
	/* steer vector:
	 * 0:  [1,1,1,1,1,1,1,1]
	 * 90: [1,-1,-1,-1,1,-1,-1,-1]
	 * 30: [1,-1j,-1j,-1j,-1,1j,1j,1j]
	 * -30:[1,1j,1j,1j,-1,-1j,-1j,-1j]
	 * for convenient beamforming, take conjugate of steer vector:
	 * conjugate of steer vector:
	 * 0:  [1,1,1,1,1,1,1,1]
	 * 90: [1,-1,-1,-1,1,-1,-1,-1]
	 * 30: [1,1j,1j,1j,-1,-1j,-1j,-1j]
	 * -30:[1,-1j,-1j,-1j,-1,1j,1j,1j]
	 * the index is: {0, 0, 0, 0, 0, 0, 0, 0}, {0, 1, 1, 1, 0, 1, 1, 1}, {0, 2, 2, 2, 1, 3, 3, 3}, {0, 3, 3, 3, 1, 2, 2, 2}
	 */
	steer_vector_conj_idx[0][0] = 0;
	steer_vector_conj_idx[0][1] = 0;
	steer_vector_conj_idx[0][2] = 0;
	steer_vector_conj_idx[0][3] = 0;
	steer_vector_conj_idx[0][4] = 0;
	steer_vector_conj_idx[0][5] = 0;
	steer_vector_conj_idx[0][6] = 0;
	steer_vector_conj_idx[0][7] = 0;

	steer_vector_conj_idx[1][0] = 0;
	steer_vector_conj_idx[1][1] = 1;
	steer_vector_conj_idx[1][2] = 1;
	steer_vector_conj_idx[1][3] = 1;
	steer_vector_conj_idx[1][4] = 0;
	steer_vector_conj_idx[1][5] = 1;
	steer_vector_conj_idx[1][6] = 1;
	steer_vector_conj_idx[1][7] = 1;
	
	steer_vector_conj_idx[2][0] = 0;
	steer_vector_conj_idx[2][1] = 2;
	steer_vector_conj_idx[2][2] = 2;
	steer_vector_conj_idx[2][3] = 2;
	steer_vector_conj_idx[2][4] = 1;
	steer_vector_conj_idx[2][5] = 3;
	steer_vector_conj_idx[2][6] = 3;
	steer_vector_conj_idx[2][7] = 3;
	
	steer_vector_conj_idx[3][0] = 0;
	steer_vector_conj_idx[3][1] = 3;
	steer_vector_conj_idx[3][2] = 3;
	steer_vector_conj_idx[3][3] = 3;
	steer_vector_conj_idx[3][4] = 1;
	steer_vector_conj_idx[3][5] = 2;
	steer_vector_conj_idx[3][6] = 2;
	steer_vector_conj_idx[3][7] = 2;
#elif CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUF || CONFIG_BOARD_MRS7241_P2840_CPUF || CONFIG_BOARD_MRS7241_P2828_CPUF
#else
#error "please select valid board!"
#endif

	for (int dbf_idx = 0; dbf_idx < FT_NOISE_EST_DBF_CASE_NUM; dbf_idx++) {
		for (int ant_idx = 0; ant_idx < CONFIG_MAX_MIMO_ANT; ant_idx++) {
			/* normalized */
			dbf_steer_vector[(dbf_idx * CONFIG_MAX_MIMO_ANT) + ant_idx].real = steer_vector_conj_lut[steer_vector_conj_idx[dbf_idx][ant_idx]].real / CONFIG_MAX_MIMO_ANT;
			dbf_steer_vector[(dbf_idx * CONFIG_MAX_MIMO_ANT) + ant_idx].imag = steer_vector_conj_lut[steer_vector_conj_idx[dbf_idx][ant_idx]].imag / CONFIG_MAX_MIMO_ANT;
		}
	}
	memcpy(diff_detect_data->dbf_param.dbf_weights_vector, dbf_steer_vector, sizeof(dbf_steer_vector));
#if CONFIG_FT_LOG_DEBUG
	for (int dbf_idx = 0; dbf_idx < FT_NOISE_EST_DBF_CASE_NUM; dbf_idx++) {
		for (int ant_idx = 0; ant_idx < CONFIG_MAX_MIMO_ANT; ant_idx++) {
			FT_LOG_PRINT("steer_vactor[%d]: %f + %fj\n", (dbf_idx * CONFIG_MAX_MIMO_ANT) + ant_idx,diff_detect_data->dbf_param.dbf_weights_vector[(dbf_idx * CONFIG_MAX_MIMO_ANT) + ant_idx].real, diff_detect_data->dbf_param.dbf_weights_vector[(dbf_idx * CONFIG_MAX_MIMO_ANT) + ant_idx].imag);
		}
	}
#endif
}

void ft_beamforming_2d(Complexf32_RealImag *ptr_ant_data, Complexf32_RealImag *dbf_result_buffer, uint16_t range_idx, uint16_t dop_idx, uint16_t dbf_case_idx)
{

	Complexf32_RealImag ptr_ant_aligned[CONFIG_MAX_MIMO_ANT];

	Complexf32_RealImag *dbf_weights = &diff_detect_data->dbf_param.dbf_weights_vector[dbf_case_idx * CONFIG_MAX_MIMO_ANT];

	if (!dbf_weights) {
		return;
	}

	/* DBF calculate, require datacube is aligned */
#if CONFIG_SOC_RS6130
		memcpy(ptr_ant_aligned, ptr_ant_data, sizeof(ptr_ant_aligned));
#elif CONFIG_SOC_RS6240
		mmw_process_ant_data_calibrate_f32(ptr_ant_data, ptr_ant_aligned, mmw_get_g_rx_ant_calib_data_const(), CONFIG_MAX_MIMO_ANT);
#elif CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUF || CONFIG_BOARD_MRS7241_P2840_CPUF || CONFIG_BOARD_MRS7241_P2828_CPUF
#else
		#error "not supported chip!"
#endif	
	
	/* DBF output: y = w * x 
	 * w is 1*3 row vector, x is 3*1 column vector, therefore, y is 1*1.
	 * (a + bj) * (c + dj) = (ac - db) + (ad + bc)j
	 * */
	for (int ant_idx = 0; ant_idx < CONFIG_MAX_MIMO_ANT; ant_idx++) {
		dbf_result_buffer->real += (dbf_weights[ant_idx].real * ptr_ant_aligned[ant_idx].real - dbf_weights[ant_idx].imag * ptr_ant_aligned[ant_idx].imag);
		dbf_result_buffer->imag += (dbf_weights[ant_idx].real * ptr_ant_aligned[ant_idx].imag + dbf_weights[ant_idx].imag * ptr_ant_aligned[ant_idx].real);
	}
}

void ft_multidim_noise_est_buf_update(Complexf32_RealImag *noise_buf, float *result_buffer, uint16_t dop_len, uint16_t static_dop_idx)
{
	/* eliminate the static dop bin and its two adjacent bins(left and right) */
	uint16_t use_dop_num = dop_len >> 1;
	uint16_t after_eliminate_dop_num = dop_len - 5;
	uint16_t first_part_len = static_dop_idx - 2;
	uint16_t second_part_start_idx = static_dop_idx + 3;
	uint16_t seconde_part_len = dop_len -  second_part_start_idx;
	
	float power_positive_speed = 0.f, power_negative_speed = 0.f, power_high_speed = 0.f, power_low_speed = 0.f;
	uint16_t positive_speed_idx, negative_speed_idx, pos_high_speed_idx, neg_high_speed_idx, pos_low_speed_idx, neg_low_speed_idx;
	
	Complexf32_RealImag *eliminate_static_dop_bin_buff = 0;
	
	mmw_process_mem_alloc((void **)&eliminate_static_dop_bin_buff, sizeof(Complexf32_RealImag) * after_eliminate_dop_num);
	if (eliminate_static_dop_bin_buff == NULL) {
		return;
	}
	
	/* first part is dop_idx from 0 to static_dop_idx - 2 */
	memcpy(eliminate_static_dop_bin_buff, noise_buf, first_part_len * sizeof(Complexf32_RealImag));
	/* second part is dop_idx from static_dop_idx + 3 to end */
	memcpy(eliminate_static_dop_bin_buff + first_part_len, noise_buf + second_part_start_idx, seconde_part_len * sizeof(Complexf32_RealImag));
			
	/* init start index */
	positive_speed_idx = after_eliminate_dop_num - 1;
	negative_speed_idx = 0;
	pos_high_speed_idx = after_eliminate_dop_num - 1;
	neg_high_speed_idx = 0;
	neg_low_speed_idx = static_dop_idx - 3;
	pos_low_speed_idx = neg_low_speed_idx + 1;
	
	power_high_speed = 0.f;
	power_low_speed = 0.f;
	power_positive_speed = 0.f;
	power_negative_speed = 0.f;
	
	/* calculate the power of negative/posiive speed in current range_idx */
	for (int idx = 0; idx < use_dop_num; idx++) {
		power_negative_speed += POWER_CAL(eliminate_static_dop_bin_buff[negative_speed_idx].real, eliminate_static_dop_bin_buff[negative_speed_idx].imag);
		power_positive_speed += POWER_CAL(eliminate_static_dop_bin_buff[positive_speed_idx].real, eliminate_static_dop_bin_buff[positive_speed_idx].imag);

		positive_speed_idx--;
		negative_speed_idx++;
	}
	/* calculate the power of high/low speed in current range_idx */
	for (int idx = 0; idx < (use_dop_num >> 1); idx++) {
		power_high_speed += POWER_CAL(eliminate_static_dop_bin_buff[pos_high_speed_idx].real, eliminate_static_dop_bin_buff[pos_high_speed_idx].imag);
		power_high_speed += POWER_CAL(eliminate_static_dop_bin_buff[neg_high_speed_idx].real, eliminate_static_dop_bin_buff[neg_high_speed_idx].imag);
		
		power_low_speed += POWER_CAL(eliminate_static_dop_bin_buff[pos_low_speed_idx].real, eliminate_static_dop_bin_buff[pos_low_speed_idx].imag);
		power_low_speed += POWER_CAL(eliminate_static_dop_bin_buff[neg_low_speed_idx].real, eliminate_static_dop_bin_buff[neg_low_speed_idx].imag);

		neg_high_speed_idx++;
		pos_high_speed_idx--;
		neg_low_speed_idx--;
		pos_low_speed_idx++;
	}
	/* save noise est result in result_buffer */
	result_buffer[0] += power_negative_speed;
	result_buffer[1] += power_positive_speed;
	result_buffer[2] += power_low_speed;
	result_buffer[3] += power_high_speed;
	
	mmw_process_mem_free((void **)&eliminate_static_dop_bin_buff);
}


bool ft_multidim_pass_verify(float *rbin_power_db, uint16_t *diff_step_buf, uint8_t step_size, float gap_diff_pwr, 
								uint32_t rbin_scop_start, uint32_t rbin_scop_end, float *diff_value_max, uint16_t *range_idx_max)
{
	bool dbf_pass = true;
	uint8_t step = 0;
	float diff_value = 0.f, diff_value_max_tmp = 0.f;
	uint16_t range_idx_pwr_max = 0;
	uint16_t range_idx_pos = 0;

	for (int step_idx = 0; step_idx < step_size; step_idx++) {
		step = diff_step_buf[step_idx];

		for (int speed_dim_idx = 0; speed_dim_idx < FT_NOISE_EST_SPEED_CASE_NUM; speed_dim_idx++) {
			/* reset ptr position */
			g_rbin_max_diff_detect_param.report_range_num = 0;
			for (uint16_t range_idx = rbin_scop_start; range_idx < rbin_scop_end - step; range_idx++) {
				uint32_t offest = range_idx * FT_NOISE_EST_SPEED_CASE_NUM + speed_dim_idx;
				uint32_t step_offest = (range_idx + step) * FT_NOISE_EST_SPEED_CASE_NUM + speed_dim_idx;
				
				diff_value = rbin_power_db[step_offest] - rbin_power_db[offest];
				
				if (ABS(diff_value) > ABS(diff_value_max_tmp)) {
					diff_value_max_tmp = diff_value;
					range_idx_pwr_max = range_idx + step;
				}
				if(ABS(diff_value) > gap_diff_pwr){
					dbf_pass = false;
				}
				
				/* storege the max diff value for all range_idx, compare the same offest range_idx  */
				range_idx_pos = range_idx + (step - diff_step_buf[0]) - rbin_scop_start;
				if (ABS(diff_value * 100) > ABS(g_diff_detect_result[range_idx_pos].diff_value)) {
					g_diff_detect_result[range_idx_pos].diff_value = (int16_t)(diff_value * 100);
				}
				g_rbin_max_diff_detect_param.report_range_num++;
				
			}
		}
		g_report_range_num = g_rbin_max_diff_detect_param.report_range_num;
	}
	
	*diff_value_max = diff_value_max_tmp;
	*range_idx_max = range_idx_pwr_max;
	
	return dbf_pass;
}


void ft_multidim_diff_process(uint16_t rx_ant_id, float* noise_diff_db_max)
{
	float *ptr_rbin_power_db = 0;
	uint16_t range_len = mradar_est_data->noise_param.range_len;
	uint16_t range_start = g_rbin_max_diff_detect_param.rbin_scop_diff[0];
	uint16_t range_end = g_rbin_max_diff_detect_param.rbin_scop_diff[1];
	uint16_t diff_step = 3;
	float noise_diff_db_tmp = 0.f;
	
	mmw_process_mem_alloc((void **)&ptr_rbin_power_db, sizeof(float) * range_len * FT_NOISE_EST_SPEED_CASE_NUM);
	uint32_t ptr_offest = rx_ant_id * range_len * FT_NOISE_EST_SPEED_CASE_NUM;
	for (int i = 0; i < range_len * FT_NOISE_EST_SPEED_CASE_NUM; i++) {
		ptr_rbin_power_db[i] = fast_db_trans_f32(mradar_est_data->noise_est_buf[ptr_offest + i]) * 0.0625f;
		
	}
	
	for (int speed_dim_idx = 0; speed_dim_idx < FT_NOISE_EST_SPEED_CASE_NUM; speed_dim_idx++) {
		for (int range_idx = range_start; range_idx < range_end - diff_step; range_idx++) {	
			uint32_t offest = range_idx * FT_NOISE_EST_SPEED_CASE_NUM + speed_dim_idx;
			uint32_t step_offest = (range_idx + diff_step) * FT_NOISE_EST_SPEED_CASE_NUM + speed_dim_idx;

			noise_diff_db_tmp = ptr_rbin_power_db[step_offest] - ptr_rbin_power_db[offest];
			if (noise_diff_db_tmp > *noise_diff_db_max) {
				*noise_diff_db_max = noise_diff_db_tmp;
			}
		}
	}
	mmw_process_mem_free((void **)&ptr_rbin_power_db);
}

void ft_max_diff_value_report(void)
{
	/* report diff value */
	int retStatus;

	uint8_t *report_data = 0;

	uint32_t report_len_byte = sizeof(*g_diff_detect_result) * g_report_range_num + 3;
	mmw_process_mem_alloc((void **)&report_data, report_len_byte);
	report_data[0] = HIF_ERRCODE_SUCCESS;
	report_data[1] = g_report_range_num & 0xFF;
	report_data[2] = (g_report_range_num >> 8) & 0xFF;
	memcpy(&report_data[3], (uint8_t*)g_diff_detect_result, report_len_byte - 3);
	
	retStatus = HIF_MsgReport(HIF_MSG_ID_MAX_DIFF_DETECT, report_data, report_len_byte, NULL);
	if (retStatus != HIF_ERRCODE_SUCCESS) {
		LOG_PRINT("hif report fail %d\n", retStatus);
	}
	mmw_process_mem_free((void **)&report_data);
	if (g_diff_detect_result != NULL) {
		mmw_process_mem_free((void **)&g_diff_detect_result);
		g_diff_detect_result = NULL;
	}
}



/********************************** each-frequency bin detect process function **********************************/
static struct mmw_freq_bin_detect_usr_param {
	uint16_t range_fft_len;
	uint16_t detect_range_num;	/* the num of range_fft_len * 1 / 10 to (range_fft_len * 9 + 9)/10 - 1  */
	uint16_t rbin_scop[2];

} g_rbin_freq_bin_detect_param = {
	.range_fft_len = 0,
	.rbin_scop ={
		[0] = 0,
		[1] = 0
	}
};


struct mmw_freq_bin_detect_t freq_bin_detect_data;
struct mmw_freq_bin_detect_report_t freq_bin_report;

struct mmw_freq_bin_detect_t* get_freq_bin_detect_param(void){
	return &freq_bin_detect_data;
}

struct mmw_freq_bin_detect_report_t* get_freq_bin_report_param(void)
{
	return &freq_bin_report;
}

static int freq_bin_detect_frame_check(void)
{
	struct mmw_freq_bin_detect_t *pdata = get_freq_bin_detect_param();
	uint8_t txrx, work;
	uint16_t range_len, doppler_len;

	mmw_fft_num_get(&range_len, &doppler_len);
	if (range_len != g_rbin_freq_bin_detect_param.range_fft_len){
		LOG_PRINT("Err: mmw_fft_num_get for range_len %d doppler_len %d\n", range_len, doppler_len);
		return -1;
	}

	mmw_mode_get (&txrx, &work);
	if ((txrx != CONFIG_NOISE_EST_MIMO_MODE) || (work != MMW_WORK_MODE_2DFFT)) {
		LOG_PRINT("Err: mmw_mode_get for txrx %d work %d\n", txrx, work);
		return -1;
	}

	if (CONFIG_NOISE_EST_MIMO_MODE == MMW_MIMO_2T4R) {
			pdata->detect_param.tx_num = 2;
			pdata->detect_param.rx_num = 4;
	} else if (CONFIG_NOISE_EST_MIMO_MODE == MMW_MIMO_1T3R) {
			pdata->detect_param.tx_num = 1;
			pdata->detect_param.rx_num = 3;
	} else {
			return -1;
	}


	if (range_len == g_rbin_freq_bin_detect_param.range_fft_len) {
		pdata->detect_param.range_len = range_len;
	} else {
		pdata->detect_param.range_len = g_rbin_freq_bin_detect_param.range_fft_len;
	}
	pdata->detect_param.doppler_len = doppler_len;
	return 0;
}


void mmw_frq_bin_detect_process_finish(void)
{
	uint32_t frame_num = ft_task_status_obj.curr_frame_index - CONFIG_MMW_VENDOR_FILTER_FRAME_NUM;
	struct mmw_freq_bin_detect_t *pdata = get_freq_bin_detect_param();
	
	float *rbin_power; 
	uint8_t tx_num = pdata->detect_param.tx_num;
	uint8_t rx_num = pdata->detect_param.rx_num;
	uint16_t range_fft_len = pdata->detect_param.range_len;
	uint16_t range_start_idx = g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID];
	uint16_t range_end_idx = g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID];
	
	mmw_ctrl_stop();
	ft_task_status_obj.running = false;
	ft_task_status_obj.curr_frame_index = 0;
	
	if (pdata->noise_est_buf == NULL) {
		LOG_PRINT("noise_est_buf == NULL\n");
		return;
	}
	
	rbin_power = pdata->noise_est_buf;


	for (int rx_ant_idx = 0; rx_ant_idx < tx_num * rx_num; rx_ant_idx++) {
		for (int range_idx = 0; range_idx <range_fft_len; range_idx++) {
			uint32_t offest = rx_ant_idx * range_fft_len + range_idx;
			rbin_power[offest] = rbin_power[offest] / ((pdata->detect_param.doppler_len >> 1) * frame_num);
		}
	}

	uint16_t report_num_cnt = 0;
	freq_bin_report.range_detect_len = g_rbin_freq_bin_detect_param.detect_range_num;

	for (int rx_ant_idx = 0; rx_ant_idx < tx_num * rx_num; rx_ant_idx++) {
		for (uint16_t range_idx = range_start_idx; range_idx < range_end_idx; range_idx++) {
			uint32_t offset = rx_ant_idx * range_fft_len + range_idx;
			freq_bin_report.typical_value[report_num_cnt].range_idx = range_idx;
			freq_bin_report.typical_value[report_num_cnt].power = (int16_t)(fast_db_trans_f32(rbin_power[offset])* 0.0625f * 100);	//q4 to float, uint:0.01dB
			
			report_num_cnt++;
		}
	}

	FT_RspSemRelease();
}


/** @brief mmw wave callback
 * main noise estmation process function.
 * 1、check mmw frame is valid.
 * 2、get all dop data for each rbin .
 * 3、restore auto gain.
 * 4、calculate the sum of all high speed dop bin power.
 * 5、storage the sum value in mradar_est_data.noise_est_buf.
 * 6、get 0 dop data, then find max power and max power index in 0 dop for each ant.
 * */
static int mmw_ctrl_ft_freq_bin_detect_frame_cb(void *mmw_data, void *arg) {
	
	FT_LOG_PRINT("Radar callback process noise data, frame: %d\n", ft_task_status_obj.curr_frame_index);
	int ret = 0;
	float power = 0.f;
	struct mmw_freq_bin_detect_t *pdata = get_freq_bin_detect_param();

	ret = freq_bin_detect_frame_check();
	if (ret < 0) {
		LOG_PRINT("err: frame param check error\n");
		return -1;
	}

	uint8_t ant_mimo_id = 0;
	uint16_t dop_len = pdata->detect_param.doppler_len;
	uint16_t range_len = pdata->detect_param.range_len;
	
	complex16_cube *noise_dop_buf = 0;
	Complexf32_RealImag *noise_dop_buffer_f32 = 0;

	ft_task_status_obj.curr_frame_index++;
	
	if (pdata->noise_est_buf == NULL) {
		return -1;
	}
	
	if (ft_task_status_obj.curr_frame_index <= CONFIG_MMW_VENDOR_FILTER_FRAME_NUM) {
		return 0;
	}
	
	mmw_process_mem_alloc((void **)&noise_dop_buf, dop_len * sizeof(complex16_cube));
	mmw_process_mem_alloc((void **)&noise_dop_buffer_f32, dop_len * sizeof(Complexf32_RealImag));
	if (noise_dop_buf == NULL || noise_dop_buffer_f32 == NULL) {
		return -1;
	}
	mmw_motion_cube_access_open();
	FT_LOG_PRINT("ongoing collecting noise frame count %d rxnum %d rangelen %d doplen %d\n", ft_task_status_obj.curr_frame_index,
		pdata->detect_param.rx_num, pdata->detect_param.range_len, pdata->detect_param.doppler_len);
	

	for (uint8_t tx_idx = 0; tx_idx < pdata->detect_param.tx_num; tx_idx++) {
		for (uint8_t rx_idx = 0; rx_idx < pdata->detect_param.rx_num; rx_idx++) {
			if (!RX_ANT_VAILD(rx_idx)) {
				continue;
			}
		/* 1、get all dop data for each rbin 
		 * 2、restore auto gain
		 * 3、calculate sum of all high speed dop bin power
		 * 4、storage the sum in mradar_est_data.noise_est_buf
		 * */
			for (uint16_t range_idx = 0; range_idx < pdata->detect_param.range_len; range_idx++) {
				mmw_fft_doppler(noise_dop_buf, dop_len, tx_idx, rx_idx, range_idx);
				fft_autogain_restore_f32(noise_dop_buf, noise_dop_buffer_f32, tx_idx, range_idx, dop_len);
				
				power = noise_est_buf_update(noise_dop_buffer_f32, dop_len);
				pdata->noise_est_buf[(ant_mimo_id * range_len) + range_idx] += power;
			}
			ant_mimo_id++;
		}
	}
	mmw_fft_autogain_clear();	
	mmw_motion_cube_access_close();
	
	mmw_process_mem_free((void **)&noise_dop_buf);
	mmw_process_mem_free((void **)&noise_dop_buffer_f32);
	
	/* process finish */
	if(ft_task_status_obj.curr_frame_index >= g_noise_est_frame.frame_num){
		mmw_frq_bin_detect_process_finish();
	}
	
	return 0;
}

/** @brief set max difference detect param */
void ft_freq_bin_detect_frame_param_set(struct mmw_freq_bin_detect_recive_param_t *param)
{
	/* check validity of param */ 
	if (param == NULL) {
		return;
	}
	
	/* config each-frequency bin detect frame param  */
	/* 
		If user don't config chirp & frame params, use default param:
		59GHz, 128fft, 8cm, 10.24m, 32 dfft
	*/
	if (param->range_mm == 0) {
		g_noise_est_frame.range_mm = FT_DEFAULT_RANGE;
	} else {
		g_noise_est_frame.range_mm = param->range_mm;
	}

	if (param->range_resolut == 0) {
		g_noise_est_frame.resol_mm = FT_DEFAULT_RANGE_RES;
	} else {
		g_noise_est_frame.resol_mm = param->range_resolut;
	}

	if (param->start_freq == 0) {
		g_noise_est_frame.startFreq_MHz = FT_DEFAULT_START_FREQ;
	} else {
		g_noise_est_frame.startFreq_MHz = param->start_freq;
	}

	if (param->veloc_mm == 0) {
		g_noise_est_frame.velocity_mm = NOISE_FT_DEFAULT_VELOC;
	} else {
		g_noise_est_frame.velocity_mm = param->veloc_mm;
	}

	if (param->vel_resol == 0) {
		g_noise_est_frame.veloc_resol = NOISE_FT_DEFAULT_VEL_RES;
	} else {
		g_noise_est_frame.veloc_resol = param->vel_resol;
	}

	if (param->frame_num == 0) {
		g_noise_est_frame.frame_num = NOISE_FT_DEFAULT_FRAME_NUM + CONFIG_MMW_VENDOR_FILTER_FRAME_NUM;
	} else {
		g_noise_est_frame.frame_num = param->frame_num + CONFIG_MMW_VENDOR_FILTER_FRAME_NUM;
	}
}


int mmw_each_freq_bin_detect_scope_set(uint16_t start_index, uint16_t end_index)
{
	if ((start_index > end_index) || (end_index >= g_rbin_freq_bin_detect_param.range_fft_len)) {
	}
	g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] 	= start_index;
	g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] 	= end_index;
	return 0;
}

/** @brief init param before max diff detect process
 * 1. init max diff detect param 
 * 2. memlloc buffer use for storage data which in noise est processing
 * */
int ft_freq_bin_detect_init(struct mmw_noise_est_frame_t *pFrame)
{
	int ret = 0;
	
	if (ft_task_status_obj.running == true) {
		ret = -1;
		return ret;
	}
	
	/* init g_rbin_noise_est_param struct */
	uint16_t range_fft_len = (pFrame->range_mm + pFrame->resol_mm - 1) / pFrame->resol_mm;
	range_fft_len = round_2power(range_fft_len);
	g_rbin_freq_bin_detect_param.range_fft_len = range_fft_len;
	
	struct mmw_freq_bin_detect_t *pdata = get_freq_bin_detect_param();
	
	float *noise_est_buf = 0;
	struct mmw_each_freq_bin_typical_value_t *typical_value = 0;

	/* if user not set scope of range index, use default param as below:
	 * 		start range index: 	range_fft_len * 1 / 10
	 * 		end range index:	(range_fft_len * 9 + 9)/10 - 1
	 * else 
	 * 		check the scope is valid.
	*/
	if (g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] == 0
		&& g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] == 0) {
		g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] = range_fft_len*1/10;
		g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] = (range_fft_len*9 + 9)/10 - 1;
	} else {
		if (g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] >= range_fft_len) {
			g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID] = range_fft_len -1;
		}
		if (g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] >= range_fft_len) {
			g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] = range_fft_len -1;
		}
	}

	g_rbin_freq_bin_detect_param.detect_range_num = g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID] - g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID];
	
	FT_LOG_PRINT("range fft len:%d,, range start:%d, range end:%d\n", 
				g_rbin_freq_bin_detect_param.range_fft_len,
				g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_START_ID], 
				g_rbin_freq_bin_detect_param.rbin_scop[RBIN_SCOPE_OF_END_ID]);
	
	mmw_process_mem_alloc((void**)&noise_est_buf, CONFIG_MAX_MIMO_ANT * range_fft_len * sizeof(float));	
	if (noise_est_buf == NULL) {
		ret = -1;
		ft_freq_bin_detect_deinit();
		return ret;
	}
	
	mmw_process_mem_alloc((void **)&typical_value, CONFIG_MAX_MIMO_ANT * g_rbin_freq_bin_detect_param.detect_range_num * sizeof(*typical_value));
	if (typical_value == NULL) {
		ret = -1;
		ft_freq_bin_detect_deinit();
		return ret;
	}
	
	
	ft_task_status_obj.curr_frame_index = 0;
	ft_task_status_obj.running = true;	
	
	memset(noise_est_buf, 0, CONFIG_MAX_MIMO_ANT * range_fft_len * sizeof(float));
	pdata->noise_est_buf = noise_est_buf;
	
	memset(typical_value, 0,  CONFIG_MAX_MIMO_ANT * g_rbin_freq_bin_detect_param.detect_range_num * sizeof(struct mmw_each_freq_bin_typical_value_t));
	freq_bin_report.typical_value = typical_value;

	return ret;
}


void ft_freq_bin_detect_deinit(void)
{
	struct mmw_freq_bin_detect_t *pdata = get_freq_bin_detect_param();
	
	if (pdata->noise_est_buf) {
		mmw_process_mem_free((void **)&pdata->noise_est_buf);
	}
	
	if (freq_bin_report.typical_value) {
		mmw_process_mem_free((void **)&freq_bin_report.typical_value);
	}
	
	ft_task_status_obj.running = false;
	ft_task_status_obj.curr_frame_index = 0;
}



/** @brief max diff detect start, max diff detect use the same mmw wave as noise estmation.
 *  1.config mmw wave
 *  2.init buffer struct
 *  3.config mmw callback
 *  4.creat semaphore
 *  5.mmw wave start
 *  @return return 0 on sucess
 * */
int mmw_freq_bin_detect_start(struct mmw_noise_est_frame_t *pFrame, struct mmw_freq_bin_detect_recive_param_t *ptr_usr_param)
{
	int ret = 0;
	uint8_t int_type;
	
	/* config mmw wave */
	mmw_wave_noise_est_frame_config(pFrame);
	/* init buffer struct */
	ret = ft_freq_bin_detect_init(pFrame);
	if (ret < 0) {
		return -1;
	}
	
	/* config mmw callback */
	if (pFrame->work_mode == MMW_WORK_MODE_2DFFT) {
		int_type = MMW_DATA_TYPE_2DFFT;
	} else {
		int_type = MMW_DATA_TYPE_1DFFT;
	}
	ret = mmw_ctrl_callback_cfg(mmw_ctrl_ft_freq_bin_detect_frame_cb, int_type, NULL);
	if (ret) {
		mmw_ctrl_callback_cfg(NULL, MMW_DATA_TYPE_DISABLE, NULL);
	}
	
	/* mmw wave start */
	ret = mmw_ctrl_start();
	if (ret) {
		return -1;
	}
	return ret;
}

int factory_test_freq_bin_detect_start(uint8_t* ft_cmd)
{
	int ret;
	/* get receive HIF msg */
	struct mmw_freq_bin_detect_recive_param_t *recive_param = (struct mmw_freq_bin_detect_recive_param_t *)((uint8_t*)ft_cmd);
	/* set each-frequency bin detect frame param */
	ft_freq_bin_detect_frame_param_set(recive_param);
	/* set detect scope */
	mmw_each_freq_bin_detect_scope_set(recive_param->rbin_start_idx, recive_param->rbin_end_idx);
	/* start max diff detect, mmw wave is the same as noise estimation */
	ret = mmw_freq_bin_detect_start(&g_noise_est_frame, recive_param);

	return ret;
}

