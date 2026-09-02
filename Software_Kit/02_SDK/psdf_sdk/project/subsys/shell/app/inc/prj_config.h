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
#define CONFIG_BOARD_MRS6130_P1812                          1
//#define CONFIG_BOARD_MRS6240_P2512_CPUF                     1

#define CONFIG_HEAP_SIZE                                    (1024 * 120)

#define CONFIG_SHELL                                        1
#define CONFIG_SHELL_UART_NUM                               0

#define CONFIG_SHELL_CMD_SYS                                1
#define CONFIG_SHELL_CMD_KERNEL                             1
#define CONFIG_SHELL_CMD_VERSION                            1
#define CONFIG_SHELL_CMD_HEAP                               1
#define CONFIG_SHELL_CMD_GPIO                               1
#define CONFIG_SHELL_CMD_WKIO                               1
#define CONFIG_SHELL_CMD_WDG                                1
#define CONFIG_SHELL_CMD_KVF                                1
#define CONFIG_SHELL_CMD_MEM                                1
#define CONFIG_SHELL_CMD_CLK                                1
#define CONFIG_CLOCK_MCO_ENABEL                             1
#define CONFIG_SHELL_CMD_FLASH                              1
#define CONFIG_SHELL_CMD_DMA                                1
#define CONFIG_SHELL_CMD_UART                               1
#define CONFIG_SHELL_CMD_SPI                                1
#define CONFIG_DRIVER_SPI0                                  1
#define CONFIG_SHELL_CMD_I2C                                1
#define CONFIG_DRIVER_I2C0                                  1
#define CONFIG_I2C_MASTER                                   1
#define CONFIG_I2C_SLAVE                                    1
#define CONFIG_I2C_TRANS_IT_ENABLE                          1
#define CONFIG_SHELL_CMD_PM                                 0

#define CONFIG_PM                                           0
#define CONFIG_TEST_WKIO                                    0
#if (CONFIG_PM == 1)
#if CONFIG_TEST_WKIO
#define CONFIG_SHELL_PM                                     0
#define CONFIG_UART_WUP_ENABLED                             0
#else
#define CONFIG_SHELL_PM                                     1
#define CONFIG_UART_WUP_ENABLED                             1
#endif
#endif

#define CONFIG_PRINTF_EARLY                                 1
#define CONFIG_BOARD_LOG_LEVEL                              4
#define CONFIG_CLOCK_LOG_LEVEL                              4
#define CONFIG_CLOCK_CALIB_LOG_LEVEL                        4

// #define CONFIG_FLASH_LOG_LEVEL                              4
// #define CONFIG_BOOT_LOG                                     1

/* 0 ~ 255 */
#define CONFIG_PROJECT_VERSION_MAJOR                        1
/* 0 ~ 255 */
#define CONFIG_PROJECT_VERSION_MINOR                        0
/* 0 ~ 65535 */
#define CONFIG_PROJECT_VERSION_REVISION                     0
/* commit -id */
#define CONFIG_PROJECT_VERSION_BUILD                        0




#endif /* _PRJ_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
