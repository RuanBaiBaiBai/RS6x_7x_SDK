/**
 **************************************************************************************************
 * @file    port_uart_com_irq_thread.c
 * @brief   port communication adaptation for uart, notify type is COM_IRQ_THREAD,
 *          support uart0 and uart1.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

#if (!HIF_HOST_TEST_MULTI_SLV)

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
#include "hal_board.h"
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

static int porting_port_uart0_read_finish(void *bus, uint32_t id);
static int porting_port_uart1_read_finish(void *bus, uint32_t id);

static int porting_port_uart0_interrupt_ctrl(void *bus, bool en, uint32_t size);
static int porting_port_uart1_interrupt_ctrl(void *bus, bool en, uint32_t size);

static int porting_port_uart0_interrupt_handle_regist(void *bus, host_com_interrupt_handler_t handler, void *arg);
static int porting_port_uart1_interrupt_handle_regist(void *bus, host_com_interrupt_handler_t handler, void *arg);
#endif


/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_UART_EN == 1))
#define PORT_UART_NUM                          (2)

#define PORT_UART0_DEV_HANDLE                  ((void *)1)
#define PORT_UART1_DEV_HANDLE                  ((void *)2)


static const host_com_ops_t uart0_ops = {
    .init                    = porting_port_uart_init,
    .deinit                  = porting_port_uart_deinit,
    .open                    = porting_port_uart_open,
    .close                   = porting_port_uart_close,
    .write                   = porting_port_uart_write,
    .read                    = porting_port_uart_read,
    .read_finish             = porting_port_uart0_read_finish,
//    .interrupt_ctrl          = porting_port_uart0_interrupt_ctrl,
    .interrupt_handle_regist = porting_port_uart0_interrupt_handle_regist,
};

static const host_com_ops_t uart1_ops = {
    .init                    = porting_port_uart_init,
    .deinit                  = porting_port_uart_deinit,
    .open                    = porting_port_uart_open,
    .close                   = porting_port_uart_close,
    .write                   = porting_port_uart_write,
    .read                    = porting_port_uart_read,
    .read_finish             = porting_port_uart1_read_finish,
//    .interrupt_ctrl          = porting_port_uart1_interrupt_ctrl,
    .interrupt_handle_regist = porting_port_uart1_interrupt_handle_regist,
};
#endif


/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static HAL_Dev_t *uartDev[PORT_UART_NUM] = { NULL, NULL };
static void * const uartHdl[PORT_UART_NUM] = { PORT_UART0_DEV_HANDLE, PORT_UART1_DEV_HANDLE };

static host_com_interrupt_handler_t port_uart_interrupt_handle[PORT_UART_NUM] = { NULL, NULL };
static void *port_uart_interrupt_handle_arg[PORT_UART_NUM] = { NULL, NULL };


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_UART_EN == 1))
static void porting_port_uart_interrupt_handle(uint8_t id)
{
    if (port_uart_interrupt_handle[id] != NULL) {
        port_uart_interrupt_handle[id](port_uart_interrupt_handle_arg[id]);
    }
}


static void porting_port_uart_rx_isr_callback(HAL_Dev_t *dev, void *param)
{
    for (uint32_t id = 0; id < PORT_UART_NUM; id++) {
        if (uartHdl[id] == param) {
            HAL_UART_DisableRxReadyIRQ(uartDev[id]);
            porting_port_uart_interrupt_handle(id);
        }
    }
}


static int porting_port_uart_get_id(void *bus, uint8_t *id)
{
    for (uint32_t i = 0; i < PORT_UART_NUM; i++) {
        if (uartHdl[i] == bus) {
            *id = i;
            return HOST_ERRCODE_SUCCESS;
        }
    }

    return HOST_ERRCODE_NOT_FOUND;
}


static void porting_port_uart_set_rx_fifo_threshold(uint8_t id, uint32_t len)
{
#if 0
    if (len >= 62) {
        ((uart_dev_t *)uartDev->reg)->FCR = 0xF1;
    } else if (len >= 32) {
#else
    if (len >= 32) {
#endif
        ((uart_dev_t *)uartDev[id]->reg)->FCR = 0xB1;
    } else if (len >= 16) {
        ((uart_dev_t *)uartDev[id]->reg)->FCR = 0x71;
    } else {
        ((uart_dev_t *)uartDev[id]->reg)->FCR = 0x31;
    }
}


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
    HAL_Callback_t temp_cb = {
        .cb = porting_port_uart_rx_isr_callback,
        .arg = NULL
    };

    if (id >= PORT_UART_NUM) {
        return NULL;
    }
    temp_cb.arg = uartHdl[id];

    if (uartDev[id] == NULL) {
        // init bus
        uartDev[id] = HAL_UART_Init(id, &uart_cfg);
        if (uartDev[id] == NULL) {
            return NULL;
        }
        status = HAL_UART_Open(uartDev[id]);
        if (status != HAL_STATUS_OK) {
            goto error;
        }
        HAL_UART_DisableRxReadyIRQ(uartDev[id]);
        status = HAL_UART_RegisterIRQ(uartDev[id], UART_DIR_RX, &temp_cb);
        if (status != HAL_STATUS_OK) {
            goto error;
        }
        status = HAL_UART_SetTransferMode(uartDev[id], UART_DIR_TX, UART_TRANS_MODE_NOMA);
        if (status != HAL_STATUS_OK) {
            goto error;
        }
        status = HAL_UART_SetTransferMode(uartDev[id], UART_DIR_RX, UART_TRANS_MODE_NOMA);
        if (status != HAL_STATUS_OK) {
            goto error;
        }
        porting_port_uart_set_rx_fifo_threshold(id, 0);
    }

    return uartHdl[id];

error:
    if (uartDev[id] != NULL) {
        status = HAL_UART_Close(uartDev[id]);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("close uart fail(%d) in uart%d init error handle\n", status, id);
        }
        status = HAL_UART_DeInit(uartDev[id]);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("deinit uart fail(%d) in uart%d init error handle\n", status, id);
        }
        uartDev[id] = NULL;
    }

    return NULL;
}


static int porting_port_uart_deinit(void *bus)
{
    int status = HOST_ERRCODE_SUCCESS;
    uint8_t id = 0xFF;

    if (porting_port_uart_get_id(bus, &id) != HOST_ERRCODE_SUCCESS) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    // deinit bus
    if (uartDev[id] != NULL) {
        HAL_UART_DisableRxReadyIRQ(uartDev[id]);
        status = HAL_UART_Close(uartDev[id]);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("close uart%d fail\n", id);
            return status;
        }
        status = HAL_UART_DeInit(uartDev[id]);
        if (status != HAL_STATUS_OK) {
            HOST_LOG_ERR("deinit uart%d fail\n", id);
            return status;
        }
        uartDev[id] = NULL;
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart_open(void *bus, uint32_t id)
{
    uint8_t idx = 0xFF;

    if (porting_port_uart_get_id(bus, &idx) != HOST_ERRCODE_SUCCESS) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (idx != id) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    HAL_UART_EnableRxReadyIRQ(uartDev[id]);

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart_close(void *bus, uint32_t id)
{
    uint8_t idx = 0xFF;

    if (porting_port_uart_get_id(bus, &idx) != HOST_ERRCODE_SUCCESS) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (idx != id) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    HAL_UART_DisableRxReadyIRQ(uartDev[id]);

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart_write(void *bus, uint32_t id, void *pbuff, uint32_t size)
{
    uint8_t idx = 0xFF;

    if (porting_port_uart_get_id(bus, &idx) != HOST_ERRCODE_SUCCESS) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (idx != id) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HAL_UART_Transmit(uartDev[id], pbuff, size, 100);
}


static int porting_port_uart_read(void *bus, uint32_t id, void *pbuff, uint32_t size)
{
    uint8_t idx = 0xFF;
    uint32_t len = 0;
    uint8_t *data = pbuff;

    if (pbuff == NULL || size < 0) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (porting_port_uart_get_id(bus, &idx) != HOST_ERRCODE_SUCCESS) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (idx != id) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    while (len < size) {
        if (HAL_UART_IsRxReady(uartDev[id])) {
            data[len++] = HAL_UART_GetRxData(uartDev[id]);
        } else {
            break;
        }
    }

    return (int)len;
}


static int porting_port_uart0_read_finish(void *bus, uint32_t id)
{
    HAL_UART_EnableRxReadyIRQ(uartDev[0]);

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart1_read_finish(void *bus, uint32_t id)
{
    HAL_UART_EnableRxReadyIRQ(uartDev[1]);

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart0_interrupt_ctrl(void *bus, bool en, uint32_t size)
{
    if (uartDev[0] == NULL) {
        return HOST_ERRCODE_NOT_READY;
    }

    if (en) {
        porting_port_uart_set_rx_fifo_threshold(0, size);
        HAL_UART_EnableRxReadyIRQ(uartDev[0]);
    } else {
        HAL_UART_DisableRxReadyIRQ(uartDev[0]);
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart1_interrupt_ctrl(void *bus, bool en, uint32_t size)
{
    if (uartDev[1] == NULL) {
        return HOST_ERRCODE_NOT_READY;
    }

    if (en) {
        porting_port_uart_set_rx_fifo_threshold(1, size);
        HAL_UART_EnableRxReadyIRQ(uartDev[1]);
    } else {
        HAL_UART_DisableRxReadyIRQ(uartDev[1]);
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart0_interrupt_handle_regist(void *bus, host_com_interrupt_handler_t handler, void *arg)
{
    port_uart_interrupt_handle[0] = handler;
    port_uart_interrupt_handle_arg[0] = arg;

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_uart1_interrupt_handle_regist(void *bus, host_com_interrupt_handler_t handler, void *arg)
{
    port_uart_interrupt_handle[1] = handler;
    port_uart_interrupt_handle_arg[1] = arg;

    return HOST_ERRCODE_SUCCESS;
}
#endif


host_com_ops_t *porting_port_uart_get_com_ops(uint8_t id)
{
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_UART_EN == 1))
    if (id == 0) {
        return (host_com_ops_t *)&uart0_ops;
    } else if (id == 1) {
        return (host_com_ops_t *)&uart1_ops;
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
