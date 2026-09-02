/**
 ******************************************************************************
 * @file    ota swap.h
 * @brief   ota swap define.
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

#ifndef _OTA_SWAP_H_
#define _OTA_SWAP_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "common.h"
#include "ota_config.h"

#if (CONFIG_OTA == 1)
#include "ota_types.h"
#include "ota_img_struct.h"
#include "ota_utils.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    uint32_t off;
    uint32_t size;
} OTA_SlotArea_t;

typedef struct {
    uint8_t  magic;
    uint8_t  done;
    uint8_t  perm;
    uint32_t seq_num;
} OTA_SlotState_t;

typedef enum {
    OTA_SLOT_IDX_0              = 0,
    OTA_SLOT_IDX_1              = 1,
    OTA_SLOT_IDX_NULL           = 2,
} OTA_SlotId_t;

#define OTA_SLOT_AREA_NUM       2


/* Exported constants.
 * ----------------------------------------------------------------------------
 */
#define OTA_FLAG_SET                        1
#define OTA_FLAG_BAD                        2
#define OTA_FLAG_UNSET                      3
#define OTA_FLAG_ANY                        4

#define OTA_FLAG_SET_VAL                    0x05

#define OTA_SWAP_TYPE_NONE                  1
#define OTA_SWAP_TYPE_TEST                  2
#define OTA_SWAP_TYPE_PERM                  3
#define OTA_SWAP_TYPE_REVERT                4
#define OTA_SWAP_TYPE_FAIL                  5

#define OTA_SLOT_SEQ_NUM_INVALID            0xFFFFFFFF

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

int ota_SLOT_Init(void);

OTA_SlotArea_t *ota_SLOT_GetArea(OTA_SlotId_t slot);

int ota_SLOT_GetState(OTA_SlotId_t slot, OTA_SlotState_t *state);

int ota_SLOT_SetState(OTA_SlotId_t slot, OTA_SlotState_t *state);

int ota_SLOT_ClearState(OTA_SlotId_t slot);

int ota_SLOT_SetPermState(OTA_SlotId_t slot);

OTA_SlotId_t ota_SLOT_GetCurrentId(void);

static inline OTA_SlotId_t ota_SLOT_GetOtherId(OTA_SlotId_t slot)
{
    return (slot ^ (slot != OTA_SLOT_IDX_NULL));
}

int ota_SLOT_EnableUpgrade(void);

#ifdef __cplusplus
}
#endif

#endif  /* CONFIG_OTA == 1 */

#endif  /* _OTA_SWAP_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
