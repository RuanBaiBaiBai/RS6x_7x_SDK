/**
 **************************************************************************************************
 * @file    port_uart_edge.c
 * @brief   port communication adaptation for uart, notify type is EDGE.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

#if 0

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"

#if (CFG_HOST_PORT_COM_EN == 1)
#include "../include/hal_com.h"
#if (CFG_HOST_PORT_OS_EN == 1)
#include "../include/hal_os.h"
#endif  /* CFG_HOST_PORT_OS_EN == 1 */

#if (CFG_HOST_PORT_COM_UART_EN == 1)
#include "hal_uart.h"
#endif  /* CFG_HOST_PORT_COM_UART_EN == 1 */
#endif  /* CFG_HOST_PORT_COM_EN == 1 */


/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_UART_EN == 1))
static void * porting_port_uart_init(uint8_t id, uint32_t speed, void *arg);
static int porting_port_uart_deinit(void *bus);

static int porting_port_uart_open(void *bus, uint32_t id);
static int porting_port_uart_close(void *bus, uint32_t id);

static int porting_port_uart_write(void *bus, uint32_t id, void *pbuff, uint32_t size);
static int porting_port_uart_read(void *bus, uint32_t id, void *pbuff, uint32_t size);
#endif


/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_UART_EN == 1))
#define PORT_UART_DEV_HANDLE                   ((void *)2)


static const host_com_ops_t uart_ops = {
    .init                    = porting_port_uart_init,
    .deinit                  = porting_port_uart_deinit,
    .open                    = porting_port_uart_open,
    .close                   = porting_port_uart_close,
    .write                   = porting_port_uart_write,
    .read                    = porting_port_uart_read,
};
#endif


/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static HAL_Dev_t *uartDev = NULL;


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_SPI_EN == 1))
static void * porting_port_uart_init(uint8_t id, uint32_t speed, void *arg)
{
    int status = HOST_ERRCODE_SUCCESS;
    UART_InitParam_t uart_cfg = {
        .baudRate = speed,
        .parity = UART_PARITY_NONE,
        .stopBits = UART_STOP_BIT_1,
        .dataBits = UART_DATA_WIDTH_8,
        .autoFlowCtrl = UART_FLOW_CTRL_NONE
    };

    if (id != 1) {
        return NULL;
    }

    if (uartDev == NULL) {
        // init bus
        uartDev = HAL_UART_Init(UART1_ID, &uart_cfg);
        if (uartDev == NULL) {
            return NULL;
        }
        status = HAL_UART_Open(uartDev);
        if (status != HAL_STATUS_OK) {
            goto error;
        }
        status = HAL_UART_SetTransferMode(uartDev, UART_DIR_TX, UART_TRANS_MODE_NOMA);
        if (status != HAL_STATUS_OK) {
            goto error;
        }
        status = HAL_UART_SetTransferMode(uartDev, UART_DIR_RX, UART_TRANS_MODE_NOMA);
        if (status != HAL_STATUS_OK) {
            goto error;
        }
    }

    return PORT_UART_DEV_HANDLE;

error:
    if (uartDev != NULL) {
        status = HAL_UART_Close(uartDev);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("close uart fail(%d) in uart init error handle\n", status);
        }
        status = HAL_UART_DeInit(uartDev);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("deinit uart fail(%d) in uart init error handle\n", status);
        }
        uartDev = NULL;
    }

    return NULL;
}


static int porting_port_uart_deinit(void *bus)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (bus != PORT_UART_DEV_HANDLE) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    // deinit bus
    if (uartDev != NULL) {
        status = HAL_UART_Close(uartDev);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("close uart fail\n");
            return status;
        }
        status = HAL_UART_DeInit(uartDev);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("deinit uart fail\n");
            return status;
        }
        uartDev = NULL;
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart_open(void *bus, uint32_t id)
{
    if (bus != PORT_UART_DEV_HANDLE || id != 1) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart_close(void *bus, uint32_t id)
{
    if (bus != PORT_UART_DEV_HANDLE || id != 1) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart_write(void *bus, uint32_t id, void *pbuff, uint32_t size)
{
    if (bus != PORT_UART_DEV_HANDLE || id != 1) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HAL_UART_Transmit(uartDev, pbuff, size, 100);
}


static int porting_port_uart_read(void *bus, uint32_t id, void *pbuff, uint32_t size)
{
    uint32_t len = 0;
    uint8_t *data = pbuff;

    if (bus != PORT_UART_DEV_HANDLE || id != 1 || pbuff == NULL || size < 0) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    while (len < size) {
        if (HAL_UART_IsRxReady(uartDev)) {
            data[len++] = HAL_UART_GetRxData(uartDev);
        } else {
            break;
        }
    }

    return (int)len;
}
#endif


host_com_ops_t *porting_port_uart_get_com_ops(uint8_t id)
{
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_UART_EN == 1))
    if (id == 1) {
        return (host_com_ops_t *)&uart_ops;
    } else {
        return NULL;
    }
#else
    return NULL;
#endif
}

#endif


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
