#ifndef __MMW_APP_POINTCLOUD_CONFIG_H__
#define __MMW_APP_POINTCLOUD_CONFIG_H__

#include <stdint.h>
#include "mmw_alg_pointcloud_typedef.h"
#include "float.h"

/**
 * Static clutter removal method: 
 * If configured as MMW_CLUTTER_REMOVAL_NONE, static clutter will not be removed,
 * 	and all stationary targets will be retained. 
 * 
 * If configured as MMW_CLUTTER_REMOVAL_ALL, hardware automatic static clutter removal will be enabled.
 * 
 * If configured as MMW_CLUTTER_REMOVAL_DC, the static component caused by the direct path will be 
 * 	estimated by software and automatically removed by hardware. 
 * 	This process requires software involvement.
 * 
 * Please refer to the RS6x_7x_Radar_Reference_Manual for more details.
 * 
 * When presence point cloud is enabled, please set to MMW_CLUTTER_REMOVAL_NONE.
 */
#ifndef	CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_RM
#if CONFIG_MMW_PRESENCE_POINT_CLOUD
#define CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_RM 			(MMW_CLUTTER_REMOVAL_NONE)
#else
#define CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_RM 			(MMW_CLUTTER_REMOVAL_ALL)
#endif
#endif

/**
 * Configures the convergence coefficient of the static clutter low-pass filter. 
 * The larger the CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_COVER_TIME value, the slower the static clutter converges, 
 * 	but the higher the clutter suppression level. 
 * 
 * When a static target impulse occurs, the number of frames required for static clutter convergence is approximately 
 * 	2 << CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_COVER_TIME.
 */
#ifndef	CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_COVER_TIME
#define CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_COVER_TIME	(4)
#endif

/**
 * @brief Configure the receive antenna gain (power)
 * @details Default: 51dB. Configurable to 5 levels: 27/33/49/45/51.
 *          Example: If normalized corner reflector power is -10dB at 51dB gain,
 *          it corresponds to -16dB at 45dB gain.
 */
#ifndef	CONFIG_MMW_POINT_CLOUD_BB_RECP_GAIN
#define CONFIG_MMW_POINT_CLOUD_BB_RECP_GAIN				(MMW_RX_ANA_GAIN_51DB)
#endif

/**
 * Configure Tx power attenuation. Default is 0dB, configurable range is 0~8dB,
 * shared by all transmit antennas. If the normalized corner reflector power is
 * -10dB at 0dB attenuation, it becomes -16dB at 6dB attenuation.
 */
#ifndef	CONFIG_MMW_POINT_CLOUD_BB_TX_ATTENUA
#define CONFIG_MMW_POINT_CLOUD_BB_TX_ATTENUA			(MMW_TX_RF_ATT_0DB)
#endif

/**
 * @brief Configures the bandwidth of the high-pass filter applied to the IF signal.
 *
 * The high-pass filter suppresses direct-path signals and close-range target energy.
 * CONFIG_MMW_POINT_CLOUD_BB_HPF_BW allows configuring the IF high-pass filter
 * bandwidth for each receive antenna individually. A wider bandwidth enhances suppression
 * of low-frequency signals (close-range targets), preventing them from exceeding the ADC
 * full-scale range.
 *
 * Additionally, due to hardware processing characteristics, target energy near the maximum
 * detection range may couple to the nearest range, and targets at the nearest range may
 * couple to the maximum detection range. Using a wider IF filter bandwidth provides better
 * suppression of these coupled signals, preventing false detections.
 */
#ifndef	CONFIG_MMW_POINT_CLOUD_BB_HPF_BW
#define CONFIG_MMW_POINT_CLOUD_BB_HPF_BW				(HPF_BW_0P5M)
#endif

