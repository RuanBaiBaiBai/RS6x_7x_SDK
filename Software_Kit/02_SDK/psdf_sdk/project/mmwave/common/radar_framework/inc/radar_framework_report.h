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
#ifndef __RADRA_FRAMEWORK_REPORT_H
#define __RADRA_FRAMEWORK_REPORT_H

#include "mmw_ctrl.h"
#include "mmw_point_cloud_psic_lib.h"

#define CUBE_HEAD_LEN	(24)

/**
 * @brief Data Type Length (TL) header structure for radar framework reporting.
 *
 * This structure defines the header format used to encapsulate data payloads
 * when reporting radar information via HIF (Host Interface). It includes
 * fields for type identification, length calculation, and configuration
 * parameters.
 *
 * @note The 'type' field is 8-bit and 'length' field is 24-bit to ensure
 *       efficient packing of the header structure.
 */
typedef struct {
    uint32_t type : 8;        ///< Type identifier for the report message.
    uint32_t length : 24;     ///< Total length of the payload following this header.
    uint8_t tx_num;           ///< Number of transmit antennas.
    uint8_t rx_num;           ///< Number of receive antennas.
    uint8_t rsv1;             ///< Reserved for future use.
    uint8_t rsv2;             ///< Reserved for future use.
    uint16_t range_bin_num;   ///< Number of range bins in the frame.
    uint16_t dop_bin_num;     ///< Number of Doppler bins in the frame.
} MMW_FRAME_TL;

typedef struct {
    uint32_t frame_idx;
    uint32_t frame_len;
	uint32_t offset;
} MMW_FRAME_UPLOAD;
/**
 * @brief get data cube report frame config parameters.
 * */
void radar_framework_report_param_get(void);

/**
 * @brief init data cube report parameters and malloc the memory.
 * 		  DataCube report module at most buffer block_num * block_size data cube.
 * @param block_num: block number used for report datacube;
 * @param block_size: block size of each block.
 * */
void radar_framework_data_report_init(uint32_t block_num, uint32_t block_size);
/**
 * @brief deinit data cube report parameters and free the memory.
 * */
void radar_framework_data_cube_report_deinit(void);

/**
 * @brief during each callback,data cube report process in SPI mode.
 * @param mmw_data：meaningless.
 * @param arg：meaningless.
 * @return return 0 indicates that data cube report sucess.
 * */
int radar_framework_ctrl_data_cube_report_cb(void *mmw_data, void *arg);

#define MMW_TL_TYPE_POINTS            4
#define MMW_TL_TYPE_TARGET            5

#define MMW_TL_FLAG_POLAR             0  /* polar */
#define MMW_TL_FLAG_CART              BIT(0)  /* cartesian */
#define MMW_TL_FLAG_MICRO             BIT(1)  /* static pointcloud */

#define POINT_DATA_SIZE	(3072)

typedef struct {
	uint8_t  type;
	uint8_t  flag;
	uint16_t length;
} TL_HEADER;

typedef struct {
	uint16_t track_uid;
	uint8_t  track_state;
	uint8_t  points_num;
	/* 1LSB=1cm of range, or 1LSB=0.01 degree of angle.*/
	int16_t  x;
	int16_t  y;
	int16_t  z;
	/* vx, vy, vz: 1LSB=1cm/s */
	int16_t  vx;
	int16_t  vy;
	int16_t  vz;
} ClusterTrackElm;

typedef struct {
	PointCloud3D    *motion_points;
	PointCloud3D    *presence_points;
	ClusterTrackElm *tracker_objs;
	uint16_t         motion_points_num;
	uint16_t         presence_points_num;
	uint16_t         tracker_objs_num;
} Detection3D_Data;

typedef struct {
	uint32_t  proc_frame_num;
	uint32_t  proc_time_us;
} Detection3D_State;

#if CONFIG_RADAR_FRAMEWORK_REPORT_HEART_BEAT

typedef enum {
    RESPONSE_CNT = 0,
    FRAME_INDEX,
    MOTION_PT_NUM,
    PRESENT_PT_NUM,
	HEART_BEAT_TYPE_MAX
}heartBeat_type;

/**
 * @brief This function set heartbeat message reporting period.
 * 	 	  The default reporting period is 10 minutes.
 * 
 * @param period_s new period for heartbeat message reporting, unit (ms)
 * @return old heartbeat reporting period, unit (ms).
 */
uint32_t radar_framework_heartBeat_period_set(uint32_t period_ms);

