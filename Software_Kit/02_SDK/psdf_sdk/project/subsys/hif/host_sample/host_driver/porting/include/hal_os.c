/**
 **************************************************************************************************
 * @file	hal_os.c
 * @brief   HAL os define.
 * @attention
 *		  Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "hal_os.h"


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
_WEAK_FUNCTION void * host_os_memset(void *addr, uint8_t val, uint32_t size)
{
    return memset(addr, val, size);
}


_WEAK_FUNCTION void * host_os_memcpy(void *dst, const void *src, uint32_t size)
{
    return memcpy(dst, src, size);
}


_WEAK_FUNCTION int host_os_memcmp(const void *addr1, const void *addr2, uint32_t size)
{
    return memcmp(addr1, addr2, size);
}


_WEAK_FUNCTION bool host_os_thread_is_valid(host_os_thread_handle_t thread_hdl)
{
    return (thread_hdl != NULL);
}


_WEAK_FUNCTION int host_os_sem_is_valid(host_os_sem_t psem)
{
    return (psem != NULL);
}
#endif  /* CFG_HOST_PORT_OS_EN == 1 */


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
