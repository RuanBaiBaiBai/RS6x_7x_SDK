/**
 ******************************************************************************
 * @file    ota_swap.c
 * @brief   ota swap define.
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

#include "ota_types.h"
#include "ota_img_struct.h"
#include "ota_sha256.h"
#include "ota_log.h"
#include "ota_utils.h"
#include "ota_slot.h"
#include "ota_img.h"
#include "ota_utils.h"


/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define OTA_STORE_START_ADDR            0
#define OTA_STORE_MIN_SIZE              0x0040000   /* 256KB */
#define OTA_STORE_MAX_SIZE              0x1000000   /* 16MB  */

#define OTA_NVM_MIN_SIZE                0x2000


#define OTA_LOCK_MAGIC_OTA              (0)
#define OTA_LOCK_MAGIC_INIT             (0x54494E49)
#define OTA_LOCK_MAGIC_LOAD             (0x44414F4C)


#define OTA_SLOT_MAGIC_SIZE             16
#define OTA_STATE_PERM_OFF              4



/* possumicbootload */
const uint8_t otaSlotMagic[OTA_SLOT_MAGIC_SIZE] = {
    0x70, 0x6F, 0x73, 0x73, 0x75, 0x6D, 0x69, 0x63,
    0x62, 0x6F, 0x6F, 0x74, 0x6C, 0x6F, 0x61, 0x64
};

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    uint32_t seq_num;
    uint8_t  perm;
    uint8_t  pad[3];
    uint8_t  done;
    uint8_t  pad1[3];
    uint8_t  magic[OTA_SLOT_MAGIC_SIZE];
} OTA_SlotInf_t;


/* Private defines.
 * ----------------------------------------------------------------------------
 */

static OTA_SlotArea_t otaSlotArea[OTA_SLOT_AREA_NUM];

static uint8_t otaSlotInitialized = OTA_FALSE;


/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int ota_SLOT_Init(void)
{
    int status = OTA_STATUS_SUCCESS;
    IMG_HDR_Boot_t bootHdr;


    OTA_LOG_DBG("slot init");
    /* read boot head from flash */
    status = OTA_FLASH_Read(OTA_STORE_START_ADDR, &bootHdr, sizeof(IMG_HDR_Boot_t));
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("read brom img header fail(%d)", status);
        return status;
    }

    /* check boot head */
    status = ota_IMG_VerifyBootHeader(&bootHdr);
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("img hdr fail(%d)", status);
        return status;
    }

    /* check ota lock state */
    uint32_t lockMagic = 0;
    uint32_t otaLockAddr = bootHdr.hdrV1.bootSize - sizeof(uint32_t);
    status = OTA_FLASH_Read(otaLockAddr, &lockMagic, sizeof(uint32_t));
    if (status != 0) {
        OTA_LOG_ERR("read lock magic err(%d)", status);
        return OTA_STATUS_IO_ERROR;
    }

    if ((lockMagic != OTA_LOCK_MAGIC_INIT) && (lockMagic != OTA_LOCK_MAGIC_OTA)) {
        OTA_LOG_ERR("ota is lock(0x%08X)", lockMagic);
        return OTA_STATUS_MAGIC_ERROR;
    }

    /* calc slot info */
    OTA_LOG_DBG("get flash size");
    if (bootHdr.hdrV1.flashSize == 0) {
        OTA_LOG_DBG("auto calc flash size");
        IMG_HDR_Boot_t scanBootHdr;

        uint32_t offset = OTA_STORE_MIN_SIZE;
        for (; offset < OTA_STORE_MAX_SIZE; ) {
            OTA_FLASH_Read(offset, &scanBootHdr, sizeof(IMG_HDR_Boot_t));
            if (memcmp(&scanBootHdr, &bootHdr, sizeof(IMG_HDR_Boot_t)) == 0) {
                bootHdr.hdrV1.flashSize = offset;
                break;
            }

            /* enlarge to double the size */
            offset <<= 1;
        }
    }

    if ((bootHdr.hdrV1.flashSize == 0) || (bootHdr.hdrV1.bootSize  == 0)) {
        OTA_LOG_ERR("flash and boot size is zero");
        return OTA_STATUS_INVALID_PARAM;
    }

    if (bootHdr.hdrV1.storeSize == 0) {
        OTA_LOG_ERR("set store size %d", OTA_NVM_MIN_SIZE);
        bootHdr.hdrV1.storeSize = OTA_NVM_MIN_SIZE;
    }

    uint32_t slot_size = (bootHdr.hdrV1.flashSize
                          - bootHdr.hdrV1.bootSize
                          - bootHdr.hdrV1.storeSize) / 2;

    otaSlotArea[OTA_SLOT_IDX_0].off  = bootHdr.hdrV1.bootSize;
    otaSlotArea[OTA_SLOT_IDX_0].size = slot_size;

    otaSlotArea[OTA_SLOT_IDX_1].off  = bootHdr.hdrV1.bootSize + slot_size;
    otaSlotArea[OTA_SLOT_IDX_1].size = slot_size;

    otaSlotInitialized = OTA_TRUE;


    OTA_LOG_INF("flash size: %dKB"  , bootHdr.hdrV1.flashSize / 1024);
    OTA_LOG_INF("sram size : %dKB"  , bootHdr.hdrV1.sramSize  / 1024);
    OTA_LOG_INF("store size: %dKB"  , bootHdr.hdrV1.storeSize / 1024);
    OTA_LOG_INF("boot size : %dKB"  , bootHdr.hdrV1.bootSize  / 1024);
    OTA_LOG_INF("kvr size  : %dKB"  , bootHdr.hdrV1.kvrSize   / 1024);
    OTA_LOG_INF("kvr addr  : 0x%08X", bootHdr.hdrV1.kvrAddr         );

    OTA_LOG_INF("slot-0: off(0x%08X), size(0x%08X %dKB)",
                otaSlotArea[OTA_SLOT_IDX_0].off,
                otaSlotArea[OTA_SLOT_IDX_0].size,
                otaSlotArea[OTA_SLOT_IDX_0].size/1024);

    OTA_LOG_INF("slot-1: off(0x%08X), size(0x%08X %dKB)",
                otaSlotArea[OTA_SLOT_IDX_1].off,
                otaSlotArea[OTA_SLOT_IDX_1].size,
                otaSlotArea[OTA_SLOT_IDX_1].size/1024);


    return status;
}


OTA_SlotId_t ota_SLOT_GetCurrentId(void)
{
    OTA_SlotId_t slotId = OTA_SLOT_IDX_NULL;

    if (otaSlotInitialized == OTA_TRUE) {
        for (uint8_t idx = 0; idx < OTA_SLOT_AREA_NUM; idx++) {
            if (OTA_SLOT_CheckCurrent(otaSlotArea[idx].off, otaSlotArea[idx].size) == OTA_TRUE) {
                slotId = idx;
                break;
            }
        }
    }

    return slotId;
}


OTA_SlotArea_t *ota_SLOT_GetArea(OTA_SlotId_t slot)
{
    if (otaSlotInitialized != OTA_TRUE) {
        return NULL;
    }

    if (slot < OTA_SLOT_AREA_NUM) {
        return &otaSlotArea[slot];
    } else {
        OTA_LOG_ERR("get slot area err, slot(%d)", slot);
        return NULL;
    }
}


int ota_SLOT_GetState(OTA_SlotId_t slot, OTA_SlotState_t *state)
{
    int status = OTA_STATUS_SUCCESS;
    OTA_SlotInf_t slot_inf;
    uint32_t off;


    if (otaSlotInitialized != OTA_TRUE) {
        return OTA_STATUS_NOT_READY;
    }


    off = otaSlotArea[slot].off + otaSlotArea[slot].size - sizeof(OTA_SlotInf_t);

    status = OTA_FLASH_Read(off, &slot_inf, sizeof(OTA_SlotInf_t));
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("read slot info err(%d), slot(%d) off(0x%08X)",
                    status, slot, off);
        return OTA_STATUS_IO_ERROR;
    }


    if (slot_inf.magic[0] == otaSlotMagic[0]) {
        state->magic = OTA_FLAG_SET;
        for (uint32_t idx = 1; idx < OTA_SLOT_MAGIC_SIZE; idx++) {
            if (slot_inf.magic[idx] != otaSlotMagic[idx]) {
                state->magic = OTA_FLAG_BAD;
                break;
            }
        }
    } else if (slot_inf.magic[0] == OTA_FLASH_ERASED_VAL) {
        state->magic = OTA_FLAG_UNSET;
        for (uint32_t idx = 1; idx < OTA_SLOT_MAGIC_SIZE; idx++) {
            if (slot_inf.magic[idx] != OTA_FLASH_ERASED_VAL) {
                state->magic = OTA_FLAG_BAD;
                break;
            }
        }
    } else {
        state->magic = OTA_FLAG_BAD;
    }


    if (slot_inf.done == OTA_FLASH_ERASED_VAL) {
        state->done = OTA_FLAG_UNSET;
    } else {
        if (slot_inf.done != OTA_FLAG_SET_VAL) {
            state->done = OTA_FLAG_BAD;
        } else {
            state->done = OTA_FLAG_SET;
        }
    }

    if (slot_inf.perm == OTA_FLASH_ERASED_VAL) {
        state->perm = OTA_FLAG_UNSET;
    } else {
        if (slot_inf.perm != OTA_FLAG_SET_VAL) {
            state->perm = OTA_FLAG_BAD;
        } else {
            state->perm = OTA_FLAG_SET;
        }
    }

    state->seq_num = slot_inf.seq_num;


    const char *flagStateStr[5] = {"ERR", "SET", "BAD", "UNSET", "ANY"};

    OTA_LOG_INF("slot: id(%d) off(0x%08X) size(%d KB)", slot,
                                            otaSlotArea[slot].off,
                                            otaSlotArea[slot].size /11024);
    OTA_LOG_INF("magic: %s(%d)", flagStateStr[state->magic], state->magic);
    OTA_LOG_INF_HEX((uint8_t *)&slot_inf.magic, OTA_SLOT_MAGIC_SIZE, "magic hex:");
    OTA_LOG_INF("perm: %s(%d, 0x%02X)", flagStateStr[state->perm],
                                          state->perm,
                                          slot_inf.perm);
    OTA_LOG_INF("done: %s(%d, 0x%02X)", flagStateStr[state->done],
                                          state->done,
                                          slot_inf.done);
    OTA_LOG_INF("seq_num: 0x%08X\n", state->seq_num);


    return status;
}


