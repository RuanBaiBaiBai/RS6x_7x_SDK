/**
 **************************************************************************************************
 * @file    host_mmw_cmd_api.c
 * @brief   Implement of MMW cmd APIs for Host.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.*/
#include "host_mmw_api.h"

#if (HOST_HIF_CASE_GENERAL_EN)

int MmwCmd_General_StartCtrl_Cfg(DEV_HANDLE DevHdl, uint32_t enable)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_START_CTRL;
    uint32_t param_len = 1;
    uint32_t buffer_len = 1;
    uint8_t param[1];
    uint8_t resp_buff[1];
    param[0] = enable;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}


int MmwCmd_General_MimoMode_Cfg(DEV_HANDLE DevHdl, uint32_t mimo_mode)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 3;
    uint32_t buffer_len = 1;
    uint8_t param[3];
    uint8_t resp_buff[1];
    param[0] = 0x10;
    param[1] = 0x01;
    param[2] = mimo_mode;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_FrameType_Cfg(DEV_HANDLE DevHdl, uint32_t frame_type)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 3;
    uint32_t buffer_len = 1;
    uint8_t param[3];
    uint8_t resp_buff[1];
    param[0] = 0x11;
    param[1] = 0x01;
    param[2] = frame_type;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_StartFreq_Cfg(DEV_HANDLE DevHdl, uint32_t start_freq)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 4;
    uint32_t buffer_len = 1;
    uint8_t param[4];
    uint8_t resp_buff[1];
    param[0] = 0x12;
    param[1] = 0x02;
    param[2] = start_freq & 0xFF;
    param[3] = (start_freq >> 8) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_TriggerRange_Cfg(DEV_HANDLE DevHdl, uint32_t trigger_range)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 4;
    uint32_t buffer_len = 1;
    uint8_t param[4];
    uint8_t resp_buff[1];
    param[0] = 0x13;
    param[1] = 0x02;
    param[2] = trigger_range & 0xFF;
    param[3] = (trigger_range >> 8) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_RangeResolution_Cfg(DEV_HANDLE DevHdl, uint32_t range_resolution)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 4;
    uint32_t buffer_len = 1;
    uint8_t param[4];
    uint8_t resp_buff[1];
    param[0] = 0x14;
    param[1] = 0x02;
    param[2] = range_resolution & 0xFF;
    param[3] = (range_resolution >> 8) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_MaxVelocity_Cfg(DEV_HANDLE DevHdl, uint32_t max_velocity)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 4;
    uint32_t buffer_len = 1;
    uint8_t param[4];
    uint8_t resp_buff[1];
    param[0] = 0x15;
    param[1] = 0x02;
    param[2] = max_velocity & 0xFF;
    param[3] = (max_velocity >> 8) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_VelResolution_Cfg(DEV_HANDLE DevHdl, uint32_t vel_resolution)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 4;
    uint32_t buffer_len = 1;
    uint8_t param[4];
    uint8_t resp_buff[1];
    param[0] = 0x16;
    param[1] = 0x02;
    param[2] = vel_resolution & 0xFF;
    param[3] = (vel_resolution >> 8) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_FramePeriod_Cfg(DEV_HANDLE DevHdl, uint32_t period_ms)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 4;
    uint32_t buffer_len = 1;
    uint8_t param[4];
    uint8_t resp_buff[1];
    param[0] = 0x17;
    param[1] = 0x02;
    param[2] = period_ms & 0xFF;
    param[3] = (period_ms >> 8) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_General_MaxPayload_Test(DEV_HANDLE DevHdl)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_GENERAL_MSG_ID_CFG;
    uint32_t param_len = 501;
    uint32_t buffer_len = 1;
    uint8_t param[501];
    for (int i = 0; i < param_len; i++) {
        param[i] = i & 0xFF;
    }
    uint8_t resp_buff[1];

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

#endif

#if (HOST_HIF_CASE_RADAR_ANALYSIS_EN)

