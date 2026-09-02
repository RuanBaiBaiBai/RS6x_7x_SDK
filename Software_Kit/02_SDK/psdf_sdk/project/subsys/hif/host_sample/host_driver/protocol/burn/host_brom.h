/**
 **************************************************************************************************
 * @file    host_brom.h
 * @brief   host brom api header file.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_BROM_H__
#define __HOST_BROM_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../../porting/host_port.h"


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef int (*host_brom_protocol_com_write_t)(void *data, uint32_t len);
typedef int (*host_brom_protocol_com_read_t)(void *buffer, uint32_t size);


/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
int host_brom_protocol_init(host_brom_protocol_com_write_t write, host_brom_protocol_com_read_t read);
void host_brom_protocol_deinit(void);

#if (CFG_HOST_BURN_FLASH_EN)
int host_brom_protocol_flash_id_get(uint32_t *flash_id);
int host_brom_protocol_flash_erase(uint32_t addr_start, uint32_t erase_type);
int host_brom_protocol_flash_write(uint32_t addr_start, uint32_t page_num, void *pdata);
#if (CFG_HOST_BURN_FLASH_READ_EN)
int host_brom_protocol_flash_read(uint32_t addr_start, uint32_t page_num, void *pdata);
#endif
#if (CFG_HOST_BURN_RUN_EN)
int host_brom_protocol_setpc(uint32_t run_addr);
#endif
#endif


#ifdef __cplusplus
}
#endif

#endif /* __HOST_BROM_H__ */

