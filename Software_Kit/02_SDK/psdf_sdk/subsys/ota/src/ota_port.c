/**
 ******************************************************************************
 * @file    ota_port.c
 * @brief   ota port define.
 * @verbatim    null
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2025 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of POSSUMIC TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */


/* Includes.
 * ----------------------------------------------------------------------------
 */

#include "ota_config.h"

#if (CONFIG_OTA == 1)

#include "ota_types.h"
#include "ota_log.h"
#include "ota_slot.h"

#include "ll_utils.h"

#include "hal_flash.h"

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int OTA_SLOT_CheckCurrent(uint32_t addr, uint32_t size)
{
    uint32_t flash_offet = 0;
    uint32_t src_addr;
    uint32_t remap_size;
    LL_REMAP_XIP_GetMap(0, &src_addr, &flash_offet, &remap_size);

    if ((flash_offet >= addr) && (flash_offet <= addr + size)) {
        return OTA_TRUE;
    }

    return OTA_FALSE;
}


int OTA_FLASH_Read(uint32_t addr, void *dst, uint32_t len)
{
    HAL_Status_t status = HAL_STATUS_OK;

    if (dst == NULL) {
        return OTA_STATUS_INVALID_PARAM;
    }

    HAL_Dev_t *pFlashDev = HAL_DEV_Find(HAL_DEV_TYPE_FLASH, 0);
    if (pFlashDev == NULL) {
        return OTA_STATUS_NOT_READY;
    }

    status = HAL_FLASH_Read(pFlashDev, addr, (uint8_t *)dst, len);
    if (status != HAL_STATUS_OK) {
        return OTA_STATUS_IO_ERROR;
    }


    return OTA_STATUS_SUCCESS;
}


int OTA_FLASH_Write(uint32_t addr, const void *src, uint32_t len)
{
    HAL_Status_t status = HAL_STATUS_OK;

    if (src == NULL) {
        return OTA_STATUS_INVALID_PARAM;
    }

    HAL_Dev_t *pFlashDev = HAL_DEV_Find(HAL_DEV_TYPE_FLASH, 0);
    if (pFlashDev == NULL) {
        return OTA_STATUS_NOT_READY;
    }

    status = HAL_FLASH_Write(pFlashDev, addr, (uint8_t *)src, len);
    if (status != HAL_STATUS_OK) {
        return OTA_STATUS_IO_ERROR;
    }


    return OTA_STATUS_SUCCESS;
}


int OTA_FLASH_Erase(uint32_t addr, uint32_t len)
{
    HAL_Status_t status = HAL_STATUS_OK;


    HAL_Dev_t *pFlashDev = HAL_DEV_Find(HAL_DEV_TYPE_FLASH, 0);
    if (pFlashDev == NULL) {
        return OTA_STATUS_NOT_READY;
    }

    status = HAL_FLASH_Erase(pFlashDev, addr, len);
    if (status != HAL_STATUS_OK) {
        return OTA_STATUS_IO_ERROR;
    }


    return OTA_STATUS_SUCCESS;
}


int OTA_FLASH_Save(uint32_t addr, const void *src, uint32_t len)
{
    HAL_Status_t status = HAL_STATUS_OK;
    uint8_t *pbuff;
    uint8_t *pdata = (uint8_t *)src;
    uint32_t write_len;
    uint32_t write_align;
    uint8_t write_buff[OTA_FLASH_WRITE_SIZE];
    if (src == NULL) {
        return OTA_STATUS_INVALID_PARAM;
    }

    HAL_Dev_t *pFlashDev = HAL_DEV_Find(HAL_DEV_TYPE_FLASH, 0);
    if (pFlashDev == NULL) {
        return OTA_STATUS_NOT_READY;
    }

    for (uint32_t off = 0; off < len; off += write_len) {
        write_len = len - off;
        write_align = OTA_FLASH_WRITE_SIZE - ((addr + off) % OTA_FLASH_WRITE_SIZE);
        if (write_len > write_align) {
            write_len = write_align;
        }
        pbuff = &pdata[off];

        status = HAL_FLASH_Write(pFlashDev, addr + off, pbuff, write_len);
        if (status != HAL_STATUS_OK) {
            return OTA_STATUS_IO_ERROR;
        }

        status = HAL_FLASH_Read(pFlashDev, addr + off, write_buff, write_len);
        if (status != HAL_STATUS_OK) {
            return OTA_STATUS_IO_ERROR;
        }


        for (uint32_t idx = 0; idx < write_len; idx++) {
            if (write_buff[idx] != pbuff[idx]) {
                return OTA_STATUS_ERROR;
            }
        }
    }


    return OTA_STATUS_SUCCESS;
}


#endif  /* CONFIG_OTA == 1 */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
