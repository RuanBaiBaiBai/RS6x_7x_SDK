/**
 ******************************************************************************
 * @file    main.c
 * @brief   main define.
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
#include "common.h"

#include "hif_config.h"
#include "hif.h"
#include "hif_log.h"

#include "log.h"
#include "hal_clock.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */

typedef struct {
    uint32_t    idx;
    uint32_t    len;
    uint32_t    offset;
} Frame_Head_t;

typedef struct {
    uint16_t        blkSize;
    uint8_t         blkCnt;
    uint8_t         pushIdx;
    uint8_t         popIdx;
    uint8_t         trigCnt;

    uint32_t        rptIntvl;

    OSI_Semaphore_t rptSem;
} Report_Ctrl_t;


typedef struct {
    uint32_t        rptLat;
    uint32_t        pollDly;
    uint32_t        headDly;
    uint32_t        guardTime;
    uint32_t        spiClk;
} Time_Meas_t;


typedef struct {
    uint32_t        spiLine;
    uint32_t        timeFreq;
    uint32_t        reportIdx;
    uint32_t        measIdx;
    uint32_t        measThresh;
    Time_Meas_t     maxTime;
    Time_Meas_t     avgTime;

    uint64_t        rptLat;
    uint64_t        pollDly;
    uint64_t        headDly;
    uint64_t        guardTime;
    uint64_t        spiClk;
} Time_Stat_t;


/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define HIF_SAMP_REPORT_TICK_MSG_ID                          0xCF
#define HIF_SAMP_REPORT_CUBE_MSG_ID                          0xC2

/* Private macros.
 * ----------------------------------------------------------------------------
 */
#define HIF_PORT_LOG_PRINT_ENA                              1
#define HIF_PORT_LOG_DBG_ENA                                1

#define HIF_SAMP_REPORT_INTERVAL                            1000    /* ms */
#define HIF_SAMP_MEAS_THRESH                                10

/*
 * fixed (non-modifiable)
 */
#define HIF_SAMP_REPORT_DATA_SIZE                           (32 * 1024)
#define HIF_SAMP_REPORT_BLOCK_SIZE                          (3840)
#define HIF_SAMP_REPORT_BLOCK_CNT                           16

#define HIF_SAMP_REPORT_DATA_HDR_SIZE                       12


#if HIF_PORT_LOG_PRINT_ENA
#define HIF_PORT_LOG_PRINT(fmt, arg...)                     printf(fmt, ##arg)
#else
#define HIF_PORT_LOG_PRINT(fmt, arg...)
#endif


#if HIF_PORT_LOG_DBG_ENA
#define HIF_PORT_LOG_DBG(fmt, arg...)                       printf(fmt, ##arg)
#else
#define HIF_PORT_LOG_DBG(fmt, arg...)
#endif


/* Private variables.
 * ----------------------------------------------------------------------------
 */
static Frame_Head_t frameHead;
static uint8_t dataHead[HIF_SAMP_REPORT_DATA_HDR_SIZE];

static Report_Ctrl_t reportCtrl;

static Time_Stat_t timeMeas;


/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

static int MeasData_Init(void);
static void MeasData_Report(void);
static void Report_Callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status);

static void TimeMeas_Init(void);
static void TimeMeas_Calc(Time_Meas_t *pTime);
static int TimeMeas_Stat(Time_Meas_t *pTime);
static void TimeMeas_Report(void);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

