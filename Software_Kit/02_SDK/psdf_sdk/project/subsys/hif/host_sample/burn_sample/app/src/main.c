/**
 ******************************************************************************
 * @file    main.c
 * @brief   main define.
 * @verbatim    null
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2026 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the
 *       distribution.
 *    3. Neither the name of POSSUMIC TECHNOLOGY CO., LTD. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */


/* Includes.
 * ----------------------------------------------------------------------------
 */
#include "common.h"
#include "host_config.h"
#include "host_driver.h"
#include "host_mmw_api.h"

/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Types.
 * ------------------------------------------------------------------------------------------------
 */
/* Define.
 * ------------------------------------------------------------------------------------------------
 */

/*
    Modify macros (HIF_BUS_TYPE, HIF_BUS_SPEED, HIF_BUS_ID, HIF_BUS_PARAM, HIF_IO_UPD, 
    HIF_IO_RST, HIF_IO_NOTIFY, HIF_PARAM) before running sample.
    macros (HIF_BUS_EVENT_METHOD, HIF_NOTIFY_TYPE, HIF_UPLOAD_TYPE) no need to modify
*/ 

/* BUS TYPE  ( SPI:LLC_BUS_TYPE_SPI   UART:LLC_BUS_TYPE_UART   I2C:LLC_BUS_TYPE_I2C ) */
#define HIF_BUS_TYPE                            LLC_BUS_TYPE_SPI

#if (HIF_BUS_TYPE == LLC_BUS_TYPE_SPI)
#define HIF_BUS_SPEED                           8000000                         /* bus speed (MAX:8000000) */
#define HIF_BUS_ID                              0                               /* bus id (spi0:0 spi1:1...) */
#define HIF_BUS_PARAM                           LLC_BUS_PARAM_SPI               /* spi:LLC_BUS_PARAM_SPI 
                                                                                   dspi:LLC_BUS_PARAM_DSPI 
                                                                                   qspi:LLC_BUS_PARAM_QSPI */
#define HIF_IO_UPD                              4                               /* upd device io */
#define HIF_IO_RST                              5                               /* rst device io */
#define HIF_IO_NOTIFY                           6                               /* host notify io */
#define HIF_PARAM                               0                               /* spi_cs pin*/
#elif (HIF_BUS_TYPE == LLC_BUS_TYPE_UART)
#define HIF_BUS_SPEED                           921600                          /* bus speed (MAX:2000000) */
#define HIF_BUS_ID                              1                               /* bus id (uart0:0 uart1:1...) */
#define HIF_BUS_PARAM                           0                               /* no use */
#define HIF_IO_UPD                              1                               /* upd device io */
#define HIF_IO_RST                              5                               /* rst device io */
#define HIF_IO_NOTIFY                           6                               /* host notify io */
#define HIF_PARAM                               1                               /* bus id */
#elif (HIF_BUS_TYPE == LLC_BUS_TYPE_I2C)
#define HIF_BUS_SPEED                           100000                          /* bus speed (MAX:400000) */
#define HIF_BUS_ID                              0                               /* bus id (i2c0:0 i2c1:1...) */
#define HIF_BUS_PARAM                           0                               /* no use */
#define HIF_IO_UPD                              2                               /* upd device io */
#define HIF_IO_RST                              3                               /* rst device io */
#define HIF_IO_NOTIFY                           6                               /* host notify io */
#define HIF_PARAM                               0x3A                            /* i2c address */
#endif

#define HIF_BURN_RETRY_COUNT                    3                               /* hif burn sample retry count */

#define HIF_CFG_CMD_BUFF_SIZE                   512                             /* hif cmd buff max len */

#define HIF_LOG_PRINT_ENA                       1
#if HIF_LOG_PRINT_ENA
#define HIF_LOG_PRINT(fmt, arg...)      printf(fmt, ##arg)
#else
#define HIF_LOG_PRINT(fmt, arg...)
#endif

/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static DEV_HANDLE devHandle  = NULL;

/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
int main(void)
{
    int status = HOST_ERRCODE_SUCCESS;

    HIF_LOG_PRINT("Host burn sample\n");
    HIF_LOG_PRINT("-------------------------------------------\n");
    HIF_LOG_PRINT("please first read and transplant port_store.c \n");


/*
    put device to burn mode
    mode0:
    1.upd Low
    2.RST Low 
    3.delay
    4.RST High
    5.delay
    6.upd High

    mode1:
    1.upd Low
    2.power off(RST Low)
    3.delay
    4.power on(RST High)
    5.delay
    6.upd High
*/
    do {
        printf("1>. Config HIF driver\n");
        DevHw_t devHw;
        HifCfg_t hifCfg = {
            .cmd_buf_len        = HIF_CFG_CMD_BUFF_SIZE,
        };

        DevHw_Set_DefaultParam(&devHw, HIF_BUS_TYPE, HIF_BUS_PARAM, HIF_BUS_ID, HIF_BUS_SPEED,
                               HIF_IO_UPD, HIF_IO_RST, HIF_IO_NOTIFY, HIF_PARAM);

        status = Host_Driver_Init();
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("init host driver fail(%d)\n", status);
            break;
        }

        status = Host_Device_Regist(&devHandle, &devHw, &hifCfg);
        if (HOST_ERRCODE_SUCCESS != status) {
            HIF_LOG_PRINT("regist device1(%p) fail(%d)\n", devHandle, status);
            break;
        }



        HIF_LOG_PRINT("2>. Open HIF driver\n");
        status = Host_Device_Open(devHandle);
        if (status != HOST_ERRCODE_SUCCESS) {
            HIF_LOG_PRINT("open device(%p) fail(%d)\n", devHandle, status);
            break;
        }


        HIF_LOG_PRINT("3>. Host Burn img\n");
        for (int count = 0; count < HIF_BURN_RETRY_COUNT; count++) {
            do {
                /* host enter burn mode */
                status = Host_BurnMode_Enter(devHandle);
                if (status != HOST_ERRCODE_SUCCESS) {
                    HIF_LOG_PRINT("device(%p) enter burn mode fail(%d)\n", devHandle, status);
                    break;
                }

                /* host burn image */
                status = Host_Burn_Image(devHandle, "image_user");
                if (status != HOST_ERRCODE_SUCCESS) {
                    HIF_LOG_PRINT("device(%p) burn image fail(%d)\n", devHandle, status);
                }

            } while(0);

            HIF_LOG_PRINT("exit burn mode\n");
            /* host exit burn mode */
            Host_BurnMode_Exit(devHandle);
            if (status == HOST_ERRCODE_SUCCESS) {
                break;
            }
        }

    } while (0);

    if (status != HOST_ERRCODE_SUCCESS) {
        HIF_LOG_PRINT("4>. Init fail, close and deinit driver\n");
        Host_Device_Close(devHandle);
        Host_Device_Unregist(devHandle);
        Host_Driver_Deinit();
    } else {
        HIF_LOG_PRINT("SUCCESS\n");
    }

    HIF_LOG_PRINT("-------------------------------------------\n");

    return 0;
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
