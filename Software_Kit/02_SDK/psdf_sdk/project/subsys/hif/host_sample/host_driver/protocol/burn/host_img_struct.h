/**
 **************************************************************************************************
 * @file    host_img_struct.h
 * @brief   host image struct define.
 * @attention
 *          Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_IMG_STRUCT_H__
#define __HOST_IMG_STRUCT_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../utils/host_std_includes.h"


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
#define IMG_MAGIC                           0x43495350
#define IMG_MAGIC_NONE                      0xFFFFFFFF
#define IMG_TLV_INFO_MAGIC                  0x6907
#define IMG_TLV_PROT_INFO_MAGIC             0x6908

#define IMG_HDR_SIZE                        32
#define IMG_HDR_BROM_SIZE                   64

#define IMG_TLV_INF_SIZE                    4
#define IMG_TLV_HDR_SIZE                    4

#define IMG_HDR_VER_MSK                     0x000000FF
#define IMG_HDR_VER_APP                     0xFF

#define IMG_HDR_FLG_MI_POS                  (15)
#define IMG_HDR_FLG_MI_MSK                  (1 << IMG_HDR_FLG_MI_POS)
#define IMG_HDR_FLG_NAME_POS                (13)
#define IMG_HDR_FLG_NAME_MSK                (1 << IMG_HDR_FLG_NAME_POS)
#define IMG_HDR_FLG_RA_POS                  (11)
#define IMG_HDR_FLG_RA_MSK                  (1 << IMG_HDR_FLG_RA_POS)
#define IMG_HDR_FLG_ENC_POS                 (10)
#define IMG_HDR_FLG_ENC_MSK                 (1 << IMG_HDR_FLG_ENC_POS)
#define IMG_HDR_FLG_EP_POS                  (9)
#define IMG_HDR_FLG_EP_MSK                  (1 << IMG_HDR_FLG_EP_POS)
#define IMG_HDR_FLG_MA_POS                  (8)
#define IMG_HDR_FLG_MA_MSK                  (1 << IMG_HDR_FLG_MA_POS)
#define IMG_HDR_FLG_IMG_ID_POS              (4)
#define IMG_HDR_FLG_IMG_ID_MSK              (0xF << IMG_HDR_FLG_IMG_ID_POS)
#define IMG_HDR_FLG_CPU_ID_POS              (0)
#define IMG_HDR_FLG_CPU_ID_MSK              (0x3 << IMG_HDR_FLG_CPU_ID_POS)
#define IMG_HDR_FLG_CPU_S_POS               (0)
#define IMG_HDR_FLG_CPU_S_MSK               (1 << IMG_HDR_FLG_CPU_S_POS)
#define IMG_HDR_FLG_CPU_F_POS               (1)
#define IMG_HDR_FLG_CPU_F_MSK               (1 << IMG_HDR_FLG_CPU_F_POS)


#define IMG_TLV_ANY                         0xFFFF
#define IMG_TLV_KEYHASH                     0x01   /* hash of the public key */
#define IMG_TLV_PUBKEY                      0x02   /* public key */
#define IMG_TLV_SHA256                      0x10   /* SHA256 of image hdr and body */
#define IMG_TLV_RSA2048_PSS                 0x20   /* RSA2048 of hash output */
#define IMG_TLV_ECDSA224                    0x21   /* ECDSA of hash output - Not supported anymore */
#define IMG_TLV_ECDSA_SIG                   0x22   /* ECDSA of hash output */
#define IMG_TLV_RSA3072_PSS                 0x23   /* RSA3072 of hash output */
#define IMG_TLV_ED25519                     0x24   /* ed25519 of hash output */
#define IMG_TLV_ENC_RSA2048                 0x30   /* Key encrypted with RSA-OAEP-2048 */
#define IMG_TLV_ENC_KW                      0x31   /* Key encrypted with AES-KW 128 or 256*/
#define IMG_TLV_ENC_EC256                   0x32   /* Key encrypted with ECIES-EC256 */
#define IMG_TLV_ENC_X25519                  0x33   /* Key encrypted with ECIES-X25519 */
#define IMG_TLV_DEPENDENCY                  0x40   /* Image depends on other image */
#define IMG_TLV_SEC_CNT                     0x50   /* security counter */
#define IMG_TLV_BOOT_RECORD                 0x60   /* measured boot record */


