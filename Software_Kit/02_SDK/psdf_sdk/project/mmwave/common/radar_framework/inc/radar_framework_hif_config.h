/**
 **************************************************************************************************
 * @brief
 * Radar framework function configurations file.
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
 **************************************************************************************************
 */


#ifndef __RADAR_FRAMEWORK_HIF_CONFIG_H
#define __RADAR_FRAMEWORK_HIF_CONFIG_H
/* Configurations that must be modified according to actual conditions,
 * which affect the size of SRAM resources including heap.
 */
#if ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0))

/* 1DFFT or 2DFFT report size
 * 1 / 2 / 4 / 8 / 16 / 32 / 64 / 128 / 256 / 512 KB
 */
#ifndef CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE
#define CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE                 (64 * 1024)
#endif

/* eg:
* CONFIG_RADAR_FRAMEWORK_DATACUBE_LATENCY_REPORT_FRAME_CNT = 2
* report latency once every two frames
 */
#ifndef CONFIG_RADAR_FRAMEWORK_DATACUBE_LATENCY_REPORT_FRAME_CNT
#define CONFIG_RADAR_FRAMEWORK_DATACUBE_LATENCY_REPORT_FRAME_CNT    (1)
#endif

#endif /* CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE or CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE */


/* HIF config
 * HIF Usage Method:
 * 1. Enable HIF: Include the subsys/hif code in the compilation and define CONFIG_HIF as 1
 * 2. Select the HIF specification and set CONFIG_HIF_MODE to 0/1 (default is 1).
 *     For details, refer to hif_config.h and the documentation;
 * 3. Select the HIF communication type UART/I2C/SPI (default supports all simultaneously).
 *     Defining CONFIG_HIF_PHY_TYPE as a single communication type can save code size
 * 4. After the above modifications, further size reduction is required.
 *     Please refer to the Hif device development documentation.
 * */

#define CONFIG_HIF                                          1
/*
 * hif communication type selection
 *
 * If the corresponding bit below is set to 1, it indicates that the communication is support
 * bit 0: uart
 * bit 1: i2c
 * bit 2: spi
 *
 * Selecting only the required communication types can save code size.
 * */
#ifndef CONFIG_HIF_PHY_TYPE
#if CONFIG_BOARD_MRS6130_P1806
#define CONFIG_HIF_PHY_TYPE                                 1  /* only support uart */
#elif CONFIG_BOARD_MRS6130_P1812 || CONFIG_BOARD_MRS6240_P2512_CPUF || CONFIG_BOARD_MRS6240_P2512_CPUS || CONFIG_BOARD_MRS6241_P2828_M62_CPUF || CONFIG_BOARD_MRS6241_P2840_M81_CPUF || CONFIG_BOARD_MRS7241_P2840_CPUF || CONFIG_BOARD_MRS7241_P2828_CPUF
#define CONFIG_HIF_PHY_TYPE                                 4  /* 0: disable, 1: UART, 2: IIC, 4: SPI */
#else
#error "please select valid board!"
#endif
#endif	/* CONFIG_HIF_PHY_TYPE */

#if (CONFIG_HIF_PHY_TYPE == 1) /* uart */
    #define CONFIG_HIF_PHY_UART_DEF_NUM                     0  /* uart 0 */
    #define CONFIG_HIF_PHY_UART_DEF_RATE                    1000000  /* uart baudrate */
#elif (CONFIG_HIF_PHY_TYPE == 2) /* i2c */
    #define CONFIG_HIF_PHY_IIC_DEF_ADDR                     0x3A
    #define CONFIG_HIF_MSG_PAYLOAD_MAX_SIZE                 (3072)
#elif (CONFIG_HIF_PHY_TYPE == 4) /* spi */
    #define CONFIG_HIF_PHY_SPI_DEF_LINE                     0 /* 0: std-SPI, 1: Dual-SPI, 2: Qard-SPI */
#endif /* CONFIG_HIF_PHY_TYPE */

/*
 * The HIF automatic data sharding function.
 * */
#define CONFIG_HIF_SPLIT_ENA                                1



/* Automatically calculate the required heap buffer size for reporting,
 * SRAM resources such as HIF nodes based on the reported cube size and reporting latency.
 * This adjustment is performed automatically, and no configuration modification is required.
 */
#if ((CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE != 0) || (CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE != 0))
	/* report pool block size
	 */
	#if (CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE <= (1 * 1024))
		#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE           (1024)
	#elif (CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE <= (8 * 1024))
		#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE           (2048)
	#else
		#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE           (3840)
	#endif

	/* report pool block count
	 */
	#if (CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE <= (64 * 1024))
		#if ((CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE % RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE) != 0)
			#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT_PER    (1 + (CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE / RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE))
		#else
			#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT_PER    (CONFIG_RADAR_FRAMEWORK_DATACUBE_REPORT_SIZE / RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE)
		#endif
	#else
		#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT_PER        (16)
	#endif

	#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT                (RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT_PER * CONFIG_RADAR_FRAMEWORK_DATACUBE_LATENCY_REPORT_FRAME_CNT)


	/* HIF frame node and data node
	 */
	#if (RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT >= 16)
	#define CONFIG_HIF_MEM_FRAME_NODE_SIZE                                (RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT + 4)
	#endif

	#if ( (CONFIG_HIF_MEM_FRAME_NODE_SIZE * 3) > 64)
	#define CONFIG_HIF_MEM_DATA_NODE_SIZE                                 (CONFIG_HIF_MEM_FRAME_NODE_SIZE * 3)
	#endif


	/* heap size
	 */
	#if ((CONFIG_HIF_PHY_TYPE == 4) && (CONFIG_HIF_PHY_SPI_DEF_LINE == 2))  /* spi-qard */
		#define HEAP_DATACUBE_REPORT_POOL                                 0
		#define CONFIG_HIF_APP_SPEC_POOL                                  1
		#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE           (3084)
		#define RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT            (16)
	#else
		#define HEAP_DATACUBE_REPORT_POOL                                 ((RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_SIZE + 24) * RADAR_FRAMEWORK_DATACUBE_REPORT_POOL_BLOCK_CNT)
		#define CONFIG_HIF_APP_SPEC_POOL                                  0
	#endif

	/* HIF other config
	 */
	#define CONFIG_HIF_APP_DATA_POOL                                    1
	/* fixed config, no modify*/
	#if (CONFIG_HIF_APP_SPEC_POOL != 0)
	#define CONFIG_HIF_MEM_FRAME_NODE_SIZE                              40
	#define CONFIG_HIF_MEM_DATA_NODE_SIZE                               120
	#define CONFIG_DRIVER_SRAM_ADDR                                     0x00420400
	#define CONFIG_DRIVER_SRAM_SIZE                                     0x00029800
	#define CONFIG_DRIVER_KVR_ADDR                                      0x00420000
	#define CONFIG_CUBE_BUFFER_A                                        (0x10400000)
	#define CONFIG_CUBE_BUFFER_B                                        (0x10410000)
	#endif

#else

	#define HEAP_DATACUBE_REPORT_POOL                                0

#endif /* CONFIG_RADAR_FRAMEWORK_REPORT_1D_DATACUBE or CONFIG_RADAR_FRAMEWORK_REPORT_2D_DATACUBE */

#if CONFIG_HIF
	#define HEAP_HIF                                                      (3584 + (CONFIG_HIF_MEM_FRAME_NODE_SIZE * 48) + CONFIG_HIF_MEM_DATA_NODE_SIZE * 20)
#else
	#define HEAP_HIF 												(0)
#endif

#endif /* __RADAR_FRAMEWORK_HIF_CONFIG_H */

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