/**
 * Configure the maximum number of points to read from the CFAR list using the macro
 * 'CONFIG_MMW_MAX_MOTION_CFAR_NUM'. When computational load is excessive or SRAM is
 * insufficient, it is recommended to reduce this value for faster computation speed.
 * If the actual number of CFAR points exceeds the value set by this macro, the point
 * cloud computation subsystem will use the first CONFIG_MMW_MAX_MOTION_CFAR_NUM points
 * for subsequent processing.
 */
#ifndef CONFIG_MMW_MAX_MOTION_CFAR_NUM						
#define CONFIG_MMW_MAX_MOTION_CFAR_NUM					(768)
#endif

/**
 * Configure the CFAR threshold using the macro 'CONFIG_MMW_MOTION_CFAR_TH_LIN'.
 * The threshold configured here represents the multiplier by which the CFAR detection
 * threshold on the power spectrum is elevated above the noise floor power.
 * For example, if a detection threshold 12 dB above the noise floor is desired,
 * CONFIG_MMW_MOTION_CFAR_TH_LIN should be set to power(10, 12/10) = 15.8489.
 * Generally, this value should be configured within a CFAR threshold range of 10~25 dB,
 * and should not exceed 30 dB.
 */
#ifndef CONFIG_MMW_MOTION_CFAR_TH_LIN
#define	CONFIG_MMW_MOTION_CFAR_TH_LIN					(15.8489f) /* default 12dB, 15.8489 = power(10, 12/10) */
#endif

/**
 * Configure the peak grouping method using the macro 'CONFIG_MMW_MOTION_PEAKGROUP_METHOD'.
 * For computational efficiency, peak grouping is generally configured as 'PEAKGROUPING_DISABLE'.
 * If distinguishing point cloud density between valid and invalid targets is required,
 * it is recommended to try various peak grouping methods to verify whether project
 * requirements can be met.
 */
#ifndef CONFIG_MMW_MOTION_PEAKGROUP_METHOD
#define CONFIG_MMW_MOTION_PEAKGROUP_METHOD				(PEAKGROUPING_DISABLE)
#endif

/**
 * Configure the clutter filtering threshold in the mmw_psic_2nd_pass_filter() function
 * using the macro 'CONFIG_MMW_MOTION_2ND_PASS_THRES'. A larger value results in weaker
 * filtering effect.
 * 'CONFIG_MMW_MOTION_2ND_PASS_THRES' must be configured greater than 0; the recommended
 * value is 3.
 */
#ifndef CONFIG_MMW_MOTION_2ND_PASS_THRES
#define	CONFIG_MMW_MOTION_2ND_PASS_THRES				(3)			    	/* threshold for 2nd pass filter, larger than 0, bigger for loosen constrain */
#endif

/**
 * Configure whether to filter out all targets with Doppler indices in the range
 * [dfft_len/2-1, dfft_len/2, dfft_len/2+1] in CFAR (both software and hardware)
 * using the macro 'CONFIG_MMW_STATIC_RM_EN'.
 * 
 * If the radar is stationary and detection of relatively static targets is not desired,
 * enable this macro. Otherwise, static targets in the environment will quickly fill
 * the CFAR target list, wasting computational resources. When the radar is stationary,
 * static target removal and static clutter removal have slightly overlapping functionality:
 * static target removal directly eliminates relatively static targets at the application
 * logic level, while static clutter removal learns and removes clutter at the digital
 * signal processing level.
 *  
 * Enabling static clutter removal allows detection of relatively static targets that
 * appear in a short period, such as pedestrians crossing with zero radial velocity,
 * offering broader adaptability, but may produce a small number of false detections
 * at zero velocity.
 *
 * Customers should consider whether to enable static target removal based on actual
 * project conditions. It is recommended that when static target removal is enabled
 * and the radar is mounted on a stationary platform, static clutter removal should
 * also be enabled simultaneously to prevent strong reflected target energy from leaking
 * beyond the static target removal range.
 */
