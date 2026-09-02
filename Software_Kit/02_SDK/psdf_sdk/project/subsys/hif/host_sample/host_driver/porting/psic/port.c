/**
 **************************************************************************************************
 * @file    port.c
 * @brief   porting.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"

#include "../../protocol/llc/host_llc_com_bus.h"


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
/* Exporte Constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_COM_EN == 1)
//extern host_com_ops_t *porting_port_com_no_alloc_get_com_ops(uint8_t id);
#if (CFG_HOST_PORT_COM_SPI_EN == 1)
extern host_com_ops_t *porting_port_spi_get_com_ops(uint8_t id);
#endif
#if (CFG_HOST_PORT_COM_I2C_EN == 1)
extern host_com_ops_t *porting_port_i2c_get_com_ops(uint8_t id);
#endif
#if (CFG_HOST_PORT_COM_UART_EN == 1)
extern host_com_ops_t *porting_port_uart_get_com_ops(uint8_t id);
#endif
#endif  /* CFG_HOST_PORT_COM_EN == 1 */

/**
 * @brief provided to LLC
 */
host_com_ops_t *porting_get_com_ops(host_bus_type_t type, uint8_t id)
{
    switch (type) {
#if (CFG_HOST_PORT_COM_EN == 1)
#if (CFG_HOST_PORT_COM_SPI_EN == 1)
        case HOST_BUS_TYPE_SPI:
            return porting_port_spi_get_com_ops(id);
//            return porting_port_com_no_alloc_get_com_ops(id);
#endif
#if (CFG_HOST_PORT_COM_I2C_EN == 1)
        case HOST_BUS_TYPE_I2C:
            return porting_port_i2c_get_com_ops(id);
//            return porting_port_com_no_alloc_get_com_ops(id);
#endif
#if (CFG_HOST_PORT_COM_UART_EN == 1)
        case HOST_BUS_TYPE_UART:
            return porting_port_uart_get_com_ops(id);
//            return porting_port_com_no_alloc_get_com_ops(id);
#endif
#endif  /* CFG_HOST_PORT_COM_EN == 1 */
        default:
            return NULL;
    }
}


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
