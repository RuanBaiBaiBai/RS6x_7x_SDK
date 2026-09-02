/**
 ******************************************************************************
 * @file    ota_loader.c
 * @brief   ota loader define.
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
#include "ota_img_struct.h"
#include "ota_sha256.h"
#include "ota_slot.h"
#include "ota_utils.h"
#include "ota_img.h"

#include "ota_log.h"



/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int ota_IMG_VerifyBootHeader(IMG_HDR_Boot_t *phdr)
{
    /* check magic head */
    if (phdr->hdrV1.magic == IMG_MAGIC) {
        uint32_t *pbuf = (uint32_t *)phdr;

        /* check hdr checksum */
        uint32_t checksum = 0;
        for (uint32_t idx = 0; idx < (IMG_HDR_BROM_V1_SIZE / 4); idx++) {
            checksum += pbuf[idx];
        }

        if (checksum != 0xFFFFFFFF) {
            return OTA_STATUS_HASH_ERROR;
        }

        /* TODO check version */
        OTA_LOG_INF("bootloader version: v25-%d.%d.%d",
                      (phdr->hdrV1.hdrVer >> 24) & 0xFF,
                      (phdr->hdrV1.hdrVer >> 16) & 0xFF,
                      (phdr->hdrV1.hdrVer >>  0) & 0xFFFF);

    } else if ((phdr->hdrV2.magic & IMG_MAGIC_SHORT_MSK) == IMG_MAGIC_SHORT) {

        uint16_t *pbuf = (uint16_t *)phdr;

        /* check hdr checksum */
        uint16_t checksum = 0;
        for (uint32_t idx = 0; idx < (IMG_HDR_BROM_V2_SIZE / 2); idx++) {
            checksum += pbuf[idx];
        }

        if (checksum != 0xFFFF) {
            return OTA_STATUS_HASH_ERROR;
        }

        /* TODO check version */
        OTA_LOG_INF("bootloader version: v31-%d",
                      (phdr->hdrV2.magic >> 16) & 0xFF);

    } else {
        return OTA_STATUS_MAGIC_ERROR;
    }

    return OTA_STATUS_SUCCESS;
}


static int ota_img_get_header(uint32_t addr, IMG_HDR_t *phdr)
{
    int status = 0;

    status = OTA_FLASH_Read(addr, phdr, IMG_HDR_SIZE);
    if (status != 0) {
        OTA_LOG_ERR("read image header err(%d), addr(0x%08X)", status, addr);
        return OTA_STATUS_IO_ERROR;
    }

    if (phdr->magic != IMG_MAGIC) {
        OTA_LOG_ERR("image header magic err(0x%08X)", phdr->magic);
        return OTA_STATUS_MAGIC_ERROR;
    }

    return OTA_STATUS_SUCCESS;
}


static int ota_tlv_get_header(uint32_t addr, uint16_t protect_tlv_size, IMG_TLV_InfoCtrl_t *ptlv_inf_ctrl)
{
    int status = OTA_STATUS_SUCCESS;
    IMG_TLV_Info_t img_tlv_inf;


    status = OTA_FLASH_Read(addr, &img_tlv_inf, IMG_TLV_INF_SIZE);
    if (status != 0) {
        OTA_LOG_ERR("get data err (%d)", status);
        return status;
    }

    if (img_tlv_inf.magic == IMG_TLV_PROT_INFO_MAGIC) {
        if ((protect_tlv_size == img_tlv_inf.len) && (protect_tlv_size != 0)) {
            ptlv_inf_ctrl->addr   = addr + IMG_TLV_INF_SIZE;
            ptlv_inf_ctrl->len    = img_tlv_inf.len - IMG_TLV_INF_SIZE;
            ptlv_inf_ctrl->offset = 0;
        } else {
            OTA_LOG_ERR("port tlv, size error");
            return OTA_STATUS_UNDEFINE;
        }

        addr  += img_tlv_inf.len;
        status = OTA_FLASH_Read(addr, &img_tlv_inf, IMG_TLV_INF_SIZE);
        if (status != 0) {
            OTA_LOG_ERR("get data err (%d)", status);
            return status;
        }

        ptlv_inf_ctrl++;
        if (img_tlv_inf.magic == IMG_TLV_INFO_MAGIC) {
            ptlv_inf_ctrl->addr   = addr + IMG_TLV_INF_SIZE;
            ptlv_inf_ctrl->len    = img_tlv_inf.len - IMG_TLV_INF_SIZE;
            ptlv_inf_ctrl->offset = 0;
        } else {
            OTA_LOG_ERR("nml tlv, magic error");
            return OTA_STATUS_UNDEFINE;
        }
    } else if (img_tlv_inf.magic == IMG_TLV_INFO_MAGIC) {
        if (protect_tlv_size != 0) {
            OTA_LOG_ERR("nml tlv, size error (%d)", protect_tlv_size);
            return OTA_STATUS_UNDEFINE;
        }

        ptlv_inf_ctrl->addr   = 0;
        ptlv_inf_ctrl->len    = 0;
        ptlv_inf_ctrl->offset = 0;

        ptlv_inf_ctrl++;
        ptlv_inf_ctrl->addr   = addr + IMG_TLV_INF_SIZE;
        ptlv_inf_ctrl->len    = img_tlv_inf.len - IMG_TLV_INF_SIZE;
        ptlv_inf_ctrl->offset = 0;
    } else {
        OTA_LOG_ERR("magic error");
        return OTA_STATUS_UNDEFINE;
    }


    return OTA_STATUS_SUCCESS;
}


static int ota_tlv_get_next(IMG_TLV_InfoCtrl_t *ptlv_inf_ctrl,
                                     IMG_TLV_Ctrl_t     *ptlv_ctrl)
{
    int status = OTA_STATUS_SUCCESS;
    IMG_TLV_Hdr_t img_tlv_hdr;

    if (ptlv_inf_ctrl->offset >= ptlv_inf_ctrl->len) {
        return OTA_STATUS_DONE;
    }

    while (ptlv_inf_ctrl->offset < ptlv_inf_ctrl->len) {
        status = OTA_FLASH_Read(ptlv_inf_ctrl->addr + ptlv_inf_ctrl->offset,
                                     &img_tlv_hdr, IMG_TLV_HDR_SIZE);
        if (status != 0) {
            OTA_LOG_DBG("get data err (%d)", status);
            return status;
        }

        OTA_LOG_DBG("get tlv : type (%04X), len (%d) ",
                    img_tlv_hdr.type, img_tlv_hdr.len);

        if ((ptlv_ctrl->type     == IMG_TLV_ANY) ||
            (img_tlv_hdr.type == ptlv_ctrl->type)) {
            ptlv_ctrl->type = img_tlv_hdr.type;
            ptlv_ctrl->len  = img_tlv_hdr.len;
            ptlv_ctrl->addr = ptlv_inf_ctrl->addr
                              + ptlv_inf_ctrl->offset
                              + IMG_TLV_HDR_SIZE;

            ptlv_inf_ctrl->offset += img_tlv_hdr.len + IMG_TLV_HDR_SIZE;
            return OTA_STATUS_SUCCESS;
        }

        ptlv_inf_ctrl->offset += img_tlv_hdr.len + IMG_TLV_HDR_SIZE;
    }

    return OTA_STATUS_UNDEFINE;
}


