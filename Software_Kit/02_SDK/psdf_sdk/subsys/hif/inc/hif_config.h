/**
 ******************************************************************************
 * @file    hif_config.h
 * @brief   hif config define.
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


#ifndef __HIF_CONFIG_H__
#define __HIF_CONFIG_H__

/* Include Files */
#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Config.
 * ----------------------------------------------------------------------------
 */

#ifndef CONFIG_HIF
#define CONFIG_HIF                                              0
#endif


/*
 * 0 : Ultra low power
 * 1 : Normal
 */
#ifndef CONFIG_HIF_MODE
#define CONFIG_HIF_MODE                                         1
#endif



/* mini mode
 * -----------------------
 */
#if (CONFIG_HIF_MODE == 0)

/* support: uart, spi
 * bit 1: uart
 * bit 2: i2c
 * bit 4: spi
 * bit 8: can
 */
#ifndef CONFIG_HIF_PHY_TYPE
#define CONFIG_HIF_PHY_TYPE                         7
#endif


#ifndef CONFIG_HIF_MEM_CMD_RSP_SIZE
#define CONFIG_HIF_MEM_CMD_RSP_SIZE                 512
#endif

#ifndef CONFIG_HIF_TASK_STACK_SIZE
#define CONFIG_HIF_TASK_STACK_SIZE                  512
#endif

/*
 * report
 */
#ifndef CONFIG_HIF_MEM_FRAME_NODE_SIZE
#define CONFIG_HIF_MEM_FRAME_NODE_SIZE              4
#endif

#ifndef CONFIG_HIF_MEM_DATA_NODE_SIZE
#define CONFIG_HIF_MEM_DATA_NODE_SIZE               16
#endif

#ifndef CONFIG_HIF_MEM_SEM_NODE_SIZE
#define CONFIG_HIF_MEM_SEM_NODE_SIZE                2
#endif


/*
 * 0: disable
 * 1: enable
 */
#ifndef CONFIG_HIF_SPLIT_ENA
#define CONFIG_HIF_SPLIT_ENA                        0
#endif

/*
 * 0: disable
 * 1: enable
 */
#ifndef CONFIG_HIF_RETRY_ENA
#define CONFIG_HIF_RETRY_ENA                        0
#endif

#endif /* CONFIG_HIF_MODE == 0 */



/* nomal mode
 * -----------------------
 */
#if (CONFIG_HIF_MODE == 1)

/* support: uart, spi
 * bit 1: uart
 * bit 2: i2c
 * bit 4: spi
 * bit 8: can
 */
#ifndef CONFIG_HIF_PHY_TYPE
#define CONFIG_HIF_PHY_TYPE                         7
#endif


#ifndef CONFIG_HIF_MEM_CMD_RSP_SIZE
#define CONFIG_HIF_MEM_CMD_RSP_SIZE                 512
#endif

#ifndef CONFIG_HIF_TASK_STACK_SIZE
#define CONFIG_HIF_TASK_STACK_SIZE                  1024
#endif

/*
 * report
 */
#ifndef CONFIG_HIF_MEM_FRAME_NODE_SIZE
#define CONFIG_HIF_MEM_FRAME_NODE_SIZE              16
#endif

#ifndef CONFIG_HIF_MEM_DATA_NODE_SIZE
#define CONFIG_HIF_MEM_DATA_NODE_SIZE               64
#endif

#ifndef CONFIG_HIF_MEM_SEM_NODE_SIZE
#define CONFIG_HIF_MEM_SEM_NODE_SIZE                4
#endif


/*
 * 0: disable
 * 1: enable
 */
#ifndef CONFIG_HIF_SPLIT_ENA
#define CONFIG_HIF_SPLIT_ENA                        0
#endif

/*
 * 0: disable
 * 1: enable
 */
#ifndef CONFIG_HIF_RETRY_ENA
#define CONFIG_HIF_RETRY_ENA                        0
#endif

#endif /* CONFIG_HIF_MODE == 1 */


#ifndef CONFIG_HIF_APP_DATA_POOL
#define CONFIG_HIF_APP_DATA_POOL                    0
#endif
/* Main Configuration.
 * ============================================================================
 */

/* Constant Definition
 * cannot be modified, easy to read.
 * -----------------------------------------
 */

