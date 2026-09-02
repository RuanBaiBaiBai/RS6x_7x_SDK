/**
 **************************************************************************************************
 * @file    hal_os.h
 * @brief   HAL os define.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HAL_OS_H_
#define _HAL_OS_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"


/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef unsigned long host_os_critical_flag_t;

typedef void * host_os_thread_handle_t;

typedef void (*host_os_thread_entry_t)(void *);

typedef void * host_os_sem_t;

typedef uint32_t host_os_timeout_t;

typedef uint32_t host_os_time_t;


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_OS_EN == 1)
host_os_critical_flag_t host_os_enter_critical(void);
void host_os_exit_critical(host_os_critical_flag_t);

host_os_time_t host_os_timestamp_us(void);

void * host_os_malloc(uint32_t size);
void   host_os_free(void *pmem);

void * host_os_memset(void *addr, uint8_t val, uint32_t size);
void * host_os_memcpy(void *dst, const void *src, uint32_t size);
int host_os_memcmp(const void *addr1, const void *addr2, uint32_t size);

host_os_thread_handle_t host_os_thread_create(const char *pname,
                                              uint32_t stack_size,
                                              host_os_thread_entry_t entry,
                                              void *arg, int32_t prio);
bool host_os_thread_is_valid(host_os_thread_handle_t thread_hdl);
int host_os_thread_delete(host_os_thread_handle_t thread_hdl);

host_os_sem_t host_os_sem_create(uint32_t initial_count, uint32_t limit);
int host_os_sem_delete(host_os_sem_t psem);
int host_os_sem_is_valid(host_os_sem_t psem);
int host_os_sem_take(host_os_sem_t psem, host_os_timeout_t timeout);
int host_os_sem_give(host_os_sem_t psem);

int host_os_delayms(uint32_t ms);
int host_os_delayus(uint32_t us);

#if (CFG_HOST_PORT_PM_EN == 1)
void host_os_pm_lock(void);
void host_os_pm_unlock(void);
#endif  /* CFG_HOST_PORT_PM_EN == 1 */
#endif  /* CFG_HOST_PORT_OS_EN == 1 */


#ifdef __cplusplus
}
#endif

#endif /* _HAL_OS_H_ */