static void TimeMeas_Thread(void * param)
{
    uint32_t sendNodeLevel;
    Time_Meas_t currTime;

    MeasData_Init();
    TimeMeas_Init();

    /* wait start msg report */
    OSI_MSleep(1000);

    for (;;) {

        /* Reset idx */
        reportCtrl.pushIdx = 0;
        reportCtrl.popIdx = 0;
        OSI_SemaphoreWait(&reportCtrl.rptSem, 0);

        /* start meas */
        sendNodeLevel = reportCtrl.trigCnt;
        HIF_ExtControl(HIF_SET_SEND_NODE_LEVEL, &sendNodeLevel, sizeof(sendNodeLevel));

        hif_LOG_TimeEnable();
        MeasData_Report();

        sendNodeLevel = 0;
        HIF_ExtControl(HIF_SET_SEND_NODE_LEVEL, &sendNodeLevel, sizeof(sendNodeLevel));

        /* wait data report done */
        if (OSI_SemaphoreWait(&reportCtrl.rptSem, reportCtrl.rptIntvl)) {
            reportCtrl.pushIdx = 0;
            reportCtrl.popIdx = 0;
            hif_LOG_TimeDisable();
        } else {
            /* Wait NULL packet */
            OSI_MSleep(10);

            hif_LOG_TimeDisable();

            TimeMeas_Calc(&currTime);
            if (TimeMeas_Stat(&currTime) == 1) {
                TimeMeas_Report();
            }

            OSI_MSleep(reportCtrl.rptIntvl);
        }

    }
}


static void TimeMeas_Init(void)
{
    memset(&timeMeas, 0, sizeof(Time_Stat_t));

    HAL_Dev_t *halClk =  HAL_CLOCK_Init();
    timeMeas.timeFreq = HAL_CLOCK_GetFreq(halClk, CLOCK_AHB) / 1000;

    uint32_t spiLine = 0;
    HIF_ExtControl(HIF_GET_COM_PARAM, &spiLine, sizeof(spiLine));
    if (spiLine == 0) {
        timeMeas.spiLine = 1;
    } else if (spiLine == 1) {
        timeMeas.spiLine = 2;
    } else if (spiLine == 2) {
        timeMeas.spiLine = 4;
    }

    timeMeas.measThresh = HIF_SAMP_MEAS_THRESH;
}


static void TimeMeas_Calc(Time_Meas_t *pTime)
{
    if (pTime == NULL) {
        return;
    }

    uint32_t tmpTime = 0;
    uint32_t collectTime[34];

    for (uint8_t idx = 1; idx < 35; idx++) {
        tmpTime = hif_LOG_TimeDump(idx);
        collectTime[idx - 1] = (tmpTime * 1000) / timeMeas.timeFreq;
        HIF_PORT_LOG_DBG("%2d %d\n", idx - 1, collectTime[idx - 1]);
    }
    HIF_PORT_LOG_DBG("\n");

    /*
     * 0 : report-latency + poll-packet
     * 1 : poll-delay + head
     * 2 : head-delay + payload 2048
     * 3/5/7/9/11/13/15/17/19/21/23/25/27/29/31 : inter-packet + head
     * 4/6/8/10/12/14/16/18/20/22/24/26/28/30/32 : head-delay + payload
     *      4 : 256 + 12
     *      6 : 512
     *      8 : 768
     *      10: 1024
     *      12: 1280
     *      14: 1536
     *      16: 1792
     *      18: 2048
     *      20: 2304
     *      22: 2560
     *      24: 2816
     *      26: 3072
     *      28: 3328
     *      30: 3584
     *      32: 3840
     * 33 : inter-packet + head(null)
     *
     */


    /* 256Bytes trans time */
    tmpTime = 0;
    for (uint8_t idx = 0; idx < 28; idx += 2) {
        tmpTime += collectTime[6 + idx] - collectTime[4 + idx];
    }
    float avgTime = tmpTime / 14;
    HIF_PORT_LOG_DBG("256B: %f us\n", avgTime);

    float tmpFreq = 256 * 8 / avgTime;
    tmpFreq /= timeMeas.spiLine;
    pTime->spiClk = tmpFreq;
    HIF_PORT_LOG_DBG("spi clk: %f, %d MHz\n", tmpFreq, pTime->spiClk);


    /* head-delay
     */
    float tmpVal;
    float headDelayTime = 0.0;
    for (uint8_t idx = 0; idx <= 28; idx += 2) {
        tmpVal = (256 * (idx / 2) + 256 + 16) / 256;
        headDelayTime += collectTime[4 + idx] - tmpVal * avgTime;
    }
    headDelayTime /= 15;
    pTime->headDly = headDelayTime;
    HIF_PORT_LOG_DBG("head-delay: %f us\n", headDelayTime);


    /* head-time
     */
    float headTime = (6 * avgTime) / 256;
    HIF_PORT_LOG_DBG("head-time: %f us\n", headTime);


    /* inter-packet
     */
    tmpTime = 0;
    for (uint8_t idx = 0; idx <= 28; idx += 2) {
        tmpTime += collectTime[3 + idx];
    }
    float interPacketTime = tmpTime / 15 - headTime;
    pTime->guardTime = interPacketTime;
    HIF_PORT_LOG_DBG("guard-time: %f us\n", interPacketTime);


    /* poll-delay
     */
    float pollDelayTime = collectTime[1] - headTime;
    pTime->pollDly = pollDelayTime;
    HIF_PORT_LOG_DBG("poll-delay: %f us\n", pollDelayTime);


    /* report-latency
     */
    float reportLatencyTime = collectTime[0] - headTime;
    pTime->rptLat = reportLatencyTime;
    HIF_PORT_LOG_DBG("report-latency: %f us\n", reportLatencyTime);

}


