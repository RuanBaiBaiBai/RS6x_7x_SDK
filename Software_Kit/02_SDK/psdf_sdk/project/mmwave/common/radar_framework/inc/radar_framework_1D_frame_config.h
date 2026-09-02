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


#ifndef __RADAR_FRAMEWORK_1D_FRAME_CONFIG_H
#define __RADAR_FRAMEWORK_1D_FRAME_CONFIG_H

#if CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN
/**
 * Once enable CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE, radar framework will report 1D DataCube
 * If developer wants to handle 1D-DataCube by their own, can disable this macro.
 * */
#ifndef CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE
#define CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE						(1)
#endif

#define HEAP_DC_CLUTTER_REMOVAL											(20 * CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN)

#if CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE

#ifndef HEAP_SIZE_1D_FRAME_CUBE
#define HEAP_SIZE_1D_FRAME_CUBE									        (HEAP_DC_CLUTTER_REMOVAL)
#endif
#else
/**
 * If CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE is not enabled, callback of 1D frame only read 1D-Cube, this buffer is used
 * to store 1D-Cube.
 */
#ifndef HEAP_SIZE_1D_FRAME_CUBE
#define HEAP_SIZE_1D_FRAME_CUBE									((CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN) * (RADAR_FRAMEWORK_MIMO_RX_NUM) * 4 + (HEAP_DC_CLUTTER_REMOVAL))
#endif
#endif	/* CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE */

#else
#undef HEAP_SIZE_1D_FRAME_CUBE
#define HEAP_SIZE_1D_FRAME_CUBE											(0)
#endif	/* CONFIG_RADAR_FRAMEWORK_1D_FRAME_EN */

#endif /* __RADAR_FRAMEWORK_1D_FRAME_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
