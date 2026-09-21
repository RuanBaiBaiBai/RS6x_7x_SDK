/**
 **************************************************************************************************
 * @file    host_mmw_api.h
 * @brief   Definition of MMW config APIs for Host.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HOST_MMW_API_H_
#define _HOST_MMW_API_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_driver.h"


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
typedef struct {
    uint8_t type;
    uint8_t len;
    uint8_t value[1];
} strMsgTlv;

#define GET_TLV_LEN(msg)                        (2 + (msg)[1])
#define GET_NEXT_TLV(msg)                       ((uint8_t *)(msg) + GET_TLV_LEN(msg))
#define GET_TLV_TYPE(msg)                       (*((uint8_t *)(msg) + 0))
#define GET_TLV_VALUE_LEN(msg)                  (*((uint8_t *)(msg) + 1))
#define GET_TLV_VALUE(msg)                      ((uint8_t *)(msg) + 2)


/* Exported Msg ID.
 * ------------------------------------------------------------------------------------------------
 */
/* MSG ID */
/**
 * @group normal
 */
#define HOST_HIF_MSG_ID_GET_VERSION                      0x00
#define HOST_HIF_MSG_ID_CHIP_INFO                        0x01
#define HOST_HIF_MSG_ID_PHY_GET                          0x02
// 0x03
#define HOST_HIF_MSG_ID_HOST_REBOOT                      0x04
#define HOST_HIF_MSG_ID_DEVICE_PWRCTRL                   0x05
// 0x06
// 0x07
// 0x08
#define HOST_HIF_MSG_ID_PHY_SET                          0x09
#define HOST_HIF_MSG_ID_HIF_PARAM_HANDLE                 0x0A
#define HOST_HIF_MSG_ID_HOST_STATE                       0x0B
#define HOST_HIF_MSG_ID_HOST_POLL                        0x0C
// 0x0D
// 0x0E
// 0x0F

/**
 * @group general(except for radar analysis)
 */
#define HOST_HIF_GENERAL_MSG_ID_START_CTRL               0x07
#define HOST_HIF_GENERAL_MSG_ID_CFG                      0x60
#define HOST_HIF_GENERAL_MSG_ID_GET                      0x61
#define HOST_HIF_GENERAL_MSG_ID_DATACUBE_DATA            0xC1

/**
 * @group radar analysis
 */
#define HOST_HIF_RADAR_ANALYSIS_MSG_ID_DATACUBE_DATA     0xC1
#define HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG               0x41


/* Exported TLV.
 * ------------------------------------------------------------------------------------------------
 */
/**
 * @group general(except for radar analysis)
 */
#define HOST_HIF_GENERAL_TLV_TYPE_MIMO                   0x10
#define HOST_HIF_GENERAL_TLV_TYPE_FRAME_TYPE             0x11
#define HOST_HIF_GENERAL_TLV_TYPE_START_FREQ             0x12
#define HOST_HIF_GENERAL_TLV_TYPE_TRIGGER_RANGE          0x13
#define HOST_HIF_GENERAL_TLV_TYPE_RANGE_RESOLUTION       0x14
#define HOST_HIF_GENERAL_TLV_TYPE_MAX_VELOCITY           0x15
#define HOST_HIF_GENERAL_TLV_TYPE_VEL_RESOLUTION         0x16
#define HOST_HIF_GENERAL_TLV_TYPE_FRAME_PERIOD           0x17

#define HOST_HIF_GENERAL_TLV_MIMO_LEN                    1
#define HOST_HIF_GENERAL_TLV_MIMO_1T1R                   0x00
#define HOST_HIF_GENERAL_TLV_MIMO_1T3R                   0x01
#define HOST_HIF_GENERAL_TLV_MIMO_2T3R                   0x02
#define HOST_HIF_GENERAL_TLV_MIMO_1T4R                   0x03
#define HOST_HIF_GENERAL_TLV_MIMO_2T4R                   0x04
#define HOST_HIF_GENERAL_TLV_MIMO_1T2R                   0x05

#define HOST_HIF_GENERAL_TLV_FRAME_TYPE_LEN              1
#define HOST_HIF_GENERAL_TLV_FRAME_TYPE_1D               0x00
#define HOST_HIF_GENERAL_TLV_FRAME_TYPE_2D               0x01

#define HOST_HIF_GENERAL_TLV_START_FREQ_LEN              2

#define HOST_HIF_GENERAL_TLV_TRIGGER_RANGE_LEN           2

#define HOST_HIF_GENERAL_TLV_RANGE_RESOLUTION_LEN        2

#define HOST_HIF_GENERAL_TLV_MAX_VELOCITY_LEN            2

