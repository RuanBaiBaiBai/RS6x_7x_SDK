/**
  **********************************************************************
  * @file      radar_framework.h
  * @brief     Radar Framework Header - Configuration and Management APIs
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
  **********************************************************************
  */

  #ifndef __RADAR_FRAMEWORK_H__
  #define __RADAR_FRAMEWORK_H__

  #ifdef __cplusplus
  extern "C" {
  #endif

  #include <common.h>
  #include "mmw_ctrl.h"
  #include "hif.h"
  #include "mmw_alg_pointcloud_typedef.h"

  /*
   * SECTION: CONFIG
   * Placeholders for specific configuration sections.
   */
  /* BB CONFIG */
  /* POINT CLOUD CONFIG */
  /* TRAJ CONFIG */
  /* UPLOAD CONFIG */

  /**
   * @brief Clutter halting methods for radar framework.
   */
  typedef enum {
	  CLUTTER_KEEP_UPDATE,        ///< Continuously update clutter map.
	  CLUTTER_HALT_AFTER_CONVERGENCE,  ///< Stop updating clutter map after it has converged.
	  CLUTTER_AUTO_UPDATE         ///< Automatically manage clutter update strategy.
  } RadarFrameworkClutterHalMethod_e;

  typedef enum {
    RADAR_FRAMEWORK_ADC_MODE = 0,     ///< ADC mode, adc data is stored in datacube
    RADAR_FRAMEWORK_RFFT_MODE,        ///< Range-fft mode, doppler fft is bypassed
    RADAR_FRAMEWORK_2DFFT_MODE,       ///< Range-fft + doppler-fft mode in 2D frame, in 1D frame struct, doppler-fft will not be done
    RADAR_FRAMEWORK_FFT_MODE_MAX
  } RadarDrameworkFFTMode_e;

  /**
   * @brief Configuration for 1D or 2D radar frames.
   */
  typedef struct {
	  uint8_t mimo_mode;                  ///< MIMO antenna mode. See constants MMW_MIMO_xx in mmw_ctrl.h.
	  uint8_t frame_type;                 ///< Frame type: 0 for 1D frame, 1 for 2D frame.
	  uint32_t start_freq_mhz;            ///< Radar start frequency in MHz (1 LSB = 1 MHz).

	  uint32_t range_resolution_mm;       ///< Range resolution in mm (1 LSB = 1 mm).

	  /**
	   * @brief Log2 of the range FFT length.
	   *
	   * The maximum detection range is calculated as: `(1 << range_fft_len_log2) * range_resolution_mm`.
	   * Maximum value for this parameter is 10 (1024 range FFT points) with auto gain OFF, 9 with auto gain ON.
	   *
	   * @note: It is strongly recommended that `doppler_fft_len_log2 + range_fft_len_log2 < 14`.
	   *        If the sum is >= 14, the datacube may be compressed.
	   *
	   * @note: (1 << range_fft_len_log2) should be smaller than macro 'CONFIG_RADAR_FRAMEWORK_MAX_RFFT_LEN',
	   *        which determines the upper bound of range fft length.
	   *
	   * @attention: If set to `UINT16_MAX`, the framework calculates the range FFT length based on
	   *             the `max_range_mm` parameter and sets the ADC sample number equal to the FFT length.
	   *             It is recommended to set this to the correct value directly unless the developer
	   *             is fully aware of the implications of mismatched ADC samples and FFT lengths.
	   */
	  uint16_t range_fft_len_log2;

	  /**
	   * @brief Maximum detection range in mm.
	   *
	   * The ADC sample number is calculated as: `max_range_mm / range_resolution_mm`.
	   *
	   * @note: This parameter is ignored if `range_fft_len_log2` is not set to `UINT16_MAX`.
	   *
	   * @attention: Recommended to set `range_fft_len_log2` directly. Only use this parameter if
	   *             the developer is not familiar with the consequences of ADC sample numbers
	   *             not matching the FFT length.
	   */
	  uint16_t max_range_mm;

	  uint32_t vel_resolution_mm;         ///< Velocity resolution in mm/s (1 LSB = 1 mm/s).

	  /**
	   * @brief Log2 of the Doppler FFT length.
	   *
	   * Doppler FFT length = `1 << dopper_fft_len_log2`.
	   * Velocity measurement range: `[-doppler_fft_len * vel_resolution_mm / 2, doppler_fft_len * vel_resolution_mm / 2)`.
	   * Maximum value is 10 (1024 Doppler FFT points).
	   *
	   * @note: It is strongly recommended that `doppler_fft_len_log2 + range_fft_len_log2 < 14`.
	   *        If the sum is >= 14, the datacube may be compressed.
	   *
	   * @attention: If set to `UINT16_MAX`, the framework calculates the Doppler FFT length based on
	   *             `max_unambigous_vel_mm`. It is recommended to set this parameter directly.
	   */
	  uint16_t dopper_fft_len_log2;

	  /**
	   * @brief Maximum unambiguous velocity (single side) in mm/s.
	   *
	   * The interval number is calculated as: `2 * max_unambigous_vel / vel_resolution_mm`.
	   *
	   * @note: This parameter is ignored if `dopper_fft_len_log2` is not set to `UINT16_MAX`.
	   *
	   * @attention: Recommended to set `dopper_fft_len_log2` directly unless the developer needs
	   *             to specify velocity via range parameters.
	   */
	  uint16_t max_unambigous_vel_mm;

	  uint32_t frame_period_ms;           ///< Frame period in ms (1 LSB = 1 ms).

	  /**
	   * @brief Log2 of the accumulation number per interval.
	   *
	   * Effective accumulation count = `2 ^ acc_num_log2`.
	   * Increasing this value improves SNR for targets but increases power consumption.
	   */
	  uint8_t acc_num_log2;

      /**
       * @brief Set fft mode, users can choose adc mode/range fft mode/doppler fft mode
       *        point cloud is only enabled when set to RADAR_FRAMEWORK_2DFFT_MODE
       */
      RadarDrameworkFFTMode_e fft_mode;

  } RadarFrameConfig_t;

  /**
   * @brief Application-specific radar configuration.
   */
  typedef struct {
	  RadarFrameworkClutterHalMethod_e clutter_halt_method;  ///< Method to handle clutter halting.
  } RadarAppConfig_t;

  /**
   * @brief Global radar configuration structure.
   */
  typedef struct {
	  RadarFrameConfig_t radar_frame_config;  ///< Frame configuration parameters.
	  RadarAppConfig_t   radar_app_config;       ///< Application-specific settings.
  } RadarConfig_t;

  /* Callback Function Pointer Definitions */
  typedef int (*MMW_RSP_ENTRY_PROCESS)(uint32_t frame_idx);
  typedef int (*MMW_RSP_POST_PROCESS)(uint8_t argc, void* arg[]);
  typedef int (*MMW_APP_INIT)(void);
  typedef int (*MMW_APP_DEINIT)(void);
  typedef int (*MMW_HW_INIT)(void);
  typedef int (*MMW_HW_DEINIT)(void);
  typedef int (*MMW_HIF_CONFIG)(HIF_MsgHdr_t *msg);

  /**
   * @brief Structure defining application operations for the radar framework.
   */
  typedef struct {
	  MMW_RSP_ENTRY_PROCESS    mmw_entry_proc_cb;            ///< Callback for response processing.
	  MMW_RSP_POST_PROCESS     mmw_post_proc_cb;             ///< Callback for response processing.
	  MMW_APP_INIT             mmw_app_init_cb;              ///< Callback for application initialization. Auto involked after hardware is configured by radar framework
	  MMW_APP_DEINIT           mmw_app_deinit_cb;            ///< Callback for application deinitialization.
	  MMW_HW_INIT              mmw_hw_init_cb;               ///< Callback for hardware initialization. Auto involked befor hardware start configuring by radar framework
	  MMW_HW_DEINIT            mmw_hw_deinit_cb;             ///< Callback for hardware deinitialization.
	  MMW_HIF_CONFIG           mmw_hif_config_handler;             ///< Callback for HIF configuration messages.
	  MMW_HIF_CONFIG           mmw_hif_report_handler;             ///< Callback for HIF report messages.
  } mmw_application_ops_t;

  /* API Function Prototypes */

  /**
   * @brief Configures the radar firmware parameters from the global databox configuration.
   * @return 0 on success, negative value on failure.
   */
  int radar_framework_firmware_config(void);

  /**
   * @brief Registers callbacks and configuration with the radar framework.
   * @param ptr_global_config Pointer to the global radar configuration.
   * @param ptr_mmw_app_ops   Pointer to the application operations structure.
   * @return 0: success, else register failed.
   */
  int radar_framework_register_callbacks(RadarConfig_t* ptr_global_config, mmw_application_ops_t* ptr_mmw_app_ops);

  /**
   * @brief Retrieves the pointer to the registered application operations.
   * @return Pointer to `mmw_application_ops_t` or NULL if not initialized.
   */
  mmw_application_ops_t* radar_framework_get_application_ops(void);

  /**
   * @brief Retrieves the pointer to the global configuration (const).
   * @return Pointer to the constant `RadarConfig_t` or NULL if not initialized.
   */
  const RadarConfig_t* radar_framework_get_config_const(void);

  /**
   * @brief Retrieves the pointer to the global configuration (non-const).
   * @return Pointer to the modifiable `RadarConfig_t` or NULL if not initialized.
   */
  RadarConfig_t* radar_framework_get_config(void);

  /**
   * @brief Increments the current frame index.
   * @return The frame index before increase.
   */
  uint32_t radar_framework_increase_frame_idx(void);

  uint32_t radar_framework_frame_idx_get(void);
  /**
   * @brief Initializes the radar framework system.
   * @return MMWave error code (defined MMW_ERR_CODE_*), Zero on success.
   */
  int radar_framework_sys_init(void);

  /**
   * @brief Runs the clutter convergence process.
   *
   * This function handles the following logic:
   * - If `clutter_rm_method` is 'MMW_CLUTTER_REMOVAL_DC':
   *   - On frame 0, the clutter removal function is enabled.
   *   - On every call, `mmw_psic_dc_suppression_update()` is invoked to continue DC suppression calculation.
   * - If `clutter_rm_method` is 'MMW_CLUTTER_REMOVAL_ALL' (removal of all clutter):
   *   - At the last frame of convergence, if `clutter_halt_method` is `CLUTTER_HALT_AFTER_CONVERGENCE`,
   *     the clutter update is halted.
   *
   * The convergence process relies on `s_clutter_removal_count`, calculated via
   * `radar_framework_calculate_coverage_frame_num()`.
   *
   * @param frame_idx  0-based frame index of the current frame since the last `mmw_ctrl_start()` call.
   *                   Pass 0 if this is the current frame. An incorrect index will result in inaccurate
   *                   convergence status.
   * @param clutter_rm_method Method for clutter removal.
   * @return 0: Clutter is not yet converged (radar data may be unstable).
   * @return 1: Clutter is converged.
   */
  int radar_framework_clutter_convergence_process(uint32_t frame_idx, MmwPointClutterRemoval_e clutter_rm_method);

  /**
   * @brief Stop mmw_ctrl and deinit monitor and application.
   *
   * This function must be called when user wants to stop mmw_ctrl and configure new frame parameters.
   * This function and 'radar_framework_restart_process()' form a matching call pair.
   *
   */
  int radar_framework_stop_process(void);

  /**
   * @brief Start mmw_ctrl and reinit monitor and application.
   *
   * This function and 'radar_framework_stop_process()' form a matching call pair.
   *
   */
  int radar_framework_restart_process(void);

#if (CONFIG_MMW_WATCHDOG)
  /**
   * @brief Feed & stop watchdog
   */
  void mmw_watchdog_feed(void);
#endif

  #ifdef __cplusplus
  }
  #endif

  #endif /* __RADAR_FRAMEWORK_H__ */