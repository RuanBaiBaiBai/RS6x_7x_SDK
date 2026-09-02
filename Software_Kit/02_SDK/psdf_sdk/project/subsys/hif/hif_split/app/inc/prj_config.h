/**
 **************************************************************************************************
 * @brief   project config define.
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
 **************************************************************************************************
 */


#ifndef _PRJ_CONFIG_H
#define _PRJ_CONFIG_H

/*
 * The board supported by the SDK,
 * please refer to platfromr/boards/board_config.h
 */

#define CONFIG_PRINTF                               1
#define CONFIG_PRINTF_UART_NUM                      0
#define CONFIG_PRINTF_EARLY_UART_NUM                0


#define CONFIG_XIP                                  1

#define CONFIG_HIF                                  1
#define CONFIG_HIF_PHY_UART_DEF_NUM                 1

#define CONFIG_HIF_RETRY_ENA                        0
#define CONFIG_HIF_TL_ACK_TIMER_ENABLE              0
#define CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE          0

#define CONFIG_LOG_IO_TAG_MASK                      0x02000000
#define CONFIG_LOG_IO_MASK                          ((1U << (11)) | (1U << (4)))
#define CONFIG_LOG_IO_TAG_MASK_HIF_SPLIT            CONFIG_LOG_IO_TAG_MASK

#define CONFIG_HIF_ML_MSG_TIMEOUT_MS_DEFAULT        1000U
#define CONFIG_HIF_ML_ACK_TIMEOUT_MS                1000U

#define HIF_TEST_COM_TYPE                           3                           // 1 - uart; 2 - iic; 3 - spi

#define HIF_TEST_SPLIT_ENA                          CONFIG_HIF_SPLIT_ENA

#define HIF_TEST_MsgReport                          (1U << (0))                 // HIF_MsgReport()
#define HIF_TEST_ListStart                          (1U << (1))                 // HIF_MsgReport_ListStart()
#define HIF_TEST_SEND_API                           (HIF_TEST_MsgReport | HIF_TEST_ListStart)

#define HIF_TEST_BLOCK_ENA                          0                           // 0 - unblocked sending; !0 - blocked sending

#define HIF_TEST_MsgReport_ID                       0xC6                        // Message ID
#define HIF_TEST_ListStart_ID                       0xC6

#define HIF_TEST_SEND_PERIOD_MS                     1000U


#endif /* _PRJ_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
