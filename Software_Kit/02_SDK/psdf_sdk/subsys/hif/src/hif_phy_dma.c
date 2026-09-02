/**
 ******************************************************************************
 * @file    hif_phy_dma.c
 * @brief   hif phy dma define.
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

#include "hif_types.h"
#include "hif_config.h"
#include "hif_log.h"
#include "hif_dll.h"
#include "hif_phy.h"
#include "hif.h"
#include "hif_phy_dma.h"

#if ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_DMA == 1))
#include "hal_dma.h"


/* Private typedef.
 * ----------------------------------------------------------------------------
 */
typedef struct {
    HIF_PHY_TransCallback_t sendCb;
    HIF_PHY_TransCallback_t recvCb;
    HIF_PHY_TransCallback_t errCb;

    HAL_Dev_t               *dmaDev;

    uint8_t                 sendShake;
    uint8_t                 recvShake;
    uint32_t                sendChId;
    uint32_t                recvChId;

    uint32_t                sendPhyAddr;
    uint32_t                recvPhyAddr;

    uint32_t                sendNodeParam;

} HIF_DMA_Ctrl_t;

static HIF_DMA_Ctrl_t hifDMACtrl;


/* Private defines.
 * ----------------------------------------------------------------------------
 */

/* Private macros.
 * ----------------------------------------------------------------------------
 */

#define CONFIG_HIF_DMA_XIP_IBUS_ADDR_START                  0x08000000
#define CONFIG_HIF_DMA_XIP_IBUS_ADDR_EBND                   0x10000000

#define CONFIG_HIF_DMA_XIP_DBUS_ADDR_MSK                    0x10000000
#define CONFIG_HIF_DMA_XIP_IBUS_ADDR_MSK                    0x0FFFFFFF


/* Private variables.
 * ----------------------------------------------------------------------------
 */

/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
static void hif_PHY_DMA_SendDoneIrqCallback(HAL_Dev_t * pDevice, void *arg);
static void hif_PHY_DMA_SendErrIrqCallback(HAL_Dev_t * pDevice, void *arg);
#if (CONFIG_HIF_PM != 0)
static void hif_PHY_DMA_ResumeCallback(HAL_Dev_t * pDevice, void *arg);
#endif

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


int hif_PHY_DMA_Init(HIF_DMA_InitCfg_t *pInitCfg)
{
    if (pInitCfg == NULL) {
        return HIF_ERRCODE_INVALID_PARAM;
    }

    hifDMACtrl.dmaDev = HAL_DMA_Init(pInitCfg->devId);
    if (hifDMACtrl.dmaDev == NULL) {
        return HIF_ERRCODE_NOT_READY;
    }

    hifDMACtrl.sendShake    = pInitCfg->sendShake;
    hifDMACtrl.recvShake    = pInitCfg->recvShake;
    hifDMACtrl.sendCb       = pInitCfg->sendCb;
    hifDMACtrl.recvCb       = pInitCfg->recvCb;
    hifDMACtrl.errCb        = pInitCfg->errCb;

    hifDMACtrl.sendNodeParam = 0;

    hifDMACtrl.sendPhyAddr  = pInitCfg->sendPhyAddr;
    hifDMACtrl.recvPhyAddr  = pInitCfg->recvPhyAddr;

    return HIF_ERRCODE_SUCCESS;
}

int hif_PHY_DMA_Deinit(void)
{
    return HAL_DMA_DeInit(hifDMACtrl.dmaDev);
}