static int ota_img_calc_hash(uint32_t addr, void *presult, uint32_t merge)
{
    int status = OTA_STATUS_SUCCESS;
    uint8_t hash_buff[CFG_OTA_HASH_BUF_SIZE];
    OTA_SHA256_t sha256_ctx;
    IMG_HDR_t image_header;
    uint32_t hash_size = 0;
    uint32_t hash_addr = 0;
    uint32_t size = 0;


    OTA_SHA256_Init(&sha256_ctx);

    /* 1. calc image head */
    status = OTA_FLASH_Read(addr, (uint8_t *)&image_header, IMG_HDR_SIZE);
    if (status != 0) {
        OTA_LOG_ERR("get data err (%d. 0x%08X, %d)", status, addr, IMG_HDR_SIZE);
        return status;
    }

    OTA_SHA256_Update(&sha256_ctx, (uint8_t *)&image_header, IMG_HDR_SIZE);

    /* 2. calc image head pad */
    if (merge) {
        size = image_header.hdrSize - IMG_HDR_SIZE;
        for (uint32_t idx = 0; idx < size; idx += hash_size) {
            hash_size = size - idx;
            if (hash_size > CFG_OTA_HASH_BUF_SIZE) {
                hash_size = CFG_OTA_HASH_BUF_SIZE;
            }

            memset(hash_buff, 0xFF, hash_size);

            OTA_SHA256_Update(&sha256_ctx, hash_buff, hash_size);
        }
    }

    /* 3. calc image payload */
    size = image_header.imgSize;
    hash_addr = addr + image_header.hdrSize;
    for (int idx = 0; idx < size; idx += hash_size) {
        hash_size = size - idx;
        if (hash_size > CFG_OTA_HASH_BUF_SIZE) {
            hash_size = CFG_OTA_HASH_BUF_SIZE;
        }

        status = OTA_FLASH_Read(hash_addr + idx, hash_buff, hash_size);
        if (status != 0) {
            OTA_LOG_ERR("get data err (%d. 0x%08X, %d)",
                        status, hash_size + idx, hash_size);
            return status;
        }

        OTA_SHA256_Update(&sha256_ctx, hash_buff, hash_size);
    }

    /* 4. calc image tlv */
    size = image_header.protectTlvSize;
    if (merge) {
        hash_addr = addr + IMG_HDR_SIZE;
    } else {
        hash_addr = addr + image_header.hdrSize + image_header.imgSize;
    }
    for (int idx = 0; idx < size; idx += hash_size) {
        hash_size = size - idx;
        if (hash_size > CFG_OTA_HASH_BUF_SIZE) {
            hash_size = CFG_OTA_HASH_BUF_SIZE;
        }

        status = OTA_FLASH_Read(hash_addr + idx, hash_buff, hash_size);
        if (status != 0) {
            OTA_LOG_ERR("get data err (%d. 0x%08X, %d)", status, hash_size + idx, hash_size);
            return status;
        }

        OTA_SHA256_Update(&sha256_ctx, hash_buff, hash_size);
    }

    OTA_SHA256_Final(presult, &sha256_ctx);

    return status;
}


int ota_img_get_info(uint32_t addr, uint32_t *pnext_img_addr, uint8_t *phash_data)
{
    int status = OTA_STATUS_UNDEFINE;
    IMG_HDR_t img_hdr;


    status = ota_img_get_header(addr, &img_hdr);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("get img header err (%d)", status);
        return status;
    }

    /* tlv info index
     * 0 : prote tlv
     * 1 : nomal tlv
     */
    uint32_t img_size = 0;
    uint32_t align_len = 0;
    uint32_t tlv_start_addr = 0;
    if ((img_hdr.flags & IMG_HDR_FLG_MI_MSK) != 0) {
        tlv_start_addr = addr + IMG_HDR_SIZE;
        img_size = img_hdr.hdrSize + img_hdr.imgSize;
        align_len = align_len % 4;
        if (align_len > 0) {
            align_len += (4 -align_len);
        }
    } else {
        tlv_start_addr = addr + img_hdr.hdrSize + img_hdr.imgSize;
        img_size = img_hdr.hdrSize + img_hdr.imgSize;
    }

    IMG_TLV_InfoCtrl_t img_tlv_inf_ctrl[2];

    status = ota_tlv_get_header(tlv_start_addr, img_hdr.protectTlvSize, img_tlv_inf_ctrl);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("get tlv inf err (%d)", status);
        return status;
    }

    if ((img_hdr.flags & IMG_HDR_FLG_MI_MSK) == 0) {
        if (img_tlv_inf_ctrl[0].len > 0) {
             img_size += img_tlv_inf_ctrl[0].len + IMG_TLV_INF_SIZE;
         }
         if (img_tlv_inf_ctrl[1].len > 0) {
             img_size += img_tlv_inf_ctrl[1].len + IMG_TLV_INF_SIZE;
         }
        uint32_t align_len = align_len % 4;
        if (align_len > 0) {
            align_len += (4 -align_len);
        }
    }

    *pnext_img_addr = addr + img_size;


    /* get nml tlv */
    IMG_TLV_Ctrl_t tlv_ctrl = {
        .type = IMG_TLV_ANY,
        .len  = 0,
        .addr = 0
    };

    do {
        tlv_ctrl.type = IMG_TLV_ANY;
        status = ota_tlv_get_next(&img_tlv_inf_ctrl[1], &tlv_ctrl);
        if (status == OTA_STATUS_DONE) {
            status = OTA_STATUS_UNDEFINE;
            break;
        } else if (status != OTA_STATUS_SUCCESS) {
            OTA_LOG_ERR("get tlv err (%d)", status);
            break;
        }

        if (tlv_ctrl.type == IMG_TLV_SHA256) {
            status = OTA_FLASH_Read(tlv_ctrl.addr, phash_data, tlv_ctrl.len);
            OTA_LOG_DBG_HEX(phash_data, CFG_OTA_HASH_SIZE, "get hash : ");
            break;
        }

    } while (1);


    return status;
}