#define IMG_TLV_NAME                        0xA1
#define IMG_TLV_REMAP_ADDR                  0xB1
#define IMG_TLV_ENTER_POINT                 0xD1
#define IMG_TLV_MOVE_ADDR                   0xD2
#define IMG_TLV_SUBIMG_LOAD                 0xF1

#define BOOT_REMAP_CPUS_SRAM_IBUS           0x11
#define BOOT_REMAP_CPUF_SRAM_IBUS           0x12
#define BOOT_REMAP_CPUSF_SRAM_IBUS          0x13
#define BOOT_REMAP_CPUS_SRAM_DBUS           0x21
#define BOOT_REMAP_CPUF_SRAM_DBUS           0x22
#define BOOT_REMAP_CPUSF_SRAM_DBUS          0x23
#define BOOT_REMAP_XIP_IBUS                 0x40
#define BOOT_REMAP_PSRAM_IBUS               0x81
#define BOOT_REMAP_PSRAM_DBUS               0x82

#define BOOT_REMAP_IBUS_BASE                0x00500000
#define BOOT_REMAP_IBUS_AREA                0x00040000
#define BOOT_REMAP_DBUS_BASE                0x10500000
#define BOOT_REMAP_DBUS_AREA                0x00040000
#define BOOT_REMAP_XIP_BASE                 0x08000000
#define BOOT_REMAP_PSRAM_BASE               0x0c000000

#define BOOT_TLV_INF_PRT_ID                 0
#define BOOT_TLV_INF_NML_ID                 1
#define BOOT_TLV_INF_NUM                    2


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef struct {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;

    uint32_t _pad1;
} img_ver_t;

typedef struct {
    uint8_t  ir_type;
    uint16_t ir_minor;

    uint16_t im_number;
    uint8_t  im_project;
    uint16_t im_minor;

    uint8_t  iv_major;
    uint8_t  iv_minor;
    uint16_t iv_revision;
} __attribute__((packed)) merge_img_ver_t;

typedef struct {
    uint32_t ih_magic;
    uint32_t ih_hdr_ver;
    uint16_t ih_hdr_size;           /* Size of image header (bytes). */
    uint16_t ih_protect_tlv_size;   /* Size of protected TLV area (bytes). */
    uint32_t ih_img_size;           /* Does not include header. */
    uint32_t ih_flags;              /* IMAGE_F_[...]. */
    union {
        img_ver_t ih_ver;
        merge_img_ver_t ih_merge_ver;
    };
} img_hdr_t;

typedef struct {
    uint32_t ih_magic;
    uint32_t ih_hdr_ver;
    uint32_t ih_hdr_crc;
    uint32_t ih_load;
    uint32_t ih_enter_point;
    uint32_t ih_data_size;
    uint32_t ih_data_crc;
    uint32_t ih_img_size;
    uint8_t  ih_type;
    uint8_t  ih_flags;
    uint16_t ih_dfu;
    uint32_t ih_next_img;
    uint32_t ih_primary_slot;
    uint32_t ih_secondary_slot;
    uint32_t ih_data_section;
    uint32_t ih_sram_base;
    uint32_t ih_sram_max;
    uint32_t ih_flash_max;
} img_hdr_brom_t;


/** Image TLV header.  All fields in little endian. */
typedef struct {
    uint16_t it_magic;
    uint16_t it_tlv_tot;  /* size of TLV area (including tlv_info header) */
} img_tlv_inf_t;


/** Image trailer TLV format. All fields in little endian. */
typedef struct {
    uint16_t it_type;   /* IMAGE_TLV_[...]. */
    uint16_t it_len;    /* Data length (not including TLV header). */
} img_tlv_hdr_t;

typedef struct {
    uint32_t addr;
    uint16_t type;
    uint16_t len;
} img_tlv_ctrl_t;

typedef struct {
    uint32_t addr;
    uint32_t len;
    uint32_t offset;
} img_tlv_inf_ctrl_t;


typedef struct {
    uint8_t  major;
    uint8_t  minor;
    uint16_t revision;
} boot_version_t;

typedef struct {
    uint32_t total_size;        /* image total size */
    uint32_t area_size;         /* Upgrade area size (Bytes) */
    boot_version_t version;     /* Current image version */
} boot_info_t;


/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
int host_get_image_info(uint32_t store_addr, boot_info_t *pinfo);


#ifdef __cplusplus
}
#endif

#endif /* __HOST_IMG_STRUCT_H__ */

