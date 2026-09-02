/**
 ******************************************************************************
 * @file    mmw_perf_test.c
 * @brief   mmw_perf_test define.
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


#define SDK_NEW                     1

/* Includes.
 * ----------------------------------------------------------------------------
 */
#if (SDK_NEW == 1)
#include "common.h"
#include "board_config.h"

#include "hal_board.h"
#include "log.h"

#else

#include <zephyr/kernel.h>
#include "rv32_gcc.h"
#include "psic_ll_utils.h"
#include "psic_ll_rcc_bus.h"
#include "psic_ll_rcc_dev.h"
#include "psic_ll_pmu.h"
#include "psic_ll_timer.h"
#include "psic_ll_gpio.h"

#endif
/* Private typedef.
 * ----------------------------------------------------------------------------
 */
typedef struct FilterPara {
    union {
        struct {
            int16_t ptr_den[4];
            uint32_t gain_lin[2];
        } bpf_4order;
        struct {
            int16_t hpf_alpha;
            uint32_t gain_lin;
        } hpf_1order;
    } filter_trans_para;
    uint16_t alpha_inv_log2;    /* Q?待定 */
}FilterPara_t;

typedef struct {
    int16_t imag; /*!< @brief imaginary part */
    int16_t real; /*!< @brief real part */
} strComplex16;

typedef struct {
    int32_t imag; /*!< @brief imaginary part */
    int32_t real; /*!< @brief real part */
}strComplexNum_32;

// typedef strComplexNum_32 FilterBuffer_t;

typedef int32_t FilterMemType_t;
typedef uint32_t PowerLin_t;

typedef struct FilterBuffer {
    FilterMemType_t* buf_real;                    /* [rx_num][filter_order] */
    FilterMemType_t* buf_imag;                    /* [rx_num][filter_order] */
    PowerLin_t* buf_power;                        /* [g_mixed_path_num] */
    strComplexNum_32 *filtered_bv;                /* [g_mixed_path_num] */
}FilterBuffer_t;

typedef struct{
    FilterBuffer_t filter_buffer;
    uint16_t snr_threshold;
    uint16_t snr_threshold_used;
    uint8_t inteference_timeout_cnt;    /* 干扰检测超时计数值，计数值为0表示不是个干扰 */
    uint32_t valid_cnt;            /* 每个bin的滤波器输出是否稳定, UINT32_MAX表示稳定，其余表示不稳定 */
} ProcessHandler_t;

/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
#if (SDK_NEW == 1)
#define TEST_LOG_IO(num, val, flip)             LOG_IO(0x00100000, num, val, flip)

#else
#define TEST_LOG_IO(num, val, flip)             HW_TRACE_IO(0x00100000, num, val, flip)

#endif

/* Private variables.
 * ----------------------------------------------------------------------------
 */
FilterPara_t g_presence_filters_fast_converage;
ProcessHandler_t *g_ptr_proc_handler = NULL;
 /* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
#define  PRESENCE_FILTER_MEM_LEN    (4)
#define RX_NUM          3

#define TEST_FRAME_MAX              100
#define TEST_RBIN_MAX               85

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


#if (SDK_NEW == 1)

#define k_malloc            malloc

#else

#define HAL_TIME_US                 1
#define LOG_PRINT                   printk

#endif

uint32_t test_GetTime(void)
{
    uint32_t timeValue = csi_coret_get_value();
    return (timeValue / 32) & 0xFFFFFFFF;
}


int32_t iir_filter_2order(const int16_t* ptr_den, uint32_t gain, int32_t* ptr_mem, int16_t x)
{
    /*
        ptr_num(0) = 1, ptr_num(1) = -2 ptr_num(2) = 1
        Difference equations:
        y(m) = ptr_num(0)x(m) + w0(m-1)
        w0(m) = ptr_num(1)x(m) + w1(m-1) - ptr_den(1)y(m)
        w1(m) = ptr_num(2)x(m) - ptr_den(2)y(m)
    */
    /* x: S16Q15, dout S32Q28, mem: S32Q25, g: U32Q31, factor: S16Q12*/
    int32_t dout = (x << 10) + ptr_mem[0];            /* S32Q25 */
    ptr_mem[0] = (((int64_t)(-x) << 23) + ((int64_t)ptr_mem[1] << 12) - (int64_t)ptr_den[0] * dout) >> 12;  /* S32Q25 */
    ptr_mem[1] = (((int64_t)x << 22) - (int64_t)ptr_den[1] * dout) >> 12;  /* S32Q25 */
    dout = ((int64_t)dout * gain) >> 28;   /* S32Q28 */
    return dout;
}


