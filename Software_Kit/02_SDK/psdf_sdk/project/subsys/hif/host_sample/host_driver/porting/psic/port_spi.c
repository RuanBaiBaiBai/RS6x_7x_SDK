/**
 **************************************************************************************************
 * @file    port_spi.c
 * @brief   port communication adaptation for spi master.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"
#include "../../protocol/llc/host_llc.h"
#if (CFG_HOST_PORT_COM_EN == 1)
#if (CFG_HOST_PORT_OS_EN == 1)
#include "../include/hal_os.h"
#endif  /* CFG_HOST_PORT_OS_EN == 1 */
#include "../include/hal_com.h"

#if (CFG_HOST_PORT_COM_SPI_EN == 1)
#include "ll_rcc_dev.h"
#include "ll_gpio.h"
#include "ll_spi.h"
#include "hal_board.h"
#include "hal_dma.h"
#include "hal_spi.h"
#endif  /* CFG_HOST_PORT_COM_SPI_EN == 1 */
#endif  /* CFG_HOST_PORT_COM_EN == 1 */

#define DMA_BURSTNUM_SPI                    DMA_BURSTNUM_8  //DMA_BURSTNUM:4/8

#if (DMA_BURSTNUM_SPI == DMA_BURSTNUM_8)
#define HOST_SPI_TXFIFO_LEVEL           24
#define HOST_SPI_RXFIFO_LEVEL           8
#define HOST_SPI_DMA_SIZE               0x07
#elif  (DMA_BURSTNUM_SPI == DMA_BURSTNUM_4)
#define HOST_SPI_TXFIFO_LEVEL           16
#define HOST_SPI_RXFIFO_LEVEL           4
#define HOST_SPI_DMA_SIZE               0x03
#endif

/* Private Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_SPI_EN == 1))
static void * porting_port_spi_init(uint8_t id, uint32_t speed, void *arg);
static int porting_port_spi_deinit(void *bus);

static int porting_port_spi_open(void *bus, uint32_t cs_pin);
static int porting_port_spi_close(void *bus, uint32_t cs_pin);

static int porting_port_spi_write(void *bus, uint32_t cs_pin, void *pbuff, uint32_t size);

static int porting_port_spi_read(void *bus, uint32_t cs_pin, void *pbuff, uint32_t size);
#endif


/* Constants.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_SPI_EN == 1))
#define PORT_SPI_DEV_HANDLE                     ((void *)1)
#define PORT_DSPI_DEV_HANDLE                    ((void *)2)
#define PORT_QSPI_DEV_HANDLE                    ((void *)3)


#define PORT_SPI_TRANS_DIR_TX                  0
#define PORT_SPI_TRANS_DIR_RX                  1

#define PORT_SPI_DEV                           QSPI_DEV
#define PORT_SPI_CS_PIN                        0
#define PORT_SPI_SCK_PIN                       1
#define PORT_SPI_MOSI_PIN                      2
#define PORT_SPI_MISO_PIN                      3
#define PORT_SPI_HOLD_PIN                      4
#define PORT_SPI_WP_PIN                        5


static const host_com_ops_t spi_ops = {
    .init                    = porting_port_spi_init,
    .deinit                  = porting_port_spi_deinit,
    .open                    = porting_port_spi_open,
    .close                   = porting_port_spi_close,
    .write                   = porting_port_spi_write,
    .read                    = porting_port_spi_read,
};
#endif


/* Private var.
 * ------------------------------------------------------------------------------------------------
 */
static uint32_t spi_speed = 0;
static uint32_t spi_timeout_A = 0;
static const uint32_t spi_timeout_M_const = 20;

static uint16_t spi_recv_burst_size = 0;

static HAL_Dev_t *dmaDev = NULL;
static uint8_t dmaChannelId[2] = { 255, 255 };

static host_os_sem_t port_spi_tx_dma_done_sem = NULL;
static host_os_sem_t port_spi_rx_dma_done_sem = NULL;

static uint8_t current_cs_pin = 255;


/* Functions.
 * ------------------------------------------------------------------------------------------------
 */
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_SPI_EN == 1))
static void host_port_spi_tx_dma_done_callback(HAL_Dev_t * pDevice, void *arg);
static void host_port_spi_rx_dma_done_callback(HAL_Dev_t * pDevice, void *arg);
static int porting_port_spi_deinit(void *bus);