int ota_IMG_VerifyAll(uint32_t start_addr, uint32_t max_addr)
{
    int status = OTA_STATUS_SUCCESS;
    uint32_t next_img_addr = 0;
    uint8_t hash_buff[CFG_OTA_HASH_SIZE];
    uint8_t hash_result[CFG_OTA_HASH_SIZE];


    OTA_LOG_INF("verify merge image");

    status = ota_img_get_info(start_addr, &next_img_addr, hash_buff);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("get info err (%d)", status);
        return status;
    }

    status = ota_img_calc_hash(start_addr, hash_result, 1);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("hash calc err (%d)", status);
        return status;
    }
    OTA_LOG_DBG_HEX(hash_result, CFG_OTA_HASH_SIZE, "cal hash : ");

    for (uint8_t idx = 0; idx < CFG_OTA_HASH_SIZE; idx++) {
        if (hash_buff[idx] != hash_result[idx]) {
            OTA_LOG_ERR("sha256 err: %d, get(0x%08X) cal(0x%08X)", idx,
                        hash_buff[idx], hash_result[idx]);
            OTA_LOG_ERR_HEX(hash_buff,   CFG_OTA_HASH_SIZE, "get hash : ");
            OTA_LOG_ERR_HEX(hash_result, CFG_OTA_HASH_SIZE, "cal hash : ");
            status = OTA_STATUS_UNDEFINE;
            return status;
        }
    }

    OTA_LOG_INF("verify success");

    for (start_addr = next_img_addr ; start_addr < max_addr; ) {
        OTA_LOG_INF("verify sub image");

        status = ota_img_get_info(start_addr, &next_img_addr, hash_buff);
        if (status != OTA_STATUS_SUCCESS) {
            if (status == OTA_STATUS_MAGIC_ERROR) {
                status = OTA_STATUS_SUCCESS;
                OTA_LOG_ERR("cfg image complete");
            } else {
                OTA_LOG_ERR("get hash err (%d)", status);
            }
            return status;
        }
        status = ota_img_calc_hash(start_addr, hash_result, 0);
        if (status != OTA_STATUS_SUCCESS) {
            OTA_LOG_ERR("hash calc err (%d)", status);
            return status;
        }
        OTA_LOG_DBG_HEX(hash_result, CFG_OTA_HASH_SIZE, "cal hash : ");
        for (uint8_t idx = 0; idx < CFG_OTA_HASH_SIZE; idx++) {
            if (hash_buff[idx] != hash_result[idx]) {
                OTA_LOG_ERR("sha256 err: %d, get(0x%08X) cal(0x%08X)", idx,
                            hash_buff[idx], hash_result[idx]);
                OTA_LOG_ERR_HEX(hash_buff, CFG_OTA_HASH_SIZE, "get hash : ");
                OTA_LOG_ERR_HEX(hash_result, CFG_OTA_HASH_SIZE, "cal hash : ");
                status = OTA_STATUS_UNDEFINE;
                return status;
            }
        }

        start_addr = next_img_addr;
        OTA_LOG_INF("verify success");
    }

    return status;
}


#endif  /* CONFIG_OTA == 1 */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
