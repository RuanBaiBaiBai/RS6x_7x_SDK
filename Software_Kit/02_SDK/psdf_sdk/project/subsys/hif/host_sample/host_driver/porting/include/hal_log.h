/**
 **************************************************************************************************
 * @file    hal_log.h
 * @brief   HAL log define.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HAL_LOG_H_
#define _HAL_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"

#include CFG_HOST_PORT_LOG_FILE


/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef HOST_LOG_LEVEL_NULL
#define HOST_LOG_LEVEL_NULL                          0
#endif

#ifndef HOST_LOG_LEVEL_ERR
#define HOST_LOG_LEVEL_ERR                           1
#endif

#ifndef HOST_LOG_LEVEL_WRN
#define HOST_LOG_LEVEL_WRN                           2
#endif

#ifndef HOST_LOG_LEVEL_INF
#define HOST_LOG_LEVEL_INF                           3
#endif

#ifndef HOST_LOG_LEVEL_DBG
#define HOST_LOG_LEVEL_DBG                           4
#endif

#ifndef CFG_HOST_LOG_LEVEL
#define CFG_HOST_LOG_LEVEL                                   HOST_LOG_LEVEL_DBG
#endif


#if (CFG_HOST_PORT_LOG_EN == 1)

#define HOST_LOG_PRINT(fmt, ...)                            host_log_print(fmt, ##__VA_ARGS__)

#define HOST_LOG_PRINT_HEX(data, len, str, ...)             \
    do {                                                    \
        HOST_LOG_PRINT(str, ##__VA_ARGS__);                 \
        unsigned char *pbuf = (unsigned char *)(data);      \
        HOST_LOG_PRINT(" (%d)\n", (len));                   \
        for (uint32_t idx = 0; idx < (len); idx++) {        \
            HOST_LOG_PRINT("%02X ", pbuf[idx]);             \
            if ((idx % 16) == 15) {                         \
                HOST_LOG_PRINT("\n");                       \
            }                                               \
        }                                                   \
        HOST_LOG_PRINT("\n");                               \
    } while (0)

#else

#define HOST_LOG_PRINT(fmt, ...)
#define HOST_LOG_PRINT_HEX(data, len, str)

#endif /* (CFG_HOST_LOG_EN == 1) */


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_LOG_LEVEL >= HOST_LOG_LEVEL_ERR) && (CFG_HOST_PORT_LOG_EN == 1))
#define HOST_LOG_ERR(fmt, ...)                    HOST_LOG_PRINT("\033[31m[E]" fmt "\033[0m", ##__VA_ARGS__)
#define HOST_LOG_ERR_HEX(data, len, str, ...)     HOST_LOG_PRINT_HEX(data, len, "\033[31m[E]" str "\033[0m", ##__VA_ARGS__)
#else
#define HOST_LOG_ERR(fmt, ...)
#define HOST_LOG_ERR_HEX(data, len, str, ...)
#endif


#if ((CFG_HOST_LOG_LEVEL >= HOST_LOG_LEVEL_WRN) && (CFG_HOST_PORT_LOG_EN == 1))
#define HOST_LOG_WRN(fmt, ...)                    HOST_LOG_PRINT("\033[33m[W]" fmt "\033[0m", ##__VA_ARGS__)
#define HOST_LOG_WRN_HEX(data, len, str, ...)     HOST_LOG_PRINT_HEX(data, len, "\033[33m[W]" str "\033[0m", ##__VA_ARGS__)
#else
#define HOST_LOG_WRN(fmt, ...)
#define HOST_LOG_WRN_HEX(data, len, str, ...)
#endif


#if ((CFG_HOST_LOG_LEVEL >= HOST_LOG_LEVEL_INF) && (CFG_HOST_PORT_LOG_EN == 1))
#define HOST_LOG_INF(fmt, ...)                    HOST_LOG_PRINT("\033[32m[I]" fmt "\033[0m", ##__VA_ARGS__)
#define HOST_LOG_INF_HEX(data, len, str, ...)     HOST_LOG_PRINT_HEX(data, len, "\033[32m[I]" str "\033[0m", ##__VA_ARGS__)
#else
#define HOST_LOG_INF(fmt, ...)
#define HOST_LOG_INF_HEX(data, len, str, ...)
#endif


#if ((CFG_HOST_LOG_LEVEL >= HOST_LOG_LEVEL_DBG) && (CFG_HOST_PORT_LOG_EN == 1))
#define HOST_LOG_DBG(fmt, ...)                    HOST_LOG_PRINT("[D]" fmt, ##__VA_ARGS__)
#define HOST_LOG_DBG_HEX(data, len, str, ...)     HOST_LOG_PRINT_HEX(data, len, "[D]" str, ##__VA_ARGS__)
#else
#define HOST_LOG_DBG(fmt, ...)
#define HOST_LOG_DBG_HEX(data, len, str, ...)
#endif


/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_LOG_EN == 1)
/**
 * @brief Print log.
 *
 * @param `const char *` The string to be format.
 *
 * @param `...` The param to be format.
 *
 * @retval The number of bytes printed 
 */
int host_log_print(const char *, ...);
#endif  /* CFG_HOST_PORT_LOG_EN == 1 */


#ifdef __cplusplus
}
#endif

#endif /* _HAL_LOG_H_ */

