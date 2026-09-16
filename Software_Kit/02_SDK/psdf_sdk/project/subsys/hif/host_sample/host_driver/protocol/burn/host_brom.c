/**
 **************************************************************************************************
 * @file    host_brom.c
 * @brief   host brom function file.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "host_brom.h"

#include "../llc/host_llc.h"

#include "host_brom_comm_struct.h"

extern void host_burn_set_buffer_get_size(uint32_t size);  // from host_burn.h


/* Macro.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_BURN_BROM_WRITE_RETRY_TIMES_MAX        3
#define HOST_BURN_BROM_READ_RETRY_TIMES_MAX         3

/* Typedef.
 * ------------------------------------------------------------------------------------------------
 */
/* Const.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_BROM_FLASH_ERASE_CHIP                  0
#define HOST_BROM_FLASH_ERASE_4KB                   1
#define HOST_BROM_FLASH_ERASE_32KB                  2
#define HOST_BROM_FLASH_ERASE_64KB                  3

#define HOST_BROM_FLASH_ERASE_4KB_MSK               (0xFFF)
#define HOST_BROM_FLASH_ERASE_32KB_MSK              (0x7FFF)
#define HOST_BROM_FLASH_ERASE_64KB_MSK              (0xFFFF)

#define HOST_BROM_FLASH_PGAE_MSK                    (0xFF)
#define HOST_BROM_FLASH_PAGE_SIZE                   (256)

#define HOST_BROM_RUN_ADDRESS_MAX                   (0x20000000)

const uint8_t host_burn_magic[IH_MAGIC_SIZE] = {IH_MAGIC0, IH_MAGIC1, IH_MAGIC2, IH_MAGIC3};


/* Local Variable.
 * ------------------------------------------------------------------------------------------------
 */
static host_brom_protocol_com_write_t com_write = NULL;
static host_brom_protocol_com_read_t com_read = NULL;


/* Local Function Declaration.
 * ------------------------------------------------------------------------------------------------
 */
static int host_brom_protocol_com_write(void *data, uint32_t len)
{
    if (com_write != NULL) {
        return com_write(data, len);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
}


static int host_brom_protocol_com_read(void *buffer, uint32_t size)
{
    if (com_read != NULL) {
        return com_read(buffer, size);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
}


static uint16_t host_brom_protocol_checksum16(uint16_t init_value, void *pbuff, uint32_t size)
{
    uint16_t checksum = init_value;
    uint8_t *pbuff8 = pbuff;
    uint16_t *pbuff16 = pbuff;
    uint32_t idx = 0;

    for (idx = 0; idx < (size >> 1); idx++) {
        checksum += pbuff16[idx];
    }

    for (idx <<= 1; idx < size; idx++) {
        checksum += pbuff8[idx];
    }

    return ~checksum;
}


static int host_brom_protocol_cmd_check(void *phdr, uint32_t hdr_size, void *pdata, uint32_t data_size)
{
    comm_hdr_t *pHdr = (comm_hdr_t *)phdr;
    cmd_memwr_t *pcmd = (cmd_memwr_t *)phdr;

    if (data_size != 0) {
        pcmd->dchecksum = ~(host_brom_protocol_checksum16(0, pdata, data_size));
    }

#if (CFG_HOST_BURN_CHECKSUM_EN == 1)
    pHdr->flag |= FLAG_CHECKSUM;
    pHdr->checksum = host_brom_protocol_checksum16(0, phdr, hdr_size);
#endif

    return HOST_ERRCODE_SUCCESS;
}


#define HOST_BROM_RSP_ERRCODE_MAGIC       -16
#define HOST_BROM_RSP_ERRCODE_VERSION     -17
#define HOST_BROM_RSP_ERRCODE_RETRY       -18
#define HOST_BROM_RSP_ERRCODE_ERR         -19
#define HOST_BROM_RSP_ERRCODE_ACK         -20
#define HOST_BROM_RSP_ERRCODE_LEN         -21
static int host_brom_protocol_rsp_check(void *phdr, uint32_t hdr_size, void *pdata, uint32_t data_size)
{
    comm_hdr_t *pHdr = (comm_hdr_t *)phdr;

    if (hdr_size != 0) {
        if (pHdr->magic != IH_MAGIC) {
            return HOST_BROM_RSP_ERRCODE_MAGIC;
        }

        if (pHdr->version != IH_VERSION2) {
            return HOST_BROM_RSP_ERRCODE_VERSION;
        }

        if ((pHdr->flag & FLAG_RETRY) != 0) {
            return HOST_BROM_RSP_ERRCODE_RETRY;
        }

        if ((pHdr->flag & FLAG_ERR) != 0) {
            uint8_t *pbuff = phdr;
            HOST_LOG_ERR("err (%02X)\n", pbuff[COMM_HDR_SIZE]);
            return HOST_BROM_RSP_ERRCODE_ERR;
        }

        if ((pHdr->flag & FLAG_ACK) == 0) {
            return HOST_BROM_RSP_ERRCODE_ACK;
        }

        if (pHdr->plen != ((data_size + hdr_size) - COMM_HDR_SIZE)) {
            return HOST_BROM_RSP_ERRCODE_LEN;
        }

        #if (CFG_HOST_BURN_CHECKSUM_EN == 1)
        if ((pHdr->flag & FLAG_CHECKSUM) != 0) {
            uint16_t cal_value = host_brom_protocol_checksum16(0, phdr, hdr_size);
            if (0 != cal_value) {
                HOST_LOG_ERR("checksum err (%04X - %04X)\n", cal_value, pHdr->checksum);
                return HOST_ERRCODE_IO_ERROR;
            }
        }
        #endif
    }


    return HOST_ERRCODE_SUCCESS;
}


static int host_brom_protocol_read_rsp_check(void *phdr, uint32_t hdr_size, void *pdata, uint32_t data_size)
{
    comm_hdr_t *pcomm = phdr;

    if (hdr_size != 0) {
        if (pcomm->magic != IH_MAGIC) {
            return HOST_BROM_RSP_ERRCODE_MAGIC;
        }

        if (pcomm->version != IH_VERSION2) {
            return HOST_BROM_RSP_ERRCODE_VERSION;
        }

        if ((pcomm->flag & FLAG_RETRY) != 0) {
            return HOST_BROM_RSP_ERRCODE_RETRY;
        }

        if ((pcomm->flag & FLAG_ERR) != 0) {
            HOST_LOG_ERR("err (%02X)\n", pcomm->flag);
            return HOST_BROM_RSP_ERRCODE_ERR;
        }

        if ((pcomm->flag & FLAG_ACK) == 0) {
            return HOST_BROM_RSP_ERRCODE_ACK;
        }

        if (pcomm->plen != ((data_size + hdr_size) - COMM_HDR_SIZE)) {
            return HOST_BROM_RSP_ERRCODE_LEN;
        }

        #if (CFG_HOST_BURN_CHECKSUM_EN == 1)
        if ((pcomm->flag & FLAG_CHECKSUM) != 0) {
            uint16_t cal_value = 0x0000;

            if (hdr_size % 2 == 0) {
                cal_value += ~host_brom_protocol_checksum16(0, phdr, hdr_size);
                cal_value += ~host_brom_protocol_checksum16(0, pdata, data_size);
                cal_value = ~cal_value;
            } else {
                cal_value += ~host_brom_protocol_checksum16(0, phdr, hdr_size);
                cal_value += ((uint16_t)((uint8_t *)pdata)[0]) << 8;
                cal_value += ~host_brom_protocol_checksum16(0, (void *)&((uint8_t *)pdata)[1], data_size - 1);
                cal_value = ~cal_value;
            }

            if (0 != cal_value) {
                HOST_LOG_ERR("checksum err (%04X - %04X)\n", cal_value, pcomm->checksum);
                return HOST_ERRCODE_IO_ERROR;
            }
        }
        #endif
    }


    return HOST_ERRCODE_SUCCESS;
}


static int host_brom_protocol_comm_write(void *phdr, uint32_t hdr_size, void *pdata, uint32_t data_size)
{
    int ret = 0;
    int status = HOST_ERRCODE_IO_ERROR;
    uint8_t *psend_hdr = phdr;
    uint8_t *psend_data = pdata;
    uint32_t send_size = 0;
    uint8_t err_retry = 0;

    /* Note:
     * Both hdr and data are optional item. and they will not exist simultaneously.
     */
    do {
        if (hdr_size > 0) {
            if (send_size < hdr_size) {
                ret = host_brom_protocol_com_write(&psend_hdr[send_size], hdr_size - send_size);
                if (ret > 0) {
                    send_size += ret;
                    if (send_size >= hdr_size) {
                        status = HOST_ERRCODE_SUCCESS;
                        HOST_LOG_DBG_HEX(phdr, hdr_size, "<-HDR");
                        break;
                    }
                } else {
                    if (++err_retry >= HOST_BURN_BROM_WRITE_RETRY_TIMES_MAX) {
                        HOST_LOG_ERR("%s:write hdr err! ret=%d\n", __func__, ret);
                        break;
                    } else {
                        host_os_delayms(10);
                    }
                }
            }
        } else if (data_size > 0) {
            if (send_size < data_size) {
                ret = host_brom_protocol_com_write(&psend_data[send_size], data_size - send_size);
                if (ret > 0) {
                    send_size += ret;
                    if (send_size >= data_size) {
                        status = HOST_ERRCODE_SUCCESS;
                        HOST_LOG_DBG_HEX(pdata, data_size, "<-PLY");
                        break;
                    }
                } else {
                    if (++err_retry >= HOST_BURN_BROM_WRITE_RETRY_TIMES_MAX) {
                        HOST_LOG_ERR("%s:write data err! ret=%d\n", __func__, ret);
                        break;
                    } else {
                        host_os_delayms(10);
                    }
                }
            }
        } else {
            HOST_LOG_ERR("send hdr and data length error\n");
            break;
        }
    } while (1);

    return status;
}


static int host_brom_protocol_comm_read(void *phdr, uint32_t hdr_size, void *pdata, uint32_t data_size)
{
    int ret = 0;
    int status = HOST_ERRCODE_IO_ERROR;
    uint8_t *precv = phdr;
    uint32_t recv_size = 0;
    uint32_t total_size = 0;
    uint8_t err_retry = 0;
    bool found = false;

    /* Note:
     * The hdr is a mandatory, while the data is an optional item.
     */
    if (data_size == 0) {
        total_size = hdr_size;
        precv = phdr;
    } else {
        total_size = hdr_size + data_size;
        precv = (uint8_t *)host_os_malloc(total_size);
        if (precv == NULL) {
            return HOST_ERRCODE_NO_BUFFER;
        }
    }

    do {
        ret = host_brom_protocol_com_read(&precv[recv_size], total_size - recv_size);
        if (ret > 0) {
            recv_size += ret;
            if (!found) {
                for (uint32_t i = 0; i + 4 <= recv_size; i++) {
                    if (precv[i] == host_burn_magic[0]
                        && precv[i+1] == host_burn_magic[1]
                        && precv[i+2] == host_burn_magic[2]
                        && precv[i+3] == host_burn_magic[3]) {
                        if (i != 0) {
                            host_os_memcpy(&precv[0], &precv[i], recv_size - i);
                            recv_size -= i;
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (recv_size >= 3
                        && precv[recv_size - 3] == host_burn_magic[0]
                        && precv[recv_size - 2] == host_burn_magic[1]
                        && precv[recv_size - 1] == host_burn_magic[3]) {
                        precv[0] = host_burn_magic[0];
                        precv[1] = host_burn_magic[1];
                        precv[2] = host_burn_magic[2];
                        recv_size = 3;
                    } else if (recv_size >= 2
                               && precv[recv_size - 2] == host_burn_magic[0]
                               && precv[recv_size - 1] == host_burn_magic[1]) {
                        precv[0] = host_burn_magic[0];
                        precv[1] = host_burn_magic[1];
                        recv_size = 2;
                    } else if (recv_size >= 1
                               && precv[recv_size - 1] == host_burn_magic[0]) {
                        precv[0] = host_burn_magic[0];
                        recv_size = 1;
                    } else {
                        recv_size = 0;
                    }
                    if (++err_retry >= HOST_BURN_BROM_READ_RETRY_TIMES_MAX) {
//                        HOST_LOG_ERR("%s:check err! ret=%d\n", __func__, ret);
                        break;
                    } else {
                        host_os_delayms(10);
                    }
                }
            }
        } else {
            if (++err_retry >= HOST_BURN_BROM_READ_RETRY_TIMES_MAX) {
                HOST_LOG_ERR("%s:read err! ret=%d\n", __func__, ret);
                break;
            } else {
                host_os_delayms(10);
            }
        }
        if (recv_size == total_size) {
            if (data_size > 0) {
                host_os_memcpy(phdr, &precv[0], hdr_size);
                host_os_memcpy(pdata, &precv[hdr_size], data_size);
            }
            status = HOST_ERRCODE_SUCCESS;
            HOST_LOG_DBG_HEX(phdr, hdr_size, "HDR->");
            HOST_LOG_DBG_HEX(pdata, data_size, "PLY->");
            break;
        }
    } while (1);

    if (data_size > 0) {
        host_os_free(precv);
        precv = NULL;
    }

    return status;
}


static int host_brom_protocol_rsp_read_check(
    void *phdr, uint32_t hdr_size, void *pdata, uint32_t data_size,
    uint32_t delayms
)
{
    int status = HOST_ERRCODE_SUCCESS;

    host_os_delayms(delayms);
    for (uint32_t idx = 0; idx < CFG_HOST_BURN_RSP_RETRY_CNT; idx++) {
        status = host_brom_protocol_comm_read(phdr, hdr_size, pdata, data_size);
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        status = host_brom_protocol_rsp_check(phdr, hdr_size, pdata, data_size);
        if (status != HOST_ERRCODE_SUCCESS) {
//            extern bool host_burn_current_device_is_notify_data(void);
//            if (!host_burn_current_device_is_notify_data()) {
                host_os_delayms(delayms);
//            }
            continue;
        } else {
            break;
        }
    }

    return status;
}


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
int host_brom_protocol_init(host_brom_protocol_com_write_t write, host_brom_protocol_com_read_t read)
{
    if (write == NULL || read == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    com_write = write;
    com_read  = read;

    return HOST_ERRCODE_SUCCESS;
}


void host_brom_protocol_deinit(void)
{
    com_write = NULL;
    com_read  = NULL;
}


#if (CFG_HOST_BURN_FLASH_EN)
static int host_brom_protocol_flash_id_get_cmd(void)
{
    cmd_flashid_t cmd_buff = {
        .hdr = {
            .magic      = IH_MAGIC,
            .version    = IH_VERSION2,
            .flag       = 0,
            .checksum   = 0,
            .plen       = CMD_FLASHID_EXT_SIZE,
        },
        .cmdid          = CMD_ID_FLASHID,
    };

    host_brom_protocol_cmd_check(&cmd_buff, CMD_FLASHID_SIZE, NULL, 0);

    host_burn_set_buffer_get_size(RSP_FLASHID_SIZE);

    return host_brom_protocol_comm_write(&cmd_buff, CMD_FLASHID_SIZE, NULL, 0);
}


static int host_brom_protocol_flash_id_get_rsp(uint32_t *flash_id)
{
    int status = HOST_ERRCODE_SUCCESS;

    _ALIGNAS_VARIABLE rsp_flashid_t rsp_buff;

    status = host_brom_protocol_rsp_read_check(&rsp_buff, RSP_FLASHID_SIZE, NULL, 0, 10);

    if (status == HOST_ERRCODE_SUCCESS) {
        *flash_id = (((rsp_buff.flash_id & 0xFF000000) >> 24) |
                     ((rsp_buff.flash_id & 0x00FF0000) >> 8 ) |
                     ((rsp_buff.flash_id & 0x0000FF00) << 8 ) |
                     ((rsp_buff.flash_id & 0x000000FF) << 24));
    } else {
        *flash_id = 0;
    }

    return status;
}


int host_brom_protocol_flash_id_get(uint32_t *flash_id)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (flash_id == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    for (uint32_t idx = 0; idx < CFG_HOST_BURN_RETRY_CNT; idx++) {
        status = host_brom_protocol_flash_id_get_cmd();
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        status = host_brom_protocol_flash_id_get_rsp(flash_id);
        if (status == HOST_ERRCODE_SUCCESS) {
            break;
        }
    }

    if (status != HOST_ERRCODE_SUCCESS) {
        *flash_id = 0;
    }

    return status;
}


static int host_brom_protocol_flash_erase_cmd(uint32_t addr_start, uint8_t erase_type)
{
    cmd_flashce_t cmd_buff = {
        .hdr = {
            .magic      = IH_MAGIC,
            .version    = IH_VERSION2,
            .flag       = 0,
            .checksum   = 0,
            .plen       = CMD_FLASHCE_EXT_SIZE,
        },
        .cmdid          = CMD_ID_FLASHCE,
        .ce_cmd         = erase_type,
        .addr           = addr_start,
    };

    host_brom_protocol_cmd_check(&cmd_buff, CMD_FLASHCE_SIZE, NULL, 0);

    host_burn_set_buffer_get_size(RSP_ACK_SIZE);

    return host_brom_protocol_comm_write(&cmd_buff, CMD_FLASHCE_SIZE, NULL, 0);
}


static int host_brom_protocol_flash_erase_rsp(void)
{
    _ALIGNAS_VARIABLE rsp_ack_t rsp_buff;

    return host_brom_protocol_rsp_read_check(&rsp_buff, RSP_ACK_SIZE, NULL, 0, 60);
}


int host_brom_protocol_flash_erase(uint32_t addr_start, uint32_t erase_type)
{
    int status = HOST_ERRCODE_SUCCESS;

    switch (erase_type) {
    case HOST_BROM_FLASH_ERASE_4KB:
        if (addr_start & HOST_BROM_FLASH_ERASE_4KB_MSK) {
            status = HOST_ERRCODE_INVALID_PARAM;
        }
        break;

    case HOST_BROM_FLASH_ERASE_32KB:
        if (addr_start & HOST_BROM_FLASH_ERASE_32KB_MSK) {
            status = HOST_ERRCODE_INVALID_PARAM;
        }
        break;

    case HOST_BROM_FLASH_ERASE_64KB:
        if (addr_start & HOST_BROM_FLASH_ERASE_64KB_MSK) {
            status = HOST_ERRCODE_INVALID_PARAM;
        }
        break;

    default:
        status = HOST_ERRCODE_UNSUPPORT;
        break;
    }

    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    for (uint32_t idx = 0; idx < CFG_HOST_BURN_RETRY_CNT; idx++) {
        status = host_brom_protocol_flash_erase_cmd(addr_start, erase_type);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("erase cmd send fail(%d)\n", status);
            continue;
        }

        status = host_brom_protocol_flash_erase_rsp();
        if (status == HOST_ERRCODE_SUCCESS) {
            break;
        }
    }

    return status;
}


static int host_brom_protocol_flash_write_cmd(uint32_t addr_start, uint32_t page_num, void* pdata)
{
    cmd_flashwr_t cmd_buff = {
        .hdr = {
            .magic      = IH_MAGIC,
            .version    = IH_VERSION2,
            .flag       = 0,
            .checksum   = 0,
            .plen       = CMD_FLASHWR_EXT_SIZE,
        },
        .cmdid          = CMD_ID_FLASHPRG,
        .paddr          = addr_start,
        .pnum           = page_num,
        .dchecksum      = 0,
    };

    host_brom_protocol_cmd_check(&cmd_buff, CMD_FLASHWR_SIZE, pdata, HOST_BROM_FLASH_PAGE_SIZE * page_num);

    host_burn_set_buffer_get_size(RSP_ACK_SIZE);

    return host_brom_protocol_comm_write(&cmd_buff, CMD_FLASHWR_SIZE, NULL, 0);
}


static int host_brom_protocol_flash_write_rsp(void)
{
    _ALIGNAS_VARIABLE rsp_ack_t rsp_buff;

    return host_brom_protocol_rsp_read_check(&rsp_buff, RSP_ACK_SIZE, NULL, 0, 50);
}


int host_brom_protocol_flash_write(uint32_t addr_start, uint32_t page_num, void *pdata)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (pdata == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    /* data len must less 16KB */
    if ((page_num == 0) || (page_num > 64)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    /* 256 byte align */
    if (addr_start & HOST_BROM_FLASH_PGAE_MSK) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    for (uint32_t idx = 0; idx < CFG_HOST_BURN_RETRY_CNT; idx++) {
        status = host_brom_protocol_flash_write_cmd(addr_start, page_num, pdata);
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        status = host_brom_protocol_flash_write_rsp();
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        host_os_delayms(40);

        /* handler data */
        status = host_brom_protocol_comm_write(NULL, 0, pdata, HOST_BROM_FLASH_PAGE_SIZE * page_num);
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        status = host_brom_protocol_flash_write_rsp();
        if (status == HOST_ERRCODE_SUCCESS) {
            break;
        }

        host_os_delayms(25);
    }

    return status;
}


#if (CFG_HOST_BURN_FLASH_READ_EN)
static int host_brom_protocol_flash_read_cmd(uint32_t addr_start, uint32_t page_num)
{
    cmd_flashrd_t cmd_buff = {
        .hdr = {
            .magic      = IH_MAGIC,
            .version    = IH_VERSION2,
            .flag       = 0,
            .checksum   = 0,
            .plen       = CMD_FLASHRD_EXT_SIZE,
        },
        .cmdid          = CMD_ID_FLASHRD,
        .paddr          = addr_start,
        .pnum           = page_num,
    };


    host_brom_protocol_cmd_check(&cmd_buff, CMD_FLASHRD_SIZE, NULL, 0);

    host_burn_set_buffer_get_size(COMM_HDR_SIZE + (HOST_BROM_FLASH_PAGE_SIZE * page_num));

    return host_brom_protocol_comm_write(&cmd_buff, CMD_FLASHRD_SIZE, NULL, 0);
}


static int host_brom_protocol_flash_read_rsp(uint8_t *pdata, uint32_t size)
{
    int status = HOST_ERRCODE_SUCCESS;
    _ALIGNAS_VARIABLE comm_hdr_t rsp_buff;

    for (uint32_t idx = 0; idx < CFG_HOST_BURN_RSP_RETRY_CNT; idx++) {
        status = host_brom_protocol_comm_read(&rsp_buff, COMM_HDR_SIZE, pdata, size);
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        status = host_brom_protocol_read_rsp_check(&rsp_buff, COMM_HDR_SIZE, pdata, size);
        if (status != HOST_ERRCODE_SUCCESS) {
            host_os_delayms(10);
            continue;
        } else {
            break;
        }
    }

    return status;
}


int host_brom_protocol_flash_read(uint32_t addr_start, uint32_t page_num, void *pdata)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (pdata == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    /* data len must less 16KB */
    if ((page_num == 0) || (page_num > 64)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    /* 256 byte align */
    if (addr_start & 0xFF) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    for (uint32_t idx = 0; idx < CFG_HOST_BURN_RETRY_CNT; idx++) {
        status = host_brom_protocol_flash_read_cmd(addr_start, page_num);
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        status = host_brom_protocol_flash_read_rsp(pdata, HOST_BROM_FLASH_PAGE_SIZE * page_num);
        if (status == HOST_ERRCODE_SUCCESS) {
            break;
        }
    }

    return status;
}
#endif  /* CFG_HOST_BURN_FLASH_READ_EN */
#endif  /* CFG_HOST_BURN_FLASH_EN */


#if (CFG_HOST_BURN_RUN_EN)
static int host_brom_protocol_setpc_cmd(uint32_t run_addr)
{
    cmd_setpc_t cmd_buff = {
        .hdr = {
            .magic      = IH_MAGIC,
            .version    = IH_VERSION2,
            .flag       = 0,
            .checksum   = 0,
            .plen       = CMD_SETPC_EXT_SIZE,
        },
        .cmdid          = CMD_ID_SETPC,
        .val            = run_addr,
    };


    host_brom_protocol_cmd_check(&cmd_buff, CMD_SETPC_SIZE, NULL, 0);

    host_burn_set_buffer_get_size(RSP_ACK_SIZE);

    return host_brom_protocol_comm_write(&cmd_buff, CMD_SETPC_SIZE, NULL, 0);
}


static int host_brom_protocol_setpc_rsp(void)
{
    _ALIGNAS_VARIABLE rsp_ack_t rsp_buff;

    return host_brom_protocol_rsp_read_check(&rsp_buff, RSP_ACK_SIZE, NULL, 0, 20);
}


int host_brom_protocol_setpc(uint32_t run_addr)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (run_addr > HOST_BROM_RUN_ADDRESS_MAX) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    for (uint32_t idx = 0; idx < CFG_HOST_BURN_RETRY_CNT; idx++) {
        status = host_brom_protocol_setpc_cmd(run_addr);
        if (status != HOST_ERRCODE_SUCCESS) {
            continue;
        }

        status = host_brom_protocol_setpc_rsp();
        if (status == HOST_ERRCODE_SUCCESS) {
            break;
        }
    }

    return status;
}
#endif  /* CFG_HOST_BURN_RUN_EN */


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
