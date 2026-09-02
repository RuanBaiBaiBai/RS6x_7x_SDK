/**
 **************************************************************************************************
 * @file    host_llc_com_bus.c
 * @brief   host llc layer com bus module function file.
 * @attention
 *        Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "host_llc_com_bus.h"

#include "host_llc_com_device.h"


/* Macro.
 * ------------------------------------------------------------------------------------------------
 */
/* Types.
 * ------------------------------------------------------------------------------------------------
 */
static host_slist_t host_bus_tree[HOST_BUS_TYPE_NUM] = {
    [0 ... HOST_BUS_TYPE_NUM-1] = {
        .next = NULL
    }
};


/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
static host_bus_t *host_bus_find_bus_by_bus_id(host_bus_type_t type, uint8_t id)
{
    host_bus_t *bus = NULL;

    if (type >= HOST_BUS_TYPE_NUM) {
        return NULL;
    }

    HOST_SLIST_FOREACH(&host_bus_tree[type], bus, host_bus_t) {
        if (bus->id == id) {
            return bus;
        }
    }

    return NULL;
}

static host_bus_t *host_bus_find_bus_by_device_virtual_id(uint16_t id)
{
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    for (uint32_t i = 0; i < HOST_BUS_TYPE_NUM; i++) {
        HOST_SLIST_FOREACH(&host_bus_tree[i], bus, host_bus_t) {
            HOST_SLIST_FOREACH(&bus->device, device, host_bus_device_t) {
                if (device->virtual_id == id) {
                    return bus;
                }
            }
        }
    }

    return NULL;
}

static host_bus_device_t *host_bus_find_device_by_device_virtual_id(uint16_t id)
{
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    for (uint32_t i = 0; i < HOST_BUS_TYPE_NUM; i++) {
        HOST_SLIST_FOREACH(&host_bus_tree[i], bus, host_bus_t) {
            HOST_SLIST_FOREACH(&bus->device, device, host_bus_device_t) {
                if (device->virtual_id == id) {
                    return device;
                }
            }
        }
    }

    return NULL;
}


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
bool host_bus_exist_virtual_id(uint16_t virtual_id)
{
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;

    for (uint32_t i = 0; i < HOST_BUS_TYPE_NUM; i++) {
        HOST_SLIST_FOREACH(&host_bus_tree[i], bus, host_bus_t) {
            HOST_SLIST_FOREACH(&bus->device, device, host_bus_device_t) {
                if (device->virtual_id == virtual_id) {
                    return true;
                }
            }
        }
    }

    return false;
}


host_bus_t *host_bus_register(host_bus_type_t type, uint8_t id, uint8_t param)
{
    host_bus_t *bus = NULL;

    if (type >= HOST_BUS_TYPE_NUM) {
        return NULL;
    }

    bus = host_bus_find_bus_by_bus_id(type, id);
    if (bus != NULL) {
        return bus;
    }

    bus = (host_bus_t *)host_os_malloc(sizeof(host_bus_t));
    if (bus == NULL) {
        return NULL;
    }
    host_os_memset(bus, 0, sizeof(host_bus_t));

    host_slist_init(&bus->parent);
    host_slist_init(&bus->device);
    bus->type = type;
    bus->state = HOST_BUS_STATE_NULL;
    bus->id = id;
    if (type == HOST_BUS_TYPE_SPI) {
        bus->param = param;
    }
    bus->allow_use = 1;
    bus->right_of_use = host_os_sem_create(1, 1);
    if (bus->right_of_use == NULL) {
        host_os_free(bus);
        return NULL;
    }

    host_slist_append(&host_bus_tree[type], &bus->parent, true);

    return bus;
}

