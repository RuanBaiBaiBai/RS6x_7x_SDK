/**
 **************************************************************************************************
 * @file    host_llc_com_device.h
 * @brief   host llc layer com device module api header file.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_LLC_COM_DEVICE_H_
#define __HOST_LLC_COM_DEVICE_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../../porting/host_port.h"

#include "host_llc_com_bus.h"


/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_BUS_DEVICE_DUMP(device, tag) \
    do {                                                                             \
        HOST_LOG_PRINT("\n------------------------------\n");                        \
        HOST_LOG_PRINT(tag ":\n");                                                   \
        if ((device) == NULL) {                                                      \
            HOST_LOG_PRINT("device is NULL\n");                                      \
            break;                                                                   \
        } else {                                                                     \
            HOST_LOG_PRINT("%p\n", (device));                                        \
        }                                                                            \
        HOST_LOG_PRINT("&parent               %p\n", &(device)->parent);             \
        HOST_LOG_PRINT("type                  %d\n", (device)->type);                \
        HOST_LOG_PRINT("bus                   %p\n", (device)->bus);                 \
        HOST_LOG_PRINT("working               %d\n", (device)->working);             \
        HOST_LOG_PRINT("virtual_id            %d\n", (device)->virtual_id);          \
        HOST_LOG_PRINT("dev.upload_type       %d\n", (device)->dev.upload_type);     \
        HOST_LOG_PRINT("dev.upd_io            %d\n", (device)->dev.upd_io);          \
        HOST_LOG_PRINT("dev.rst_io            %d\n", (device)->dev.rst_io);          \
        HOST_LOG_PRINT("dev.notify_io         %d\n", (device)->dev.notify_io);       \
        HOST_LOG_PRINT("dev.notify_type       %d\n", (device)->dev.notify_type);     \
        HOST_LOG_PRINT("dev.param             %u\n", (device)->dev.param);           \
        HOST_LOG_PRINT("\n------------------------------\n");                        \
    } while (0)


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef enum {
    HOST_DEVICE_UPLOAD_TYPE_NULL = 0,         // no use
    HOST_DEVICE_UPLOAD_TYPE_ACTIVE,           // active report
    HOST_DEVICE_UPLOAD_TYPE_PASSIVE,          // passive report
    HOST_DEVICE_UPLOAD_TYPE_ACTIVE_DELAY,     // active report(delay)
} host_device_upload_type_t;

typedef uint32_t host_bus_device_param_t;

typedef struct {
    host_slist_t parent;     // bus
    host_bus_type_t type;    // bus type
    void *bus;               // bus handle
    uint8_t working;         // device state, 0 -> close, other -> open
    uint16_t virtual_id;     // device virtual id, like ip
    struct {
        host_device_upload_type_t upload_type;
        uint8_t upd_io;
        uint8_t rst_io;
        uint8_t notify_io;
#define HOST_BUS_DEVICE_NOTIFY_TYPE_EDGE                0
#define HOST_BUS_DEVICE_NOTIFY_TYPE_LEVEL               1
#define HOST_BUS_DEVICE_NOTIFY_TYPE_COM_ISR             2
#define HOST_BUS_DEVICE_NOTIFY_TYPE_COM_IRQ_THREAD      3
#define HOST_BUS_DEVICE_NOTIFY_TYPE_DATA_USER           4
        uint8_t notify_type;
        host_bus_device_param_t param;
    } dev;
} host_bus_device_t;


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
extern int host_bus_attach_device(host_bus_type_t type, uint8_t id, host_bus_device_t *device);
extern host_bus_device_t *host_bus_detach_device(uint16_t virtual_id);

host_bus_device_t *host_bus_device_register(
    host_bus_type_t bus_type, uint8_t bus_id, uint16_t virtual_id,
    host_device_upload_type_t upload_type, uint8_t notify_type,
    uint8_t upd_io, uint8_t rst_io, uint8_t notify_io,
    host_bus_device_param_t dev_param
);
int host_bus_device_unregister(host_bus_device_t *dev);

int host_bus_device_set_upload_type(host_bus_device_t *dev, host_device_upload_type_t upload_type);
host_device_upload_type_t host_bus_device_get_upload_type(host_bus_device_t *dev);
int host_bus_device_set_io(host_bus_device_t *dev, uint8_t upd_io, uint8_t rst_io, uint8_t notify_io);
uint8_t host_bus_device_get_upd_io(host_bus_device_t *dev);
uint8_t host_bus_device_get_rst_io(host_bus_device_t *dev);
uint8_t host_bus_device_get_notify_io(host_bus_device_t *dev);
int host_bus_device_set_param(host_bus_device_t *dev, host_bus_device_param_t dev_param);
host_bus_device_param_t host_bus_device_get_param(host_bus_device_t *dev);

int host_bus_device_open(host_bus_device_t *dev);
int host_bus_device_close(host_bus_device_t *dev);


#ifdef __cplusplus
}
#endif

#endif /* __HOST_LLC_COM_DEVICE_H_ */