#define HOST_HIF_GENERAL_TLV_VEL_RESOLUTION_LEN          2

#define HOST_HIF_GENERAL_TLV_FRAME_PERIOD_LEN            2

/**
 * @group radar analysis
 */
// none


/* Exported MMW Const.
 * ------------------------------------------------------------------------------------------------
 */
/**
 * @group radar analysis
 */
#define HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_1T1R            0
#define HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_1T3R            1
#define HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_2T3R            2
#define HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_1T4R            3
#define HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_2T4R            4
#define HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_1T2R            5
#define HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_LAST            6

#define HOST_HIF_RADAR_ANALYSIS_MMW_MODE_1DFFT           1
#define HOST_HIF_RADAR_ANALYSIS_MMW_MODE_2DFFT           2

#define HOST_HIF_RADAR_ANALYSIS_MMW_REPORT_TYPE_CUBE               0
#define HOST_HIF_RADAR_ANALYSIS_MMW_REPORT_TYPE_MMIC               2
#define HOST_HIF_RADAR_ANALYSIS_MMW_REPORT_TYPE_POINTS             4
#define HOST_HIF_RADAR_ANALYSIS_MMW_REPORT_TYPE_TRACKING           5
#define HOST_HIF_RADAR_ANALYSIS_MMW_REPORT_TYPE_POINTS_TRACKING    6
#define HOST_HIF_RADAR_ANALYSIS_MMW_REPORT_TYPE_DISABLE            0xFF


/* Exported functions of send command.
 * ------------------------------------------------------------------------------------------------
 */
#if (HOST_HIF_CASE_GENERAL_EN)

