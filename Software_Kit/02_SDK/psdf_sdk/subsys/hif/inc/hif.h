/**
 ******************************************************************************
 * @file    hif.h
 * @brief   hif define.
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

#ifndef _HIF_H
#define _HIF_H

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "hif_types.h"
#include "hif_msg.h"
#include "hif_mem.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported macro.
 * ----------------------------------------------------------------------------
 */

/* Exported types.
 * ----------------------------------------------------------------------------
 */
typedef enum {
    HIF_COM_TYPE_DIS                        = 0,
    HIF_COM_TYPE_UART                       = 1,
    HIF_COM_TYPE_IIC                        = 2,
    HIF_COM_TYPE_SPI                        = 3,
    HIF_COM_TYPE_CAN                        = 4,
} HIF_ComType_t;

typedef enum {
    HIF_IO_L                                = 0,
    HIF_IO_H                                = 1,
} HIF_IOLevel_t;

typedef enum {
    HIF_WAKE_TYPE_DIS                       = 0,
    HIF_WAKE_TYPE_COM                       = 1,
    HIF_WAKE_TYPE_IO                        = 2,
} HIF_WakeType_t;

typedef enum {
    HIF_NOTIFY_TYPE_DIS                     = 0,
    HIF_NOTIFY_TYPE_IO                      = 1,
    HIF_NOTIFY_TYPE_DLY                     = 2,
} HIF_NotifyType_t;

typedef enum {
    HIF_REPORT_MODE_DISABLE                 = 0,
    HIF_REPORT_MODE_ACTIOVE                 = 1,
    HIF_REPORT_MODE_PASSIVE                 = 2,
} HIF_ReportMode_t;

typedef struct {
    /* com
     */
    HIF_ComType_t       comType;
    uint32_t            comId;
    uint32_t            comRate;
    /* uart :
     *      start bit, data bit, stop bit, parity
     *
     * i2c :
     *      address
     * spi :
     *      line:
     *           0: stand
     *           1: dual
     *           2: quad
     */
    uint32_t            comParam;

    HIF_ReportMode_t    reportMode;

    /* device notify host
     */
    HIF_NotifyType_t    notifyType;
    HIF_IOLevel_t       notifyIoLevel;
    uint32_t            notifyIoNum;

    /* host wakeup device
     */
    HIF_WakeType_t      wakeType;
    HIF_IOLevel_t       wakeIoLevel;
    uint32_t            wakeIoNum;
    uint32_t            wakeComParam;

    /* timeout
     */
    /* begin receive frame magic to all frame
     */
    uint32_t            recvFrameTo;
    /*
     * from the start of sending to successful sending.
     */
    uint32_t            sendFrameTo;
    /* configuration phase
     * after waking up the device but before sending any cmd,
     * if a timeout occurs, this is marked as a false wake-up.
     */
    uint32_t            sendDelay;
    /* configuration phase
     * a series of cmd were sent, but the sleep cmd was not sent for a long time.
     * after this timeout, the system will auto enter sleep mode.
     *
     * calc start from the resoinse to the last cmd.
     */
    uint32_t            sleepTo;

    uint32_t            splitSize;
    uint32_t            splitTotalSize;
    uint32_t            splitEna;
    uint32_t            splitRetryEna;

    uint32_t            reportInitStatus;

    uint32_t            retryEna;

    uint32_t            burstEna;

    uint32_t            sendMode;
    uint32_t            recvMode;

    uint32_t            cmdrspBuffSize;

    uint32_t            frameNodeSize;
    uint32_t            dataNodeSize;
    uint32_t            semNodeSize;

    uint32_t            taskPrio;
    uint32_t            taskStackSize;

    uint32_t            cascadeThresh;

    uint32_t            msgNodeSize;

    uint32_t            sendNodeLevel;
} HIF_CfgParam_t;

