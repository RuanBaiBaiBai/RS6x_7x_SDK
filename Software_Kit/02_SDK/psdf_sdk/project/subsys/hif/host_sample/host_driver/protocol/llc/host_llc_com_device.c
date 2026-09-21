/**
 **************************************************************************************************
 * @file    host_llc_com_device.c
 * @brief   host llc layer com device module function file.
 * @attention
 *        Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "host_llc_com_device.h"


/* Macro.
 * ------------------------------------------------------------------------------------------------
 */
/* Types.
 * ------------------------------------------------------------------------------------------------
 */
/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
host_bus_device_t *host_bus_device_register(
    host_bus_type_t bus_type, uint8_t bus_id, uint16_t virtual_id,
    host_device_upload_type_t upload_type, uint8_t notify_type,
    uint8_t upd_io, uint8_t rst_io, uint8_t notify_io,
    host_bus_device_param_t dev_param
)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *device = NULL;

    if (bus_type >= HOST_BUS_TYPE_NUM) {
        return NULL;
    }

    if (notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL) {
        return NULL;
    }
    if ((notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE)
        || (notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL)) {
        if (upload_type != HOST_DEVICE_UPLOAD_TYPE_PASSIVE) {
            return NULL;
        }
    } else {
        if (upload_type == HOST_DEVICE_UPLOAD_TYPE_PASSIVE) {
            return NULL;
        }
    }

    device = (host_bus_device_t *)host_os_malloc(sizeof(host_bus_device_t));
    if (device == NULL) {
        return NULL;
    }
    host_os_memset(device, 0, sizeof(host_bus_device_t));

    host_slist_init(&device->parent);
    device->type = bus_type;
    device->virtual_id = virtual_id;
    device->dev.upload_type = upload_type;
    device->dev.upd_io = upd_io;
    device->dev.rst_io = rst_io;
    device->dev.notify_io = notify_io;
    device->dev.notify_type = notify_type;
    host_os_memcpy((void *)&device->dev.param, (const void *)&dev_param, sizeof(device->dev.param));

    status = host_bus_attach_device(bus_type, bus_id, device);
    if (status != HOST_ERRCODE_SUCCESS) {
        host_os_free(device);
        device = NULL;
        return NULL;
    }

    return device;
}

int host_bus_device_unregister(host_bus_device_t *dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_device_t *d = NULL;

    if (dev == NULL) {
        return HOST_ERRCODE_SUCCESS;
    }

    // close device
    status = host_bus_device_close(dev);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: close device fail(%d)\n", __func__, status);
        return status;
    }

    // detach from device list
    d = host_bus_detach_device(dev->virtual_id);
    if (d != dev) {
        HOST_LOG_ERR("%s: detach device fail(%d)\n", __func__, status);
        return HOST_ERRCODE_INVALID_PARAM;
    }

    // free all device source
    // nothing to do

    // free device
    host_os_free(dev);
    dev = NULL;

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_device_set_upload_type(host_bus_device_t *dev, host_device_upload_type_t upload_type)
{
    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    dev->dev.upload_type = upload_type;

    return HOST_ERRCODE_SUCCESS;
}

host_device_upload_type_t host_bus_device_get_upload_type(host_bus_device_t *dev)
{
    if (dev == NULL) {
        return HOST_DEVICE_UPLOAD_TYPE_NULL;
    }

    return dev->dev.upload_type;
}

int host_bus_device_set_io(host_bus_device_t *dev, uint8_t upd_io, uint8_t rst_io, uint8_t notify_io)
{
    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    dev->dev.upd_io = upd_io;
    dev->dev.rst_io = rst_io;
    dev->dev.notify_io = notify_io;

    return HOST_ERRCODE_SUCCESS;
}

uint8_t host_bus_device_get_upd_io(host_bus_device_t *dev)
{
    if (dev == NULL) {
        return 255;
    }

    return dev->dev.upd_io;
}

uint8_t host_bus_device_get_rst_io(host_bus_device_t *dev)
{
    if (dev == NULL) {
        return 255;
    }

    return dev->dev.rst_io;
}

uint8_t host_bus_device_get_notify_io(host_bus_device_t *dev)
{
    if (dev == NULL) {
        return 255;
    }

    return dev->dev.notify_io;
}

int host_bus_device_set_param(host_bus_device_t *dev, host_bus_device_param_t dev_param)
{
    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    host_os_memcpy((void *)&dev->dev.param, (const void *)&dev_param, sizeof(dev->dev.param));

    return HOST_ERRCODE_SUCCESS;
}

host_bus_device_param_t host_bus_device_get_param(host_bus_device_t *dev)
{
    host_bus_device_param_t param;

    if (dev == NULL) {
        host_os_memset((void *)&param, 0, sizeof(param));
        return param;  // ???
    }

    return dev->dev.param;
}

int host_bus_device_open(host_bus_device_t *dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_os_critical_flag_t critical_flag;

    if (dev == NULL || dev->bus == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (!host_bus_exist_device(dev->bus, dev)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (dev->working == 0) {
        critical_flag = host_os_enter_critical();
        do {
            // bus
            status = host_bus_open_device(dev->bus, dev);
            if (status != HOST_ERRCODE_SUCCESS) {
                break;
            }
            // io
            if ((dev->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE)
                || (dev->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL)) {
                // enable notify io irq in LLC layer
#if (CFG_HOST_PORT_PM_EN)
                status = host_io_mode_set(dev->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, true);
#else
                status = host_io_mode_set(dev->dev.notify_io, HOST_IO_MODE_INPUT, HOST_IO_PULL_DOWN, false);
#endif
                if (status != HOST_ERRCODE_SUCCESS) {
                    break;
                }
            }
            //mark
            dev->working = 1;
        } while (0);
        host_os_exit_critical(critical_flag);
    }

    return status;
}

int host_bus_device_close(host_bus_device_t *dev)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_os_critical_flag_t critical_flag;

    if (dev == NULL || dev->bus == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (!host_bus_exist_device(dev->bus, dev)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (dev->working == 1) {
        critical_flag = host_os_enter_critical();
        do {
            // bus
            status = host_bus_close_device(dev->bus, dev);
            if (status != HOST_ERRCODE_SUCCESS) {
                break;
            }
            // io
            status = host_io_mode_set(dev->dev.upd_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_UP, false);
            if (status != HOST_ERRCODE_SUCCESS) {
                break;
            }
            status = host_io_mode_set(dev->dev.rst_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_DOWN, false);
            if (status != HOST_ERRCODE_SUCCESS) {
                break;
            }
            if ((dev->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE)
                || (dev->dev.notify_type == HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL)) {
#if (CFG_HOST_PORT_PM_EN)
                status = host_io_mode_set(dev->dev.notify_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_DOWN, true);
#else
                status = host_io_mode_set(dev->dev.notify_io, HOST_IO_MODE_DISABLE, HOST_IO_PULL_DOWN, false);
#endif
                if (status != HOST_ERRCODE_SUCCESS) {
                    break;
                }
            }
            // mark
            dev->working = 0;
        } while (0);
        host_os_exit_critical(critical_flag);
    }

    return status;
}


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
