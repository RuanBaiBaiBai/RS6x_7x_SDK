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


#ifndef _PRJ_CONFIG_H
#define _PRJ_CONFIG_H

#define CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN						0
#define CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN						1
/* calculate and motion and presence point cloud */
#define CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE               0
#define CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN                1
#define CONFIG_MMW_MOTION_POINT_CLOUD						    1
#define CONFIG_MMW_PRESENCE_POINT_CLOUD					  	    1

#define CONFIG_MMW_DRIVER_TEMP_MGMT                             1
#define CONFIG_MMW_LOOP_TASK_MONITOR                            1
#define CONFIG_MMW_WATCHDOG                                     1

#define CONFIG_RADAR_FRAMEWORK_FT								1
/* For more details, please refer to 'radar_framework_default_config.h' */

/*
 * Individual configurations for frame config and application configure for each project
 * */

/* RS6240 Support 57G~66G, RS6130 Support 58G~64G */
#if CONFIG_BOARD_MRS6130_P1806 || CONFIG_BOARD_MRS6130_P1812
#define RADAR_FRAMEWORK_SAMPLE_START_FREQ_MHZ					(58880)
#define RADAR_FRAMEWORK_SAMPLE_MIMO_MODE						(MMW_MIMO_1T3R)
#elif CONFIG_BOARD_MRS6240_P2512_CPUF || CONFIG_BOARD_MRS6240_P2512_CPUS || CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUF || CONFIG_BOARD_MRS7241_P2840_CPUF || CONFIG_BOARD_MRS7241_P2828_CPUF
#define RADAR_FRAMEWORK_SAMPLE_START_FREQ_MHZ					(58880)
#define RADAR_FRAMEWORK_SAMPLE_MIMO_MODE						(MMW_MIMO_2T4R)
#else
	#error "Please Slect Proper SOC!"
#endif

/*
 * The frame type used can be set as 0/1.
 * When configured as 0,it indicates the use of 1D frame mode,only one interval is sent for each frame.
 * When configured as 1,it indicates the use of 2D frame mode,based on the doppler points,multiple intervals are sent in each frame.
 */
#define RADAR_FRAMEWORK_SAMPLE_FRAME_TYPE						(1)
/* Range fft length = 1 << RADAR_FRAMEWORK_SAMPLE_RFFT_LEN_LOG2 */
#define RADAR_FRAMEWORK_SAMPLE_RFFT_LEN_LOG2					(8)
#define RADAR_FRAMEWORK_SAMPLE_RANGE_RESOLUTION_MM				(40)

/*
 * Doppler fft length = 1<<RADAR_FRAMEWORK_SAMPLE_DFFT_LEN_LOG2,
 * Doppler measurement range is [-Doppler fft length / 2 * RADAR_FRAMEWORK_SAMPLE_VEL_RESOLUTION_MM, Doppler fft length / 2 * RADAR_FRAMEWORK_SAMPLE_VEL_RESOLUTION_MM)
 */
#define RADAR_FRAMEWORK_SAMPLE_DFFT_LEN_LOG2					(5)
#define RADAR_FRAMEWORK_SAMPLE_VEL_RESOLUTION_MM				(200)

#if CONFIG_BOARD_MRS6130_P1812 || CONFIG_BOARD_MRS6240_P2512_CPUF || CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUF
#define RADAR_FRAMEWORK_SAMPLE_FRAME_PERIOD_MS					(50)
#else
	#error "Sleep Sample only support MRS6130_P1812, MRS6240_P2512 and MRS6241_P2828_M62!"
#endif

/* Acc number in one interval is 1<<RADAR_FRAMEWORK_SAMPLE_ACC_NUM_LOG2 */
#define RADAR_FRAMEWORK_SAMPLE_ACC_NUM_LOG2						(0)

/* Clutter halt method is keep udate*/
#define RADAR_FRAMEWORK_SAMPLE_CLUTTER_HALT_METHOD				(CLUTTER_KEEP_UPDATE)

/* Sleep project default param define */
#define CONFIG_MMW_POINT_CLOUD_BB_CLUTTER_RM 			(MMW_CLUTTER_REMOVAL_NONE)

#define	CONFIG_MMW_MOTION_CFAR_TH_LIN					(31.6228f) /* default 15dB, 15dB = 10log10(31.6228) */

#define CONFIG_MMW_STATIC_RM_EN							(MMW_ENABLE)

/* Point cloud is only enabled in RADAR_FRAMEWORK_2DFFT_MODE */
#define CONFIG_RADAR_FRAMEWORK_FFT_MODE					(RADAR_FRAMEWORK_2DFFT_MODE)

/* HIF configuration defined in file radar_framework_default_config.h */

#include "radar_framework_default_config.h"

#endif /* _PRJ_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