typedef enum {
    /* set */
    HIF_SET_COM_TYPE = 0x0100,  /* comType */
    HIF_SET_COM_ID,             /* comId */
    HIF_SET_COM_RATE,           /* comRate */
    HIF_SET_COM_PARAM,          /* comParam */

    HIF_SET_REPORT_MODE,        /* reportMode */

    HIF_SET_NOTIFY_TYPE,        /* notifyType */
    HIF_SET_NOTIFY_IO_LEVEL,    /* notifyIoLevel */
    HIF_SET_NOTIFY_IO_NUM,      /* notifyIoNum */

    HIF_SET_WAKE_TYPE,          /* wakeType */
    HIF_SET_WAKE_IO_LEVEL,      /* wakeIoLevel */
    HIF_SET_WAKE_IO_NUM,        /* wakeIoNum */
    HIF_SET_WAKE_COM_PARAM,     /* wakeComParam */

    HIF_SET_RECV_FRAME_TO,      /* recvFrameTo */
    HIF_SET_SEND_FRAME_TO,      /* sendFrameTo */
    HIF_SET_SEND_DELAY,         /* sendDelay */
    HIF_SET_SLEEP_TO,           /* sleepTo */

    HIF_SET_SPLIT_SIZE,         /* splitSize */
    HIF_SET_SPLIT_TOTAL_SIZE,   /* splitTotalSize */
    HIF_SET_SPLIT_ENA,          /* splitEna */
    HIF_SET_SPLIT_RETRY_ENA,    /* splitRetryEna */

    HIF_SET_REPORT_INIT_STATUS, /* reportInitStatus */

    HIF_SET_RETRY_ENA,          /* retryEna */
    HIF_SET_BURST_ENA,          /* burstEna */

    HIF_SET_SEND_MODE,          /* sendMode */
    HIF_SET_RECV_MODE,          /* recvMode */

    HIF_SET_CMDRSP_BUFF_SIZE,   /* cmdrspBuffSize */
    HIF_SET_FRAME_POOL_SIZE,    /* frameNodeSize */
    HIF_SET_DATA_POOL_SIZE,     /* dataNodeSize */
    HIF_SET_SEM_POOL_SIZE,      /* semNodeSize */
    HIF_SET_TASK_PRIO,          /* taskPrio */
    HIF_SET_TASK_SIZE_SIZE,     /* taskStackSize */
    HIF_SET_CASCADE_THRESH,     /* cascadeThresh */

    HIF_SET_MSG_POOL_SIZE,      /* msgNodeSize */

    HIF_SET_SEND_NODE_LEVEL,    /* trigget send node level */

    HIF_SET_CFG_PARAM,          /* HIF_CfgParam_t */


    /* get */
    HIF_GET_COM_TYPE = 0x0200,  /* comType */
    HIF_GET_COM_ID,             /* comId */
    HIF_GET_COM_RATE,           /* comRate */
    HIF_GET_COM_PARAM,          /* comParam */

    HIF_GET_REPORT_MODE,        /* reportMode */

    HIF_GET_NOTIFY_TYPE,        /* notifyType */
    HIF_GET_NOTIFY_IO_LEVEL,    /* notifyIoLevel */
    HIF_GET_NOTIFY_IO_NUM,      /* notifyIoNum */

    HIF_GET_WAKE_TYPE,          /* wakeType */
    HIF_GET_WAKE_IO_LEVEL,      /* wakeIoLevel */
    HIF_GET_WAKE_IO_NUM,        /* wakeIoNum */
    HIF_GET_WAKE_COM_PARAM,     /* wakeComParam */

    HIF_GET_RECV_FRAME_TO,      /* recvFrameTo */
    HIF_GET_SEND_FRAME_TO,      /* sendFrameTo */
    HIF_GET_SEND_DELAY,         /* sendDelay */
    HIF_GET_SLEEP_TO,           /* sleepTo */

    HIF_GET_SPLIT_SIZE,         /* splitSize */
    HIF_GET_SPLIT_TOTAL_SIZE,   /* splitTotalSize */
    HIF_GET_SPLIT_ENA,          /* splitEna */
    HIF_GET_SPLIT_RETRY_ENA,    /* splitRetryEna */

    HIF_GET_REPORT_INIT_STATUS, /* reportInitStatus */

    HIF_GET_RETRY_ENA,          /* retryEna */
    HIF_GET_BURST_ENA,          /* burstEna */

    HIF_GET_SEND_MODE,          /* sendMode */
    HIF_GET_RECV_MODE,          /* recvMode */

    HIF_GET_CMDRSP_BUFF_SIZE,   /* cmdrspBuffSize */
    HIF_GET_FRAME_POOL_SIZE,    /* frameNodeSize */
    HIF_GET_DATA_POOL_SIZE,     /* dataNodeSize */
    HIF_GET_SEM_POOL_SIZE,      /* semNodeSize */
    HIF_GET_TASK_PRIO,          /* taskPrio */
    HIF_GET_TASK_SIZE_SIZE,     /* taskStackSize */
    HIF_GET_CASCADE_THRESH,     /* cascadeThresh */

    HIF_GET_MSG_POOL_SIZE,      /* msgNodeSize */

    HIF_GET_SEND_NODE_LEVEL,    /* trigget send node level*/

    HIF_GET_CFG_PARAM,

} HIF_Attr_t;


typedef HIF_Data_DoubleHead_t   HIF_Data_ListHead_t;

/**
 * @brief Callbck type
 * @param msgId Message ID
 * @param pDataListHead Should use HIF_MsgReport_ListPopData() to
 *                      get and manage all data pointers(user data address) in the list.
 * @param status Sending status.
 */
typedef void (* HIF_MsgTrans_Callback)(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status);

/* Exported constants.
 * ----------------------------------------------------------------------------
 */

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

/**
 * @brief Init HIF according to config param
 * @param pInitParam The config param
 * @return int
 */
int HIF_Init(
    HIF_CfgParam_t *pInitParam);

/**
 * @brief Deinit HIF
 * @return int
 */
