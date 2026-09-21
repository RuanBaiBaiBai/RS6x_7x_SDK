/**
 **************************************************************************************************
 * @file    host_llc.c
 * @brief   host llc layer function file.
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
#include "host_llc.h"

#include "host_llc_com_bus.h"
#include "host_llc_com_device.h"


/* Macro.
 * ------------------------------------------------------------------------------------------------
 */
#define LLC_BUFFER_GET_SIZE_DEFAULT                         (1024)


/* Typedef.
 * ------------------------------------------------------------------------------------------------
 */
typedef struct {
    host_bus_device_t *devcie;
    DevHw_t            devHw;

    uint8_t            notify_enable;  // mark notify irq able state

    // buffer get
    LLC_Buffer_Get_CB  buffer_get;
    void              *buffer_get_arg;

    // notify callback
    LLC_Notify_CB      notify_callback;
    void              *notify_arg;

    // recv mode in COM_ISR and COM_IRQ_THREAD
#define LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_NONE      0
#define LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_IDLE      1
#define LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CLEAN     2
#define LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CACHE     3
#define LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_DIRECT    4
    uint8_t            active_upload_int_mode;

    // buffer/data get
    bool               buffer_from_alloc;
    bool               is_getting_buffer;
    uint32_t           buffer_timeout;
    host_os_sem_t      buffer_complete_sem;
    uint8_t           *buffer;
    uint32_t           buffer_size;
    uint32_t           buffer_data_len;

    // cache
    uint8_t           *rx_cache;
    uint32_t           rx_cache_size;
    uint32_t           rx_cache_data_len;
    uint32_t           rx_cache_data_offset;
} LLC_HANDLE_S;


/* Local Variable.
 * ------------------------------------------------------------------------------------------------
 */
/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
LLC_LOCAL static int llc_bus_type_com_to_llc(host_bus_type_t com_bus_type, uint8_t *llc_bus_type)
{
    if (com_bus_type == HOST_BUS_TYPE_SPI) {
        *llc_bus_type = LLC_BUS_TYPE_SPI;
    } else if (com_bus_type == HOST_BUS_TYPE_I2C) {
        *llc_bus_type = LLC_BUS_TYPE_I2C;
    } else if (com_bus_type == HOST_BUS_TYPE_UART) {
        *llc_bus_type = LLC_BUS_TYPE_UART;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_bus_type_llc_to_com(uint8_t llc_bus_type, host_bus_type_t *com_bus_type)
{
    if (llc_bus_type == LLC_BUS_TYPE_SPI) {
        *com_bus_type = HOST_BUS_TYPE_SPI;
    } else if (llc_bus_type == LLC_BUS_TYPE_I2C) {
        *com_bus_type = HOST_BUS_TYPE_I2C;
    } else if (llc_bus_type == LLC_BUS_TYPE_UART) {
        *com_bus_type = HOST_BUS_TYPE_UART;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_bus_param_com_to_llc(uint8_t bus_type, uint8_t com_bus_param, uint8_t *llc_bus_param)
{
    if (bus_type == LLC_BUS_TYPE_SPI) {
        if (com_bus_param == HOST_BUS_PARAM_SPI) {
            *llc_bus_param = LLC_BUS_PARAM_SPI;
        } else if (com_bus_param == HOST_BUS_PARAM_DSPI) {
            *llc_bus_param = LLC_BUS_PARAM_DSPI;
        } else if (com_bus_param == HOST_BUS_PARAM_QSPI) {
            *llc_bus_param = LLC_BUS_PARAM_QSPI;
        } else {
            return HOST_ERRCODE_INVALID_PARAM;
        }
    } else {
        *llc_bus_param = 0;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_bus_param_llc_to_com(uint8_t bus_type, uint8_t llc_bus_param, uint8_t *com_bus_param)
{
    if (bus_type == LLC_BUS_TYPE_SPI) {
        if (llc_bus_param == LLC_BUS_PARAM_SPI) {
            *com_bus_param = HOST_BUS_PARAM_SPI;
        } else if (llc_bus_param == LLC_BUS_PARAM_DSPI) {
            *com_bus_param = HOST_BUS_PARAM_DSPI;
        } else if (llc_bus_param == LLC_BUS_PARAM_QSPI) {
            *com_bus_param = HOST_BUS_PARAM_QSPI;
        } else {
            return HOST_ERRCODE_INVALID_PARAM;
        }
    } else {
        *com_bus_param = 0;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_device_notify_type_com_to_llc(uint8_t com_notify_type, uint8_t *llc_notify_type)
{
    if (com_notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE) {
        *llc_notify_type  = LLC_NOTIFY_TYPE_EDGE;
//    } else if (com_notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL) {
//        *llc_notify_type  = LLC_NOTIFY_TYPE_LEVEL;
    } else if (com_notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR) {
        *llc_notify_type  = LLC_NOTIFY_TYPE_COM_ISR;
    } else if (com_notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD) {
        *llc_notify_type  = LLC_NOTIFY_TYPE_COM_IRQ_THREAD;
//    } else if (com_notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_DATA_USER) {
//        *llc_notify_type  = LLC_NOTIFY_TYPE_DATA_USER;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_device_notify_type_llc_to_com(uint8_t llc_notify_type, uint8_t *com_notify_type)
{
    if (llc_notify_type == LLC_NOTIFY_TYPE_EDGE) {
        *com_notify_type  = HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE;
//    } else if (llc_notify_type == LLC_NOTIFY_TYPE_LEVEL) {
//        *com_notify_type  = HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL;
    } else if (llc_notify_type == LLC_NOTIFY_TYPE_COM_ISR) {
        *com_notify_type  = HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR;
    } else if (llc_notify_type == LLC_NOTIFY_TYPE_COM_IRQ_THREAD) {
        *com_notify_type  = HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD;
//    } else if (llc_notify_type == LLC_NOTIFY_TYPE_DATA_USER) {
//        *com_notify_type  = HOST_BUS_DEVICE_NOTIFY_TYPE_DATA_USER;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_device_upload_type_com_to_llc(host_device_upload_type_t com_upload_type, uint8_t *llc_upload_type)
{
    if (com_upload_type == HOST_DEVICE_UPLOAD_TYPE_ACTIVE) {
        *llc_upload_type = LLC_UPLOAD_TYPE_ACTIVE;
    } else if (com_upload_type == HOST_DEVICE_UPLOAD_TYPE_PASSIVE) {
        *llc_upload_type = LLC_UPLOAD_TYPE_PASSIVE;
    } else if (com_upload_type == HOST_DEVICE_UPLOAD_TYPE_ACTIVE_DELAY) {
        *llc_upload_type = LLC_UPLOAD_TYPE_ACTIVE_DELAY;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_device_upload_type_llc_to_com(uint8_t llc_upload_type, host_device_upload_type_t *com_upload_type)
{
    if (llc_upload_type == LLC_UPLOAD_TYPE_ACTIVE) {
        *com_upload_type = HOST_DEVICE_UPLOAD_TYPE_ACTIVE;
    } else if (llc_upload_type == LLC_UPLOAD_TYPE_PASSIVE) {
        *com_upload_type = HOST_DEVICE_UPLOAD_TYPE_PASSIVE;
    } else if (llc_upload_type == LLC_UPLOAD_TYPE_ACTIVE_DELAY) {
        *com_upload_type = HOST_DEVICE_UPLOAD_TYPE_ACTIVE_DELAY;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_generate_virtual_id(uint16_t *virtual_id)
{
    uint32_t seed = 0;
    uint16_t id = 0;

//    if (virtual_id == NULL) {
//        return HOST_ERRCODE_INVALID_PARAM;
//    }

    // rand
    for (uint32_t i = 0; i < 1000; i++) {
        seed = (uint32_t)host_os_timestamp_us();
        seed = ((seed >> 16) + seed) & 0xFFFF;
        id = (uint16_t)(((uint32_t)(seed * 1103515245U + 12345U)) >> 16);
        if (!host_bus_exist_virtual_id(id)) {
            *virtual_id = id;
            return HOST_ERRCODE_SUCCESS;
        }
    }

    // liner
    for (uint32_t i = 0; i < 0x10000; i++) {
//        id = i & 0xFFFF;
        id = i;
        if (!host_bus_exist_virtual_id(id)) {
            *virtual_id = id;
            return HOST_ERRCODE_SUCCESS;
        }
    }

    return HOST_ERRCODE_IO_ERROR;
}


LLC_LOCAL static int llc_handle_verify_and_get(LLC_HANDLE dev, host_bus_t **bus, host_bus_device_t **device)
{
    LLC_HANDLE_S *handle = dev;

    if (dev == NULL || handle->devcie == NULL || ((host_bus_device_t *)handle->devcie)->bus == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (!host_bus_exist_device(((host_bus_device_t *)handle->devcie)->bus, handle->devcie)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (device != NULL) {
        *device = handle->devcie;
    }
    if (bus != NULL) {
        *bus = ((host_bus_device_t *)handle->devcie)->bus;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_LOCAL static int llc_write(host_bus_t *bus, host_bus_device_t *device, const uint8_t *pbuff, uint32_t size)
{
    if (bus->ops.write != NULL) {
        return bus->ops.write(bus->bus, device->dev.param, (void *)pbuff, size);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
}


// do not be used at other function if it is allowed LLC_Recv
LLC_LOCAL static int llc_read(host_bus_t *bus, host_bus_device_t *device, const uint8_t *pbuff, uint32_t size)
{
    if (bus->ops.read != NULL) {
        return bus->ops.read(bus->bus, device->dev.param, (void *)pbuff, size);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
}


LLC_LOCAL static int llc_read_finish(host_bus_t *bus, uint32_t param)
{
    if (bus->ops.read_finish != NULL) {
        return bus->ops.read_finish(bus->bus, param);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
}


LLC_LOCAL static bool llc_interrupt_ctrl_is_valid(host_bus_t *bus)
{
    return (bus->ops.interrupt_ctrl != NULL);
}


LLC_LOCAL static int llc_interrupt_ctrl(host_bus_t *bus, bool en, uint32_t size)
{
    if (bus->ops.interrupt_ctrl != NULL) {
        return bus->ops.interrupt_ctrl(bus->bus, en, size);
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
}


LLC_LOCAL static void llc_iterator_bus_tree_print(host_bus_t *bus, host_bus_device_t *device, bool is_last_bus_device)
{
    const char *bus_type = "";

    if (bus->type == HOST_BUS_TYPE_SPI) {
        bus_type = "spi";
    } else if (bus->type == HOST_BUS_TYPE_I2C) {
        bus_type = "i2c";
    } else if (bus->type == HOST_BUS_TYPE_UART) {
        bus_type = "uart";
    } else {
        bus_type = "unknow";
    }

    HOST_LOG_PRINT("[%s] %p -- %p [%05d]\n", bus_type, bus, device, device->virtual_id);
    if (is_last_bus_device) {
        HOST_LOG_PRINT("\n");
    }
}


LLC_LOCAL static void llc_iterator_bus_tree_unregist(host_bus_t *bus, host_bus_device_t *device, bool is_last_bus_device)
{
    LLC_Device_UnRegist(device);
}


LLC_LOCAL static bool llc_is_recving(LLC_HANDLE dev)
{
    LLC_HANDLE_S *handle = dev;
    host_os_critical_flag_t critical_flag;
    bool is_recving;

    if (dev == NULL) {
        return true;
    }

    critical_flag = host_os_enter_critical();
    is_recving = handle->is_getting_buffer || handle->buffer != NULL;
    host_os_exit_critical(critical_flag);

    return is_recving;
}


LLC_CALLBACK_IRQ static void llc_notify_io_irq_handle(void *arg)
{
    LLC_HANDLE_S *handle = arg;
    LLC_Notify_CB notify_cb = NULL;

    if (handle != NULL) {
        notify_cb = handle->notify_callback;
        if (notify_cb != NULL) {
            notify_cb(NULL, 0, handle->notify_arg);
        }
    }
}


LLC_CALLBACK_IRQ static void llc_interrupt_handle(void *arg)
{
    int status = HOST_ERRCODE_SUCCESS;
    LLC_HANDLE_S *handle = arg;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(arg, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("dev(%p) verify fail(%d) when handle interrupt\n", arg, status);
        return ;
    }

    if ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR) ||
        ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD) && llc_interrupt_ctrl_is_valid(bus))) {
        switch (handle->active_upload_int_mode) {
            case LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_IDLE:
                {
                    if (handle->notify_callback != NULL) {
                        // trans state
                        handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CLEAN;
                        // notify TL to recv data
                        handle->notify_callback(NULL, 0, handle->notify_arg);
                    } else {
                        HOST_LOG_ERR("[IDLE]dev(%p) not regist notify callback!\n", arg);
                        return ;
                    }
                }
            case LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CLEAN:
                {
                    // set interrupt
                    status = llc_interrupt_ctrl(bus, true, handle->rx_cache_size);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("[IDLE]dev(%p) set interrupt fail(%d)\n", arg, status);
                        return ;
                    }
                    // trans state
                    handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CACHE;
                    // clean cache to keep data
                    handle->rx_cache_data_len = 0;
                    handle->rx_cache_data_offset = 0;
                }
            case LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CACHE:
                {
                    status = llc_read(bus, device,
                                      &handle->rx_cache[handle->rx_cache_data_len],
                                      handle->rx_cache_size - handle->rx_cache_data_len);
                    if (status > 0) {
                        handle->rx_cache_data_len += status;
                    }
                    if (handle->rx_cache_data_len >= handle->rx_cache_size) {
                        handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_DIRECT;
                        status = llc_interrupt_ctrl(bus, false, 0);
                        if (status != HOST_ERRCODE_SUCCESS) {
                            HOST_LOG_ERR("[CACHE]dev(%p) disable interrupt fail(%d)\n", handle, status);
                            return ;
                        }
                    } else {
                        status = llc_interrupt_ctrl(bus, true, handle->rx_cache_size - handle->rx_cache_data_len);
                        if (status != HOST_ERRCODE_SUCCESS) {
                            HOST_LOG_ERR("[CACHE]dev(%p) set interrupt fail(%d)\n", handle, status);
                            return ;
                        }
                    }
                }
                break;
            case LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_DIRECT:
                {
                    if (handle->buffer != NULL) {
                        status = llc_read(bus, device,
                                          &handle->buffer[handle->buffer_data_len],
                                          handle->buffer_size - handle->buffer_data_len);
                        if (status > 0) {
                            handle->buffer_data_len += status;
                        }
                        if (handle->buffer_data_len >= handle->buffer_size) {
                            host_os_sem_give(handle->buffer_complete_sem);
                            // disable interrupt
                            status = llc_interrupt_ctrl(bus, false, 0);
                            if (status != HOST_ERRCODE_SUCCESS) {
                                HOST_LOG_ERR("[DIRECT]dev(%p) disable interrupt fail(%d)\n", arg, status);
                                return ;
                            }
                        } else {
                            status = llc_interrupt_ctrl(bus, true, handle->buffer_size - handle->buffer_data_len);
                            if (status != HOST_ERRCODE_SUCCESS) {
                                HOST_LOG_ERR("[DIRECT]dev(%p) enable interrupt fail(%d)\n", arg, status);
                                return ;
                            }
                        }
                    } else {
                        handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CLEAN;
                    }
                }
                break;
            default:
                HOST_LOG_ERR("[MODE ERROR]dev(%p) notify type=%d, int mode=%d, not valid!\n",
                    arg, device->dev.notify_type, handle->active_upload_int_mode);
                return ;
        }
    } else if (device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD) {
        if (handle->notify_callback != NULL && handle->rx_cache != NULL && handle->rx_cache_size > 0) {
            // recv till empty or recv fifo full
            handle->rx_cache_data_len = 0;
            handle->rx_cache_data_offset = 0;
            status = llc_read(bus, device, &handle->rx_cache[0], handle->rx_cache_size);
            if (status > 0) {
                handle->rx_cache_data_len = status;
            }
            // notify TL to recv data
            handle->notify_callback(NULL, 0, handle->notify_arg);
        } else {
            if (handle->notify_callback == NULL) {
                HOST_LOG_ERR("dev(%p) not regist notify callback!\n", arg);
            }
            if (handle->rx_cache == NULL) {
                HOST_LOG_ERR("dev(%p) not alloc rx cache!\n", arg);
            }
            if (handle->rx_cache_size <= 0) {
                HOST_LOG_ERR("dev(%p) rx cache is less or equal than 0!\n", arg);
            }
        }
    }
}


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
// from porting, defined by productor
/**
 * @brief Obtain com_ops for different buses.
 *
 * @note This function must be defined, otherwise the bus can not be controlled.
 *
 * @param type The bus type.
 *
 * @param id The bus id.
 *
 * @retval The bus com_ops.
 */
extern host_com_ops_t *porting_get_com_ops(host_bus_type_t type, uint8_t id);


void LLC_Print_Device_Tree(void)
{
    HOST_LOG_PRINT("\n==================================================\n");
    HOST_LOG_PRINT("\n                   device tree                    \n");
    HOST_LOG_PRINT("\n--------------------------------------------------\n");
    host_bus_iterator_bus_tree((void (*)(void *, void *, bool))llc_iterator_bus_tree_print);
    HOST_LOG_PRINT("\n==================================================\n");
}


int LLC_Init(void)
{
    return HOST_ERRCODE_SUCCESS;
}


int LLC_Deinit(void)
{
    int status = HOST_ERRCODE_SUCCESS;

    // clear bus tree
    status = host_bus_iterator_bus_tree((void (*)(void *, void *, bool))llc_iterator_bus_tree_unregist);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


LLC_HANDLE LLC_Device_Regist(DevHw_t *dev_cfg)
{
    int status = HOST_ERRCODE_SUCCESS;
    uint16_t virtual_id = 0;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    host_com_ops_t *com_ops = NULL;
    LLC_HANDLE_S *handle = NULL;

    host_bus_type_t bus_type;
    uint8_t bus_param;
    uint8_t notify_type;
    host_device_upload_type_t upload_type;

    if (dev_cfg == NULL) {
        return NULL;
    }

    if (HOST_ERRCODE_SUCCESS != llc_bus_type_llc_to_com(dev_cfg->bus_type, &bus_type)) {
        return NULL;
    }
    if (HOST_ERRCODE_SUCCESS != llc_bus_param_llc_to_com(dev_cfg->bus_type, dev_cfg->bus_param, &bus_param)) {
        return NULL;
    }
    if (HOST_ERRCODE_SUCCESS != llc_device_notify_type_llc_to_com(dev_cfg->notify_type, &notify_type)) {
        return NULL;
    }
    if (HOST_ERRCODE_SUCCESS != llc_device_upload_type_llc_to_com(dev_cfg->upload_type, &upload_type)) {
        return NULL;
    }

    handle = (LLC_HANDLE_S *)host_os_malloc(sizeof(LLC_HANDLE_S));
    if (handle == NULL) {
        return NULL;
    }
    host_os_memset(handle, 0, sizeof(LLC_HANDLE_S));

    // regist or find a bus
    bus = host_bus_register(bus_type, dev_cfg->bus_id, bus_param);
    if (bus == NULL) {
        goto error;
    }
    do {
        status = host_bus_set_speed(bus, dev_cfg->bus_speed);
        if (status != HOST_ERRCODE_SUCCESS) {
            if (status == HOST_ERRCODE_BUSY) {
                break;
            }
            goto error;
        }
        com_ops = porting_get_com_ops(dev_cfg->bus_type, dev_cfg->bus_id);
        if (com_ops == NULL) {
            goto error;
        }
        host_bus_set_com_ops(bus, (const host_com_ops_t *)com_ops);
    } while (0);

    // regist a device and attach it into bus
    status = llc_generate_virtual_id(&virtual_id);
    if (status != HOST_ERRCODE_SUCCESS) {
        goto error;
    }
    device = host_bus_device_register(
        bus_type, dev_cfg->bus_id, virtual_id,
        upload_type, notify_type,
        dev_cfg->upd_io, dev_cfg->rst_io, dev_cfg->notify_io,
        (host_bus_device_param_t)dev_cfg->param
    );
    if (device == NULL) {
        goto error;
    }

    handle->devcie = device;
    host_os_memcpy(&handle->devHw, dev_cfg, sizeof(handle->devHw));
    handle->notify_enable     = 0;
    handle->buffer_get        = NULL;
    handle->buffer_get_arg    = NULL;
    handle->notify_callback   = NULL;
    handle->notify_arg        = NULL;
    if (upload_type == HOST_DEVICE_UPLOAD_TYPE_PASSIVE) {
        handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_NONE;
    } else {
        handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_IDLE;
    }
    handle->buffer_from_alloc = false;
    handle->is_getting_buffer = false;
    handle->buffer_timeout    = 50;  // 50ms
    if ((handle->devHw.notify_type == LLC_NOTIFY_TYPE_COM_ISR) ||
        ((handle->devHw.notify_type == LLC_NOTIFY_TYPE_COM_IRQ_THREAD) && llc_interrupt_ctrl_is_valid(bus))) {
        handle->buffer_complete_sem = host_os_sem_create(0, 1);
        if (handle->buffer_complete_sem == NULL) {
            goto error;
        }
    } else {
        handle->buffer_complete_sem = NULL;
    }
    handle->buffer            = NULL;
    handle->buffer_size       = 0;
    handle->buffer_data_len   = 0;
    if ((handle->devHw.notify_type == LLC_NOTIFY_TYPE_COM_ISR)
        || (handle->devHw.notify_type == LLC_NOTIFY_TYPE_COM_IRQ_THREAD)) {
        handle->rx_cache      = host_os_malloc(CFG_LLC_DEVICE_RX_CACHE_SIZE);
        if (handle->rx_cache == NULL) {
            goto error;
        }
        handle->rx_cache_size = CFG_LLC_DEVICE_RX_CACHE_SIZE;
        handle->rx_cache_data_len = 0;
        handle->rx_cache_data_offset = 0;
    } else {
        handle->rx_cache      = NULL;
        handle->rx_cache_size = 0;
        handle->rx_cache_data_len = 0;
        handle->rx_cache_data_offset = 0;
    }

    return (LLC_HANDLE)handle;

error:
    if (handle->rx_cache != NULL) {
        host_os_free(handle->rx_cache);
        handle->rx_cache = NULL;
    }
    if (handle->buffer_complete_sem != NULL ){
        host_os_sem_delete(handle->buffer_complete_sem);
        handle->buffer_complete_sem = NULL;
    }
    if (device != NULL) {
        status = host_bus_device_unregister(device);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("unregist device fail(%d) in regist LLC device function\n", status);
        }
    }
    if (bus != NULL) {
        if (host_bus_get_device_num(bus) == 0) {
            status = host_bus_unregister(bus);
            if (status != HOST_ERRCODE_SUCCESS) {
                HOST_LOG_ERR("unregist bus fail(%d) in regist LLC device function\n", status);
            }
        }
    }
    if (handle != NULL) {
        host_os_free(handle);
    }

    return NULL;
}


int LLC_Device_UnRegist(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    LLC_HANDLE_S *handle = dev;

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (handle->rx_cache != NULL) {
        host_os_free(handle->rx_cache);
        handle->rx_cache = NULL;
    }

    if (handle->buffer_complete_sem != NULL ){
        host_os_sem_delete(handle->buffer_complete_sem);
        handle->buffer_complete_sem = NULL;
    }

    status = host_bus_device_unregister(device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (host_bus_get_device_num(bus) == 0) {
        host_bus_unregister(bus);
    }

    host_os_free(handle);

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Device_Open(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    // open bus first
    if (host_bus_get_device_num(bus) > 0) {
        if (host_bus_get_device_open_num(bus) == 0) {
            status = host_bus_open(bus);
            if (status != HOST_ERRCODE_SUCCESS) {
                return status;
            }
        }
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    // open device
    status = host_bus_device_open(device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Device_Close(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    // take bus
    status = LLC_Device_Take_Bus(dev, CFG_HOST_LLC_DEVICE_CLOSE_WAIT_TIME_MS);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: take bus fail(%d), timeout=%ums\n",
            __func__, status, CFG_HOST_LLC_DEVICE_CLOSE_WAIT_TIME_MS);
        return status;
    }

    // not allow
    status = host_bus_not_allow_use(bus);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: not allow fail(%d)\n", __func__, status);
        return status;
    }

    // release bus
    status = LLC_Device_Release_Bus(dev);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: release bus fail(%d)\n", __func__, status);
        return status;
    }

    // close device
    status = host_bus_device_close(device);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: close device fail(%d)\n", __func__, status);
        return status;
    }

    // close bus
    if (host_bus_get_device_open_num(bus) == 0) {
        status = host_bus_close(bus);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("%s: close bus fail(%d)\n", __func__, status);
            return status;
        }
    }

    // allow
    status = host_bus_allow_use(bus);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: allow fail(%d)\n", __func__, status);
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


DevHw_t *LLC_Device_Get_DevHwCfg(LLC_HANDLE dev)
{
    LLC_HANDLE_S *handle = dev;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    if (llc_handle_verify_and_get(dev, &bus, &device) != HOST_ERRCODE_SUCCESS) {
        return NULL;
    }

    handle->devHw.bus_speed        = bus->speed;
    if (HOST_ERRCODE_SUCCESS != llc_bus_type_com_to_llc(bus->type, &handle->devHw.bus_type)) {
        return NULL;
    }
    handle->devHw.bus_id           = bus->id;
    if (HOST_ERRCODE_SUCCESS != llc_bus_param_com_to_llc(handle->devHw.bus_type, bus->param, &handle->devHw.bus_param)) {
        return NULL;
    }
    handle->devHw.upd_io           = device->dev.upd_io;
    handle->devHw.rst_io           = device->dev.rst_io;
    handle->devHw.notify_io        = device->dev.notify_io;
    if (HOST_ERRCODE_SUCCESS != llc_device_notify_type_com_to_llc(device->dev.notify_type, &handle->devHw.notify_type)) {
        return NULL;
    }
    if (HOST_ERRCODE_SUCCESS != llc_device_upload_type_com_to_llc(device->dev.upload_type, &handle->devHw.upload_type)) {
        return NULL;
    }
    handle->devHw.param            = device->dev.param;

    return &handle->devHw;
}


int LLC_Device_Set_Bus_Param(LLC_HANDLE dev, uint8_t bus_param)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;

    status = llc_handle_verify_and_get(dev, &bus, NULL);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (bus->type != HOST_BUS_TYPE_SPI) {
        return HOST_ERRCODE_UNSUPPORT;
    }

    status = host_bus_set_param(bus, bus_param);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Device_Set_Bus_Speed(LLC_HANDLE dev, uint32_t bus_speed)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;

    status = llc_handle_verify_and_get(dev, &bus, NULL);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    status = host_bus_set_speed(bus, bus_speed);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Device_Set_Device_Param(LLC_HANDLE dev, DevParam device_param)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;
    host_bus_device_param_t pa = device_param;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    // only i2c can change device param after regist in psic
    // if other bus support it, you can delete this judgment
    if (device->type != HOST_BUS_TYPE_I2C) {
        return HOST_ERRCODE_UNSUPPORT;
    }

    status = host_bus_device_set_param(device, pa);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Device_Take_Bus(LLC_HANDLE dev, uint32_t timeout)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (bus->allow_use) {
        status = host_bus_take_right_of_use(bus, timeout);
        if (status != HOST_ERRCODE_SUCCESS) {
            return status;
        }

        status = host_bus_record_occupy(bus, device->virtual_id);
        if (status != HOST_ERRCODE_SUCCESS) {
            return status;
        }

        return HOST_ERRCODE_SUCCESS;
    } else {
        host_os_delayms(timeout < 1000 ? timeout : 1000);
        return HOST_ERRCODE_NOT_ALLOW;
    }
}


int LLC_Device_Release_Bus(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;

    status = llc_handle_verify_and_get(dev, &bus, NULL);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    status = host_bus_release_right_of_use(bus);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


uint32_t LLC_Send(LLC_HANDLE dev, const uint8_t *data, uint32_t len, uint32_t timeout)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    host_os_time_t start_time, expect_time;
    int send_num = 0;
    int temp;

    if (data == NULL || len <= 0) {
        return 0;
    }

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return 0;
    }

    start_time = host_os_timestamp_us();
    expect_time = timeout * 1000;

    do {
        while (send_num < len) {
            if (host_os_timestamp_us() - start_time < expect_time) {
                temp = llc_write(bus, device, &data[send_num], len - send_num);
                if (temp > 0) {
                    send_num += temp;
                }
            } else {
                break;
            }
        }
    } while (0);
    if (status != HOST_ERRCODE_SUCCESS) {
        return 0;
    }

    return (send_num > 0) ? ((uint32_t)send_num) : 0;
}


uint32_t LLC_Recv(LLC_HANDLE dev, uint8_t *buffer, uint32_t size, uint32_t timeout)
{
    int status = HOST_ERRCODE_SUCCESS;
    LLC_HANDLE_S *handle = dev;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    host_os_time_t start_time, expect_time;
    int recv_num = 0;
    int temp;

    if (buffer == NULL || size <= 0) {
        return 0;
    }

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return 0;
    }

    if ((device->dev.notify_type != HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE)
        && (device->dev.notify_type != HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL)
        && (device->dev.notify_type != HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR)
        && (device->dev.notify_type != HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD)) {
        return 0;
    }

    start_time = host_os_timestamp_us();
    expect_time = timeout * 1000;

    do {
        // recv rx cache first
        if ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR)
            || (device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD)) {
            if ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR) || 
                ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD) && llc_interrupt_ctrl_is_valid(bus))) {
                if (handle->active_upload_int_mode == LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CACHE) {
                    status = llc_interrupt_ctrl(bus, false, 0);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("dev(%p) 1 disable interrupt in recv fail(%d)\n", handle, status);
                    }
                    handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_DIRECT;
                }
            }
            if ((handle->rx_cache != NULL && handle->rx_cache_size > 0)
                && (handle->rx_cache_data_len > 0 && handle->rx_cache_data_len <= handle->rx_cache_size)
                && (handle->rx_cache_data_offset < handle->rx_cache_data_len)) {
                if (handle->rx_cache_data_len - handle->rx_cache_data_offset > size) {
                    host_os_memcpy(&buffer[recv_num], &handle->rx_cache[handle->rx_cache_data_offset], size);
                    recv_num += size;
                    handle->rx_cache_data_offset += size;
                } else {
                    host_os_memcpy(&buffer[recv_num],
                                   &handle->rx_cache[handle->rx_cache_data_offset],
                                   handle->rx_cache_data_len - handle->rx_cache_data_offset);
                    recv_num += handle->rx_cache_data_len - handle->rx_cache_data_offset;
                    handle->rx_cache_data_offset += handle->rx_cache_data_len - handle->rx_cache_data_offset;
                }
            }
        }
        // recv from com
        if ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR) ||
            ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD) && llc_interrupt_ctrl_is_valid(bus))) {
            while (recv_num < size) {
                if (host_os_timestamp_us() - start_time > ((host_os_timeout_t)timeout * 1000)) {
                    status = llc_interrupt_ctrl(bus, false, 0);
                    handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_CLEAN;
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("dev(%p) 2 disable interrupt in recv fail(%d)\n", handle, status);
                    }
                    break;
                }
                if (handle->buffer == NULL) {
                    handle->buffer_size = size - recv_num;
                    handle->buffer_data_len = 0;
                    handle->buffer = &buffer[recv_num];
                    status = llc_interrupt_ctrl(bus, true, handle->buffer_size);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("dev(%p) enable interrupt in recv fail(%d)\n", handle, status);
                    }
                    handle->buffer_timeout = timeout - (host_os_timestamp_us() - start_time) / 1000;
                    status = host_os_sem_take(handle->buffer_complete_sem, handle->buffer_timeout);
                    handle->buffer = NULL;
                    if (status != HOST_ERRCODE_SUCCESS) {
                        if (status == HOST_ERRCODE_TIMEOUT) {
                            status = llc_interrupt_ctrl(bus, false, 0);
                            if (status != HOST_ERRCODE_SUCCESS) {
                                HOST_LOG_ERR("dev(%p) 2 disable interrupt in recv fail(%d)\n", handle, status);
                            }
                        } else {
                            HOST_LOG_ERR("%s:wait interrupt recv data error(%d)!\n", __func__, status);
                        }
                    }
                    recv_num += handle->buffer_data_len;
                } else {
                    HOST_LOG_ERR("dev(%p) recv buffer=%p\n", handle, handle->buffer);
                }
            }
        } else {
            while (recv_num < size) {
                if (host_os_timestamp_us() - start_time < expect_time) {
                    temp = llc_read(bus, device, &buffer[recv_num], size - recv_num);
                    if (temp > 0) {
                        recv_num += temp;
                    }
                } else {
                    break;
                }
            }
        }
    } while (0);
    if (status != HOST_ERRCODE_SUCCESS) {
        return 0;
    }

    return (recv_num > 0) ? ((uint32_t)recv_num) : 0;
}


int LLC_Device_Recv_Finish(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    LLC_HANDLE_S *handle = dev;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if ((device->dev.notify_type != HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR)
        && (device->dev.notify_type != HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD)) {
        return HOST_ERRCODE_INVALID_HANDLE;
    }

    if ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR) || 
        ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD) && llc_interrupt_ctrl_is_valid(bus))) {
        handle->active_upload_int_mode = LLC_ACTIVE_UPLOAD_INTERRUPT_MODE_IDLE;
        status = llc_interrupt_ctrl(bus, true, 1);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("dev(%p) enable interrupt in recv finish fail(%d)\n", dev, status);
        }
    } else {
        status = llc_read_finish(bus, device->dev.param);
    }

    return status;
}


int LLC_Notify_Type_Get(LLC_HANDLE dev, uint8_t *type)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    if (type == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    status = llc_device_notify_type_com_to_llc(device->dev.notify_type, type);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Buffer_Get_Handle_Regist(LLC_HANDLE dev, LLC_Buffer_Get_CB buffer_get, void *arg)
{
    int status = HOST_ERRCODE_SUCCESS;
    LLC_HANDLE_S *handle = dev;

    status = llc_handle_verify_and_get(dev, NULL, NULL);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (llc_is_recving(dev)) {
        return HOST_ERRCODE_BUSY;
    }

    handle->buffer_get = buffer_get;
    handle->buffer_get_arg = arg;

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Buffer_Get_Handle_Get(LLC_HANDLE dev, LLC_Buffer_Get_CB *buffer_get, void **arg)
{
    int status = HOST_ERRCODE_SUCCESS;
    LLC_HANDLE_S *handle = dev;

    status = llc_handle_verify_and_get(dev, NULL, NULL);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (buffer_get != NULL) {
        *buffer_get = handle->buffer_get;
    }
    if (arg != NULL) {
        *arg = handle->buffer_get_arg;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Notify_Handle_Regist(LLC_HANDLE dev, LLC_Notify_CB cb, void *arg, uint32_t poll_period)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    LLC_HANDLE_S *handle = dev;

    status = llc_handle_verify_and_get(dev, &bus, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (llc_is_recving(dev)) {
        return HOST_ERRCODE_BUSY;
    }

    if ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE)
        || (device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL)) {
        if (cb != NULL) {
#if (CFG_HOST_PORT_PM_EN)
            status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INTERRUPT, HOST_IO_PULL_DOWN, true);
#else
            status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INTERRUPT, HOST_IO_PULL_DOWN, false);
#endif
            if (status != HOST_ERRCODE_SUCCESS) {
                return status;
            }
            if (device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE) {
#if (CFG_HOST_PORT_PM_EN)
                status = host_io_irq_cfg(device->dev.notify_io, HOST_IO_IRQ_EDGE_RISING, llc_notify_io_irq_handle, handle, true);
#else
                status = host_io_irq_cfg(device->dev.notify_io, HOST_IO_IRQ_EDGE_RISING, llc_notify_io_irq_handle, handle, false);
#endif
            } else {
#if (CFG_HOST_PORT_PM_EN)
                status = host_io_irq_cfg(device->dev.notify_io, HOST_IO_IRQ_LEVEL_HIGH, llc_notify_io_irq_handle, handle, true);
#else
                status = host_io_irq_cfg(device->dev.notify_io, HOST_IO_IRQ_LEVEL_HIGH, llc_notify_io_irq_handle, handle, false);
#endif
            }
            if (status != HOST_ERRCODE_SUCCESS) {
                return status;
            }
        } else {
#if (CFG_HOST_PORT_PM_EN)
            status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, true);
#else
            status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, false);
#endif
            if (status != HOST_ERRCODE_SUCCESS) {
                return status;
            }
        }
    } else if ((device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR)
               || (device->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD)) {
        if (bus->ops.interrupt_handle_regist != NULL) {
            if (cb != NULL) {
                status = bus->ops.interrupt_handle_regist(bus->bus, llc_interrupt_handle, handle);
            } else {
                status = bus->ops.interrupt_handle_regist(bus->bus, NULL, NULL);
            }
            if (status != HOST_ERRCODE_SUCCESS) {
                return status;
            }
        } else {
            return HOST_ERRCODE_UNSUPPORT;
        }
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    handle->notify_callback = cb;
    handle->notify_arg = arg;

    return HOST_ERRCODE_SUCCESS;
}


