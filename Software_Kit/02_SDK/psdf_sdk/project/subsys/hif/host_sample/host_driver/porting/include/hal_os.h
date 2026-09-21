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
// the critical flag, you can change the type by yourself.
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
/**
 * @brief Enter the critical section.
 *
 * @retval The value required to exit critical section.
 */
host_os_critical_flag_t host_os_enter_critical(void);
/**
 * @brief Exit the critical section.
 *
 * @param `host_os_critical_flag_t`
 *        The value required to exit critical section.
 */
void host_os_exit_critical(host_os_critical_flag_t);

/**
 * @brief Get timestamp.
 *
 * @retval Timestamp, unit is us.
 */
host_os_time_t host_os_timestamp_us(void);

/**
 * @brief Dynamically allocate memory.
 *
 * @param size Allocate memory size, unit is byte.
 *
 * @retval Allocated memory addr.
 */
void * host_os_malloc(uint32_t size);
/**
 * @brief Free dynamically allocated memory.
 *
 * @param pmem The memory addr.
 */
void   host_os_free(void *pmem);

/**
 * @brief Set the value of the memory area according to byte size.
 *
 * @param addr The memory addr.
 *
 * @param size The memory area size, unit is byte.
 *
 * @retval The memory addr.
 */
void * host_os_memset(void *addr, uint8_t val, uint32_t size);
/**
 * @brief Copy the value of the memory area.
 *
 * @param dst The distination addr.
 *
 * @param src The source addr.
 *
 * @param size Copy size, unit is byte.
 *
 * @retval The distination addr.
 */
void * host_os_memcpy(void *dst, const void *src, uint32_t size);
/**
 * @brief Compare the value of the memory area.
 *
 * @param addr1 The memory 1 addr to be compared.
 *
 * @param addr2 The memory 2 addr to be compared.
 *
 * @param size Compare size, unit is byte.
 *
 * @retval <0  -->  addr1 < addr2,
 *         =0  -->  addr1 = addr2,
 *         >0  -->  addr1 > addr2.
 */
int host_os_memcmp(const void *addr1, const void *addr2, uint32_t size);

/**
 * @brief Create a thread.
 *
 * @param pname The thread name.
 *
 * @param stack_size The thread stack size.
 *
 * @param entry The thread function.
 *
 * @param arg The thread function's arg.
 *
 * @param prio The thread priority.
 *
 * @retval The thread handle.
 */
host_os_thread_handle_t host_os_thread_create(const char *pname,
                                              uint32_t stack_size,
                                              host_os_thread_entry_t entry,
                                              void *arg, int32_t prio);
/**
 * @brief Verify the validity of thread handle.
 *
 * @param thread_hdl The thread handle.
 *
 * @retval true   -->  is valid,
 *         false  -->  not valid.
 */
bool host_os_thread_is_valid(host_os_thread_handle_t thread_hdl);
/**
 * @brief Delete a thread.
 *
 * @param thread_hdl The thread handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_os_thread_delete(host_os_thread_handle_t thread_hdl);

/**
 * @brief Create a semaphore.
 *
 * @param initial_count The intial count value.
 *
 * @param limit The max count value.
 *
 * @retval The semaphore handle.
 */
host_os_sem_t host_os_sem_create(uint32_t initial_count, uint32_t limit);
/**
 * @brief Delete a semaphore.
 *
 * @param psem The semaphore handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_os_sem_delete(host_os_sem_t psem);
/**
 * @brief Verify the validity of semaphore handle.
 *
 * @param psem The semaphore handle.
 *
 * @retval true   -->  is valid,
 *         false  -->  not valid.
 */
int host_os_sem_is_valid(host_os_sem_t psem);
/**
 * @brief Take semaphore.
 *
 * @param psem The semaphore handle.
 *
 * @param timeout The maximun waiting time, unit is ms.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_os_sem_take(host_os_sem_t psem, host_os_timeout_t timeout);
/**
 * @brief Release semaphore.
 *
 * @param psem The semaphore handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_os_sem_give(host_os_sem_t psem);

/**
 * @brief Non-block delay.
 *
 * @param ms The delay time, unit is ms.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_os_delayms(uint32_t ms);
/**
 * @brief Delay.
 *
 * @param us The delay time, unit is us.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_os_delayus(uint32_t us);

#if (CFG_HOST_PORT_PM_EN == 1)
/**
 * @brief Not allow sleep.
 */
void host_os_pm_lock(void);
/**
 * @brief Allow sleep.
 */
void host_os_pm_unlock(void);
#endif  /* CFG_HOST_PORT_PM_EN == 1 */
#endif  /* CFG_HOST_PORT_OS_EN == 1 */


#ifdef __cplusplus
}
#endif

#endif /* _HAL_OS_H_ */