static int porting_port_spi_dma_init(void *bus, uint8_t dir)
{
    int status = 0;
    HAL_Callback_t temp_cb;
    DMA_ChannelConf_t dmaCfg;

    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (dmaDev == NULL) {
        dmaDev = HAL_DMA_Init(DMA0_ID);
        if (dmaDev == NULL) {
            HOST_LOG_ERR("dma init fail...\n");
            return -1;
        }
    }

    host_os_memset((void *)&dmaCfg, 0, sizeof(dmaCfg));

    dmaCfg.priority        = DMA_PRIORITY_7;
    dmaCfg.srcBrustNum     = DMA_BURSTNUM_SPI;
    dmaCfg.dstBrustnum     = DMA_BURSTNUM_SPI;
    dmaCfg.srcWidth        = DMA_BITWIDTH_8;
    dmaCfg.dstWidth        = DMA_BITWIDTH_8;
    if (dir == PORT_SPI_TRANS_DIR_TX) {
        dmaCfg.direction       = DMA_DIRECTION_MEM2PER;
        dmaCfg.dstHandShake    = DMA_HANDSHAKE_TXFIFO_SPI0;
        dmaCfg.srcIncreMode    = DMA_ADDR_INCREMENT;
        dmaCfg.dstIncreMode    = DMA_ADDR_FIXED;
    } else {
        dmaCfg.direction       = DMA_DIRECTION_PER2MEM;
        dmaCfg.srcHandShake    = DMA_HANDSHAKE_RXFIFO_SPI0;
        dmaCfg.srcIncreMode    = DMA_ADDR_FIXED;
        dmaCfg.dstIncreMode    = DMA_ADDR_INCREMENT;
    }

    status = HAL_DMA_Open(dmaDev, &dmaCfg);
    if (status < 0) {
        HOST_LOG_ERR("dma open channel fail (%d)\n", status);
        return status;
    } else {
        dmaChannelId[dir] = status;
    }

    temp_cb.arg = bus;
    if (dir == PORT_SPI_TRANS_DIR_TX) {
        temp_cb.cb = host_port_spi_tx_dma_done_callback;
    } else {
        temp_cb.cb = host_port_spi_rx_dma_done_callback;
    }

    status = HAL_DMA_RegisterIRQ(dmaDev, dmaChannelId[dir], DMA_EVENT_DONE, &temp_cb);
    if (status != HAL_STATUS_OK) {
        HOST_LOG_ERR("dma register channel done irq fail (%d)\n", status);
        return status;
    }
    status = HAL_DMA_EnableIRQ(dmaDev, dmaChannelId[dir], DMA_EVENT_DONE);
    if (status != HAL_STATUS_OK) {
        HOST_LOG_ERR("dma enable channel done irq fail (%d)\n", status);
        return status;
    }

    if (port_spi_tx_dma_done_sem == NULL) {
        port_spi_tx_dma_done_sem = host_os_sem_create(0, 1);
        if (port_spi_tx_dma_done_sem == NULL) {
            HOST_LOG_ERR("create port spi tx dma done sem fail\n");
            return HOST_ERRCODE_SYSERR;
        }
    }
    if (port_spi_rx_dma_done_sem == NULL) {
        port_spi_rx_dma_done_sem = host_os_sem_create(0, 1);
        if (port_spi_rx_dma_done_sem == NULL) {
            HOST_LOG_ERR("create port spi rx dma done sem fail\n");
            host_os_sem_delete(port_spi_tx_dma_done_sem);
            port_spi_tx_dma_done_sem = NULL;
            return HOST_ERRCODE_SYSERR;
        }
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_spi_dma_deinit(void *bus)
{
    HAL_Status_t status = HAL_STATUS_OK;

    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (dmaDev == NULL) {
        return HOST_ERRCODE_SUCCESS;
    }

    HAL_DMA_Close(dmaDev, dmaChannelId[PORT_SPI_TRANS_DIR_TX]);
    dmaChannelId[PORT_SPI_TRANS_DIR_TX] = 255;
    HAL_DMA_Close(dmaDev, dmaChannelId[PORT_SPI_TRANS_DIR_RX]);
    dmaChannelId[PORT_SPI_TRANS_DIR_RX] = 255;

    status = HAL_DMA_DeInit(dmaDev);
    if (status != HAL_STATUS_OK) {
        HOST_LOG_ERR("deinit dma fail");
//        return status;
    }
    dmaDev = NULL;

    if (port_spi_tx_dma_done_sem != NULL) {
        host_os_sem_delete(port_spi_tx_dma_done_sem);
        port_spi_tx_dma_done_sem = NULL;
    }
    if (port_spi_rx_dma_done_sem != NULL) {
        host_os_sem_delete(port_spi_rx_dma_done_sem);
        port_spi_rx_dma_done_sem = NULL;
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_spi_dma_config(void *bus, uint8_t dir, uint32_t addr, uint32_t size)
{
    HAL_Status_t status = 0;
    DMA_BlockConf_t DMA_BlockConf = {0x00};

    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (dir == PORT_SPI_TRANS_DIR_TX) {
        dir = PORT_SPI_TRANS_DIR_TX;
    } else {
        dir = PORT_SPI_TRANS_DIR_RX;
    }

    if (dir == PORT_SPI_TRANS_DIR_TX) {
        DMA_BlockConf.srcAddr = addr;
        DMA_BlockConf.dstAddr = (uint32_t)&PORT_SPI_DEV->DR;
    } else {
        DMA_BlockConf.srcAddr = (uint32_t)&PORT_SPI_DEV->DR;
        DMA_BlockConf.dstAddr = addr;
    }
    DMA_BlockConf.blockSize = size;

    status = HAL_DMA_ConfigBlockTranfer(dmaDev, dmaChannelId[dir], &DMA_BlockConf);

    return status == HAL_STATUS_OK ? HOST_ERRCODE_SUCCESS : status;
}


static int porting_port_spi_dma_start(void *bus, uint8_t dir)
{
    HAL_Status_t status = 0;

    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (dir == PORT_SPI_TRANS_DIR_TX) {
        dir = PORT_SPI_TRANS_DIR_TX;
    } else {
        dir = PORT_SPI_TRANS_DIR_RX;
    }

    status = HAL_DMA_StartTransfer(dmaDev, dmaChannelId[dir]);

    return status == HAL_STATUS_OK ? HOST_ERRCODE_SUCCESS : status;
}


static int porting_port_spi_dma_stop(void *bus, uint8_t dir)
{
    HAL_Status_t status = 0;

    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (dir == PORT_SPI_TRANS_DIR_TX) {
        dir = PORT_SPI_TRANS_DIR_TX;
    } else {
        dir = PORT_SPI_TRANS_DIR_RX;
    }

    status = HAL_DMA_AbortTransfer(dmaDev, dmaChannelId[dir]);

    return status == HAL_STATUS_OK ? HOST_ERRCODE_SUCCESS : status;
}


static void host_port_spi_tx_dma_done_callback(HAL_Dev_t * pDevice, void *arg)
{
    uint32_t start, end;

//    csi_dcache_clean();
//    csi_dcache_invalid();

    porting_port_spi_dma_stop(arg, PORT_SPI_TRANS_DIR_TX);
    while (!LL_SPI_BusIsActiveFlag(PORT_SPI_DEV, LL_QSPI_STATUS_TXE)) ;
    start = HAL_BOARD_GetTime(HAL_TIME_US);
    end = start + 10;  // wait 10us, 1MHz trans 1 byte use 8us
    while (LL_SPI_BusIsActiveFlag(PORT_SPI_DEV, LL_QSPI_STATUS_BUSY)) {
        if (HAL_BOARD_GetTime(HAL_TIME_US) > end) {
            break;
        }
    }
    if (current_cs_pin < 16) {
        LL_GPIO_SetOutputPin(GPIOA_DEV, HW_BIT(current_cs_pin));
    } else {
        LL_GPIO_SetOutputPin(GPIOB_DEV, HW_BIT(current_cs_pin - 16));
    }
    host_os_sem_give(port_spi_tx_dma_done_sem);
}


static void host_port_spi_rx_dma_done_callback(HAL_Dev_t * pDevice, void *arg)
{
//    csi_dcache_clean();
//    csi_dcache_invalid();
    if (arg == PORT_SPI_DEV_HANDLE) {
        porting_port_spi_dma_stop(arg, PORT_SPI_TRANS_DIR_RX);
        LL_GPIO_SetOutputPin(GPIOA_DEV, HW_BIT(current_cs_pin));
    }
    host_os_sem_give(port_spi_rx_dma_done_sem);
}


static void * porting_port_spi_init(uint8_t id, uint32_t speed, void *arg)
{
    int status = HOST_ERRCODE_SUCCESS;
    int32_t freq = 0;
    int16_t div = 0;
    uint32_t spi_mode = (uint32_t)(uintptr_t)arg;
    if (id != 0) {
        return NULL;
    }
    if (speed <= 0) {
        return NULL;
    }

    /* 1. config io */
//    LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_CS_PIN, LL_GPIOA_P0_F2_SPI_CS);
    LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_SCK_PIN, LL_GPIOA_P1_F2_SPI_CLK);
    LL_GPIO_SetPinStreng(GPIOA_DEV, PORT_SPI_SCK_PIN, LL_GPIO_DRIVING_LVL_1);
    LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_MOSI_PIN, LL_GPIOA_P2_F2_SPI_MOSI);
    LL_GPIO_SetPinStreng(GPIOA_DEV, PORT_SPI_MOSI_PIN, LL_GPIO_DRIVING_LVL_1);
    LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_MISO_PIN, LL_GPIOA_P3_F2_SPI_MISO);
    LL_GPIO_SetPinStreng(GPIOA_DEV, PORT_SPI_MISO_PIN, LL_GPIO_DRIVING_LVL_1);
    if (spi_mode == LLC_BUS_PARAM_QSPI) {
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_HOLD_PIN, GPIOA_P4_F2_SPI_HOLD);
        LL_GPIO_SetPinStreng(GPIOA_DEV, PORT_SPI_HOLD_PIN, LL_GPIO_DRIVING_LVL_1);
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_WP_PIN, GPIOA_P5_F2_SPI_WP);
        LL_GPIO_SetPinStreng(GPIOA_DEV, PORT_SPI_WP_PIN, LL_GPIO_DRIVING_LVL_1);
    }

    /* 2. config clock */
    LL_RCC_SPI_SetClockSource(LL_RCC_SPI_AHB);
    freq = HAL_BOARD_GetFreq(CLOCK_AHB) + 100000;
    if (freq >= 200000000) {
        LL_RCC_SPI_SetPrescaler(LL_RCC_SPI_DIV_2);
        freq /= 2;
    } else {
        LL_RCC_SPI_SetPrescaler(LL_RCC_SPI_DIV_1);
    }
    LL_RCC_SPI_DisableBusClock();
    LL_RCC_SPI_DisableClock();
    LL_RCC_SPI_Reset();
    LL_RCC_SPI_EnableBusClock();
    LL_RCC_SPI_EnableClock();
    div = (freq / speed) & 0xFFFFFFFE;
    LL_SPI_SetPrescaler(PORT_SPI_DEV, div);

    /* 3. config spi */
    LL_SPI_Disable(PORT_SPI_DEV);

    div = LL_SPI_GetPrescaler(PORT_SPI_DEV);
    if ((speed >= 28000000) && (div >= (LL_QSPI_CLK_DIV4 << 1))) {
        /* mode1 */
        LL_SPI_SlvClkSelect(PORT_SPI_DEV, LL_QSPI_SLVCLK_SSICLK);
    } else {
        /* mode2 */
        LL_SPI_SetMode(PORT_SPI_DEV, LL_QSPI_MST_MODE);
        LL_SPI_EnableSlvClkIn(PORT_SPI_DEV);
        LL_SPI_SetMode(PORT_SPI_DEV, LL_QSPI_SLV_MODE);
        LL_SPI_SlvClkSelect(PORT_SPI_DEV, LL_QSPI_SLVCLK_FUNCLK);
        LL_SPI_DisableSlvClkIn(PORT_SPI_DEV);
    }

    LL_SPI_SetMode(PORT_SPI_DEV, LL_QSPI_MST_MODE);

    LL_SPI_SetClockPhase(PORT_SPI_DEV, LL_QSPI_CPHA_0);
    LL_SPI_SetClockPolarity(PORT_SPI_DEV, LL_QSPI_CPOL_0);

    LL_SPI_SetWorkLines(PORT_SPI_DEV, (spi_mode - 1));
    LL_SPI_SetWorkFormat(PORT_SPI_DEV, LL_QSPI_FMODE_SPI);
    LL_SPI_SetDataWidth(PORT_SPI_DEV, LL_QSPI_DFS_8BIT);

    if (spi_mode != LLC_BUS_PARAM_SPI) {
        LL_SPI_SetAddrLenght(PORT_SPI_DEV, SPI_DATAWIDTH_8BIT/4);
        LL_SPI_SetInstructLenght(PORT_SPI_DEV, 0);
        LL_SPI_SetTransType(PORT_SPI_DEV, LL_QSPI_TRAN_INS_AND_ADDR_TYPE);
        LL_SPI_StretchEnable(PORT_SPI_DEV);
    } else {
        LL_SPI_StretchDisable(PORT_SPI_DEV);
    }

    LL_SPI_SetTxFifoStartLevel(PORT_SPI_DEV, LL_QSPI_FIFO_LEVEL(1));
    LL_SPI_SetTxFifoThreshold(PORT_SPI_DEV, LL_QSPI_FIFO_LEVEL(16));  // diff
    LL_SPI_SetRxFifoThreshold(PORT_SPI_DEV, LL_QSPI_FIFO_LEVEL(1));

    LL_SPI_SetDMALevel_TX(PORT_SPI_DEV, LL_QSPI_FIFO_LEVEL(HOST_SPI_TXFIFO_LEVEL));
    LL_SPI_SetDMALevel_RX(PORT_SPI_DEV, LL_QSPI_FIFO_LEVEL(HOST_SPI_RXFIFO_LEVEL));

    LL_SPI_DisableSlaveSelect(PORT_SPI_DEV);
    LL_SPI_DisableSStoggle(PORT_SPI_DEV);
    LL_SPI_SetReciveNum(PORT_SPI_DEV, spi_recv_burst_size);

