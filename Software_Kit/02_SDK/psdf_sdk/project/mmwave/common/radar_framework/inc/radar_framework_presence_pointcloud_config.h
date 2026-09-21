/**
 **************************************************************************************************
 * @brief
 * Radar framework function configurations file.
 * @attention
 *    This is the default configuration file, and may be override in prj_config.h in each project ./mmw_application/
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


#ifndef __RADAR_FRAMEWORK_PRESENCE_POINTCLOUD_CONFIG_H
#define __RADAR_FRAMEWORK_PRESENCE_POINTCLOUD_CONFIG_H

 /**
  * In subfunction macro 'CONFIG_MMW_MOTION_POINT_CLOUD', user can configure following macros:
  *
  *   - CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD (default 1, MICRO_PROCESS_CFAR_METHOD)
  *		See 'mmw_app_micro_pointcloud.h' for details. 
  * 
  *   - CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE (default 0, MICRO_CFAR_MODE_SINGLE_SIDE)
  * 	MICRO_CFAR_MODE_SINGLE_SIDE will cost less heap memory and faster computation time, which is recommanded.
  * 	The value of this macro take effect only in CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD is 1.
  *     See 'mmw_app_micro_pointcloud.h' for details.
  *
  *   - CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC (default 2)
  * 	Presence point cloud calcuate and buffer datacube every CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC bins.
  * 	Set this CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC to 1, use all datacube to calculate presence points.
  * 	Set this macro not equal to 1, can greatly reduce memory usage, it's recommand to set this macro to 2 in
  * 	tranditional application.
  * 	See 'HEAP_SIZE_PRESENCE' to see how 'CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC' can influence heap size.
  *
  *   - CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2 (default 3)
  * 	Presence will use '1 << CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2' chirps to do micro-doppler fft. More chirps will increase
  * 	snr of presence point, but more memory usage. It's recommand to keep this macro default to 8.
  * 	See 'HEAP_SIZE_PRESENCE' to see how '1 << CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2' can influence heap size.
  *
  *   - CONFIG_MMW_PRESENCE_RANGE_BIN_NUM
  * 	Config maximun presence range index seperately,
  *
  *
  *   - CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT  (default 1)
  *       Disable it when there is no need to report motion point cloud via psic debug protocol.
  */
#if CONFIG_MMW_PRESENCE_POINT_CLOUD

#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD
#define CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD            1   /* default cfar method */
#endif

#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == 1
#ifndef CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE
#define CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE                        1   /* default MICRO_CFAR_MODE_SINGLE_SIDE mode */
#endif
#endif

/* Config the maximun number of point clouds.if the number of point cloud results exceedsthe value,the subsequent target will be discarded. */
#ifndef CONFIG_MMW_PRESENCE_POINT_MAX
#define CONFIG_MMW_PRESENCE_POINT_MAX								256
#endif

/**
 * Each presence point cloud is stored in a size of 24 bits(refer to structure PresencePointCloudBuffer_t)
 * and the memory required to store the point cloud results is the maximun number of point
 * clouds mulitplied by the size of each point cloud.
 */
#ifndef HEAP_SIZE_PRESENCE_CALCULATION_RESULT
#define HEAP_SIZE_PRESENCE_CALCULATION_RESULT		(CONFIG_MMW_PRESENCE_POINT_MAX * 24)
#endif

/* The following CONFIG_MMW_xxx configurations are override default configurations
 * in subsys/mmw_application/mmw_app_pointcloud_config.h.
 * The decide the heap used for presence point cloud. For detail, peleas refer to
 * 'subsys/mmw_application/mmw_app_pointcloud_config.h.' or 'RS6x_7x_PointCloud_Demo_manual'
 * for details.
 */
#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC
#define CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC						(2)
#endif

/**
  * Default use CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE = 0, need basic 37(max win_len) * 4 Byte heap
  * If CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == 1, need basic 73(max win_len) * 4 Byte heap
  * */
