/**
 ******************************************************************************
 * @file    ota img struct.h
 * @brief   ota img struct define.
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

#ifndef _OTA_IMG_STRUCT_H_
#define _OTA_IMG_STRUCT_H_

/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "ota_config.h"

#if (CONFIG_OTA == 1)

#ifdef __cplusplus
extern "C" {
#endif


/* Exported constants.
 * ----------------------------------------------------------------------------
 */
/* magic define */
#define IMG_MAGIC                           0x43495350
#define IMG_MAGIC_NONE                      0xFFFFFFFF
#define IMG_MAGIC_SHORT                     0x00005350
#define IMG_MAGIC_SHORT_MSK                 0x0000FFFF

/* img header size define */
#define IMG_HDR_SIZE                        32
#define IMG_HDR_BROM_SIZE                   64
#define IMG_HDR_BROM_V1_SIZE                64
#define IMG_HDR_BROM_V2_SIZE                16

/* boot or app img version define */
#define IMG_HDR_VER_MSK                     0xFF
#define IMG_HDR_VER_APP                     0xFF

/* merge or sub img flag define */
#define IMG_HDR_FLG_MI_POS                  (15)
#define IMG_HDR_FLG_MI_MSK                  (1 << IMG_HDR_FLG_MI_POS)

/* tlv define */
#define IMG_TLV_INFO_MAGIC                  0x6907
#define IMG_TLV_PROT_INFO_MAGIC             0x6908

#define IMG_TLV_INF_SIZE                    4
#define IMG_TLV_HDR_SIZE                    4

#define IMG_TLV_ANY                         0xFFFF

#define IMG_TLV_SHA256                      0x10   /* SHA256 of image hdr and body */

/* Exported types.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    uint8_t     majorV;
    uint8_t     minorV;
    uint16_t    revisionV;
    uint32_t    buildNumV;

    uint32_t    _pad1;
} IMG_Version_t;

typedef struct {
    uint8_t     typeR;
    uint16_t    minorR;

    uint16_t    numberM;
    uint8_t     projectM;
    uint16_t    minorM;

    uint8_t     majorV;
    uint8_t     minorV;
    uint16_t    revisionV;
} __attribute__((packed)) IMG_VersionMerge_t;

typedef struct {
    uint32_t magic;
    uint32_t hdrVer;
    uint16_t hdrSize;           /* Size of image header (bytes). */
    uint16_t protectTlvSize;    /* Size of protected TLV area (bytes). */
    uint32_t imgSize;           /* Does not include header. */
    uint32_t flags;             /* IMAGE_F_[...]. */
    union {
        IMG_Version_t       ver;
        IMG_VersionMerge_t  verMerge;
    };
} IMG_HDR_t;

typedef struct {
    /* magic: 'PSIC' 0x43495350
     */
    uint32_t magic;
    uint32_t hdrVer;
    uint32_t hdrCrc;
    uint32_t load;
    uint32_t enterPoint;
    uint32_t dataSize;
    uint32_t dataCrc;
    uint32_t kvrAddr;
    uint32_t kvrSize;
    uint32_t slotSize;
    uint32_t bootSize;
    uint32_t storeSize;
    uint32_t sramSize;
    uint32_t flashSize;
    uint32_t rsv0;
    uint32_t rsv1;
} IMG_HDR_BootV1_t;

typedef struct {
    /* uint16_t magic (0x5350)
     * uint8_t  version
     * uint8_t  flag (0)
     */
    uint32_t magic;
    /* all image size, include image head payload and checksum
     */
    uint32_t imgSize;
    /* application start address
     */
    uint32_t enterPoint;
    /* uint8_t section num (2: info section and image section)
     * uint8_t first section offset (24, header 16 + checksum 4 + hmac 4)
     * uint16_t header checksum
     */
    uint32_t hdrCrc;
    /* hash32
     * include: header 16 + info section (4 + 28) + image section(8 + img len)
     */
    uint32_t imgHash;
    /* image hmac, fill 0xFFFFFFFF
     */
    uint32_t imgMac;

    /* info section header
     * uint8_t type, 1
     * uint8_t feat, 0
     * uint16_t len, 28
     */
    uint32_t infoSecHdr;

    /* info section payload (4 * 7 = 27)
     */
    uint32_t kvrAddr;
    uint32_t kvrSize;
    uint32_t slotSize;
    uint32_t bootSize;
    uint32_t storeSize;
    uint32_t sramSize;
    uint32_t flashSize;

    /* image section header
     * uint8_t type, 2
     * uint8_t feat, 0
     * uint16_t len, (bootloader image length, must align 4)
     * uint32_t loader_addr, (bootloader image load to sram address)
     */
    uint32_t imgSecHdr;
    uint32_t loadAddr;
} IMG_HDR_BootV2_t;

typedef struct {
    union {
        IMG_HDR_BootV1_t hdrV1;
        IMG_HDR_BootV2_t hdrV2;
    };
} IMG_HDR_Boot_t;

/** Image TLV header.  All fields in little endian. */
typedef struct {
    uint16_t magic;
    uint16_t len;  /* size of TLV area (including tlv_info header) */
} IMG_TLV_Info_t;


/** Image trailer TLV format. All fields in little endian. */
typedef struct {
    uint16_t type;   /* IMAGE_TLV_[...]. */
    uint16_t len;    /* Data length (not including TLV header). */
} IMG_TLV_Hdr_t;

typedef struct {
    uint32_t addr;
    uint16_t type;
    uint16_t len;
} IMG_TLV_Ctrl_t;

typedef struct {
    uint32_t addr;
    uint32_t len;
    uint32_t offset;
} IMG_TLV_InfoCtrl_t;


#ifdef __cplusplus
}
#endif

#endif  /* CONFIG_OTA == 1 */

#endif /* _OTA_IMG_STRUCT_H_ */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