int host_bus_unregister(host_bus_t *bus)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_bus_t *b = NULL;

    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    b = host_bus_find_bus_by_bus_id(bus->type, bus->id);
    if (b == NULL) {
        return HOST_ERRCODE_SUCCESS;
    }

    if (bus->dev_open_num > 0) {
        return HOST_ERRCODE_BUSY;
    }

    // not allow use
    status = host_bus_not_allow_use(bus);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: not allow use fail(%d)\n", __func__, status);
        return status;
    }

    // close peripheral
    status = host_bus_close(bus);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: close bus fail(%d)\n", __func__, status);
        return status;
    }

    // detach from bus tree
    host_slist_remove(&host_bus_tree[bus->type], &bus->parent);

    // free bus
    status = host_os_sem_delete(bus->right_of_use);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("%s: delete sem of right of use fail(%d)\n", __func__, status);
        return status;
    }
    bus->right_of_use = NULL;
    host_os_free((void *)bus);
    bus = NULL;

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_attach_device(host_bus_type_t type, uint8_t id, host_bus_device_t *device)
{
    host_bus_t *bus = NULL;
    host_bus_device_t *d = NULL;
    host_os_critical_flag_t critical_flag;

    if (type >= HOST_BUS_TYPE_NUM || device == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    bus = host_bus_find_bus_by_bus_id(type, id);
    if (bus == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    // UART can only attach one device now
    if ((type == HOST_BUS_TYPE_UART) && (host_bus_get_device_num(bus) >= 1)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    d = host_bus_find_device_by_device_virtual_id(device->virtual_id);
    if (d != NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    // add device
    critical_flag = host_os_enter_critical();
    device->bus = (void *)bus;
    host_slist_append(&bus->device, &device->parent, false);
    bus->dev_num++;
    host_os_exit_critical(critical_flag);

    return HOST_ERRCODE_SUCCESS;
}

host_bus_device_t *host_bus_detach_device(uint16_t virtual_id)
{
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    host_os_critical_flag_t critical_flag;

    bus = host_bus_find_bus_by_device_virtual_id(virtual_id);
    if (bus == NULL) {
        return NULL;
    }
    device = host_bus_find_device_by_device_virtual_id(virtual_id);
    if (device == NULL) {
        return NULL;
    }

    // remove device
    critical_flag = host_os_enter_critical();
    host_slist_remove(&bus->device, &device->parent);
    device->bus = NULL;
    bus->dev_num--;
    host_os_exit_critical(critical_flag);

    return device;
}

int host_bus_iterator_bus_tree(void (*each)(void *bus, void *device, bool is_last_bus_device))
{
    host_bus_t *bus = NULL;
    host_bus_t *b = NULL;
    host_bus_device_t *device = NULL;
    host_bus_device_t *d = NULL;

    if (each == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < HOST_BUS_TYPE_NUM; i++) {
        b = NULL;
        HOST_SLIST_FOREACH(&host_bus_tree[i], bus, host_bus_t) {
            if (b != NULL) {
                each(b, d, true);
            }
            d = NULL;
            HOST_SLIST_FOREACH(&bus->device, device, host_bus_device_t) {
                if (d != NULL) {
                    each(bus, d, false);
                }
                d = device;
            }
//            if (d != NULL) {
//                each(bus, d, true);
//            }
            b = bus;
        }
        if (b != NULL) {
            each(b, d, true);
        }
    }

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_iterator_bus(host_bus_type_t type, uint8_t id, void (*each)(void *device))
{
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    host_bus_device_t *d = NULL;

    if (each == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    bus = host_bus_find_bus_by_bus_id(type, id);
    if (bus == NULL) {
        return HOST_ERRCODE_SUCCESS;
    }

    d = NULL;
    HOST_SLIST_FOREACH(&bus->device, device, host_bus_device_t) {
        if (d != NULL) {
            each(d);
        }
        d = device;
    }
    if (d != NULL) {
        each(d);
    }

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_detach_all_device(host_bus_type_t type, uint8_t id, void (*eachb)(void *device), void (*eacha)(void *device))
{
    host_bus_t *bus = NULL;
    host_bus_device_t *device = NULL;
    host_bus_device_t *d = NULL;
    host_os_critical_flag_t critical_flag;

    bus = host_bus_find_bus_by_bus_id(type, id);
    if (bus == NULL) {
        return HOST_ERRCODE_SUCCESS;
    }

    d = NULL;
    HOST_SLIST_FOREACH(&bus->device, device, host_bus_device_t) {
        if (d != NULL) {
            // after clean
            if (eacha != NULL) {
                eacha(d);
            }
        }
        // before clean
        if (eachb != NULL) {
            eachb(device);
        }
        // remove device
        critical_flag = host_os_enter_critical();
        host_slist_remove(&bus->device, &device->parent);
        device->bus = NULL;
        bus->dev_num--;
        host_os_exit_critical(critical_flag);
        d = device;
    }
    if (d != NULL) {
        // after clean
        if (eacha != NULL) {
            eacha(d);
        }
    }

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_set_speed(host_bus_t *bus, uint32_t speed)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (bus->state != HOST_BUS_STATE_NULL) {
        return HOST_ERRCODE_BUSY;
    }

    bus->speed = speed;

    return HOST_ERRCODE_SUCCESS;
}

uint32_t host_bus_get_speed(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return 0;
    }

    return bus->speed;
}

int host_bus_set_param(host_bus_t *bus, uint8_t bus_param)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (bus->state != HOST_BUS_STATE_NULL) {
        return HOST_ERRCODE_BUSY;
    }

    if (bus->type == HOST_BUS_TYPE_SPI) {
        if (bus_param < HOST_BUS_SPI_PARAM_MIN || bus_param > HOST_BUS_SPI_PARAM_MAX) {
            return HOST_ERRCODE_INVALID_PARAM;
        }

        bus->param = bus_param;
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}

uint8_t host_bus_get_param(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (bus->type == HOST_BUS_TYPE_SPI) {
        return bus->param;
    } else {
        return 0;
    }
}

int host_bus_set_com_ops(host_bus_t *bus, const host_com_ops_t *ops)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM || ops == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (bus->state != HOST_BUS_STATE_NULL) {
        return HOST_ERRCODE_BUSY;
    }

    host_os_memcpy((void *)&bus->ops, (const void *)ops, sizeof(bus->ops));

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_allow_use(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    bus->allow_use = 1;

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_not_allow_use(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    bus->allow_use = 0;

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_open(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (bus->state >= HOST_BUS_STATE_INIT) {
        return HOST_ERRCODE_SUCCESS;
    }
    if (bus->state != HOST_BUS_STATE_NULL) {
        return HOST_ERRCODE_STATE;
    }

    // init
    if (bus->bus == NULL) {
        if (bus->ops.init != NULL) {
            bus->bus = bus->ops.init(bus->id, bus->speed, (void *)(int)bus->param);
            if (bus->bus == NULL) {
                return HOST_ERRCODE_IO_ERROR;
            }
        } else {
            return HOST_ERRCODE_UNSUPPORT;
        }
    }

    bus->state = HOST_BUS_STATE_INIT;

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_close(host_bus_t *bus)
{
    int status = 0;

    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (bus->state == HOST_BUS_STATE_NULL) {
        return HOST_ERRCODE_SUCCESS;
    }
    if (bus->state != HOST_BUS_STATE_INIT) {
        return HOST_ERRCODE_STATE;
    }

    // deinit
    if (bus->bus != NULL) {
        if (bus->ops.deinit != NULL) {
            status = bus->ops.deinit(bus->bus);
            if (status != HOST_ERRCODE_SUCCESS) {
                return status;
            }
            bus->bus = NULL;
        } else {
            return HOST_ERRCODE_UNSUPPORT;
        }
    } else {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    bus->state = HOST_BUS_STATE_NULL;

    return HOST_ERRCODE_SUCCESS;
}

bool host_bus_exist_device(host_bus_t *bus, void *dev)
{
    host_bus_device_t *device = NULL;
    host_bus_device_t *d = NULL;

    if (bus == NULL || dev == NULL) {
        return false;
    }
    device = (host_bus_device_t *)dev;
    if (device->bus != bus) {
        return false;
    }

    HOST_SLIST_FOREACH(&bus->device, d, host_bus_device_t) {
        if (d == device) {
            return true;
        }
    }

    return false;
}

int host_bus_open_device(host_bus_t *bus, void *dev)
{
    int status = 0;
    host_bus_device_t *device = NULL;
    host_os_critical_flag_t critical_flag;

    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    device = (host_bus_device_t *)dev;

    if (bus->state < HOST_BUS_STATE_INIT) {
        return HOST_ERRCODE_NOT_READY;
    }

    if (bus->ops.open != NULL) {
        status = bus->ops.open(bus->bus, device->dev.param);
        if (status != HOST_ERRCODE_SUCCESS) {
            return status;
        }
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
    critical_flag = host_os_enter_critical();
    bus->dev_open_num++;
    if (bus->state == HOST_BUS_STATE_INIT) {
        bus->state = HOST_BUS_STATE_OPEN;
    }
    host_os_exit_critical(critical_flag);

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_close_device(host_bus_t *bus, void *dev)
{
    int status = 0;
    host_bus_device_t *device = NULL;
    host_os_critical_flag_t critical_flag;

    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (dev == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    device = (host_bus_device_t *)dev;

    if (bus->state == HOST_BUS_STATE_NULL) {
        return HOST_ERRCODE_NOT_READY;
    } else if (bus->state == HOST_BUS_STATE_INIT) {
        return HOST_ERRCODE_SUCCESS;
    } else if (bus->state == HOST_BUS_STATE_BUSY) {
        if (bus->occupy == device->virtual_id) {
            return HOST_ERRCODE_BUSY;
        }
    }

    if (bus->ops.close != NULL) {
        status = bus->ops.close(bus->bus, device->dev.param);
        if (status != HOST_ERRCODE_SUCCESS) {
            return status;
        }
    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }
    critical_flag = host_os_enter_critical();
    bus->dev_open_num--;
    if (bus->dev_open_num == 0) {
        bus->state = HOST_BUS_STATE_INIT;
    }
    host_os_exit_critical(critical_flag);

    return HOST_ERRCODE_SUCCESS;
}

int host_bus_get_device_num(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return bus->dev_num;
}

int host_bus_get_device_open_num(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return bus->dev_open_num;
}

bool host_bus_is_busy(host_bus_t *bus)
{
    if (bus == NULL) {
        return true;
    }

    if (bus->state == HOST_BUS_STATE_BUSY) {
        return true;
    }

    return false;
}

int host_bus_take_right_of_use(host_bus_t *bus, uint32_t timeout)
{
    int status = HOST_ERRCODE_SUCCESS;
    host_os_critical_flag_t critical_flag;

    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (bus->right_of_use == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = host_os_sem_take(bus->right_of_use, timeout);
    if (status == HOST_ERRCODE_SUCCESS) {
        critical_flag = host_os_enter_critical();
        if (bus->state == HOST_BUS_STATE_OPEN) {
            bus->state = HOST_BUS_STATE_BUSY;
        } else {
            host_os_exit_critical(critical_flag);
            host_os_delayms(1);
            critical_flag = host_os_enter_critical();
            if (bus->state == HOST_BUS_STATE_OPEN) {
                bus->state = HOST_BUS_STATE_BUSY;
            } else {
                status = HOST_ERRCODE_NOT_READY;
            }
        }
        host_os_exit_critical(critical_flag);
        if (status != HOST_ERRCODE_SUCCESS) {
            host_os_sem_give(bus->right_of_use);
        }
    }

    return status;
}

int host_bus_release_right_of_use(host_bus_t *bus)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (bus->right_of_use == NULL) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (bus->state != HOST_BUS_STATE_BUSY) {
        return HOST_ERRCODE_NOT_READY;
    }

    status = host_os_sem_give(bus->right_of_use);
    if (status == HOST_ERRCODE_SUCCESS) {
        bus->state = HOST_BUS_STATE_OPEN;
    }

    return status;
}

int host_bus_record_occupy(host_bus_t *bus, uint16_t virtual_id)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    bus->occupy = virtual_id;

    return HOST_ERRCODE_SUCCESS;
}

uint16_t host_bus_get_occupy(host_bus_t *bus)
{
    if (bus == NULL || bus->type >= HOST_BUS_TYPE_NUM) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return bus->occupy;
}


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
