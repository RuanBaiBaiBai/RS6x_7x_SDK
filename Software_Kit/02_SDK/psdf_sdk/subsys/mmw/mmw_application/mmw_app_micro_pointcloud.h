/*
 **************************************************************************************************
 *          Copyright (c) 2022 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */
#ifndef __MMW_POINTCLOUD_H__
#define __MMW_POINTCLOUD_H__

/* Include Files */
#include "common.h"
#include "config.h"
#include "mmw_ctrl.h"
#include "mmw_app_pointcloud.h"
#include "mmw_alg_doa.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	BOARD_MRS6130_P1806 = 0,
	BOARD_MRS6130_P1812,
	BOARD_MRS6240_P2512,
	BOARD_MRS6241_P2828_M62,
	BOARD_MRS6241_P2840_M81,
	BOARD_MRS7241_P2828_M62,
	BOARD_MRS7241_P2840_M81,
	BOAED_INVALID
} PSIC_Board_e;


#define PRESENCE_POINT_MAX         256  //default presence point num;

/* Config the processing method for presence point cloud, 0--> SNR THRESHOLD, 1--> CFAR
 * SNR THRESHOLD method: filter point clouds based on signal-to-noise ratio and threshold.
 * CFAR method: filter point clouds using the CFAR method provided by PSIC.
 * */
#define MICRO_PROCESS_SNR_THRESHOLD_METHOD	(0)
#define MICRO_PROCESS_CFAR_METHOD			(1)

#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD
#define CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD            MICRO_PROCESS_CFAR_METHOD   /* default SNR_THRESHOLD method */
#endif

#if (CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD != MICRO_PROCESS_SNR_THRESHOLD_METHOD) && (CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD != MICRO_PROCESS_CFAR_METHOD)
#error "CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD must be 0 or 1"
#endif

#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_CFAR_METHOD
#define MICRO_CFAR_MODE_SINGLE_SIDE    (0)
#define MICRO_CFAR_MODE_BOTH_SIDE      (1)

/* Config micro cfar mode, 1 --> single side mode; 0 --> both side mode
 * single side mode：This mode is suitable for simple environments where the area close to the target is open and unobstructed.
 * 				     Background noise estimation is only performed using the noise reference unit in front of the detected unit.
 * both side mode: This mode is suitable for scenarios where the surrounding environment of the target is complex and there may 
 * 				   be clutter in front. It provides a more robust estimation of multiple noise units in the surrounding complex 
 * 				   environment and balances the background noise.
 * supplementary note: It is recommended to use single side mode, if there are objects behind the target in the usage scenario. 
 * 					When select 'MICRO_CFAR_MODE_BOTH_SIDE' heap will cost more 10K.
 * */

#ifndef CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE
#define CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE								MICRO_CFAR_MODE_BOTH_SIDE
#endif

#if (CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE != MICRO_CFAR_MODE_SINGLE_SIDE) && (CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE != MICRO_CFAR_MODE_BOTH_SIDE)
#error "CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE must be 0 or 1"
#endif
#endif /* #if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == MICRO_PROCESS_CFAR_METHOD */

#ifndef CONFIG_MMW_PRESENCE_POINT_MAX
#define CONFIG_MMW_PRESENCE_POINT_MAX										PRESENCE_POINT_MAX
#endif

/* skip num of range bin from start */
#define MICRO_RANGE_BIN_SKIP_NUM      3

#define NOISE_SEC_LEFT          1
#define NOISE_SEC_RIGHT         1
#define NOISE_SEC_NUM           (1 + NOISE_SEC_LEFT + NOISE_SEC_RIGHT)

/* ca-cfar default param */
#define MICRO_CA_CFAR_NOISE_LEN			16   //16 bin
#define MICRO_CA_CFAR_GUARD_RANGE_MM	400   //400 mm
#define MICRO_CA_CFAR_RANGE_RES_MM_MIN_LIM		(MICRO_CA_CFAR_GUARD_RANGE_MM / (MICRO_CA_CFAR_NOISE_LEN - 6))


typedef struct micro_ca_cfar_param {
	/* CA_CFAR Param */
	uint16_t	linear_snr_q4;	// q4
	uint16_t	guard_len;
	uint16_t	win_len;
	uint16_t	hw_hpf_suppressed_range_bin_len;
	uint16_t	*linear_th_buffer_target;
	uint16_t	*linear_th_buffer_no_target;
	
	union {
        struct {
            int16_t noise_len_segment1;
            int16_t noise_len_segment2;
        };		// MICRO_CFAR_MODE_SINGLE_SIDE
        struct {
            int16_t noise_len;
            int16_t half_win_len;
        };		// MICRO_CFAR_MODE_BOTH_SIDE
    };
	
}MicroCfarPARAM_t;

typedef struct{
	uint8_t micro_chirp_num_log2;
	uint8_t micro_dop_num_log2;
	uint16_t mdop_fft_buf_num;
	uint8_t micro_dop_fft_gain_log2;
	uint8_t micro_dop_gain_log2;
} MicroProcessParam_t;

typedef struct{ 
	float alpha;
    int16_t first_clip_margin_db;     //q4
    int16_t second_clip_margin_db;    //q4
    int16_t two_level_diff_db;        //q4
    uint8_t scene_type;             
}MicroSceneJudgeParam_t;

typedef struct _presence_pointcloud_t {
	uint32_t    frame_num;      /* micro frame counter */
	uint16_t    presence_points_num;
	uint8_t     cfar_stationary;
	uint8_t     frame_div_idx;
	uint8_t     frame_div_cnt;
	/* Scene judge param */
	MicroSceneJudgeParam_t mscene_param;
	/* Process param */
	MicroProcessParam_t mprocess_param;
	/* CFAR Param */
	MicroCfarPARAM_t mcfar_Param;
	/* API param */
	uint16_t 	range_bin_skip;
	uint16_t    range_bin_max;
	uint16_t    range_min_mm;  /* user param  convert to range_bin_skip */
	uint16_t    range_max_mm;  /* user param  convert to range_bin_max */
	
	uint16_t 	dop_bin_start_skip;
	uint16_t 	dop_bin_end_skip;
	
	const int16_t  *hanning_win;
	void           *proc_bufs;      /* buffer for mdop fft and dbf */
	uint32_t	   *proc_abs_bufs;			/* U32Q15 */
	uint32_t	   *proc_limit_abs_bufs;	/* U32Q15 */
	uint16_t	   *proc_cfar_th_bufs;	/* pointer points to buffer for linear threshold, U16Q4 */
	uint8_t 	board_type;
} MPC_CTRL;

/* initial and reset */
int  mmw_presence_point_init(void);
void mmw_presence_point_deinit(void);
void mmw_presence_point_restart(void);

/* data flow processing */
uint32_t mmw_presence_point_num_get(void);
uint32_t mmw_micro_doppler_num_get(void);
bool     mmw_micro_point_frame(void);

PresencePointCloudBuffer_t* mmw_presence_point_process(void);

/* parameters config */
int mmw_presence_point_range_set (uint32_t min_range_mm, uint32_t max_range_mm);
int mmw_presence_point_range_get (uint32_t *min_range_mm, uint32_t *max_range_mm);
int mmw_micro_cfar_snr_set(uint32_t snr_linear);
uint32_t mmw_micro_cfar_snr_get(void);
int mmw_micro_frame_rate_set(uint8_t mmw_frame_div_cnt);
uint8_t mmw_micro_frame_rate_get(void);
MPC_CTRL *mmw_presence_g_mpc_ctrl_get(void);


#ifdef __cplusplus
}
#endif

#endif /* __MMW_POINTCLOUD_H__ */

