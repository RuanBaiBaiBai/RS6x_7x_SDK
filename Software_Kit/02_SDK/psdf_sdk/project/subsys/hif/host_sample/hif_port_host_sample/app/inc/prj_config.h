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
#define CONFIG_BOARD_MRS6240_P2512_CPUF                          1

#define CONFIG_HEAP_SIZE                                    (120 * 1024)

#define CONFIG_PRINTF_UART_NUM								0
#define CONFIG_PRINTF_EARLY_UART_NUM						0

#define CONFIG_DRIVER_SPI0                                  1

#define CONFIG_DRIVER_I2C0                                  1
#define CONFIG_I2C_MASTER                                   1

#define CONFIG_KERNEL_DEBUG                                 1

#define CONFIG_SHELL										1
#define CONFIG_SHELL_CMD_KERNEL                             1
#define CONFIG_SHELL_CMD_HEAP                               1

#define CONFIG_XIP											1

#ifndef HIF_HOST_TEST
#define HIF_HOST_TEST                                       0
#endif

#ifndef HIF_HOST_TEST_UART
#define HIF_HOST_TEST_UART                                  0
#endif

#ifndef HIF_HOST_TEST_I2C
#define HIF_HOST_TEST_I2C                                   0
#endif

#ifndef HIF_HOST_TEST_SPI
#define HIF_HOST_TEST_SPI                                   0
#endif

#ifndef HIF_HOST_TEST_SPI_DUAL
#define HIF_HOST_TEST_SPI_DUAL                              0
#endif

#ifndef HIF_HOST_TEST_SPI_QUAD
#define HIF_HOST_TEST_SPI_QUAD                              0
#endif

#ifndef HIF_HOST_TEST_UART_SPEED
#define HIF_HOST_TEST_UART_SPEED                            1000000
#endif

#ifndef HIF_HOST_TEST_I2C_SPEED
#define HIF_HOST_TEST_I2C_SPEED                             400000
#endif

#ifndef HIF_HOST_TEST_SPI_SPEED
#define HIF_HOST_TEST_SPI_SPEED                             64000000
#endif

#ifndef HIF_HOST_TEST_DSPI_SPEED
#define HIF_HOST_TEST_DSPI_SPEED                            64000000
#endif

#ifndef HIF_HOST_TEST_QSPI_SPEED
#define HIF_HOST_TEST_QSPI_SPEED                            32000000
#endif

#ifndef HIF_HOST_TEST_DEV0_UART_SPEED
#define HIF_HOST_TEST_DEV0_UART_SPEED                       1000000
#endif

#ifndef HIF_HOST_TEST_DEV1_UART_SPEED
#define HIF_HOST_TEST_DEV1_UART_SPEED                       1000000
#endif

#ifndef HIF_HOST_TEST_DEV0_SPI_SPEED
#define HIF_HOST_TEST_DEV0_SPI_SPEED                        20000000
#endif

#ifndef HIF_HOST_TEST_DEV1_SPI_SPEED
#define HIF_HOST_TEST_DEV1_SPI_SPEED                        20000000
#endif

#ifndef HIF_HOST_TEST_DEV0_DSPI_SPEED
#define HIF_HOST_TEST_DEV0_DSPI_SPEED                       20000000
#endif

#ifndef HIF_HOST_TEST_DEV1_DSPI_SPEED
#define HIF_HOST_TEST_DEV1_DSPI_SPEED                       20000000
#endif

#ifndef HIF_HOST_TEST_DEV0_QSPI_SPEED
#define HIF_HOST_TEST_DEV0_QSPI_SPEED                       4000000
#endif

#ifndef HIF_HOST_TEST_DEV1_QSPI_SPEED
#define HIF_HOST_TEST_DEV1_QSPI_SPEED                       4000000
#endif

#ifndef HIF_HOST_TEST_MULTI_SLV
#define HIF_HOST_TEST_MULTI_SLV                             0
#endif

#ifndef HIF_HOST_TEST_TL_RETRY_EN
#define HIF_HOST_TEST_TL_RETRY_EN                           0
#endif

#ifndef HIF_HOST_TEST_FRAG_EN
#define HIF_HOST_TEST_FRAG_EN                               0
#endif

#ifndef HIF_HOST_TEST_FRAG_RETRY_EN
#define HIF_HOST_TEST_FRAG_RETRY_EN                         0
#endif

#ifndef HIF_HOST_TEST_APP_RETRY_EN
#define HIF_HOST_TEST_APP_RETRY_EN                          0
#endif

#ifndef HIF_HOST_TEST_MSG_ID_C1_EN
#define HIF_HOST_TEST_MSG_ID_C1_EN                          1
#endif

#ifndef HIF_HOST_TEST_MSG_ID_C2_EN
#define HIF_HOST_TEST_MSG_ID_C2_EN                          1
#endif

#ifndef HIF_HOST_TEST_MSG_ID_C3_EN
#define HIF_HOST_TEST_MSG_ID_C3_EN                          1
#endif

#ifndef HIF_HOST_TEST_MSG_ID_C6_EN
#define HIF_HOST_TEST_MSG_ID_C6_EN                          1
#endif

#define CFG_TL_RETRY_EN                                     HIF_HOST_TEST_TL_RETRY_EN
#define CFG_MSG_FRAGMENT_EN                                 HIF_HOST_TEST_FRAG_EN
#define CFG_MSG_FRAGMENT_RETRY_EN                           HIF_HOST_TEST_FRAG_RETRY_EN
#define CFG_APP_RETRY_EN                                    HIF_HOST_TEST_APP_RETRY_EN


//#define CONFIG_DEBUG_SIZE_LEVEL                   2

#endif /* _PRJ_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
