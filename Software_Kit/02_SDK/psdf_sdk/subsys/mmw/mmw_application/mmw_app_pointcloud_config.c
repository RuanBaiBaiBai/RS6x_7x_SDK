#include "mmw_ctrl.h"
#include "mmw_app_pointcloud_config.h"
#include "float.h"
#include "stdbool.h"
#include "stdio.h"
#include "mmw_point_cloud_psic_lib.h"

/* This configuration is used for users to config some function after init the track */
MmwPointCloudUserCfg_t g_mmw_det_3d_user_cfg = {
	.mmw_point_cloud_filter_config = 
	{
		.range_threshold_idx = {CONFIG_MMW_RANGE_INDEX_THRESHOLD_MIN, CONFIG_MMW_RANGE_INDEX_THRESHOLD_MAX},
		.negative_vel_threshold_idx = {CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MIN, CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MAX},
		.positive_vel_threshold_idx = {CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MIN, CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MAX}
	},
	
	.mmw_point_cloud_detection_config = 
	{
		.clutter_rm_method = CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_RM,
		.clutter_cover_time = CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_COVER_TIME,
		.recep_gain_config = CONFIG_MMW_POINT_CLOUD_BB_RECP_GAIN,
		.trans_attenua_config = CONFIG_MMW_POINT_CLOUD_BB_TX_ATTENUA,
		.hpf_bandwith_config = CONFIG_MMW_POINT_CLOUD_BB_HPF_BW
	},
	
	.mmw_motion_point_cloud_config = 
	{
		.motion_point_cloud_max_cfar_num = CONFIG_MMW_MAX_MOTION_CFAR_NUM,
		.peak_grouping_method = CONFIG_MMW_MOTION_PEAKGROUP_METHOD,
		.static_rm = CONFIG_MMW_STATIC_RM_EN,
		.threshold_snr_pwr_lin = CONFIG_MMW_MOTION_CFAR_TH_LIN,			    /* default 12dB, 12dB = 10log10(15.8489) */
		.psic_2nd_pass_thres = CONFIG_MMW_MOTION_2ND_PASS_THRES,
		.low_speed_filter_en = CONFIG_MMW_LOW_SPEED_FILTER_EN,
		.low_speed_filter_factor = CONFIG_MMW_LOW_SPEED_FILTER_FACTOR
	},

	.chip_mount_type = CONFIG_MMW_CHIP_MOUNT_TYPE,
	.auto_gain_flag = CONFIG_MMW_FFT_AUTOGAIN_EN
};

MMWPresencePointCloudUserCfg_t g_mmw_presence_det_3d_user_cfg = {
	/* micro ca cfar snr threshold, default 14dB */
	.micro_ca_cfar_snr_th = CONFIG_MMW_PRESENCE_POINT_CLOUD_CFAR_TH_DB,
	/* micro ca cfar snr linear threshold offset value, default 0dB(no offest) */
	.micro_ca_cfar_snr_linear_th_offest = CONFIG_MMW_PRESENCE_POINT_CLOUD_CFAR_LINEAR_TH_OFFEST_DB,
	/* presence point cloud frame period is motion point period multiply micro_frame_div_factor */
	.micro_frame_div_factor = CONFIG_MMW_PRESENCE_POINT_CLOUD_INTERVAL_DEC,
	/* range bin extract step-size, recommend [1,2,4] */
	.micro_cube_range_extract_frequency = CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC,		
	/* micro chirp num recommend 8.
	 * if a larger chirp num is required(e.g. 16, 32), then more memory is needed for storage micro cube. 
	 * Alternatively, set a larger range extract frequency to reduce memory requirement.
	 * */
	.micro_chirp_num = 1 << CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2,	
	/* dop fft len only support [16,32], and dop fft len must greater than  micro chirp num */
	.micro_dop_fft_len = 1 << CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2
};

/* g_rx_ant_calib_data is used for angle calibration before measurement and is assigend in mmw_point_cloud_init() function */
#if CONFIG_SOC_RS6240
	Complexf32_RealImag g_rx_ant_calib_data[MMW_POINT_CLOUD_MAX_MIMO_RX_NUM] = {};

    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t g_azi_geometry[4] = {0, 1, 4, 5};
    uint8_t g_elev_geometry[4] = {3, 2, 1};
    MmwPointAntGeometry_t g_azi_geometry_struct = {
        .ptr_geometry = g_azi_geometry,
        .geometry_len = 4
    };
    MmwPointAntGeometry_t g_elev_geometry_struct = {
        .ptr_geometry = g_elev_geometry,
        .geometry_len = 3
    };
#elif CONFIG_BOARD_MRS7241_P2840_CPUF
/* all rx antennas calibrate data */
	Complexf32_RealImag g_rx_ant_calib_data[MMW_POINT_CLOUD_MAX_MIMO_RX_NUM] = {};

    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t g_azi_geometry[8] = {4, 5, 6, 7, 0, 1, 2, 3};
    uint8_t g_elev_geometry[2] = {0};
    MmwPointAntGeometry_t g_azi_geometry_struct = {
        .ptr_geometry = g_azi_geometry,
        .geometry_len = 8
    };
    MmwPointAntGeometry_t g_elev_geometry_struct = {
        .ptr_geometry = g_elev_geometry,
        .geometry_len = 0
    };
#elif CONFIG_BOARD_MRS7241_P2828_CPUF
	Complexf32_RealImag g_rx_ant_calib_data[MMW_POINT_CLOUD_MAX_MIMO_RX_NUM] = {};

    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t g_azi_geometry[6] = {4, 0, 6, 2, 7, 3};
    uint8_t g_elev_geometry[6] = {5, 0};
    MmwPointAntGeometry_t g_azi_geometry_struct = {
        .ptr_geometry = g_azi_geometry,
        .geometry_len = 6
    };
    MmwPointAntGeometry_t g_elev_geometry_struct = {
        .ptr_geometry = g_elev_geometry,
        .geometry_len = 2
    };
#elif CONFIG_SOC_RS6130
    /* RS6130 has no calibration data, is load from flash in mmw_point_cloud_init() function. */
    uint8_t g_azi_geometry[2] = {2, 1};
    uint8_t g_elev_geometry[2] = {0, 1};
    MmwPointAntGeometry_t g_azi_geometry_struct = {
        .ptr_geometry = g_azi_geometry,
        .geometry_len = 2
    };
    MmwPointAntGeometry_t g_elev_geometry_struct = {
        .ptr_geometry = g_elev_geometry,
        .geometry_len = 2
    };
#elif CONFIG_BOARD_MRS6241_P2840_M81_CPUF
    /* all rx antennas calibrate data */
	Complexf32_RealImag g_rx_ant_calib_data[MMW_POINT_CLOUD_MAX_MIMO_RX_NUM] = {};

    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t g_azi_geometry[8] = {4, 5, 6, 7, 0, 1, 2, 3};
    uint8_t g_elev_geometry[2] = {0};
    MmwPointAntGeometry_t g_azi_geometry_struct = {
        .ptr_geometry = g_azi_geometry,
        .geometry_len = 8
    };
    MmwPointAntGeometry_t g_elev_geometry_struct = {
        .ptr_geometry = g_elev_geometry,
        .geometry_len = 0
    };
#elif CONFIG_BOARD_MRS6241_P2828_M62_CPUF
	/* all rx antennas calibrate data */
	Complexf32_RealImag g_rx_ant_calib_data[MMW_POINT_CLOUD_MAX_MIMO_RX_NUM] = {};

    /* length of g_azi_geometry and g_elev_geometry must handle all mount types */
    uint8_t g_azi_geometry[6] = {4, 0, 6, 2, 7, 3};
    uint8_t g_elev_geometry[6] = {5, 0};
    MmwPointAntGeometry_t g_azi_geometry_struct = {
        .ptr_geometry = g_azi_geometry,
        .geometry_len = 6
    };
    MmwPointAntGeometry_t g_elev_geometry_struct = {
        .ptr_geometry = g_elev_geometry,
        .geometry_len = 2
    };
#else
    #error "Please Set User-Defined Ant Geometry!"
#endif

MmwPointCloudUserCfg_t *mmw_point_cloud_get_user_cfg(void)
{
	return &g_mmw_det_3d_user_cfg;
}

const MmwPointCloudUserCfg_t *mmw_point_cloud_get_user_cfg_const(void)
{
	return &g_mmw_det_3d_user_cfg;
}

MMWPresencePointCloudUserCfg_t *mmw_presence_point_cloud_get_user_cfg(void)
{
	return &g_mmw_presence_det_3d_user_cfg;
}

#if (CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X)
const Complexf32_RealImag *mmw_get_g_rx_ant_calib_data_const(void) {
	return &g_rx_ant_calib_data[0];
}
Complexf32_RealImag *mmw_get_g_rx_ant_calib_data(void) {
	return &g_rx_ant_calib_data[0];
}
#endif

MmwPointAntGeometry_t *mmw_get_g_azi_geometry_struct(void){
	return &g_azi_geometry_struct;
}

MmwPointAntGeometry_t *mmw_get_g_elev_geometry_struct(void){
	return &g_elev_geometry_struct;
}

const MmwPointAntGeometry_t *mmw_get_g_azi_geometry_struct_const(void){
	return &g_azi_geometry_struct;
}

const MmwPointAntGeometry_t *mmw_get_g_elev_geometry_struct_const(void){
	return &g_elev_geometry_struct;
}