/**
 * @brief Config radar work state.
 *
 * @param DevHdl Device handle.
 *
 * @param enable 1 -> work, 0 -> stop.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_StartCtrl_Cfg(DEV_HANDLE DevHdl, uint32_t enable);

/**
 * @brief Config radar mimo mode.
 *
 * @param DevHdl Device handle.
 *
 * @param mimo_mode The mimo mode,
 *            macro of HOST_HIF_GENERAL_TLV_MIMO_xTxR
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_MimoMode_Cfg(DEV_HANDLE DevHdl, uint32_t mimo_mode);

/**
 * @brief Config radar frame type.
 *
 * @param DevHdl Device handle.
 *
 * @param frame_type The frame type,
              macro HOST_HIF_GENERAL_TLV_FRAME_TYPE_xD
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_FrameType_Cfg(DEV_HANDLE DevHdl, uint32_t frame_type);

/**
 * @brief Config radar start frequence.
 *
 * @param DevHdl Device handle.
 *
 * @param start_freq The start frequence, unit is MHz.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_StartFreq_Cfg(DEV_HANDLE DevHdl, uint32_t start_freq);

/**
 * @brief Config radar max range.
 *
 * @param DevHdl Device handle.
 *
 * @param trigger_range The max range, unit is mm.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_TriggerRange_Cfg(DEV_HANDLE DevHdl, uint32_t trigger_range);

/**
 * @brief Config radar range resolution.
 *
 * @param DevHdl Device handle.
 *
 * @param trigger_range The range resolution, unit is mm.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_RangeResolution_Cfg(DEV_HANDLE DevHdl, uint32_t range_resolution);

/**
 * @brief Config radar max velocity.
 *
 * @param DevHdl Device handle.
 *
 * @param trigger_range The max velocity, unit is mm/s.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_MaxVelocity_Cfg(DEV_HANDLE DevHdl, uint32_t max_velocity);

/**
 * @brief Config radar velocity resolution.
 *
 * @param DevHdl Device handle.
 *
 * @param trigger_range The velocity resolution, unit is mm/s.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_VelResolution_Cfg(DEV_HANDLE DevHdl, uint32_t vel_resolution);

/**
 * @brief Config radar frame period.
 *
 * @param DevHdl Device handle.
 *
 * @param trigger_range The frame period, unit is ms.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_FramePeriod_Cfg(DEV_HANDLE DevHdl, uint32_t period_ms);

/**
 * @brief Test the max payload in command.
 *
 * @param DevHdl Device handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_General_MaxPayload_Test(DEV_HANDLE DevHdl);

#endif


#if (HOST_HIF_CASE_RADAR_ANALYSIS_EN)

/**
 * @brief Start radar analysis.
 *
 * @param DevHdl Device handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Start(DEV_HANDLE DevHdl);

/**
 * @brief Stop radar analysis.
 *
 * @param DevHdl Device handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Stop(DEV_HANDLE DevHdl);

/**
 * @brief Config radar analysis mimo mode and frame type.
 *
 * @param DevHdl Device handle.
 *
 * @param txrx The mimo mode,
 *              macro HOST_HIF_RADAR_ANALYSIS_MMW_MIMO_xTxR.
 *
 * @param mode The frame type,
 *              macro HOST_HIF_RADAR_ANALYSIS_MMW_MODE_xDFFT.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Mode_Cfg(DEV_HANDLE DevHdl, uint32_t txrx, uint32_t mode);

/**
 * @brief Config radar analysis start frequence and max frequence.
 *
 * @param DevHdl Device handle.
 *
 * @param start_freq The start frequence, unit is MHz.
 *
 * @param max_freq The max frequence, unit is MHz, can set to 0 means there's no limit on max MHz.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Freq_Cfg(DEV_HANDLE DevHdl, uint32_t start_freq, uint32_t max_freq);

/**
 * @brief Config radar analysis max range and range resolution.
 *
 * @param DevHdl Device handle.
 *
 * @param range The max range, unit is mm.
 *
 * @param resol The range resolution, unit is mm.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Range_Cfg(DEV_HANDLE DevHdl, uint32_t range, uint32_t resol);

/**
 * @brief Config radar analysis max velocity and velocity resolution.
 *
 * @param DevHdl Device handle.
 *
 * @param veloc The max velocity, unit is mm/s.
 *
 * @param resol The velocity resolution, unit is mm/s.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Veloc_Cfg(DEV_HANDLE DevHdl, uint32_t veloc, uint32_t resol);

/**
 * @brief Config radar analysis frame period and frame number.
 *
 * @param DevHdl Device handle.
 *
 * @param period The frame period, unit is ms.
 *
 * @param num The frame number, `0` -> no limit, `>0` -> frame number.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Frame_Cfg(DEV_HANDLE DevHdl, uint32_t period, uint32_t num);

/**
 * @brief Config radar analysis interval period and interval number.
 *
 * @param DevHdl Device handle.
 *
 * @param period The interval period, unit is us.
 *
 * @param intv_num The interval number in a frame.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Intv_Cfg(DEV_HANDLE DevHdl, uint32_t period, uint32_t intv_num);

/**
 * @brief Config radar analysis chirp number in a interval.
 *
 * @param DevHdl Device handle.
 *
 * @param num The number of chirps sending in a interval, must be power of 2.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_ChirpNum_Cfg(DEV_HANDLE DevHdl, uint32_t num);

/**
 * @brief Config radar analysis report data type and data number.
 *
 * @param DevHdl Device handle.
 *
 * @param report_type The report data type,
 *              macro HOST_HIF_RADAR_ANALYSIS_MMW_REPORT_TYPE_xxxx.
 *
 * @param report_num The number of frame to upload.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_Report_Cfg(DEV_HANDLE DevHdl, uint32_t report_type, uint32_t report_num);

/**
 * @brief Config radar analysis open or close DopFFT when enable 2DFFT.
 *
 * @param DevHdl Device handle.
 *
 * @param flag 1 -> open, 0 -> close.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int MmwCmd_RadarAnalysis_DopFft_Cfg(DEV_HANDLE DevHdl, uint32_t flag);

#endif


/* Exported functions of report data.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

typedef struct {
    uint16_t imag;
    uint16_t real;
} Frame_FFT_t;

typedef struct {
    union {
        struct {
            uint16_t range;
            uint16_t azimuth;
            uint16_t elevation;
            uint16_t velocity;
            uint16_t snr;
            uint16_t reserved;
        } polar;
        struct {
            uint16_t x;
            uint16_t y;
            uint16_t z;
            uint16_t velocity;
            uint16_t snr;
            uint16_t reserved;
        } coord;
    } type;
} Frame_Point_t;

typedef struct {
    uint32_t frameIdx;
    uint32_t frameLen;   /* unit=DWORD(0xC1/0xC3) or BYTE(0xC2) */
    uint32_t dataOffset; /* unit=DWORD(0xC1/0xC3) or BYTE(0xC2) */
    uint8_t  data[0];
} FrameHdr_Data_t;

typedef struct {
    uint32_t frameIdx;
    uint32_t frameLen;
    uint32_t dataOffset;
    Frame_FFT_t data[0];
} FrameData_C1_t;
#define HOST_MMW_GET_C1_DATA_LENGTH(data_len) \
    (((data_len) - sizeof(FrameData_C1_t)) / sizeof(Frame_FFT_t))

typedef struct {
    uint32_t frameIdx;
    uint32_t frameLen;
    uint32_t dataOffset;

    uint32_t type         : 8;
    uint32_t total_length : 24;
    uint8_t  tx_num;
    uint8_t  rx_num;
    uint8_t  rev;
    uint8_t  rsv;
    uint16_t range_bin_num;
    uint16_t dop_bin_num;

    Frame_FFT_t data[0];
} FrameData_C2_t;
#define HOST_MMW_GET_C2_DATA_LENGTH(data_len) \
    (((data_len) - sizeof(FrameData_C2_t)) / sizeof(Frame_FFT_t))

