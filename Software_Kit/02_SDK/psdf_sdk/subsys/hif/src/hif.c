/**
 ******************************************************************************
 * @file    hif.c
 * @brief   hif define.
 * @verbatim    null
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


/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "hif_types.h"
#include "hif_config.h"

#if (CONFIG_HIF == 1)
#include "hif.h"
#include "hif_checksum.h"
#include "hif_mem.h"
#include "hif_io.h"
#include "hif_pm.h"

#include "hif_log.h"

#include "hif_phy.h"
#include "hif_dll.h"

#include "hif_ml.h"
#include "hif_priv.h"
#include "hif_tl.h"

#include "hal_board.h"

#if CONFIG_HIF_CMD
#include "hif_cmd.h"
#endif

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
    #define HIF_STATE_IDLE          0
    #define HIF_STATE_INIT          1
    #define HIF_STATE_DEINIT        2
    #define HIF_STATE_INIT_SUCCESS  3
    #define HIF_STATE_INIT_FAIL     4
    #define HIF_STATE_DEINIT_FAIL   5
static uint8_t hifCtrlState;

static uint8_t hifCfgState;

HIF_CfgParam_t hifCfg;
HIF_Ctrl_t hifCtrl;

#if CONFIG_HIF_TL_DEBUG_EN
hif_tl_debug_info_t hif_tl_debug_info = {0};
#endif


const uint8_t hifNullPacket[HIF_HEAD_LEN] = {0xA5, 0x4B, 0x03, 0x0C, 0x00, 0x00};

/* Private defines.
 * ----------------------------------------------------------------------------
 */
#ifndef BIT
#define BIT(n)                      (1U<<(n))
#endif

#ifndef MIN
#define MIN(x, y)                    ((x) < (y) ? (x) : (y))
#endif

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static int hif_SpecFrame_Init(uint32_t buffSize);
static void hif_SpecFrame_Deinit(void);

static int hif_TL_SendCallback(HIF_Frame_Node_t * pFrameNode, uint32_t flag);
static int hif_TL_RecvCallback(HIF_Frame_Node_t * pFrameNode, uint32_t flag);

static void hif_MSG_UpdateChecksum(HIF_Frame_Node_t *pFrameNode);
static OSI_Time_t hif_TIME_Get(void);

static void hif_Task(void *arg);

static void hif_RPT_Flush(void);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int HIF_DefaultInitParam(HIF_CfgParam_t * cfgParam, HIF_ComType_t comType, uint32_t comRate)
{
    if (cfgParam == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    cfgParam->comType           = comType;
    cfgParam->comRate           = comRate;

    switch (cfgParam->comType) {
#if (CONFIG_HIF_PHY_UART != 0)
    case HIF_COM_TYPE_UART:
    {
        cfgParam->comId         = CONFIG_HIF_PHY_UART_DEF_NUM;
        cfgParam->comParam      = 0;
        cfgParam->wakeComParam  = CONFIG_HIF_PM_UART_DEF_WAKE_THRESH;
        cfgParam->notifyType    = HIF_NOTIFY_TYPE_DIS;
        cfgParam->reportMode    = HIF_REPORT_MODE_ACTIOVE;
        cfgParam->sendMode      = HIF_PHY_TRANS_MODE_DMA;
        cfgParam->recvMode      = HIF_PHY_TRANS_MODE_INT;
        if (cfgParam->comRate == 0) {
            cfgParam->comRate   = CONFIG_HIF_PHY_UART_DEF_RATE;
        }
        cfgParam->burstEna      = 0;
        break;
    }
#endif

#if (CONFIG_HIF_PHY_IIC != 0)
    case HIF_COM_TYPE_IIC:
    {
        cfgParam->comId         = CONFIG_HIF_PHY_IIC_DEF_NUM;
        cfgParam->comParam      = CONFIG_HIF_PHY_IIC_DEF_ADDR;
        cfgParam->wakeComParam  = CONFIG_HIF_PHY_IIC_DEF_ADDR;
        cfgParam->notifyType    = HIF_NOTIFY_TYPE_IO;
        cfgParam->reportMode    = HIF_REPORT_MODE_PASSIVE;
        cfgParam->sendMode      = HIF_PHY_TRANS_MODE_INT;
        cfgParam->recvMode      = HIF_PHY_TRANS_MODE_INT;
        if (cfgParam->comRate == 0) {
            cfgParam->comRate   = CONFIG_HIF_PHY_IIC_DEF_RATE;
        }
        cfgParam->burstEna      = 1;
        break;
    }
#endif

#if (CONFIG_HIF_PHY_SPI != 0)
    case HIF_COM_TYPE_SPI:
    {
        cfgParam->comId         = CONFIG_HIF_PHY_SPI_DEF_NUM;
        cfgParam->comParam      = CONFIG_HIF_PHY_SPI_DEF_LINE; /* 0: stand, 1: dual, 2: quad */
        cfgParam->wakeComParam  = CONFIG_HIF_PM_SPI_DEF_WAKE_THRESH;
        cfgParam->notifyType    = HIF_NOTIFY_TYPE_IO;
        cfgParam->reportMode    = HIF_REPORT_MODE_PASSIVE;
        cfgParam->sendMode      = HIF_PHY_TRANS_MODE_DMA;
        cfgParam->recvMode      = HIF_PHY_TRANS_MODE_DMA;
        if (cfgParam->comRate == 0) {
            cfgParam->comRate   = CONFIG_HIF_PHY_SPI_DEF_RATE;
        }
        cfgParam->burstEna      = 1;
        break;
    }
#endif

#if (CONFIG_HIF_PHY_CAN != 0)
    case HIF_COM_TYPE_CAN:
    {
        cfgParam->comId         = CONFIG_HIF_PHY_CAN_DEF_NUM;
        cfgParam->comParam      = 0;
        cfgParam->wakeComParam  = 0;
        cfgParam->notifyType    = HIF_NOTIFY_TYPE_IO;
        cfgParam->reportMode    = HIF_REPORT_MODE_PASSIVE;
        cfgParam->sendMode      = HIF_PHY_TRANS_MODE_INT;
        cfgParam->recvMode      = HIF_PHY_TRANS_MODE_INT;
        if (cfgParam->comRate == 0) {
            cfgParam->comRate   = CONFIG_HIF_PHY_CAN_DEF_RATE;
        }
        cfgParam->burstEna      = 0;
        break;
    }
#endif

    case HIF_COM_TYPE_DIS:
    {
        cfgParam->comParam      = 0;
        cfgParam->notifyType    = HIF_NOTIFY_TYPE_IO;
        cfgParam->reportMode    = HIF_REPORT_MODE_DISABLE;
        break;
    }

    default:
        return HIF_ERRCODE_INVALID_PARAM;
        break;
    }

    cfgParam->notifyIoLevel     = CONFIG_HIF_PM_DEF_NOTIFY_IO_LEVEL;
    cfgParam->notifyIoNum       = CONFIG_HIF_PM_DEF_NOTIFY_IO;
    cfgParam->wakeType          = CONFIG_HIF_PM_DEF_WAKE_MODE;
    cfgParam->wakeIoLevel       = HIF_IO_H;
    cfgParam->wakeIoNum         = 0;
    cfgParam->sendDelay         = CONFIG_HIF_DEF_SEND_DELAY;
    cfgParam->sendFrameTo       = CONFIG_HIF_DEF_SEND_TO;
    cfgParam->recvFrameTo       = CONFIG_HIF_DEF_RECV_TO;
    cfgParam->sleepTo           = CONFIG_HIF_DEF_SLEEP_TO;

    cfgParam->splitSize         = CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE - HIF_ML_FRAG_HDR_LEN;
    cfgParam->splitTotalSize    = CONFIG_HIF_SPLIT_TOTAL_SIZE_DEF;
    cfgParam->splitEna          = CONFIG_HIF_SPLIT_ENA;
    cfgParam->splitRetryEna     = CONFIG_HIF_SEND_SPLIT_RETRY_ENA;

    cfgParam->cmdrspBuffSize    = CONFIG_HIF_MEM_CMD_RSP_SIZE;

    cfgParam->taskPrio          = CONFIG_HIF_TASK_PRIO;
    cfgParam->taskStackSize     = CONFIG_HIF_TASK_STACK_SIZE;

    cfgParam->frameNodeSize     = CONFIG_HIF_MEM_FRAME_NODE_SIZE;
    cfgParam->dataNodeSize      = CONFIG_HIF_MEM_DATA_NODE_SIZE;
    cfgParam->semNodeSize       = CONFIG_HIF_MEM_SEM_NODE_SIZE;

    cfgParam->msgNodeSize       = CONFIG_HIF_MEM_MSG_NODE_SIZE;

    cfgParam->retryEna          = CONFIG_HIF_RETRY_ENA;

    cfgParam->reportInitStatus  = 1;

    cfgParam->cascadeThresh     = CONFIG_HIF_CASCADE_THRESH;

    cfgParam->sendNodeLevel     = 0;

    return HIF_ERRCODE_SUCCESS;
}


int HIF_Init(HIF_CfgParam_t *pInitParam)
{
#if CONFIG_HIF_TL_DEBUG_EN
    hif_tl_debug_info.currentCpuId = csi_get_cpu_id();

    if (hif_tl_debug_info.currentCpuId == 0) {
        hif_tl_debug_info.cpuResetFlag = LL_SYS_GetSys0ResetSource();
    } else if (hif_tl_debug_info.currentCpuId == 1) {
        hif_tl_debug_info.cpuResetFlag = LL_SYS_GetSys1ResetSource();
    } else {
        LOG_PRINT("Unknown cpu id\n");
    }

    LOG_PRINT("\n");
    LOG_PRINT("currentCpuId 0x%d\n", hif_tl_debug_info.currentCpuId);
    LOG_PRINT("cpuResetFlag 0x%08X\n", hif_tl_debug_info.cpuResetFlag);
#endif

    int status = HIF_ERRCODE_SUCCESS;

    /* Do not check pInitParam is NULL
     * allow NULL param to used default configurations insted.
     */

    HIF_LOG_DBG("HIF_Init");

    if (hifCtrlState == HIF_STATE_INIT_SUCCESS) {
        HIF_LOG_DBG("already init");
        return HIF_ERRCODE_SUCCESS;
    }

    if (hifCtrlState != HIF_STATE_IDLE) {
        HIF_LOG_DBG("deinitializing");
        return HIF_ERRCODE_NOT_READY;
    }


    hifCtrlState = HIF_STATE_INIT;
    OSI_Memset(&hifCtrl, 0, sizeof(HIF_Ctrl_t));

    if (pInitParam != NULL) {
        OSI_Memcpy(&hifCfg, pInitParam, sizeof(HIF_CfgParam_t));
        hifCfgState = HIF_STATE_INIT_SUCCESS;
    } else {
        if (hifCfgState == HIF_STATE_IDLE) {
            return HIF_ERRCODE_NOT_READY;
        }
    }


    do {
        /* 1>
         * memory pool init
         */
        HIF_LOG_DBG("memory pool init");
        status = hif_MEM_DataPoolInit(hifCfg.dataNodeSize);
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("data pool init fail: %d", status);
            break;
        }

        status = hif_MEM_FramePoolInit(hifCfg.frameNodeSize);
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("frame pool init fail: %d", status);
            break;
        }

#if CONFIG_HIF_SPLIT_ENA
        status = hif_MEM_MsgPoolInit(hifCfg.msgNodeSize);
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("msg pool init fail: %d", status);
            break;
        }
#endif

        status = hif_MEM_SemPoolInit(hifCfg.semNodeSize);
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("sem pool init fail: %d", status);
            break;
        }

        status = hif_LOG_Init(CONFIG_HIF_LOG_IO_MEM_SIZE);
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("log init fail: %d", status);
            break;
        }


        /* 2>
         * report msg queue init
         */
        HIF_LOG_DBG("report msg queue init");
        hif_FRAME_Init(&hifCtrl.put);
        hifCtrl.putNodeCnt = 0;
        hif_FRAME_Init(&hifCtrl.freePending);
#if (CONFIG_HIF_RETRY_ENA != 0)
        hif_FRAME_Init(&hifCtrl.ackPending);
        hif_FRAME_Init(&hifCtrl.retry);
#endif


        /* 3>
         * msg handle init
         */
        HIF_LOG_DBG("msg handle init");
        status = HIF_MsgHdl_Init();
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }

        /* 4>
         * DLL(Data link) & PHY init
         */
        status = hif_SpecFrame_Init(hifCfg.cmdrspBuffSize);
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }

        HIF_PHY_InitCfg_t hifPHYInitCfg = {
            .phyType        = hifCfg.comType,
            .devId          = hifCfg.comId,
            .transFreq      = hifCfg.comRate,
            .transCfg       = hifCfg.comParam,
            .sendMode       = (uint8_t)hifCfg.sendMode,
            .recvMode       = (uint8_t)hifCfg.recvMode,
        };

        HIF_DLL_InitCfg_t hifDLLInitCfg = {
            .sendCb         = hif_TL_SendCallback,
            .recvCb         = hif_TL_RecvCallback,
            .cascadeThresh  = hifCfg.cascadeThresh,
            .pPHYCfg        = &hifPHYInitCfg,
        };

        status = hif_DLL_Init(&hifDLLInitCfg);
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }

        HIF_LOG_DBG("hif_DLL_Open");
        status = hif_DLL_Open();
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }

#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
        HIF_LOG_DBG("hif_tl_init");
        status = hif_tl_init();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_DBG("hif_tl_init fail");
            break;
        }