#ifndef CONFIG_MMW_STATIC_RM_EN
#define CONFIG_MMW_STATIC_RM_EN							(MMW_ENABLE)
#endif

/**
 * Configure whether to enable the low-speed filtering function using the macro
 * 'CONFIG_MMW_LOW_SPEED_FILTER_EN'.
 *
 * If the "static target removal" function is found to filter out too many relatively
 * static targets, the low-speed filtering function can be used to narrow the velocity
 * range for filtering stationary targets. This allows more micro-moving targets to pass
 * through while filtering out the majority of environmental interference. After enabling
 * 'CONFIG_MMW_LOW_SPEED_FILTER_EN', the point cloud computing subsystem will perform
 * precise velocity measurement for each target with a Doppler index of dfft_len/2 that
 * is also an extremum point in the Doppler dimension, thereby consuming additional CPU
 * computation time.
 */
#ifndef CONFIG_MMW_LOW_SPEED_FILTER_EN
#define CONFIG_MMW_LOW_SPEED_FILTER_EN					(MMW_DISABLE)
#endif

/**
 * Configure the low-speed filtering range using the macro 'CONFIG_MMW_LOW_SPEED_FILTER_FACTOR'.
 * This parameter determines that the low-speed filtering range is:
 * [dfft_len/2 - CONFIG_MMW_LOW_SPEED_FILTER_FACTOR, dfft_len/2 + CONFIG_MMW_LOW_SPEED_FILTER_FACTOR]
 * The valid value range for CONFIG_MMW_LOW_SPEED_FILTER_FACTOR is between 0 and 0.5.
 */
#ifndef CONFIG_MMW_LOW_SPEED_FILTER_FACTOR
#define CONFIG_MMW_LOW_SPEED_FILTER_FACTOR				(0.14f)
#endif


#ifndef CONFIG_MMW_CHIP_MOUNT_TYPE
#if CONFIG_BOARD_MRS6130_P1806 || CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS7241_P2828_CPUF
#define  CONFIG_MMW_CHIP_MOUNT_TYPE						(MMW_MOUNT_HORIZONTAL)			/* Default is Horizontal Mount for radar coordinate -- develop board is vertical */
#elif CONFIG_BOARD_MRS6130_P1812 || CONFIG_BOARD_MRS6240_P2512_CPUF || CONFIG_BOARD_MRS6240_P2512_CPUS || CONFIG_BOARD_MRS6241_P2840_M81_CPUF || CONFIG_BOARD_MRS7241_P2840_CPUF
#define  CONFIG_MMW_CHIP_MOUNT_TYPE						(MMW_MOUNT_VERTICAL)			/* Default is Vertical Mount for radar coordinate -- develop board is vertical */
#else
#error "please select valid board!"
#endif	/* #if CONFIG_BOARD_MRS6130_P1806 || CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS7241_P2828_CPUF */
#endif	/* CONFIG_MMW_CHIP_MOUNT_TYPE */

/**
 * Enable the adaptive gain feature of hardware FFT using the macro 'CONFIG_MMW_FFT_AUTOGAIN_EN'. 
 * Do not enable adaptive gain in the following cases:
 * 	1. When DataCube reporting is required: enabling hardware FFT adaptive gain requires additional reporting of the gain coefficient,
 * 		which is not supported by the current protocol.
 * 	2. When using software CFAR via mmw_point_cloud_process_sw_cfar(): 
 * 		software CFAR does not support adaptive gain compensation.
 * 
 * If only motion or presence point clouds are used, it is recommended to enable adaptive gain for a higher detection dynamic range. 
 * 		The default value is to disable adaptive gain.
 * 
 * For more details on adaptive gain, refer to the 'RS6x_7x_Radar_Reference_Manual'.
 */
#ifndef CONFIG_MMW_FFT_AUTOGAIN_EN
#define CONFIG_MMW_FFT_AUTOGAIN_EN		(0)
#endif	/* CONFIG_MMW_FFT_AUTOGAIN_EN */

