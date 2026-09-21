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
#define HIF_DEV0_BUS_SPEED                      20000000                        /* bus speed */
#define HIF_DEV0_BUS_ID                         0                               /* bus id (spi0:0 spi1:1...) */
#define HIF_DEV0_BUS_PARAM                      LLC_BUS_PARAM_SPI               /* spi:LLC_BUS_PARAM_SPI 
                                                                                   dspi:LLC_BUS_PARAM_DSPI 
                                                                                   qspi:LLC_BUS_PARAM_QSPI */
#define HIF_DEV0_IO_UPD                         5                               /* upd device io */
#define HIF_DEV0_IO_RST                         5                               /* rst device io */
#define HIF_DEV0_IO_NOTIFY                      6                               /* host notify io */
#define HIF_DEV0_PARAM                          0                               /* spi_cs pin*/

#define HIF_DEV1_BUS_SPEED                      20000000                        /* bus speed */
#define HIF_DEV1_BUS_ID                         0                               /* bus id (spi0:0 spi1:1...) */
#define HIF_DEV1_BUS_PARAM                      LLC_BUS_PARAM_SPI               /* spi:LLC_BUS_PARAM_SPI 
                                                                                   dspi:LLC_BUS_PARAM_DSPI 
                                                                                   qspi:LLC_BUS_PARAM_QSPI */
#define HIF_DEV1_IO_UPD                         5                               /* upd device io */
#define HIF_DEV1_IO_RST                         5                               /* rst device io */
#define HIF_DEV1_IO_NOTIFY                      11                              /* host notify io */
#define HIF_DEV1_PARAM                          4                               /* spi_cs pin*/

#elif (HIF_BUS_TYPE == LLC_BUS_TYPE_UART)
#define HIF_DEV0_BUS_SPEED                      1000000                         /* bus speed */
#define HIF_DEV0_BUS_ID                         1                               /* bus id (uart0:0 uart1:1...) */
#define HIF_DEV0_BUS_PARAM                      0                               /* no use */
#define HIF_DEV0_IO_UPD                         4                               /* upd device io */
#define HIF_DEV0_IO_RST                         5                               /* rst device io */
#define HIF_DEV0_IO_NOTIFY                      6                               /* host notify io */
#define HIF_DEV0_PARAM                          1                               /* bus id */

#define HIF_DEV1_BUS_SPEED                      1000000                         /* bus speed */
#define HIF_DEV1_BUS_ID                         2                               /* bus id (uart0:0 uart1:1...) */
#define HIF_DEV1_BUS_PARAM                      0                               /* no use */
#define HIF_DEV1_IO_UPD                         4                               /* upd device io */
#define HIF_DEV1_IO_RST                         5                               /* rst device io */
#define HIF_DEV1_IO_NOTIFY                      6                               /* host notify io */
#define HIF_DEV1_PARAM                          2                               /* bus id */

#elif (HIF_BUS_TYPE == LLC_BUS_TYPE_I2C)
#define HIF_DEV0_BUS_SPEED                      400000                          /* bus speed */
#define HIF_DEV0_BUS_ID                         0                               /* bus id (i2c0:0 i2c1:1...) */
#define HIF_DEV0_BUS_PARAM                      0                               /* no use */
#define HIF_DEV0_IO_UPD                         2                               /* upd device io */
#define HIF_DEV0_IO_RST                         3                               /* rst device io */
#define HIF_DEV0_IO_NOTIFY                      6                               /* host notify io */
#define HIF_DEV0_PARAM                          0x3A                            /* i2c address */

#define HIF_DEV1_BUS_SPEED                      400000                          /* bus speed */
#define HIF_DEV1_BUS_ID                         0                               /* bus id (i2c0:0 i2c1:1...) */
#define HIF_DEV1_BUS_PARAM                      0                               /* no use */
#define HIF_DEV1_IO_UPD                         0                               /* upd device io */
#define HIF_DEV1_IO_RST                         1                               /* rst device io */
#define HIF_DEV1_IO_NOTIFY                      11                              /* host notify io */
#define HIF_DEV1_PARAM                          0xE5                            /* i2c address */
#endif

#define HIF_CFG_CMD_BUFF_SIZE                   512                             /* hif cmd buff max len */

#define HIF_LOG_PRINT_ENA                       1
#if HIF_LOG_PRINT_ENA
#define HIF_LOG_PRINT(fmt, arg...)      printf(fmt, ##arg)
#else
#define HIF_LOG_PRINT(fmt, arg...)
#endif

/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static DEV_HANDLE dev0Handle  = NULL;
static DEV_HANDLE dev1Handle  = NULL;

/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
static void Internal_Msg_Report_CB_DEV0(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg);
static void Internal_Msg_Report_CB_DEV1(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg);

