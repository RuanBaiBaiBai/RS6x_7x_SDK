/**
 **************************************************************************************************
 * @file    radar_signal_process_1d.h
 * @brief   Header file for 1D Radar Signal Processing and Data Cube handling.
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

 #ifndef __RADAR_SIGNAL_PROCESS_1D_H
 #define __RADAR_SIGNAL_PROCESS_1D_H
 
 /**
  * @brief Callback function invoked when radar signal processing completes for a 1D frame.
  * 
  * This function is triggered by the radar framework after the 1D FFT processing (typically Range FFT)
  * is finished for a specific frame. It allows the application layer to receive the processed
  * data cube or intermediate results.
  * 
  * This function can be registered via mmw_ctrl_callback_cfg() function.
  * 
  * @param mmw_data Pointer to the processed radar data structure.
  *                 In 1D mode, this typically points to a buffer containing complex IQ samples
  *                 organized by range bins (e.g., `complex16_cube` or similar structure).
  * @param arg      Additional argument passed by the caller. This can be used to pass context,
  *                 application-specific state, or pointers to user-defined structures.
  * 
  * @return int
  *         - Returns 0 on successful processing.
  *         - Returns a non-zero error code if data validation fails or processing encounters an error.
  * 
  * @note The application must ensure that the `mmw_data` buffer remains valid during the callback execution.
  */
 extern int radar_framework_1d_frame_cb(void *mmw_data, void *arg);
 
 /**
  * @brief Initialize the 1D signal processing application module.
  * 
  * This function sets up the necessary resources, allocates memory, and configures the
  * 1D signal processing pipeline required for handling radar data. It should be called
  * before starting the radar sensor and after frame type and baseband has been configured.
  * 
  * @return int
  *         - Returns 0 if initialization is successful.
  *         - Returns a negative error code if initialization fails (e.g., memory allocation failure,
  *           hardware configuration error).
  * 
  * @sa radar_signal_process_1d_app_deinit()
  */
 extern int radar_signal_process_1d_app_init(void);
 
 /**
  * @brief Deinitialize the 1D signal processing application module.
  * 
  * This function cleans up resources allocated during `radar_signal_process_1d_app_init()`.
  * It frees memory, and resets internal states.
  * 
  * @note This function should be called when the radar application is shutting down to prevent
  *       resource leaks or invalid memory access.
  * 
  * @return int
  *         - Returns 0 if deinitialization is successful.
  *         - Returns a non-zero error code if cleanup fails.
  * 
  * @sa radar_signal_process_1d_app_init()
  */
 extern int radar_signal_process_1d_app_deinit(void);
 
 #endif /* __RADAR_SIGNAL_PROCESS_1D_H */
 
 /*
  **************************************************************************************************
  * (C) COPYRIGHT POSSUMIC TECHNOLOGY
  * END OF FILE
  */