static int TimeMeas_Stat(Time_Meas_t *pTime)
{
    if (pTime == NULL) {
        return -1;
    }

    timeMeas.measIdx++;

    if (timeMeas.maxTime.rptLat < pTime->rptLat) {
        timeMeas.maxTime.rptLat = pTime->rptLat;
    }

    if (timeMeas.maxTime.pollDly < pTime->pollDly) {
        timeMeas.maxTime.pollDly = pTime->pollDly;
    }

    if (timeMeas.maxTime.headDly < pTime->headDly) {
        timeMeas.maxTime.headDly = pTime->headDly;
    }

    if (timeMeas.maxTime.guardTime < pTime->guardTime) {
        timeMeas.maxTime.guardTime = pTime->guardTime;
    }

    if (timeMeas.maxTime.spiClk < pTime->spiClk) {
        timeMeas.maxTime.spiClk = pTime->spiClk;
    }


    timeMeas.rptLat += pTime->rptLat;
    timeMeas.avgTime.rptLat = timeMeas.rptLat / timeMeas.measIdx;

    timeMeas.pollDly += pTime->pollDly;
    timeMeas.avgTime.pollDly = timeMeas.pollDly / timeMeas.measIdx;

    timeMeas.headDly += pTime->headDly;
    timeMeas.avgTime.headDly = timeMeas.headDly / timeMeas.measIdx;

    timeMeas.guardTime += pTime->guardTime;
    timeMeas.avgTime.guardTime = timeMeas.guardTime / timeMeas.measIdx;

    timeMeas.spiClk += pTime->spiClk;
    timeMeas.avgTime.spiClk = timeMeas.spiClk / timeMeas.measIdx;


    HIF_PORT_LOG_PRINT("\nidx [%d]\n", timeMeas.measIdx);
    HIF_PORT_LOG_PRINT("%16s: %12s %12s\n", "type", "avg", "max");
    HIF_PORT_LOG_PRINT("%16s: %12d %12d\n", "report-latency",
                    timeMeas.avgTime.rptLat, timeMeas.maxTime.rptLat);
    HIF_PORT_LOG_PRINT("%16s: %12d %12d\n", "poll-delay",
                    timeMeas.avgTime.pollDly, timeMeas.maxTime.pollDly);
    HIF_PORT_LOG_PRINT("%16s: %12d %12d\n", "head-delay",
                    timeMeas.avgTime.headDly, timeMeas.maxTime.headDly);
    HIF_PORT_LOG_PRINT("%16s: %12d %12d\n", "inter-packet",
                    timeMeas.avgTime.guardTime, timeMeas.maxTime.guardTime);
    HIF_PORT_LOG_PRINT("%16s: %12d %12d\n", "spi-clock",
                    timeMeas.avgTime.spiClk, timeMeas.maxTime.spiClk);

    if (timeMeas.measIdx >= timeMeas.measThresh) {
        timeMeas.reportIdx++;

        timeMeas.measIdx    = 0;
        timeMeas.rptLat     = 0;
        timeMeas.pollDly    = 0;
        timeMeas.headDly    = 0;
        timeMeas.guardTime  = 0;
        timeMeas.spiClk     = 0;

        return 1;
    }

    return 0;
}


