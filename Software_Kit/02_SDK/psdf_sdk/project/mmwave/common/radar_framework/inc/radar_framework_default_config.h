/**
 **************************************************************************************************
 * @brief
 * Radar framework function configurations file.
 * Global configurations:
 * 		1. CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO
 * 		   Use PSIC debug protocol to upload frame config, heap size left and calcuation time vs point num.
 * 		2. CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN
 * 		   Configure max range fft length radar framework may support, this parameter desice the heap size that is needed.
 *   3. Global disable 1D or 2D frame struct,
 *       see macro 'CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN' and 'CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN'.
 *
 * Top module function switches:
 * 		1. Upload 1D datacube in 1D frame struct,
 *       see macro 'CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN';
 * 		2. Upload 2D datacube in 2D frame struct,
 *       see macro 'CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE';
 * 		3. Upload RFFT result of interval 0 in 2D frame struct,
 *       see macro 'CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN';
 * 		4. Calculate and report motion points in 2D frame struct,
 *       see macro 'CONFIG_MMW_MOTION_POINT_CLOUD';
 * 		5. Calculate and report presence points in 2D frame struct,
 *       see macro 'CONFIG_MMW_PRESENCE_POINT_CLOUD';
 *
 * For detail configurations for each top module, please search each macro configuration.
 *
 * Default configurations of radar framework is:
 *    1. Global disable 1D or 2D frame struct:
 *       ENABLE both 1D frame struct and 2D frame struct.
 * 		2. Upload 1D datacube in 1D frame struct:
 *       DISABLED
 * 		3. Upload 2D datacube in 2D frame struct:
 *       DISABLED
 * 		4. Upload RFFT result of interval 0 in 2D frame struct,
 *       DISABLED
 * 		5. Calculate and report motion points,
 *       ENABLED
 * 		6. Calculate and report presence points,
 *       ENABLED
 *
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


#ifndef __RADAR_FRAMEWORK_DEFAULT_CONFIG_H
#define __RADAR_FRAMEWORK_DEFAULT_CONFIG_H

 /**
  * TOP function configure, global enabled 1D/2D frame truct or not.
  *
  * If 'CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN' is set to 0, radar framework will not COMPLIE code related to 1d frame struct
  * to reduce code size usage in radar framework.
  *
  * The only purpose of this macro is only to REDUCE CODE SIZE!
  *
  * @note: Even if 'CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN' is set to 1, radar framework can emit 1d frame struct due to 'RadarConfig_t::frame_type'
  * domain.
  *
  * 'CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN' works same as 'CONFIG_RADAR_RAMEWORK_1D_FRAME_EN'
  */
#ifndef CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
#define CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN           (1)
#endif
#ifndef CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN
#define CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN           (1)
#endif

/**
 * Gloabl max range fft number that radar framework support.
 * Maximun heap size is associated with max range fft length.
 * Range fft value is checked when call  radar_signal_process_1d_app_init/radar_signal_process_2d_app_init
 * function.
 * It's strongly recommanded configura this macro as small as application use, which may reduce much heap size.
 * */
#ifndef CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN
#define CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN         (256)
#endif

/**
 * Report frame info via PSIC debug protocol to indicate waveform configuration.
 * This macro shows how to use PSIC debug protocol to report debug information.
 *
 * @note It's recommanded to disable 'CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO' in 1D frame struct,
 * 		 because 1D frame usually use high frame rate, PSIC debug protocol is not designed for
 * 		 high performance reporting but for debuging only.
 * */
#ifndef CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO
#define CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO			(1)
#endif

/**
 * Report heartbeat message via HIF protocol with MsgId = 0xFE
 * This is useful when in low power mode, which may has no HIF message for a long time,
 * developer can use heartbeat message to check radar is still alive.
 * Defualt heartbeat reporting period is 10 mins.
 *
 * */
#ifndef CONFIG_RADAR_FRAMEWORK_REPORT_HEART_BEAT
#define CONFIG_RADAR_FRAMEWORK_REPORT_HEART_BEAT            (0)
#endif

/*
 * It's recommanded to not use auto (fft) gain feature when directly use data cube or is not familier with auto gain, which is
 * an advanced feature and will cost extra memory and calculate time and in has no benifit most condition.
 *
 * Use auto gain in 1D frame struct is not support yet, if developer wants to learn more about auto gain, please refer to:
 * /subsys/mmw/mmw_algorithm/mmw_point_cloud_psic_lib.h
 * */
#ifndef CONFIG_MMW_FFT_AUTOGAIN_EN
#define CONFIG_MMW_FFT_AUTOGAIN_EN				0
#endif

/*
 * Board related configurations to determin max mimo_rx_num
 * */
#if CONFIG_BOARD_MRS6130_P1812 ||  CONFIG_BOARD_MRS6130_P1806
#define RADAR_FRAMEWORK_MIMO_RX_NUM									(3)
#elif CONFIG_BOARD_MRS6240_P2512_CPUF || CONFIG_BOARD_MRS6240_P2512_CPUS || CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUF || CONFIG_BOARD_MRS7241_P2840_CPUF || CONFIG_BOARD_MRS7241_P2828_CPUF
#define RADAR_FRAMEWORK_MIMO_RX_NUM									(8)
#else
#error "please select valid board!"
#endif

#include "radar_framework_1D_frame_config.h"
#include "radar_framework_2D_frame_config.h"
#include "radar_framework_Interval0_report_config.h"
#include "radar_framework_motion_pointcloud_config.h"
#include "radar_framework_presence_pointcloud_config.h"
#include "radar_framework_hif_config.h"
#include "radar_framework_soc_config.h"

/*
 * HEAP_SIZE_BASIC is os & hif needed.
 * Disable all features in this .h file but leave 'CONFIG_RADAR_FRAMEWORK_REPORT_FRAME_INFO' on in 2d radar frame to check basic heap size.
 * */
#define HEAP_SIZE_BASIC										(1024 * 15) /* 15KB */
#define HEAP_SIZE_2D_FRAME									((HEAP_DATACUBE_REPORT_POOL) + (HEAP_SIZE_PRESENCE) + (HEAP_SIZE_1D_CUBE) + (HEAP_SIZE_MOTION))
#define HEAP_SIZE_1D_FRAME									((HEAP_DATACUBE_REPORT_POOL) + (HEAP_SIZE_1D_FRAME_CUBE))
#if (HEAP_SIZE_1D_FRAME) > (HEAP_SIZE_2D_FRAME)
#define CONFIG_HEAP_SIZE 									((HEAP_SIZE_1D_FRAME) + (HEAP_SIZE_BASIC) + (HEAP_HIF))
#else
#define CONFIG_HEAP_SIZE 									((HEAP_SIZE_2D_FRAME) + (HEAP_SIZE_BASIC) + (HEAP_HIF))
#endif

#if (CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN == 0) && (CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN == 0)
#error "Must select at least one frame struct!"
#endif

#if (CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN || CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE || CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN) && CONFIG_MMW_FFT_AUTOGAIN_EN
	#error "CONFIG_MMW_FFT_AUTOGAIN_EN must be 0 when report any datacube."
#endif

#endif /* __RADAR_FRAMEWORK_DEFAULT_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
