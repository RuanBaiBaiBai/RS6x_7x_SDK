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


#if HIF_TEST_SPLIT_ENA
#define HIF_SEND_TEST_BUF_LEN_1     (CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE + 18)
#if (HIF_TEST_SEND_API & HIF_TEST_ListStart)
#define HIF_SEND_TEST_BUF_LEN_2     ((uint32_t)(CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE/2))
#define HIF_SEND_TEST_BUF_LEN_3     (CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE)
#endif
#else
#define HIF_SEND_TEST_BUF_LEN_1     ((uint32_t)(CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE/5))
#if (HIF_TEST_SEND_API & HIF_TEST_ListStart)
#define HIF_SEND_TEST_BUF_LEN_2     (HIF_SEND_TEST_BUF_LEN_1)
#define HIF_SEND_TEST_BUF_LEN_3     (HIF_SEND_TEST_BUF_LEN_1)
#endif
#endif

static uint8_t *sendBuf_1  = NULL;
#if (HIF_TEST_SEND_API & HIF_TEST_ListStart)
static uint8_t *sendBuf_2  = NULL;
static uint8_t *sendBuf_3  = NULL;
#endif

static uint32_t sendCnt = 0;        // Counts successful sending

#if (HIF_TEST_BLOCK_ENA == 0)
static uint32_t callbackCnt = 0;    // Counts calling callback
#endif

#if (HIF_TEST_BLOCK_ENA == 0)
/**
 * @brief Sending callback
 * @param msgId
 * @param dataList
 * @param status
 */
static void hif_send_callback(uint8_t msgId, HIF_Data_DoubleHead_t *dataList, int8_t status)
{
    callbackCnt++;

    uint8_t *bufAddr = NULL;

    LOG_PRINT("hif_send_callback() is called, ID 0x%02X, status %d\n", msgId, status);

    do {
        bufAddr = HIF_MsgReport_ListPopData(dataList);
        if (bufAddr == NULL) {
            break;
        }
        LOG_PRINT("hif_send_callback pop bufAddr 0x%08X\n", (uint32_t)bufAddr);
    } while (1);
}
#endif


/**
 * @brief Init send buffers for test
 * @return int: HIF_ERRCODE_SUCCESS or HIF_ERRCODE_NO_BUFFER.
 */
static int hif_test_init_sendBuf(void)
{
    LOG_PRINT("init send buffers for test\n");

    sendBuf_1 = OSI_Malloc(HIF_SEND_TEST_BUF_LEN_1);
    if (sendBuf_1 == NULL) {
        LOG_PRINT("malloc sendBuf_1 fail\n");
        return HIF_ERRCODE_NO_BUFFER;
    }
    for (uint32_t i = 0; i < HIF_SEND_TEST_BUF_LEN_1; i++) {
        sendBuf_1[i] = 0x11;
    }

#if (HIF_TEST_SEND_API & HIF_TEST_ListStart)
    sendBuf_2 = OSI_Malloc(HIF_SEND_TEST_BUF_LEN_2);
    if (sendBuf_2 == NULL) {
        OSI_Free(sendBuf_1);

        LOG_PRINT("malloc sendBuf_2 fail\n");
        return HIF_ERRCODE_NO_BUFFER;
    }
    for (uint32_t i = 0; i < HIF_SEND_TEST_BUF_LEN_2; i++) {
        sendBuf_2[i] = 0x22;
    }

    sendBuf_3 = OSI_Malloc(HIF_SEND_TEST_BUF_LEN_3);
    if (sendBuf_3 == NULL) {
        OSI_Free(sendBuf_1);
        OSI_Free(sendBuf_2);

        LOG_PRINT("malloc sendBuf_3 fail\n");
        return HIF_ERRCODE_NO_BUFFER;
    }
    for (uint32_t i = 0; i < HIF_SEND_TEST_BUF_LEN_3; i++) {
        sendBuf_3[i] = 0x33;
    }
#endif

    return HIF_ERRCODE_SUCCESS;
}


int main(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    LOG_PRINT("-----------------------------------------------\n");
    LOG_PRINT("\tHIF Split Test Demo\n");
    LOG_PRINT("-----------------------------------------------\n");

    LOG_PRINT("init hif\n");

    HIF_CfgParam_t hifInitParam;

#if (HIF_TEST_COM_TYPE == 1)
    LOG_PRINT("hif based on uart\n");
    HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_UART, 921600);
#elif (HIF_TEST_COM_TYPE == 2)
    LOG_PRINT("hif based on iic\n");
    HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_IIC, 400000);
#elif (HIF_TEST_COM_TYPE == 3)
    LOG_PRINT("hif based on spi\n");
    HIF_DefaultInitParam(&hifInitParam, HIF_COM_TYPE_SPI, 56000000);
#else
    #error "hif baseed on none"
