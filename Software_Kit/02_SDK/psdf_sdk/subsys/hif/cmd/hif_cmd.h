/**
 ******************************************************************************
 * @file    hif_cmd.h
 * @brief   hif_cmd.
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

#ifndef _HIF_MSG_CONFIG_H_
#define _HIF_MSG_CONFIG_H_

/* Include Files */
#include "hif.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CONFIG_HIF_CMD

#ifndef CONFIG_HIF_MSG_FEXEC
#define CONFIG_HIF_MSG_FEXEC            1
#endif
#define HIF_MSG_FEXEC_PARAM_MAX_NUM     10

#ifndef CONFIG_HIF_MSG_READ_MEM
#define CONFIG_HIF_MSG_READ_MEM         1
#endif

#ifndef CONFIG_HIF_MSG_WRITE_MEM
#define CONFIG_HIF_MSG_WRITE_MEM        1
#endif


#define ALIGN_UP(v, n)                  (((v)+(n)-1) & ~((n)-1))    // n is power of 2
#define ALIGN_DOWN(v, n)                ((v) & ~((n)-1))            // n is power of 2

int hif_cmd_init(void);

#if CONFIG_HIF_MSG_FEXEC
int hif_msghdl_fexec_init(void);
#endif

#if CONFIG_HIF_MSG_READ_MEM
int hif_msghdl_readMem_init(void);
#endif

#if CONFIG_HIF_MSG_WRITE_MEM
int hif_msghdl_writeMem_init(void);
#endif


#endif      // CONFIG_HIF_CMD


#ifdef __cplusplus
}
#endif

#endif      // _HIF_MSG_CONFIG_H_