#endif

#if CONFIG_HIF_SPLIT_ENA
        HIF_LOG_DBG("hif_ml_init");
        status = hif_ml_init();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_DBG("hif_ml_init fail");
            break;
        }
#endif

        /* 5>
         * PM init
         */
        HIF_LOG_DBG("pm init");

#if (CONFIG_HIF_PM == 1)
        HIF_PM_InitCfg_t hifPmInitCfg= {
            .wakeType       = (uint8_t)hifCfg.wakeType,
            .wakeIoNum      = (uint8_t)hifCfg.wakeIoNum,
            .wakeIoLevel    = (uint8_t)hifCfg.wakeIoLevel,
            .wakeComParam   = (uint8_t)hifCfg.wakeComParam,
        };
        status = hif_PM_Init(&hifPmInitCfg);
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }
#endif

        /* 6>
         * IO init
         */
        HIF_LOG_DBG("io init");
        if (hifCfg.notifyType == HIF_NOTIFY_TYPE_IO || hifCfg.notifyType == HIF_NOTIFY_TYPE_DLY) {
            status = hif_IO_Init(hifCfg.notifyIoNum);
            if (status != HIF_ERRCODE_SUCCESS) {
                break;
            }
            hif_IO_Clr();
        }

        /* 7>
         * task init
         */
        HIF_LOG_DBG("task init");
        OSI_Status_t osiStatus = OSI_STATUS_OK;

        OSI_SemaphoreSetInvalid(&hifCtrl.semProc);
        osiStatus = OSI_SemaphoreCreate(&hifCtrl.semProc, 0, 5);
        if (osiStatus != OSI_STATUS_OK) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        OSI_SemaphoreSetInvalid(&hifCtrl.semOper);
        osiStatus = OSI_SemaphoreCreate(&hifCtrl.semOper, 1, 1);
        if (osiStatus != OSI_STATUS_OK) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        OSI_SemaphoreSetInvalid(&hifCtrl.semExit);
        osiStatus = OSI_SemaphoreCreate(&hifCtrl.semExit, 0, 1);
        if (osiStatus != OSI_STATUS_OK) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        OSI_ThreadSetInvalid(&hifCtrl.hifTaskHandle);
        osiStatus = OSI_ThreadCreate(&hifCtrl.hifTaskHandle,
            "hif task",
            (OSI_ThreadEntry_t)hif_Task,
            NULL,
            hifCfg.taskPrio,
            hifCfg.taskStackSize);
        if (osiStatus != OSI_STATUS_OK) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        hif_LOG_TimeInit();
    } while (0);


    if (status == HIF_ERRCODE_SUCCESS) {
        hifCtrlState = HIF_STATE_INIT_SUCCESS;
    } else {
        hifCtrlState = HIF_STATE_INIT_FAIL;
    }


    return status;
}


int HIF_DeInit(void)
{
    int status = HIF_ERRCODE_SUCCESS;


    HIF_LOG_DBG("HIF_DeInit");
    if (hifCtrlState == HIF_STATE_IDLE) {
        HIF_LOG_DBG("already deinit");
        return HIF_ERRCODE_SUCCESS;
    }

    if (hifCtrlState != HIF_STATE_IDLE) {
        HIF_LOG_DBG("deinitializing");
        return HIF_ERRCODE_NOT_READY;
    }


    hifCtrlState = HIF_STATE_DEINIT;

    do {
        /* 1>
         * task deinit
         */
        HIF_LOG_DBG("task deinit");
        /* hif task already create success */
        if (OSI_ThreadIsValid(&hifCtrl.hifTaskHandle)) {
            /* trig hif task run */
            OSI_SemaphoreRelease(&hifCtrl.semProc);
            /* wait hif taks exit */
            OSI_SemaphoreWait(&hifCtrl.semExit, OSI_WAIT_FOREVER);
        }

        OSI_SemaphoreDelete(&hifCtrl.semExit);
        OSI_SemaphoreDelete(&hifCtrl.semOper);
        OSI_SemaphoreDelete(&hifCtrl.semProc);


        /* 2>
         * IO deinit
         */
        HIF_LOG_DBG("io deinit");
        if (hifCfg.notifyType == HIF_NOTIFY_TYPE_IO || hifCfg.notifyType == HIF_NOTIFY_TYPE_DLY) {
            hif_IO_Clr();
            status = hif_IO_Deinit(hifCfg.notifyIoNum);
            if (status != HIF_ERRCODE_SUCCESS) {
                break;
            }
        }


        /* 3>
         * PM deinit
         */
#if (CONFIG_HIF_PM == 1)
        status = hif_PM_Deinit();
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }
#endif

        /* 4>
         * DLL(Data link) & PHY deinit
         */
        status = hif_DLL_Close();
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }

        status = hif_DLL_Deinit();
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hif_SpecFrame_Deinit();


        /* 5>
          * msg handle init
          */
         HIF_MsgHdl_Deinit();


        /* 6>
         * report msg queue init
         */
        hif_FRAME_Init(&hifCtrl.put);
        hif_FRAME_Init(&hifCtrl.freePending);
#if (CONFIG_HIF_RETRY_ENA != 0)
        hif_FRAME_Init(&hifCtrl.ackPending);
        hif_FRAME_Init(&hifCtrl.retry);
#endif

#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
        HIF_LOG_DBG("hif_tl_deInit");
        status = hif_tl_deInit();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_DBG("hif_tl_deInit fail");
        }
#endif

#if CONFIG_HIF_SPLIT_ENA
        HIF_LOG_DBG("hif_ml_deInit");
        status = hif_ml_deInit();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_DBG("hif_ml_deInit fail");
        }
#endif

        /* 7>
         * memory pool init
         */
        HIF_LOG_DBG("memory pool init");
        status = hif_MEM_DataPoolDeinit();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("data pool init fail: %d", status);
            break;
        }

        status = hif_MEM_FramePoolDeinit();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("frame pool init fail: %d", status);
            break;
        }

#if CONFIG_HIF_SPLIT_ENA
        status = hif_MEM_MsgPoolDeinit();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("msg pool deInit fail: %d", status);
        }
#endif

        status = hif_MEM_SemPoolDeinit();
        if (status != HIF_ERRCODE_SUCCESS) {
            HIF_LOG_ERR("sem pool init fail: %d", status);
            break;
        }


        //HIF_DeInit();
        hif_LOG_TimeDeinit();

    } while (0);


    if (status == HIF_ERRCODE_SUCCESS) {
        OSI_Memset(&hifCtrl, 0, sizeof(HIF_Ctrl_t));
        OSI_Memset(&hifCfg, 0, sizeof(HIF_CfgParam_t));
        hifCtrlState = HIF_STATE_INIT;
        HIF_LOG_DBG("HIF Deinit Success");
    } else {
        hifCtrlState = HIF_STATE_DEINIT_FAIL;
        HIF_LOG_DBG("HIF Deinit Fail");
    }


    return status;
}


int HIF_InitStatusReport(uint8_t status)
{
    hifCtrl.startupState = status;
    return HIF_ERRCODE_SUCCESS;
}

__sram_text uint32_t hif_msg_response_cnt_get(void)
{
    return hifCtrl.responseCnt;
}