//    freq = HAL_BOARD_GetFreq(CLOCK_PLL_SOC);
//    if (freq >= 92000000) {
//        LL_SPI_SetRxSampleDelay(PORT_SPI_DEV, LL_QSPI_SAMPLE_DELAY_3);
//    } else if (freq >= 46000000) {
//        LL_SPI_SetRxSampleDelay(PORT_SPI_DEV, LL_QSPI_SAMPLE_DELAY_2);
//    } else if (freq >= 23000000) {
//        LL_SPI_SetRxSampleDelay(PORT_SPI_DEV, LL_QSPI_SAMPLE_DELAY_1);
//    } else {
//        LL_SPI_SetRxSampleDelay(PORT_SPI_DEV, 0);
//    }
    // diff
    if (speed > 32000000) { // >32MHz
        LL_SPI_SetRxSampleDelay(PORT_SPI_DEV, 2);
    } else {
        LL_SPI_SetRxSampleDelay(PORT_SPI_DEV, 0);
    }
//    LL_SPI_DisableRxHalfSampleDelayClk(PORT_SPI_DEV);  // diff
    LL_SPI_EnableRxHalfSampleDelayClk(PORT_SPI_DEV);

    LL_SPI_ITDisable(PORT_SPI_DEV, LL_QSPI_ITE_ALL);
    LL_SPI_ITFlagClearALL(PORT_SPI_DEV);