int hif_PHY_DMA_Open(void)
{
    int status = HIF_ERRCODE_SUCCESS;
    DMA_ChannelConf_t hifDmaCfg;
    HAL_Callback_t dmaCb;

    /* send dma channel config */
    if (hifDMACtrl.sendShake != 0xFF) {
        hifDmaCfg.direction       = DMA_DIRECTION_MEM2PER;
        hifDmaCfg.priority        = DMA_PRIORITY_7;
        hifDmaCfg.srcBrustNum     = DMA_BURSTNUM_8;
        hifDmaCfg.dstBrustnum     = DMA_BURSTNUM_8;
        hifDmaCfg.srcWidth        = DMA_BITWIDTH_8;
        hifDmaCfg.dstWidth        = DMA_BITWIDTH_8;
        hifDmaCfg.dstHandShake    = hifDMACtrl.sendShake;
        hifDmaCfg.srcIncreMode    = DMA_ADDR_INCREMENT;
        hifDmaCfg.dstIncreMode    = DMA_ADDR_FIXED;
        hifDmaCfg.srcEnReload     = 0;
        hifDmaCfg.dstEnReload     = 0;
        hifDmaCfg.srcEnStatus     = 0;
        hifDmaCfg.dstEnStatus     = 0;

        status = HAL_DMA_Open(hifDMACtrl.dmaDev, &hifDmaCfg);
        if (status < 0) {
            return HIF_ERRCODE_NOT_READY;
        } else {
            hifDMACtrl.sendChId = status;
        }

        if (hifDMACtrl.sendCb != NULL) {
            dmaCb.arg = NULL;
            dmaCb.cb = hif_PHY_DMA_SendDoneIrqCallback;
            status = HAL_DMA_RegisterIRQ(hifDMACtrl.dmaDev, hifDMACtrl.sendChId, DMA_EVENT_DONE, &dmaCb);
            if (status != HAL_STATUS_OK) {
                return HIF_ERRCODE_IO_ERROR;
            }
            status = HAL_DMA_EnableIRQ(hifDMACtrl.dmaDev, hifDMACtrl.sendChId, DMA_EVENT_DONE);
            if (status != HAL_STATUS_OK) {
                return HIF_ERRCODE_IO_ERROR;
            }

            dmaCb.arg = NULL;
            dmaCb.cb = hif_PHY_DMA_SendErrIrqCallback;
            status = HAL_DMA_RegisterIRQ(hifDMACtrl.dmaDev, hifDMACtrl.sendChId, DMA_EVENT_ERR, &dmaCb);
            if (status != HAL_STATUS_OK) {
                return HIF_ERRCODE_IO_ERROR;
            }

            status = HAL_DMA_EnableIRQ(hifDMACtrl.dmaDev, hifDMACtrl.sendChId, DMA_EVENT_ERR);
            if (status != HAL_STATUS_OK) {
                return HIF_ERRCODE_IO_ERROR;
            }
        }
    }


    /* recv dma channel config */
    if (hifDMACtrl.recvShake != 0xFF) {
        hifDmaCfg.direction       = DMA_DIRECTION_PER2MEM;
        hifDmaCfg.priority        = DMA_PRIORITY_7;
        hifDmaCfg.srcBrustNum     = DMA_BURSTNUM_8;
        hifDmaCfg.dstBrustnum     = DMA_BURSTNUM_8;
        hifDmaCfg.srcWidth        = DMA_BITWIDTH_8;
        hifDmaCfg.dstWidth        = DMA_BITWIDTH_8;
        hifDmaCfg.srcHandShake    = hifDMACtrl.recvShake;
        hifDmaCfg.srcIncreMode    = DMA_ADDR_FIXED;
        hifDmaCfg.dstIncreMode    = DMA_ADDR_INCREMENT;
        hifDmaCfg.srcEnReload     = 0;
        hifDmaCfg.dstEnReload     = 0;
        hifDmaCfg.srcEnStatus     = 0;
        hifDmaCfg.dstEnStatus     = 0;

        status = HAL_DMA_Open(hifDMACtrl.dmaDev, &hifDmaCfg);
        if (status < 0) {
            return HIF_ERRCODE_NOT_READY;
        } else {
            hifDMACtrl.recvChId = status;
        }

#if (CONFIG_HIF_PM != 0)
        dmaCb.arg = NULL;
        dmaCb.cb = hif_PHY_DMA_ResumeCallback;
        HAL_DMA_ExtendControl(hifDMACtrl.dmaDev, hifDMACtrl.recvChId, DMA_EXTATTR_RESUME_CALLBACK, &dmaCb);
#endif
    }

    DMA_NodeConf_t scanNode;
    memset(&scanNode, 0, sizeof(DMA_NodeConf_t));
    HAL_DMA_InitNodeCtrl(hifDMACtrl.dmaDev, hifDMACtrl.sendChId, &scanNode, 0);
    hifDMACtrl.sendNodeParam = scanNode.nodeCtrl0;


    return HIF_ERRCODE_SUCCESS;
}


