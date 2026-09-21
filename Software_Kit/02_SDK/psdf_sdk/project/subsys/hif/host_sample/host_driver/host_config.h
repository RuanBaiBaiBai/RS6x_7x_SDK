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
// Align variable as N bytes.
// If certain peripherals require N bytes aligned memory addr,
// the macro should be modified.
#ifndef _ALIGNAS_VARIABLE
#define _ALIGNAS_VARIABLE                                                 _Alignas(32)
#endif

// Weak function.
#ifndef _WEAK_FUNCTION
#define _WEAK_FUNCTION                                                    __attribute__((weak))
#endif

// Waiting forever for hal_os.
#ifndef HOST_OS_TIMEOUT_FOREVER
#define HOST_OS_TIMEOUT_FOREVER                                           ((host_os_timeout_t)0xFFFFFFFF)
#endif

// Do not wait.
#ifndef HOST_OS_TIMEOUT_NO_WAIT
#define HOST_OS_TIMEOUT_NO_WAIT                                           ((host_os_timeout_t)0)
#endif

// priority: high > normal > low

// Normal priority for thread.
#ifndef HOST_OS_THREAD_PRIO_NORMAL
#define HOST_OS_THREAD_PRIO_NORMAL                                        (7)
#endif

// Low priority for thread.
#ifndef HOST_OS_THREAD_PRIO_LOW
#define HOST_OS_THREAD_PRIO_LOW                                           ((HOST_OS_THREAD_PRIO_NORMAL + 1)/2)
#endif

// High priority for thread.
#ifndef HOST_OS_THREAD_PRIO_HIGH
#define HOST_OS_THREAD_PRIO_HIGH                                          (HOST_OS_THREAD_PRIO_NORMAL + HOST_OS_THREAD_PRIO_LOW)
#endif

// Enable LOG.
#ifndef CFG_HOST_PORT_LOG_EN
#define CFG_HOST_PORT_LOG_EN                                              1
#endif

// The relative position between the LOG interface file and
// the hal_log.h file.
#ifndef CFG_HOST_PORT_LOG_FILE
#define CFG_HOST_PORT_LOG_FILE                                            0
#endif

// LOG level:
// HOST_LOG_LEVEL_NULL
// HOST_LOG_LEVEL_ERR
// HOST_LOG_LEVEL_WRN
// HOST_LOG_LEVEL_INF
// HOST_LOG_LEVEL_DBG
#ifndef CFG_HOST_LOG_LEVEL
#define CFG_HOST_LOG_LEVEL                                                HOST_LOG_LEVEL_INF
#endif

// Enable store/filesystem.
#ifndef CFG_HOST_PORT_STORE_EN
#define CFG_HOST_PORT_STORE_EN                                            1
#endif

// Enable COM.
#ifndef CFG_HOST_PORT_COM_EN
#define CFG_HOST_PORT_COM_EN                                              1
#endif

#if (CFG_HOST_PORT_COM_EN)
// Enable SPI COM.
#ifndef CFG_HOST_PORT_COM_SPI_EN
#define CFG_HOST_PORT_COM_SPI_EN                                          1
#endif
// Enable I2C COM.
#ifndef CFG_HOST_PORT_COM_I2C_EN
#define CFG_HOST_PORT_COM_I2C_EN                                          1
#endif
// Enable UART COM.
#ifndef CFG_HOST_PORT_COM_UART_EN
#define CFG_HOST_PORT_COM_UART_EN                                         1
#endif
#endif

// Enable OS.
#ifndef CFG_HOST_PORT_OS_EN
#define CFG_HOST_PORT_OS_EN                                               1
#endif

// Enable sleep.
#ifndef CFG_HOST_PORT_PM_EN
#define CFG_HOST_PORT_PM_EN                                               0
#endif

#if (!CFG_HOST_PORT_OS_EN)
#if (CFG_HOST_PORT_PM_EN)
#error if enable CFG_HOST_PORT_PM_EN, must enable CFG_HOST_PORT_OS_EN
#endif
#endif

// Enable IO.
#ifndef CFG_HOST_PORT_IO_EN
#define CFG_HOST_PORT_IO_EN                                               1
#endif

// SPI speed when download.
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
// Enable HIF TL layer retry.
#ifndef CFG_TL_RETRY_EN
#define CFG_TL_RETRY_EN                                                   1
#endif

// Enable HIF MSG layer message fragmentation.
#ifndef CFG_MSG_FRAGMENT_EN
#define CFG_MSG_FRAGMENT_EN                                               1
#endif

// Enable HIF MSG layer message fragmentation retry.
#ifndef CFG_MSG_FRAGMENT_RETRY_EN
#define CFG_MSG_FRAGMENT_RETRY_EN                                         1
#endif

// Enable HIF APP layer retry(some msg id support).
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

// Do not modify this macro!
#define HOST_HIF_POLL_ACK_TLV_IS_VALID                                    ((CFG_MSG_FRAGMENT_RETRY_EN) || (CFG_APP_RETRY_EN))

// The rx_cache size, unit is byte.
// If notify_type is LLC_NOTIFY_TYPE_COM_ISR or LLC_NOTIFY_TYPE_COM_IRQ_THREAD,
// will use rx_cache to cache data.
#ifndef CFG_LLC_DEVICE_RX_CACHE_SIZE
#define CFG_LLC_DEVICE_RX_CACHE_SIZE                                      (128)
#endif

