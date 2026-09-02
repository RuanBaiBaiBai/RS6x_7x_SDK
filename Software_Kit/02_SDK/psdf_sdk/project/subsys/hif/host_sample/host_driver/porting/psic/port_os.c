/**
 **************************************************************************************************
 * @file	port_os.c
 * @brief   communication adaptation for uart.
 * @attention
 *		  Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "policy.h"

#include "../host_port.h"

#if (CFG_HOST_PORT_OS_EN == 1)
#include "osi.h"
#include "hal_board.h"
#endif  /* CFG_HOST_PORT_OS_EN == 1 */


/* Types.
 * ------------------------------------------------------------------------------------------------
 */
/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_OS_EN == 1)
host_os_critical_flag_t host_os_enter_critical(void)
{
    if (OSI_IsISRContext()) {
        return portSET_INTERRUPT_MASK_FROM_ISR();
    } else {
        vPortEnterCritical();
        return 0;
    }
}


void host_os_exit_critical(host_os_critical_flag_t flag)
{
    if (OSI_IsISRContext()) {
        portCLEAR_INTERRUPT_MASK_FROM_ISR(flag);
    } else {
        vPortExitCritical();
    }
}


host_os_time_t host_os_timestamp_us(void)
{
    return (uint32_t)HAL_BOARD_GetTime(HAL_TIME_US);
}

#define CACHE_LINE_SIZE   32u
#define PTR_STORAGE_SIZE  4u
#define ALIGN_MASK        (CACHE_LINE_SIZE - 1)

void * host_os_malloc(uint32_t size)
{
#if 0
    void *mem = NULL;
    mem = OSI_Malloc(size);
    return mem;
#else
    if (size == 0) {
        return NULL;
    }

    size = (size + 31) & (~0x1F);

    size_t total_size = size + (CACHE_LINE_SIZE - 1) + PTR_STORAGE_SIZE;
    void *raw = malloc(total_size);
    if (raw == NULL) {
        return NULL;
    }

    uintptr_t raw_addr = (uintptr_t)raw;
    uintptr_t aligned_base = (raw_addr + PTR_STORAGE_SIZE + ALIGN_MASK) & ~ALIGN_MASK;

    void **ptr_storage = (void **)(aligned_base - PTR_STORAGE_SIZE);
    *ptr_storage = raw;

    return (void *)aligned_base;
#endif
}


void host_os_free(void *pmem)
{
#if 0
    if (pmem != NULL) {
        OSI_Free(pmem);
    }
#else
    if (pmem != NULL) {
        void *raw_ptr = *((void **)((uintptr_t)pmem - PTR_STORAGE_SIZE));
        free(raw_ptr);
    }
#endif
}


void * host_os_memset(void *addr, uint8_t val, uint32_t size)
{
    if (addr == NULL) {
        return NULL;
    }
    return OSI_Memset(addr, val, size);
}


void * host_os_memcpy(void *dst, const void *src, uint32_t size)
{
    if (dst == NULL || src == NULL) {
        return NULL;
    }
    return OSI_Memcpy(dst, src, size);
}


int host_os_memcmp(const void *addr1, const void *addr2, uint32_t size)
{
    if (addr1 == NULL || addr2 == NULL) {
        return 0;
    }
    return OSI_Memcmp(addr1, addr2, size);
}


host_os_thread_handle_t host_os_thread_create(
    const char *pname,
    uint32_t stack_size,
    host_os_thread_entry_t entry,
    void *arg, int32_t prio
)
{
    OSI_Status_t status = OSI_STATUS_OK;
    OSI_Thread_t *thread = NULL;

    if (stack_size == 0) {
        HOST_LOG_ERR("stack size 0\n");
        return NULL;
    }

    if (entry == NULL) {
        HOST_LOG_ERR("enter address NULL\n");
        return NULL;
    }

    thread = (OSI_Thread_t *)host_os_malloc(sizeof(OSI_Thread_t));
    if (thread == NULL) {
        HOST_LOG_ERR("alloc thread handle fail\n");
        return NULL;
    }
    host_os_memset(thread, 0, sizeof(OSI_Thread_t));

    OSI_ThreadSetInvalid(thread);
    status = OSI_ThreadCreate(thread, pname, (OSI_ThreadEntry_t)entry, arg, prio, stack_size);
    if (status != OSI_STATUS_OK) {
        HOST_LOG_ERR("create thread fail\n");
        host_os_free((void *)thread);
        thread = NULL;
        return NULL;
    }

    return (host_os_thread_handle_t)thread;
}