int hif_PHY_DMA_Close(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HAL_DMA_Close(hifDMACtrl.dmaDev, hifDMACtrl.sendChId);
    if (status != HAL_STATUS_OK) {
        return HIF_ERRCODE_NOT_READY;
    }

    status = HAL_DMA_Close(hifDMACtrl.dmaDev, hifDMACtrl.recvChId);
    if (status != HAL_STATUS_OK) {
        return HIF_ERRCODE_NOT_READY;
    }

    return HIF_ERRCODE_SUCCESS;
}

__hif_isr_text int hif_PHY_DMA_Recv(void * pDmaNode)
{
    HIF_Data_Node_t *pNode = (HIF_Data_Node_t *)pDmaNode;

    DMA_BlockConf_t DMA_BlockConf = {
        .srcAddr    = hifDMACtrl.recvPhyAddr,
        .dstAddr    = (uint32_t)pNode->data,
        .blockSize  = pNode->len,
    };

    return HAL_DMA_ConfigBlockTranfer(hifDMACtrl.dmaDev, hifDMACtrl.recvChId, &DMA_BlockConf);
}

__hif_isr_text int hif_PHY_DMA_RecvStart(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HAL_DMA_StartTransfer(hifDMACtrl.dmaDev, hifDMACtrl.recvChId);
    if (status != HAL_STATUS_OK) {
        status = HIF_ERRCODE_IO_ERROR;
    }

    return status;
}

__hif_isr_text int hif_PHY_DMA_RecvStop(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HAL_DMA_AbortTransfer(hifDMACtrl.dmaDev, hifDMACtrl.recvChId);
    if (status != HAL_STATUS_OK) {
        status = HIF_ERRCODE_IO_ERROR;
    }

    return status;
}

__hif_isr_text int hif_PHY_DMA_Send(void * pDmaNode)
{
    HAL_DMA_ConfigListTranfer(hifDMACtrl.dmaDev, hifDMACtrl.sendChId, (DMA_NodeConf_t *)pDmaNode);

    return 0;
}

__hif_isr_text int hif_PHY_DMA_SendStart(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HAL_DMA_StartTransfer(hifDMACtrl.dmaDev, hifDMACtrl.sendChId);
    if (status != HAL_STATUS_OK) {
        status = HIF_ERRCODE_IO_ERROR;
    }

    return status;
}

__hif_isr_text int hif_PHY_DMA_SendStop(void)
{
    int status = HIF_ERRCODE_SUCCESS;

    status = HAL_DMA_AbortTransfer(hifDMACtrl.dmaDev, hifDMACtrl.sendChId);
    if (status != HAL_STATUS_OK) {
        status = HIF_ERRCODE_IO_ERROR;
    }

    return status;
}