#endif

    LOG_PRINT("hif max payload length %d\n", CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE);

    status = HIF_Init(&hifInitParam);

    if (status != HIF_ERRCODE_SUCCESS) {
        LOG_PRINT("host interface init fail! ret = %d \r\n", status);
        return 1;
    }

    status = hif_test_init_sendBuf();

    if (status != 0) {
        LOG_PRINT("init sendBuf fail, ret = %d \r\n", status);
        return 1;
    }

    OSI_MSleep(10);

    for (uint32_t idx = 0; ; idx++) {
#if (HIF_TEST_BLOCK_ENA)
        LOG_PRINT("\n---- alive [%d]; sendCnt %d ----\n", idx, sendCnt);
#else
        LOG_PRINT("\n---- alive [%d]; sendCnt %d, callbackCnt %d ----\n", idx, sendCnt, callbackCnt);
#endif

        OSI_MSleep(HIF_TEST_SEND_PERIOD_MS);

#if (HIF_TEST_SEND_API & HIF_TEST_MsgReport)
    #if HIF_TEST_BLOCK_ENA
        status = HIF_MsgReport(HIF_TEST_MsgReport_ID, sendBuf_1, HIF_SEND_TEST_BUF_LEN_1, NULL);
        if (status != HIF_ERRCODE_SUCCESS) {
            LOG_PRINT("HIF_MsgReport blocked send 0x%02X, length %d, fail %d\n\r", HIF_TEST_MsgReport_ID, HIF_SEND_TEST_BUF_LEN_1, status);
        } else {
            sendCnt++;
            LOG_PRINT("HIF_MsgReport blocked send 0x%02X, length %d, bufAddr 0x%08X, success\n\r", HIF_TEST_MsgReport_ID, HIF_SEND_TEST_BUF_LEN_1, (uint32_t)sendBuf_1);
        }
    #else
        status = HIF_MsgReport(HIF_TEST_MsgReport_ID, sendBuf_1, HIF_SEND_TEST_BUF_LEN_1, hif_send_callback);
        if (status != HIF_ERRCODE_SUCCESS) {
            LOG_PRINT("HIF_MsgReport unblocked send 0x%02X, length %d, fail %d\n\r", HIF_TEST_MsgReport_ID, HIF_SEND_TEST_BUF_LEN_1, status);
        } else {
            sendCnt++;
            LOG_PRINT("HIF_MsgReport unblocked send 0x%02X, length %d, bufAddr 0x%08X, success\n\r", HIF_TEST_MsgReport_ID, HIF_SEND_TEST_BUF_LEN_1, (uint32_t)sendBuf_1);
        }
    #endif
#endif

#if (HIF_TEST_SEND_API & HIF_TEST_ListStart)

        HIF_Data_ListHead_t dataList = {0};

        status = HIF_MsgReport_ListPushData(&dataList, sendBuf_1, HIF_SEND_TEST_BUF_LEN_1);
        if (status != HIF_ERRCODE_SUCCESS) {
            while (HIF_MsgReport_ListPopData(&dataList)) {;}
            LOG_PRINT("HIF_MsgReport_ListPushData sendBuf_1 fail %d\n", status);
            continue;
        }

        status = HIF_MsgReport_ListPushData(&dataList, sendBuf_2, HIF_SEND_TEST_BUF_LEN_2);
        if (status != HIF_ERRCODE_SUCCESS) {
            while (HIF_MsgReport_ListPopData(&dataList)) {;}
            LOG_PRINT("HIF_MsgReport_ListPushData sendBuf_2 fail %d\n", status);
            continue;
        }

        status = HIF_MsgReport_ListPushData(&dataList, sendBuf_3, HIF_SEND_TEST_BUF_LEN_3);
        if (status != HIF_ERRCODE_SUCCESS) {
            while (HIF_MsgReport_ListPopData(&dataList)) {;}
            LOG_PRINT("HIF_MsgReport_ListPushData sendBuf_3 fail %d\n", status);
            continue;
        }

        #if HIF_TEST_BLOCK_ENA
            status = HIF_MsgReport_ListStart(HIF_TEST_ListStart_ID, &dataList, NULL);
            while (HIF_MsgReport_ListPopData(&dataList)) {;}
            if (status != HIF_ERRCODE_SUCCESS) {
                LOG_PRINT("HIF_MsgReport_ListStart blocked send 0x%02X, length %d, fail %d\n\r",
                            HIF_TEST_ListStart_ID, HIF_SEND_TEST_BUF_LEN_1 + HIF_SEND_TEST_BUF_LEN_2 + HIF_SEND_TEST_BUF_LEN_3, status);
            } else {
                sendCnt++;
                LOG_PRINT("HIF_MsgReport_ListStart blocked send 0x%02X, length %d, bufAddr 0x%08X, 0x%08X, 0x%08X, success\n\r",
                            HIF_TEST_ListStart_ID, HIF_SEND_TEST_BUF_LEN_1 + HIF_SEND_TEST_BUF_LEN_2 + HIF_SEND_TEST_BUF_LEN_3,
                            (uint32_t)sendBuf_1, (uint32_t)sendBuf_2, (uint32_t)sendBuf_3);
            }
        #else
            status = HIF_MsgReport_ListStart(HIF_TEST_ListStart_ID, &dataList, hif_send_callback);
            if (status != HIF_ERRCODE_SUCCESS) {
                while (HIF_MsgReport_ListPopData(&dataList)) {;}
                LOG_PRINT("HIF_MsgReport_ListStart unblocked send 0x%02X, length %d, fail %d\n\r",
                            HIF_TEST_ListStart_ID, HIF_SEND_TEST_BUF_LEN_1 + HIF_SEND_TEST_BUF_LEN_2 + HIF_SEND_TEST_BUF_LEN_3, status);
            } else {
                sendCnt++;
                LOG_PRINT("HIF_MsgReport_ListStart unblocked send 0x%02X, length %d, bufAddr 0x%08X, 0x%08X, 0x%08X, success\n\r",
                            HIF_TEST_ListStart_ID, HIF_SEND_TEST_BUF_LEN_1 + HIF_SEND_TEST_BUF_LEN_2 + HIF_SEND_TEST_BUF_LEN_3,
                            (uint32_t)sendBuf_1, (uint32_t)sendBuf_2, (uint32_t)sendBuf_3);
            }
        #endif
#endif
    }
    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