#if CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == 0
  #define HEAP_SIZE_PRESENCE_BASE                                ((37 / CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC + 1) * 4)
#elif CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE == 1
  #define HEAP_SIZE_PRESENCE_BASE                             	 ((73 / CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC + 1) * 4)
#else
  #error "CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE must be 0 or 1"
#endif    /* CONFIG_MMW_MICRO_POINT_CLOUD_CFAR_MODE */

#ifndef CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2
#define CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2									(3)
#endif

#ifndef CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2
#define CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2					(4)
#endif

#ifndef CONFIG_MMW_PRESENCE_RANGE_BIN_NUM
#define CONFIG_MMW_PRESENCE_RANGE_BIN_NUM             (256)
#endif

#ifndef CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT
#define CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT				(1)
#endif

/* Each presence point cloud report requires 12 bits(refer to structure PointCloud_Cart) */
#if CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT
  #define HEAP_SIZE_PRESENCE_REPORT					(CONFIG_MMW_PRESENCE_POINT_MAX * 12)
#else
  #define HEAP_SIZE_PRESENCE_REPORT					(0)
#endif    /* CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT */

#define PRESENCE_DOP_FFT_LEN						 (1 << CONFIG_MMW_PRESENCE_DFFT_LEN_LOG2)
#define PRESENCE_INTERVAL_NUM                        (1 << CONFIG_MMW_PRESENCE_INTERVAL_NUM_LOG2)
#define PRESENCE_RANGE_BIN_NUM_DECM                  (CONFIG_MMW_PRESENCE_RANGE_BIN_NUM / CONFIG_MMW_PRESENCE_POINT_CLOUD_RANGE_DEC + 1)
#define HEAP_SIZE_PRESENCE_CUBE                      (PRESENCE_RANGE_BIN_NUM_DECM * RADAR_FRAMEWORK_MIMO_RX_NUM * (PRESENCE_INTERVAL_NUM + 1) * 4)
#define HEAP_SIZE_PRESENCE_CHIRP                     (RADAR_FRAMEWORK_MIMO_RX_NUM * (PRESENCE_INTERVAL_NUM + 1) * 4)
#if CONFIG_MMW_PRESENCE_POINT_CLOUD_PROCESS_METHOD == 1
#define HEAP_SIZE_PRESENCE_STORE                     ((PRESENCE_DOP_FFT_LEN * RADAR_FRAMEWORK_MIMO_RX_NUM * HEAP_SIZE_PRESENCE_BASE) + ((2 * PRESENCE_DOP_FFT_LEN + 1)) * (PRESENCE_RANGE_BIN_NUM_DECM * 4))
#else
#define HEAP_SIZE_PRESENCE_STORE                     (4 * 3 * PRESENCE_DOP_FFT_LEN * RADAR_FRAMEWORK_MIMO_RX_NUM)
#endif
#define HEAP_SIZE_PRESENCE							 ((HEAP_SIZE_PRESENCE_CALCULATION_RESULT) + (HEAP_SIZE_PRESENCE_CUBE) + (HEAP_SIZE_PRESENCE_CHIRP) + (HEAP_SIZE_PRESENCE_STORE) + HEAP_SIZE_PRESENCE_REPORT)

/****************** CHECKPOINT OF CONFIG_MMW_PRESENCE_POINT_CLOUD ******************/
#if CONFIG_MMW_PRESENCE_RANGE_BIN_NUM > CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN
#error "CONFIG_MMW_PRESENCE_RANGE_BIN_NUM is grater than CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN"
#endif

#else
#define HEAP_SIZE_PRESENCE										(1024 * 0)
#undef CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT
#define CONFIG_RADAR_FRAMEWORK_PRESENCE_POINT_CLOUD_REPORT			(0)
#endif  /* CONFIG_MMW_PRESENCE_POINT_CLOUD */

#endif /* __RADAR_FRAMEWORK_PRESENCE_POINTCLOUD_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
