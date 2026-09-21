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
#include "kvf.h"

#include "log.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */

/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
#define KVF_TEST_BUFF_SIZE                  32
#define KVF_TEST_ID                         3

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
    uint8_t testBuff[KVF_TEST_BUFF_SIZE];
    uint32_t testLen = 13;
    uint32_t testSeq = 0;

    LOG_PRINT("Key-Value flash Demo\n");
    LOG_PRINT("--------------------------\n");


    do {
        printf("\nload\n");
        ret = KVF_Load(KVF_TEST_ID, testBuff, testLen);
        if (ret < 0) {
            printf("load kvf fail (%d)\n", ret);
        } else if (ret != testLen) {
            printf("load kvf len error (%d)\n", ret);
        } else {
            printf("load data success: %d\n", testLen);
            for (uint32_t idx = 0; idx < testLen; idx++) {
                printf("%02X ", testBuff[idx]);
            }
        }

        testSeq = testBuff[0] + 0x10;

        printf("\nsave\n");
        for (uint32_t idx = 0; idx < testLen; idx++) {
            testBuff[idx] = idx + testSeq;
        }
        ret = KVF_Save(KVF_TEST_ID, testBuff, testLen);
        if (ret != testLen) {
            printf("save kvf fail (%d)\n", ret);
        } else {
            printf("save kvf success\n");
        }

        printf("\ndemo done\n");
    } while (0);

    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