int LLC_Notify_Handle_Get(LLC_HANDLE dev, LLC_Notify_CB *cb, void **arg, uint32_t *poll_period)
{
    int status = HOST_ERRCODE_SUCCESS;
    LLC_HANDLE_S *handle = dev;

    status = llc_handle_verify_and_get(dev, NULL, NULL);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if(cb == NULL || arg == NULL || poll_period == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    *cb = handle->notify_callback;
    *arg = handle->notify_arg;
    *poll_period = 0;

    return HOST_ERRCODE_SUCCESS;
}


int LLC_IO_Enable_Upd(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    status = host_io_mode_set(device->dev.upd_io, HOST_IO_MODE_OUTPUT, HOST_IO_PULL_UP, false);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_IO_Disable_Upd(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    status = host_io_mode_set(device->dev.upd_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_UP, false);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_IO_Enable_Rst(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    status = host_io_mode_set(device->dev.rst_io, HOST_IO_MODE_OUTPUT, HOST_IO_PULL_DOWN, false);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_IO_Disable_Rst(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    status = host_io_mode_set(device->dev.rst_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_DOWN, false);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return HOST_ERRCODE_SUCCESS;
}


int LLC_IO_Set_Upd(LLC_HANDLE dev, host_io_value_t level)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    host_io_write(device->dev.upd_io, level, false);

    return HOST_ERRCODE_SUCCESS;
}


int LLC_IO_Set_Rst(LLC_HANDLE dev, host_io_value_t level)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    host_io_write(device->dev.rst_io, level, false);

    return HOST_ERRCODE_SUCCESS;
}


host_io_value_t LLC_IO_Get_Notify(LLC_HANDLE dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_io_value_t level = HOST_IO_VALUE_LOW;
    host_bus_device_t *device = NULL;
    LLC_HANDLE_S *handle = dev;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return 0;
    }

#if (CFG_HOST_PORT_PM_EN)
    host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, true);
#else
    host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, false);
#endif
    if (handle->notify_enable) {
#if (CFG_HOST_PORT_PM_EN)
        level = host_io_read(device->dev.notify_io, true);
        host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INTERRUPT, HOST_IO_PULL_DOWN, true);
#else
        level = host_io_read(device->dev.notify_io, false);
        host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INTERRUPT, HOST_IO_PULL_DOWN, false);
#endif
    } else {
#if (CFG_HOST_PORT_PM_EN)
        level = host_io_read(device->dev.notify_io, true);
        host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_DOWN, true);
#else
        level = host_io_read(device->dev.notify_io, false);
        host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_DOWN, false);
#endif
    }

    return level;
}


int LLC_IO_Notify_Irq_Control(LLC_HANDLE dev, bool enable)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;
    LLC_HANDLE_S *handle = dev;

    status = llc_handle_verify_and_get(dev, NULL, &device);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    if (enable) {
#if (CFG_HOST_PORT_PM_EN)
        status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INTERRUPT, HOST_IO_PULL_DOWN, true);
#else
        status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INTERRUPT, HOST_IO_PULL_DOWN, false);
#endif
        if (status != HOST_ERRCODE_SUCCESS) {
            return status;
        }
#if (CFG_HOST_PORT_PM_EN)
        status = host_io_irq_enable(device->dev.notify_io, true);
#else
        status = host_io_irq_enable(device->dev.notify_io, false);
#endif
    } else {
#if (CFG_HOST_PORT_PM_EN)
        status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, true);
#else
        status = host_io_mode_set(device->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, false);
#endif
        if (status != HOST_ERRCODE_SUCCESS) {
            return status;
        }
#if (CFG_HOST_PORT_PM_EN)
        status = host_io_irq_disable(device->dev.notify_io, true);
#else
        status = host_io_irq_disable(device->dev.notify_io, false);
#endif
    }
    if (status == HOST_ERRCODE_SUCCESS) {
        handle->notify_enable = enable ? 1 : 0;
    }

    return status;
}


int LLC_IO_Notify_Irq_Update(LLC_HANDLE dev)
{
    LLC_HANDLE_S *handle = dev;

    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return LLC_IO_Notify_Irq_Control(dev, handle->notify_enable);
}


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
