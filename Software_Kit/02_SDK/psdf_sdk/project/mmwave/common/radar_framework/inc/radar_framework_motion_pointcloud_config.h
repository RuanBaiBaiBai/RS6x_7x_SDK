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

#ifndef __RADAR_FRAMEWORK_MOTION_POINTCLOUD_CONFIG_H
#define __RADAR_FRAMEWORK_MOTION_POINTCLOUD_CONFIG_H

 /**
  * In subfunction macro 'CONFIG_MMW_MOTION_POINT_CLOUD', user can configure following macros:
  *
  *   - CONFIG_MMW_SW_CFAR_ENABLE (default 0)
  *       Enable software CFAR or not. Software CFAR use ca-cfar method, it will output less density point cloud than Hardware CFAR.
  *       The software CFAR cost typical 1.8ms(range-fft: 256, doppler-fft: 32, ant:8) in RS6240.
  *       This macro is recommanded disabled. It's usually enabled at the following condition:
  *         1. When user has make sure that cfar results read from hardware have too many clutters;
  *         2. When user wants to do software CFAR using own cfar algorithm, software CFAR offers highly-optimized interface.
  *   @note: When enabling 'CONFIG_MMW_SW_CFAR_ENABLE', 'CONFIG_MMW_FFT_AUTOGAIN_EN' must be disabled,
  * 		because framework has not adjust software cfar in autogain mode.
  *
  *   - CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT  (default 1)
  *       Enable Upload motion point cloud by psic protocol (MsgID = 0xC3),
  * 	  If user wants to use own process, it's better to disable macro 'CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT'
  */
#if CONFIG_MMW_MOTION_POINT_CLOUD
/**
 * Configure the maximum number of points to read from the CFAR list using the macro
 * 'CONFIG_MMW_MAX_MOTION_CFAR_NUM'. When computational load is excessive or SRAM is
 * insufficient, it is recommended to reduce this value for faster computation speed.
 * If the actual number of CFAR points exceeds the value set by this macro, the point
 * cloud computation subsystem will use the first CONFIG_MMW_MAX_MOTION_CFAR_NUM points
 * for subsequent processing.
 */
#ifndef CONFIG_MMW_MAX_MOTION_CFAR_NUM
#define CONFIG_MMW_MAX_MOTION_CFAR_NUM					(512)
#endif

#ifndef CONFIG_MMW_SW_CFAR_ENABLE
#define CONFIG_MMW_SW_CFAR_ENABLE										0
#endif

#if CONFIG_MMW_SW_CFAR_ENABLE && CONFIG_MMW_FFT_AUTOGAIN_EN
#error "When Enable CONFIG_MMW_SW_CFAR_ENABLE, CONFIG_MMW_FFT_AUTOGAIN_EN must set to 0"
#endif

#ifndef CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT
#define CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT				(1)
#endif

/* The HW_CFAR can supoort up to 512 points, and each CFAR result requires 4 bytes */
#define HEAP_SIZE_MOTION_HW_CFAR										(512 * 4)
/* When the number of HW_CFAR results reaches 512, SW_CFAR needs to be performed, and its maximum quantity is the same as CONFIG_MMW_MAX_MOTION_CFAR_NUM */
#define HEAP_SIZE_MOTION_SW_CFAR										(CONFIG_MMW_MAX_MOTION_CFAR_NUM * 4)
#define HEAP_SIZE_MOTION_CFAR											(HEAP_SIZE_MOTION_HW_CFAR + HEAP_SIZE_MOTION_SW_CFAR)

/**
 * Each point cloud is stored in a size of 24 bits(refer to structure PointCloudBuffer_t)
 * and the memory required to store the point cloud results is the maximun number of point
 * clouds mulitplied by the size of each point cloud.
 */
#define HEAP_SIZE_MOTION_HEAP_SIZE_MOTION_CALCULATION_RESULT		(CONFIG_MMW_MAX_MOTION_CFAR_NUM * 24)

/* when used low-speed filtering, 1K of memory is required */
#define HEAP_SIZE_MOTION_CALCULATION                                (HEAP_SIZE_MOTION_CFAR + HEAP_SIZE_MOTION_HEAP_SIZE_MOTION_CALCULATION_RESULT + CONFIG_MMW_MAX_MOTION_CFAR_NUM)

#if CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT
  /* Each point cloud report requires 12 bits(refer to structure PointCloud_Cart) */
  #define HEAP_SIZE_MOTION_REPORT                                		(CONFIG_MMW_MAX_MOTION_CFAR_NUM * 12)
#else
  #define HEAP_SIZE_MOTION_REPORT                                		(1024 * 0)
#endif  /* CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT */

/* MOTION POINT needs SRAM equal HEAP_SIZE_MOTION_REPORT + HEAP_SIZE_MOTION_CALCULATION */
#define HEAP_SIZE_MOTION                                			(HEAP_SIZE_MOTION_CALCULATION + HEAP_SIZE_MOTION_REPORT + CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN * 8)
#else
#define HEAP_SIZE_MOTION                                			(0)
#undef CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT
#define CONFIG_RADAR_FRAMEWORK_MOTION_POINT_CLOUD_REPORT			(0)
#endif  /* CONFIG_MMW_MOTION_POINT_CLOUD */

#endif /* __RADAR_FRAMEWORK_MOTION_POINTCLOUD_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