int HIF_DeInit(void);

/**
 * @brief Init default HIF config param
 * @param cfgParam The config param variable(pointer) for storing default HIF config param
 * @param comType Communication type
 * @param comRate Communication rate
 * @return int
 */
int HIF_DefaultInitParam(
    HIF_CfgParam_t * cfgParam,
    HIF_ComType_t comType,
    uint32_t comRate);

int HIF_ExtControl(
    HIF_Attr_t attr,
    void * arg,
    uint32_t argSize);

int HIF_InitStatusReport(
    uint8_t status);

/**
 * @brief Register the handler of a message ID(received).
 * @param msgId The message ID.
 * @param msgHdl The message-received handler. Is called if receiving the message.
 * @return int
 * @attention MUST use HIF_MsgResp() to respond to host in the message-received handler.
 */
int HIF_MsgHdl_Regist(
    uint8_t msgId,
    HIF_MsgHdl_t msgHdl);

/**
 * @brief Call this function to respond to host in a message-received handler
 * @param pMsg Message handler
 * @param dataLen The length of data responded to host
 * @param status The ack status responded to host
 * @return int
 */
int HIF_MsgResp(
    HIF_MsgHdr_t *pMsg,
    uint16_t dataLen,
    uint8_t status);

/**
 * @brief Sends data
 * @param msgId Message id
 * @param pData Pointer pointing to the data
 * @param len The Length of the data
 * @param sentCb Sending callback. Should use HIF_MsgReport_ListPopData() to get and manage the data pointers
 *              in the sentCb. sentCb counld be NULL(blocked sending).
 * @return int
 * @attention If sentCb is NULL, Manage(e.g., free memory) the data pointer after this function return.
 */
int HIF_MsgReport(
    uint8_t msgId,
    void * pData,
    uint32_t len,
    HIF_MsgTrans_Callback sentCb);

/**
 * @brief Push a data pointer at the tail of a list.
 * @param pDataListHead The list
 * @param pData The data pointer
 * @param len The length of data pointed by the pointer
 * @return int
 */
int HIF_MsgReport_ListPushData(
    HIF_Data_ListHead_t * pDataListHead,
    void *data,
    uint16_t len);

/**
 * @brief Pop a data pointer from the head of a list
 * @param pDataListHead The list
 * @return uint8_t*, the data pointer. If NULL, Means there is not pointer in the list.
 */
uint8_t *HIF_MsgReport_ListPopData(
    HIF_Data_ListHead_t * pDataListHead);

/**
 * @brief Sends a data list
 * @param msgId Message ID
 * @param pDataListHead Pointer to the data list
 * @param sentCb Sending callback. Should use HIF_MsgReport_ListPopData() to get and manage all data pointers
 *               in the list in the sentCb. sentCb counld be NULL(blocked sending).
 * @return int
 * @attention If return NOT HIF_ERRCODE_SUCCESS, Caller Must use HIF_MsgReport_ListPopData() to get all data pointers;
 *            If return HIF_ERRCODE_SUCCESS && sentCb is NULL, Caller manages all data pointers
 *                  in the list(Do NOT use HIF_MsgReport_ListPopData() to get pointers);
 */
int HIF_MsgReport_ListStart(
    uint8_t msgId,
    HIF_Data_ListHead_t * pDataListHead,
    HIF_MsgTrans_Callback sentCb);

/**
 * @brief Flush Frame waiting for sending
 * @param flushEnable !0 - Flush, 0 - No Flush
 * @return int
 * @attention Used by mesage ID: 0xC0 ~ 0xCF
 */
int HIF_MsgReport_Flush(uint8_t flushEnable);

void HIF_MsgReport_Lock(void);

void HIF_MsgReport_Unlock(void);

void HIF_PM_WakeupDevice(void);

void HIF_PM_SuspendDevice(void);

void hif_TL_RefreshFrame(HIF_Frame_Node_t *pFrame);

uint32_t hif_msg_response_cnt_get(void);

void hif_tl_retry(uint8_t errSeqNum, uint8_t *errSeqArray);


#if CONFIG_HIF_TL_DEBUG_EN

typedef struct hif_tl_debug_info_t_ {
    uint32_t currentCpuId;
    uint32_t cpuResetFlag;
    uint32_t frameCnt_from_app;
    uint32_t frameCnt_to_app;
    uint32_t frameCnt_to_dll;
    uint32_t frameCnt_from_dll;
    uint32_t frameCnt_from_ml;
    uint32_t frameCnt_to_ml;
    uint32_t taskConsecutiveCpuTimeMax;
} hif_tl_debug_info_t;

extern hif_tl_debug_info_t hif_tl_debug_info;

void hif_tl_debug_info_print(void);
void hif_tl_global_state_print(void);

#else

#define hif_tl_debug_info_print(void)

#define hif_tl_global_state_print(void)

#endif

#ifdef __cplusplus
}
#endif

#endif /* _HIF_H */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
