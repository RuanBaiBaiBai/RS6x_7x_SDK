/**
 **************************************************************************************************
 * @file    port_i2c.c
 * @brief   port communication adaptation for i2c master.
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

#if (CFG_HOST_PORT_COM_I2C_EN == 1)
#include "hal_i2c.h"
#include "ll_gpio.h"
#endif  /* CFG_HOST_PORT_COM_I2C_EN == 1 */
#endif  /* CFG_HOST_PORT_COM_EN == 1 */


/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_I2C_EN == 1))
static void * porting_port_i2c_init(uint8_t id, uint32_t speed, void *arg);
static int porting_port_i2c_deinit(void *bus);

static int porting_port_i2c_open(void *bus, uint32_t addr);
static int porting_port_i2c_close(void *bus, uint32_t addr);

static int porting_port_i2c_write(void *bus, uint32_t addr, void *pbuff, uint32_t size);
static int porting_port_i2c_read(void *bus, uint32_t addr, void *pbuff, uint32_t size);
#endif


/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_I2C_EN == 1))
#define PORT_I2C_DEV_HANDLE                    ((void *)1)


static const host_com_ops_t i2c_ops = {
    .init                    = porting_port_i2c_init,
    .deinit                  = porting_port_i2c_deinit,
    .open                    = porting_port_i2c_open,
    .close                   = porting_port_i2c_close,
    .write                   = porting_port_i2c_write,
    .read                    = porting_port_i2c_read,
};
#endif


/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static HAL_Dev_t *i2cDev = NULL;


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_I2C_EN == 1))
static void * porting_port_i2c_init(uint8_t id, uint32_t speed, void *arg)
{
    I2C_InitParam_t i2c_cfg = {
        .mode      =  I2C_MODE_MASTER,
        .busErrCb  =  {
            .arg   =  NULL,
            .cb    =  NULL,
        },
    };

    if (id != 0) {
        return NULL;
    }

    if (i2cDev != NULL) {
        return PORT_I2C_DEV_HANDLE;
    }

    if (speed <= 100000) {  // 100 Kbps
        i2c_cfg.speed = I2C_SPEED_STANDARD;
    } else if (speed <= 400000) {  // 400 Kbps
        i2c_cfg.speed = I2C_SPEED_FAST;
    } else if (speed <= 1000000) {  // 1 Mbps
        i2c_cfg.speed = I2C_SPEED_FAST_PLUS;
    } else {
        i2c_cfg.speed = I2C_SPEED_STANDARD;
    }

    i2cDev = HAL_I2C_Init(I2C0_ID, &i2c_cfg);
    if (i2cDev == NULL) {
        return NULL;
    }

    return PORT_I2C_DEV_HANDLE;
}


static int porting_port_i2c_deinit(void *bus)
{
    int status = HOST_ERRCODE_SUCCESS;

    if (bus != PORT_I2C_DEV_HANDLE) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (i2cDev == NULL) {
        return HOST_ERRCODE_SUCCESS;
    }

    status = HAL_I2C_DeInit(i2cDev);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("deinit i2c fail(%d)\n", status);
        return status;
    }
    i2cDev = NULL;

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_i2c_open(void *bus, uint32_t addr)
{
    if (bus != PORT_I2C_DEV_HANDLE) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_i2c_close(void *bus, uint32_t addr)
{
    if (bus != PORT_I2C_DEV_HANDLE) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_i2c_write(void *bus, uint32_t addr, void *pbuff, uint32_t size)
{
    int status = HOST_ERRCODE_SUCCESS;
    int len = 0;

    if (bus != PORT_I2C_DEV_HANDLE) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = HAL_I2C_Open(i2cDev, I2C_ADDR_WIDTH_7BIT, addr);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    len = HAL_I2C_Transmit(i2cDev, (uint8_t *)pbuff, size, 500);

    status = HAL_I2C_Close(i2cDev);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return len > 0 ? len : 0;
}


static int porting_port_i2c_read(void *bus, uint32_t addr, void *pbuff, uint32_t size)
{
    int status = HOST_ERRCODE_SUCCESS;
    int len = 0;

    if (bus != PORT_I2C_DEV_HANDLE) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = HAL_I2C_Open(i2cDev, I2C_ADDR_WIDTH_7BIT, addr);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    len = HAL_I2C_Receive(i2cDev, (uint8_t *)pbuff, size, 500);
    if (len != size) {
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, 5, LL_GPIOx_Pn_F1_OUTPUT);
        LL_GPIO_SetPinOutputType(GPIOA_DEV, 5, LL_GPIO_OUTPUT_PUSH_PULL);
        LL_GPIO_SetPinPull(GPIOA_DEV, 5, LL_GPIO_PULL_UP);
        LL_GPIO_SetPinStreng(GPIOA_DEV, 5, LL_GPIO_DRIVING_LVL_3);
        LL_GPIO_SetOutputPin(GPIOA_DEV, HW_BIT(5));
        host_os_delayms(1);
    }

    status = HAL_I2C_Close(i2cDev);
    if (status != HOST_ERRCODE_SUCCESS) {
        return status;
    }

    return len > 0 ? len : 0;
}
#endif


host_com_ops_t *porting_port_i2c_get_com_ops(uint8_t id)
{
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_I2C_EN == 1))
    if (id == 0) {
        return (host_com_ops_t *)&i2c_ops;
    } else {
        return NULL;
    }
#else
    return NULL;
#endif
}


/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
