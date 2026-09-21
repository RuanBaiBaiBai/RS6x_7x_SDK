/**
 ******************************************************************************
 * @file    cmd_devmem.c
 * @brief   cmd devmem define.
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
#include "hal_types.h"
#include "hal_dev.h"

#include "cmd_config.h"


#if ((CONFIG_SHELL_CMD_DEVMEM == 1) && (CONFIG_SHELL == 1))

/* Private typedef.
 * ----------------------------------------------------------------------------
 */
/* Private defines.
 * ----------------------------------------------------------------------------
 */
/* Private macros.
 * ----------------------------------------------------------------------------
 */
/* Private variables.
 * ----------------------------------------------------------------------------
 */
/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */
/* Exported functions.
 * ----------------------------------------------------------------------------
 */

static int cmd_devmem_read(Shell *shell, int argc, char *argv[])
{
	uint32_t addr = 0;
    uint32_t len = 1;
	uint32_t value;

	if (argc == 2 || argc == 3) {
		addr = (strtoul(argv[1], NULL, 0) & 0xfffffffc);
        if (argc == 3) {
            len = strtoul(argv[2], NULL, 0);
            if (len < 1) {
                return -EINVAL;
            }
        }
	} else {
		return -EINVAL;
	}

    shellWriteString(shell, "Read value");
    for (uint32_t idx = 0; idx < len; idx++) {
        if (idx > 0 && idx % 4 == 0) {
            shellWriteString(shell, "\n");
        }
        value = *(((volatile uint32_t *)addr) + idx);
        shellPrint(shell, " 0x%08x", value);
    }
    shellWriteString(shell, "\n");

	return 0;
}

static int cmd_devmem_write(Shell *shell, int argc, char *argv[])
{
	uint32_t value = 0;
	uint32_t addr = 0;
	int err = 0;

	if (argc == 3) {
		addr = (strtoul(argv[1], NULL, 0) & 0xfffffffc);
		value = strtoul(argv[2], NULL, 0);
	} else {
		return -EINVAL;
	}

	*(volatile uint32_t *)(addr) = value;

	return err;
}

ShellCommand devmemGroup[] = {
    SHELL_CMD_ITEM(read, 	cmd_devmem_read,  "devmem read <address> [len]"),
    SHELL_CMD_ITEM(write, 	cmd_devmem_write, "devmem write <address> <value>"),
    SHELL_CMD_ITEM_END()
};

SHELL_CMD_GROUP(devmem, devmemGroup, "Read/write physical memory");

#endif /* CONFIG_SHELL_CMD_DEVMEM */




/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */
