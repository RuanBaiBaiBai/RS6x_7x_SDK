/**
 **************************************************************************************************
 * @brief
 * Radar framework function configurations file.
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


#ifndef __RADAR_FRAMEWORK_SOC_CONFIG_H
#define __RADAR_FRAMEWORK_SOC_CONFIG_H

#define CONFIG_MMW_CTRL                                     1
#define CONFIG_MMW_DRIVER                                   1
#define CONFIG_MMW_CALIB_DATA_LOAD                          1 /* load ant calibration data from flash */

/**
 * Monitor Config
 */
/*
 * @brief Once enable CONFIG_MMW_DRIVER_TEMP_MGMT, radar will do re-calibration automatilly when temperatur varies over internal threshold.
*/
#ifndef CONFIG_MMW_DRIVER_TEMP_MGMT
#define CONFIG_MMW_DRIVER_TEMP_MGMT                         0
#endif

/**
  * @brief Once enable CONFIG_MMW_LOOP_TASK_MONITOR, radar firmware will run a extra software-based monitor, if loop task monitor is not triggerd
  * 	   in CONFIG_MMW_LOOP_TASK_TIMEOUT_MS, loop task monitor will reset radar.
*/
#ifndef CONFIG_MMW_LOOP_TASK_MONITOR
#define CONFIG_MMW_LOOP_TASK_MONITOR                        0
#endif

/**
  * @brief Once enable CONFIG_MMW_WATCHDOG, radar will enable hardware watchdog timer, default timeout is 5s.
*/
#ifndef CONFIG_MMW_WATCHDOG
#define CONFIG_MMW_WATCHDOG                                 0
#endif
#if CONFIG_MMW_WATCHDOG
#define CONFIG_WDG_TIMEOUT_MS                               5000
#endif

/* Enable factory test feature.
 * Use ProductTestingAnalysisTool to test factory function.
 * See 'ProductTestingAnalysisTool' user manual for details.
 * */
#ifndef CONFIG_RADAR_FRAMEWORK_FT
#define CONFIG_RADAR_FRAMEWORK_FT                          (0)
#endif

/* power mange config
 */
/**
  * @brief If enable 'CONFIG_PM', after CPU finished processm, CPU will shut down and consumes a current of 10uA between frames.
  */
#ifndef CONFIG_PM
#define CONFIG_PM                                           0
#endif
#define CONFIG_PMU_EXTLDO_SLEEP_SW_SUBMODE                  1

/*
 * Port compliance check and adaptation
 * */
/* printf config decide whether can use printf() function to print functions via uart.
 * MRS6130-P1806 will use uart0 as HIF and uart1 as printf com port,
 * MRS6130-P1812/MRS6240-P2512 will use uart1 as printf & shell, SPI or UART0 as HIF
 */
#define CONFIG_PRINTF                                       1
#define CONFIG_PRINTF_UART_BAUDRATE                         115200

#if (CONFIG_PRINTF != 0)
    #if (CONFIG_HIF_PHY_TYPE == 1) /* hif in uart mode, uart 0 is used for hif, only uart1 can used for printf */
        #define CONFIG_PRINTF_UART_NUM                      1  /* uart 1 */
    #else /* spi or i2c mode, only uart0 is supported for printf() */
        #define CONFIG_PRINTF_UART_NUM                      0  /* uart 0 */
    #endif
#endif /* CONFIG_PRINTF */

#endif /* __RADAR_FRAMEWORK_SOC_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