#define MMW_START_BIN_IDX						4					/* due to the influenceof the direct wave,micro motion point cloud need skip the first 4 range bin */
#define MMW_DIRECT_WAVE_RANGE_MM				200					/* due to the influenceof the direct wave,motion point cloud need raise threshold in the first 200 mm */
#define MMW_DIRECT_WAVE_THRES					1995.2623149f		/* 10 * log10(3.1622777) = 33dB */

/* 
 * Configures the maximum range bin for presence point cloud search and storage.
 * The actual number of range bins used by the presence point cloud is 
 * 	Min(Range FFT size, CONFIG_MMW_PRESENCE_RANGE_BIN_NUM).
 * 
 * If the Range FFT size is larger than CONFIG_MMW_PRESENCE_RANGE_BIN_NUM,
 * verify if it is necessary to increase this macro to match the Range FFT size.
 * 
 * Increasing this value will increase the SRAM consumption of the presence point cloud.
 */
#ifndef CONFIG_MMW_PRESENCE_RANGE_BIN_NUM
#define CONFIG_MMW_PRESENCE_RANGE_BIN_NUM					(256)
#endif

/* CFAR threshold for presence point cloud */
#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD_CFAR_TH_DB
#define CONFIG_MMW_PRESENCE_POINT_CLOUD_CFAR_TH_DB			(14)
#endif

/* CFAR linear threshold offest value for presence point cloud */
#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD_CFAR_LINEAR_TH_OFFEST_DB
#define CONFIG_MMW_PRESENCE_POINT_CLOUD_CFAR_LINEAR_TH_OFFEST_DB	(0)
#endif

/**
 * he macro 'CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2'. The actual number of Intervals 
 * used is 1 << CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2.
 * 
 * The frame span for calculating the presence point cloud DataCube is:
 * CONFIG_MMW_PRESENCE_POINT_CLOUD_INTERVAL_DEC * 1 << CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2
 * 
 * Using a larger number of Intervals improves the detection capability for distant presence 
 * point clouds and those with slower changes, but significantly increases SRAM overhead and 
 * slightly increases computation time.
 * 
 * This value must be less than or equal to CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2. 
 * Please refer to Section 5 for more details on the calculation process and extraction methods.
 */
#ifndef CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2
#define CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2					(3)
#endif

/* Doppler FFT Length for Presence Point Cloud (log2)
 * Configure the micro-Doppler FFT length for the presence point cloud using the macro 
 * 'CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2'.
 * 
 * CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2 must >= CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2.
 * 
 * A larger micro-Doppler FFT length does not enhance target detection capability, but it can 
 * improve point cloud density. It also increases the computation time for micro-Doppler point 
 * clouds. Only 4 and 5 is supportted for this macro.
 * 
 * Do not modify this value unless necessary.
 **/
#ifndef CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2
#define CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2					(4)
#endif

/**
 * @brief Configures the range downsampling ratio for the presence point cloud via the macro
 *        'CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC'. The point cloud computation submodule
 *        downsamples and stores range data according to this ratio. Using a larger downsampling
 *        ratio significantly reduces storage and computational load but decreases presence
 *        point cloud density.
 *
 * @details For example: If 'CONFIG_MMW_PRESENCE_RANGE_BIN_NUM' is configured to 256 and
 *          'CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC' to 2, the actual range bins used for
 *          micro-Doppler DataCube calculation are 0, 2, 4... totaling 127 bins. The remaining
 *          range bins are not cached or computed.
 *
 * @note    It is recommended to keep the default value of 2. If significant storage and
 *          computational pressure persists, use a larger range downsampling ratio.
 */
#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC
#define CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC			(2)
#endif