// Enable MSG layer message fragmentation abnormal detection.
#ifndef SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT
#define SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT            1
#endif

// The MSG layer message fragmentation abnormal detection thr.
// If N consecutive data packets are not from the currently unrecved fragmentation
// stream, will be abandoned.
#ifndef CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR
#define CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR             8
#endif

#if (CFG_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_BURST_THR < 8)
#error abnormal fragment burst number may be too less...
#endif

// Enable check abnormal fragmentation stream when recving data.
// 0  -->  check when processing,
// 1  -->  check when recving.
// Suggest check when processing, which means defining the macro as 0.
#ifndef SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT_TIMELY
#define SUPPORT_HOST_HIF_FRAGMENT_PENDING_INFO_ABNORMAL_DETECT_TIMELY     0
#endif

// Called before free abnormal fragmentation stream.
#ifndef ABNORMAL_FRAGMENT_INFO_LOG
#define ABNORMAL_FRAGMENT_INFO_LOG(report_hdl, frag_info, abnormal_burst_num_max) \
    HOST_LOG_WRN("MsgID=%02X Fraginfo %p(flow%u) triggered abnormal detection, abnormal burst number is %u>=%u\n", \
                 report_hdl->msg_id, frag_info, frag_info->flow_seq, \
                 frag_info->abnormal_burst_num, abnormal_burst_num_max)
#endif

// The maximum number of IDLE fragmentation stream handle which will be keep.
// -1 is no limit, 0 is not keep, >0 is keep limited number of idle fragment info
#ifndef CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX
#define CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX                           8
#endif

#if (CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX < -1)
#error CFG_HOST_HIF_FRAGMENT_FREE_INFO_NUM_MAX must be -1, 0, or a positive interger
#endif

// The maximum size of fragmentation stream buffer which will be keep, unit is byte.
// 0 is no limit, >0 is the max of hold buffer size
#ifndef CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX
#define CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX                        (16 * 1024)
#endif

#if (CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX < 0)
#error CFG_HOST_HIF_FRAGMENT_HOLD_BUFFER_SIZE_MAX must >= 0
#endif

// Enable read check verification when download image.
// Only UART support it.
#ifndef CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN
#define CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN                              0  // only for uart
#endif

// Enable burn flash.
#ifndef CFG_HOST_BURN_FLASH_EN
#define CFG_HOST_BURN_FLASH_EN                                            1
#endif

// Enable burn read flash.
#ifndef CFG_HOST_BURN_FLASH_READ_EN
#define CFG_HOST_BURN_FLASH_READ_EN                                       0
#endif

#if (CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN)
#if (!CFG_HOST_BURN_FLASH_READ_EN)
#error if enable CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN, must enable CFG_HOST_BURN_FLASH_READ_EN
#endif
#endif

// Enable burn sram.
#ifndef CFG_HOST_BURN_SRAM_EN
#define CFG_HOST_BURN_SRAM_EN                                             0
#endif

// Enable set PC value when burn mode.
#ifndef CFG_HOST_BURN_RUN_EN
#define CFG_HOST_BURN_RUN_EN                                              0
#endif

// The maximum retry times when send the command of burn mode.
#ifndef CFG_HOST_BURN_RETRY_CNT
#define CFG_HOST_BURN_RETRY_CNT                                           3
#endif

// The maximum retry times when recv the response of burn mode.
#ifndef CFG_HOST_BURN_RSP_RETRY_CNT
#define CFG_HOST_BURN_RSP_RETRY_CNT                                       3
#endif

// Enable check checksum.
#ifndef CFG_HOST_BURN_CHECKSUM_EN
#define CFG_HOST_BURN_CHECKSUM_EN                                         1
#endif

// The max wait time when close LLC device.
#ifndef CFG_HOST_LLC_DEVICE_CLOSE_WAIT_TIME_MS
#define CFG_HOST_LLC_DEVICE_CLOSE_WAIT_TIME_MS                            5000
#endif


/* APP-Level Configurations.
 * ------------------------------------------------------------------------------------------------
 */
// Enable get device state info.
#ifndef HOST_DEVICE_STATE_INFO
#define HOST_DEVICE_STATE_INFO                                            1
#endif

// Enable mmwave general command.
#ifndef HOST_HIF_CASE_GENERAL_EN
#define HOST_HIF_CASE_GENERAL_EN                                          1
#endif

// Enable mmwave radar_analysis command.
#ifndef HOST_HIF_CASE_RADAR_ANALYSIS_EN
#define HOST_HIF_CASE_RADAR_ANALYSIS_EN                                   1
#endif


/* TL-Level Configurations.
 * ------------------------------------------------------------------------------------------------
 */
// The stack size of TL layer main task, unit is byte.
#ifndef HIF_TL_TASK_STACK_SIZE
#define HIF_TL_TASK_STACK_SIZE                                            1024
#endif

// The stack size of TL layer process task, unit is byte.
#ifndef HIF_TL_REPORT_TASK_STACK_SIZE
#define HIF_TL_REPORT_TASK_STACK_SIZE                                     2048
#endif

// The default max num of report queue.
// The number must be 2^N.
#ifndef HIF_TL_REPORT_QUEUE_MAX_DEF
#define HIF_TL_REPORT_QUEUE_MAX_DEF                                       16
#endif


#ifdef __cplusplus
}
#endif

#endif /* __HOST_CONFIG_H_ */

