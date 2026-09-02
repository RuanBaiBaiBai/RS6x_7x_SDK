/**
 **************************************************************************************************
 * @file    port_log.h
 * @brief   log define.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __PORT_LOG_H__
#define __PORT_LOG_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"

#if (CFG_HOST_PORT_LOG_EN == 1)
#include "log.h"
#endif  /* CFG_HOST_PORT_LOG_EN == 1 */


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
#define host_log_print(fmt, ...)                            printf(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __PORT_LOG_H__ */

