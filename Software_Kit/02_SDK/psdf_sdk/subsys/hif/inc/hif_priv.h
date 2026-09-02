/**
 ******************************************************************************
 * @file    hif_priv.h
 * @brief   hif private define define.
 ******************************************************************************
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
 ******************************************************************************
 */

#ifndef _HIF_PRIVATE_H_
#define _HIF_PRIVATE_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "hif_config.h"
#include "hif_types.h"
#include "hif_mem.h"
#include "hif.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    HIF_Frame_DoubleHead_t       put;
    HIF_Frame_DoubleHead_t       freePending;
#if (CONFIG_HIF_RETRY_ENA != 0)
    HIF_Frame_DoubleHead_t       ackPending;
    HIF_Frame_DoubleHead_t       retry;
#endif

    HIF_Frame_Node_t            *pCmdRspFrame;
    HIF_Data_Node_t             *pCmdRspHead;
    HIF_Data_Node_t             *pCmdRspData;
    HIF_Data_Node_t             *pCmdRspTail;

    uint8_t                     *pCmdRspFreeBuff;
    uint8_t                     *pCmdRspBuff;
    uint16_t                     CmdRspSize;

    HIF_Frame_Node_t            *pEndFrame;
    HIF_Data_Node_t             *pEndData;

        #define HIF_EVENT_NULL                  0x0000
        #define HIF_EVENT_FAULT                 0x0001
        #define HIF_EVENT_NO_RSP                0x0002
        #define HIF_EVENT_CMD_RECV_DONE         0x0010
        #define HIF_EVENT_RSP_SEND_DONE         0x0020
        #define HIF_EVENT_WAKEUP                0x0040
        #define HIF_EVENT_RSP_SKIP              0x0080
        #define HIF_EVENT_REPORT_POLL           0x0100
        #define HIF_EVENT_REPORT_REFRESH        0x0200
        #define HIF_EVENT_REPORT_DONE           0x0400
        #define HIF_EVENT_REPORT_POLL_NOACK     0x0800
        #define HIF_EVENT_SLEEP                 0x1000
        #define HIF_EVENT_POLL_ACK_RETRY        0x2000
        #define HIF_EVENT_ALL                   0xFFFF
    uint32_t                    event;

    OSI_Semaphore_t             semProc;
    OSI_Semaphore_t             semOper;
    OSI_Semaphore_t             semExit;
    OSI_Thread_t                hifTaskHandle;

    uint8_t                     startupState;

        #define HIF_TIMER_CMDRSP_SEND           0x01
        #define HIF_TIMER_CMDRSP_SLEEP          0x02
        #define HIF_TIMER_CMDRSP_MSK            0xF0

        #define HIF_TIMER_REPORT_SEND           0x10
        #define HIF_TIMER_REPORT_SLEEP          0x20
        #define HIF_TIMER_REPORT_DELAY          0x40
        #define HIF_TIMER_REPORT_MSK            0x0F
    uint8_t                     timerState;

        #define HIF_PM_STATE_CMDRSP_PREVENT     0x10
        #define HIF_PM_STATE_REPORT_PREVENT     0x20
        #define HIF_PM_STATE_APP_PREVENT        0x40
        #define HIF_PM_STATE_PREVENT_MSK        0xF0
        #define HIF_PM_STATE_SLEEP_REQ          0x01
        #define HIF_PM_STATE_WAKE_REQ           0x02
    uint8_t                     pmState;

        #define HIF_REPORT_STATE_IDLE               0
        #define HIF_REPORT_STATE_NOTIFY             1
        #define HIF_REPORT_STATE_SEND               2
        #define HIF_REPORT_STATE_WAIT_CONTINUE_DATA 3
        #define HIF_REPORT_STATE_WAIT_SEND_DONE     4

    uint8_t                     reportState;

        #define HIF_REPORT_PAUSE                0x01        /*< Pause transport-layer to put report frame into dll */
        #define HIF_REPORT_PREVENT              0x02        /*< Prevent application to send data */
    uint8_t                     reportPrevent;

    uint8_t                     txSeq;

    uint8_t                     putNodeCnt;

    uint8_t                     msgRecvStartLock;

    uint32_t                    burstPeriod;

    uint32_t                    fault;

    uint32_t                    responseCnt;
}HIF_Ctrl_t;

extern HIF_CfgParam_t hifCfg;
extern HIF_Ctrl_t hifCtrl;

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

/* Exported macro.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
void hif_EVENT_Set(uint32_t event);

void hif_IO_Set(void);

void hif_IO_Clr(void);

#ifdef __cplusplus
}
#endif

#endif /* _HIF_PRIVATE_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
