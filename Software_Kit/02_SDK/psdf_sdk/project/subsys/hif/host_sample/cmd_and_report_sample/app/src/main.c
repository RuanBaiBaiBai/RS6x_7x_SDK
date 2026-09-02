/**
 ******************************************************************************
 * @file    main.c
 * @brief   main define.
 * @verbatim    null
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2026 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
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
 ******************************************************************************
 */


/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "common.h"
#include "host_config.h"
#include "host_driver.h"
#include "host_mmw_api.h"

/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Types.
 * ------------------------------------------------------------------------------------------------
 */
/* Define.
 * ------------------------------------------------------------------------------------------------
 */

/*
    Modify macros (HIF_BUS_TYPE, HIF_BUS_SPEED, HIF_BUS_ID, HIF_BUS_PARAM, HIF_IO_UPD, 
    HIF_IO_RST, HIF_IO_NOTIFY, HIF_PARAM) before running sample.
    macros (HIF_BUS_EVENT_METHOD, HIF_NOTIFY_TYPE, HIF_UPLOAD_TYPE) no need to modify
*/ 

/* BUS TYPE  ( SPI:LLC_BUS_TYPE_SPI   UART:LLC_BUS_TYPE_UART   I2C:LLC_BUS_TYPE_I2C ) */
#define HIF_BUS_TYPE                            LLC_BUS_TYPE_SPI

#if (HIF_BUS_TYPE == LLC_BUS_TYPE_SPI)
#define HIF_BUS_SPEED                           32000000                        /* bus speed */
#define HIF_BUS_ID                              0                               /* bus id (spi0:0 spi1:1...) */
#define HIF_BUS_PARAM                           LLC_BUS_PARAM_SPI               /* spi:LLC_BUS_PARAM_SPI 
                                                                                   dspi:LLC_BUS_PARAM_DSPI 
                                                                                   qspi:LLC_BUS_PARAM_QSPI */
#define HIF_IO_UPD                              4                               /* upd device io */
#define HIF_IO_RST                              5                               /* rst device io */
#define HIF_IO_NOTIFY                           6                               /* host notify io */
#define HIF_PARAM                               0                               /* spi_cs pin*/
#elif (HIF_BUS_TYPE == LLC_BUS_TYPE_UART)
#define HIF_BUS_SPEED                           1000000                         /* bus speed */
#define HIF_BUS_ID                              1                               /* bus id (uart0:0 uart1:1...) */
#define HIF_BUS_PARAM                           0                               /* no use */
#define HIF_IO_UPD                              4                               /* upd device io */
#define HIF_IO_RST                              5                               /* rst device io */
#define HIF_IO_NOTIFY                           6                               /* host notify io */
#define HIF_PARAM                               1                               /* bus id */
#elif (HIF_BUS_TYPE == LLC_BUS_TYPE_I2C)
#define HIF_BUS_SPEED                           400000                          /* bus speed */
#define HIF_BUS_ID                              0                               /* bus id (i2c0:0 i2c1:1...) */
#define HIF_BUS_PARAM                           0                               /* no use */
#define HIF_IO_UPD                              2                               /* upd device io */
#define HIF_IO_RST                              3                               /* rst device io */
#define HIF_IO_NOTIFY                           6                               /* host notify io */
#define HIF_PARAM                               0x3A                            /* i2c address */
#endif

#define HIF_CFG_CMD_BUFF_SIZE                   512                             /* hif cmd buff max len */

#define MMW_START                               1
#define MMW_STOP                                0


#define HIF_LOG_PRINT_ENA                       1
#if HIF_LOG_PRINT_ENA
#define HIF_LOG_PRINT(fmt, arg...)      printf(fmt, ##arg)
#else
#define HIF_LOG_PRINT(fmt, arg...)
#endif

/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static DEV_HANDLE devHandle  = NULL;

typedef struct {
    uint8_t mimo_mode;
    uint8_t frame_type;
    uint32_t start_freq_mhz;
    uint32_t range_resolution_mm;
    uint16_t range_fft_len;
    uint32_t vel_resolution_mm;
    uint16_t dopper_fft_len;
    uint32_t frame_period_ms;
} RadarFrameConfig_t;

RadarFrameConfig_t data_box_frame_config = 
{
    .mimo_mode = 4,
    .start_freq_mhz = 58800,
    .frame_type = 1,
    .range_fft_len = 10240,
    .range_resolution_mm = 40,
    .dopper_fft_len = 3200,
    .vel_resolution_mm = 200,
    .frame_period_ms = 50,
};

/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
static void Internal_Msg_Report_CB(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg);

