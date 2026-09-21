/**
 **************************************************************************************************
 * @file    host_burn.c
 * @brief   host_burn definition.
 * @attention
 *        Copyright (c) 2024 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "host_burn.h"

#include "../llc/host_llc.h"

#ifdef HOST_BURN_PROTOCOL_BROM
#include "host_brom.h"
#endif


/* Macro.
 * ------------------------------------------------------------------------------------------------
 */
/* Const.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_BURN_DOWNLOAD_ERROR_TIMES_MAX                     100


/* Typedef.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_BURN_FLASH_EN)
typedef int (*host_burn_flash_id_get_t)(uint32_t *flash_id);
typedef int (*host_burn_flash_erase_t)(uint32_t addr_start, uint32_t erase_type);
typedef int (*host_burn_flash_write_t)(uint32_t addr_start, uint32_t page_num, void *pdata);
#if (CFG_HOST_BURN_FLASH_READ_EN)
typedef int (*host_burn_flash_read_t)(uint32_t addr_start, uint32_t page_num, void *pdata);
#endif
#endif
#if (CFG_HOST_BURN_SRAM_EN)
typedef int (*host_burn_sram_write_t)(uint32_t addr_start, uint32_t size, void *pdata);
typedef int (*host_burn_sram_read_t)(uint32_t addr_start, uint32_t size, void *pdata);
#endif
#if (CFG_HOST_BURN_RUN_EN)
typedef int (*host_burn_setpc_t)(uint32_t run_addr);
#endif

typedef struct {
#if (CFG_HOST_BURN_FLASH_EN)
    host_burn_flash_id_get_t        flash_id_get;
    host_burn_flash_erase_t         flash_erase;
    host_burn_flash_write_t         flash_write;
#if (CFG_HOST_BURN_FLASH_READ_EN)
    host_burn_flash_read_t          flash_read;
#endif
#endif
#if (CFG_HOST_BURN_SRAM_EN)
    host_burn_sram_write_t          sram_write;
    host_burn_sram_read_t           sram_read;
#endif
#if (CFG_HOST_BURN_RUN_EN)
    host_burn_setpc_t               setpc;
#endif
} host_burn_ops_t;

static host_burn_ops_t burn_ops = { NULL };


/* Local Variable.
 * ------------------------------------------------------------------------------------------------
 */
static host_os_sem_t host_burn_protocol_oper_sem = NULL;

static LLC_HANDLE llcHdl = NULL;
static host_burn_protocol_type_t llcProtocolType = HOST_BURN_PROTOCOL_NONE;
static uint8_t notifyType = 0;
static uint8_t busType = 0;

static LLC_Buffer_Get_CB bufferGetBackup = NULL;
static void *bufferGetArgBackup = NULL;

static LLC_Notify_CB notifyCbBackup = NULL;
static void *argBackup = NULL;
static uint32_t pollPeriodBackup = 0;

static void *buffer_get_buffer = NULL;
static uint32_t buffer_get_size = 0;
static bool llc_recving = false;

static uint8_t *notify_buffer = NULL;
static uint32_t notify_buffer_size = 0;
static host_os_sem_t notify_sem = NULL;


/* Local Function Declaration.
 * ------------------------------------------------------------------------------------------------
 */
static bool host_burn_is_notify_data(uint8_t type)
{
    return ((type == LLC_NOTIFY_TYPE_COM_ISR)
            || (type == LLC_NOTIFY_TYPE_COM_IRQ_THREAD));
}


bool host_burn_current_device_is_notify_data(void)
{
    return host_burn_is_notify_data(notifyType);
}


static bool host_burn_is_notify_data_arriving(uint8_t type)
{
    return ((type == LLC_NOTIFY_TYPE_COM_ISR)
            || (type == LLC_NOTIFY_TYPE_COM_IRQ_THREAD));
}


static bool host_burn_is_notify_data_arriving_if_not_recv(uint8_t type)
{
    return ((type == LLC_NOTIFY_TYPE_COM_ISR));
}


static bool host_burn_is_notify_io(uint8_t type)
{
    return ((type == LLC_NOTIFY_TYPE_EDGE));
}


static int host_burn_protocol_com_write(void *data, uint32_t len)
{
    uint32_t ret = 0;

    if (busType != LLC_BUS_TYPE_I2C) {
        ret = LLC_Send(llcHdl, data, len, 100);
    } else {
        ret = LLC_Send(llcHdl, data, len, 500);
    }

    if (ret == 0 && len != 0) {
        return HOST_ERRCODE_IO_ERROR;
    }

    return (int)ret;
}