int ota_SLOT_SetState(OTA_SlotId_t slot, OTA_SlotState_t *state)
{
    int status = OTA_STATUS_SUCCESS;
    OTA_SlotInf_t slot_inf;
    uint32_t off;


    if (otaSlotInitialized != OTA_TRUE) {
        return OTA_STATUS_NOT_READY;
    }

    memset(&slot_inf, OTA_FLASH_ERASED_VAL, sizeof(OTA_SlotInf_t));

    for (uint8_t idx = 0; idx < OTA_SLOT_MAGIC_SIZE; idx++) {
        if (state->magic == OTA_FLAG_SET) {
            slot_inf.magic[idx] = otaSlotMagic[idx];
        } else if (state->magic == OTA_FLAG_BAD) {
            slot_inf.magic[idx] = idx;
        }
    }

    if (state->done == OTA_FLAG_SET) {
        slot_inf.done = OTA_FLAG_SET_VAL;
    } else if (state->done == OTA_FLAG_BAD) {
        slot_inf.done = 0;
    }

    if (state->perm == OTA_FLAG_SET) {
        slot_inf.perm = OTA_FLAG_SET_VAL;
    } else if (state->perm == OTA_FLAG_BAD) {
        slot_inf.perm = 0;
    }

    slot_inf.seq_num = state->seq_num;


    off = otaSlotArea[slot].off + otaSlotArea[slot].size - sizeof(OTA_SlotInf_t);

    status = OTA_FLASH_Write(off, &slot_inf, sizeof(OTA_SlotInf_t));
    if (status != OTA_STATUS_SUCCESS) {
        OTA_LOG_ERR("write slot state err(%d), slot(%d) off(0x%08X)",
                    status, slot, off);
        return OTA_STATUS_IO_ERROR;
    }


    OTA_SlotInf_t slot_inf_read;
    status = OTA_FLASH_Read(off, &slot_inf_read, sizeof(OTA_SlotInf_t));
    if (status != 0) {
        OTA_LOG_ERR("read slot state err(%d), "
                    "slot(%d) off(0x%08X)",
                    status, slot, off);
        return OTA_STATUS_IO_ERROR;
    }



    return status;
}


int ota_SLOT_ClearState(OTA_SlotId_t slot)
{
    int status = OTA_STATUS_SUCCESS;
    uint32_t off;


    if (otaSlotInitialized != OTA_TRUE) {
        return OTA_STATUS_NOT_READY;
    }


    off = otaSlotArea[slot].off + otaSlotArea[slot].size - sizeof(OTA_SlotInf_t);
    off &= ~OTA_FLASH_ERASE_SIZE_MSK;

    status = OTA_FLASH_Erase(off, OTA_FLASH_ERASE_SIZE);
    if (status != 0) {
        OTA_LOG_ERR("erase slot state err(%d), "
                    "slot(%d) off(0x%08X)", status, slot, off);
        return OTA_STATUS_IO_ERROR;
    }


    return status;
}


int ota_SLOT_SetPermState(OTA_SlotId_t slot)
{
    int status = OTA_STATUS_SUCCESS;
    uint32_t off;
    uint8_t perm = OTA_FLAG_SET_VAL;


    if (otaSlotInitialized != OTA_TRUE) {
        return OTA_STATUS_NOT_READY;
    }

    off = otaSlotArea[slot].off + otaSlotArea[slot].size - sizeof(OTA_SlotInf_t) + OTA_STATE_PERM_OFF;

    status = OTA_FLASH_Write(off, &perm, sizeof(uint8_t));
    if (status != 0) {
        return OTA_STATUS_IO_ERROR;
    }


    return OTA_STATUS_SUCCESS;
}


int ota_SLOT_EnableUpgrade(void)
{
    int status = OTA_STATUS_SUCCESS;
    uint32_t lockMagic;
    uint32_t otaLockAddr = otaSlotArea[OTA_SLOT_IDX_0].off - sizeof(uint32_t);


    if (otaSlotInitialized != OTA_TRUE) {
        return OTA_STATUS_NOT_READY;
    }


    status = OTA_FLASH_Read(otaLockAddr, &lockMagic, sizeof(uint32_t));
    if (status != 0) {
        OTA_LOG_ERR("read lock magic err(%d)", status);
        return OTA_STATUS_IO_ERROR;
    }

    if (lockMagic != OTA_LOCK_MAGIC_OTA) {
        lockMagic = OTA_LOCK_MAGIC_OTA;
        status = OTA_FLASH_Write(otaLockAddr, &lockMagic, sizeof(uint32_t));
    }


    return status;
}


#endif  /* CONFIG_OTA == 1 */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
