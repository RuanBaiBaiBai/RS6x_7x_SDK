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
#include "kvr.h"

#include "log.h"
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    uint32_t ivBuildNum;
    uint16_t ivRevision;
    uint8_t ivMinor;
    uint8_t ivMajor;

    uint32_t addr;
    uint16_t size;

    uint8_t id;
    uint8_t pad;
} cfgImgInfo_t;

typedef struct __packed {
    uint16_t irMinor;
    uint8_t  irType;

    uint8_t  imProject;
    uint16_t imNumber;
    uint16_t imMinor;

    uint8_t  ivMajor;
    uint8_t  ivMinor;
    uint16_t ivRevision;

    uint32_t addr;
    uint32_t size;
} mergeImgInfo_t;
/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
#define KVR_TEST_BUFF_SIZE                  32
#define KVR_TEST_ID                         3

/* Private variables.
 * ----------------------------------------------------------------------------
 */

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int main(void)
{
    int ret = 0;

    LOG_PRINT("Key-Value ram Demo\n\n");


    LOG_PRINT("\n1> Get Sys KVR ------------------ \n");
    mergeImgInfo_t mergeImgInfo;
    LOG_PRINT("\nload merge img info\n");
    ret = KVR_Load(KVR_SYS_ID_MERGE_IMG_INFO, &mergeImgInfo, sizeof(mergeImgInfo_t));
    if (ret < 0) {
        LOG_PRINT("load kvr fail (%d)\n", ret);
    } else if (ret != sizeof(mergeImgInfo_t)) {
        LOG_PRINT("load kvr len error (%d)\n", ret);
    } else {
        LOG_PRINT("V %d.%d.%d - R %d.%d M %d.%d.%d\n",
            mergeImgInfo.ivMajor, mergeImgInfo.ivMinor, mergeImgInfo.ivRevision,
            mergeImgInfo.irMinor, mergeImgInfo.irType,
            mergeImgInfo.imProject, mergeImgInfo.imNumber, mergeImgInfo.imMinor);

        LOG_PRINT("addr: %08X\nsize: %08X\n", mergeImgInfo.addr, mergeImgInfo.size);
    }


    cfgImgInfo_t cfgImgInfo;
    LOG_PRINT("\nload flash list info\n");
    ret = KVR_Load(KVR_SYS_ID_FLASH_LIST_IMG_INFO, &cfgImgInfo, sizeof(cfgImgInfo_t));
    if (ret < 0) {
        LOG_PRINT("load kvr fail (%d)\n", ret);
    } else if (ret != sizeof(cfgImgInfo_t)) {
        LOG_PRINT("load kvr len error (%d)\n", ret);
    } else {
        LOG_PRINT("V %d.%d.%d+%08X\n",
            cfgImgInfo.ivMajor, cfgImgInfo.ivMinor, cfgImgInfo.ivRevision, cfgImgInfo.ivBuildNum);

        LOG_PRINT("addr: %08X\nsize: %08X\n", cfgImgInfo.addr, cfgImgInfo.size);
    }


    LOG_PRINT("\nload mmw custom cfg info\n");
    ret = KVR_Load(KVR_SYS_ID_MMW_CUSTOM_CFG_IMG_INFO, &cfgImgInfo, sizeof(cfgImgInfo_t));
    if (ret < 0) {
        LOG_PRINT("load kvr fail (%d)\n", ret);
    } else if (ret != sizeof(cfgImgInfo_t)) {
        LOG_PRINT("load kvr len error (%d)\n", ret);
    } else {
        LOG_PRINT("V %d.%d.%d+%08X\n",
            cfgImgInfo.ivMajor, cfgImgInfo.ivMinor, cfgImgInfo.ivRevision, cfgImgInfo.ivBuildNum);

        LOG_PRINT("addr: %08X\nsize: %08X\n", cfgImgInfo.addr, cfgImgInfo.size);
    }


    LOG_PRINT("\n2.1> Save user KVR -------------- \n");
    uint16_t kvr_id = KVR_TEST_ID;
    uint8_t kvr_data[KVR_TEST_BUFF_SIZE];
    uint32_t kvr_size = 7;
    for (int idx = 0; idx < kvr_size; idx++) {
        kvr_data[idx] = idx;
    }
    ret = KVR_Save(kvr_id, kvr_data, kvr_size);
    if (ret != kvr_size) {
        LOG_PRINT("save kvr fail (%d)\n", ret);
    } else {
        LOG_PRINT("save kvr success\n");
    }

    LOG_PRINT("\n2.2> Load user KVR -------------- \n");
    for (int idx = 0; idx < KVR_TEST_BUFF_SIZE; idx++) {
        kvr_data[idx] = 0;
    }
    ret = KVR_Load(kvr_id, kvr_data, kvr_size);
    if (ret != kvr_size) {
        LOG_PRINT("load kvr fail (%d)\n", ret);
    } else {
        LOG_PRINT("load kvr success\n");
        for (int idx = 0; idx < kvr_size; idx++) {
            LOG_PRINT("%02X ", kvr_data[idx]);
        }
        LOG_PRINT("\n");
    }

    LOG_PRINT("\n2.3> Delete user KVR ------------ \n");
    ret = KVR_Delete(kvr_id);
    if (ret != 0) {
        LOG_PRINT("delete kvr fail (%d)\n", ret);
    } else {
        LOG_PRINT("delete kvr success\n");
    }

    LOG_PRINT("\n2.4> Load user KVR -------------- \n");
    for (int idx = 0; idx < KVR_TEST_BUFF_SIZE; idx++) {
        kvr_data[idx] = 0;
    }
    ret = KVR_Load(kvr_id, kvr_data, kvr_size);
    if (ret != kvr_size) {
        LOG_PRINT("load kvr fail (%d)\n", ret);
    } else {
        LOG_PRINT("load kvr success\n");
        for (int idx = 0; idx < kvr_size; idx++) {
            LOG_PRINT("%02X ", kvr_data[idx]);
        }
        LOG_PRINT("\n");
    }

    LOG_PRINT("\n2.5> Save user KVR -------------- \n");
    kvr_size = 13;
    for (int idx = 0; idx < kvr_size; idx++) {
        kvr_data[idx] = kvr_size;
    }
    ret = KVR_Save(kvr_id, kvr_data, kvr_size);
    if (ret != kvr_size) {
        LOG_PRINT("save kvr fail (%d)\n", ret);
    } else {
        LOG_PRINT("save kvr success\n");
    }

    LOG_PRINT("\n2.6> Load user KVR ------------- \n");
    for (int idx = 0; idx < KVR_TEST_BUFF_SIZE; idx++) {
        kvr_data[idx] = 0;
    }
    ret = KVR_Load(kvr_id, kvr_data, kvr_size);
    if (ret != kvr_size) {
        LOG_PRINT("load kvr fail (%d)\n", ret);
    } else {
        LOG_PRINT("load kvr success\n");
        for (int idx = 0; idx < kvr_size; idx++) {
            LOG_PRINT("%02X ", kvr_data[idx]);
        }
        LOG_PRINT("\n");
    }


    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