static int host_burn_protocol_com_read(void *buffer, uint32_t size)
{
    int status = HOST_ERRCODE_SUCCESS;
    uint32_t ret = 0;

    if (host_burn_is_notify_data(notifyType)) {
        if (host_burn_is_notify_data_arriving(notifyType)) {
            if (!host_burn_is_notify_data_arriving_if_not_recv(notifyType)) {
                status = host_os_sem_take(notify_sem, 100);
                if (status != HOST_ERRCODE_SUCCESS) {
                    return status;
                }
            }
            ret = LLC_Recv(llcHdl, buffer, size, 100);
            LLC_Device_Recv_Finish(llcHdl);
        } else {
            status = host_os_sem_take(notify_sem, 100);
            if (status != HOST_ERRCODE_SUCCESS) {
                return status;
            }
            if (size < notify_buffer_size) {
                host_os_memcpy(buffer, notify_buffer, size);
                ret = size;
            } else {
                host_os_memcpy(buffer, notify_buffer, notify_buffer_size);
                ret = notify_buffer_size;
            }
            notify_buffer_size = 0;
        }
    } else {
        if (busType != LLC_BUS_TYPE_I2C) {
            ret = LLC_Recv(llcHdl, buffer, size, 100);
        } else {
            ret = LLC_Recv(llcHdl, buffer, size, 500);
        }
    }

    if (ret == 0 && size != 0) {
        return HOST_ERRCODE_IO_ERROR;
    }

    return (int)ret;
}


static void *host_burn_buffer_get(uint32_t *size, uint32_t *timeout, void *arg)
{
    void *buffer = NULL;

    llc_recving = true;

    if (size == NULL || timeout == NULL) {
        return NULL;
    }

    buffer = host_os_malloc(buffer_get_size);
    if (buffer == NULL) {
        return NULL;
    }

    *size = buffer_get_size;
    *timeout = 50;
    buffer_get_buffer = buffer;
    return buffer;
}


static void host_burn_notify_callback(uint8_t *data, uint32_t len, void *arg)
{
    if (host_burn_is_notify_data(notifyType)) {
        if (host_burn_is_notify_data_arriving(notifyType)) {
            host_os_sem_give(notify_sem);
        } else {
            if (llc_recving) {
                llc_recving = false;
                if (len < HOST_BURN_BUFFER_GET_SIZE) {
                    host_os_memcpy(notify_buffer, data, len);
                    notify_buffer_size = len;
                } else {
                    host_os_memcpy(notify_buffer, data, HOST_BURN_BUFFER_GET_SIZE);
                    notify_buffer_size = HOST_BURN_BUFFER_GET_SIZE;
                }
                if (buffer_get_buffer != NULL) {
                    host_os_free(data);
                }
                host_os_sem_give(notify_sem);
            }
        }
    }
}


void host_burn_set_buffer_get_size(uint32_t size)
{
    if (host_burn_is_notify_data(notifyType)) {
        buffer_get_size = size;
    }
}