static void TimeMeas_Report(void)
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

    uint8_t reportBuff[45];
    reportBuff[0] = 2;
    uint32_t *pReport = (uint32_t *)&reportBuff[1];
    pReport[0] = timeMeas.reportIdx;
    pReport[1] = timeMeas.avgTime.rptLat;
    pReport[2] = timeMeas.avgTime.pollDly;
    pReport[3] = timeMeas.avgTime.headDly;
    pReport[4] = timeMeas.avgTime.guardTime;
    pReport[5] = timeMeas.avgTime.spiClk;
    pReport[6] = timeMeas.maxTime.rptLat;
    pReport[7] = timeMeas.maxTime.pollDly;
    pReport[8] = timeMeas.maxTime.headDly;
    pReport[9] = timeMeas.maxTime.guardTime;
    pReport[10] = timeMeas.maxTime.spiClk;

    HIF_MsgReport(HIF_SAMP_REPORT_TICK_MSG_ID, reportBuff, sizeof(reportBuff), NULL);
}


static int MeasData_Init(void)
{
    int status = 0;

    HIF_AppPool_Cfg_t appPoolCfg = {
        .nodeCnt    = HIF_SAMP_REPORT_BLOCK_CNT,
        .nodeSize   = HIF_SAMP_REPORT_BLOCK_SIZE + sizeof(Frame_Head_t) + HIF_SAMP_REPORT_DATA_HDR_SIZE,
        .buffer     = NULL
    };

    status = hif_MEM_AppPoolInit(&appPoolCfg, 1);
    if (status != HIF_ERRCODE_SUCCESS) {
        return status;
    }

    dataHead[0] = 0x00;
    dataHead[1] = 0x08;
    dataHead[2] = 0x80;
    dataHead[3] = 0x00;
    dataHead[4] = 0x02;
    dataHead[5] = 0x04;
    dataHead[6] = 0x00;
    dataHead[7] = 0x00;
    dataHead[8] = 0x40;
    dataHead[9] = 0x00;
    dataHead[10] = 0x10;
    dataHead[11] = 0x00;

    reportCtrl.blkSize  = HIF_SAMP_REPORT_BLOCK_SIZE;
    reportCtrl.blkCnt   = HIF_SAMP_REPORT_BLOCK_CNT;
    reportCtrl.trigCnt  = reportCtrl.blkCnt;
    reportCtrl.rptIntvl = HIF_SAMP_REPORT_INTERVAL;

    frameHead.len       = HIF_SAMP_REPORT_DATA_SIZE + HIF_SAMP_REPORT_DATA_HDR_SIZE;
    frameHead.idx       = 0;
    frameHead.offset    = 0;


    return 0;
}


static void MeasData_Report(void)
{
    uint8_t *pBuff = NULL;
    uint32_t sendLen = 0;
    uint32_t dataLen = 0;

    for (; reportCtrl.pushIdx < reportCtrl.blkCnt; reportCtrl.pushIdx++) {
        pBuff = hif_MEM_AppNodeMalloc(0);
        if (pBuff == NULL) {
            break;
        }

        sendLen = sizeof(Frame_Head_t);
        if (reportCtrl.pushIdx == 0) {
            /* push frame head */
            frameHead.offset = 0;
            frameHead.idx++;
            memcpy(pBuff, &frameHead, sendLen);

            /* push data head */
            memcpy(&pBuff[sendLen], dataHead, HIF_SAMP_REPORT_DATA_HDR_SIZE);
            sendLen += HIF_SAMP_REPORT_DATA_HDR_SIZE;

            dataLen = 2048;
            memset(&pBuff[sendLen], 0x55, dataLen);
            sendLen += dataLen;

            /* update offset */
            frameHead.offset = dataLen + HIF_SAMP_REPORT_DATA_HDR_SIZE;

        } else {
            /* push frame head */
            memcpy(pBuff, &frameHead, sendLen);

            dataLen = reportCtrl.pushIdx * 256;
            memset(&pBuff[sendLen], 0x55, dataLen);
            sendLen += dataLen;

            /* update offset */
            frameHead.offset += dataLen;
        }

        int status = HIF_MsgReport(HIF_SAMP_REPORT_CUBE_MSG_ID, pBuff, sendLen, Report_Callback);
        if (status != HIF_ERRCODE_SUCCESS) {
            break;
        }
    }
}


static void Report_Callback(uint8_t msgId, HIF_Data_ListHead_t * pDataListHead, int8_t status)
{
    LOG_IO(0x1000, 11, 0, 2);

    reportCtrl.popIdx++;

    uint8_t *pBuff = NULL;
    do {
        pBuff = HIF_MsgReport_ListPopData(pDataListHead);
        if (pBuff == NULL) {
            break;
        }

        hif_MEM_AppNodeFree(pBuff);
    } while (1);

    MeasData_Report();

    if (reportCtrl.popIdx >= reportCtrl.blkCnt) {
        reportCtrl.popIdx = 0;
        OSI_SemaphoreRelease(&reportCtrl.rptSem);
    }

}


int main(void)
{
    int status = HAL_STATUS_OK;

    HIF_PORT_LOG_PRINT("-----------------------------------------------\n");
    HIF_PORT_LOG_PRINT("\tHIF Port Device Sample\n");
    HIF_PORT_LOG_PRINT("\t\thost timer meas\n");
    HIF_PORT_LOG_PRINT("-----------------------------------------------\n");


    HIF_PORT_LOG_PRINT("\n\n1>. Init HIF\n");

    HIF_CfgParam_t hifCfgParam;
    #if (CONFIG_HIF_PHY_TYPE & 1)
        HIF_PORT_LOG_PRINT("\t1.1. Communication type: UART 921600.\n");
        HIF_DefaultInitParam(&hifCfgParam, HIF_COM_TYPE_UART, 921600);
    #elif (CONFIG_HIF_PHY_TYPE & 2)
        HIF_PORT_LOG_PRINT("\t1.1. Communication type: I2C 400000.\n");
        HIF_DefaultInitParam(&hifCfgParam, HIF_COM_TYPE_IIC, 400000);
    #elif (CONFIG_HIF_PHY_TYPE & 4)
        char * pSpiLineStr = NULL;
        #if (CONFIG_HIF_PHY_SPI_DEF_LINE == 0)
            pSpiLineStr = "Std";
        #elif (CONFIG_HIF_PHY_SPI_DEF_LINE == 1)
            pSpiLineStr = "Dual";
        #elif (CONFIG_HIF_PHY_SPI_DEF_LINE == 2)
            pSpiLineStr = "Quad";
        #else
            pSpiLineStr = "Null";
        #endif
        HIF_PORT_LOG_PRINT("\t1.1. Communication type: SPI-%s 64000000.\n", pSpiLineStr);
        HIF_DefaultInitParam(&hifCfgParam, HIF_COM_TYPE_SPI, 56000000);
    #endif

    HIF_PORT_LOG_PRINT("\t1.2. Init\n");
    status = HIF_Init(&hifCfgParam);
    if (status != HAL_STATUS_OK) {
        HIF_PORT_LOG_PRINT("\t     FAIL (%d)\n", status);
        return status;
    }
    HIF_PORT_LOG_PRINT("\tTime Meas start ...\n");

    OSI_SemaphoreSetInvalid(&reportCtrl.rptSem);
    OSI_SemaphoreCreate(&reportCtrl.rptSem, 0, 1);

    OSI_Thread_t xHandle;
    OSI_ThreadSetInvalid(&xHandle);
    OSI_Status_t ret = OSI_ThreadCreate(&xHandle,
                        "HIF test thread",
                        TimeMeas_Thread,
                        NULL,
                        4,
                        2048);
    if (ret != OSI_STATUS_OK) {
        LOG_ERR("create main thread fail %d\n", ret);
        return HAL_STATUS_ERROR;
    }


    return 0;
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