/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
int main(void)
{
    int status = HOST_ERRCODE_SUCCESS;

    HIF_LOG_PRINT("Host cmd and report test sample\n");
    HIF_LOG_PRINT("-------------------------------------------\n");

    do {
        printf("1>. Config HIF driver\n");
        DevHw_t devHw;
        HifCfg_t hifCfg = {
            .cmd_buf_len        = HIF_CFG_CMD_BUFF_SIZE,
        };

        DevHw_Set_DefaultParam(&devHw, HIF_BUS_TYPE, HIF_BUS_PARAM, HIF_BUS_ID, HIF_BUS_SPEED,
                               HIF_IO_UPD, HIF_IO_RST, HIF_IO_NOTIFY, HIF_PARAM);

        status = Host_Driver_Init();
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("init host driver fail(%d)\n", status);
            break;
        }

        status = Host_Device_Regist(&devHandle, &devHw, &hifCfg);
        if (HOST_ERRCODE_SUCCESS != status) {
            HIF_LOG_PRINT("regist device1(%p) fail(%d)\n", devHandle, status);
            break;
        }



        HIF_LOG_PRINT("2>. Register report callback\n");
        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_FFT_DATA);
        status = Mmw_C2_FFTData_Handle_Set(devHandle, Internal_Msg_Report_CB, devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_FFT_DATA, status);
            break;
        }

        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_OBJECTS);
        status = Mmw_C3_Objects_Handle_Set(devHandle, Internal_Msg_Report_CB, devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_OBJECTS, status);
            break;
        }

        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_MOTION_DATA);
        status = Mmw_C6_GeneralData_Handle_Set(devHandle, Internal_Msg_Report_CB, devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_MOTION_DATA, status);
            break;
        }



        HIF_LOG_PRINT("3>. Open HIF driver\n");
        status = Host_Device_Open(devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("open device(%p) fail(%d)\n", devHandle, status);
            break;
        }



        HIF_LOG_PRINT("4>. Cmd config\n");

        HIF_LOG_PRINT("4.1>. Stop mmw\n");
        status = MmwCmd_General_StartCtrl_Cfg(devHandle, MMW_STOP);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }


        HIF_LOG_PRINT("4.2>. Config mmw\n");
        status = MmwCmd_General_MimoMode_Cfg(devHandle, data_box_frame_config.mimo_mode);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }

        status = MmwCmd_General_StartFreq_Cfg(devHandle, data_box_frame_config.start_freq_mhz);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }

        status = MmwCmd_General_FrameType_Cfg(devHandle, data_box_frame_config.frame_type);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }

        status = MmwCmd_General_RangeResolution_Cfg(devHandle, data_box_frame_config.range_resolution_mm);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }

        status = MmwCmd_General_TriggerRange_Cfg(devHandle, data_box_frame_config.range_fft_len);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }

        status = MmwCmd_General_VelResolution_Cfg(devHandle, data_box_frame_config.vel_resolution_mm);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }

        status = MmwCmd_General_MaxVelocity_Cfg(devHandle, data_box_frame_config.dopper_fft_len);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }

        status = MmwCmd_General_FramePeriod_Cfg(devHandle, data_box_frame_config.frame_period_ms);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }


        HIF_LOG_PRINT("4.3>. Start mmw\n");
        status = MmwCmd_General_StartCtrl_Cfg(devHandle, MMW_START);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("host send cmd fail(%d)\n", status);
        }


    } while (0);


    if (status != HOST_ERRCODE_SUCCESS) {
        HIF_LOG_PRINT("5>. Init fail, close and deinit driver\n");
        Host_Device_Close(devHandle);
        HIF_LOG_PRINT("close\n");
        Host_Device_Unregist(devHandle);
        HIF_LOG_PRINT("Unregist\n");
        Host_Driver_Deinit();
        HIF_LOG_PRINT("Deinit\n");
    } else {
        HIF_LOG_PRINT("SUCCESS\n");
    }

    HIF_LOG_PRINT("-------------------------------------------\n");

    return 0;
}

static void Internal_Msg_Report_CB(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg)
{
    static uint64_t counter[4] = { 0, 0, 0, 0 };
    static uint32_t last_frame_idx[3] = { 0 - 1, 0 - 1, 0 - 1 };
    if (msg_id == 0xC6) {
        HIF_LOG_PRINT("%02X payload len = %u\n", msg_id, payload_len);
    } else {
        FrameHdr_Data_t *frame = (FrameHdr_Data_t *)payload;

        if (frame != NULL) {
            if (frame->frameIdx == last_frame_idx[msg_id-0xC1] + 1) {
                HIF_LOG_PRINT("[%llu][%u] device(%p) recved %02X data(%p), len=%u\n",
                             counter[msg_id-0xC1]++, frame->frameIdx, arg, msg_id, payload, payload_len);
            } else {
                HIF_LOG_PRINT("device(%p) recved %02X frame idx are not consecutive!\n", arg, msg_id);
                HIF_LOG_PRINT("last frame idx is %u\n", last_frame_idx[msg_id-0xC1]);
                HIF_LOG_PRINT("curr frame idx is %u\n", frame->frameIdx);
            }
            last_frame_idx[msg_id-0xC1] = frame->frameIdx;
        } else {
            HIF_LOG_PRINT("device(%p) recved frame data is NULL!\n", arg);
        }
    }
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
