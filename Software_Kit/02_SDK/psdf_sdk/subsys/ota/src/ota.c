/**
 ******************************************************************************
 * @file    ota.c
 * @brief   ota define.
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

#include "common.h"
#include "ota_config.h"

#if (CONFIG_OTA == 1)

#include "ota.h"
#include "ota_types.h"
#include "ota_img_struct.h"
#include "ota_sha256.h"
#include "ota_img.h"
#include "ota_log.h"

#include "ota_slot.h"

#include "ll_utils.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */

typedef enum {
    OTA_STATE_MGC                    = 0,
    OTA_STATE_HDR                    = 1,
    OTA_STATE_PLD                    = 3,
    OTA_STATE_DONE                   = 5,
    OTA_STATE_PADDING                = 7,
    OTA_STATE_ERROR                  = 9,
} ota_ctrl_state_t;

typedef struct {
    ota_ctrl_state_t    state;
    uint32_t            max_addr;
    uint32_t            write_addr;
    uint32_t            erase_addr;
    uint32_t            size;
    uint32_t            offset;
    uint32_t            data_size;
    uint32_t            write_offset;
    uint32_t            data_offset;

    uint32_t            tail_offset;
    uint8_t             hdr[IMG_HDR_BROM_SIZE];
    uint8_t             hdr_offset;
    uint8_t             hdr_size;
    uint8_t             skip;
    uint32_t            pre_offset;
    uint32_t            cur_offset;
    uint8_t             verify;
} OTA_Ctrl_t;


static OTA_Ctrl_t otaCtrl;

static OTA_SlotId_t otaUpgradeSlotId = OTA_SLOT_IDX_NULL;

static uint8_t otaVerify = OTA_FALSE;


#define MAGIC_SIZE                          4
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static OTA_SlotId_t ota_GetUpgradeSlotId(void);

static OTA_SlotArea_t * ota_GetUpgradeArea(void);

/* Exported functions.
 * ----------------------------------------------------------------------------
*/
int OTA_GetUpgradeInfo(OTA_Info_t *pinfo)
{
    OTA_SlotArea_t * pSlotArea = NULL;

    OTA_LOG_INF("OTA_GetUpgradeInfo");


    pSlotArea = ota_GetUpgradeArea();
    if (pSlotArea == NULL) {
        OTA_LOG_ERR("get upgrade slot area fail");
        return OTA_STATUS_NOT_READY;
    }

    OTA_LOG_INF("upgrade slot: off (0x%08X) size (0x%08X %dKB)",
                pSlotArea->off, pSlotArea->size, pSlotArea->size/1024);
    if ((pSlotArea->size <= (OTA_FLASH_ERASE_SIZE * 2)) ||
       ((pSlotArea->size % OTA_FLASH_ERASE_SIZE) != 0)) {
        OTA_LOG_ERR("upgrade slot size error(%08X Bytes)", pSlotArea->size);
        return OTA_STATUS_ERROR;
    }

    pinfo->area_size = pSlotArea->size - OTA_FLASH_ERASE_SIZE;

    if (VER_GetSdkVersion((VER_SdkVersion_t *)&pinfo->version) != 0) {
        OTA_LOG_ERR("get img version error");
        return OTA_STATUS_ERROR;
    }

    OTA_LOG_INF("current slot image version: R %d.%d M %d.%d.%d V %d.%d.%02X",
                pinfo->version.minorR,
                pinfo->version.typeR,
                pinfo->version.projectM,
                pinfo->version.numberM,
                pinfo->version.minorM,
                pinfo->version.majorV,
                pinfo->version.minorV,
                pinfo->version.revisionV);


    return OTA_STATUS_SUCCESS;
}


/*
 * image include: bootloader + merge image + cfg image
 *
 * if bootloader image check success:
 *   1. clear upgrade slot state
 *   2. set magic, set perm to unset, set done to unset
 *
 */
int OTA_WriteImage(uint32_t offset, void *pimg, uint32_t len)
{
    int      status   =  0U;
    uint32_t data_len =  0U;
    uint32_t wr_len   =  0U;
    uint32_t req_len  =  0U;
    uint8_t *pdata    = (uint8_t *)pimg;

    if (offset == 0) {
        OTA_SlotArea_t *pslot_area;
        pslot_area = ota_GetUpgradeArea();
        if (pslot_area == NULL) {
            OTA_LOG_ERR("\nget upgrade area info fail");
            return -1;
        }

        otaCtrl.max_addr   = pslot_area->off + pslot_area->size
                              - OTA_FLASH_ERASE_SIZE;
        otaCtrl.erase_addr = pslot_area->off;
        otaCtrl.write_addr = pslot_area->off;
        otaCtrl.state      = OTA_STATE_HDR;
        otaCtrl.size       = IMG_HDR_SIZE;
        otaCtrl.offset     = 0;
        otaCtrl.skip       = 0;

        otaCtrl.hdr_size   = MAGIC_SIZE;
        otaCtrl.hdr_offset = 0;

        otaCtrl.pre_offset = 0U;
        otaCtrl.cur_offset = 0U;

        if (len < sizeof(IMG_MAGIC)) {
            OTA_LOG_ERR("\nthe length of the first data is too short");
            return -2;
        }

        if ((*(uint32_t *)pdata != IMG_MAGIC) &&
            (((*(uint32_t *)pdata) & IMG_MAGIC_SHORT_MSK) != IMG_MAGIC_SHORT)) {
            OTA_LOG_ERR("\nthe data first word must be image's magic");
            return -3;
        }
    } else {
        if (otaCtrl.pre_offset >= offset) {
            OTA_LOG_WRN("\npre off >= off (%08X)\n ", offset);
            return 0;
        }
    }


    if (otaCtrl.cur_offset != offset) {
        OTA_LOG_ERR("\ninvalid offset(%08x %08x)", otaCtrl.cur_offset, offset);
        return -4;
    }


    otaCtrl.data_size   = len;
    otaCtrl.data_offset = 0;

    do {
        switch (otaCtrl.state) {
        case OTA_STATE_HDR:
            if (otaCtrl.offset < otaCtrl.size) {
                req_len = otaCtrl.size - otaCtrl.offset;
                data_len = otaCtrl.data_size - otaCtrl.data_offset;
                if (req_len > data_len) {
                    req_len = data_len;
                }

                memcpy(&otaCtrl.hdr[otaCtrl.offset],
                       &pdata[otaCtrl.data_offset],
                       req_len);

                otaCtrl.data_offset += req_len;
                otaCtrl.offset      += req_len;
            } else {
                IMG_HDR_t *pimg_hdr = (IMG_HDR_t *)otaCtrl.hdr;
                if ((pimg_hdr->magic == IMG_MAGIC) ||
                    ((pimg_hdr->magic & IMG_MAGIC_SHORT_MSK) == IMG_MAGIC_SHORT)) {
                    if ((pimg_hdr->hdrVer & IMG_HDR_VER_MSK) == IMG_HDR_VER_APP) {
                        /* found application image */
                        if ((pimg_hdr->flags & IMG_HDR_FLG_MI_MSK) != 0) {
                            /* found merge image */
                            otaCtrl.hdr_offset = 0;
                            otaCtrl.hdr_size   = otaCtrl.size;
                            otaCtrl.skip       = 0;
                            otaCtrl.offset     = otaCtrl.size;
                            otaCtrl.size       = pimg_hdr->hdrSize
                                                  + pimg_hdr->imgSize;

                            req_len = otaCtrl.size & CFG_OTA_ALIGN_MSK;
                            if (req_len > 0) {
                                otaCtrl.size += req_len;
                            }

                            otaCtrl.state = OTA_STATE_PLD;
                            OTA_LOG_INF("\nmerge img");
                        } else {
                            /* found cfg image */
                            otaCtrl.hdr_offset = 0;
                            otaCtrl.hdr_size   = otaCtrl.size;
                            otaCtrl.skip       = 0;
                            otaCtrl.offset     = otaCtrl.size;
                            otaCtrl.size       = pimg_hdr->hdrSize
                                                  + pimg_hdr->imgSize
                                                  + pimg_hdr->protectTlvSize
                                                  + CFG_OTA_TLV_HDR_SIZE
                                                  + CFG_OTA_TLV_HDR_SIZE
                                                  + CFG_OTA_HASH_SIZE;

                            req_len = otaCtrl.size & CFG_OTA_ALIGN_MSK;
                            if (req_len > 0) {
                                otaCtrl.size += req_len;
                            }

                            otaCtrl.state = OTA_STATE_PLD;
                            OTA_LOG_INF("\ncfg img");
                        }
                    } else {
                        /* found bootloader image */
                        if (otaCtrl.size == IMG_HDR_SIZE) {
                            otaCtrl.size     = IMG_HDR_BROM_SIZE;
                            otaCtrl.hdr_size = IMG_HDR_BROM_SIZE;
                        } else {
                            /* clear upgrade slot state */
                            otaCtrl.state = OTA_STATE_ERROR;
                            otaCtrl.verify = 0;
                            uint8_t slot;
                            slot = ota_GetUpgradeSlotId();
                            if (slot == OTA_SLOT_IDX_NULL) {
                                OTA_LOG_ERR("get upgrade slot fail");
                                return -1;
                            }

                            status = ota_SLOT_ClearState(slot);
                            if (status != OTA_STATUS_SUCCESS) {
                                return -2;
                            }

                            IMG_HDR_Boot_t *pimg_hdr_bl = (IMG_HDR_Boot_t *)otaCtrl.hdr;

                            otaCtrl.offset = otaCtrl.size;
                            otaCtrl.size   = pimg_hdr_bl->hdrV1.bootSize;
                            otaCtrl.skip   = 1;
                            otaCtrl.state  = OTA_STATE_PLD;
                            OTA_LOG_INF("\nbootloader img");
                        }
                    }

                } else {
                    /* magic error, the remaining data is not image and skip it */
                    // otaCtrl.state = OTA_STATE_DONE;
                    otaCtrl.state = (otaCtrl.pre_offset >= IMG_HDR_SIZE
                                     ? OTA_STATE_PADDING : OTA_STATE_ERROR);

                    OTA_LOG_ERR("\nmagic error (magic %08X)", pimg_hdr->magic);
                }
            }
            break;


        case OTA_STATE_PLD:
            if (otaCtrl.offset < otaCtrl.size) {
                if (otaCtrl.skip) {
                    req_len  = otaCtrl.size      - otaCtrl.offset;
                    data_len = otaCtrl.data_size - otaCtrl.data_offset;
                    if (req_len > data_len) {
                        req_len = data_len;
                    }
                    otaCtrl.data_offset += req_len;
                    otaCtrl.offset      += req_len;
                } else {
                    if (otaCtrl.erase_addr <= otaCtrl.write_addr) {
                        if ((otaCtrl.erase_addr + OTA_FLASH_ERASE_SIZE) > otaCtrl.max_addr) {
                            OTA_LOG_ERR("\nupgrade area size less image size");
                            return -5;
                        }

                        status = OTA_FLASH_Erase(otaCtrl.erase_addr, OTA_FLASH_ERASE_SIZE);

                        if (status != 0) {
                            OTA_LOG_ERR("\nerase flash error(addr(0x%08X) "
                                        "len(%d)", otaCtrl.erase_addr,
                                                     OTA_FLASH_ERASE_SIZE);
                            return -6;
                        }
                        otaCtrl.erase_addr += OTA_FLASH_ERASE_SIZE;
                    }

                    if (otaCtrl.hdr_offset < otaCtrl.hdr_size) {
                        req_len = otaCtrl.hdr_size   - otaCtrl.hdr_offset;
                        wr_len  = otaCtrl.erase_addr - otaCtrl.write_addr;
                        if (req_len > wr_len) {
                            req_len = wr_len;
                        }

                        status = OTA_FLASH_Save(otaCtrl.write_addr,
                                   &otaCtrl.hdr[otaCtrl.hdr_offset],
                                   req_len);
                        if (status != 0) {
                            OTA_LOG_ERR("\nsave flash error(addr(0x%08X) "
                                        "len(%d)", otaCtrl.write_addr,
                                                     req_len);
                            return -7;
                        }
                        otaCtrl.write_addr += req_len;
                        otaCtrl.hdr_offset += req_len;
                    } else {
                        req_len = otaCtrl.size - otaCtrl.offset;
                        data_len = otaCtrl.data_size - otaCtrl.data_offset;
                        wr_len = otaCtrl.erase_addr - otaCtrl.write_addr;
                        if (req_len > data_len) {
                            req_len = data_len;
                        }
                        if (req_len > wr_len) {
                            req_len = wr_len;
                        }

                        status = OTA_FLASH_Save(otaCtrl.write_addr,
                                                     &pdata[otaCtrl.data_offset],
                                                     req_len);
                        if (status != 0) {
                            OTA_LOG_ERR("\nsave flash error(addr(0x%08X) "
                                        "len(%d)", otaCtrl.write_addr,
                                                     req_len);
                            return -8;
                        }
                        otaCtrl.write_addr  += req_len;
                        otaCtrl.offset      += req_len;
                        otaCtrl.data_offset += req_len;
                    }
                }
            } else {
                otaCtrl.size   = IMG_HDR_SIZE;
                otaCtrl.offset = 0;
                otaCtrl.state  = OTA_STATE_HDR;
            }
            break;

        case OTA_STATE_PADDING:
            otaCtrl.data_size   = 0;
            otaCtrl.data_offset = 0;
            status = 0;
            break;


        case OTA_STATE_DONE:
            /* Not support */
            break;

        case OTA_STATE_ERROR:
            /* skip the remaining data, until new image(offset == 0) */
            status = -10;
            break;

        default:
            OTA_LOG_ERR("\nunsupport state (%d)", otaCtrl.state);
            break;
        }
    } while ((status == 0) && (otaCtrl.data_offset < otaCtrl.data_size));

    if (status == 0) {
        /* record Pre offset compare next offset */
        otaCtrl.pre_offset = offset;
        otaCtrl.cur_offset += len;
    }


    return status;
}