/**
 * Interval Down-sampling Rate for Presence Point Cloud
 * Configure the Interval down-sampling rate for the presence point cloud using the macro 
 * 'CONFIG_MMW_PRESENCE_POINT_CLOUD_INTERVAL_DEC'. The point cloud calculation sub-module 
 * acquires the range FFT data of the zero-velocity bin from the DataCube once every 
 * CONFIG_MMW_PRESENCE_POINT_CLOUD_INTERVAL_DEC frames, serving as one Interval sample for 
 * the micro-Doppler DataCube.
 * 
 * The frame span for the presence DataCube is:
 * CONFIG_MMW_PRESENCE_POINT_CLOUD_INTERVAL_DEC * 1 << CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2
 * 
 * If developer wish to observe weaker presence point clouds, developer can set a larger frame span for 
 * the presence DataCube.
 */
#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD_INTERVAL_DEC
#define CONFIG_MMW_PRESENCE_POINT_CLOUD_INTERVAL_DEC		(4)
#endif


/**
 * Configure the point cloud processing range using the macros
 * 'CONFIG_MMW_RANGE_INDEX_THRESHOLD_MIN' and 'CONFIG_MMW_RANGE_INDEX_THRESHOLD_MAX'.
 * Only point clouds with distance indices within
 * [CONFIG_MMW_RANGE_INDEX_THRESHOLD_MIN, CONFIG_MMW_RANGE_INDEX_THRESHOLD_MAX] will
 * undergo angle estimation and be output.
 * To reduce computation for uninteresting regions, use these macros to limit the
 * distance index range.
 *
 */
#ifndef CONFIG_MMW_RANGE_INDEX_THRESHOLD_MIN
#define CONFIG_MMW_RANGE_INDEX_THRESHOLD_MIN				(0)
#endif
#ifndef CONFIG_MMW_RANGE_INDEX_THRESHOLD_MAX
#define CONFIG_MMW_RANGE_INDEX_THRESHOLD_MAX				(UINT16_MAX)
#endif

/**
 * Configure the valid speed range for point clouds using the macros
 * 'CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MIN', 'CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MAX',
 * 'CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MIN', and 'CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MAX'.
 * For positive target speeds, only speed indices with an offset relative to 0-speed within
 * [CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MIN, CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MAX] will
 * undergo angle estimation and be output.
 * For negative target speeds, only targets with an absolute speed index offset relative to 0-speed
 * within [CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MIN, CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MAX]
 * will undergo angle estimation and be output.
*/
#ifndef CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MIN
#define CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MIN		(0)
#endif
#ifndef CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MAX
#define CONFIG_MMW_POSITIVE_SPEED_INDEX_THRESHOLD_MAX		(UINT16_MAX)
#endif
#ifndef CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MIN
#define CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MIN		(0)
#endif
#ifndef CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MAX
#define CONFIG_MMW_NEGATIVE_SPEED_INDEX_THRESHOLD_MAX		(UINT16_MAX)
#endif

typedef struct{
	uint16_t range_threshold_idx[2];			/* report point cloud range threshold,first is left, second is right */
	uint16_t positive_vel_threshold_idx[2];		/* report point cloud positive velocity threshold,first is left, second is right */
	uint16_t negative_vel_threshold_idx[2];		/* report point cloud negative velocity threshold,first is left, second is right */
} MmwPointCloudFilterConfig_t;

typedef struct{
	/*	if enalbe clutter config,frame_type must be DATA_BOX_2D_FRAME_MODE
		if use micro motion or motion point cloud,clutter_rm_method can not be MMW_CLUTTER_REMOVAL_ALL
		if radar is placed on a mobile platform, clutter need disable
	*/ 
	MmwPointClutterRemoval_e clutter_rm_method;		/* See MmwPointClutterRemoval_e enumerate */
	uint8_t clutter_cover_time;						/* use greater clutter cover time,accuracy is higher,range is 0~7 */
	uint8_t recep_gain_config;						/* recep attenua config range is MMW_RX_ANA_GAIN_xxx in mmw_ctrl.h, if max trigger range < 5m,need set less rx gain */
	uint8_t trans_attenua_config;					/* trans attenua config range is MMW_TX_RF_ATT_XXX(1 LSB = 1 dB) in mmw_ctrl.h */
	uint8_t hpf_bandwith_config;					/* analog hpf bandwith config,config value HPF_BW_xxx in mmw_ctrl.h,corresponding effective value is [0.5, 1, 2, 4, 8, 16](1 LSB = 1 MHz) */
} MmwPointCloudDetectionConfig_t;

typedef struct{
	uint16_t motion_point_cloud_max_cfar_num;			/* set motion point cloud max cfar number,range is 0~768 */
	PeakGroupingMethod_e peak_grouping_method;			/* default method for peak grouping process */
	uint8_t static_rm;									/* after enable,the moving point cloud will not report targets that are relativelys stationary to the radar;0 is disable,1 is enable */
	float threshold_snr_pwr_lin;						/* snr for software snr filter for non-coherent power, unit is W,range is 1.f~1584.8931.f */
	uint8_t psic_2nd_pass_thres;						/* threshold in psic_2nd_pass_filter, 3 is recommanded by default */
	uint8_t low_speed_filter_en;						/* after enable, will filter the point whose dop_idx after interpolation is less than low_speed_filter_factor */
	float low_speed_filter_factor;						/* during low-speed filting,if the interpolated dop_idx is less than low_speed_filter_threshold,it indicates that the current point has been filtered */
} MmwMotionPointCloudConfig_t;

typedef struct {
	MmwPointCloudFilterConfig_t mmw_point_cloud_filter_config;
	MmwPointCloudDetectionConfig_t mmw_point_cloud_detection_config;
	MmwMotionPointCloudConfig_t mmw_motion_point_cloud_config;
	MmwMountType_e chip_mount_type; 				/* mount type for CHIP, different mount type has different 'g_azi_geometry' and 'g_elev_geometry' */
	uint8_t auto_gain_flag;					/* use auto gain in fft or not, recomand to use autogain in application */
} MmwPointCloudUserCfg_t;

typedef struct {
	uint8_t micro_ca_cfar_snr_th;					/* ca cfar snr threshold, unit: dB */
	uint8_t micro_ca_cfar_snr_linear_th_offest;		/* ca cfar snr linear threshold offest value */
	uint8_t micro_frame_div_factor;					/* micro cube down sampling factor */
	uint8_t micro_cube_range_extract_frequency; 	/* micro cube range bin filter step size */
	uint8_t micro_chirp_num;						/* chirp num in one frame */
	uint8_t micro_dop_fft_len;						/* length of micro doppler fft */
}MMWPresencePointCloudUserCfg_t;

/* obtain the algorithm layer configuration */
extern MmwPointCloudUserCfg_t *mmw_point_cloud_get_user_cfg(void);
/* obtain the algorithm layer configuration (cosnt) */
extern const MmwPointCloudUserCfg_t *mmw_point_cloud_get_user_cfg_const(void);

extern MMWPresencePointCloudUserCfg_t *mmw_presence_point_cloud_get_user_cfg(void);

#if (CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X)
/* For RS6240, ant calibration is done by software, so calibration data may be got outside point cloud application */
Complexf32_RealImag *mmw_get_g_rx_ant_calib_data(void);
const Complexf32_RealImag *mmw_get_g_rx_ant_calib_data_const(void);
#endif

/* Get varies ant geometry structure used in point cloud application */
MmwPointAntGeometry_t *mmw_get_g_azi_geometry_struct(void);
MmwPointAntGeometry_t *mmw_get_g_elev_geometry_struct(void);
const MmwPointAntGeometry_t *mmw_get_g_azi_geometry_struct_const(void);
const MmwPointAntGeometry_t *mmw_get_g_elev_geometry_struct_const(void);

#endif