//    LL_SPI_Enable(PORT_SPI_DEV);

    status = porting_port_spi_dma_init((void *)(uintptr_t)spi_mode, PORT_SPI_TRANS_DIR_TX);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("init spi tx dma fail\n");
        return NULL;
    }

    status = porting_port_spi_dma_init((void *)(uintptr_t)spi_mode, PORT_SPI_TRANS_DIR_RX);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("init spi rx dma fail\n");
        porting_port_spi_deinit((void *)(uintptr_t)spi_mode);
        return NULL;
    }

    // A = 8*ceil((2^M)/(1000X)) = 8*ceil((1000 * 2^M)/speed) = 8*ceil(K/speed) = 8*floor((K+speed-1)/speed)
    do {
        uint64_t K = (1ULL << spi_timeout_M_const) * 1000ULL;
        spi_speed = speed;
        spi_timeout_A = 8 * (uint32_t)((K + speed - 1) / speed);
        if (spi_timeout_A == 0) {
            spi_timeout_A = 8392;  // if speed = 1MHz, M = 20
        }
    } while (0);

    return (void *)(uintptr_t)spi_mode;
}


static int porting_port_spi_deinit(void *bus)
{
    int status = HOST_ERRCODE_SUCCESS;

    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    status = porting_port_spi_dma_deinit(bus);
    if (status != HOST_ERRCODE_SUCCESS) {
        HOST_LOG_ERR("deinit spi dma fail(%d)\n", status);
        return status;
    }

    LL_RCC_SPI_DisableClock();
    LL_RCC_SPI_DisableBusClock();

    LL_SPI_Disable(PORT_SPI_DEV);

    LL_RCC_SPI_DisableBusClock();
    LL_RCC_SPI_DisableClock();
    LL_RCC_SPI_Reset();
    LL_RCC_SPI_DisableClock();
    LL_RCC_SPI_DisableBusClock();

    LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_SCK_PIN, LL_GPIOx_Pn_F15_DIS);
    LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_MOSI_PIN, LL_GPIOx_Pn_F15_DIS);
    LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_MISO_PIN, LL_GPIOx_Pn_F15_DIS);
    if (bus == PORT_QSPI_DEV_HANDLE) {
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_HOLD_PIN, LL_GPIOx_Pn_F15_DIS);
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, PORT_SPI_WP_PIN, LL_GPIOx_Pn_F15_DIS);
    }
    spi_speed = 0;
    spi_timeout_A = 0;

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_spi_open(void *bus, uint32_t cs_pin)
{
    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (cs_pin < 16) {
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, cs_pin, LL_GPIOx_Pn_F1_OUTPUT);
        LL_GPIO_SetPinOutputType(GPIOA_DEV, cs_pin, LL_GPIO_OUTPUT_PUSH_PULL);
        LL_GPIO_SetPinStreng(GPIOA_DEV, cs_pin, LL_GPIO_DRIVING_LVL_1);
        LL_GPIO_SetPinPull(GPIOA_DEV, cs_pin, LL_GPIO_PULL_UP);
        LL_GPIO_SetOutputPin(GPIOA_DEV, HW_BIT(cs_pin));
    } else {
        cs_pin = cs_pin - 16;
        LL_GPIO_SetPinFuncMode(GPIOB_DEV, cs_pin, LL_GPIOx_Pn_F1_OUTPUT);
        LL_GPIO_SetPinOutputType(GPIOB_DEV, cs_pin, LL_GPIO_OUTPUT_PUSH_PULL);
        LL_GPIO_SetPinStreng(GPIOB_DEV, cs_pin, LL_GPIO_DRIVING_LVL_1);
        LL_GPIO_SetPinPull(GPIOB_DEV, cs_pin, LL_GPIO_PULL_UP);
        LL_GPIO_SetOutputPin(GPIOB_DEV, HW_BIT(cs_pin));
    }
    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_spi_close(void *bus, uint32_t cs_pin)
{
    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }
    if (cs_pin < 16) {
        LL_GPIO_SetPinFuncMode(GPIOA_DEV, cs_pin, LL_GPIOx_Pn_F15_DIS);
    } else {
        cs_pin = cs_pin - 16;
        LL_GPIO_SetPinFuncMode(GPIOB_DEV, cs_pin, LL_GPIOx_Pn_F15_DIS);
    }

    return HOST_ERRCODE_SUCCESS;
}