int OTA_VerifyImage(void)
{
    int status = OTA_STATUS_SUCCESS;
    OTA_SlotArea_t *pslot_area;


    pslot_area = ota_GetUpgradeArea();
    if (pslot_area == NULL) {
        OTA_LOG_ERR("get upgrade area fail");
        return -2;
    }

    status = ota_IMG_VerifyAll(pslot_area->off, pslot_area->off + pslot_area->size);
    if (status == OTA_STATUS_SUCCESS) {
        otaVerify = OTA_TRUE;
    } else {
        otaVerify = OTA_FALSE;
    }


    return status;
}


int OTA_RequestUpgrade(const OTA_UpgradeMode_t mode)
{
    int status = OTA_STATUS_SUCCESS;
    OTA_SlotState_t slotState;
    OTA_SlotId_t slotId;
    uint32_t seqNum;

    OTA_LOG_INF("OTA_RequestUpgrade");


    slotId = ota_GetUpgradeSlotId();
    if (slotId == OTA_SLOT_IDX_NULL) {
        OTA_LOG_ERR("get upgrade slot fail");
        return OTA_STATUS_NOT_READY;
    }

    /* get upgrade state */
    status = ota_SLOT_GetState(slotId, &slotState);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("get current slot(%d) state err(%d)", slotId, status);
        return status;
    }

    /* check upgrade state 
     * magic == OTA_FLAG_SET
     *              new-perm    new-test
     * old-perm     success     fail
     * old-test     run         success
     */
    if (slotState.magic == OTA_FLAG_SET) {
        if (slotState.perm == OTA_FLAG_SET) {
            if (mode == OTA_UPGRADE_MODE_TEST) {
                OTA_LOG_ERR("upgrade slot already in test mode");
                return OTA_STATUS_NOT_READY;
            } else {
                OTA_LOG_ERR("upgrade slot already complete");
                return OTA_STATUS_SUCCESS;
            }
        } else {
            if (mode == OTA_UPGRADE_MODE_TEST) {
                OTA_LOG_ERR("upgrade slot already complete");
                return OTA_STATUS_SUCCESS;
            }
        }
    }

    /* must verify */
    if (otaVerify != OTA_TRUE) {
        status = OTA_VerifyImage();
        if (status != OTA_STATUS_SUCCESS) {
            return status;
        }
    }

    /* clear upgrade state */
    status = ota_SLOT_ClearState(slotId);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("clear upgrade slot(%d) state err(%d)", slotId, status);
        return status;
    }

    /* get current slot state */
    status = ota_SLOT_GetState(ota_SLOT_GetOtherId(slotId), &slotState);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("get current slot(%d) state err(%d)", slotId, status);
        return status;
    }

    /* calc upgrade slot seq num */
    if (slotState.magic == OTA_FLAG_SET) {
        seqNum = slotState.seq_num + 1;
    } else {
        seqNum = 1;
    }

    slotState.magic = OTA_FLAG_SET;
    slotState.seq_num = seqNum;
    slotState.done = OTA_FLAG_UNSET;
    if (mode == OTA_UPGRADE_MODE_TEST) {
        slotState.perm = OTA_FLAG_UNSET;
    } else {
        slotState.perm = OTA_FLAG_SET;
    }

    status = ota_SLOT_SetState(slotId, &slotState);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("set current slot(%d) state err(%d)", slotId, status);
        return status;
    }

    /* change slot magic to ota */
    status = ota_SLOT_EnableUpgrade();


    return OTA_STATUS_SUCCESS;
}