__sram_text int HIF_ExtControl(HIF_Attr_t attr, void * arg, uint32_t argSize)
{
    int status = HIF_ERRCODE_SUCCESS;

    if (arg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if (attr == HIF_SET_CFG_PARAM) {
        if (argSize == sizeof(HIF_CfgParam_t)) {
            OSI_Memcpy(&hifCfg, arg, sizeof(HIF_CfgParam_t));
            hifCfgState = HIF_STATE_INIT_SUCCESS;
        } else {
            status = HIF_ERRCODE_INVALID_PARAM;
        }
    } else if (attr == HIF_GET_CFG_PARAM) {
        if (argSize == sizeof(HIF_CfgParam_t)) {
            OSI_Memcpy(arg, &hifCfg, sizeof(HIF_CfgParam_t));
        } else {
            status = HIF_ERRCODE_INVALID_PARAM;
        }
    } else if (attr < HIF_SET_CFG_PARAM) {
        if (argSize == sizeof(uint32_t)) {
            uint32_t *pCfgParam = (uint32_t *)arg;
            uint32_t *pHifCfg = (uint32_t *)&hifCfg;
            uint32_t cfgIdx = attr & 0xFF;
            pHifCfg[cfgIdx] = *pCfgParam;
        } else {
            status = HIF_ERRCODE_INVALID_PARAM;
        }
    } else if (attr < HIF_GET_CFG_PARAM) {
        if (argSize == sizeof(uint32_t)) {
            uint32_t *pCfgParam = (uint32_t *)arg;
            uint32_t *pHifCfg = (uint32_t *)&hifCfg;
            uint32_t cfgIdx = attr & 0xFF;
            *pCfgParam = pHifCfg[cfgIdx];
        } else {
            status = HIF_ERRCODE_INVALID_PARAM;
        }
    } else {
        status = HIF_ERRCODE_INVALID_PARAM;
    }


    return status;
}


int HIF_MsgResp(HIF_MsgHdr_t *pMsg, uint16_t dataLen, uint8_t status)
{
    int retStatus = HIF_ERRCODE_SUCCESS;

    if (pMsg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if (hifCtrlState != HIF_STATE_INIT_SUCCESS) {
        return HIF_ERRCODE_NOT_READY;
    }

    if ((pMsg->flag & HIF_MSG_FLAG_REQ_BIT) == 0) {
        hif_EVENT_Set(HIF_EVENT_NO_RSP);
        return HIF_ERRCODE_SUCCESS;
    }

    /* init frame */
    hif_MEM_FrameNodeInit(hifCtrl.pCmdRspFrame);
    hifCtrl.pCmdRspFrame->flag = HIF_TRANS_FLAG_LAST_FRAME | HIF_FRAME_FLAG_CMD_FRAME | HIF_FRAME_FLAG_REFRESH_DATA;
    hifCtrl.pCmdRspFrame->frameLen = 0;

    /* packet head */
    pMsg->type   = HIF_MSG_TYPE_TO_HOST;
    pMsg->flag  &= ~HIF_MSG_FLAG_REQ_BIT;
    pMsg->frag   = HIF_MSG_FRAG_INVALID;
    pMsg->length = dataLen + sizeof(HIF_MsgGenAck_t); /* status len */

    memcpy(&hifCtrl.pCmdRspFrame->hdr[HIF_PHY_HEAD_LEN], pMsg, HIF_MSG_HEAD_LEN);
    uint8_t check8 = HIF_CheckSum8(HIF_PHY_MSG_MAGIC, (uint8_t *)pMsg, HIF_MSG_HEAD_LEN);
    hifCtrl.pCmdRspFrame->hdr[0] = HIF_PHY_MSG_MAGIC;
    hifCtrl.pCmdRspFrame->hdr[1] = ~check8;

    uint16_t hdrLen = HIF_HEAD_LEN + hif_DLL_GetTransDummy();
    hif_DLL_SendDataNodeInit(hifCtrl.pCmdRspFrame->hdr, hdrLen, hifCtrl.pCmdRspHead, HIF_DATA_NODE_FLAG_HEAD);

    hifCtrl.pCmdRspFrame->frameLen += HIF_HEAD_LEN;
    hif_DATA_Append(&hifCtrl.pCmdRspFrame->listHead, hifCtrl.pCmdRspHead);


    /* packet payload and checksume*/
    HIF_MsgGenAck_t *ack = (HIF_MsgGenAck_t *)(pMsg + 1);
    ack->status = status;
    uint8_t *pPayload = (uint8_t *)ack;
    uint16_t payloadLen = dataLen + 1;

    if (pMsg->flag & HIF_MSG_FLAG_CHECK_BIT) {
        hif_DLL_SendDataNodeInit(pPayload, payloadLen, hifCtrl.pCmdRspData, 0);
        hif_DATA_Append(&hifCtrl.pCmdRspFrame->listHead, hifCtrl.pCmdRspData);

        uint32_t checksum32 = 0;
        uint32_t offset = 0;
        checksum32 = checksum32_calc(pPayload, payloadLen, &offset) + *(uint32_t *)pMsg;
        *(uint32_t *)hifCtrl.pCmdRspFrame->tail = ~checksum32;
        uint16_t taiLen = HIF_CHKEC_LEN + hif_DLL_GetTransDummy();
        hif_DLL_SendDataNodeInit(hifCtrl.pCmdRspFrame->tail, taiLen, hifCtrl.pCmdRspTail, HIF_DATA_NODE_FLAG_LAST);

        hif_DATA_Append(&hifCtrl.pCmdRspFrame->listHead, hifCtrl.pCmdRspTail);
        hifCtrl.pCmdRspFrame->frameLen += payloadLen + HIF_CHKEC_LEN;

    } else {
        hif_DLL_SendDataNodeInit(pPayload, payloadLen + hif_DLL_GetTransDummy(), hifCtrl.pCmdRspData, HIF_DATA_NODE_FLAG_LAST);
        hif_DATA_Append(&hifCtrl.pCmdRspFrame->listHead, hifCtrl.pCmdRspData);
        hifCtrl.pCmdRspFrame->frameLen += payloadLen;
    }


    /* send rsp */
    uint32_t timeout = 0;
    do {
        retStatus = hif_DLL_Send(hifCtrl.pCmdRspFrame);
        if (retStatus == HIF_ERRCODE_NOT_READY_TMP) {
            OSI_MSleep(5);      // Wait x ms
            timeout += 5;
        } else {
            break;
        }
    } while (timeout < hifCfg.sendFrameTo);

    hifCtrl.responseCnt ++;

    return retStatus;
}

__hif_sram_text int HIF_MsgReport(uint8_t msgId, void * pData, uint32_t len, HIF_MsgTrans_Callback sentCb)
{
    if (hifCtrlState != HIF_STATE_INIT_SUCCESS) {
        return HIF_ERRCODE_NOT_READY;
    }

    if (hifCtrl.reportPrevent & HIF_REPORT_PREVENT) {
        return HIF_ERRCODE_NOT_READY;
    }

    if ((pData == NULL) || (len == 0)) {
        return HIF_ERRCODE_INVALID_PARAM;
    }


    int status = HIF_ERRCODE_SUCCESS;
    HIF_Data_ListHead_t dataListHead = {
        .head = NULL,
        .tail = NULL,
    };

#if (CONFIG_HIF_SPLIT_ENA)

    uint32_t dataNodeCnt = 0;
    uint32_t lastDataNodeLen = 0;

    if (len > CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE) {
        if (hifCfg.splitEna) {
            if (len > (hifCfg.splitSize + HIF_ML_FRAG_HDR_LEN)) {
                if (len > hifCfg.splitTotalSize) {
                    return HIF_ERRCODE_INVALID_PARAM;
                }
                dataNodeCnt = len / 0xFFFF;     // uint16_t: 0xFFFF
                lastDataNodeLen = len % 0xFFFF;

                if (lastDataNodeLen) {
                    dataNodeCnt += 1;
                } else {
                    lastDataNodeLen = 0xFFFF;
                }
            } else {
                HIF_LOG_DBG("(hifCfg.splitSize + HIF_ML_FRAG_HDR_LEN) Should <= CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE");
                return HIF_ERRCODE_FAIL;
            }
        } else {
            return HIF_ERRCODE_INVALID_PARAM;
        }
    } else {
        dataNodeCnt = 1;
        lastDataNodeLen = len;
    }

    for (uint32_t i = 0; i < dataNodeCnt; i++) {
        uint32_t dataAddr = (uint32_t)pData + i * 0xFFFF;
        if ((i + 1) < dataNodeCnt) {
            status = HIF_MsgReport_ListPushData(&dataListHead, (void *)dataAddr, 0xFFFF);
        } else {
            status = HIF_MsgReport_ListPushData(&dataListHead, (void *)dataAddr, lastDataNodeLen);
        }

        if (status != HIF_ERRCODE_SUCCESS) {
            while (HIF_MsgReport_ListPopData(&dataListHead)) {;}
            return status;
        } else {
            dataListHead.tail->rsv2 |= (uint16_t)HIF_DATA_NODE_FLAG_MSG_REPORT;
        }
    }

    status = HIF_MsgReport_ListStart(msgId, &dataListHead, sentCb);

    /* free data node */
    if (status != HIF_ERRCODE_SUCCESS) {
        while (HIF_MsgReport_ListPopData(&dataListHead)) {
        }
    } else {
        if (sentCb == NULL) {
            while (HIF_MsgReport_ListPopData(&dataListHead)) {
            }
        }
    }

#else

    if (len > CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    do {
        if (pData != NULL) {
            status = HIF_MsgReport_ListPushData(&dataListHead, pData, len);
            if (status != HIF_ERRCODE_SUCCESS) {
                break;
            }
        }

        status = HIF_MsgReport_ListStart(msgId, &dataListHead, sentCb);
    } while (0);

    if ((status != HIF_ERRCODE_SUCCESS) || (sentCb == NULL)) {
        while (HIF_MsgReport_ListPopData(&dataListHead)) {
        }
    }

#endif


    return status;
}


__hif_sram_text int HIF_MsgReport_ListPushData(HIF_Data_ListHead_t * pDataListHead, void *pData, uint16_t len)
{
    if (hifCtrlState != HIF_STATE_INIT_SUCCESS) {
        return HIF_ERRCODE_NOT_READY;
    }

    if (hifCtrl.reportPrevent & HIF_REPORT_PREVENT) {
        return HIF_ERRCODE_NOT_READY;
    }

    if (pDataListHead == NULL) {
        HIF_LOG_DBG("PUSH null");
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if ((pData == NULL) || (len == 0)) {
        HIF_LOG_DBG("PUSH len");
        return HIF_ERRCODE_INVALID_PARAM;
    }

#if (CONFIG_HIF_SPLIT_ENA == 0)
    if (len > CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE) {
        HIF_LOG_DBG("PUSH len too long");
        return HIF_ERRCODE_INVALID_PARAM;
    }
#endif

    HIF_Data_Node_t *pDataNode = hif_MEM_DataNodeMalloc();
    if(pDataNode == NULL) {
        HIF_LOG_DBG("PUSH no data node");
        return HIF_ERRCODE_NO_BUFFER;
    }

#if (CONFIG_HIF_SPLIT_ENA)
    if (len <= CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE) {
        hif_DLL_SendDataNodeInit(pData, len, pDataNode, 0);
    } else {
        hif_DLL_SendDataNodeInit(pData, len, pDataNode, HIF_DATA_NODE_FLAG_NOT_DLL_INIT);
    }
#else
    hif_DLL_SendDataNodeInit(pData, len, pDataNode, 0);
#endif

    HIF_Data_DoubleHead_t * pDataDblHead = (HIF_Data_DoubleHead_t *)pDataListHead;
    hif_DATA_Append(pDataDblHead, pDataNode);


    return HIF_ERRCODE_SUCCESS;
}


__hif_sram_text uint8_t *HIF_MsgReport_ListPopData(HIF_Data_ListHead_t * pDataListHead)
{
    HIF_Data_Node_t *pDataNode;

    pDataNode = hif_DATA_PopHead(pDataListHead);

    if (pDataNode != NULL) {
        uint8_t *pData = hif_DLL_SendDataNodeDeinit(pDataNode);
        return pData;
    } else {
        return NULL;
    }
}


__hif_sram_text int HIF_MsgReport_ListStart(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, HIF_MsgTrans_Callback sentCb)
{
    if (hifCtrlState != HIF_STATE_INIT_SUCCESS) {
        return HIF_ERRCODE_NOT_READY;
    }

    if (hifCtrl.reportPrevent & HIF_REPORT_PREVENT) {
        return HIF_ERRCODE_NOT_READY;
    }

    if(pDataListHead == NULL) {
        HIF_LOG_DBG("START null");
        return HIF_ERRCODE_INVALID_PARAM;
    }


    HIF_MsgHdr_t msgHdr = {0};
    HIF_MSG_HDL_PACK(msgHdr, msgId, 0);

#if (CONFIG_HIF_SPLIT_ENA)
    uint32_t totalDataLen = hif_data_item_link_calDataLen(pDataListHead->head);
    if (totalDataLen > CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE) {
        if (hifCfg.splitEna) {
            if (totalDataLen > (hifCfg.splitSize + HIF_ML_FRAG_HDR_LEN)) {
                if (totalDataLen > hifCfg.splitTotalSize) {
                    return HIF_ERRCODE_INVALID_PARAM;
                }
                int retVal = HIF_ERRCODE_SUCCESS;
                retVal = hif_ml_frag_send(&msgHdr,
                                            pDataListHead,
                                            totalDataLen,
                                            sentCb,
                                            CONFIG_HIF_ML_MSG_TIMEOUT_MS_DEFAULT);
                return retVal;
            } else {
                HIF_LOG_DBG("(hifCfg.splitSize + HIF_ML_FRAG_HDR_LEN) Should <= CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE");
                return HIF_ERRCODE_FAIL;
            }
        } else {
            return HIF_ERRCODE_INVALID_PARAM;
        }
    }
#endif

    /* get payload length and checksume */
    uint16_t dataLen = 0;
    uint8_t chechsumEna = msgHdr.flag & HIF_MSG_FLAG_CHECK_BIT;
    HIF_Data_Node_t *pNode = pDataListHead->head;
    uint32_t checksum32 = 0;
    uint32_t offset = 0;
    while (pNode) {
        if (chechsumEna) {
            checksum32 += checksum32_calc(pNode->data, pNode->len, &offset);
        }

        dataLen += pNode->len;
        pNode = pNode->next;
    }

    if (dataLen > CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    msgHdr.length = dataLen;

    int status = HIF_ERRCODE_SUCCESS;
    HIF_Frame_Node_t *pFrameNode = NULL;
    HIF_Data_Node_t *pHeadNode = NULL;
    HIF_Data_Node_t *pTailNode = NULL;
    OSI_Semaphore_t *pSem = NULL;

    do {
        pFrameNode = hif_MEM_FrameNodeMalloc();
        if (pFrameNode == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        pHeadNode = hif_MEM_DataNodeMalloc();
        if (pHeadNode == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        if (chechsumEna) {
            pTailNode = hif_MEM_DataNodeMalloc();
            if (pTailNode == NULL) {
                status = HIF_ERRCODE_NO_BUFFER;
                break;
            }
        }

        if (sentCb == NULL) {
            pSem = hif_MEM_SemMalloc();
            if (pSem == NULL) {
                status = HIF_ERRCODE_NO_BUFFER;
                break;
            }
        }
    } while (0);

    if (status != HIF_ERRCODE_SUCCESS) {
        HIF_LOG_DBG("START no node");
        hif_MEM_FrameNodeFree(pFrameNode);
        hif_MEM_DataNodeFree(pHeadNode);
        hif_MEM_DataNodeFree(pTailNode);
        hif_MEM_SemFree(pSem);

        return status;
    }


    hif_MEM_FrameNodeInit(pFrameNode);
    pFrameNode->sendCb = (void *)sentCb;
    pFrameNode->sem = pSem;


    /* packet head */
    OSI_Memcpy(&pFrameNode->hdr[HIF_PHY_HEAD_LEN], &msgHdr, sizeof(HIF_MsgHdr_t));
    uint16_t hdrLen = HIF_HEAD_LEN + hif_DLL_GetTransDummy();
    hif_DLL_SendDataNodeInit(pFrameNode->hdr, hdrLen, pHeadNode, HIF_DATA_NODE_FLAG_HEAD);
    hif_DATA_Append(&pFrameNode->listHead, pHeadNode);
    pFrameNode->flag |= HIF_FRAME_FLAG_FRAME_HEAD;
    pFrameNode->frameLen = hdrLen;
    HIF_MsgHdr_t *pMsgHdr = (HIF_MsgHdr_t *)&pFrameNode->hdr[HIF_PHY_HEAD_LEN];
    pMsgHdr->length = dataLen;
    pMsgHdr->flag |= HIF_MSG_FLAG_MORE_DATA_BIT;


    /* packet payload */
    if (dataLen != 0) {
        pHeadNode->next = pDataListHead->head;
        pFrameNode->listHead.tail = pDataListHead->tail;

        pFrameNode->frameLen += dataLen;
    }


    /* packet checksume */
    if (chechsumEna != 0) {
        *(uint32_t *)pFrameNode->tail = checksum32;
        uint16_t tailLen = HIF_CHKEC_LEN + hif_DLL_GetTransDummy();

        hif_DLL_SendDataNodeInit(pFrameNode->tail, tailLen, pTailNode, HIF_DATA_NODE_FLAG_LAST);
        hif_DATA_Append(&pFrameNode->listHead, pTailNode);
        pFrameNode->flag |= HIF_FRAME_FLAG_FRAME_TAIL;
        pFrameNode->frameLen += HIF_CHKEC_LEN;
    } else {
        if (dataLen != 0) {
            pNode = pDataListHead->tail;
            hif_DLL_SendDataNodeInit(pNode->data, pNode->len + hif_DLL_GetTransDummy(), pNode, HIF_DATA_NODE_FLAG_LAST);
        }
    }


#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
    pFrameNode->endTick = OSI_GetTicks() + pdMS_TO_TICKS(CONFIG_HIF_TL_FRAME_TIMEOUT_MS_DEFAULT);
#endif

    hif_FRAME_Append(&hifCtrl.put, pFrameNode);
    hifCtrl.putNodeCnt ++;

#if CONFIG_HIF_TL_DEBUG_EN
    hif_tl_debug_info.frameCnt_from_app += 1;
#endif

    if(hifCtrl.putNodeCnt >= hifCfg.sendNodeLevel || pSem != NULL) {
        hif_EVENT_Set(HIF_EVENT_REPORT_REFRESH);
    }

    if (pSem != NULL) {
        OSI_SemaphoreWait(pSem, OSI_WAIT_FOREVER);
        status = pFrameNode->status;
        pDataListHead->tail->next = NULL;
        hif_MEM_SemFree(pSem);
        hif_MEM_FrameNodeFree(pFrameNode);
        hif_MEM_DataNodeFree(pHeadNode);
        hif_MEM_DataNodeFree(pTailNode);
    }

    return status;
}


__hif_sram_text int HIF_MsgReport_Flush(uint8_t flushEnable)
{
    if (hifCtrl.put.head != NULL) {
        if (flushEnable) {
            hif_RPT_Flush();
        }
        return 1;
    } else {
        return 0;
    }
}


void HIF_MsgReport_Lock(void)
{
    unsigned long key = __lock_irq();

    if ((hifCtrl.reportPrevent & HIF_REPORT_PREVENT) == 0) {
        hifCtrl.reportPrevent |= HIF_REPORT_PREVENT;

        hifCtrl.reportState = HIF_REPORT_STATE_IDLE;

        hifCtrl.event &= ~(HIF_EVENT_REPORT_POLL);
        hifCtrl.event &= ~(HIF_EVENT_REPORT_DONE);
        hifCtrl.event &= ~(HIF_EVENT_REPORT_POLL_NOACK);
        hifCtrl.event &= ~(HIF_EVENT_POLL_ACK_RETRY);

        hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;

        hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;

        hif_EVENT_Set(HIF_EVENT_REPORT_REFRESH);
    }

    __unlock_irq((unsigned long)key);
}


void HIF_MsgReport_Unlock(void)
{
    unsigned long key = __lock_irq();

    hifCtrl.reportPrevent &= ~HIF_REPORT_PREVENT;
    hif_EVENT_Set(HIF_EVENT_REPORT_REFRESH);

    __unlock_irq((unsigned long)key);
}


__hif_sram_text void hif_phy_io_set(void)
{
    if (hifCfg.notifyType == HIF_NOTIFY_TYPE_IO) {
        hif_IO_SetOutput(hifCfg.notifyIoNum, hifCfg.notifyIoLevel);
        if(hifCfg.comType == HIF_COM_TYPE_UART) {
            HAL_BOARD_BlkDelay(30, HAL_TIME_US);
        }
    }
}

__hif_isr_text void hif_phy_io_clr(void)
{
    if (hifCfg.notifyType == HIF_NOTIFY_TYPE_IO) {
        hif_IO_SetOutput(hifCfg.notifyIoNum, hifCfg.notifyIoLevel ^ 1);
    }
}


__hif_sram_text void hif_IO_Set(void)
{
    if (hifCfg.notifyType == HIF_NOTIFY_TYPE_IO || hifCfg.notifyType == HIF_NOTIFY_TYPE_DLY) {
        hif_IO_SetOutput(hifCfg.notifyIoNum, hifCfg.notifyIoLevel);
    }
}


__hif_sram_text void hif_IO_Clr(void)
{
    if (hifCfg.notifyType == HIF_NOTIFY_TYPE_IO || hifCfg.notifyType == HIF_NOTIFY_TYPE_DLY) {
        hif_IO_SetOutput(hifCfg.notifyIoNum, hifCfg.notifyIoLevel ^ 1);
    }
}


static int hif_RPT_NullFrameSend(void)
{
    if (hifCtrl.pEndFrame == NULL) {
        HIF_LOG_DBG("End Frame null");
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if (hifCtrl.pEndData == NULL) {
        HIF_LOG_DBG("End data null");
        return HIF_ERRCODE_INVALID_PARAM;
    }

    if (hif_DLL_EndFrameIsBusy()) {
        HIF_MsgHdr_t *msg = (HIF_MsgHdr_t *)&hifCtrl.pEndFrame->hdr[HIF_PHY_HEAD_LEN];
        msg->seq = (hifCtrl.txSeq) & 0x07;
        uint8_t check8 = HIF_CheckSum8(HIF_PHY_MSG_MAGIC, &hifCtrl.pEndFrame->hdr[HIF_PHY_HEAD_LEN], HIF_MSG_HEAD_LEN);
        hifCtrl.pEndFrame->hdr[1] = ~check8;
        return HIF_ERRCODE_SUCCESS;
    }

    hif_MEM_FrameNodeInit(hifCtrl.pEndFrame);

    OSI_Memcpy(hifCtrl.pEndFrame->hdr, hifNullPacket, HIF_HEAD_LEN);
    hifCtrl.pEndFrame->flag = HIF_FRAME_FLAG_END_FRAME | HIF_FRAME_FLAG_REFRESH_DATA;
    hifCtrl.pEndFrame->frameLen = HIF_HEAD_LEN;

    HIF_MsgHdr_t *msg = (HIF_MsgHdr_t *)&hifCtrl.pEndFrame->hdr[HIF_PHY_HEAD_LEN];
    msg->seq = (hifCtrl.txSeq) & 0x07;
    uint8_t check8 = HIF_CheckSum8(HIF_PHY_MSG_MAGIC, &hifCtrl.pEndFrame->hdr[HIF_PHY_HEAD_LEN], HIF_MSG_HEAD_LEN);
    hifCtrl.pEndFrame->hdr[1] = ~check8;

    hif_DLL_SendDataNodeInit(hifCtrl.pEndFrame->hdr,
        HIF_HEAD_LEN + hif_DLL_GetTransDummy(), hifCtrl.pEndData, HIF_DATA_NODE_FLAG_HEAD | HIF_DATA_NODE_FLAG_LAST);
    hifCtrl.pEndFrame->listHead.head = hifCtrl.pEndData;
    hifCtrl.pEndFrame->listHead.tail = hifCtrl.pEndData;


    return hif_DLL_Send(hifCtrl.pEndFrame);
}


__hif_sram_text static int hif_RPT_Send(void)
{
    int status = HIF_ERRCODE_NO_BUFFER;
    HIF_Frame_Node_t *pFrameNode = NULL;


    uint32_t burstNum = 1;

    if (hifCfg.burstEna) {
        burstNum = hifCtrl.burstPeriod;
    }

    if (burstNum == 0) {
        return HIF_ERRCODE_NOT_READY;
    }


    do {
#if (CONFIG_HIF_RETRY_ENA != 0)
        /* 1> High priority retransmission */
        if (hifCtrl.retry.head != NULL) {
            do {
                pFrameNode = hif_FRAME_PopHead(&hifCtrl.retry);

#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
                if (hif_tl_frame_isTimeout(pFrameNode)) {
                    pFrameNode->status = HIF_ERRCODE_TIMEOUT;
                    hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
                    continue;
                }
#endif

                if (burstNum == 1) {
                    pFrameNode->flag |= HIF_TRANS_FLAG_LAST_FRAME;
                    /* last frame */
                    if (hifCtrl.retry.head == NULL) {
                        /* clear more bit */
                        if (pFrameNode->hdr[2] & BIT(5)) {
                            pFrameNode->hdr[2] &= ~ BIT(5);
                            pFrameNode->hdr[1] += BIT(5);
                            (*(uint32_t *)&pFrameNode->tail[0]) += BIT(5);
                        }
                    }
                }

                HIF_Data_Node_t * pDataNode = NULL;
                pDataNode = hif_DATA_PeekHead(&pFrameNode->listHead);
                while (pDataNode) {
                    hif_PHY_SendDataNodeRefresh(pDataNode);
                    pDataNode = pDataNode->next;
                }

                status = hif_DLL_Send(pFrameNode);
                 if (status != HIF_ERRCODE_SUCCESS) {
                    hif_FRAME_Prepend(&hifCtrl.retry, pFrameNode);
                    burstNum = 0;
                    break;
                }
#if CONFIG_HIF_TL_DEBUG_EN
                else {
                    hif_tl_debug_info.frameCnt_to_dll += 1;
                }
#endif

                burstNum--;
            } while ((burstNum != 0) && (hifCtrl.retry.head != NULL));

            break;
        }
#endif

        /* 2> Low priority transmission of new data */
        if (hifCtrl.put.head != NULL) {
            do {
                pFrameNode = hif_FRAME_PopHead(&hifCtrl.put);
                hifCtrl.putNodeCnt --;
#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
                if (hif_tl_frame_isTimeout(pFrameNode)) {
                    pFrameNode->status = HIF_ERRCODE_TIMEOUT;
                    hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
                    continue;
                }
#endif

#if CONFIG_HIF_SPLIT_ENA
                HIF_MsgHdr_t *msgHdr = NULL;
                msgHdr = (HIF_MsgHdr_t *)(&(pFrameNode->hdr[HIF_PHY_HEAD_LEN]));
#endif

                if (burstNum == 1) {
                    pFrameNode->flag |= HIF_TRANS_FLAG_LAST_FRAME;
                    /* last frame */
                    if (hifCtrl.put.head == NULL) {
                        /* clear more bit */
                        if (pFrameNode->hdr[2] & BIT(5)) {
                            pFrameNode->hdr[2] &= ~ BIT(5);
#if CONFIG_HIF_SPLIT_ENA
                            if (msgHdr->frag) {
                                pFrameNode->hdr[1] += BIT(5);
                                (*(uint32_t *)&pFrameNode->tail[0]) += BIT(5);
                            }
#endif
                        }
                    }
                }

#if CONFIG_HIF_SPLIT_ENA
                if (msgHdr->frag) {
                    hif_tl_frame_update_seq_checksum(pFrameNode);
                } else {
                    hif_MSG_UpdateChecksum(pFrameNode);
                }
#else
                hif_MSG_UpdateChecksum(pFrameNode);
#endif

                status = hif_DLL_Send(pFrameNode);
                if (status != HIF_ERRCODE_SUCCESS) {
                    hif_FRAME_Prepend(&hifCtrl.put, pFrameNode);
                    hifCtrl.putNodeCnt ++;
                    burstNum = 0;
                    HIF_LOG_DBG("TX FAIL: %d", status);
                    break;
                }
#if CONFIG_HIF_TL_DEBUG_EN
                else {
                    hif_tl_debug_info.frameCnt_to_dll += 1;
                }
#endif

                 hifCtrl.txSeq++;
                burstNum--;
            } while ((burstNum != 0) && (hifCtrl.put.head != NULL));

            break;
        }

    }while (0);



    if ((burstNum) && (hifCfg.burstEna)) {
        status = hif_RPT_NullFrameSend();
    } else if((burstNum == 0) && (hifCfg.burstEna)){
        if(hif_DLL_EndFrameIsBusy()) {
            hif_DLL_EndFrameRemove();
        }
    }


    if (hifCfg.burstEna) {
        hifCtrl.burstPeriod = burstNum;
    }


    return status;
}


__hif_isr_text static void hif_RPT_FreeFrameQueue(HIF_Frame_DoubleHead_t *pFrameList, uint8_t status)
{
    HIF_MsgTrans_Callback sentCb = NULL;
    HIF_Frame_Node_t *pFrameNode = NULL;

    do {
        pFrameNode = hif_FRAME_PopHead(pFrameList);
        if (pFrameNode == NULL) {
            break;
        }

#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
        if (pFrameList == (&hifCtrl.freePending)) {
            ; // status is already set in pFrameNode->status
        } else {
            pFrameNode->status = status;
        }
#else
        pFrameNode->status = status;
#endif

#if CONFIG_HIF_SPLIT_ENA
        HIF_MsgHdr_t *msgHdr = (HIF_MsgHdr_t *)(&(pFrameNode->hdr[HIF_PHY_HEAD_LEN]));
        if (msgHdr->frag) {
            ((hif_ml_sendCallback_t)(pFrameNode->sendCb))(pFrameNode);               // Message-layer callback
#if CONFIG_HIF_TL_DEBUG_EN
            hif_tl_debug_info.frameCnt_to_ml += 1;
#endif
        } else {
            sentCb = pFrameNode->sendCb;
            if (sentCb) {
                HIF_Data_Node_t *pDataNode = NULL;
                if (pFrameNode->flag & HIF_FRAME_FLAG_FRAME_HEAD) {
                    pDataNode = hif_DATA_PopHead(&pFrameNode->listHead);
                    hif_MEM_DataNodeFree(pDataNode);
                    pFrameNode->flag &= ~HIF_FRAME_FLAG_FRAME_HEAD;
                }

                if (pFrameNode->flag & HIF_FRAME_FLAG_FRAME_TAIL) {
                    pDataNode = hif_DATA_PopTail(&pFrameNode->listHead);
                    hif_MEM_DataNodeFree(pDataNode);
                    pFrameNode->flag &= ~HIF_FRAME_FLAG_FRAME_TAIL;
                }
                sentCb(((HIF_MsgHdr_t *)(&(pFrameNode->hdr[HIF_PHY_HEAD_LEN])))->msg_id,
                    &pFrameNode->listHead,
                    pFrameNode->status);

                hif_MEM_DataListFree(&pFrameNode->listHead);
                hif_MEM_FrameNodeFree(pFrameNode);
#if CONFIG_HIF_TL_DEBUG_EN
                hif_tl_debug_info.frameCnt_to_app += 1;
#endif
            } else {
                if (pFrameNode->sem != NULL) {
                    OSI_SemaphoreRelease(pFrameNode->sem);
#if CONFIG_HIF_TL_DEBUG_EN
                    hif_tl_debug_info.frameCnt_to_app += 1;
#endif
                }
            }
        }
#else
        sentCb = pFrameNode->sendCb;
        if (sentCb) {
            HIF_Data_Node_t *pDataNode = NULL;
            if (pFrameNode->flag & HIF_FRAME_FLAG_FRAME_HEAD) {
                pDataNode = hif_DATA_PopHead(&pFrameNode->listHead);
                hif_MEM_DataNodeFree(pDataNode);
                pFrameNode->flag &= ~HIF_FRAME_FLAG_FRAME_HEAD;
            }

            if (pFrameNode->flag & HIF_FRAME_FLAG_FRAME_TAIL) {
                pDataNode = hif_DATA_PopTail(&pFrameNode->listHead);
                hif_MEM_DataNodeFree(pDataNode);
                pFrameNode->flag &= ~HIF_FRAME_FLAG_FRAME_TAIL;
            }
            sentCb(((HIF_MsgHdr_t *)(&(pFrameNode->hdr[HIF_PHY_HEAD_LEN])))->msg_id,
                &pFrameNode->listHead,
                pFrameNode->status);

            hif_MEM_DataListFree(&pFrameNode->listHead);
            hif_MEM_FrameNodeFree(pFrameNode);
#if CONFIG_HIF_TL_DEBUG_EN
            hif_tl_debug_info.frameCnt_to_app += 1;
#endif
        } else {
            if (pFrameNode->sem != NULL) {
                OSI_SemaphoreRelease(pFrameNode->sem);
#if CONFIG_HIF_TL_DEBUG_EN
                hif_tl_debug_info.frameCnt_to_app += 1;
#endif
            }
        }
#endif
    } while (1);
}


__hif_isr_text static void hif_RPT_Flush(void)
{
    hif_RPT_FreeFrameQueue(&hifCtrl.put, HIF_ERRCODE_TIMEOUT);
    hifCtrl.putNodeCnt = 0;
#if (CONFIG_HIF_RETRY_ENA != 0)
    hif_RPT_FreeFrameQueue(&hifCtrl.ackPending, HIF_ERRCODE_TIMEOUT);
    hif_RPT_FreeFrameQueue(&hifCtrl.retry, HIF_ERRCODE_TIMEOUT);
#endif
}


static void hif_RPT_Recover(void)
{
    hif_RPT_FreeFrameQueue(&hifCtrl.freePending, HIF_ERRCODE_SUCCESS);
}


void HIF_PM_WakeupDevice(void)
{
    hifCtrl.pmState |= HIF_PM_STATE_APP_PREVENT;
    OSI_SemaphoreRelease(&hifCtrl.semProc);
}


void HIF_PM_SuspendDevice(void)
{
    hifCtrl.pmState &= ~HIF_PM_STATE_APP_PREVENT;

    hif_EVENT_Set(HIF_EVENT_SLEEP);
}


void hif_HST_ChangeState(uint8_t ready)
{

}


__hif_isr_text void hif_EVENT_Set(uint32_t event)
{
    unsigned long key = __lock_irq();

    hifCtrl.event |= event;

    __unlock_irq((unsigned long)key);

    OSI_SemaphoreRelease(&hifCtrl.semProc);
}

__hif_sram_text static inline void hif_EVENT_Clr(uint32_t event)
{
    unsigned long key = __lock_irq();

    hifCtrl.event &= ~event;

    __unlock_irq((unsigned long)key);
}

static inline int hif_EVENT_IsSet(uint32_t event)
{
    int ret = 0;

    unsigned long key = __lock_irq();

    ret = hifCtrl.event & event;

    __unlock_irq((unsigned long)key);

    return ret;
}


bool hif_OperateLock(void)
{
    if (hifCtrlState != HIF_STATE_INIT_SUCCESS) {
        return false;
    }

    OSI_SemaphoreWait(&hifCtrl.semOper, OSI_WAIT_FOREVER);

    return true;
}


void hif_OperateUnlock(void)
{
    OSI_SemaphoreRelease(&hifCtrl.semOper);
}


static void hif_MSG_UpdateChecksum(HIF_Frame_Node_t *pFrameNode)
{
    if ((pFrameNode->flag & HIF_FRAME_FLAG_CHECKSUME) == 0) {
        HIF_MsgHdr_t *msg = (HIF_MsgHdr_t *)&pFrameNode->hdr[HIF_PHY_HEAD_LEN];
        msg->seq = (hifCtrl.txSeq) & 0x07;

        pFrameNode->hdr[0] = HIF_PHY_MSG_MAGIC;

        /* update head checksum */
        uint8_t check8 = HIF_CheckSum8(HIF_PHY_MSG_MAGIC, &pFrameNode->hdr[HIF_PHY_HEAD_LEN], HIF_MSG_HEAD_LEN);
        pFrameNode->hdr[1] = ~check8;

        /* update frame checksum */
        if (msg->flag & HIF_MSG_FLAG_CHECK_BIT) {
            uint32_t * checksum32 = (uint32_t *)pFrameNode->tail;
            *checksum32 += *(uint32_t *)msg;
            *checksum32 = ~(*checksum32);
        }

        pFrameNode->flag |= HIF_FRAME_FLAG_CHECKSUME;
    }
}

bool HIF_MSG_IsSendDone(void)
{
#if (CONFIG_HIF_RETRY_ENA != 0)
    if(hif_FRAME_IsEmpty(&hifCtrl.put)
       && hif_FRAME_IsEmpty(&hifCtrl.ackPending)
       && hif_FRAME_IsEmpty(&hifCtrl.retry)
       && hif_FRAME_IsEmpty(&hifCtrl.freePending)) {
        return true;
    }
#else
    if(hif_FRAME_IsEmpty(&hifCtrl.put)
       && hif_FRAME_IsEmpty(&hifCtrl.freePending)) {
        return true;
    }
#endif

    return false;
}


/**
 * @brief Start receiving msg
 * @return __hif_isr_text 
 */
__hif_isr_text static int hif_MSG_Recv_Start(void)
{
    if (hifCtrl.msgRecvStartLock) {
        return HIF_ERRCODE_NOT_READY;   // Can not start receiving msg
    }

    hifCtrl.msgRecvStartLock = 1;       // Avoid restart

    hifCtrl.pCmdRspData->data = hifCtrl.pCmdRspBuff;
    hifCtrl.pCmdRspData->len = hifCtrl.CmdRspSize;

    hifCtrl.pCmdRspFrame->status = HIF_ERRCODE_SUCCESS;

    hifCtrl.pCmdRspFrame->listHead.head = NULL;
    hifCtrl.pCmdRspFrame->listHead.tail = NULL;
    hif_DATA_Append(&hifCtrl.pCmdRspFrame->listHead, hifCtrl.pCmdRspData);

    return hif_DLL_Recv(hifCtrl.pCmdRspFrame);
}

/**
 * @brief Unlock hif_MSG_Recv_Start
 * @return __hif_isr_text
 */
__hif_isr_text static void hif_MSG_Recv_Start_Unlock(void)
{
    hifCtrl.msgRecvStartLock = 0;
}

/**
 * @brief Response wakeup signal(55 FF 55 FF)
 * @return __hif_isr_text 0 - HIF_ERRCODE_SUCCESS; Others - Response failed
 */
__hif_isr_text static int hif_MSG_WakeupResp(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
        hif_MEM_FrameNodeInit(hifCtrl.pCmdRspFrame);
        hifCtrl.pCmdRspFrame->flag = HIF_TRANS_FLAG_LAST_FRAME | HIF_FRAME_FLAG_CMD_FRAME | HIF_FRAME_FLAG_REFRESH_DATA;

        hifCtrl.pCmdRspFrame->hdr[0] = HIF_PHY_ACK_MAGIC;
        hifCtrl.pCmdRspFrame->hdr[1] = HIF_PHY_ACK_MAGIC;
        hifCtrl.pCmdRspFrame->hdr[2] = HIF_PHY_ACK_MAGIC;
        hifCtrl.pCmdRspFrame->hdr[3] = HIF_PHY_ACK_MAGIC;

        uint16_t hdrLen = 4 + hif_DLL_GetTransDummy();

        hif_DLL_SendDataNodeInit(hifCtrl.pCmdRspFrame->hdr, hdrLen, hifCtrl.pCmdRspHead,
                                HIF_DATA_NODE_FLAG_HEAD | HIF_DATA_NODE_FLAG_LAST);

        hifCtrl.pCmdRspFrame->frameLen = 4;

        hif_DATA_Append(&hifCtrl.pCmdRspFrame->listHead, hifCtrl.pCmdRspHead);

        status = hif_DLL_Send(hifCtrl.pCmdRspFrame);
    } else {
        hifCtrl.pCmdRspData->data = hifCtrl.pCmdRspBuff;
        hifCtrl.pCmdRspData->len = 4;

        hifCtrl.pCmdRspBuff[0] = HIF_PHY_ACK_MAGIC;
        hifCtrl.pCmdRspBuff[1] = HIF_PHY_ACK_MAGIC;
        hifCtrl.pCmdRspBuff[2] = HIF_PHY_ACK_MAGIC;
        hifCtrl.pCmdRspBuff[3] = HIF_PHY_ACK_MAGIC;

        hif_DLL_SendDataNodeInit(hifCtrl.pCmdRspBuff,
            4, hifCtrl.pCmdRspData, HIF_DATA_NODE_FLAG_HEAD | HIF_DATA_NODE_FLAG_LAST);

        hif_DLL_SendWakeupResp(hifCtrl.pCmdRspData);
    }

    return status;
}


static void hif_MSG_SendStartupCb(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
{
    /* free data node */
    HIF_MsgReport_ListPopData(pDataListHead);
}


static void hif_MSG_SendStartup(void)
{
    if (hifCfg.reportInitStatus) {
        HIF_MsgReport(HIF_MSG_ID_STARTUP, (void *)&hifCtrl.startupState, 1, hif_MSG_SendStartupCb);
    }
}


static int hif_SpecFrame_Init(uint32_t buffSize)
{
    int status = HIF_ERRCODE_SUCCESS;

    if (buffSize < CONFIG_HIF_MEM_CMD_RSP_SIZE) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    hifCtrl.pCmdRspFrame    = NULL;
    hifCtrl.pCmdRspHead     = NULL;
    hifCtrl.pCmdRspData     = NULL;
    hifCtrl.pCmdRspTail     = NULL;
    hifCtrl.pCmdRspFreeBuff = NULL;
    hifCtrl.pCmdRspBuff     = NULL;
    hifCtrl.CmdRspSize      = buffSize;

    hifCtrl.pEndFrame       = NULL;
    hifCtrl.pEndData        = NULL;

    do {
        hifCtrl.pEndFrame = hif_MEM_FrameNodeMalloc();
        if (hifCtrl.pEndFrame == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        hifCtrl.pEndData = hif_MEM_DataNodeMalloc();
        if (hifCtrl.pEndData == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        /* malloc frame node and data node for CMD RSP frame */
        hifCtrl.pCmdRspFrame = hif_MEM_FrameNodeMalloc();
        if (hifCtrl.pCmdRspFrame == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        hifCtrl.pCmdRspHead = hif_MEM_DataNodeMalloc();
        if (hifCtrl.pCmdRspHead == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        hifCtrl.pCmdRspData = hif_MEM_DataNodeMalloc();
        if (hifCtrl.pCmdRspData == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        hifCtrl.pCmdRspTail = hif_MEM_DataNodeMalloc();
        if (hifCtrl.pCmdRspTail == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        hifCtrl.pCmdRspFreeBuff = HIF_MemMalloc(hifCtrl.CmdRspSize + HIF_MEM_ALIGN_NUM);
        if (hifCtrl.pCmdRspFreeBuff == NULL) {
            status = HIF_ERRCODE_NO_BUFFER;
            break;
        }

        uint32_t buffAddr = (uint32_t)hifCtrl.pCmdRspFreeBuff;
        buffAddr = HIF_MEM_ALIGN(buffAddr);

        csi_dcache_clean_range((unsigned long *)buffAddr, hifCtrl.CmdRspSize);

        hifCtrl.pCmdRspBuff = (uint8_t *)LL_DeCodeAddr((uint32_t)buffAddr);
        HIF_LOG_DBG("hifCtrl.pCmdRspBuff %p", hifCtrl.pCmdRspBuff);
    } while (0);

    if (status != HIF_ERRCODE_SUCCESS) {
        //hif_SpecFrame_Deinit();
        HIF_LOG_DBG("Spec frame init fail");
        return status;
    } else {
        hif_MEM_FrameNodeInit(hifCtrl.pEndFrame);

        OSI_Memcpy(hifCtrl.pEndFrame->hdr, hifNullPacket, HIF_HEAD_LEN);
        hifCtrl.pEndFrame->flag = HIF_FRAME_FLAG_END_FRAME | HIF_FRAME_FLAG_REFRESH_DATA;
        hifCtrl.pEndFrame->frameLen = HIF_HEAD_LEN;

        return HIF_ERRCODE_SUCCESS;
    }
}


static void hif_SpecFrame_Deinit(void)
{
    if (hifCtrl.pEndFrame != NULL) {
        hif_MEM_FrameNodeFree(hifCtrl.pEndFrame);
        hifCtrl.pEndFrame = NULL;
    }

    if (hifCtrl.pEndData != NULL) {
        hif_MEM_DataNodeFree(hifCtrl.pEndData);
        hifCtrl.pEndData = NULL;
    }

    if (hifCtrl.pCmdRspFrame != NULL) {
        hif_MEM_FrameNodeFree(hifCtrl.pCmdRspFrame);
        hifCtrl.pCmdRspFrame = NULL;
    }

    if (hifCtrl.pCmdRspHead != NULL) {
        hif_MEM_DataNodeFree(hifCtrl.pCmdRspHead);
        hifCtrl.pCmdRspHead = NULL;
    }

    if (hifCtrl.pCmdRspData != NULL) {
        hif_MEM_DataNodeFree(hifCtrl.pCmdRspData);
        hifCtrl.pCmdRspData = NULL;
    }

    if (hifCtrl.pCmdRspTail != NULL) {
        hif_MEM_DataNodeFree(hifCtrl.pCmdRspTail);
        hifCtrl.pCmdRspTail = NULL;
    }

    if (hifCtrl.pCmdRspFreeBuff != NULL) {
        HIF_MemFree(hifCtrl.pCmdRspFreeBuff);
        hifCtrl.pCmdRspFreeBuff = NULL;
        hifCtrl.pCmdRspBuff = NULL;
    }
}

__hif_isr_text static int hif_TL_SendCallback(HIF_Frame_Node_t * pFrameNode, uint32_t flag)
{

    if (flag == HIF_DLL_FLAG_SEND_FRAME) {

        if (pFrameNode->flag & HIF_TRANS_FLAG_LAST_FRAME) {
            hifCtrl.burstPeriod = 0;
        }

#if (CONFIG_HIF_RETRY_ENA != 0)
        if (hifCfg.retryEna) {
#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
            if (hif_tl_frame_isTimeout(pFrameNode)) {
                pFrameNode->status = HIF_ERRCODE_TIMEOUT;
                hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
            } else {
                hif_FRAME_Append(&hifCtrl.ackPending, pFrameNode);
            }
#else

            hif_FRAME_Append(&hifCtrl.ackPending, pFrameNode);
#endif

#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
            if (hifCtrl.ackPending.head != NULL) {
                if (OSI_STATUS_OK != hif_tl_ack_timer_start(CONFIG_HIF_TL_ACK_TIMEOUT_MS)) {
                    HIF_LOG_ERR("Start tl ack timer fail");
                }
            }
#endif
        } else
#endif /* CONFIG_HIF_RETRY_ENA != 0 */
        {
            hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
        }

        hif_EVENT_Set(HIF_EVENT_REPORT_REFRESH);

#if CONFIG_HIF_TL_DEBUG_EN
        hif_tl_debug_info.frameCnt_from_dll += 1;
#endif
    } else if (flag == HIF_DLL_FLAG_SEND_RSP_DONE) {
        hif_MSG_Recv_Start_Unlock();
        hif_MSG_Recv_Start();

        hif_EVENT_Set(HIF_EVENT_RSP_SEND_DONE);
    } else if (flag == HIF_DLL_FLAG_SEND_RPT_DONE) {
        hif_MSG_Recv_Start();   /*  */

        hif_EVENT_Set(HIF_EVENT_REPORT_DONE);
    } else if (flag == HIF_DLL_FLAG_SEND_WAKE) {    /* */
        hif_MSG_Recv_Start_Unlock();
        hif_MSG_Recv_Start();

        hif_EVENT_Set(HIF_EVENT_RSP_SEND_DONE);
    } else if (flag == HIF_DLL_FLAG_SEND_FLUSH) {
        hif_MSG_Recv_Start();   /*  */

        if (pFrameNode->flag & HIF_TRANS_FLAG_LAST_FRAME) {
            hifCtrl.burstPeriod = 0;
        }
        hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
        hif_EVENT_Set(HIF_EVENT_REPORT_REFRESH);
#if CONFIG_HIF_TL_DEBUG_EN
        hif_tl_debug_info.frameCnt_from_dll += 1;
#endif
    }


    return 0;
}


__hif_isr_text static int hif_TL_RecvCallback(HIF_Frame_Node_t * pFrameNode, uint32_t flag)
{
    if ((flag == HIF_DLL_FLAG_RECV_FRAME) || (flag == HIF_DLL_FLAG_RECV_FAIL)) {

        if (pFrameNode == NULL) {
            return HIF_ERRCODE_INVALID_PARAM;
        }


        hif_EVENT_Set(HIF_EVENT_CMD_RECV_DONE);
    } else if (flag == HIF_DLL_FLAG_RECV_WAKE) {
        /* receive wakeup cmd, response wakeup rsp */
        hifCtrl.reportPrevent |= HIF_REPORT_PAUSE;
        if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
            hifCtrl.reportPrevent |= HIF_REPORT_PREVENT;
        }
        hif_EVENT_Set(HIF_EVENT_WAKEUP | HIF_EVENT_CMD_RECV_DONE);
    }

    return HIF_ERRCODE_SUCCESS;
}


__hif_sram_text static OSI_Time_t hif_TASK_CmdRsp(OSI_Time_t currentTime, OSI_Time_t *pCmdRspTime)
{
    int status = HIF_ERRCODE_SUCCESS;
    OSI_Time_t cmdTimeout = OSI_WAIT_FOREVER;

    if (hifCtrl.event & HIF_EVENT_CMD_RECV_DONE) {
        hif_EVENT_Clr(HIF_EVENT_CMD_RECV_DONE);

        /* If it is awake-up command */
        if (hifCtrl.event & HIF_EVENT_WAKEUP) {
            hif_EVENT_Clr(HIF_EVENT_WAKEUP);

            if (hif_DLL_SendFlush() == HIF_ERRCODE_SUCCESS) {
                hif_RPT_Flush();

                /* Start sending timeot */
                hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                hifCtrl.timerState |= HIF_TIMER_CMDRSP_SEND;
                *pCmdRspTime = currentTime;
                cmdTimeout = hifCfg.sendFrameTo;

                /* Lock sleep mode until sleep timeout is completed */
                hifCtrl.pmState |= HIF_PM_STATE_CMDRSP_PREVENT;
                hifCtrl.pmState &= ~HIF_PM_STATE_WAKE_REQ;

                status = hif_MSG_WakeupResp();

                if (status != HIF_ERRCODE_SUCCESS) {
                    /* No sending timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    // *pCmdRspTime = currentTime;
                    cmdTimeout = 0;

                    /* Unlock sleep mode */
                    hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT;

                    hif_MSG_Recv_Start_Unlock();
                    hif_MSG_Recv_Start();

                    hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                    if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
                        hifCtrl.reportPrevent &= ~HIF_REPORT_PREVENT;
                    }
                }
            } else {
                hif_MSG_Recv_Start_Unlock();
                hif_MSG_Recv_Start();

                hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PREVENT;
                }
            }

        /* If it is a general command */
        } else {

            do {
                if (hifCtrl.pCmdRspFrame == NULL) {
                    hifCtrl.fault = 1;
                    break;
                }

                HIF_Data_Node_t * pDataNode = hifCtrl.pCmdRspFrame->listHead.head;
                if (pDataNode == NULL) {
                    hifCtrl.fault = 2;
                    break;
                }

                HIF_MsgHdr_t *pCmdMsg = (HIF_MsgHdr_t *)(pDataNode->data + sizeof(HIF_PhyHdr_t));
                if (pCmdMsg == NULL) {
                    hifCtrl.fault = 3;
                    break;
                }
                hifCtrl.reportPrevent |= HIF_REPORT_PAUSE;
                uint8_t recvStatus = hifCtrl.pCmdRspFrame->status;
                if (recvStatus == HIF_ERRCODE_SUCCESS) {
                    status = HIF_MsgHdl_Process(pCmdMsg);
                    /* update time */
                    currentTime = hif_TIME_Get();
                } else {
                    status = HIF_MsgResp(pCmdMsg, 0, recvStatus);
                }

                if (hifCtrl.event & HIF_EVENT_SLEEP) {
                    hif_EVENT_Clr(HIF_EVENT_SLEEP);
                    hifCtrl.pmState |= HIF_PM_STATE_SLEEP_REQ;
                }

                if (status != HIF_ERRCODE_SUCCESS) {
                    hifCtrl.fault = 4;

                    hif_MSG_Recv_Start_Unlock();
                    hif_MSG_Recv_Start();

                    /* Start sleep timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    *pCmdRspTime = currentTime;
                    /* unlock the report */
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                    /* Lock sleep mode until sleep timeout is completed */
                    //hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT; /* ? */
                    if(hifCtrl.pmState & HIF_PM_STATE_CMDRSP_PREVENT) {
                        cmdTimeout = hifCfg.sleepTo;
                    }
                    hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT;

                    break;
                }

                /* The poll command does not response */
                if (hifCtrl.event & HIF_EVENT_RSP_SKIP) {
                    hif_MSG_Recv_Start_Unlock();

                    hif_EVENT_Clr(HIF_EVENT_RSP_SKIP);

                    /* Start sleep timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    *pCmdRspTime = currentTime;
                    /* unlock the report */
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                    /* Lock sleep mode until sleep timeout is completed */
                    //hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT; /* ? */
                    if(hifCtrl.pmState & HIF_PM_STATE_CMDRSP_PREVENT) {
                        cmdTimeout = hifCfg.sleepTo;
                    }
                } else if (hifCtrl.event & HIF_EVENT_REPORT_POLL_NOACK) {//recv poll ack to recv start have dead time, = schedule time
                    hif_EVENT_Clr(HIF_EVENT_REPORT_POLL_NOACK);

                    hif_MSG_Recv_Start_Unlock();
                    hif_MSG_Recv_Start();

                    /* Start sleep timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    *pCmdRspTime = currentTime;
                    /* unlock the report */
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                    /* Lock sleep mode until sleep timeout is completed */
                    //hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT; /* ? */
                    if(hifCtrl.pmState & HIF_PM_STATE_CMDRSP_PREVENT) {
                        cmdTimeout = hifCfg.sleepTo;
                    }
                    hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT;
                } else if (hifCtrl.event & HIF_EVENT_NO_RSP) {
                    hif_EVENT_Clr(HIF_EVENT_NO_RSP);

                    hif_MSG_Recv_Start_Unlock();
                    hif_MSG_Recv_Start();

                    /* Start sleep timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    *pCmdRspTime = currentTime;
                    /* unlock the report */
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                    /* Lock sleep mode until sleep timeout is completed */
                    if(hifCtrl.pmState & HIF_PM_STATE_CMDRSP_PREVENT) {
                        cmdTimeout = hifCfg.sleepTo;
                    }
                    hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT;
                }else {

                    /* Start sending timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    hifCtrl.timerState |= HIF_TIMER_CMDRSP_SEND;
                    *pCmdRspTime = currentTime;
                    cmdTimeout = hifCfg.sendFrameTo;
                    /* Lock the report until reply is sent */
                    hifCtrl.reportPrevent |= HIF_REPORT_PAUSE;
                    /* Lock sleep mode until sleep timeout is completed */
                    hifCtrl.pmState |= HIF_PM_STATE_CMDRSP_PREVENT;
                }
            } while (0);
        }

    } else if (hifCtrl.event & HIF_EVENT_RSP_SEND_DONE) {
        //hifCtrl.event &= ~HIF_EVENT_RSP_SEND_DONE;
        hif_EVENT_Clr(HIF_EVENT_RSP_SEND_DONE);

        /* stop response send */

        if (hifCtrl.pmState & HIF_PM_STATE_SLEEP_REQ) {
            /* stop sleep timeot */
            hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
            *pCmdRspTime = currentTime;
            cmdTimeout = OSI_WAIT_FOREVER;
            /* unlock the report */
            hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
            /* Unlock sleep mode */
            hifCtrl.pmState &= ~(HIF_PM_STATE_CMDRSP_PREVENT | HIF_PM_STATE_SLEEP_REQ);

            if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
                hifCtrl.reportPrevent &= ~HIF_REPORT_PREVENT;
            }
        } else {
            /* Start sleep timeot */
            hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
            hifCtrl.timerState |= HIF_TIMER_CMDRSP_SLEEP;
            *pCmdRspTime = currentTime;
            cmdTimeout = hifCfg.sleepTo;
            /* unlock the report */
            hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
            /* Lock sleep mode until sleep timeout is completed */
            hifCtrl.pmState |= HIF_PM_STATE_CMDRSP_PREVENT;
            if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
                hifCtrl.reportPrevent &= ~HIF_REPORT_PREVENT;
            }
        }
    } else {


        if (hifCtrl.event & HIF_EVENT_WAKEUP) {
            //hifCtrl.event &= ~HIF_EVENT_WAKEUP;
            hif_EVENT_Clr(HIF_EVENT_WAKEUP);

            /* Start sending timeot */
            hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
            hifCtrl.timerState |= HIF_TIMER_CMDRSP_SEND;
            *pCmdRspTime = currentTime;
            cmdTimeout = hifCfg.sendFrameTo;
            /* Lock the report until reply is sent */
            hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
            /* Lock sleep mode until sleep timeout is completed */
            hifCtrl.pmState |= HIF_PM_STATE_WAKE_REQ;

        } else if (hifCtrl.timerState & HIF_TIMER_CMDRSP_SEND) {
            cmdTimeout = currentTime - *pCmdRspTime;

            if (cmdTimeout >= hifCfg.sendFrameTo) {
                /* stop response send */
                hif_IO_Clr();

                hif_MSG_Recv_Start_Unlock();
                hif_MSG_Recv_Start();

                if (hifCtrl.pmState & (HIF_PM_STATE_SLEEP_REQ | HIF_PM_STATE_WAKE_REQ)) {
                    /* stop sleep timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    *pCmdRspTime = currentTime;
                    cmdTimeout = OSI_WAIT_FOREVER;
                    /* unlock the report */
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                    /* Unlock sleep mode */
                    hifCtrl.pmState &= ~(HIF_PM_STATE_CMDRSP_PREVENT | HIF_PM_STATE_SLEEP_REQ | HIF_PM_STATE_WAKE_REQ);

                } else {
                    /* Start sleep timeot */
                    hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                    hifCtrl.timerState |= HIF_TIMER_CMDRSP_SLEEP;
                    *pCmdRspTime = currentTime;
                    cmdTimeout = hifCfg.sleepTo;
                    /* unlock the report */
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                    /* Lock sleep mode until sleep timeout is completed */
                    hifCtrl.pmState |= HIF_PM_STATE_CMDRSP_PREVENT;
                }
            } else {
                cmdTimeout = hifCfg.sendFrameTo - cmdTimeout;
            }
        } else if (hifCtrl.timerState & HIF_TIMER_CMDRSP_SLEEP) {
            cmdTimeout = currentTime - *pCmdRspTime;

            if (cmdTimeout >= hifCfg.sleepTo) {
                /* stop response send */

                /* stop sleep timeot */
                hifCtrl.timerState &= HIF_TIMER_CMDRSP_MSK;
                *pCmdRspTime = currentTime;
                cmdTimeout = OSI_WAIT_FOREVER;
                /* unlock the report */
                hifCtrl.reportPrevent &= ~HIF_REPORT_PAUSE;
                /* Unlock sleep mode */
                hifCtrl.pmState &= ~HIF_PM_STATE_CMDRSP_PREVENT;

                if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
                    hifCtrl.reportPrevent &= ~HIF_REPORT_PREVENT;
                }

            } else {
                cmdTimeout = hifCfg.sleepTo - cmdTimeout;
            }
        }
    }


    return cmdTimeout;
}


__hif_sram_text static OSI_Time_t hif_TASK_Report(OSI_Time_t currentTime, OSI_Time_t *pReportTime)
{
    int status = HIF_ERRCODE_SUCCESS;
    OSI_Time_t reportTimeout = OSI_WAIT_FOREVER;

    if (hifCfg.reportMode == HIF_REPORT_MODE_DISABLE) {
        hif_RPT_Flush();
        return reportTimeout;
    }


    if (hifCtrl.event & HIF_EVENT_REPORT_REFRESH) {
        //hifCtrl.event &= ~HIF_EVENT_REPORT_REFRESH;
        hif_EVENT_Clr(HIF_EVENT_REPORT_REFRESH);
    }

    if (hifCtrl.reportPrevent & HIF_REPORT_PREVENT) {
        if (!hif_FRAME_IsEmpty(&hifCtrl.put)) {
            hif_RPT_Flush();
            hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
            hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
            hifCtrl.reportState = HIF_REPORT_STATE_IDLE;

            if ((hifCtrl.reportPrevent & HIF_REPORT_PAUSE) == 0) {
                hif_IO_Clr();
            }
        }

        return reportTimeout;

    } else if (hifCtrl.reportPrevent & HIF_REPORT_PAUSE) {
        return reportTimeout;

    }


    switch (hifCtrl.reportState) {
    case HIF_REPORT_STATE_IDLE:
    {
        if (hifCtrl.event & HIF_EVENT_POLL_ACK_RETRY) {
            hif_EVENT_Clr(HIF_EVENT_POLL_ACK_RETRY);
            hifCtrl.reportState = HIF_REPORT_STATE_SEND;
            hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
            *pReportTime = currentTime;
            reportTimeout = 0; /* quickly enter next loop */
            hifCtrl.pmState |= HIF_PM_STATE_REPORT_PREVENT;
        } else if (hifCtrl.event & HIF_EVENT_REPORT_POLL) {
            /* receive a pool frame, response null frame */
            hif_EVENT_Clr(HIF_EVENT_REPORT_POLL);
            status = hif_RPT_NullFrameSend();
            if (status == HIF_ERRCODE_SUCCESS) {
                /* Start sending timeot */
                hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                hifCtrl.timerState |= HIF_TIMER_REPORT_SEND;
                *pReportTime = currentTime;
                reportTimeout = hifCfg.sendFrameTo;

                hifCtrl.pmState |= HIF_PM_STATE_REPORT_PREVENT;
            } else {
                /* send fail, transsfer error */
                hifCtrl.fault = 2;
                reportTimeout = 0; /* quickly enter next loop */
            }
        } else {
            /* process NULL frame response */
            if (hifCtrl.timerState & HIF_TIMER_REPORT_SEND) {
                /* send done */
                if (hifCtrl.event & HIF_EVENT_REPORT_DONE) {
                    //hifCtrl.event &= ~HIF_EVENT_REPORT_DONE;
                    hif_EVENT_Clr(HIF_EVENT_REPORT_DONE);
                    hif_IO_Clr();
                    hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                    *pReportTime = currentTime;
                    reportTimeout = 0; /* quickly enter next loop */
                    hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                } else {
                    reportTimeout = currentTime - *pReportTime;
                    if (reportTimeout >= hifCfg.sendFrameTo) {
                        hif_IO_Clr();
                        hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                        *pReportTime = currentTime;
                        reportTimeout = 0; /* quickly enter next loop */
                        hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                    } else {
                        reportTimeout = hifCfg.sendFrameTo - reportTimeout;
                    }
                }
            } else {
                if(hifCtrl.event & HIF_EVENT_REPORT_DONE) {
                    hif_EVENT_Clr(HIF_EVENT_REPORT_DONE);
                }

                bool retryState = false;
#if (CONFIG_HIF_RETRY_ENA != 0)
                retryState = (!hif_FRAME_IsEmpty(&hifCtrl.retry)) || (!hif_FRAME_IsEmpty(&hifCtrl.ackPending));
#endif
                if ((!hif_FRAME_IsEmpty(&hifCtrl.put)) || retryState) {
                    if (hifCtrl.event & HIF_EVENT_REPORT_REFRESH) {
                        //hifCtrl.event &= ~HIF_EVENT_REPORT_REFRESH;
                        hif_EVENT_Clr(HIF_EVENT_REPORT_REFRESH);
                    }
                    hifCtrl.reportState = HIF_REPORT_STATE_NOTIFY;

                    hif_LOG_TimeRecord();
                    hif_IO_Set();
                    hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                    hifCtrl.pmState |= HIF_PM_STATE_REPORT_PREVENT;

                    *pReportTime = currentTime;
                    if (hifCfg.reportMode == HIF_REPORT_MODE_PASSIVE) {
                        hifCtrl.timerState |= HIF_TIMER_REPORT_SEND;
                        reportTimeout = hifCfg.sendFrameTo;
                    } else if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
                        hifCtrl.timerState |= HIF_TIMER_REPORT_DELAY;
                        reportTimeout = hifCfg.sendDelay;
                    } else {
                        /* error */
                        hifCtrl.fault = 3;
                        reportTimeout = 0; /* quickly enter next loop */
                        hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                    }
                }
            }

        }

        break;
    }

    case HIF_REPORT_STATE_NOTIFY:
    {
        if (hifCfg.reportMode == HIF_REPORT_MODE_PASSIVE) {
            if (hifCtrl.timerState & HIF_TIMER_REPORT_SEND) {
                if (hifCtrl.event & HIF_EVENT_REPORT_POLL) {
                    //hifCtrl.event &= ~HIF_EVENT_REPORT_POLL;
                    hif_EVENT_Clr(HIF_EVENT_REPORT_POLL);
                    hifCtrl.reportState = HIF_REPORT_STATE_SEND;
                    //hif_IO_Clr();  /* maybe clear at recv isr */
                    hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                    *pReportTime = currentTime;
                    reportTimeout = 0; /* quickly enter next loop */
                } else {
                    if (hifCtrl.event & HIF_EVENT_REPORT_REFRESH) {
                        //hifCtrl.event &= ~HIF_EVENT_REPORT_REFRESH;
                        hif_EVENT_Clr(HIF_EVENT_REPORT_REFRESH);
                    }

                    reportTimeout = currentTime - *pReportTime;
                    if (reportTimeout >= hifCfg.sendFrameTo) {
                        hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
                        hif_IO_Clr();
                        hif_RPT_Flush();
                        hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                        *pReportTime = currentTime;
                        reportTimeout = 0; /* quickly enter next loop */
                        hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                    } else {
                        reportTimeout = hifCfg.sendFrameTo - reportTimeout;
                    }
                }

            } else  if  (hifCtrl.timerState & HIF_TIMER_REPORT_DELAY) {
                hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                *pReportTime = currentTime;
                hifCtrl.reportState = HIF_REPORT_STATE_SEND;
                reportTimeout = 0; /* quickly enter next loop */

            } else {
                hifCtrl.fault = 3;
                reportTimeout = 0; /* quickly enter next loop */
                hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
            }
        } else if (hifCfg.reportMode == HIF_REPORT_MODE_ACTIOVE) {
            if (hifCtrl.timerState & HIF_TIMER_REPORT_DELAY) {
                reportTimeout = currentTime - *pReportTime;
                if (reportTimeout >= hifCfg.sendDelay) {
                    hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                    reportTimeout = 0;
                    *pReportTime = currentTime;
                    hifCtrl.reportState = HIF_REPORT_STATE_SEND;
                } else {
                    reportTimeout = hifCfg.sendDelay - reportTimeout;
                }
            } else {
                hifCtrl.fault = 3;
                reportTimeout = 0; /* quickly enter next loop */
                hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
            }
        } else {
            /* error */
            hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
            hifCtrl.fault = 3;
            reportTimeout = 0; /* quickly enter next loop */
            hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
        }

        break;
    }

    case HIF_REPORT_STATE_SEND:
    {
        status = hif_RPT_Send();
        if (status == HIF_ERRCODE_SUCCESS) {  /* send done, send fail */
            if (hifCtrl.event & HIF_EVENT_REPORT_DONE) {
                hif_EVENT_Clr(HIF_EVENT_REPORT_DONE);
                hif_IO_Clr();  /* maybe clear at recv isr */
                hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                *pReportTime = currentTime;
                reportTimeout = 0; /* quickly enter next loop */
                hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
                break;
            }

            if (hifCtrl.timerState & HIF_TIMER_REPORT_SEND) {
                *pReportTime = currentTime;
                reportTimeout = hifCfg.sendFrameTo;
            } else {
                hifCtrl.timerState |= HIF_TIMER_REPORT_SEND;
                reportTimeout = hifCfg.sendFrameTo;
                *pReportTime = currentTime;
            }
        } else {
            hifCtrl.timerState |= HIF_TIMER_REPORT_SEND;
            reportTimeout = hifCfg.sendFrameTo;
            *pReportTime = currentTime;
            if(status == HIF_ERRCODE_NO_BUFFER) {
                hifCtrl.reportState = HIF_REPORT_STATE_WAIT_CONTINUE_DATA;
            } else {
                hifCtrl.reportState = HIF_REPORT_STATE_WAIT_SEND_DONE;
            }
        }
        break;
    }

    case HIF_REPORT_STATE_WAIT_CONTINUE_DATA:
    {
        if (hifCtrl.timerState & HIF_TIMER_REPORT_SEND) {
            if (hifCtrl.event & HIF_EVENT_REPORT_DONE) {
                //hifCtrl.event &= ~HIF_EVENT_REPORT_DONE;
                hif_EVENT_Clr(HIF_EVENT_REPORT_DONE);
                hifCtrl.reportState = HIF_REPORT_STATE_SEND;
                hif_IO_Clr();  /* maybe clear at recv isr */
                hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                *pReportTime = currentTime;
                reportTimeout = 0; /* quickly enter next loop */
                hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
            } else {
                if(hifCtrl.put.head != NULL) {
                    hifCtrl.reportState = HIF_REPORT_STATE_SEND;
                    hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                    *pReportTime = currentTime;
                    reportTimeout = 0; /* quickly enter next loop */
                } else {
                    reportTimeout = currentTime - *pReportTime;
                    if (reportTimeout >= hifCfg.sendFrameTo) {
                        hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
                        hif_IO_Clr();
                        hif_DLL_SendFlush();
                        hif_RPT_Flush();
                        hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                        *pReportTime = currentTime;
                        reportTimeout = 0; /* quickly enter next loop */
                        hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                        hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
                    } else {
                        reportTimeout = hifCfg.sendFrameTo - reportTimeout;
                    }
                }
            }
        } else {
            /* error */
            hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
            hifCtrl.fault = 3;
            reportTimeout = 0; /* quickly enter next loop */
            hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
            hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
        }
        break;
    }

    case HIF_REPORT_STATE_WAIT_SEND_DONE:
    {
        if (hifCtrl.timerState & HIF_TIMER_REPORT_SEND) {
            if (hifCtrl.event & HIF_EVENT_REPORT_DONE) {
                //hifCtrl.event &= ~HIF_EVENT_REPORT_DONE;
                hif_EVENT_Clr(HIF_EVENT_REPORT_DONE);
                hifCtrl.reportState = HIF_REPORT_STATE_SEND;
                hif_IO_Clr();  /* maybe clear at recv isr */
                hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                *pReportTime = currentTime;
                reportTimeout = 0; /* quickly enter next loop */
                hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
            } else {
                reportTimeout = currentTime - *pReportTime;
                if (reportTimeout >= hifCfg.sendFrameTo) {
                    hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
                    hif_IO_Clr();
                    hif_DLL_SendFlush();
                    hif_RPT_Flush();
                    hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
                    *pReportTime = currentTime;
                    reportTimeout = 0; /* quickly enter next loop */
                    hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
                    hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
                } else {
                    reportTimeout = hifCfg.sendFrameTo - reportTimeout;
                }
            }
        } else {
            /* error */
            hifCtrl.timerState &= HIF_TIMER_REPORT_MSK;
            hifCtrl.fault = 3;
            reportTimeout = 0; /* quickly enter next loop */
            hifCtrl.pmState &= ~HIF_PM_STATE_REPORT_PREVENT;
            hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
        }

        break;
    }

    default:
        break;
    }



    return reportTimeout;
}


static OSI_Time_t hif_TIME_Get(void)
{
    OSI_Time_t currentTick = OSI_GetTicks();

    return OSI_TicksToMSecs(currentTick);
}


__hif_sram_text static void hif_Task(void *arg)
{
    OSI_Time_t currentTime = hif_TIME_Get();
    OSI_Time_t cmdrspTime = currentTime;
    OSI_Time_t reportTime = currentTime;

    OSI_Time_t cmdrspTimeout;
    OSI_Time_t reportTimeout;

    hif_MSG_Recv_Start();

    hif_MSG_SendStartup();

#if CONFIG_HIF_CMD
    if (hif_cmd_init() != 0) {
        HIF_LOG_DBG("hif commands init failed\n");
    }
#endif

#if CONFIG_HIF_TL_DEBUG_EN
    OSI_Time_t lastTaskWakeTime = 0;
    hif_tl_global_state_print();
    hif_tl_debug_info_print();
#endif

    /* task loop */
    do {
        if (hifCtrl.event & HIF_EVENT_FAULT) {
            HIF_LOG_DBG("\nhif fault\n");
            hif_RPT_Flush();
            hif_IO_Clr();
            hifCtrl.fault = 0;
            hifCtrl.reportState = HIF_REPORT_STATE_IDLE;
            hifCtrl.reportPrevent = 0;
            HAL_BOARD_Reset(HAL_RESET_SYS);
        }


        currentTime = hif_TIME_Get();
        /* 1>. process command.
         * -----------------------------------------------------------------
         *      Receive command
         *      Dispath and process command
         *      Start send response
         *      Reset command timeout
         */
        cmdrspTimeout = hif_TASK_CmdRsp(currentTime, &cmdrspTime);


        /* 2>. process resport.
         * -----------------------------------------------------------------
         * 1. if report or retry data should send.
         *      1.1 Notify Host (Set Host IO)
         *      1.2.1 Wait POLL Cmd(SPI/I2C/UART passive report); Start timer.
         *          a. If receive POLL command, Set ReportEnable, Call ReportSend
         *          b. If timeout, Flush report data
         *      1.2.2 Delay (UART active report); Start timer.
         *          a. Delay done, Call Report
         *
         * 2. if no report data
         *      2.1 free report buffer
         *      2.2 reset SleepPrevent
         */
        reportTimeout = hif_TASK_Report(currentTime, &reportTime);

        /* recover resource */
        hif_RPT_Recover();



        /* 3> process power manage.
         * -----------------------------------------------------------------
         * 1. if report data wait send or transfer, prevent
         * 2. if timeout prevent (sleep timer out)
         *      2.1 wakeup reset
         *      2.2 any command reset
         */
        if (hifCtrl.pmState) {

            HIF_PM_Lock();
        } else {
            if (hif_DLL_SendIsDone()) {
                HIF_PM_Unlock();
            } else {
                reportTimeout = 1;
            }
        }


        if (hifCtrl.event == 0) {
            OSI_Time_t semTimeout = MIN(cmdrspTimeout, reportTimeout);
            OSI_SemaphoreWait(&hifCtrl.semProc, semTimeout);

#if CONFIG_HIF_TL_DEBUG_EN
            if (semTimeout != 0) {
                lastTaskWakeTime = hif_TIME_Get();
            } else {
                OSI_Time_t currTime = hif_TIME_Get();

                if (hif_tl_debug_info.taskConsecutiveCpuTimeMax < (currTime - lastTaskWakeTime)) {
                    hif_tl_debug_info.taskConsecutiveCpuTimeMax = currTime - lastTaskWakeTime;
                }

                if ((currTime - lastTaskWakeTime) >= 3000) {
                        LOG_PRINT("hifCtrl.event 0x%08X, %d\n", hifCtrl.event, (currTime - lastTaskWakeTime));
                        hif_tl_global_state_print();
                        hif_msg_debug_info_print();

                }
            }
        } else {
            OSI_Time_t currTime = hif_TIME_Get();

            if (hif_tl_debug_info.taskConsecutiveCpuTimeMax < (currTime - lastTaskWakeTime)) {
                hif_tl_debug_info.taskConsecutiveCpuTimeMax = currTime - lastTaskWakeTime;
            }

            if ((currTime - lastTaskWakeTime) >= 3000) {
                    LOG_PRINT("hifCtrl.event 0x%08X, %d\n", hifCtrl.event, (currTime - lastTaskWakeTime));
                    hif_tl_global_state_print();
                    hif_msg_debug_info_print();

            }
#endif
        }

    } while (hifCtrlState == HIF_STATE_INIT_SUCCESS);

    hif_RPT_Flush();
    hif_IO_Clr();

    OSI_SemaphoreRelease(&hifCtrl.semExit);

    OSI_ThreadDelete(NULL);
}

/**
 * @brief Retry of transport-layer
 * @param errSeqNum Number of error sequences
 * @param errSeqArray Pointer(errSeqArray[errSeqNum]) of error sequences
 */
void hif_tl_retry(uint8_t errSeqNum, uint8_t *errSeqArray)
{
#if (CONFIG_HIF_RETRY_ENA != 0)

    if ((errSeqArray == NULL) && (errSeqNum != 0)) {
        HIF_LOG_DBG("Error param of hif_tl_retry\n");
        return;
    }

    if (errSeqNum > 8) {
        HIF_LOG_DBG("errSeqNum should less than 8\n");
    }

    if (hifCfg.retryEna) {
#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
        hif_tl_ack_timer_stop();
#endif
        int retryIdx = 0;

        do {
            HIF_Frame_Node_t *pFrameNode = hif_FRAME_PopHead(&hifCtrl.ackPending);
            if (pFrameNode == NULL) {
                break;
            }

            if (retryIdx < errSeqNum) {
                /* Note: ensure ack seq is in order. */
                if (errSeqArray[retryIdx] == ((pFrameNode->hdr[5]>>4) & 0x7)) {
#if CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
                    if (hif_tl_frame_isTimeout(pFrameNode)) {
                        pFrameNode->status = HIF_ERRCODE_TIMEOUT;
                        hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
                    } else {
                        hif_FRAME_Append(&hifCtrl.retry, pFrameNode);
                    }
#else
                    hif_FRAME_Append(&hifCtrl.retry, pFrameNode);
#endif

                    retryIdx++;
                } else {
                    hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
                }
            } else {
                hif_FRAME_Append(&hifCtrl.freePending, pFrameNode);
            }
        } while (true);


#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
        if (hifCtrl.ackPending.head != NULL) {
            if (OSI_STATUS_OK != hif_tl_ack_timer_start(CONFIG_HIF_TL_ACK_TIMEOUT_MS)) {
                HIF_LOG_ERR("Start tl ack timer fail");
            }
        }
#endif
    }

#endif

}

int msghdl_host_poll(HIF_MsgHdr_t *msg)
{
    HOST_POLL_ACK *poll_ack = NULL;
    uint16_t retryFrameCnt = 0;

    if (msg->length) {
        poll_ack = ((HOST_POLL_ACK *)(msg + 1));
        hifCtrl.burstPeriod = poll_ack->burst_period;

        if (poll_ack->poll_type == HOST_POLL_TYPE_ACK) {
            hif_tl_retry(poll_ack->ack_err_num, poll_ack->err_seq);
        } else if(poll_ack->poll_type == HOST_POLL_TYPE_APP_CUBE) {
        	HOST_POLL_APP_CUBE *cube_ack = (HOST_POLL_APP_CUBE *)poll_ack;
            cube_report_retry_handler((uint8_t *)&cube_ack->retry[0], cube_ack->retry_num);
        } else if (poll_ack->poll_type == HOST_POLL_TYPE_MSG_TLVS) {
            if (poll_ack->ack_err_num == 0) {
                /** if poll_ack->poll_type == HOST_POLL_TYPE_MSG_TLVS and poll_ack->ack_err_num == 0,
                 *  that is retry of transport-layer, and no need to retry.
                 */
                hif_tl_retry(0, NULL);
            } else {
                retryFrameCnt = hif_host_ack_tvl_handler((hif_host_poll_ack_tlv_t *)poll_ack);
            }
        }
    } else {

        hifCtrl.burstPeriod = 1;
    }

    if(poll_ack != NULL && poll_ack->burst_period == 0) {
        hif_EVENT_Set(HIF_EVENT_REPORT_POLL_NOACK);
    } else {
        if (retryFrameCnt) {
            hif_EVENT_Set(HIF_EVENT_RSP_SKIP | HIF_EVENT_POLL_ACK_RETRY);
        } else {
            hif_EVENT_Set(HIF_EVENT_RSP_SKIP | HIF_EVENT_REPORT_POLL);
        }
    }


    return 0;
}

/**
 * @brief Refresh a Frame(for retry):
 *        1. Refresh all data node in the frame according to DLL's rule
 *        2. Reset status of the frame
 *        3. Clear some flags of the frame
 * @param pFrame
 */
void hif_TL_RefreshFrame(HIF_Frame_Node_t *pFrame)
{
    if (pFrame == NULL) {
        return;
    }

    /* 1. */
    HIF_Data_Node_t *pDataNode = NULL;
    pDataNode = pFrame->listHead.head;
    while (pDataNode) {
        hif_DLL_SendDataNodeRefresh(pDataNode);
        pDataNode = pDataNode->next;
    }

    /* 2. */
    pFrame->status = 0;

    /* 3. */
    pFrame->flag &= ~HIF_FRAME_FLAG_CHECKSUME;
    pFrame->flag &= ~HIF_TRANS_FLAG_LAST_FRAME;
    // pFrame->flag &= ~HIF_FRAME_FLAG_REFRESH_DATA;
    // pFrame->flag &= ~HIF_FRAME_FLAG_CMD_FRAME
    // pFrame->flag &= ~HIF_FRAME_FLAG_END_FRAME
    // pFrame->flag &= ~HIF_FRAME_FLAG_FRAME_HEAD
    // pFrame->flag &= ~HIF_FRAME_FLAG_FRAME_TAIL
}


#if CONFIG_HIF_TL_DEBUG_EN
void hif_tl_global_state_print(void)
{
//     HIF_Frame_DoubleHead_t       put;
//     HIF_Frame_DoubleHead_t       freePending;
// #if (CONFIG_HIF_RETRY_ENA != 0)
//     HIF_Frame_DoubleHead_t       ackPending;
//     HIF_Frame_DoubleHead_t       retry;
// #endif

    LOG_PRINT("pCmdRspFrame 0x%08X\n", (uint32_t)hifCtrl.pCmdRspFrame);
    LOG_PRINT("pCmdRspHead 0x%08X\n", (uint32_t)hifCtrl.pCmdRspHead);
    LOG_PRINT("pCmdRspData 0x%08X\n", (uint32_t)hifCtrl.pCmdRspData);
    LOG_PRINT("pCmdRspTail 0x%08X\n", (uint32_t)hifCtrl.pCmdRspTail);

    LOG_PRINT("pCmdRspFreeBuff 0x%08X\n", (uint32_t)hifCtrl.pCmdRspFreeBuff);
    LOG_PRINT("pCmdRspBuff 0x%08X\n", (uint32_t)hifCtrl.pCmdRspBuff);

    LOG_PRINT("CmdRspSize %d\n", hifCtrl.CmdRspSize);

    LOG_PRINT("pEndFrame 0x%08X\n", (uint32_t)hifCtrl.pEndFrame);
    LOG_PRINT("pEndData 0x%08X\n", (uint32_t)hifCtrl.pEndData);

    LOG_PRINT("event 0x%08X\n", hifCtrl.event);

    // OSI_Semaphore_t             semProc;
    // OSI_Semaphore_t             semOper;
    // OSI_Semaphore_t             semExit;
    // OSI_Thread_t                hifTaskHandle;

    LOG_PRINT("startupState %d\n", hifCtrl.startupState);

    LOG_PRINT("timerState 0x%02X\n", hifCtrl.timerState);

    LOG_PRINT("pmState 0x%02X\n", hifCtrl.pmState);

    LOG_PRINT("reportState %d\n", hifCtrl.reportState);

    LOG_PRINT("reportPrevent 0x%02X\n", hifCtrl.reportPrevent);

    LOG_PRINT("txSeq %d\n", hifCtrl.txSeq);

    LOG_PRINT("putNodeCnt %d\n", hifCtrl.putNodeCnt);

    LOG_PRINT("msgRecvStartLock %d\n", hifCtrl.msgRecvStartLock);

    LOG_PRINT("burstPeriod %d\n", hifCtrl.burstPeriod);

    LOG_PRINT("fault %d\n", hifCtrl.fault);

    LOG_PRINT("responseCnt %d\n", hifCtrl.responseCnt);
}

void hif_tl_debug_info_print(void)
{
    LOG_PRINT("currentCpuId %d\n", hif_tl_debug_info.currentCpuId);
    LOG_PRINT("cpuResetFlag 0x%08X\n", hif_tl_debug_info.cpuResetFlag);

    LOG_PRINT("frameCnt_from_app %d\n", hif_tl_debug_info.frameCnt_from_app);
    LOG_PRINT("frameCnt_to_app   %d\n", hif_tl_debug_info.frameCnt_to_app);

    LOG_PRINT("frameCnt_to_dll   %d\n", hif_tl_debug_info.frameCnt_to_dll);
    LOG_PRINT("frameCnt_from_dll %d\n", hif_tl_debug_info.frameCnt_from_dll);

    LOG_PRINT("frameCnt_from_ml  %d\n", hif_tl_debug_info.frameCnt_from_ml);
    LOG_PRINT("frameCnt_to_ml    %d\n", hif_tl_debug_info.frameCnt_to_ml);

    LOG_PRINT("taskConsecutiveCpuTimeMax %d\n", hif_tl_debug_info.taskConsecutiveCpuTimeMax);
}
#endif

#endif /* CONFIG_HIF */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
