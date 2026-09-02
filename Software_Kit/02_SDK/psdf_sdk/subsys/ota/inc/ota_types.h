/**
 ******************************************************************************
 * @file    ota_slot.h
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

#ifndef _OTA_TYPES_H_
#define _OTA_TYPES_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Exported types.
 * ----------------------------------------------------------------------------
 */

#define OTA_STATUS_SUCCESS                  (0)  /* 0 should not change */
#define OTA_STATUS_INVALID_PARAM            (-1)
#define OTA_STATUS_NOT_READY                (-2)
#define OTA_STATUS_IO_ERROR                 (-3)
#define OTA_STATUS_MAGIC_ERROR              (-4)
#define OTA_STATUS_HASH_ERROR               (-5)
#define OTA_STATUS_DONE                     (-6)
#define OTA_STATUS_UNDEFINE                 (-7)
#define OTA_STATUS_ERROR                    (-8)

/* Exported constants.
 * ----------------------------------------------------------------------------
 */
#ifndef OTA_TRUE
#define OTA_TRUE                    1
#endif

#ifndef OTA_FALSE
#define OTA_FALSE                   0
#endif

/* Exported functions.
 * ----------------------------------------------------------------------------
 */

#ifdef __cplusplus
}
#endif

#endif /* _OTA_TYPES_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
