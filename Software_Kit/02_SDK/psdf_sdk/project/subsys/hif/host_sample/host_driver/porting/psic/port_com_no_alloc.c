/**
 **************************************************************************************************
 * @file    port_com_no_alloc.c
 * @brief   port communication adaptation.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"

#if (CFG_HOST_PORT_COM_EN == 1)
#include "../include/hal_com.h"
#endif  /* CFG_HOST_PORT_COM_EN == 1 */


/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_COM_EN == 1)
static void * porting_port_com_init(uint8_t id, uint32_t speed, void *arg);
static int porting_port_com_deinit(void *bus);

static int porting_port_com_open(void *bus, uint32_t param);
static int porting_port_com_close(void *bus, uint32_t param);

static int porting_port_com_write(void *bus, uint32_t param, void *pbuff, uint32_t size);
static int porting_port_com_read(void *bus, uint32_t param, void *pbuff, uint32_t size);

static int porting_port_com_read_finish(void *bus, uint32_t param);

static int porting_port_com_interrupt_handle_regist(void *bus, host_com_interrupt_handler_t handler, void *arg);
static int porting_port_com_data_callback_regist(void *bus, host_com_data_callback_t cb, void *arg);
#endif


/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_COM_EN == 1)
static const host_com_ops_t com_ops = {
    .init                    = porting_port_com_init,
    .deinit                  = porting_port_com_deinit,
    .open                    = porting_port_com_open,
    .close                   = porting_port_com_close,
    .write                   = porting_port_com_write,
    .read                    = porting_port_com_read,
    .read_finish             = porting_port_com_read_finish,
    .interrupt_handle_regist = porting_port_com_interrupt_handle_regist,
    .data_callback_regist    = porting_port_com_data_callback_regist,
};
#endif


/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_COM_EN == 1)
static void * porting_port_com_init(uint8_t id, uint32_t speed, void *arg)
{
    return (void *)(1 + id);
}


static int porting_port_com_deinit(void *bus)
{
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_com_open(void *bus, uint32_t param)
{
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_com_close(void *bus, uint32_t param)
{
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_com_write(void *bus, uint32_t param, void *pbuff, uint32_t size)
{
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_com_read(void *bus, uint32_t param, void *pbuff, uint32_t size)
{
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_com_read_finish(void *bus, uint32_t param)
{
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_com_interrupt_handle_regist(void *bus, host_com_interrupt_handler_t handler, void *arg)
{
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_com_data_callback_regist(void *bus, host_com_data_callback_t cb, void *arg)
{
    return HOST_ERRCODE_SUCCESS;
}
#endif


host_com_ops_t *porting_port_com_no_alloc_get_com_ops(uint8_t id)
{
#if (CFG_HOST_PORT_COM_EN == 1)
    return (host_com_ops_t *)&com_ops;
#else
    return NULL;
#endif
}


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
