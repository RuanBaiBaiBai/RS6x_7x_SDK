/**
 **************************************************************************************************
 * @file    host_brom_comm_struct.h
 * @brief   host_brom communication data struct definition.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */


/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HOST_BROM_COMM_STRUCT_H_
#define _HOST_BROM_COMM_STRUCT_H_

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
#define IH_SYNC                             (0x7E)
#define IH_SYNC_ACK                         (0x59)

/* magic "PSIC" */
#define IH_MAGIC                            (0x43495350)
#define IH_MAGIC0                           (0x50)
#define IH_MAGIC1                           (0x53)
#define IH_MAGIC2                           (0x49)
#define IH_MAGIC3                           (0x43)

#define IH_MAGIC_SIZE                       (4)

#define IH_VERSION1                         (1)
#define IH_VERSION2                         (2)

/* flag type defination */
#define FLAG_ACK                            (1U << 0)
#define FLAG_CHECKSUM                       (1U << 1)
#define FLAG_ERR                            (1U << 2)
#define FLAG_RETRY                          (1U << 3)


/* command type */
#define CMD_TYP_SYSGET                      (0x0)
#define CMD_TYP_SYSSET                      (0x1)
#define CMD_TYP_SRAM                        (0x2)
#define CMD_TYP_FLASH                       (0x3)

/* SYSGET command line */
#define CMD_ID_GETBAUD                      ((CMD_TYP_SYSGET << 4) | 0x3)

/* SYSSET command line */
#define CMD_ID_SETBAUD                      ((CMD_TYP_SYSSET << 4) | 0x0)
#define CMD_ID_SETPC                        ((CMD_TYP_SYSSET << 4) | 0x1)

/* SRAMRW command line */
#define CMD_ID_SRAMWR                       ((CMD_TYP_SRAM << 4) | 0x0)
#define CMD_ID_SRAMRD                       ((CMD_TYP_SRAM << 4) | 0x1)

/* FLASH command line */
#define CMD_ID_FLASHCE                      ((CMD_TYP_FLASH << 4) | 0x0)
#define CMD_ID_FLASHPRG                     ((CMD_TYP_FLASH << 4) | 0x1)
#define CMD_ID_FLASHRD                      ((CMD_TYP_FLASH << 4) | 0x2)
#define CMD_ID_FLASHID                      ((CMD_TYP_FLASH << 4) | 0x3)


/* command error type */
#define EXE_OK                              (0)
#define CHECKSUM_ERR                        (1)
#define CMD_ERR                             (2)
#define ADDR_ILLEGAL                        (3)
#define CMD_TBD                             (4)
#define FLASH_ERASE_ERR                     (5)
#define FLASH_PRG_ERR                       (6)
#define FLASH_RD_ERR                        (7)
#define DATA_LEN_ERR                        (8)


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __packed
#define __packed __attribute__((__packed__))
#endif


typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t flag;
    uint16_t checksum;
    uint32_t plen;
} __packed comm_hdr_t;

#define COMM_HDR_SIZE                        (sizeof(comm_hdr_t))


typedef struct {
    comm_hdr_t hdr;
    uint8_t err;
} __packed rsp_ack_t;

#define RSP_ACK_EXT_SIZE                    0//1
#define RSP_ACK_SIZE                        (COMM_HDR_SIZE + RSP_ACK_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
} __packed cmd_flashid_t;

#define CMD_FLASHID_EXT_SIZE                 1
#define CMD_FLASHID_SIZE                     (COMM_HDR_SIZE + CMD_FLASHID_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint32_t flash_id;
} __packed rsp_flashid_t;

#define RSP_FLASHID_EXT_SIZE                4
#define RSP_FLASHID_SIZE                    (COMM_HDR_SIZE + RSP_FLASHID_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
    uint32_t val;
} __packed cmd_setpc_t;

#define CMD_SETPC_EXT_SIZE                 5
#define CMD_SETPC_SIZE                     (COMM_HDR_SIZE + CMD_SETPC_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
    uint32_t paddr;
    uint32_t pnum;
    uint16_t dchecksum;
} __packed cmd_memwr_t;


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
    uint8_t ce_cmd;
    uint32_t addr;
} __packed cmd_flashce_t;

#define CMD_FLASHCE_EXT_SIZE                6
#define CMD_FLASHCE_SIZE                    (COMM_HDR_SIZE + CMD_FLASHCE_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
    uint32_t paddr;
    uint32_t pnum;
    uint16_t dchecksum;
} __packed cmd_flashwr_t;

#define CMD_FLASHWR_EXT_SIZE                11
#define CMD_FLASHWR_SIZE                    (COMM_HDR_SIZE + CMD_FLASHWR_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
    uint32_t paddr;
    uint32_t pnum;
} __packed cmd_flashrd_t;

#define CMD_FLASHRD_EXT_SIZE                9
#define CMD_FLASHRD_SIZE                    (COMM_HDR_SIZE + CMD_FLASHRD_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
    uint32_t addr;
    uint32_t dlen;
    uint16_t dchecksum;
} __packed cmd_sramwr_t;

#define CMD_SRAMWR_EXT_SIZE                 11
#define CMD_SRAMWR_SIZE                     (COMM_HDR_SIZE + CMD_SRAMWR_EXT_SIZE)


typedef struct {
    comm_hdr_t hdr;
    uint8_t cmdid;
    uint32_t addr;
    uint32_t dlen;
} __packed cmd_sramrd_t;

#define CMD_SRAMRD_EXT_SIZE                 9
#define CMD_SRAMRD_SIZE                     (COMM_HDR_SIZE + CMD_SRAMRD_EXT_SIZE)


/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */


#ifdef __cplusplus
}
#endif

#endif /* _HOST_BROM_COMM_STRUCT_H_ */


