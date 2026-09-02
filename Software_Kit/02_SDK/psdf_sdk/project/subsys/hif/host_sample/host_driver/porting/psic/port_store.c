/**
 **************************************************************************************************
 * @file    port_store.c
 * @brief   adaptation for store.
 * @attention
 *        Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"

#if (CFG_HOST_PORT_STORE_EN == 1)
#include "hal_flash.h"
#if (CFG_HOST_PORT_STORE_NV_EN == 1)
#include "nvs.h"
#include "kvf.h"
#endif /* CFG_HOST_PORT_STORE_NV_EN == 1 */
#endif /* CFG_HOST_PORT_STORE_EN == 1 */


/* Local Variable.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_STORE_EN == 1)
static HAL_Dev_t *pFlashDev = NULL;
#endif /* CFG_HOST_PORT_STORE_EN == 1 */


/* Local constants.
 * ------------------------------------------------------------------------------------------------
 */
#define CFG_PORT_HOST_STORE_IMAGE_HANDLE                    ((void *)1)
#define CFG_PORT_HOST_STORE_IMAGE_START_ADDR                (0x50000)
#define CFG_PORT_HOST_STORE_IMAGE_LEN                       (0x20000)


/* Macro.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_STORE_IMAGE_IS_VALID(img_hdl)                  ((img_hdl) == CFG_PORT_HOST_STORE_IMAGE_HANDLE)
#define HOST_STORE_IMAGE_GET_HANDLE(path)                   (CFG_PORT_HOST_STORE_IMAGE_HANDLE)
#define HOST_STORE_IMAGE_GET_LENGTH(path)                   (CFG_PORT_HOST_STORE_IMAGE_LEN)


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_STORE_EN == 1)
int host_store_open(void **hdl, void *path, uint32_t *len)
{
    if (hdl == NULL || len == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    // set image handle, this is just an example
    *hdl = HOST_STORE_IMAGE_GET_HANDLE(path);
    // set image length, typical unit is byte
    *len = HOST_STORE_IMAGE_GET_LENGTH(path);

    return HOST_ERRCODE_SUCCESS;
}


void host_store_close(void *hdl)
{
    /* No code needs to be added for the current adaptation. */
}


int host_store_read(void *hdl, uint32_t offset, void *buff, uint32_t size)
{
    int status = HOST_ERRCODE_SUCCESS;

    /* check param */
    if (!HOST_STORE_IMAGE_IS_VALID(hdl)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (buff == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    /* find image */
    pFlashDev = HAL_DEV_Find(HAL_DEV_TYPE_FLASH, 0);
    if (pFlashDev == NULL) {
        HOST_LOG_PRINT("flash not init\n");
        return HOST_ERRCODE_IO_ERROR;
    }

    /* read image data to buffer */
    // CFG_PORT_HOST_STORE_IMAGE_START_ADDR is the image address
    status = HAL_FLASH_Read(pFlashDev, CFG_PORT_HOST_STORE_IMAGE_START_ADDR + offset, buff, size);
    if (status != 0) {
        return HOST_ERRCODE_IO_ERROR;
    }

    return HOST_ERRCODE_SUCCESS;
}
#endif /* CFG_HOST_PORT_STORE_EN == 1 */



/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
