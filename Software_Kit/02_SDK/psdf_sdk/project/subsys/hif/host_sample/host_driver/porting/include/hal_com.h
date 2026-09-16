/**
 **************************************************************************************************
 * @file    hal_com.h
 * @brief   HAL com define.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HAL_COM_H_
#define _HAL_COM_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"


/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
typedef void (*host_com_interrupt_handler_t)(void *arg);
typedef void (*host_com_data_callback_t)(void *data, uint32_t len, void *arg);


typedef enum {
    HOST_COM_NULL = 0,
#if (CFG_HOST_PORT_COM_SPI_EN == 1)
    HOST_COM_SPI,
#endif  /* CFG_HOST_PORT_COM_SPI_EN == 1 */
#if (CFG_HOST_PORT_COM_I2C_EN == 1)
    HOST_COM_I2C,
#endif  /* CFG_HOST_PORT_COM_I2C_EN == 1 */
#if (CFG_HOST_PORT_COM_UART_EN == 1)
    HOST_COM_UART,
#endif  /* CFG_HOST_PORT_COM_UART_EN == 1 */
} host_com_type_t;

/* speed
 *     freq     for spi
 *     speed    for i2c
 *     baudrate for uart
 * called when bus open or close
 */
/**
 * @brief Open bus.
 *
 * @param id The bus id, the id is defined by yourself,
 *           such as, id=0 is SPI2, id=1 is SPI3.
 *
 * @param speed The bus speed, if device has other speed,
 *              you can ignore the param.
 *
 * @param arg The bus param set by yourself.
 *
 * @retval bus handle witch is defined by yourself.
 */
typedef void * (*host_com_init_t)(uint8_t id, uint32_t speed, void *arg);
/**
 * @brief Close bus.
 *
 * @param bus The bus handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
typedef int (*host_com_deinit_t)(void *bus);

/* param such as
 *     cs_pin   for spi
 *     addr     for i2c
 *     id       for uart
 * can defined by yourself
 * called when device open or close
 */
/**
 * @brief Open device.
 *
 * @param bus The bus handle.
 *
 * @param param The device param set by yourself.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
typedef int (*host_com_open_t)(void *bus, uint32_t param);
/**
 * @brief Close device.
 *
 * @param bus The bus handle.
 *
 * @param param The device param set by yourself.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
typedef int (*host_com_close_t)(void *bus, uint32_t param);

/* write and read must return 0 or >0
 * if return >0, means the number of write or read bytes
 */
/**
 * @brief Send data to a device.
 *
 * @param bus The bus handle.
 *
 * @param param The device param set by yourself.
 *
 * @param pbuff The buffer to be sent.
 *
 * @param size The buffer size, unit is byte.
 *
 * @retval >0     -->  the number of sent bytes,
 *         other  -->  error occured.
 */
typedef int (*host_com_write_t)(void *bus, uint32_t param, void *pbuff, uint32_t size);
/**
 * @brief Recv data from a device.
 *
 * @param bus The bus handle.
 *
 * @param param The device param set by yourself.
 *
 * @param pbuff The buffer to be recved.
 *
 * @param size The buffer size, unit is byte.
 *
 * @retval >0     -->  the number of recved bytes,
 *         other  -->  error occured.
 */
typedef int (*host_com_read_t)(void *bus, uint32_t param, void *pbuff, uint32_t size);

