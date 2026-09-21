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


#ifndef __RADAR_FRAMEWORK_INTERVAL0_REPROT__H
#define __RADAR_FRAMEWORK_INTERVAL0_REPROT__H

#if CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN

/**
 * Enable this macro to enable report Interval0 range fft data.
 * */
#ifndef CONFIG_RADAR_FRAMWORK_REPORT_INTERVAL0_RFFT
#define	CONFIG_RADAR_FRAMWORK_REPORT_INTERVAL0_RFFT			(1)
#endif

/** Heap size(Bytes) for save 1d-FFT data is:
 *			range_fft_len * RX_NUM * TX_NUM * 4;
 *  Interval0 rfft use ping-pong struct, so heap is: 2 * range_fft_len * RX_NUM * TX_NUM * 4;
 * */
#ifndef HEAP_SIZE_1D_CUBE
#define HEAP_SIZE_1D_CUBE									(2 * CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN * RADAR_FRAMEWORK_MIMO_RX_NUM * 4)
#endif

#else
#undef  HEAP_SIZE_1D_CUBE
#define HEAP_SIZE_1D_CUBE									(1024 * 0)
#endif	/* #if CONFIG_RADAR_FRAMEWORK_INTERVAL0_RFFT_EN */

#endif

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