/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
int main(void)
{
    int status = HOST_ERRCODE_SUCCESS;

    HIF_LOG_PRINT("Host multi device sample\n");
    HIF_LOG_PRINT("-------------------------------------------\n");

    do {
        printf("1>. Config HIF driver\n");
        DevHw_t dev0Hw, dev1Hw;
        HifCfg_t hifCfg = {
            .cmd_buf_len        = HIF_CFG_CMD_BUFF_SIZE,
        };

        DevHw_Set_DefaultParam(&dev0Hw, HIF_BUS_TYPE, HIF_DEV0_BUS_PARAM, HIF_DEV0_BUS_ID, HIF_DEV0_BUS_SPEED,
                               HIF_DEV0_IO_UPD, HIF_DEV0_IO_RST, HIF_DEV0_IO_NOTIFY, HIF_DEV0_PARAM);
        DevHw_Set_DefaultParam(&dev1Hw, HIF_BUS_TYPE, HIF_DEV1_BUS_PARAM, HIF_DEV1_BUS_ID, HIF_DEV1_BUS_SPEED,
                               HIF_DEV1_IO_UPD, HIF_DEV1_IO_RST, HIF_DEV1_IO_NOTIFY, HIF_DEV1_PARAM);

        status = Host_Driver_Init();
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("init host driver fail(%d)\n", status);
            break;
        }

        status = Host_Device_Regist(&dev0Handle, &dev0Hw, &hifCfg);
        if (HOST_ERRCODE_SUCCESS != status) {
            HIF_LOG_PRINT("regist device1(%p) fail(%d)\n", dev0Handle, status);
            break;
        }

        status = Host_Device_Regist(&dev1Handle, &dev1Hw, &hifCfg);
        if (HOST_ERRCODE_SUCCESS != status) {
            HIF_LOG_PRINT("regist device1(%p) fail(%d)\n", dev1Handle, status);
            break;
        }



        HIF_LOG_PRINT("2>. Register report callback\n");
        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_FFT_DATA);
        status = Mmw_C2_FFTData_Handle_Set(dev0Handle, Internal_Msg_Report_CB_DEV0, dev0Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_FFT_DATA, status);
            break;
        }

        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_OBJECTS);
        status = Mmw_C3_Objects_Handle_Set(dev0Handle, Internal_Msg_Report_CB_DEV0, dev0Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_OBJECTS, status);
            break;
        }

        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_MOTION_DATA);
        status = Mmw_C6_GeneralData_Handle_Set(dev0Handle, Internal_Msg_Report_CB_DEV0, dev0Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_MOTION_DATA, status);
            break;
        }


        status = Mmw_C2_FFTData_Handle_Set(dev1Handle, Internal_Msg_Report_CB_DEV1, dev1Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_FFT_DATA, status);
            break;
        }

        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_OBJECTS);
        status = Mmw_C3_Objects_Handle_Set(dev1Handle, Internal_Msg_Report_CB_DEV1, dev1Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_OBJECTS, status);
            break;
        }

        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_ID_MOTION_DATA);
        status = Mmw_C6_GeneralData_Handle_Set(dev1Handle, Internal_Msg_Report_CB_DEV1, dev1Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_MOTION_DATA, status);
            break;
        }



        HIF_LOG_PRINT("3>. Open HIF driver\n");
        status = Host_Device_Open(dev0Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("open device(%p) fail(%d)\n", dev0Handle, status);
            break;
        }

        status = Host_Device_Open(dev1Handle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("open device(%p) fail(%d)\n", dev1Handle, status);
            break;
        }


    } while (0);


    if (status != HOST_ERRCODE_SUCCESS) {
        HIF_LOG_PRINT("4>. Init fail, close and deinit driver\n");
        Host_Device_Close(dev0Handle);
        Host_Device_Close(dev1Handle);
        HIF_LOG_PRINT("close\n");
        Host_Device_Unregist(dev0Handle);
        Host_Device_Unregist(dev1Handle);
        HIF_LOG_PRINT("Unregist\n");
        Host_Driver_Deinit();
        HIF_LOG_PRINT("Deinit\n");
    } else {
        HIF_LOG_PRINT("SUCCESS\n");
    }

    HIF_LOG_PRINT("-------------------------------------------\n");

    return 0;
}

static void Internal_Msg_Report_CB_DEV0(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg)
{
    static uint64_t counter[4] = { 0, 0, 0, 0 };
    static uint32_t last_frame_idx[3] = { 0 - 1, 0 - 1, 0 - 1 };
    if (msg_id == 0xC6) {
        HIF_LOG_PRINT("%02X payload len = %u\n", msg_id, payload_len);
    } else {
        FrameHdr_Data_t *frame = (FrameHdr_Data_t *)payload;

        if (frame != NULL) {
            if (frame->frameIdx == last_frame_idx[msg_id-0xC1] + 1) {
                HIF_LOG_PRINT("[%llu][%u] device0(%p) recved %02X data(%p), len=%u\n",
                             counter[msg_id-0xC1]++, frame->frameIdx, arg, msg_id, payload, payload_len);
            } else {
                HIF_LOG_PRINT("device0(%p) recved %02X frame idx are not consecutive!\n", arg, msg_id);
                HIF_LOG_PRINT("last frame idx is %u\n", last_frame_idx[msg_id-0xC1]);
                HIF_LOG_PRINT("curr frame idx is %u\n", frame->frameIdx);
            }
            last_frame_idx[msg_id-0xC1] = frame->frameIdx;
        } else {
            HIF_LOG_PRINT("device(%p) recved frame data is NULL!\n", arg);
        }
    }
}

static void Internal_Msg_Report_CB_DEV1(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg)
{
    static uint64_t counter[4] = { 0, 0, 0, 0 };
    static uint32_t last_frame_idx[3] = { 0 - 1, 0 - 1, 0 - 1 };
    if (msg_id == 0xC6) {
        HIF_LOG_PRINT("%02X payload len = %u\n", msg_id, payload_len);
    } else {
        FrameHdr_Data_t *frame = (FrameHdr_Data_t *)payload;

        if (frame != NULL) {
            if (frame->frameIdx == last_frame_idx[msg_id-0xC1] + 1) {
                HIF_LOG_PRINT("[%llu][%u] device1(%p) recved %02X data(%p), len=%u\n",
                             counter[msg_id-0xC1]++, frame->frameIdx, arg, msg_id, payload, payload_len);
            } else {
                HIF_LOG_PRINT("device1(%p) recved %02X frame idx are not consecutive!\n", arg, msg_id);
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