/* HIF switch deine
 */
#ifndef CONFIG_HIF_ENA
#define CONFIG_HIF_ENA                      1
#endif

#ifndef CONFIG_HIF_DIS
#define CONFIG_HIF_DIS                      0
#endif


/* PHY type define
 */
#ifndef CONFIG_HIF_PHY_TYPE_DIS
#define CONFIG_HIF_PHY_TYPE_DIS             0
#endif

#ifndef CONFIG_HIF_PHY_TYPE_UART
#define CONFIG_HIF_PHY_TYPE_UART            1
#endif

#ifndef CONFIG_HIF_PHY_TYPE_IIC
#define CONFIG_HIF_PHY_TYPE_IIC             2
#endif

#ifndef CONFIG_HIF_PHY_TYPE_SPI
#define CONFIG_HIF_PHY_TYPE_SPI             4
#endif

#ifndef CONFIG_HIF_PHY_TYPE_CAN
#define CONFIG_HIF_PHY_TYPE_CAN             8
#endif

/* PHY transfer mode define
 */
#ifndef CONFIG_HIF_PHY_TRANS_MODE_DIS
#define CONFIG_HIF_PHY_TRANS_MODE_DIS       0
#endif

#ifndef CONFIG_HIF_PHY_TRANS_MODE_INT
#define CONFIG_HIF_PHY_TRANS_MODE_INT       1
#endif

#ifndef CONFIG_HIF_PHY_TRANS_MODE_DMA
#define CONFIG_HIF_PHY_TRANS_MODE_DMA       2
#endif



/* Main Configuration
 * -------------------------------------------------
 */

/* Log config
 */
#ifndef CONFIG_HIF_LOG_LEVEL
#define CONFIG_HIF_LOG_LEVEL                        1
#endif

#ifndef CONFIG_HIF_LOG_IO_TAG
#define CONFIG_HIF_LOG_IO_TAG                       0
#endif

#ifndef CONFIG_HIF_LOG_IO_MEM
#define CONFIG_HIF_LOG_IO_MEM                       0
#endif

#ifndef CONFIG_HIF_LOG_IO_MEM_SIZE
#define CONFIG_HIF_LOG_IO_MEM_SIZE                  128
#endif

#ifndef CONFIG_HIF_LOG_TIME
#define CONFIG_HIF_LOG_TIME                         0
#endif

#ifndef CONFIG_HIF_LOG_TIME_SIZE
#define CONFIG_HIF_LOG_TIME_SIZE                    64
#endif


/* Feature config
 */
#ifndef CONFIG_HIF_SPLIT_ENA
#define CONFIG_HIF_SPLIT_ENA                        CONFIG_HIF_DIS
#endif

#ifndef CONFIG_HIF_RETRY_ENA
#define CONFIG_HIF_RETRY_ENA                        CONFIG_HIF_DIS
#endif


/*
 * PHY config
 */
#ifndef CONFIG_HIF_PHY_TYPE
#define CONFIG_HIF_PHY_TYPE                         (CONFIG_HIF_PHY_TYPE_UART | CONFIG_HIF_PHY_TYPE_SPI | CONFIG_HIF_PHY_TYPE_IIC)
#endif


/* PM config
 */
#ifndef CONFIG_HIF_PM
#if (CONFIG_PM != 0)
#define CONFIG_HIF_PM                               CONFIG_HIF_ENA
#else
#define CONFIG_HIF_PM                               CONFIG_HIF_DIS
#endif
#endif


/* Buff config
 */
#ifndef CONFIG_HIF_MEM_CMD_RSP_SIZE
#define CONFIG_HIF_MEM_CMD_RSP_SIZE                 512
#endif

#ifndef CONFIG_HIF_MEM_FRAME_NODE_SIZE
#define CONFIG_HIF_MEM_FRAME_NODE_SIZE              16
#endif

#ifndef CONFIG_HIF_MEM_DATA_NODE_SIZE
#define CONFIG_HIF_MEM_DATA_NODE_SIZE               64
#endif

#ifndef CONFIG_HIF_MEM_SEM_NODE_SIZE
#define CONFIG_HIF_MEM_SEM_NODE_SIZE                4
#endif


/* Task config
 */
