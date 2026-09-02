/**
 **************************************************************************************************
 * @file    hal_io.h
 * @brief   HAL io define.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HAL_IO_H_
#define _HAL_IO_H_

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
typedef enum {
    HOST_IO_MODE_DISABLE,
    HOST_IO_MODE_OUTPUT,
    HOST_IO_MODE_INPUT,
    HOST_IO_MODE_INTERRUPT
} host_io_mode_t;

typedef enum {
    HOST_IO_PULL_NO,
    HOST_IO_PULL_UP,
    HOST_IO_PULL_DOWN,
} host_io_pull_t;

typedef enum {
    HOST_IO_VALUE_LOW,
    HOST_IO_VALUE_HIGH,
} host_io_value_t;

typedef enum {
    HOST_IO_IRQ_EDGE_RISING,
    HOST_IO_IRQ_EDGE_FALLING,
    HOST_IO_IRQ_EDGE_BOTH,
    HOST_IO_IRQ_LEVEL_HIGH,
    HOST_IO_IRQ_LEVEL_LOW,
} host_io_irq_trig_t;

typedef void (*host_io_irq_callback_t)(void *arg);


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_IO_EN == 1)
int host_io_mode_set(uint32_t io, host_io_mode_t mode, host_io_pull_t pull, bool is_wkio);
int host_io_irq_cfg(uint32_t io, host_io_irq_trig_t trig, host_io_irq_callback_t pcallback, void *arg, bool is_wkio);
int host_io_irq_enable(uint32_t io, bool is_wkio);
int host_io_irq_disable(uint32_t io, bool is_wkio);
void host_io_write(uint32_t io, host_io_value_t value, bool is_wkio);
host_io_value_t host_io_read(uint32_t io, bool is_wkio);
#endif  /* CFG_HOST_PORT_IO_EN == 1 */


#ifdef __cplusplus
}
#endif

#endif /* _HAL_IO_H_ */

