/**
 **************************************************************************************************
 * @file    host_burn.h
 * @brief   host burn api header file.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_BURN_H__
#define __HOST_BURN_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../../porting/host_port.h"

#include "../../protocol/llc/host_llc.h"


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_BURN_FLASH_ERASE_CHIP                  0
#define HOST_BURN_FLASH_ERASE_4KB                   1
#define HOST_BURN_FLASH_ERASE_32KB                  2
#define HOST_BURN_FLASH_ERASE_64KB                  3

#define HOST_BURN_FLASH_ERASE_4KB_MSK               (0xFFF)
#define HOST_BURN_FLASH_ERASE_32KB_MSK              (0x7FFF)
#define HOST_BURN_FLASH_ERASE_64KB_MSK              (0xFFFF)

#define HOST_BURN_FLASH_ERASE_4KB_SIZE              (0x1000)
#define HOST_BURN_FLASH_ERASE_32KB_SIZE             (0x8000)
#define HOST_BURN_FLASH_ERASE_64KB_SIZE             (0x10000)

#define HOST_BURN_FLASH_PGAE_MSK                    (0xFF)
#define HOST_BURN_FLASH_PAGE_SIZE                   (256)


#define HOST_BURN_FLASH_WRITE_SIZE                  (2048)


#if (CFG_HOST_BURN_FLASH_READ_EN)
#define HOST_BURN_BUFFER_GET_SIZE                   (2048+12)
#else
#define HOST_BURN_BUFFER_GET_SIZE                   (128)
#endif


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
/**
 * @group burn protocol def
 */
#define HOST_BURN_PROTOCOL_NONE                     (0)
#define HOST_BURN_PROTOCOL_BROM                     (1)
#define HOST_BURN_PROTOCOL_HIF                      (2)
#define HOST_BURN_PROTOCOL_MIN                      HOST_BURN_PROTOCOL_BROM
#define HOST_BURN_PROTOCOL_MAX                      HOST_BURN_PROTOCOL_HIF
typedef uint8_t host_burn_protocol_type_t;

#define HOST_BURN_DEVICE_STORE_NONE                 (0)
#define HOST_BURN_DEVICE_STORE_FLASH                (1)
#define HOST_BURN_DEVICE_STORE_SRAM                 (2)
#define HOST_BURN_DEVICE_STORE_MIN                  HOST_BURN_DEVICE_STORE_FLASH
#define HOST_BURN_DEVICE_STORE_MAX                  HOST_BURN_DEVICE_STORE_SRAM
typedef uint8_t host_burn_device_store_type_t;


/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
int HOST_BURN_Init(void);
void HOST_BURN_DeInit(void);
/**
 * @brief init burn protocol for a llc device, can only be used by one LLC obj at a time
 *
 * @param llc_hdl LLC obj
 *
 * @retval
 *     0 -> succ
 *     HOST_ERRCODE_BUSY -> fail(using)
 *     other -> fail
 */
int HOST_BURN_Protocol_Init(LLC_HANDLE llc_hdl);
void HOST_BURN_Protocol_DeInit(void);

int HOST_BURN_Open_Image(void **img, void *path, uint32_t *len);
void HOST_BURN_Close_Image(void **img);

int HOST_BURN_Get_Flash_ID(uint32_t *flash_id);
int HOST_BURN_Download_Image(void *img, uint32_t len, host_burn_device_store_type_t type);
int HOST_BURN_Set_PC(uint32_t run_addr);

int HOST_BURN_Sync(uint32_t sync_cnt, host_burn_protocol_type_t *proto_type);


#ifdef __cplusplus
}
#endif

#endif /* __HOST_BURN_H__ */