#ifndef CONFIG_HIF_TASK_STACK_SIZE
#define CONFIG_HIF_TASK_STACK_SIZE                  1024
#endif

#ifndef CONFIG_HIF_TASK_PRIO
#define CONFIG_HIF_TASK_PRIO                        14
#endif



/* Sub Configuration.
 * ============================================================================
 */

/* PHY Config
 * -------------------------------------------------
 */

/* Auto config.  [No need to modify]
 */
#ifndef CONFIG_HIF_PHY_UART
#define CONFIG_HIF_PHY_UART                         (CONFIG_HIF_PHY_TYPE & CONFIG_HIF_PHY_TYPE_UART)
#endif

#ifndef CONFIG_HIF_PHY_IIC
#define CONFIG_HIF_PHY_IIC                          (CONFIG_HIF_PHY_TYPE & CONFIG_HIF_PHY_TYPE_IIC)
#endif

#ifndef CONFIG_HIF_PHY_SPI
#define CONFIG_HIF_PHY_SPI                          (CONFIG_HIF_PHY_TYPE & CONFIG_HIF_PHY_TYPE_SPI)
#endif

#ifndef CONFIG_HIF_PHY_CAN
#define CONFIG_HIF_PHY_CAN                          (CONFIG_HIF_PHY_TYPE & CONFIG_HIF_PHY_TYPE_CAN)
#endif


/* PHY trans mode config. [modifiable]
 */
#ifndef CONFIG_HIF_PHY_UART_SEND_MODE
#define CONFIG_HIF_PHY_UART_SEND_MODE               (CONFIG_HIF_PHY_TRANS_MODE_DMA)
#endif

#ifndef CONFIG_HIF_PHY_UART_RECV_MODE
#define CONFIG_HIF_PHY_UART_RECV_MODE               (CONFIG_HIF_PHY_TRANS_MODE_INT)
#endif

#ifndef CONFIG_HIF_PHY_IIC_SEND_MODE
#define CONFIG_HIF_PHY_IIC_SEND_MODE                (CONFIG_HIF_PHY_TRANS_MODE_INT)
#endif

#ifndef CONFIG_HIF_PHY_IIC_RECV_MODE
#define CONFIG_HIF_PHY_IIC_RECV_MODE                (CONFIG_HIF_PHY_TRANS_MODE_INT)
#endif

#ifndef CONFIG_HIF_PHY_SPI_SEND_MODE
#define CONFIG_HIF_PHY_SPI_SEND_MODE                (CONFIG_HIF_PHY_TRANS_MODE_DMA)
#endif

#ifndef CONFIG_HIF_PHY_SPI_RECV_MODE
#define CONFIG_HIF_PHY_SPI_RECV_MODE                (CONFIG_HIF_PHY_TRANS_MODE_DMA)
#endif

#ifndef CONFIG_HIF_PHY_CAN_SEND_MODE
#define CONFIG_HIF_PHY_CAN_SEND_MODE                (CONFIG_HIF_PHY_TRANS_MODE_DIS)
#endif

#ifndef CONFIG_HIF_PHY_CAN_RECV_MODE
#define CONFIG_HIF_PHY_CAN_RECV_MODE                (CONFIG_HIF_PHY_TRANS_MODE_DIS)
#endif


/* Auto config, [No need to modify]
 */
#ifndef CONFIG_HIF_PHY_DMA
#if ((CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_CAN_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_CAN_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
#define CONFIG_HIF_PHY_DMA                  CONFIG_HIF_ENA
#else
#define CONFIG_HIF_PHY_DMA                  CONFIG_HIF_DIS
#endif
#endif


#ifndef CONFIG_HIF_PHY_SEND_DMA
#if ((CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_CAN_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
#define CONFIG_HIF_PHY_SEND_DMA             CONFIG_HIF_ENA
#else
#define CONFIG_HIF_PHY_SEND_DMA             CONFIG_HIF_DIS
#endif
#endif

#ifndef CONFIG_HIF_PHY_RECV_DMA
#if ((CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA) || \
        (CONFIG_HIF_PHY_CAN_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_DMA))
#define CONFIG_HIF_PHY_RECV_DMA             CONFIG_HIF_ENA
#else
#define CONFIG_HIF_PHY_RECV_DMA             CONFIG_HIF_DIS
#endif
#endif

