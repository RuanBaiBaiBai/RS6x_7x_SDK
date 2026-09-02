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

/* Define.
 * ------------------------------------------------------------------------------------------------
 */

/* shell be adapted by user according to its own platform. 
 */

#define HIF_BUS_TYPE                            LLC_BUS_TYPE_SPI
#define HIF_BUS_PARAM                           LLC_BUS_PARAM_SPI
#define HIF_BUS_ID                              0
#define HIF_BUS_SPEED                           16000000

#define HIF_IO_UPD                              5
#define HIF_IO_RST                              5
#define HIF_IO_NOTIFY                           6

#define HIF_PARAM                               0


#define HIF_CFG_CMD_BUFF_SIZ                    1024
#define HIF_CFG_TL_RETRY_ENA                    1
#define HIF_CFG_FRAGMENT_ENA                    1
#define HIF_CFG_FRAEMENT_RETRY_ENA              1
#define HIF_CFG_APP_RETRY_ENA                   1


#define HIF_LOG_PRINT_ENA                       1

/* Types.
 * ------------------------------------------------------------------------------------------------
 */
/* Define.
 * ------------------------------------------------------------------------------------------------
 */

#if HIF_LOG_PRINT_ENA
#define HIF_LOG_PRINT(fmt, arg...)      printf(fmt, ##arg)
#else
#define HIF_LOG_PRINT(fmt, arg...)
#endif


#define HIF_MSG_DBG                             0xCF

/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static DEV_HANDLE devHandle  = NULL;

static void Internal_Msg_Report_CB(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg);
static void Msg_CF_Dbg_Report_CB(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg);

/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */


int main(void)
{
    int status = HOST_ERRCODE_SUCCESS;

    HIF_LOG_PRINT("Host port sample\n");
    HIF_LOG_PRINT("host time meas\n");
    HIF_LOG_PRINT("-------------------------------------------\n");


    do {
        printf("1>. Config HIF driver\n");
        DevHw_t devHw;
        HifCfg_t hifCfg = {
            .cmd_buf_len            = HIF_CFG_CMD_BUFF_SIZ,
//            .tl_retry_enable        = HIF_CFG_TL_RETRY_ENA,
//            .fragment_enable        = HIF_CFG_FRAGMENT_ENA,
//            .fragment_retry_enable  = HIF_CFG_FRAEMENT_RETRY_ENA,
//            .app_retry_enable       = HIF_CFG_APP_RETRY_ENA,
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
        HIF_HANDLE dev = Host_HIF_Handle_Get(devHandle);
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
        status = HIF_Report_Handle_Regist(dev, HIF_MSG_ID_MOTION_DATA, Internal_Msg_Report_CB, devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_ID_MOTION_DATA, status);
            break;
        }

        HIF_LOG_PRINT("\tRegister 0x%02X\n", HIF_MSG_DBG);
        status = HIF_Report_Handle_Regist(dev, HIF_MSG_DBG, Msg_CF_Dbg_Report_CB, devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("regist %d report handle fail(%d)\n", HIF_MSG_DBG, status);
            break;
        }



        HIF_LOG_PRINT("3>. Open HIF driver\n");
        status = Host_Device_Open(devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("open device(%p) fail(%d)\n", devHandle, status);
            break;
        }


    } while (0);


    if (status != HOST_ERRCODE_SUCCESS) {
        HIF_LOG_PRINT("4>. Init fail, close and deinit driver\n");
        Host_Device_Close(devHandle);
        HIF_Report_Handle_Regist(Host_HIF_Handle_Get(devHandle), HIF_MSG_ID_MOTION_DATA, NULL, NULL);
        HIF_Report_Handle_Regist(Host_HIF_Handle_Get(devHandle), HIF_MSG_DBG, NULL, NULL);
        Host_Device_Unregist(devHandle);
        Host_Driver_Deinit();
    } else {
        HIF_LOG_PRINT("SUCCESS\n");
    }

    HIF_LOG_PRINT("-------------------------------------------\n");



    return 0;
}



static void Msg_CF_Dbg_Report_CB(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg)
{
    /* payload:
     * type[1B]:
     * - Tick[4B] : 1
     * - Time[44B] : 2
     *      - idx[4B]
     *      - avg [20B]
     *          - Report Latency [4B]
     *          - Poll Delay [4B]
     *          - Packet Head Delay [4B]
     *          - Guard Time [4B]
     *          - SPI Clock [4B]
     *      - max [20B]
     *          - Report Latency [4B]
     *          - Poll Delay [4B]
     *          - Packet Head Delay [4B]
     *          - Guard Time [4B]
     *          - SPI Clock [4B]
     */

    if ((payload == NULL) || (msg_id != HIF_MSG_DBG)) {
        HIF_LOG_PRINT("Msg(0x%02X) or payload(%p) error\n", msg_id, payload);
        return;
    }

    if (payload[0] == 1) {
        if (payload_len < 5) {
            HIF_LOG_PRINT("Len error: %d\n", payload_len);
            return;
        }

        uint32_t *pTick = (uint32_t *)&payload[1];

        HIF_LOG_PRINT("Tick: %d\n", *pTick);
    } else if (payload[0] == 2) {
        if (payload_len < 45) {
            HIF_LOG_PRINT("Len error: %d\n", payload_len);
            return;
        }

        uint32_t *pBuff = (uint32_t *)&payload[1];

        HIF_LOG_PRINT("idx[%d]:\n", pBuff[0]);

        HIF_LOG_PRINT("%16s: %12s %12s\n", "type", "average (us)", "max (us)");
        HIF_LOG_PRINT("%16s: %12d %12d\n", "report-latency", pBuff[1], pBuff[6]);
        HIF_LOG_PRINT("%16s: %12d %12d\n", "poll-delay", pBuff[2], pBuff[7]);
        HIF_LOG_PRINT("%16s: %12d %12d\n", "head-delay", pBuff[3], pBuff[8]);
        HIF_LOG_PRINT("%16s: %12d %12d\n", "guard-time", pBuff[4], pBuff[9]);
        HIF_LOG_PRINT("%16s: %12d %12d\n\n", "spi-clock(MHz)", pBuff[5], pBuff[10]);

    }
}


static void Internal_Msg_Report_CB(uint32_t msg_id, uint8_t *payload, uint32_t payload_len, void *arg)
{
    //static uint64_t counter[4] = { 0, 0, 0, 0 };
    static uint32_t last_frame_idx[3] = { 0 - 1, 0 - 1, 0 - 1 };
    if (msg_id == 0xC6) {
        //HIF_LOG_PRINT("[%llu] device(%p) recv 0x%02X msg\n", counter[3]++, arg, msg_id);
        HIF_LOG_PRINT("payload len = %u\n", payload_len);
    } else {
        FrameHdr_Data_t *frame = (FrameHdr_Data_t *)payload;

        if (frame != NULL) {
            if (frame->frameIdx == last_frame_idx[msg_id-0xC1] + 1) {
                // HIF_LOG_PRINT("[%llu][%u] device(%p) recved %02X data(%p), len=%u\n",
                //              counter[msg_id-0xC1]++, frame->frameIdx, arg, msg_id, payload, payload_len);
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
