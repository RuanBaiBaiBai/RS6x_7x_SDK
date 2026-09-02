/**
 ******************************************************************************
 * @file    main.c
 * @brief   memory to memory block mode test.
 * @verbatim    null
 ******************************************************************************
 * @attention
 *
 * Copyright (C) 2025 POSSUMIC TECHNOLOGY CO., LTD. All rights reserved.
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
#include "hal_dma.h"

#include "log.h"
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define DMA_TEST_SIZE                               (4096U)

#if ((CONFIG_PSRAM_PREINIT_EN == 1) && (CONFIG_SOC_MULTI_CORE == 0))
#define BUFF_PSRAM_ADDR                             (0x1c080000UL)
#endif


/* Private macros.
 * ----------------------------------------------------------------------------
 */
/* Private variables.
 * ----------------------------------------------------------------------------
 */
static HAL_Dev_t *pdmaDev = NULL;
static DMA_ChannelConf_t chancfg;
static OSI_Semaphore_t  pdoneSem;

/* Sram */
static __aligned(32) uint8_t src_buff[DMA_TEST_SIZE];

#if ((CONFIG_PSRAM_PREINIT_EN == 1) && (CONFIG_SOC_MULTI_CORE == 0))
static uint8_t *dst_buff = (uint8_t *)BUFF_PSRAM_ADDR;
#else
/* Sram */
static __aligned(32) uint8_t dst_buff[DMA_TEST_SIZE];
#endif



/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static void log_data(uint8_t *src_data, uint32_t src_len, uint8_t *dst_data, uint32_t dst_len)
{
    LOG_PRINT("src data:\n");
    LOG_HEX(src_data, src_len, "src addr 0x%08x", (uint32_t)src_data);

    printk("dst data:\n");
    LOG_HEX(dst_data, dst_len, "dst addr 0x%08x", (uint32_t)dst_data);
}

static void init_data(uint8_t *buff, uint32_t len)
{
    uint32_t data = 0;
    int inc       = 1;

    for (int i = 0; i < len ; ++i, data += inc) {
        buff[i] = data & 0xFF;
    }

    csi_dcache_invalid_range((unsigned long *)(buff), DMA_TEST_SIZE);
}

static void dma_done(HAL_Dev_t *pdmaDev, void *arg)
{
    //UNUSED_PARAMETER(pdmaDev);
    UNUSED_PARAMETER(arg);
    uint32_t doneNode = 0U;

    int32_t ch_busy = HAL_DMA_GetChanStatus(pdmaDev, 0, &doneNode);

    //LOG_PRINT("\nDMA Tranfer Done! (Busy %d Number: %d)\n", ch_busy, doneNode);

    OSI_SemaphoreRelease(&pdoneSem);
}

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int main(void)
{
    HAL_Status_t status = HAL_STATUS_OK;
    uint32_t chid       = 0;
    DMA_BlockConf_t blockcfg  = { 0 };
    HAL_Callback_t  dma_done_cb = { .cb = NULL, .arg = NULL };

    LOG_PRINT("              DMA Memory 2 Memory Block Test\n");
    LOG_PRINT("-------------------------------------------\n\n");

    /* data init */
    printk("init src addr 0x%08x\n", (uint32_t)src_buff);
    init_data(src_buff, DMA_TEST_SIZE);

    printk("init dst addr 0x%08x\n", (uint32_t)dst_buff);
    OSI_Memset(dst_buff, 0, DMA_TEST_SIZE);

    //log_data(src_buff, DMA_TEST_SIZE / 8, dst_buff, DMA_TEST_SIZE / 8);

    /* Init semaphore */
    OSI_SemaphoreCreateBinary(&pdoneSem);

    pdmaDev = HAL_DMA_Init(DMA0_ID);
    if (!pdmaDev) {
        LOG_PRINT("dma init fail...\n");
    }

    chancfg.direction          =   DMA_DIRECTION_MEM2MEM;
    chancfg.priority           =   DMA_PRIORITY_0;
    chancfg.srcBrustNum        =   DMA_BURSTNUM_8;
    chancfg.dstBrustnum        =   DMA_BURSTNUM_8;
    chancfg.srcWidth           =   DMA_BITWIDTH_32;
    chancfg.dstWidth           =   DMA_BITWIDTH_32;
    chancfg.srcIncreMode       =   DMA_ADDR_INCREMENT;
    chancfg.dstIncreMode       =   DMA_ADDR_INCREMENT;
    chancfg.srcEnReload        =   0;
    chancfg.dstEnReload        =   0;
    chancfg.srcEnStatus        =   0;
    chancfg.dstEnStatus        =   0;

    int32_t ret = HAL_DMA_Open(pdmaDev, &chancfg);
    if (ret < 0) {
        LOG_PRINT("dma open channel fail (%d)\n", ret);
    } else {
        chid = ret;
        LOG_PRINT("dma open channel success --ch_id(%d)\n", chid);
    }

    blockcfg.srcAddr    =   (uint32_t)src_buff;
    blockcfg.dstAddr    =   (uint32_t)dst_buff;
    blockcfg.blockSize  =   DMA_TEST_SIZE;

    status = HAL_DMA_ConfigBlockTranfer(pdmaDev, chid, &blockcfg);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma config block mode (direct %d) fail (%d)\n",
                                chancfg.direction, status);
    } else {
        LOG_PRINT("dma config block mode success\n");
    }

    dma_done_cb.cb = dma_done;
    dma_done_cb.arg = NULL;
    status = HAL_DMA_RegisterIRQ(pdmaDev, chid, DMA_EVENT_DONE, &dma_done_cb);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma register channel irq fail (%d)\n", status);
    } else {
        LOG_PRINT("dma register channel irq success\n");
    }

    status = HAL_DMA_EnableIRQ(pdmaDev, chid, DMA_EVENT_ALL);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma enable channel irq fail (%d)\n", status);
    }  else {
        LOG_PRINT("dma enable channel irq success\n");
    }

    /* Invalid dcache range */
    csi_dcache_clean_invalid_range((unsigned long *)src_buff, DMA_TEST_SIZE);

    status = HAL_DMA_StartTransfer(pdmaDev, chid);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma start channel fail (%d)\n", status);
    }  else {
        LOG_PRINT("\ndma start channel success\n");
    }

    ret = OSI_SemaphoreWait(&pdoneSem, 1000);
    if (ret != OSI_STATUS_OK) {
        LOG_PRINT("semaphore waiting timeout\n");
    }

    if (0 != HAL_Memcmp(src_buff, dst_buff, DMA_TEST_SIZE)) {
        log_data(src_buff, DMA_TEST_SIZE / 8, dst_buff, DMA_TEST_SIZE / 8);
        LOG_PRINT("\n");
        LOG_PRINT("err: cmp\n");
    } else {
        LOG_PRINT("dma cmp memory success\n");
    }

    status = HAL_DMA_Close(pdmaDev, chid);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma close chanenl fail (%d)\n", status);
        //return -5;
    }  else {
        LOG_PRINT("dma close chanenl success\n");
    }

    status  =  HAL_DMA_DeInit(pdmaDev);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma deinit fail (%d)\n", status);
        //return -6;
    }  else {
        LOG_PRINT("dma deinit success\n");
    }

    OSI_SemaphoreDelete(&pdoneSem);

    LOG_PRINT("Test done\n");

    while (1) {
        OSI_MSleep(10);
    }
    return 0;
}

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