int MmwCmd_RadarAnalysis_Start(DEV_HANDLE DevHdl)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 1;
    uint32_t buffer_len = 1;
    uint8_t param[1];
    uint8_t resp_buff[1];
    param[0] = 0x03;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Stop(DEV_HANDLE DevHdl)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 1;
    uint32_t buffer_len = 1;
    uint8_t param[1];
    uint8_t resp_buff[1];
    param[0] = 0x04;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Mode_Cfg(DEV_HANDLE DevHdl, uint32_t txrx, uint32_t mode)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 3;
    uint32_t buffer_len = 1;
    uint8_t param[3];
    uint8_t resp_buff[1];
    param[0] = 0x07;
    param[1] = txrx;  // xtxr
    param[2] = mode;  // 1d/2d

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Freq_Cfg(DEV_HANDLE DevHdl, uint32_t start_freq, uint32_t max_freq)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 12;
    uint32_t buffer_len = 1;
    uint8_t param[12];
    uint8_t resp_buff[1];
    param[0] = 0x08;
    param[1] = 0x00;  // reserve0
    param[2] = 0x00;  // reserve1
    param[3] = 0x00;  // reserve2
    param[4] = start_freq & 0xFF;
    param[5] = (start_freq >> 8) & 0xFF;
    param[6] = (start_freq >> 16) & 0xFF;
    param[7] = (start_freq >> 24) & 0xFF;
    param[8] = max_freq & 0xFF;
    param[9] = (max_freq >> 8) & 0xFF;
    param[10] = (max_freq >> 16) & 0xFF;
    param[11] = (max_freq >> 24) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Range_Cfg(DEV_HANDLE DevHdl, uint32_t range, uint32_t resol)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 12;
    uint32_t buffer_len = 1;
    uint8_t param[12];
    uint8_t resp_buff[1];
    param[0] = 0x09;
    param[1] = 0x00;  // reserve0
    param[2] = 0x00;  // reserve1
    param[3] = 0x00;  // reserve2
    param[4] = range & 0xFF;
    param[5] = (range >> 8) & 0xFF;
    param[6] = (range >> 16) & 0xFF;
    param[7] = (range >> 24) & 0xFF;
    param[8] = resol & 0xFF;
    param[9] = (resol >> 8) & 0xFF;
    param[10] = (resol >> 16) & 0xFF;
    param[11] = (resol >> 24) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Veloc_Cfg(DEV_HANDLE DevHdl, uint32_t veloc, uint32_t resol)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 12;
    uint32_t buffer_len = 1;
    uint8_t param[12];
    uint8_t resp_buff[1];
    param[0] = 0x0A;
    param[1] = 0x00;  // reserve0
    param[2] = 0x00;  // reserve1
    param[3] = 0x00;  // reserve2
    param[4] = veloc & 0xFF;
    param[5] = (veloc >> 8) & 0xFF;
    param[6] = (veloc >> 16) & 0xFF;
    param[7] = (veloc >> 24) & 0xFF;
    param[8] = resol & 0xFF;
    param[9] = (resol >> 8) & 0xFF;
    param[10] = (resol >> 16) & 0xFF;
    param[11] = (resol >> 24) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Frame_Cfg(DEV_HANDLE DevHdl, uint32_t period, uint32_t num)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 12;
    uint32_t buffer_len = 1;
    uint8_t param[12];
    uint8_t resp_buff[1];
    param[0] = 0x0B;
    param[1] = 0x00;  // reserve0
    param[2] = 0x00;  // reserve1
    param[3] = 0x00;  // reserve2
    param[4] = period & 0xFF;
    param[5] = (period >> 8) & 0xFF;
    param[6] = (period >> 16) & 0xFF;
    param[7] = (period >> 24) & 0xFF;
    param[8] = num & 0xFF;
    param[9] = (num >> 8) & 0xFF;
    param[10] = (num >> 16) & 0xFF;
    param[11] = (num >> 24) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Intv_Cfg(DEV_HANDLE DevHdl, uint32_t period, uint32_t intv_num)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 12;
    uint32_t buffer_len = 1;
    uint8_t param[12];
    uint8_t resp_buff[1];
    param[0] = 0x0C;
    param[1] = 0x00;  // reserve0
    param[2] = 0x00;  // reserve1
    param[3] = 0x00;  // reserve2
    param[4] = period & 0xFF;
    param[5] = (period >> 8) & 0xFF;
    param[6] = (period >> 16) & 0xFF;
    param[7] = (period >> 24) & 0xFF;
    param[8] = intv_num & 0xFF;
    param[9] = (intv_num >> 8) & 0xFF;
    param[10] = (intv_num >> 16) & 0xFF;
    param[11] = (intv_num >> 24) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_ChirpNum_Cfg(DEV_HANDLE DevHdl, uint32_t num)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 2;
    uint32_t buffer_len = 1;
    uint8_t param[2];
    uint8_t resp_buff[1];
    param[0] = 0x0E;
    param[1] = num;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_Report_Cfg(DEV_HANDLE DevHdl, uint32_t report_type, uint32_t report_num)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 8;
    uint32_t buffer_len = 1;
    uint8_t param[8];
    uint8_t resp_buff[1];
    param[0] = 0x16;
    param[1] = report_type;
    param[2] = 0x00;  // reserve0
    param[3] = 0x00;  // reserve1
    param[4] = report_num & 0xFF;
    param[5] = (report_num >> 8) & 0xFF;
    param[6] = (report_num >> 16) & 0xFF;
    param[7] = (report_num >> 24) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

int MmwCmd_RadarAnalysis_DopFft_Cfg(DEV_HANDLE DevHdl, uint32_t flag)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_RADAR_ANALYSIS_MSG_ID_CFG;
    uint32_t param_len = 8;
    uint32_t buffer_len = 1;
    uint8_t param[8];
    uint8_t resp_buff[1];
    param[0] = 0x1F;
    param[1] = 0x00;  // reserve0
    param[2] = 0x00;  // reserve1
    param[3] = 0x00;  // reserve2
    param[4] = flag & 0xFF;
    param[5] = (flag >> 8) & 0xFF;
    param[6] = (flag >> 16) & 0xFF;
    param[7] = (flag >> 24) & 0xFF;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

#endif

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */

