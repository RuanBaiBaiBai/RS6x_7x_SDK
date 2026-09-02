/**
 ******************************************************************************
 * @file    main.c
 * @brief   memory to memory list mode test.
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

/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define DMA_LOG_EN                                  (0)

#define DMA_TEST_BLOCK_CNT                          (8U)
#define DMA_TEST_SIZE                               (1024U)


/* Private typedef.
 * ----------------------------------------------------------------------------
 */
typedef uint8_t (*srcArry_t)[DMA_TEST_SIZE];

/* Private macros.
 * ----------------------------------------------------------------------------
 ;*/

/* Private variables.
 * ----------------------------------------------------------------------------
 */
static OSI_Semaphore_t  g_pdoneSem;

/* SRAM */
_Alignas(32) static DMA_NodeConf_t Node[DMA_TEST_BLOCK_CNT];

_Alignas(32) static uint8_t src_buff[DMA_TEST_BLOCK_CNT][DMA_TEST_SIZE];

_Alignas(32) static uint8_t dst_buff[DMA_TEST_BLOCK_CNT * DMA_TEST_SIZE];


/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static void data_init(uint32_t rows, uint32_t cols, srcArry_t ptr)
{
    for (int i = 0; i < rows; i++) {
        #if DMA_LOG_EN
        LOG_PRINT("init data addr 0x%08x...\n", (uint32_t)&ptr[i][0]);
        #endif

        if ((i % 2) == 0) {
            for (int j = 0; j < cols; j++) {
                ptr[i][j] = (j & 0xFF);
            }
        } else {
            for (int j = 0; j < cols; j++) {
                ptr[i][j] = 0xAA;
            }
        }

        #if DMA_LOG_EN
        LOG_HEX(ptr[i], cols, "addr 0x%08x init success", (uint32_t)&ptr[i][0]);
        #endif
    }
}


static HAL_Status_t dma_init_nodes(HAL_Dev_t *pdmaDev, uint32_t chId)
{
    HAL_Status_t status       =  HAL_STATUS_OK;

    OSI_Memset(Node, 0, sizeof(Node));

    for (int i = 0; i < DMA_TEST_BLOCK_CNT; i++) {
        Node[i].srcAddr     =   (uint32_t)&src_buff[i][0];
        Node[i].dstAddr     =   (uint32_t)&dst_buff[DMA_TEST_SIZE * i];
        Node[i].blockSize   =   DMA_TEST_SIZE;

        status = HAL_DMA_InitNodeCtrl(pdmaDev, chId, &Node[i], true);
        if (status != HAL_STATUS_OK) {
            LOG_PRINT("init node_%d fail (%d)\n", i, status);
            break;
        }

        #if DMA_LOG_EN
        LOG_PRINT("Node_%d Info: src=0x%08X dst=0x%08x size=%d\n",
                  i,
                  (uint32_t)Node[i].srcAddr,
                  (uint32_t)Node[i].dstAddr,
                  (uint32_t)Node[i].blockSize);
         #endif
    }

    return status;
}


static void dma_link_nodes(void)
{
    for (int i = 0; i < DMA_TEST_BLOCK_CNT - 1U; i++) {
        HAL_DMA_AddNextNode(&Node[i], &Node[i + 1]);

        #if DMA_LOG_EN
        LOG_PRINT("Link Node_%d(0x%08X) ——> Node_%d(0x%08X)\n",
                 i, (uint32_t)&Node[i], (i + 1), (uint32_t)&Node[i + 1]);
        #endif
    }
}

static HAL_Status_t dma_init_list(HAL_Dev_t *pdmaDev, uint32_t chId)
{
    HAL_Status_t status = HAL_STATUS_OK;

    status = dma_init_nodes(pdmaDev, chId);
    if (status != HAL_STATUS_OK) {
        return status;
    }

    dma_link_nodes();

    /* @Note: The DMA linked list must be placed in SRAM
     *        beacuse DMA can only directly access physical
     *        memory. This can avoid the problems of Cache
     *        inconsistency and invisible.
     **/
    csi_dcache_clean_range((unsigned long *)Node, sizeof(Node));

    status = HAL_DMA_ConfigListTranfer(pdmaDev, chId, Node);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("config list tranfer(%d)\n", status);
        return status;
    }

    return HAL_STATUS_OK;
}

/**
 * @breif  DMA CallBack
 **/
static void dma_list_single_block_cb(HAL_Dev_t *pdmaDev, void *arg)
{
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(pdmaDev);

    //LOG_PRINT("\n\tDMA Block cb!\n");
}

static void dma_list_done_cb(HAL_Dev_t *pdmaDev, void *arg)
{
    uint32_t chId = *((uint32_t *)arg);
    uint32_t doneNode = 0U;

    if (!pdmaDev) {
        LOG_PRINT("dma dev find fail\n");
    }

    int32_t ch_busy = HAL_DMA_GetChanStatus(pdmaDev, chId, &doneNode);

    //LOG_PRINT("\n\tDMA Tranfer Done Cb! (Busy %d Number: %d)\n\n", ch_busy, doneNode);

    csi_dcache_invalid_range((unsigned long *)dst_buff, sizeof(dst_buff));

    OSI_SemaphoreRelease(&g_pdoneSem);
}

/* Exported functions.
 * ----------------------------------------------------------------------------
 */
int main(void)
{
    LOG_PRINT("  DMA Memory 2 Memory List Test\n");
    LOG_PRINT("-------------------------------------------\n\n");


    HAL_Status_t status = HAL_STATUS_OK;
    DMA_ChannelConf_t chancfg  =  { 0 };
    HAL_Callback_t dma_cb      =  { 0 };

    data_init(DMA_TEST_BLOCK_CNT, DMA_TEST_SIZE, (srcArry_t)src_buff);
    LOG_PRINT("[1] src buff init success (buff addr: 0x%08x)\n\n", (uint32_t)src_buff);

    /* Put the initalized buff in SRAM */
    csi_dcache_clean_range((unsigned long *)src_buff, sizeof(src_buff));

    OSI_Memset(dst_buff, 0, DMA_TEST_SIZE * DMA_TEST_BLOCK_CNT);
    LOG_PRINT("[2] dst buff clear zero success (buff addr: 0x%08x)\n\n", (uint32_t)dst_buff);


    if (OSI_SemaphoreCreateBinary(&g_pdoneSem) != OSI_STATUS_OK) {
        LOG_PRINT("Semaphore create fail\n");
        return -1;
    }

    HAL_Dev_t *pdmaDev = HAL_DMA_Init(DMA0_ID);
    if (NULL == pdmaDev) {
        LOG_PRINT("dma init fail (%d)\n", status);
        return -1;
    }

    chancfg.direction   =   DMA_DIRECTION_MEM2MEM;
    chancfg.priority    =   DMA_PRIORITY_7;   // highest
    chancfg.srcBrustNum =   DMA_BURSTNUM_8;
    chancfg.dstBrustnum =   DMA_BURSTNUM_8;
    chancfg.srcWidth    =   DMA_BITWIDTH_32;
    chancfg.dstWidth    =   DMA_BITWIDTH_32;
    chancfg.srcIncreMode =  DMA_ADDR_INCREMENT;
    chancfg.dstIncreMode =  DMA_ADDR_INCREMENT;
    chancfg.srcEnReload  =  0;
    chancfg.dstEnReload  =  0;
    chancfg.srcEnStatus  =  0;
    chancfg.dstEnStatus  =  0;

    uint32_t chanId = 0xFFU;
    int32_t ret = HAL_DMA_Open(pdmaDev, &chancfg);
    if (ret < 0) {
        LOG_PRINT("dma open channel fail (%d)\n", ret);
        goto ret_cleanup_dma;
    } else {
        chanId = ret;
    }

    LOG_PRINT("[3] dma open channel No.%d success\n", chanId);

    status = dma_init_list(pdmaDev, chanId);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma config list mode fail (%d)\n", status);
        goto ret_cleanup_dma;
    }

    dma_cb.cb   =   &dma_list_done_cb;
    dma_cb.arg  =   NULL;

    status = HAL_DMA_RegisterIRQ(pdmaDev, chanId, DMA_EVENT_DONE, &dma_cb);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma register DONE irq fail (%d)\n", status);
    }

    dma_cb.cb   =   &dma_list_single_block_cb;
    dma_cb.arg  =   NULL;

    status = HAL_DMA_RegisterIRQ(pdmaDev, chanId, DMA_EVENT_BLOCK_DONE, &dma_cb);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma register BLOCK irq fail (%d)\n", status);
    }

    status = HAL_DMA_EnableIRQ(pdmaDev, chanId, DMA_EVENT_ALL);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma enable irq fail (%d)\n", status);
    }

    status = HAL_DMA_StartTransfer(pdmaDev, chanId);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma start fail (%d)\n", status);
    }

    ret = OSI_SemaphoreWait(&g_pdoneSem, 5000);
    if (ret != OSI_STATUS_OK) {
        LOG_PRINT("semaphore waiting timeout\n");
    }

    if (0 == OSI_Memcmp(src_buff, dst_buff, DMA_TEST_SIZE * DMA_TEST_BLOCK_CNT)) {
        LOG_PRINT("[4] data compare success\n\n");
    } else {
        LOG_PRINT("[4] data compare fail\n\n");
        #if DMA_LOG_EN
        LOG_HEX(src_buff, DMA_TEST_SIZE * DMA_TEST_BLOCK_CNT, "src data (addr:0x%08x)", (uint32_t)src_buff);
        LOG_HEX(dst_buff, DMA_TEST_SIZE * DMA_TEST_BLOCK_CNT, "dst data (addr:0x%08x)", (uint32_t)dst_buff);
        #endif
        goto ret_cleanup_dma;
    }

ret_cleanup_dma:
    status = HAL_DMA_AbortTransfer(pdmaDev, chanId);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma abort transfer fail (%d)\n", status);
    }

    status = HAL_DMA_Close(pdmaDev, chanId);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma close channel fail (%d)\n", status);
        return -2;
    }

    status = HAL_DMA_DeInit(pdmaDev);
    if (status != HAL_STATUS_OK) {
        LOG_PRINT("dma deinit fail (%d)\n", status);
        return -2;
    }

    if (OSI_SemaphoreDelete(&g_pdoneSem) != OSI_STATUS_OK) {
        LOG_PRINT("Semaphore delete fail\n");
        return -1;
    }

    LOG_PRINT("[5] Test done (dma list)\n");

    return 0;
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

