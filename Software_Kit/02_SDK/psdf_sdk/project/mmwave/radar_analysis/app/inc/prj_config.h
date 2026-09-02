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

#define CONFIG_MMW_PRESENCE_POINT_CLOUD						1

#define CONFIG_MMW_CTRL                                     1
#define CONFIG_MMW_CALIB_DATA_LOAD                          1  // load ant calib data from flash
#define CONFIG_MMW_DRIVER                                   1
#if (CONFIG_MMW_PRESENCE_POINT_CLOUD)

#define CONFIG_HEAP_SIZE                                (1024 * 175)
#endif

#define POINTCLOUD_SHOW_TIME_INFO                           1

/* Default specification 'Nomal' is used,
 * please refer to the hif_config.h filer for detailed configuration
 * */
#define CONFIG_HIF                                          1

#define CONFIG_HIF_APP_DATA_POOL							1

#define CONFIG_HIF_PHY_UART_DEF_NUM                         1

#define CONFIG_MMW_DRIVER                                   1
#define CONFIG_SHELL                                        1
#define CONFIG_SHELL_UART_NUM                               0
#define CONFIG_DRIVER_UART_0                                1
#define CONFIG_MMW_SHELL                                    1
#define CONFIG_SHELL_CMD_VERSION                            1
#define CONFIG_SHELL_CMD_HEAP                               1
// #define CONFIG_SHELL_CMD_FLASH                              1
#define CONFIG_SHELL_CMD_MEM                                1
#define CONFIG_SHELL_CMD_CLK                                1
#define CONFIG_SHELL_CMD_KERNEL                             1

//#define CONFIG_LOG_IO_TAG_MASK                            (0x1000)
//#define CONFIG_LOG_IO_MASK                                (0x800)

#define CONFIG_PM                                           0
#define CONFIG_PMU_EXTLDO_SLEEP_SW_SUBMODE                  1


#endif /* _PRJ_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