int OTA_ConfirmImage(void)
{
    int status = OTA_STATUS_SUCCESS;
    OTA_SlotState_t slotState;
    OTA_SlotId_t slotId;


    OTA_LOG_INF("OTA_ConfirmImage");

    /* get current slot */
    slotId = ota_GetUpgradeSlotId();
    if (slotId == OTA_SLOT_IDX_NULL) {
        return OTA_STATUS_NOT_READY;
    }

    slotId = ota_SLOT_GetOtherId(slotId);
    status = ota_SLOT_GetState(slotId, &slotState);
    if (status != OTA_STATUS_SUCCESS) {
        return status;
    }

    if (slotState.magic != OTA_FLAG_SET) {
        return OTA_STATUS_NOT_READY;
    }

    if (slotState.done != OTA_FLAG_SET) {
        return OTA_STATUS_NOT_READY;
    }

    if (slotState.perm == OTA_FLAG_SET) {
        return OTA_STATUS_SUCCESS;
    }


    return ota_SLOT_SetPermState(slotId);
}


int OTA_ImageIsPermanent(void)
{
    int status = OTA_STATUS_SUCCESS;
    OTA_SlotState_t slotState;
    OTA_SlotId_t slotId;


    slotId = ota_GetUpgradeSlotId();
    if (slotId == OTA_SLOT_IDX_NULL) {
        return OTA_STATUS_NOT_READY;
    }

    slotId = ota_SLOT_GetOtherId(slotId);
    status = ota_SLOT_GetState(slotId, &slotState);
    if (status != OTA_STATUS_SUCCESS) {
        return OTA_STATUS_NOT_READY;
    }

    if ((slotState.magic == OTA_FLAG_SET) &&
        (slotState.perm == OTA_FLAG_SET)) {
        return OTA_TRUE;
    } else {
        return OTA_FALSE;
    }
}



static OTA_SlotId_t ota_GetUpgradeSlotId(void)
{
    int status = OTA_STATUS_SUCCESS;

    if ((otaUpgradeSlotId != OTA_SLOT_IDX_0) &&
        (otaUpgradeSlotId != OTA_SLOT_IDX_1)) {
        status = ota_SLOT_Init();
        if (status != OTA_STATUS_SUCCESS) {
            otaUpgradeSlotId = OTA_SLOT_IDX_NULL;
            OTA_LOG_ERR("slot init fail(%d)", status);
        } else {
            otaUpgradeSlotId = ota_SLOT_GetOtherId(ota_SLOT_GetCurrentId());
        }
    }

    return otaUpgradeSlotId;
}


static OTA_SlotArea_t * ota_GetUpgradeArea(void)
{
    uint8_t slot = 0;

    slot = ota_GetUpgradeSlotId();
    if (slot == OTA_SLOT_IDX_NULL) {
        OTA_LOG_ERR("get upgrade slot fail");
        return NULL;
    }

    return ota_SLOT_GetArea(slot);
}



#endif  /* CONFIG_OTA == 1 */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