__hif_isr_text void hif_PHY_DMA_SendDataNodeInit(uint8_t *pData, uint16_t len, HIF_Data_Node_t *pDataNode, uint32_t flag)
{
    uint32_t addr = (uint32_t)pData;
    uint16_t addrMark = 0;

    if ((addr >= CONFIG_HIF_DMA_XIP_IBUS_ADDR_START) &&
        (addr <= CONFIG_HIF_DMA_XIP_IBUS_ADDR_EBND)) {
        addr |= CONFIG_HIF_DMA_XIP_DBUS_ADDR_MSK;
        addrMark = 0x0080;
    } else if ((addr >= 0x00500000) && (addr <= (0x005C0000 + 0x00040000))) {
        /* sram ibus */
        addrMark = (uint16_t)(addr >> 16) & 0x00FF;
        if (addr < 0x00540000) {
            addr = (addr - 0x00500000) + HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[0]);
        } else if (addr < 0x00580000) {
            addr = (addr - 0x00540000) + HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[1]);
        } else if (addr < 0x005C0000) {
            addr = (addr - 0x00580000) + HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[2]);
        } else {
            addr = (addr - 0x005C0000) + HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[3]);
        }
        addr |= 0x10000000;

        csi_dcache_clean_range((unsigned long *)pData, len);

    } else if ((addr >= 0x10500000) && (addr <= (0x10580000 + 0x00040000))) {

        csi_dcache_clean_range((unsigned long *)pData, len);
    }


    pDataNode->rsv0     = hifDMACtrl.sendPhyAddr;
    pDataNode->rsv1     = hifDMACtrl.sendNodeParam;
    if (flag & HIF_DATA_NODE_FLAG_LAST) {
        pDataNode->rsv1 |= 1;
    }
    pDataNode->len      = len & 0xFFF;
    pDataNode->data     = (uint8_t *)addr;
    pDataNode->next     = NULL;
    pDataNode->rsv2     = addrMark | (((uint16_t)flag) & HIF_DATA_NODE_FLAG_MSK);
}

__hif_sram_text uint8_t * hif_PHY_DMA_SendDataNodeDeinit(HIF_Data_Node_t *pDataNode)
{
    uint32_t addr = (uint32_t)pDataNode->data;
    uint16_t addrMark = (uint16_t)pDataNode->rsv2 & 0x00FF;

    if ((addr >= (CONFIG_HIF_DMA_XIP_IBUS_ADDR_START | CONFIG_HIF_DMA_XIP_DBUS_ADDR_MSK)) &&
        (addr <= (CONFIG_HIF_DMA_XIP_IBUS_ADDR_EBND | CONFIG_HIF_DMA_XIP_DBUS_ADDR_MSK))) {
        addr &= CONFIG_HIF_DMA_XIP_IBUS_ADDR_MSK;
    } else if ((addrMark >= 0x0050) && (addrMark <= 0x0060)) {
        addr &= 0x0FFFFFFF;
        if (addrMark < 0x0054) {
            addr = addr - HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[0]) + 0x00500000;
        } else if (addrMark < 0x0058) {
            addr = addr - HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[1]) + 0x00540000;
        } else if (addrMark < 0x005C) {
            addr = addr - HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[2]) + 0x00580000;
        } else {
            addr = addr - HW_GET_VAL(SYSCFG_DEV->CPU_I_REMAP_BDADDR[3]) + 0x005c0000;
        }
    }

    return (uint8_t *)addr;
}

__hif_isr_text uint32_t hif_PHY_DMA_GetRecvSize(void)
{
    return HAL_DMA_BlockSizeGet(hifDMACtrl.dmaDev, hifDMACtrl.recvChId);
}

__hif_isr_text static void hif_PHY_DMA_SendDoneIrqCallback(HAL_Dev_t * pDevice, void *arg)
{
    if (hifDMACtrl.sendCb != NULL) {
        hifDMACtrl.sendCb(NULL, 0, 0);
    }
}

__hif_sram_text static void hif_PHY_DMA_SendErrIrqCallback(HAL_Dev_t * pDevice, void *arg)
{


}

#if (CONFIG_HIF_PM != 0)
static void hif_PHY_DMA_ResumeCallback(HAL_Dev_t * pDevice, void *arg)
{
    hif_PHY_RecvCallback(0, 0);
}
#endif

#endif /* ((CONFIG_HIF == 1) && (CONFIG_HIF_PHY_DMA == 1)) */

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