static int porting_port_spi_write(void *bus, uint32_t cs_pin, void *pbuff, uint32_t size)
{
    int status = HOST_ERRCODE_SUCCESS;
    uint32_t msec = 0;
    uint32_t cs_port = 0;
    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    LL_SPI_Disable(PORT_SPI_DEV);
    if (bus != PORT_SPI_DEV_HANDLE) {
        LL_SPI_SetInstructLenght(PORT_SPI_DEV, 0);
        LL_SPI_SetAddrLenght(PORT_SPI_DEV, SPI_DATAWIDTH_8BIT/4);
        LL_SPI_SetTransType(PORT_SPI_DEV, SPI_TRANS_ALL_SPECIFIED);
    }
    LL_SPI_EnableDMAReq_TX(PORT_SPI_DEV);
    LL_SPI_SetTransferMode(PORT_SPI_DEV, LL_QSPI_TMODE_TXONLY);
    LL_SPI_Enable(PORT_SPI_DEV);
    current_cs_pin = cs_pin;
    if (cs_pin >= 16) {
        cs_port = 1;
    }

    // trans N bytes data
    // t = N / (speed / 8) s = 1000 * N / (speed / 8) ms
    // if speed = X MHz, then
    // t = 8N / 1000X ms
    // shrinking t to get
    // t = 8N * (1 / 1000X) ms <= 8N * (C / 2^M) ms = 8NC / 2^M ms
    // T = ceil(t) <= ceil(8NC / 2^M) ms = floor((8NC + 2^M - 1) / 2^M) ms
    // for C / 2^M >= 1 / 1000X, let C = ceil(2^M / 1000X), then
    //            (            (   2^M   )            )
    //            |  8N * ceil | ------- | + 2^M - 1  |
    //            |            (  1000X  )            |
    // T <= floor | --------------------------------- | ms
    //            (                2^M                )
    //             8N
    // delta_t <= -----
    //             2^M
    //
    // delta_T <= 1, if delta_t <= 1
    //
    // if N=4096, then M>15
    msec = (spi_timeout_A * size + ((1UL << spi_timeout_M_const) - 1)) >> spi_timeout_M_const;
    if (msec == 0) {
        msec = 2;
    } else if (msec + 2 != 0) {
        msec = msec + 2;
    } else {
        msec = msec + 1;
    }

    csi_dcache_clean_range((unsigned long *)pbuff, size);
    porting_port_spi_dma_config(bus, PORT_SPI_TRANS_DIR_TX, (uint32_t)pbuff, size);
    if (cs_port == 0) {
        LL_GPIO_ResetOutputPin(GPIOA_DEV, HW_BIT(cs_pin));
    } else {
        LL_GPIO_ResetOutputPin(GPIOB_DEV, HW_BIT(cs_pin - 16));
    }
    porting_port_spi_dma_start(bus, PORT_SPI_TRANS_DIR_TX);
    status = host_os_sem_take(port_spi_tx_dma_done_sem, msec);
    if (status != HOST_ERRCODE_SUCCESS) {
        porting_port_spi_dma_stop(bus, PORT_SPI_TRANS_DIR_TX);
        if (cs_port == 0) {
            LL_GPIO_SetOutputPin(GPIOA_DEV, HW_BIT(cs_pin));
        } else {
            LL_GPIO_SetOutputPin(GPIOB_DEV, HW_BIT(cs_pin - 16));
        }
        return status;
    }
    LL_SPI_Disable(PORT_SPI_DEV);

//    porting_port_spi_dma_stop(bus, PORT_SPI_TRANS_DIR_TX);

    return size;
}


