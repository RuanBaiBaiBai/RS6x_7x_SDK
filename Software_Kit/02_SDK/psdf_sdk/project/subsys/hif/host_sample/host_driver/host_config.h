/**
 **************************************************************************************************
 * @file    host_config.h
 * @brief   host config files.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_CONFIG_H_
#define __HOST_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "user_config.h"


/* Top-Level Configurations.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _ALIGNAS_VARIABLE
#define _ALIGNAS_VARIABLE                                                 _Alignas(32)
#endif

#ifndef _WEAK_FUNCTION
#define _WEAK_FUNCTION                                                    __attribute__((weak))
#endif

#ifndef HOST_OS_TIMEOUT_FOREVER
#define HOST_OS_TIMEOUT_FOREVER                                           ((host_os_timeout_t)0xFFFFFFFF)
#endif

#ifndef HOST_OS_TIMEOUT_NO_WAIT
#define HOST_OS_TIMEOUT_NO_WAIT                                           ((host_os_timeout_t)0)
#endif

#ifndef HOST_OS_THREAD_PRIO_NORMAL
#define HOST_OS_THREAD_PRIO_NORMAL                                        (7)
#endif

#ifndef HOST_OS_THREAD_PRIO_LOW
#define HOST_OS_THREAD_PRIO_LOW                                           ((HOST_OS_THREAD_PRIO_NORMAL + 1)/2)
#endif

#ifndef HOST_OS_THREAD_PRIO_HIGH
#define HOST_OS_THREAD_PRIO_HIGH                                          (HOST_OS_THREAD_PRIO_NORMAL + HOST_OS_THREAD_PRIO_LOW)
#endif

#ifndef CFG_HOST_PORT_LOG_EN
#define CFG_HOST_PORT_LOG_EN                                              1
#endif

#ifndef CFG_HOST_PORT_LOG_FILE
#define CFG_HOST_PORT_LOG_FILE                                            0
#endif

#ifndef CFG_HOST_LOG_LEVEL
#define CFG_HOST_LOG_LEVEL                                                HOST_LOG_LEVEL_INF
#endif

#ifndef CFG_HOST_PORT_STORE_EN
#define CFG_HOST_PORT_STORE_EN                                            1
#endif

#ifndef CFG_HOST_PORT_COM_EN
#define CFG_HOST_PORT_COM_EN                                              1
#endif

#if (CFG_HOST_PORT_COM_EN)
#ifndef CFG_HOST_PORT_COM_SPI_EN
#define CFG_HOST_PORT_COM_SPI_EN                                          1
#endif
#ifndef CFG_HOST_PORT_COM_I2C_EN
#define CFG_HOST_PORT_COM_I2C_EN                                          1
#endif
#ifndef CFG_HOST_PORT_COM_UART_EN
#define CFG_HOST_PORT_COM_UART_EN                                         1
#endif
#endif

#ifndef CFG_HOST_PORT_OS_EN
#define CFG_HOST_PORT_OS_EN                                               1
#endif

#ifndef CFG_HOST_PORT_PM_EN
#define CFG_HOST_PORT_PM_EN                                               0
#endif

#if (!CFG_HOST_PORT_OS_EN)
#if (CFG_HOST_PORT_PM_EN)
#error if enable CFG_HOST_PORT_PM_EN, must enable CFG_HOST_PORT_OS_EN
#endif
#endif

#ifndef CFG_HOST_PORT_IO_EN
#define CFG_HOST_PORT_IO_EN                                               1
#endif

#ifndef CFG_HOST_DRIVER_SPI_SPEED_IN_DOWNLOAD
#define CFG_HOST_DRIVER_SPI_SPEED_IN_DOWNLOAD                             8000000
#endif

// spi speed must < 8M when download
#if (CFG_HOST_DRIVER_SPI_SPEED_IN_DOWNLOAD > 8000000)
#error spi speed must < 8M when download
#endif


/* Protocol-Level Configurations.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef CFG_TL_RETRY_EN
#define CFG_TL_RETRY_EN                                                   1
#endif

#ifndef CFG_MSG_FRAGMENT_EN
#define CFG_MSG_FRAGMENT_EN                                               1
#endif

#ifndef CFG_MSG_FRAGMENT_RETRY_EN
#define CFG_MSG_FRAGMENT_RETRY_EN                                         1
#endif

#ifndef CFG_APP_RETRY_EN
#define CFG_APP_RETRY_EN                                                  1
#endif

#if ((CFG_MSG_FRAGMENT_EN == 0) && (CFG_MSG_FRAGMENT_RETRY_EN != 0))
#error if open fragment retry, must open fragment first!
#endif

#if ((CFG_TL_RETRY_EN != 0) && (CFG_MSG_FRAGMENT_EN != 0) && (CFG_MSG_FRAGMENT_RETRY_EN == 0))
    #undef CFG_MSG_FRAGMENT_RETRY_EN
    #define CFG_MSG_FRAGMENT_RETRY_EN                                     1
#endif

#define HOST_HIF_POLL_ACK_TLV_IS_VALID                                    ((CFG_MSG_FRAGMENT_RETRY_EN) || (CFG_APP_RETRY_EN))

#ifndef CFG_LLC_DEVICE_RX_CACHE_SIZE
#define CFG_LLC_DEVICE_RX_CACHE_SIZE                                      (128)
#endif

#ifndef SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT
#define SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT            1
#endif

#ifndef CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR
#define CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR             8
#endif

#if (CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR < 8)
#error abnormal fragment burst number may be too less...
#endif

#ifndef SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT_TIMELY
#define SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT_TIMELY     0
#endif

// ABNORMAL_FRAGMENT_INFO_LOG is in critical!!!
#ifndef ABNORMAL_FRAGMENT_INFO_LOG
#define ABNORMAL_FRAGMENT_INFO_LOG(report_hdl, frag_info, abnormal_burst_num_max) \
    HOST_LOG_WRN("MsgID=%02X Fraginfo %p(flow%u) triggered abnormal detection, abnormal burst number is %u>=%u\n", \
                 report_hdl->msg_id, frag_info, frag_info->flow_seq, \
                 frag_info->abnormal_burst_num, abnormal_burst_num_max)
#endif

// -1 is no limit, 0 is not keep, >0 is keep limited number of idle fragment info
#ifndef CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX
#define CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX                           8
#endif

#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX < -1)
#error CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX must be -1, 0, or a positive interger
#endif

// 0 is no limit, >0 is the max of hold buffer size
#ifndef CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX
#define CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX                        (16 * 1024)
#endif

#if (CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX < 0)
#error CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX must >= 0
#endif

#ifndef CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN
#define CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN                              0  // only for uart
#endif

#ifndef CFG_HOST_BURN_FLASH_EN
#define CFG_HOST_BURN_FLASH_EN                                            1
#endif

#ifndef CFG_HOST_BURN_FLASH_READ_EN
#define CFG_HOST_BURN_FLASH_READ_EN                                       0
#endif

#if (CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN)
#if (!CFG_HOST_BURN_FLASH_READ_EN)
#error if enable CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN, must enable CFG_HOST_BURN_FLASH_READ_EN
#endif
#endif

#ifndef CFG_HOST_BURN_SRAM_EN
#define CFG_HOST_BURN_SRAM_EN                                             0
#endif

#ifndef CFG_HOST_BURN_RUN_EN
#define CFG_HOST_BURN_RUN_EN                                              0
#endif

#ifndef CFG_HOST_BURN_RETRY_CNT
#define CFG_HOST_BURN_RETRY_CNT                                           3
#endif

#ifndef CFG_HOST_BURN_RSP_RETRY_CNT
#define CFG_HOST_BURN_RSP_RETRY_CNT                                       3
#endif

#ifndef CFG_HOST_BURN_CHECKSUM_EN
#define CFG_HOST_BURN_CHECKSUM_EN                                         1
#endif

#ifndef CFG_HOST_LLC_DEVICE_CLOSE_WAIT_TIME_MS
#define CFG_HOST_LLC_DEVICE_CLOSE_WAIT_TIME_MS                            5000
#endif


/* APP-Level Configurations.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef HOST_DEVICE_STATE_INFO
#define HOST_DEVICE_STATE_INFO                                            1
#endif

#ifndef HOST_HIF_CASE_GENERAL_EN
#define HOST_HIF_CASE_GENERAL_EN                                          1
#endif

#ifndef HOST_HIF_CASE_RADAR_ANALYSIS_EN
#define HOST_HIF_CASE_RADAR_ANALYSIS_EN                                   1
#endif


#ifdef __cplusplus
}
#endif

#endif /* __HOST_CONFIG_H_ */