int host_os_thread_delete(host_os_thread_handle_t thread)
{
    OSI_Status_t status = OSI_STATUS_OK;

    if (thread == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = OSI_ThreadDelete((OSI_Thread_t *)thread);
    if (status != OSI_STATUS_OK) {
        HOST_LOG_ERR("delete thread fail\n");
        return HOST_ERRCODE_SYSERR;
    }

    host_os_free(thread);
    thread = NULL;

    return HOST_ERRCODE_SUCCESS;
}


host_os_sem_t host_os_sem_create(uint32_t initial_count, uint32_t limit)
{
    OSI_Status_t status = OSI_STATUS_OK;
    OSI_Semaphore_t *psem = NULL;

    psem = (OSI_Semaphore_t *)host_os_malloc(sizeof(OSI_Semaphore_t));
    if (psem == NULL) {
        HOST_LOG_ERR("alloc sem handle fail\n");
        return NULL;
    }
    host_os_memset(psem, 0, sizeof(OSI_Semaphore_t));

    status = OSI_SemaphoreCreate(psem, initial_count, limit);
    if (status != OSI_STATUS_OK) {
        HOST_LOG_ERR("create sem fail\n");
        host_os_free(psem);
        psem = NULL;
        return NULL;
    }

    return (host_os_sem_t)psem;
}


int host_os_sem_delete(host_os_sem_t psem)
{
    OSI_Status_t status = OSI_STATUS_OK;

    if (psem == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = OSI_SemaphoreDelete((OSI_Semaphore_t *)psem);
    if (status != OSI_STATUS_OK) {
        HOST_LOG_ERR("delete sem fail\n");
        return HOST_ERRCODE_SYSERR;
    }

    host_os_free(psem);
    psem = NULL;

    return HOST_ERRCODE_SUCCESS;
}


int host_os_sem_is_valid(host_os_sem_t psem)
{
//    if (psem == NULL) {
//        return HOST_ERRCODE_INVALID_PARAM;
//    }

    return OSI_SemaphoreIsValid((OSI_Semaphore_t *)psem);
}


int host_os_sem_take(host_os_sem_t psem, host_os_timeout_t timeout)
{
    if (psem == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    int status = OSI_SemaphoreWait((OSI_Semaphore_t *)psem, timeout);
    if (status == OSI_STATUS_OK) {
        return HOST_ERRCODE_SUCCESS;
    } else if (status == OSI_STATUS_ERR_TIMEOUT) {
        return HOST_ERRCODE_TIMEOUT;
    } else {
        return status;
    }
}


int host_os_sem_give(host_os_sem_t psem)
{
    if (psem == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return OSI_SemaphoreRelease((OSI_Semaphore_t *)psem);
}


int host_os_delayms(uint32_t ms)
{
    OSI_MSleep(ms);

    return 0;
}


int host_os_delayus(uint32_t us)
{
	if (us >= 1000) {
		OSI_MSleep(us / 1000);
        us %= 1000;
	}
    HAL_BOARD_BlkDelay(us, HAL_TIME_US);
    return 0;
}


#if (CFG_HOST_PORT_PM_EN == 1)
static struct wakelock host_pm_lock;
void host_os_pm_lock(void)
{
	pm_policy_wake_lock(&host_pm_lock);
}

void host_os_pm_unlock(void)
{
	pm_policy_wake_unlock(&host_pm_lock);
}
#endif  /* CFG_HOST_PORT_PM_EN == 1 */
#endif  /* CFG_HOST_PORT_OS_EN == 1 */


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
