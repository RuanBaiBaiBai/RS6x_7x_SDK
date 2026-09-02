/**
 **************************************************************************************************
 * @brief
 * Radar framework function configurations file.
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


#ifndef __RADAR_FRAMEWORK_2D_FRAME_CONFIG_H
#define __RADAR_FRAMEWORK_2D_FRAME_CONFIG_H


#if (CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN == 0)
  /* These configures are fixed. */
  #undef CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE
  #undef CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN
  #undef CONFIG_MMW_MOTION_POINT_CLOUD
  #undef CONFIG_MMW_PRESENCE_POINT_CLOUD

  #define CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE                 0
  #define CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN              	0
  #define CONFIG_MMW_MOTION_POINT_CLOUD						 		0
  #define CONFIG_MMW_PRESENCE_POINT_CLOUD					  		0
#else
/**
  * Enable Report 2D-FFT DataCube while configured in 2D-FFT Mode.
  * The typical time cost for reporting in SPI is 85ms while reporting 256K datacube.
  * DataCube size is calculated as following:
  * 			range fft length * doppler fft length * tx_num * rx_num * 4Bytes
  */
#ifndef CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE
#define CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE                 (0)
#endif

/**
  * Enable Upload range-FFT data of interval 1 in 2D-FFT Mode.
  * Once enable this 'CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN', hardware will save range-fft data of interval 0 in buffer and report(Optional).
  * This macro is usually used in heart beat and breath detection application, along with motion point reporting.
  * If this macro is enabled, extra frame period will be added.
  */
#ifndef CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN
#define CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN                (0)
#endif

/**
  * Enable motion point cloud, motion point cloud can detect moving target or static targets(according to config).
  * In worst case(calcuate full 768 points), motion point cloud may cost 28ms.
  * It is recommanded to disable motion point cloud when user wants to report datacube, it will increase the frame rate
  * in datacube reporting application.
  */
#ifndef CONFIG_MMW_MOTION_POINT_CLOUD
#define CONFIG_MMW_MOTION_POINT_CLOUD						(0)
#endif

/**
  * Enable presence point cloud. Different from motion point cloud, presence point cloud is capable of capturing motions
  * impreceptible to human eye, for example, a person sitting still, sleeping or the subtle movement of chest breathing.
  * Enabling this feature will incur additional computational time and memory overhead.
  */
#ifndef CONFIG_MMW_PRESENCE_POINT_CLOUD
#define CONFIG_MMW_PRESENCE_POINT_CLOUD						(0)
#endif

#endif	/* #if CONFIG_RADAR_FRAMEWORK_2D_FRAME_EN == 0 */

#endif

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