static int porting_port_spi_read(void *bus, uint32_t cs_pin, void *pbuff, uint32_t size)
{
    int status = HOST_ERRCODE_SUCCESS;
    uint32_t msec = 0;
    uint32_t cs_port = 0;
    if ((bus != PORT_SPI_DEV_HANDLE) && (bus != PORT_DSPI_DEV_HANDLE) && (bus != PORT_QSPI_DEV_HANDLE)) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (NULL == pbuff || 0 == size) {
        return HOST_ERRCODE_INVALID_PARAM;
    }

    if (bus == PORT_SPI_DEV_HANDLE) {
        LL_SPI_Disable(PORT_SPI_DEV);
        LL_SPI_EnableDMAReq_RX(PORT_SPI_DEV);
        LL_SPI_SetTransferMode(PORT_SPI_DEV, LL_QSPI_TMODE_RXONLY);
        current_cs_pin = cs_pin;

    //    msec = (spi_timeout_A * size + ((1UL << spi_timeout_M_const) - 1)) >> spi_timeout_M_const;
    //    if (msec == 0) {
    //        msec = 2;
    //    } else if (msec + 2 != 0) {
    //        msec = msec + 2;
    //    } else {
    //        msec = msec + 1;
    //    }
        msec = 10;

        csi_dcache_clean_range((unsigned long *)pbuff, size);
        spi_recv_burst_size = size;
        LL_SPI_SetReciveNum(PORT_SPI_DEV, spi_recv_burst_size);
        LL_SPI_Enable(PORT_SPI_DEV);
        porting_port_spi_dma_config(bus, PORT_SPI_TRANS_DIR_RX, (uint32_t)pbuff, size);
        LL_GPIO_ResetOutputPin(GPIOA_DEV, HW_BIT(cs_pin));
        porting_port_spi_dma_start(bus, PORT_SPI_TRANS_DIR_RX);
        LL_SPI_TransmitData(PORT_SPI_DEV, 0);  // triger sclk

        status = host_os_sem_take(port_spi_rx_dma_done_sem, msec);
        if (status != HOST_ERRCODE_SUCCESS) {
            porting_port_spi_dma_stop(bus, PORT_SPI_TRANS_DIR_RX);
            LL_GPIO_SetOutputPin(GPIOA_DEV, HW_BIT(cs_pin));
            return status;
        }
        csi_dcache_invalid_range((unsigned long *)pbuff, size);

        LL_SPI_Disable(PORT_SPI_DEV);
    } else {
        LL_SPI_Disable(PORT_SPI_DEV);
        LL_SPI_SetInstructLenght(PORT_SPI_DEV, 0);
        LL_SPI_SetAddrLenght(PORT_SPI_DEV, 0);
        LL_SPI_SetTransType(PORT_SPI_DEV, SPI_TRANS_ALL_SPECIFIED);
        spi_recv_burst_size = size;
        LL_SPI_SetReciveNum(PORT_SPI_DEV, spi_recv_burst_size);
        LL_SPI_EnableDMAReq_RX(PORT_SPI_DEV);
        LL_SPI_SetTransferMode(PORT_SPI_DEV, LL_QSPI_TMODE_RXONLY);
        LL_SPI_Enable(PORT_SPI_DEV);
        current_cs_pin = cs_pin;
        if (cs_pin >= 16) {
            cs_port = 1;
        }

        if (cs_port == 0) {
            LL_GPIO_ResetOutputPin(GPIOA_DEV, HW_BIT(cs_pin));
        } else {
            LL_GPIO_ResetOutputPin(GPIOB_DEV, HW_BIT(cs_pin - 16));
        }
        uint32_t dma_size = size & (~HOST_SPI_DMA_SIZE);
        if (dma_size) {
            csi_dcache_clean_range((unsigned long *)pbuff, dma_size);
            porting_port_spi_dma_config(bus, PORT_SPI_TRANS_DIR_RX, (uint32_t)pbuff, dma_size);
            porting_port_spi_dma_start(bus, PORT_SPI_TRANS_DIR_RX);
        }
        LL_SPI_TransmitData(PORT_SPI_DEV, 0);  // triger sclk

        if (dma_size) {
            msec = (spi_timeout_A * size + ((1UL << spi_timeout_M_const) - 1)) >> spi_timeout_M_const;
            if (msec == 0) {
                msec = 2;
            } else if (msec + 2 != 0) {
                msec = msec + 2;
            } else {
                msec = msec + 1;
            }

            status = host_os_sem_take(port_spi_rx_dma_done_sem, msec);
            if (HOST_ERRCODE_SUCCESS == status) {
                 csi_dcache_invalid_range((unsigned long *)pbuff, dma_size);
            }
            porting_port_spi_dma_stop(bus, PORT_SPI_TRANS_DIR_RX);
        }

        uint8_t *read_data = (uint8_t *)pbuff;
        while (dma_size < size) {
            if (LL_SPI_GetRxFifoLevel(PORT_SPI_DEV)) {
                LL_SPI_ReceiveData8(PORT_SPI_DEV, &read_data[dma_size]);
                dma_size++;
            }
        }
        if (cs_port == 0) {
            LL_GPIO_SetOutputPin(GPIOA_DEV, HW_BIT(cs_pin));
        } else {
            LL_GPIO_SetOutputPin(GPIOB_DEV, HW_BIT(cs_pin - 16));
        }
        LL_SPI_Disable(PORT_SPI_DEV);
        }

    return size;
}
#endif


host_com_ops_t *porting_port_spi_get_com_ops(uint8_t id)
{
#if ((CFG_HOST_PORT_COM_EN == 1) && (CFG_HOST_PORT_COM_SPI_EN == 1))
    if (id == 0) {
        return (host_com_ops_t *)&spi_ops;
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