typedef struct {
    uint32_t frameIdx;
    uint32_t frameLen;
    uint32_t dataOffset;

    uint8_t  type;
    uint8_t  flag;
    uint16_t total_length;

    Frame_Point_t data[0];
} FrameData_C3_t;
#define HOST_MMW_GET_C3_DATA_LENGTH(data_len) \
    (((data_len) - sizeof(FrameData_C3_t)) / sizeof(Frame_Point_t))

// MSG ID = 0xC6
typedef struct {
    // debug payload header
    uint8_t  dim;
    // [1:0] data type
    //     0 -> byte
    //     1 -> short
    //     2 -> word
    //     3 -> float/double
    // [2]   sign
    //     0 -> if data type is 0 or 1, it is unsigned
    //          if data type is 2 or 3, it is single
    //     1 -> if data type is 0 or 1, it is signed
    //          if data type is 2 or 3, it is double
    // [7:3] Q
    //     displayed value = data / 2^Q
    // [8]   align mode
    //     0 -> aligned in col
    //     1 -> aligned in row
    uint16_t dataFormats;
    uint16_t len;

    // payload name and data, name is end of '\0'
    uint8_t  payload[0];
} __packed GeneralFrameHdr_Data_t;

#define HOST_HIF_MSG_FE_TLV_TYPE_RESPONSE_CNT       0x00
#define HOST_HIF_MSG_FE_TLV_TYPE_FRAME_INDEX        0x01
#define HOST_HIF_MSG_FE_TLV_TYPE_MOTION_PT_NUM      0x02
#define HOST_HIF_MSG_FE_TLV_TYPE_PRESENT_PT_NUM     0x03
typedef struct{
    uint32_t response_cnt;
    uint32_t frame_index;
    uint16_t motion_pt_num;
    uint16_t present_pt_num;
} Heartbeat_Data_t;

/**
 * @brief Report data callback function.
 *
 * @param msg_id The msg id.
 *
 * @param data the data.
 *
 * @param data_len The data length.
 *
 * @param cb_arg The user arg.
 */
typedef void (*Report_CB)(uint32_t msg_id, uint8_t *data, uint32_t data_len, void *cb_arg);

/**
 * @brief Set data handle for a msg id.
 *
 * @param DevHdl Device handle.
 *
 * @param cb Store in HIF report_handle->cb_arg->report_cb.
 *
 * @param cb_arg Store in HIF report_handle->cb_arg->cb_arg.
 *
 * @param buffer Store in HIF report_handle->cb_arg->buffer, no use now.
 *
 * @param buffer_len Store in HIF report_handle->cb_arg->bufferLen, no use now.
 *
 * @param msg_id Used to regist callback.
 *
 * @param proc_handle The data callback function, called when recved a burst.
 *            proc_handle(msg_id, payload, payload_len, report_handle->cb_arg);
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_Report_Handle_Set_General(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg,
		void *buffer, uint32_t buffer_len, uint8_t msg_id, void *proc_handle);

/**
 * @brief Free all data handle, include 0xC1, 0xC2, 0xC3, 0xC6, 0xC7, 0xC8, 0xCA, 0xCB
 *
 * @param DevHdl Device handle.
 */
void Mmw_Report_Handle_FreeAll(DEV_HANDLE DevHdl);

/**
 * @brief Set 0xC1 data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_C1_CubeData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xC2 data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_C2_FFTData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xC3 data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_C3_Objects_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xC6 data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_C6_GeneralData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xC7 data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_C7_PlotData_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xC8 data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_C8_MotionSensor_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xCA data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_CA_MotionSensor_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xCB data handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_CB_MotionSensor_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Set 0xFE heartbeat handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_FE_Heartbeat_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

/**
 * @brief Parse data to heartbeat data.
 *
 * @param heartbeat_out Parsed data.
 *
 * @param data Origin data.
 *
 * @param data_len Origin data length.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_FE_Heartbeat_Data_Parse(Heartbeat_Data_t *heartbeat_out, uint8_t *data, uint32_t data_len);

/**
 * @brief Set 0xFF startup handle.
 *
 * @param DevHdl Device handle.
 *
 * @param cb The data callback function.
 *
 * @param cb_arg The data callback function arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Mmw_FF_Startup_Handle_Set(DEV_HANDLE DevHdl, Report_CB cb, void *cb_arg);

#ifdef __cplusplus
}
#endif

#endif /* _HOST_MMW_API_H_ */