static int host_burn_flash_id_get(uint32_t *flash_id)
{
#if (CFG_HOST_BURN_FLASH_EN)
    if (burn_ops.flash_id_get != NULL) {
        return burn_ops.flash_id_get(flash_id);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
#else
    return HOST_ERRCODE_UNSUPPORT;
#endif
}


static int host_burn_flash_erase(uint32_t addr_start, uint32_t erase_type)
{
#if (CFG_HOST_BURN_FLASH_EN)
    if (burn_ops.flash_erase != NULL) {
        return burn_ops.flash_erase(addr_start, erase_type);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
#else
    return HOST_ERRCODE_UNSUPPORT;
#endif
}


static int host_burn_flash_write(uint32_t addr_start, uint32_t page_num, void *pdata)
{
#if (CFG_HOST_BURN_FLASH_EN)
    if (burn_ops.flash_write != NULL) {
        return burn_ops.flash_write(addr_start, page_num, pdata);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
#else
    return HOST_ERRCODE_UNSUPPORT;
#endif
}


static int host_burn_flash_read(uint32_t addr_start, uint32_t page_num, void *pdata)
{
#if ((CFG_HOST_BURN_FLASH_EN) && (CFG_HOST_BURN_FLASH_READ_EN))
    if (burn_ops.flash_read != NULL) {
        return burn_ops.flash_read(addr_start, page_num, pdata);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
#else
    return HOST_ERRCODE_UNSUPPORT;
#endif
}


//static int host_burn_sram_write(uint32_t addr_start, uint32_t size, void *pdata)
//{
//#if (CFG_HOST_BURN_SRAM_EN)
//    if (burn_ops.sram_write != NULL) {
//        return burn_ops.sram_write(addr_start, size, pdata);
//    } else {
//        return HOST_ERRCODE_UNSUPPORT;
//    }
//#else
//    return HOST_ERRCODE_UNSUPPORT;
//#endif
//}
//
//
//static int host_burn_sram_read(uint32_t addr_start, uint32_t size, void *pdata)
//{
//#if (CFG_HOST_BURN_SRAM_EN)
//    if (burn_ops.sram_read != NULL) {
//        return burn_ops.sram_read(addr_start, size, pdata);
//    } else {
//        return HOST_ERRCODE_UNSUPPORT;
//    }
//#else
//    return HOST_ERRCODE_UNSUPPORT;
//#endif
//}


static int host_burn_setpc(uint32_t run_addr)
{
#if (CFG_HOST_BURN_RUN_EN)
    if (burn_ops.setpc != NULL) {
        return burn_ops.setpc(run_addr);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
#else
    return HOST_ERRCODE_UNSUPPORT;
#endif
}


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
int HOST_BURN_Init(void)
{
    if (host_burn_protocol_oper_sem == NULL) {
        host_burn_protocol_oper_sem = host_os_sem_create(1, 1);
        if (host_burn_protocol_oper_sem == NULL) {
            return HOST_ERRCODE_SYSERR;
        }
    }

    return HOST_ERRCODE_SUCCESS;
}


void HOST_BURN_DeInit(void)
{
    HOST_BURN_Protocol_DeInit();
    if (host_burn_protocol_oper_sem != NULL) {
        host_os_sem_delete(host_burn_protocol_oper_sem);
        host_burn_protocol_oper_sem = NULL;
    }
}


int HOST_BURN_Protocol_Init(LLC_HANDLE llc_hdl)
{
    int status = HOST_ERRCODE_SUCCESS;
    bool bus_right_has_been_token = false;
    host_os_critical_flag_t critical_flag;
    DevHw_t *devHwCfg = NULL;

    if (!host_os_sem_is_valid(host_burn_protocol_oper_sem)) {
        return HOST_ERRCODE_NOT_READY;
    }

    status = host_os_sem_take(host_burn_protocol_oper_sem, 1000);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (llcHdl != NULL) {
        status = HOST_ERRCODE_BUSY;
        goto exit;
    }

    if (llc_hdl == NULL) {
        status = HOST_ERRCODE_INVALID_PARAM;
        goto exit;
    }
    if (llc_hdl == llcHdl) {
        status = HOST_ERRCODE_SUCCESS;
        goto exit;
    }

    if (notify_sem != NULL || notify_buffer != NULL) {
        status = HOST_ERRCODE_STATE;
        goto exit;
    }

    status = LLC_Device_Take_Bus(llc_hdl, 1000);
    if (status != HOST_ERRCODE_SUCCESS) {
        goto exit;
    }
    bus_right_has_been_token = true;

    devHwCfg = LLC_Device_Get_DevHwCfg(llc_hdl);
    if (devHwCfg == NULL) {
        status = HOST_ERRCODE_INVALID_PARAM;
        goto exit;
    }

    notify_sem = host_os_sem_create(0, 1);
    if (notify_sem == NULL) {
        status = HOST_ERRCODE_NO_BUFFER;
        goto exit;
    }
    notify_buffer = (uint8_t *)host_os_malloc(HOST_BURN_BUFFER_GET_SIZE);
    if (notify_buffer == NULL) {
        status = HOST_ERRCODE_NO_BUFFER;
        goto exit;
    }

    // backup
    status = LLC_Notify_Type_Get(llc_hdl, &notifyType);
    if (status != HOST_ERRCODE_SUCCESS) {
        goto exit;
    }
    busType = devHwCfg->bus_type;
    if (host_burn_is_notify_data(notifyType)) {
        critical_flag = host_os_enter_critical();
        // backup
        do {
            status = LLC_Buffer_Get_Handle_Get(llc_hdl, &bufferGetBackup, &bufferGetArgBackup);
            if (status != HOST_ERRCODE_SUCCESS) {
                break;
            }
            status = LLC_Notify_Handle_Get(llc_hdl, &notifyCbBackup, &argBackup, &pollPeriodBackup);
            if (status != HOST_ERRCODE_SUCCESS) {
                break;
            }

            // replace
            status = LLC_Buffer_Get_Handle_Regist(llc_hdl, host_burn_buffer_get, NULL);
            if (status != HOST_ERRCODE_SUCCESS) {
                break;
            }
            status = LLC_Notify_Handle_Regist(llc_hdl, host_burn_notify_callback, llc_hdl, 0);
            if (status != HOST_ERRCODE_SUCCESS) {
                LLC_Buffer_Get_Handle_Regist(llc_hdl, bufferGetBackup, bufferGetArgBackup);
                break;
            }
        } while (0);
        host_os_exit_critical(critical_flag);
        if (status != HOST_ERRCODE_SUCCESS) {
            goto exit;
        }
    }

    // init
    llc_recving = false;
    buffer_get_buffer = NULL;
    host_burn_set_buffer_get_size(HOST_BURN_BUFFER_GET_SIZE);
    host_os_memset(&burn_ops, 0, sizeof(burn_ops));

    // occupy
    llcHdl = llc_hdl;
    llcProtocolType = HOST_BURN_PROTOCOL_NONE;

    host_os_sem_give(host_burn_protocol_oper_sem);

    return HOST_ERRCODE_SUCCESS;

exit:
    if (notify_buffer != NULL) {
        host_os_free(notify_buffer);
        notify_buffer = NULL;
    }
    if (notify_sem != NULL) {
        host_os_sem_delete(notify_sem);
        notify_sem = NULL;
    }
    host_os_sem_give(host_burn_protocol_oper_sem);
    if (bus_right_has_been_token) {
        LLC_Device_Release_Bus(llc_hdl);
    }
    return status;
}


void HOST_BURN_Protocol_DeInit(void)
{
    uint32_t delay = 10;
    host_os_critical_flag_t critical_flag;

    if (llcHdl != NULL) {
        // recover
        if (host_burn_is_notify_data(notifyType)) {
            critical_flag = host_os_enter_critical();

            // wait idle
            do {
                if (!llc_recving) {
                    break;
                }
                host_os_exit_critical(critical_flag);
                host_os_delayms(delay);
                critical_flag = host_os_enter_critical();
            } while (1);

            LLC_Buffer_Get_Handle_Regist(llcHdl, bufferGetBackup, bufferGetArgBackup);
            LLC_Notify_Handle_Regist(llcHdl, notifyCbBackup, argBackup, pollPeriodBackup);

            host_os_exit_critical(critical_flag);
        }

        // deinit
        llc_recving = false;
        buffer_get_buffer = NULL;
        host_burn_set_buffer_get_size(0);
        host_os_memset(&burn_ops, 0, sizeof(burn_ops));
        if (llcProtocolType == HOST_BURN_PROTOCOL_BROM) {
            host_brom_protocol_deinit();
        }

        if (notify_sem != NULL) {
            host_os_sem_delete(notify_sem);
            notify_sem = NULL;
        }
        if (notify_buffer != NULL) {
            host_os_free(notify_buffer);
            notify_buffer = NULL;
        }
        LLC_Device_Release_Bus(llcHdl);

        // release
        llcHdl = NULL;
        llcProtocolType = HOST_BURN_PROTOCOL_NONE;
    }
}


int HOST_BURN_Open_Image(void **img, void *path, uint32_t *len)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (llcHdl == NULL) {
        return HOST_ERRCODE_NOT_READY;
    }

    if (img == NULL || len == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = host_store_open(img, path, len);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


void HOST_BURN_Close_Image(void **img)
{
    if (llcHdl != NULL && img != NULL) {
        host_store_close(*img);
        *img = NULL;
    }
}


int HOST_BURN_Get_Flash_ID(uint32_t *flash_id)
{
    if (llcHdl != NULL) {
        return host_burn_flash_id_get(flash_id);
    } else {
        return HOST_ERRCODE_NOT_READY;
    }
}


int HOST_BURN_Download_Image(void *img, uint32_t len, host_burn_device_store_type_t type)
{
    int status = HOST_ERRCODE_SUCCESS;
    uint32_t err_times = 0;
    uint32_t erase_size, write_size;
    uint8_t *temp_buffer = NULL;
#if ((CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN) && (CFG_HOST_BURN_FLASH_READ_EN))
    uint8_t *temp_buffer_read = NULL;
#endif

    if (llcHdl == NULL) {
        return HOST_ERRCODE_NOT_READY;
    }

    if (img == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (type < HOST_BURN_DEVICE_STORE_MIN || type > HOST_BURN_DEVICE_STORE_MAX) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (type != HOST_BURN_DEVICE_STORE_FLASH) {
        return HOST_ERRCODE_UNSUPPORT;
    }

    temp_buffer = (uint8_t *)host_os_malloc(HOST_BURN_FLASH_WRITE_SIZE);
    if (temp_buffer == NULL) {
        return HOST_ERRCODE_NO_BUFFER;
    }
#if ((CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN) && (CFG_HOST_BURN_FLASH_READ_EN))
    temp_buffer_read = (uint8_t *)host_os_malloc(HOST_BURN_FLASH_WRITE_SIZE);
    if (temp_buffer_read == NULL) {
        host_os_free(temp_buffer);
        return HOST_ERRCODE_NO_BUFFER;
    }
#endif

    do {
        // erase
        err_times = 0;
        HOST_LOG_INF("1> erase\n");
        if ((len & HOST_BURN_FLASH_ERASE_4KB_MSK) == 0) {
            erase_size = len;
        } else {
            erase_size = len + HOST_BURN_FLASH_ERASE_4KB_SIZE;
        }
        erase_size &= ~HOST_BURN_FLASH_ERASE_4KB_MSK;
        for (uint32_t addr = 0; addr < erase_size; addr += HOST_BURN_FLASH_ERASE_4KB_SIZE) {
            HOST_LOG_INF("----[%2d %%] 0x%08X 4KB 0x%08X\n", (addr * 100) / erase_size, erase_size, addr);
            do {
                status = host_burn_flash_erase(addr, HOST_BURN_FLASH_ERASE_4KB);
                if (status != HOST_ERRCODE_SUCCESS) {
                    err_times++;
                    host_os_delayms(10);
                } else {
                    break;
                }
            } while (err_times < HOST_BURN_DOWNLOAD_ERROR_TIMES_MAX);
            if (status != HOST_ERRCODE_SUCCESS) {
                HOST_LOG_ERR("\tfail %d %08X\n", status, addr);
                break;
            } else {
                HOST_LOG_INF("\033[2k\r\033[1A\033[2k");
            }
        }
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("erase flash fail\n");
            break;
        } else {
            HOST_LOG_INF("----[100 %%] 0x%08X 4KB 0x%08X\n", erase_size, erase_size);
        }

        // download
        err_times = 0;
        HOST_LOG_INF("2> download\n");
        if ((len & HOST_BURN_FLASH_PGAE_MSK) == 0) {
            write_size = len;
        } else {
            write_size = len + HOST_BURN_FLASH_PAGE_SIZE;
        }
        write_size &= ~HOST_BURN_FLASH_PGAE_MSK;
        uint32_t read_size = HOST_BURN_FLASH_WRITE_SIZE;
        for (uint32_t addr = 0; addr < write_size; addr += read_size) {
            read_size = HOST_BURN_FLASH_WRITE_SIZE;
            uint32_t page_num  = HOST_BURN_FLASH_WRITE_SIZE / HOST_BURN_FLASH_PAGE_SIZE;
            if (addr + read_size > len) {
                read_size = len - addr;
                page_num  = (read_size + HOST_BURN_FLASH_PAGE_SIZE - 1) / HOST_BURN_FLASH_PAGE_SIZE;
                host_os_memset(temp_buffer + read_size, 0xFF, page_num * HOST_BURN_FLASH_PAGE_SIZE - read_size);
            }
            HOST_LOG_INF("----[%2d %%]  0x%08X %dKB 0x%08X\n", (addr * 100) / write_size, 
                write_size, (read_size / 1024), addr);
            status = host_store_read(img, addr, temp_buffer, read_size);
            if (status != HOST_ERRCODE_SUCCESS) {
                HOST_LOG_ERR("\n\tread fail %d %08X\n", status, addr);
                break;
            }
            do {
                status = host_burn_flash_write(addr, page_num, temp_buffer);
                if (status != HOST_ERRCODE_SUCCESS) {
                    err_times++;
                    host_os_delayms(10);
                } else {
                    break;
                }
            } while (err_times < HOST_BURN_DOWNLOAD_ERROR_TIMES_MAX);
            if (status != HOST_ERRCODE_SUCCESS) {
                HOST_LOG_ERR("\n\twrite fail %d %08X\n", status, addr);
                break;
            }
#if ((CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN) && (CFG_HOST_BURN_FLASH_READ_EN))
            if (busType == LLC_BUS_TYPE_UART) {
                do {
                    status = host_burn_flash_read(addr, page_num, temp_buffer_read);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        err_times++;
                        host_os_delayms(10);
                    } else {
                        break;
                    }
                } while (err_times < HOST_BURN_DOWNLOAD_ERROR_TIMES_MAX);
                if (status != HOST_ERRCODE_SUCCESS) {
                    HOST_LOG_ERR("\n\tread fail %d %08X\n", status, addr);
                    break;
                }
                if (host_os_memcmp(temp_buffer, temp_buffer_read, read_size)) {
                    HOST_LOG_ERR("\n\tdata not correct\n");
                    status = HOST_ERRCODE_STATE;
                    break;
                }
            }
#endif
            HOST_LOG_INF("\033[2k\r\033[1A\033[2k");
        }
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("download flash fail\n");
            break;
        } else {
            HOST_LOG_INF("----[100 %%] 0x%08X %dKB 0x%08X\n", write_size, (read_size / 1024), write_size);
        }
    } while (0);

    host_os_free(temp_buffer);
#if ((CFG_HOST_BURN_DOWNLOAD_READ_CHECK_EN) && (CFG_HOST_BURN_FLASH_READ_EN))
    host_os_free(temp_buffer_read);
#endif

    return status;
}


int HOST_BURN_Set_PC(uint32_t run_addr)
{
    return host_burn_setpc(run_addr);
}


#define DEVICE_SYNC_MAGIC0        0x7E
#define DEVICE_SYNC_MAGIC1        0x55
#define DEVICE_SYNC_RESP0         0x59
#define DEVICE_SYNC_RESP1         0x79
int HOST_BURN_Sync(uint32_t sync_cnt, host_burn_protocol_type_t *proto_type)
{
    uint8_t magic0    = DEVICE_SYNC_MAGIC0;
    uint8_t magic1[2] = { DEVICE_SYNC_MAGIC1, 0xFF };
    _ALIGNAS_VARIABLE uint8_t resp[2] = { 0 };

    if (llcHdl == NULL) {
        return HOST_ERRCODE_NOT_READY;
    }

    *proto_type = HOST_BURN_PROTOCOL_NONE;
    do {
        host_burn_set_buffer_get_size(1);
        if (host_burn_protocol_com_write(&magic0, 1) == 1) {
            host_os_delayms(10);
            host_burn_protocol_com_read(&resp[0], 1);
            if (resp[0] == DEVICE_SYNC_RESP0) {  // 25
                *proto_type = HOST_BURN_PROTOCOL_BROM;
                break;
            } else if (resp[0] == DEVICE_SYNC_RESP1) {  // 31
                host_burn_set_buffer_get_size(2);
                host_burn_protocol_com_write(&magic1[0], 2);
                host_burn_protocol_com_read(&resp[0], 2);
                if (resp[0] == DEVICE_SYNC_RESP1 || resp[1] == DEVICE_SYNC_RESP1) {
                    *proto_type = HOST_BURN_PROTOCOL_HIF;
                    break;
                }
            }
        }
    } while (--sync_cnt);
    if (sync_cnt == 0) {
        return HOST_ERRCODE_IO_ERROR;
    }

    llcProtocolType = *proto_type;

    if (llcProtocolType == HOST_BURN_PROTOCOL_BROM) {
#if (CFG_HOST_BURN_FLASH_EN)
        burn_ops.flash_id_get = host_brom_protocol_flash_id_get;
        burn_ops.flash_erase  = host_brom_protocol_flash_erase;
        burn_ops.flash_write  = host_brom_protocol_flash_write;
#if (CFG_HOST_BURN_FLASH_READ_EN)
        burn_ops.flash_read   = host_brom_protocol_flash_read;
#endif
#endif
#if (CFG_HOST_BURN_SRAM_EN)
        burn_ops.sram_write   = NULL;
        burn_ops.sram_read    = NULL;
#endif
#if (CFG_HOST_BURN_RUN_EN)
        burn_ops.setpc        = host_brom_protocol_setpc;
#endif
        host_brom_protocol_init(host_burn_protocol_com_write, host_burn_protocol_com_read);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }

    return HOST_ERRCODE_SUCCESS;
}


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */

