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

#include "log.h"
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */

#define HIF_SAMP_CMDRSP_CMD_ID                          0x60
#define HIF_SAMP_REPORT_MSG_ID                          0xC7

/* Private macros.
 * ----------------------------------------------------------------------------
 */

#define HIF_SAMP_REPORT_DELAY                           1000

/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static int hif_SAMP_MsgHandle(HIF_MsgHdr_t *pMsg);

static int hif_TMP_MsgHandle(HIF_MsgHdr_t *pMsg);

static void hif_SAMP_ReportCallback(uint8_t msgId, HIF_Data_DoubleHead_t *pDataList, int8_t status);

static void hif_SAMP_ListReportCallback(uint8_t msgId, HIF_Data_DoubleHead_t *pDataList, int8_t status);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


int main(void)
{
    int status = HAL_STATUS_OK;

    LOG_PRINT("-----------------------------------------------\n");
    LOG_PRINT("\tHIF Sample\n");
    LOG_PRINT("-----------------------------------------------\n");


    LOG_PRINT("\n\n1>. Init HIF\n");

    LOG_PRINT("\t1.1. Communication type: I2C.\n");
    LOG_PRINT("\t     Obtain default parameter configuration.\n");
    HIF_CfgParam_t hifCfgParam;
    HIF_DefaultInitParam(&hifCfgParam, HIF_COM_TYPE_IIC, 400000);

    LOG_PRINT("\t1.2. Init\n");
    status = HIF_Init(&hifCfgParam);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("\t     FAIL (%d)\n", status);
        return status;
    }
    LOG_PRINT("\t     SUCCESS\n");



    LOG_PRINT("\n\n2>. Register message handler\n");
    LOG_PRINT("\t     Message ID: 0x%02X\n", HIF_SAMP_CMDRSP_CMD_ID);
    LOG_PRINT("\t     The host can send a command with MsgId 0x%02X to the device,\n", HIF_SAMP_CMDRSP_CMD_ID);
    LOG_PRINT("\t     and the device will response the process status and data for this command.\n");
    HIF_MsgHdl_Regist(HIF_SAMP_CMDRSP_CMD_ID, hif_SAMP_MsgHandle);
    HIF_MsgHdl_Regist(HIF_MSG_ID_START_CTRL, hif_TMP_MsgHandle);
    OSI_MSleep(10);




    LOG_PRINT("\n\n3>. Report sample\n");
    LOG_PRINT("\t     The device initiates data reporting to the host.\n");
    LOG_PRINT("\t     If the communication type is SPI. you need to enable 'Start SPI scan' on the tool.\n");

    /* Refer to hif_msg.h */
    do {

        LOG_PRINT("\n\t3.1. Block Report\n");
        LOG_PRINT("\t     If the reporte data is a local variable reporting needs to be blocked.\n");
        uint8_t reportBlockData[] = {'B', 'l', 'o', 'c', 'k'};
        LOG_HEX(reportBlockData, sizeof(reportBlockData), "report data");
        status = HIF_MsgReport(HIF_SAMP_REPORT_MSG_ID, reportBlockData, sizeof(reportBlockData), NULL);
        if (status != HIF_ERRCODE_SUCCESS) {
            LOG_PRINT("\t     FAIL\n");
        } else {
            LOG_PRINT("\t     SUCCESS\n");
        }


        OSI_MSleep(HIF_SAMP_REPORT_DELAY);



        LOG_PRINT("\n\t3.2. Unblock Report\n");
        uint8_t *pReportData = NULL;
        pReportData = OSI_Malloc(8);
        if (pReportData == NULL) {
            LOG_PRINT("\t     Malloc FAIL\n");
            break;
        } else {
            LOG_PRINT("\t     Malloc SUCCESS: %p\n", pReportData);
        }

        pReportData[0] = 'U';
        pReportData[1] = 'n';
        pReportData[2] = 'b';
        pReportData[3] = 'l';
        pReportData[4] = 'o';
        pReportData[5] = 'c';
        pReportData[6] = 'k';
        pReportData[7] = 0;
        LOG_HEX(pReportData, sizeof(pReportData), "report data");
        status = HIF_MsgReport(HIF_SAMP_REPORT_MSG_ID, pReportData, 8, hif_SAMP_ReportCallback);
        if (status != HIF_ERRCODE_SUCCESS) {
            LOG_PRINT("\t     FAIL (%d)\n", status);

            LOG_PRINT("\t     Free report buffer\n");
            OSI_Free(pReportData);
        } else {
            LOG_PRINT("\t     SUCCESS\n");
        }


        OSI_MSleep(HIF_SAMP_REPORT_DELAY);



        LOG_PRINT("\n\t3.3. List block Report\n");

        LOG_PRINT("\t     Define and init report data list\n");
        HIF_Data_ListHead_t reportList = {NULL, NULL};

        do {

            LOG_PRINT("\t     Push data A\n");
            uint8_t reportListDataA[] = {'L', 'i', 's', 't'};
            LOG_HEX(reportListDataA, sizeof(reportListDataA), "report data list-A");
            status = HIF_MsgReport_ListPushData(&reportList, reportListDataA, sizeof(reportListDataA));
            if (status != HIF_ERRCODE_SUCCESS) {
                LOG_PRINT("\t     FAIL\n");
                break;
            }

            LOG_PRINT("\t     Push data B\n");
            uint8_t reportListDataB[5] = {'B', 'l', 'o', 'c', 'k'};
            LOG_HEX(reportListDataB, sizeof(reportListDataB), "report data list-B");
            status = HIF_MsgReport_ListPushData(&reportList, reportListDataB, sizeof(reportListDataB));
            if (status != HIF_ERRCODE_SUCCESS) {
                LOG_PRINT("\t     FAIL\n");
                break;
            }


            LOG_PRINT("\t     Push data C\n");
            uint8_t reportListDataC[6] = {'R', 'e', 'p', 'o', 'r', 't'};
            LOG_HEX(reportListDataC, sizeof(reportListDataC), "report data list-C");
            status = HIF_MsgReport_ListPushData(&reportList, reportListDataC, sizeof(reportListDataC));
            if (status != HIF_ERRCODE_SUCCESS) {
                LOG_PRINT("\t     FAIL\n");
                break;
            }

            LOG_PRINT("\t     Start block send\n");
            status = HIF_MsgReport_ListStart(HIF_SAMP_REPORT_MSG_ID, &reportList, NULL);

        } while (0);

        if (status != HIF_ERRCODE_SUCCESS) {
            LOG_PRINT("\t     Free data list and report buffer\n");
            uint8_t *pBuff = NULL;
            do {
                pBuff = HIF_MsgReport_ListPopData(&reportList);
                if (pBuff == NULL) {
                    /* complete */
                    break;
                }

                /* add free report buffer */
            } while (1);
        }


        OSI_MSleep(HIF_SAMP_REPORT_DELAY);



        LOG_PRINT("\n\t3.4. List unblock Report\n");
        /* must init list */
        reportList.head = NULL;
        reportList.tail = NULL;

        do {

            LOG_PRINT("\t     Push data A\n");
            uint8_t *pReportListData = OSI_Malloc(4);
            if (pReportListData == NULL) {
                LOG_PRINT("\t     Malloc FAIL\n");
                break;
            } else {
                LOG_PRINT("\t     Malloc SUCCESS: %p\n", pReportListData);
            }

            pReportListData[0] = 'L';
            pReportListData[1] = 'i';
            pReportListData[2] = 's';
            pReportListData[3] = 't';
            LOG_HEX(pReportListData, 4, "report data list-A");
            status = HIF_MsgReport_ListPushData(&reportList, pReportListData, 4);
            if (status != HIF_ERRCODE_SUCCESS) {
                LOG_PRINT("\t     FAIL\n");
                break;
            }

            LOG_PRINT("\t     Push data B\n");
            pReportListData = OSI_Malloc(7);
            if (pReportListData == NULL) {
                LOG_PRINT("\t     Malloc FAIL\n");
                break;
            } else {
                LOG_PRINT("\t     Malloc SUCCESS: %p\n", pReportListData);
            }

            pReportListData[0] = 'U';
            pReportListData[1] = 'n';
            pReportListData[2] = 'b';
            pReportListData[3] = 'l';
            pReportListData[4] = 'o';
            pReportListData[5] = 'c';
            pReportListData[6] = 'k';
            LOG_HEX(pReportListData, 7, "report data list-B");
            status = HIF_MsgReport_ListPushData(&reportList, pReportListData, 7);
            if (status != HIF_ERRCODE_SUCCESS) {
                LOG_PRINT("\t     FAIL\n");
                break;
            }


            LOG_PRINT("\t     Push data C\n");
            pReportListData = OSI_Malloc(6);
            if (pReportListData == NULL) {
                LOG_PRINT("\t     Malloc FAIL\n");
                break;
            } else {
                LOG_PRINT("\t     Malloc SUCCESS: %p\n", pReportListData);
            }

            pReportListData[0] = 'R';
            pReportListData[1] = 'e';
            pReportListData[2] = 'p';
            pReportListData[3] = 'o';
            pReportListData[4] = 'r';
            pReportListData[5] = 't';
            LOG_HEX(pReportListData, 6, "report data list-C");
            status = HIF_MsgReport_ListPushData(&reportList, pReportListData, 6);
            if (status != HIF_ERRCODE_SUCCESS) {
                LOG_PRINT("\t     FAIL\n");
                break;
            }

            LOG_PRINT("\t     Start block send\n");
            status = HIF_MsgReport_ListStart(HIF_SAMP_REPORT_MSG_ID, &reportList, hif_SAMP_ListReportCallback);

        } while (0);

        if (status != HIF_ERRCODE_SUCCESS) {
            LOG_PRINT("\t     Free data list and report buffer\n");
            uint8_t *pBuff = NULL;
            do {
                pBuff = HIF_MsgReport_ListPopData(&reportList);
                if (pBuff == NULL) {
                    /* complete */
                    break;
                }

                /* add free report buffer */
                LOG_PRINT("\t     Free report buffer %p\n", pBuff);
                OSI_Free(pBuff);
            } while (1);
        }



        OSI_MSleep(HIF_SAMP_REPORT_DELAY);

    } while (1);


    LOG_PRINT("\n\nSAMPLE Done\n");


    return 0;
}



static int hif_SAMP_MsgHandle(HIF_MsgHdr_t *pMsg)
{
    static uint8_t sampSeq = 0;

    int8_t status = HIF_CMD_STATUS_SUCCESS;
    uint16_t respDataLen = 0;
    uint8_t *pRespData = HIF_Msg_AckDataPtr(pMsg);

    LOG_PRINT("\n\tRcv Msg\n");
    LOG_PRINT("\t\tmsg-id: %02X\n", pMsg->msg_id);
    LOG_PRINT("\t\tlen:    %d\n", pMsg->length);
    LOG_PRINT("\t\tseq:    %d\n", pMsg->seq);
    LOG_PRINT("\t\ttype:   %d\n", pMsg->type);
    LOG_PRINT("\t\tflag:   %d\n", pMsg->flag);
    LOG_PRINT("\t\tfrag:   %d\n", pMsg->frag);
    LOG_PRINT("\t\tSend Ack : %d\n", status);
    LOG_PRINT("\t\tResp Data : 0x%02X\n", sampSeq);

    pRespData[0] = sampSeq++;
    respDataLen = 1;


    return HIF_MsgResp(pMsg, respDataLen, status);
}


static int hif_TMP_MsgHandle(HIF_MsgHdr_t *pMsg)
{
    int8_t status = HIF_CMD_STATUS_SUCCESS;
    uint16_t respDataLen = 0;

    return HIF_MsgResp(pMsg, respDataLen, status);
}


static void hif_SAMP_ListReportCallback(uint8_t msgId, HIF_Data_DoubleHead_t *pDataList, int8_t status)
{
    LOG_PRINT("\t     List report callback. msgId: %02x, status: %d\n", msgId, status);

    uint8_t *pBuff = NULL;
    do {
        pBuff = HIF_MsgReport_ListPopData(pDataList);
        if (pBuff == NULL) {
            /* complete */
            break;
        }

        /* add free report buffer */
        LOG_PRINT("\t     Free report buffer %p\n", pBuff);
        OSI_Free(pBuff);
    } while (1);
}


static void hif_SAMP_ReportCallback(uint8_t msgId, HIF_Data_DoubleHead_t *pDataList, int8_t status)
{
    LOG_PRINT("\t     Unblock report callback. msgId: %02x, status: %d\n", msgId, status);

    uint8_t *pBuff = HIF_MsgReport_ListPopData(pDataList);
    LOG_PRINT("\t     Free report buffer: %p\n", pBuff);
    OSI_Free(pBuff);
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