/**
 * @brief This function pack heartbeat message and report them via HIF with msgId = 0xFE.
 * 	 	  The default report period of heartbeat message is 10 mins. Developers can call
 * 		  @ref radar_framework_heartBeat_period_set() function to re-set heartbeat report 
 * 		  period.
 * 		  The basic format of heartbeat message is TLV, which contains @ref heartBeat_type data.
 * @param motion_pt_num motion point cloud number, if motion point cloud is not enabled, set to 0.
 * @param presence_pt_num presence point cloud number, if presence point cloud is not enabled, set to 0. 
 */
void radar_framework_report_heartBeat(uint16_t motion_pt_num, uint16_t presence_pt_num);
#endif
/**
 * @brief Prints the current radar framework configuration parameters.
 *
 * This function retrieves the active configuration settings for the radar
 * framework (e.g., frame settings, sensor parameters) and outputs them to
 * the system log for debugging or monitoring purposes.
 */
extern void radar_framework_report_frame_config(void);


/**
 * @brief Reports radar data cube (IQ data) via 0xC2 HIF message.
 *
 * This function encapsulates the radar data cube (complex IQ samples) into a
 * 0xC2 HIF message and transmits it to the host. It handles the construction
 * of the TL header automatically or allows the caller to provide one.
 *
 * @param data Pointer to the `complex16_cube` data to be reported.
 * @param frame_idx The index of the current frame.
 *              - @note The frame index must be incremented for every call to this function.
 *              - If the index is not incremented, the host may generate error messages
 *                indicating sequence misalignment.
 * @param ptr_tl Pointer to a pre-allocated `MMW_FRAME_TL` header structure.
 *              - If set to `NULL`: The function will allocate and fill a private TL
 *                header using standard parameters based on the current frame configuration.
 *                The data size will be derived from the provided cube dimensions.
 *              - If provided: The caller must fill all domain fields except `length`.
 *                The `length` field will be calculated internally based on the data.
 *
 * @return HIF error code (defined as `HIF_ERRCODE_*`).
 *         - Returns `0` on success.
 *         - Returns a non-zero error code if the HIF transmission fails or parameters are invalid.
 */
int radar_framework_report_cube(complex16_cube *data, uint32_t frame_idx, MMW_FRAME_TL* ptr_tl);

#if CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO
void radar_framework_report_frameInfo(uint16_t frame_idx, uint32_t during_time, uint16_t motion_pt_num, uint16_t presence_pt_num);
#endif

#if (CONFIG_MMW_MOTION_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT) || (CONFIG_MMW_PRESENCE_POINT_CLOUD && CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT)
/**
 * @brief Deinitialize and release the point cloud report buffer resources.
 *
 * This function frees the previously allocated report buffer and sets the
 * point cloud report finish flag to indicate that reporting has been completed.
 *
 * @note This function should be called to clean up resources when point cloud
 *       reporting is no longer needed.
 */
int radar_framework_point_cloud_report_init(void);

/**
 * @brief Deinitialize and release the point cloud report buffer resources.
 *
 * This function frees the previously allocated report buffer and sets the
 * point cloud report finish flag to indicate that reporting has been completed.
 *
 * @note This function should be called to clean up resources when point cloud
 *       reporting is no longer needed.
 */
void radar_framework_point_cloud_report_deinit(void);

/**
 * @brief Reports radar point could reults (x/cm,y/cm,z/cm,vel/cm/s,snr/0.01dB) via 0xC3 HIF message.
 *
 * This function encapsulates the point could reults (x/cm,y/cm,z/cm,vel/cm/s,snr/0.01dB) into a
 * 0xC3 HIF message and transmits it to the host. It handles the construction
 * of the TL header automatically or allows the caller to provide one.
 *
 * @param ptr_point_cloud_buffer The pointer for storing the calculation results of motion point cloud data.
 * @param ptr_presence_point_cloud_buffer The pointer for storing the calculation results of presence point cloud data.
 * @param frame_idx The index of the current frame.
 *              - @note The frame index must be incremented for every call to this function.
 *              - If the index is not incremented, the host may generate error messages
 *                indicating sequence misalignment.
 *
 * @return HIF error code (defined as `HIF_ERRCODE_*`).
 *         - Returns `0` on success.
 *         - Returns a non-zero error code if the HIF transmission fails or parameters are invalid.
 * */
int radar_framework_report_point_cloud_data(const PointCloudBuffer_t *ptr_point_cloud_buffer, const PresencePointCloudBuffer_t *ptr_presence_point_cloud_buffer, uint32_t frame_idx);
#endif

#endif  /* __RADRA_FRAMEWORK_REPORT_H */