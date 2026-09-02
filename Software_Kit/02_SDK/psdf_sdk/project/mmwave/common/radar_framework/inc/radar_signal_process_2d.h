/**
 * @file
 * @brief 2D Radar Signal Processing Framework Header
 *
 * @attention
 * Copyright (C) 2025 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the name of POSSUMIC TECHNOLOGY CO., LTD. nor the names of
 *      its contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @brief Core components for 2D radar signal processing framework
 * @{
 */

 #ifndef __RADAR_SIGNAL_PROCESS_2D_H
 #define __RADAR_SIGNAL_PROCESS_2D_H
 
 #include "mmw_type.h"

 /**
  * @brief   2D Frame Mode Callback Function
  *
  * Callback function triggered when a 2D radar frame is processed.
  * This function allows the application to handle radar data after
  * signal processing is complete.
  *
  * @param mmw_data  Pointer to MMW radar data structure containing
  *                  processed frame information
  * @param arg       Optional user-defined argument passed during
  *                  callback registration
  *
  * @return          0 on success, negative value on error
  *
  * @note            This function must be implemented by the application
  *                  layer to process radar frames in 2D mode
  */
 extern int radar_framework_2d_frame_cb(void *mmw_data, void *arg);
 
 /**
  * @brief Initialize the 2D Radar Signal Processing Application
  *
  * Sets up all necessary resources, buffers, and internal state
  * required for 2D radar signal processing operations.
  *
  * @return 0 on successful initialization, negative error code otherwise
  *
  * @note This function should be called before any radar processing
  *       operations. All resources will be cleaned up by the
  *       corresponding @ref radar_signal_process_2d_app_deinit() function.
  *
  * @sa radar_signal_process_2d_app_deinit
  */
 int radar_signal_process_2d_app_init(void);
 
 /**
  * @brief   Deinitialize the 2D Radar Signal Processing Application
  *
  * Releases all resources allocated during initialization, including
  * memory buffers, handles, and internal state.
  *
  * @return 0 on successful deinitialization, negative error code otherwise
  *
  * @note Should be called when the radar processing module is no longer
  *       needed. This function must be called after @ref radar_signal_process_2d_app_init()
  *       and before application termination.
  *
  * @sa radar_signal_process_2d_app_init
  */
 int radar_signal_process_2d_app_deinit(void);
 
 #endif /* __RADAR_SIGNAL_PROCESS_2D_H */