void presence_filter_hpf(const FilterPara_t* ptr_filter_para,
            const strComplex16* raw_bv,
            uint32_t rx_num,
            FilterBuffer_t* ptr_mem,
            strComplexNum_32* filtered_bv)
{
    const int16_t* ptr_den = ptr_filter_para->filter_trans_para.bpf_4order.ptr_den;
    const uint32_t* ptr_gain_lin = ptr_filter_para->filter_trans_para.bpf_4order.gain_lin;
    FilterMemType_t* ptr_buf_real, *ptr_buf_imag;

    ptr_buf_real = ptr_mem->buf_real;
    ptr_buf_imag = ptr_mem->buf_imag;
    for (uint32_t rx_idx = 0; rx_idx < rx_num; rx_idx++) {
        filtered_bv[rx_idx].real = iir_filter_2order(ptr_den, ptr_gain_lin[0], ptr_buf_real, raw_bv[rx_idx].real);
        filtered_bv[rx_idx].imag = iir_filter_2order(ptr_den, ptr_gain_lin[0], ptr_buf_imag, raw_bv[rx_idx].imag);
        ptr_buf_real += PRESENCE_FILTER_MEM_LEN;
        ptr_buf_imag += PRESENCE_FILTER_MEM_LEN;
    }
}

void presnece_filter_test(uint32_t g_frame_idx, uint32_t rbin_idx) {
    FilterPara_t* ptr_filter_para = &g_presence_filters_fast_converage;
    strComplex16 ptr_raw_bv[RX_NUM];
    ProcessHandler_t * ptr_proc_handler = &g_ptr_proc_handler[rbin_idx];

    for (uint32_t rx_idx = 0; rx_idx < RX_NUM; rx_idx++) {
        ptr_raw_bv[rx_idx].imag = 100 + 500 * rbin_idx - 100 * rx_idx + 10 * g_frame_idx;
        ptr_raw_bv[rx_idx].real = 100 + 500 * rbin_idx + 100 * rx_idx + 10 * g_frame_idx;
    }
    presence_filter_hpf(ptr_filter_para,
        ptr_raw_bv,
        RX_NUM,
        &ptr_proc_handler->filter_buffer,
        ptr_proc_handler->filter_buffer.filtered_bv);
}



int init_test_buff(uint32_t max_det_idx)
{
    g_ptr_proc_handler = malloc(sizeof(ProcessHandler_t) * max_det_idx);
    if (g_ptr_proc_handler == NULL) {
        return -1;
    }

    for (uint32_t rbin_idx = 0; rbin_idx < max_det_idx; rbin_idx++) {
        g_ptr_proc_handler[rbin_idx].filter_buffer.buf_real = malloc(sizeof(FilterMemType_t) * RX_NUM * PRESENCE_FILTER_MEM_LEN);
        g_ptr_proc_handler[rbin_idx].filter_buffer.buf_imag = malloc(sizeof(FilterMemType_t) * RX_NUM * PRESENCE_FILTER_MEM_LEN);
        g_ptr_proc_handler[rbin_idx].filter_buffer.buf_power = malloc(sizeof(PowerLin_t) * RX_NUM * PRESENCE_FILTER_MEM_LEN);
        g_ptr_proc_handler[rbin_idx].filter_buffer.filtered_bv = malloc(sizeof(strComplexNum_32) * RX_NUM);
        if ((g_ptr_proc_handler[rbin_idx].filter_buffer.buf_real == NULL) ||
            (g_ptr_proc_handler[rbin_idx].filter_buffer.buf_imag == NULL) ||
            (g_ptr_proc_handler[rbin_idx].filter_buffer.buf_power == NULL) ||
            (g_ptr_proc_handler[rbin_idx].filter_buffer.filtered_bv == NULL)) {
            return -1;
        }
    }

    g_presence_filters_fast_converage.filter_trans_para.bpf_4order.ptr_den[0] = 4690;
    g_presence_filters_fast_converage.filter_trans_para.bpf_4order.ptr_den[1] = 2060;
    g_presence_filters_fast_converage.filter_trans_para.bpf_4order.ptr_den[2] = -1944;
    g_presence_filters_fast_converage.filter_trans_para.bpf_4order.ptr_den[3] = 1446;
    g_presence_filters_fast_converage.filter_trans_para.bpf_4order.gain_lin[0] = 1070173388;
    g_presence_filters_fast_converage.filter_trans_para.bpf_4order.gain_lin[1] = 1070173388;
    g_presence_filters_fast_converage.alpha_inv_log2 = 5;

    return 0;
}


void mmw_perf_test(void)
{
    uint32_t startTime = 0;
    uint32_t endTime = 0;
    uint32_t totalTime = 0;

    int status = init_test_buff(TEST_RBIN_MAX);
    if (status != 0) {
        LOG_PRINT("init fail\n");
        return;
    }


    for (uint32_t frame_idx = 0; frame_idx < TEST_FRAME_MAX; frame_idx++) {
        for (uint32_t rbin_idx = 0; rbin_idx < TEST_RBIN_MAX; rbin_idx++) {
            startTime = test_GetTime();
            TEST_LOG_IO(1, 1, 0);
            presnece_filter_test(frame_idx, rbin_idx);
            TEST_LOG_IO(1, 0, 0);
            endTime = test_GetTime();
            totalTime += (endTime - startTime);
            LOG_PRINT("[%d-%d] <%d-%d>\n", frame_idx, rbin_idx, totalTime, (endTime - startTime));
        }
    }
}


/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
