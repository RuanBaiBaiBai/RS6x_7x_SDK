/**
 **************************************************************************************************
 * @file    host_port.h
 * @brief   host port define.
 * @attention
 *          Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_PORT_H_
#define __HOST_PORT_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_config.h"
#include "../protocol/utils/host_types.h"

#include "include/hal_log.h"
#if (CFG_HOST_PORT_STORE_EN == 1)
#include "include/hal_store.h"
#endif
#if (CFG_HOST_PORT_COM_EN == 1)
#include "include/hal_com.h"
#endif
#if (CFG_HOST_PORT_OS_EN == 1)
#include "include/hal_os.h"
#endif
#if (CFG_HOST_PORT_IO_EN == 1)
#include "include/hal_io.h"
#endif


/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */


#ifdef __cplusplus
}
#endif

#endif /* __HOST_PORT_H_ */