/**
 * @brief Send a signal to a device indicating completion of recv,
 *        allowing the start of the next round of recv.
 *
 * @note After the interface is called, if there is data that can
 *       be recved, it is necessary to call the handle registed
 *       with `host_com_interrupt_handle_regist_t` to notify HostDriver
 *       to recv data.
 *       Only defined when notify_type is LLC_NOTIFY_TYPE_COM_IRQ_THREAD
 *       or LLC_NOTIFY_TYPE_COM_ISR.
 *
 * @param bus The bus handle.
 *
 * @param param The device param set by yourself.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
typedef int (*host_com_read_finish_t)(void *bus, uint32_t param);

/**
 * @brief Control interrupt enable/disable, control interrupt thr,
 *        unit is byte.
 *
 * @note Only defined when notify_type is LLC_NOTIFY_TYPE_COM_IRQ_THREAD
 *       or LLC_NOTIFY_TYPE_COM_ISR.
 *       If notify_type is LLC_NOTIFY_TYPE_COM_IRQ_THREAD, you can define
 *       it or not, if it is defined, all rx_cache will be used to cache
 *       the data. The size of rx_cache is defined by macro
 *       `CFG_LLC_DEVICE_RX_CACHE_SIZE` at host_config.h, unit is byte.
 *
 * @param bus The bus handle.
 *
 * @param en Enable or disable interrupt.
 *
 * @param size The number of data to recv, the interrupt thr.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
typedef int (*host_com_interrupt_ctrl_t)(void *bus, bool en, uint32_t size);

/**
 * @brief Regist data recving and processing callback function.
 *
 * @param bus The bus handle.
 *
 * @param handler Call the handle, if there is data that can be recv.
 *
 * @param arg The handle arg.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
typedef int (*host_com_interrupt_handle_regist_t)(void *bus, host_com_interrupt_handler_t handler, void *arg);
/**
 * @brief This function has been abandoned.
 */
typedef int (*host_com_data_callback_regist_t)(void *bus, host_com_data_callback_t cb, void *arg);


#define HOST_COM_OPS_DUMP(com_ops, tag) \
    do {                                                                       \
        HOST_LOG_PRINT("\n------------------------------\n");                  \
        HOST_LOG_PRINT(tag ":\n");                                             \
        if ((com_ops) == NULL) {                                               \
            HOST_LOG_PRINT("bus is NULL\n");                                   \
            break;                                                             \
        } else {                                                               \
            HOST_LOG_PRINT("%p\n", (com_ops));                                 \
        }                                                                      \
        HOST_LOG_PRINT("init                  %p\n", (com_ops)->init);         \
        HOST_LOG_PRINT("deinit                %p\n", (com_ops)->deinit);       \
        HOST_LOG_PRINT("open                  %p\n", (com_ops)->open);         \
        HOST_LOG_PRINT("close                 %p\n", (com_ops)->close);        \
        HOST_LOG_PRINT("write                 %p\n", (com_ops)->write);        \
        HOST_LOG_PRINT("read                  %p\n", (com_ops)->read);         \
        HOST_LOG_PRINT("read_finish           %p\n", (com_ops)->read_finish);  \
        HOST_LOG_PRINT("interrupt_ctrl        %p\n", (com_ops)->interrupt_ctrl);    \
        HOST_LOG_PRINT("interrupt_handle_regist    %p\n", (com_ops)->interrupt_handle_regist);    \
        HOST_LOG_PRINT("data_callback_regist  %p\n", (com_ops)->data_callback_regist);    \
        HOST_LOG_PRINT("\n------------------------------\n");                  \
    } while (0)


typedef struct {
#if (CFG_HOST_PORT_COM_EN == 1)
    host_com_init_t                    init;         // called in bus open
    host_com_deinit_t                  deinit;       // called in bus close

    host_com_open_t                    open;         // called in device open
    host_com_close_t                   close;        // called in device close

    host_com_write_t                   write;        // write data
    host_com_read_t                    read;         // read data

    host_com_read_finish_t             read_finish;  // stop read, called by LLC, usually be called when timeout, let PHY do report

    host_com_interrupt_ctrl_t          interrupt_ctrl;  // enable or disable interrupt, set interrupt thr

    host_com_interrupt_handle_regist_t interrupt_handle_regist;  // interrupt for data available
    host_com_data_callback_regist_t    data_callback_regist;     // PHY directly reports the data in time, that recv finish or timeout
#endif  /* CFG_HOST_PORT_COM_EN == 1 */
} host_com_ops_t;


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */


#ifdef __cplusplus
}
#endif

#endif /* _HAL_COM_H_ */