#ifndef CONFIG_HIF_PHY_SEND_INT
#if ((CONFIG_HIF_PHY_UART_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
        (CONFIG_HIF_PHY_SPI_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
        (CONFIG_HIF_PHY_IIC_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
        (CONFIG_HIF_PHY_CAN_SEND_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT))
#define CONFIG_HIF_PHY_SEND_INT             CONFIG_HIF_ENA
#else
#define CONFIG_HIF_PHY_SEND_INT             CONFIG_HIF_DIS
#endif
#endif

#ifndef CONFIG_HIF_PHY_RECV_INT
#if ((CONFIG_HIF_PHY_UART_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
        (CONFIG_HIF_PHY_SPI_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
        (CONFIG_HIF_PHY_IIC_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT) || \
        (CONFIG_HIF_PHY_CAN_RECV_MODE & CONFIG_HIF_PHY_TRANS_MODE_INT))
#define CONFIG_HIF_PHY_RECV_INT             CONFIG_HIF_ENA
#else
#define CONFIG_HIF_PHY_RECV_INT             CONFIG_HIF_DIS
#endif
#endif


#ifndef CONFIG_HIF_SEND
#if (CONFIG_HIF_PHY_SEND_DMA || CONFIG_HIF_PHY_SEND_INT)
#define CONFIG_HIF_SEND                     CONFIG_HIF_ENA
#else
#define CONFIG_HIF_SEND                     CONFIG_HIF_DIS
#endif
#endif

#ifndef CONFIG_HIF_RECV
#if (CONFIG_HIF_PHY_RECV_DMA || CONFIG_HIF_PHY_RECV_INT)
#define CONFIG_HIF_RECV                     CONFIG_HIF_ENA
#else
#define CONFIG_HIF_RECV                     CONFIG_HIF_DIS
#endif
#endif

#ifndef CONFIG_HIF_DATA_NODE_EXT
#if CONFIG_HIF_PHY_DMA
#define CONFIG_HIF_DATA_NODE_EXT            1
#else
#define CONFIG_HIF_DATA_NODE_EXT            0
#endif
#endif

/* Defalt config.  [modifiable]
 */
#ifndef CONFIG_HIF_PHY_UART_DEF_RATE
#define CONFIG_HIF_PHY_UART_DEF_RATE                921600
#endif

#ifndef CONFIG_HIF_PHY_UART_DEF_NUM
#define CONFIG_HIF_PHY_UART_DEF_NUM                 0
#endif

#ifndef CONFIG_HIF_PHY_IIC_DEF_RATE
#define CONFIG_HIF_PHY_IIC_DEF_RATE                 400000
#endif

#ifndef CONFIG_HIF_PHY_IIC_DEF_NUM
#define CONFIG_HIF_PHY_IIC_DEF_NUM                  0
#endif

#ifndef CONFIG_HIF_PHY_IIC_DEF_ADDR
#define CONFIG_HIF_PHY_IIC_DEF_ADDR                 0x3A
#endif

#ifndef CONFIG_HIF_PHY_SPI_DEF_RATE
#define CONFIG_HIF_PHY_SPI_DEF_RATE                 56000000
#endif

#ifndef CONFIG_HIF_PHY_SPI_DEF_NUM
#define CONFIG_HIF_PHY_SPI_DEF_NUM                  0
#endif

/* 0 : std
 * 1 : dual
 * 2 : quad
 */
#ifndef CONFIG_HIF_PHY_SPI_DEF_LINE
#define CONFIG_HIF_PHY_SPI_DEF_LINE                 0
#endif

#ifndef CONFIG_HIF_PHY_CAN_DEF_RATE
#define CONFIG_HIF_PHY_CAN_DEF_RATE                 1000000
#endif

#ifndef CONFIG_HIF_PHY_CAN_DEF_NUM
#define CONFIG_HIF_PHY_CAN_DEF_NUM                  0
#endif



/* PM Config
 * -------------------------------------------------
 */
#ifndef CONFIG_HIF_PM_DEF_WAKE_MODE
#define CONFIG_HIF_PM_DEF_WAKE_MODE                 1
#endif

#ifndef CONFIG_HIF_PM_UART_DEF_WAKE_THRESH
#define CONFIG_HIF_PM_UART_DEF_WAKE_THRESH          8
#endif

#ifndef CONFIG_HIF_PM_IIC_DEF_WAKE_THRESH
#define CONFIG_HIF_PM_IIC_DEF_WAKE_THRESH           0x3A
#endif

#ifndef CONFIG_HIF_PM_SPI_DEF_WAKE_THRESH
#define CONFIG_HIF_PM_SPI_DEF_WAKE_THRESH           8
#endif

#ifndef CONFIG_HIF_PM_DEF_NOTIFY_MODE
#define CONFIG_HIF_PM_DEF_NOTIFY_MODE               1
#endif

#ifndef CONFIG_HIF_PM_DEF_NOTIFY_IO
#define CONFIG_HIF_PM_DEF_NOTIFY_IO                 6
#endif

#ifndef CONFIG_HIF_PM_DEF_NOTIFY_IO_LEVEL
#define CONFIG_HIF_PM_DEF_NOTIFY_IO_LEVEL           1
#endif



/* Timeout Config
 * -------------------------------------------------
 */
#ifndef CONFIG_HIF_DEF_SEND_TO
#define CONFIG_HIF_DEF_SEND_TO                      100
#endif

#ifndef CONFIG_HIF_DEF_RECV_TO
#define CONFIG_HIF_DEF_RECV_TO                      100
#endif

#ifndef CONFIG_HIF_DEF_SLEEP_TO
#define CONFIG_HIF_DEF_SLEEP_TO                     2000
#endif

#ifndef CONFIG_HIF_DEF_SEND_DELAY
#define CONFIG_HIF_DEF_SEND_DELAY                   0
#endif



/* Buff Config
 * -------------------------------------------------
 */
#ifndef CONFIG_HIF_CMD_RSP_SIZE_MIN
#define CONFIG_HIF_CMD_RSP_SIZE_MIN                 256
#endif

#ifndef CONFIG_HIF_CMD_PAYLOAD_MAX_SIZE
#define CONFIG_HIF_CMD_PAYLOAD_MAX_SIZE             (CONFIG_HIF_MEM_CMD_RSP_SIZE - 16)
#endif

/* work around the issue that the length cannot exceed 3.75K when bypassing the adapter chip
 */
#ifndef CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE
#define CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE             (4096 - 128)
#endif

#ifndef CONFIG_HIF_CASCADE_THRESH
#define CONFIG_HIF_CASCADE_THRESH                   512
#endif


/* Auto config, [No need to modify]
 */
#if (CONFIG_HIF_PHY_UART != 0)

#undef CONFIG_DRIVER_UART
#define CONFIG_DRIVER_UART                          1

#if ((CONFIG_HIF_PM_DEF_WAKE_MODE == 1) && (CONFIG_HIF_PM != 0))
#undef CONFIG_UART_WUP_ENABLED
#define CONFIG_UART_WUP_ENABLED                     1
#endif

#endif /* CONFIG_HIF_PHY_UART != 0 */


#if (CONFIG_HIF_PHY_IIC != 0)

#undef CONFIG_DRIVER_I2C0
#define CONFIG_DRIVER_I2C0                          1
#define CONFIG_I2C_SLAVE                            1
#define CONFIG_I2C_TRANS_IT_ENABLE                  1

#if ((CONFIG_HIF_PM_DEF_WAKE_MODE == 1) && (CONFIG_HIF_PM != 0))
#undef CONFIG_I2C_WAKEUP
#define CONFIG_I2C_WAKEUP                           1
#endif

#endif /* CONFIG_HIF_PHY_IIC != 0 */


#if (CONFIG_HIF_PHY_SPI != 0)

#undef CONFIG_DRIVER_SPI0
#define CONFIG_DRIVER_SPI0                          1

#if ((CONFIG_HIF_PM_DEF_WAKE_MODE == 1) && (CONFIG_HIF_PM != 0))
#undef CONFIG_SPI_WUP_ENABLED
#define CONFIG_SPI_WUP_ENABLED                      1
#endif

#endif /* CONFIG_HIF_PHY_SPI != 0 */


#if (CONFIG_HIF_PHY_CAN != 0)

#undef CONFIG_DRIVER_CAN
#define CONFIG_DRIVER_CAN                           1

#if ((CONFIG_HIF_PM_DEF_WAKE_MODE == 1) && (CONFIG_HIF_PM != 0))
#undef CONFIG_CAN_WUP_ENABLED
#define CONFIG_CAN_WUP_ENABLED                      1
#endif

#endif /* CONFIG_HIF_PHY_CAN != 0 */


#if (CONFIG_HIF_PHY_DMA != 0)

#undef CONFIG_DRIVER_DMA
#define CONFIG_DRIVER_DMA                           1

#endif /* CONFIG_HIF_PHY_DMA != 0 */



#ifndef CONFIG_HIF_CODE_SRAM
#define CONFIG_HIF_CODE_SRAM                        1
#endif


#if (CONFIG_SECTION_ATTRIBUTE_SRAM)

#if (CONFIG_HIF_CODE_SRAM != 0)
#define __hif_sram_text                         __sram_text
#else
#define __hif_sram_text
#endif

#define __hif_isr_text                          __sram_text

#else /* CONFIG_HIF_CODE_SRAM */

#define __hif_isr_text
#define __hif_sram_text

#endif




/**  ML config, start... **/

#ifndef CONFIG_HIF_SPLIT_TOTAL_SIZE_DEF
#define CONFIG_HIF_SPLIT_TOTAL_SIZE_DEF                         ((uint32_t)0xFFFFFFFF)
#endif

#if CONFIG_HIF_SPLIT_ENA
    #ifndef CONFIG_HIF_SEND_SPLIT_RETRY_ENA
    #define CONFIG_HIF_SEND_SPLIT_RETRY_ENA           0
    #endif
#else
    #ifdef CONFIG_HIF_SEND_SPLIT_RETRY_ENA
    #undef CONFIG_HIF_SEND_SPLIT_RETRY_ENA
    #endif

    #define CONFIG_HIF_SEND_SPLIT_RETRY_ENA           0   // Must disable split retry if disable fragment
#endif

#ifndef CONFIG_HIF_MEM_MSG_NODE_SIZE
#define CONFIG_HIF_MEM_MSG_NODE_SIZE                    (4)
#endif

#ifndef CONFIG_HIF_ML_MSG_TIMEOUT_MS_DEFAULT
#define CONFIG_HIF_ML_MSG_TIMEOUT_MS_DEFAULT            500U
#endif

#ifndef CONFIG_HIF_ML_ACK_TIMEOUT_MS
#define CONFIG_HIF_ML_ACK_TIMEOUT_MS                    500U
#endif

/**  ML config end **/


/** Timeout mechanism of TL, start... */

#if CONFIG_HIF_RETRY_ENA
    #ifndef CONFIG_HIF_TL_ACK_TIMER_ENABLE
    #define CONFIG_HIF_TL_ACK_TIMER_ENABLE              0
    #endif
#else
    #ifdef CONFIG_HIF_TL_ACK_TIMER_ENABLE
    #undef CONFIG_HIF_TL_ACK_TIMER_ENABLE
    #endif

    #define CONFIG_HIF_TL_ACK_TIMER_ENABLE              0   // Must disable ack timer if disable tl retry
#endif

#if CONFIG_HIF_TL_ACK_TIMER_ENABLE
    #ifdef CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
    #undef CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
    #endif

    #define CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE          1   // Must enable frame timeout if enable ack timeout
#else
    #ifndef CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE
    #define CONFIG_HIF_TL_FRAME_TIMEOUT_ENABLE          0
    #endif
#endif

#ifndef CONFIG_HIF_TL_ACK_TIMEOUT_MS
#define CONFIG_HIF_TL_ACK_TIMEOUT_MS                    500U
#endif

#ifndef CONFIG_HIF_TL_FRAME_TIMEOUT_MS_DEFAULT
#define CONFIG_HIF_TL_FRAME_TIMEOUT_MS_DEFAULT          500U
#endif

/** Timeout mechanism of TL, end */


#ifndef CONFIG_HIF_CMD
#define CONFIG_HIF_CMD                                  0
#endif

#ifdef __cplusplus
}
#endif

#endif //__HIF_CONFIG_H__

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
