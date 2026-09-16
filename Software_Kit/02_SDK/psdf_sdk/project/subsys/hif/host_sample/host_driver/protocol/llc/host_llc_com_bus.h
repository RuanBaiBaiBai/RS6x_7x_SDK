/**
 **************************************************************************************************
 * @file    host_llc_com_bus.h
 * @brief   host llc layer com bus module api header file.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_LLC_COM_BUS_H_
#define __HOST_LLC_COM_BUS_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../../porting/host_port.h"


/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_BUS_DUMP(bus, tag) \
    do {                                                                       \
        HOST_LOG_PRINT("\n------------------------------\n");                  \
        HOST_LOG_PRINT(tag ":\n");                                             \
        if ((bus) == NULL) {                                                   \
            HOST_LOG_PRINT("bus is NULL\n");                                   \
            break;                                                             \
        } else {                                                               \
            HOST_LOG_PRINT("%p\n", (bus));                                     \
        }                                                                      \
        HOST_LOG_PRINT("&parent               %p\n", &(bus)->parent);          \
        HOST_LOG_PRINT("&device               %p\n", &(bus)->device);          \
        HOST_LOG_PRINT("bus                   %p\n", (bus)->bus);              \
        HOST_LOG_PRINT("speed                 %u\n", (bus)->speed);            \
        HOST_LOG_PRINT("type                  %d\n", (bus)->type);             \
        HOST_LOG_PRINT("state                 %d\n", (bus)->state);            \
        HOST_LOG_PRINT("id                    %d\n", (bus)->id);               \
        HOST_LOG_PRINT("param                 %d\n", (bus)->param);            \
        HOST_LOG_PRINT("dev_num               %d\n", (bus)->dev_num);          \
        HOST_LOG_PRINT("dev_open_num          %d\n", (bus)->dev_open_num);     \
        HOST_LOG_PRINT("allow_use             %d\n", (bus)->allow_use);        \
        HOST_LOG_PRINT("occupy                %d\n", (bus)->occupy);           \
        HOST_LOG_PRINT("right_of_use          %p\n", (bus)->right_of_use);     \
        HOST_LOG_PRINT("&ops                  %p\n", &(bus)->ops);             \
        HOST_COM_OPS_DUMP(&(bus)->ops, "&ops->");                              \
        HOST_LOG_PRINT("\n------------------------------\n");                  \
    } while (0)


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef enum {
    HOST_BUS_TYPE_SPI = 0,
    HOST_BUS_TYPE_I2C,
    HOST_BUS_TYPE_UART,

    HOST_BUS_TYPE_NUM
} host_bus_type_t;

#define HOST_BUS_PARAM_SPI           1
#define HOST_BUS_PARAM_DSPI          2
#define HOST_BUS_PARAM_QSPI          3
#define HOST_BUS_SPI_PARAM_MIN       HOST_BUS_PARAM_SPI
#define HOST_BUS_SPI_PARAM_MAX       HOST_BUS_PARAM_QSPI

typedef enum {
    HOST_BUS_STATE_NULL = 0,         // no init
    HOST_BUS_STATE_INIT,             // just init, not open device
    HOST_BUS_STATE_OPEN,             // init and open device
    HOST_BUS_STATE_BUSY              // device using bus
} host_bus_state_t;

typedef struct {
    host_slist_t parent;             // bus tree node
    host_slist_t device;             // device head
    void *bus;                       // bus handle
    uint32_t speed;                  // bus speed
    host_bus_type_t type;            // bus type
    host_bus_state_t state;          // bus state
    uint8_t id;                      // bus id
    uint8_t param;                   // bus param
    uint8_t dev_num;                 // owned device number
    uint8_t dev_open_num;            // open device number
    uint8_t allow_use;               // allow device use bus
    uint16_t occupy;                 // device id who occupied bus
    host_os_sem_t right_of_use;      // bus right of use
    host_com_ops_t ops;              // com ops
} host_bus_t;  // bus


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
bool host_bus_exist_virtual_id(uint16_t virtual_id);

host_bus_t *host_bus_register(host_bus_type_t type, uint8_t id, uint8_t param);
int host_bus_unregister(host_bus_t *bus);

/**
 * @param each::is_last_bus_device
 *            the device is the bus's last device
 */
int host_bus_iterator_bus_tree(void (*each)(void *bus, void *device, bool is_last_bus_device));
int host_bus_iterator_bus(host_bus_type_t type, uint8_t id, void (*each)(void *device));

int host_bus_detach_all_device(host_bus_type_t type, uint8_t id, void (*eachb)(void *device), void (*eacha)(void *device));

int host_bus_set_speed(host_bus_t *bus, uint32_t speed);
uint32_t host_bus_get_speed(host_bus_t *bus);
int host_bus_set_param(host_bus_t *bus, uint8_t bus_param);
uint8_t host_bus_get_param(host_bus_t *bus);
int host_bus_set_com_ops(host_bus_t *bus, const host_com_ops_t *ops);

int host_bus_allow_use(host_bus_t *bus);
int host_bus_not_allow_use(host_bus_t *bus);

int host_bus_open(host_bus_t *bus);
int host_bus_close(host_bus_t *bus);

bool host_bus_exist_device(host_bus_t *bus, void *dev);
int host_bus_open_device(host_bus_t *bus, void *dev);
int host_bus_close_device(host_bus_t *bus, void *dev);

int host_bus_get_device_num(host_bus_t *bus);
int host_bus_get_device_open_num(host_bus_t *bus);

bool host_bus_is_busy(host_bus_t *bus);
int host_bus_take_right_of_use(host_bus_t *bus, uint32_t timeout);
int host_bus_release_right_of_use(host_bus_t *bus);

int host_bus_record_occupy(host_bus_t *bus, uint16_t virtual_id);
uint16_t host_bus_get_occupy(host_bus_t *bus);


#ifdef __cplusplus
}
#endif

#endif /* __HOST_LLC_COM_BUS_H_ */

