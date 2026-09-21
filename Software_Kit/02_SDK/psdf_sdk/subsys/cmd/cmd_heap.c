/**
 ******************************************************************************
 * @file    cmd_heap.c
 * @brief   cmd heap define.
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


#if ((CONFIG_SHELL_CMD_HEAP == 1) && (CONFIG_SHELL == 1))

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

int heap_stats_cmd(Shell *shell, int argc, char *argv[])
{
    HeapStats_t heapStats;

    vPortGetHeapStats(&heapStats);

    shellPrint(shell, "start addr         : %p\n", heapStats.xStartAddr);
    uint8_t *pHeap = heapStats.xStartAddr;
    pHeap += heapStats.xTotalSize;
    shellPrint(shell, "end addr           : %p\n", pHeap);
    shellPrint(shell, "total size         : %d KB (%d B)\n", heapStats.xTotalSize / 1024, heapStats.xTotalSize);
    shellPrint(shell, "current free size  : %d KB (%d B)\n",
                    heapStats.xAvailableHeapSpaceInBytes / 1024, heapStats.xAvailableHeapSpaceInBytes);
    shellPrint(shell, "largest free size  : %d KB (%d B)\n",
                    heapStats.xSizeOfLargestFreeBlockInBytes / 1024, heapStats.xSizeOfLargestFreeBlockInBytes);
    shellPrint(shell, "smallest free size  : %d KB (%d B)\n",
                    heapStats.xSizeOfSmallestFreeBlockInBytes / 1024, heapStats.xSizeOfSmallestFreeBlockInBytes);
    shellPrint(shell, "number free blocks : %d \n", heapStats.xNumberOfFreeBlocks);
    shellPrint(shell, "min free size in history : %d KB (%d B)\n",
                    heapStats.xMinimumEverFreeBytesRemaining / 1024, heapStats.xMinimumEverFreeBytesRemaining);
    shellPrint(shell, "number call alloc  : %d \n", heapStats.xNumberOfSuccessfulAllocations);
    shellPrint(shell, "number call free   : %d \n", heapStats.xNumberOfSuccessfulFrees);

    return HAL_STATUS_OK;
}


int heap_map_cmd(Shell *shell, int argc, char *argv[])
{
    HeapBlock_t heapBlock = {0, 0};

    shellPrint(shell, "F: free\n");

    int status = 0;
    uint32_t idx = 0;
    do {
        status = vPortGetHeapMap(&heapBlock);
        if (status != 0) {
            break;
        }

        char * pStr = (heapBlock.size & 0x80000000) ? " " : "F";
        uint32_t addr = heapBlock.addr + sizeof(HeapBlock_t);
        uint32_t len = heapBlock.size & 0x7FFFFFFF;
        if (len > 0) {
            len -= sizeof(HeapBlock_t);
        }
        shellPrint(shell, "%3d\t%s\t%08X\t%5d\t%3d KB\n", idx++, pStr, addr, len, len / 1024);

    } while (1);

    return 0;
}


ShellCommand heap_cmds[] = {
    SHELL_CMD_ITEM(stat,  heap_stats_cmd,  "Get heap stats info"),
    SHELL_CMD_ITEM(map,   heap_map_cmd,    "Get heap map"),
    SHELL_CMD_ITEM_END()
};

SHELL_CMD_GROUP(heap, heap_cmds, "heap command");

#endif

/*
 ******************************************************************************
 * (C) COPYRIGHT POSSUMIC TECHNOLOGY
 * END OF FILE
 */

