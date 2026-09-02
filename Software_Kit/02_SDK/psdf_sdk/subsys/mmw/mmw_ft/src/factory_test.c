/**
 **************************************************************************************************
 * @brief   factory test function define.
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
 **************************************************************************************************
 */

#include "factory_test.h"
#include "hif.h"
#include "mmw_type.h"
#include "ft_ctrl.h"
#include "mmw_app_pointcloud.h"
#include "mmw_alg_pointcloud.h"
#include "mmw_point_cloud_psic_lib.h"
#include "hif_pm.h"
#include "hal_wdg.h"
#include "ll_utils.h"
#include "log.h"
#include "mmw.h"

/* Private typedef.
 * ----------------------------------------------------------------------------
 */



typedef struct {
    uint8_t     *pData;
    uint16_t    dataLen;
    uint8_t     msgId;
} FT_Msg_t;

typedef enum {
    FT_STATE_CLOSE = 0,
    FT_STATE_OPEN,
    FT_STATE_BUSY,
} FT_State_t;


typedef enum {
    FT_MODE_RESPOND = 0,
    FT_MODE_REPORT
} FT_Mode_t;


/* Private defines.
 * ----------------------------------------------------------------------------
 */
#define CONFIG_FT_MSG_QUEUE_SIZE                        2

/* Private macros.
 * ----------------------------------------------------------------------------
 */

#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
    #define FT_CMD_MAX_LENGTH    123
#elif CONFIG_SOC_SERIES_RS613X
    #define FT_CMD_MAX_LENGTH    73
#else
	#error "not support chip!"
#endif


/* Private variables.
 * ----------------------------------------------------------------------------
 */

struct mmw_noise_est_frame_t g_noise_est_frame = {
#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
    .trx_mimo = MMW_MIMO_2T4R,
#elif CONFIG_SOC_SERIES_RS613X
    .trx_mimo = MMW_MIMO_1T3R,
#else
    #error "not support board"
#endif
    .work_mode = MMW_WORK_MODE_2DFFT,
    .clutter_mode = MMW_CLUTTER_MODE_DISABLE,
    .startFreq_MHz = FT_DEFAULT_START_FREQ,
    .endFreq_MHz = FT_DEFAULT_END_FREQ,
    .range_mm = FT_DEFAULT_RANGE,
    .resol_mm = FT_DEFAULT_RANGE_RES,
    .velocity_mm = NOISE_FT_DEFAULT_VELOC,
    .veloc_resol = NOISE_FT_DEFAULT_VEL_RES,
    .frame_period_ms = NOISE_FT_DEFAULT_FRAME_PERIOD,
    .frame_num = NOISE_FT_DEFAULT_FRAME_NUM + CONFIG_MMW_VENDOR_FILTER_FRAME_NUM,
    .chirp_num = NOISE_FT_DEFAULT_CHIRP_NUM,
};

struct mmw_angle_est_frame_t angle_frame = {
    .start_freq = FT_DEFAULT_START_FREQ,
    .mount_type = MMW_MOUNT_HORIZONTAL,
    .range_mm = FT_DEFAULT_RANGE,
    .range_res_mm = FT_DEFAULT_RANGE_RES,
};

uint8_t g_ft_reset_enable = 0;    // 0: do not reset system when enter/quit ft mode; 1: reset
uint8_t g_ft_report_type = 0;    // 0: radar respond result when function end; 1: radar respond status firstly, when function end, report result.
uint8_t g_ft_max_diff_value_detect_finish_flag = 0;    // 0: max diff value detect not finish; 1: finishend.

uint8_t g_est_rx_msk = 0;
static int8_t g_ft_mmw_calib_status = HIF_ERRCODE_NOT_READY;

/* this buffer use for storage ft-cmd from host, when hif respond to host, the first byte of payload will be changed to hif status.
 * therefore, it is necessary to copy ft-cmd before hif respond */
uint8_t *g_ft_cmd_param_buffer = 0;

HAL_Dev_t *ft_trigger_wkio_Dev = NULL;


static FT_State_t ftState;
static FT_Mode_t ftMode;

static OSI_Thread_t ftTaskHandle;
static OSI_Queue_t ftMsgQueue;
static OSI_Semaphore_t ftRspSem;


/* Private function prototypes.
 * ----------------------------------------------------------------------------
 */

static void FT_Task(void *arg);
static int (*ft_mode_cb)(uint8_t event);
extern uint8_t g_max_diff_detect_pass_flag;
extern void HIF_PM_SuspendDevice(void);
void FactoryTest_Deinit(void);
void ft_wait_for_mmw_callback_returns(void);

/* Exported functions.
 * ----------------------------------------------------------------------------
 */


uint8_t ft_cmd_param_len_check(uint16_t cmd_len, uint16_t true_len)
{
    uint8_t ret = 0;
    if (cmd_len == true_len) {
        ret = 1;
    }
    return ret;
}


int ft_power_mode_2hz_frame_config(void)
{
    int ret;

#if CONFIG_SOC_SERIES_RS613X
    ret = mmw_mode_cfg(MMW_MIMO_1T3R, MMW_WORK_MODE_1DFFT);
#elif CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
    ret = mmw_mode_cfg(MMW_MIMO_2T4R, MMW_WORK_MODE_1DFFT);
#else
    #error "not support board"
#endif
    if (ret) {
        LOG_PRINT("mode cfg error! %d\n", ret);
    }
    ret = mmw_range_cfg(10240, 80); /* max10.24m, resol=8cm */
    if (ret) {
        LOG_PRINT("range cfg error! %d\n", ret);
    }
    ret = mmw_velocity_cfg(1600, 200); /* max=1.6m/s, resol=0.02m/s, 16fft */
    if (ret) {
        LOG_PRINT("velocity cfg error! %d\n", ret);
    }
    ret = mmw_frame_cfg(500, 0);    /* frame period 500ms */
    if (ret) {
        LOG_PRINT("frame cfg error! %d\n", ret);
    }
    ret = mmw_freq_cfg (FT_DEFAULT_START_FREQ, FT_DEFAULT_END_FREQ);
    if (ret) {
        LOG_PRINT("mmw_freq_cfg error\n");
    }
    return ret;
}

static int ft_2hz_mode_frame_cb(void *mmw_data, void *arg)
{
    return 0;
}


static int ft_trigger_frame_cb(void *mmw_data, void *arg)
{

    mmw_dsp_poweron();
    PointCloud_rept pc_report = {
        .range = 0,
        .azim_deg = 0,
        .elev_deg = 0
    };
    PointCloudBuffer_t *ptr_point_cloud_buffer;

    uint32_t trigger_range_cm = FT_TRIGGER_RANGE_MM / 10;
    uint16_t valid_pc_num = 0;
    uint8_t ft_led_flag = 0;
    /* get point cloud result via function mmw_point_cloud_process() */
    ptr_point_cloud_buffer = mmw_point_cloud_process();
    /* check whether exist point cloud within FT_TRIGGER_RANGE_MM */
    valid_pc_num = ft_judge_trigger_detection(&pc_report, ptr_point_cloud_buffer, trigger_range_cm, ptr_point_cloud_buffer->point_cloud_num, FT_MIN_FORCE_TRIGGER_MOVING_PC_NUM);
    FT_LOG_PRINT("%x, %x, %x\n",pc_report.range, pc_report.azim_deg, pc_report.elev_deg);

    mmw_fft_autogain_clear();
    mmw_process_mem_free((void**) &ptr_point_cloud_buffer->ptr_motion_point_cloud_data);
    mmw_process_mem_free((void**) &ptr_point_cloud_buffer);

    if (valid_pc_num != 0) {
        ft_led_flag = 1;
        ft_trigger_point_cloud_report(&pc_report, sizeof(pc_report) * 1);
    }

    ft_led_set(ft_trigger_wkio_Dev, ft_led_flag);

#if (CONFIG_MMW_LOOP_TASK_MONITOR)
        mmw_loop_task_time_reset(NULL);
#endif

    return 0;
}

int factory_test_trigger_process(void){

    int status = 0;

    ft_trigger_wkio_Dev = ft_trigger_init(&status);
    if (status != 0) {
        LOG_PRINT("ft trigger init fail %d\n", status);
    }

    status = mmw_ctrl_callback_cfg(&ft_trigger_frame_cb, MMW_DATA_TYPE_2DFFT, NULL);
    if (status != 0) {
        LOG_PRINT("mmw_ctrl_callback_cfg fail %d\n", status);
    }

    status = mmw_ctrl_start();
    if (status != 0) {
        LOG_PRINT("mmw ctrl start fail %d\n", status);
    }
    return status;
}


static int FT_MsgQueueInit(void)
{
    return OSI_QueueCreate(&ftMsgQueue, CONFIG_FT_MSG_QUEUE_SIZE, sizeof(FT_Msg_t));
}

static void FT_MsgQueueDeinit(void)
{
    OSI_QueueDelete(&ftMsgQueue);
}

static int FT_TaskInit(void)
{
    OSI_ThreadSetInvalid(&ftTaskHandle);
    OSI_Status_t osiStatus = OSI_ThreadCreate(&ftTaskHandle,
                                        "Factory Test",
                                        FT_Task,
                                        NULL,
                                        3,
                                        1024);
    if (osiStatus != OSI_STATUS_OK) {
        return -1;
    }

    return 0;
}

static void FT_TaskDeinit(void)
{
    OSI_ThreadDelete(&ftTaskHandle);
}


static int FT_RspSemInit(void)
{
    return OSI_SemaphoreCreate(&ftRspSem, 0, 1);
}

static void FT_RspSemDeinit(void)
{
    OSI_SemaphoreDelete(&ftRspSem);
}

static int FT_RspSemWait(uint32_t timeout)
{
    return OSI_SemaphoreWait(&ftRspSem, timeout);
}

void FT_RspSemRelease(void)
{
    OSI_SemaphoreRelease(&ftRspSem);
}


static void FT_PopMsg(FT_Msg_t * pMsg)
{
    OSI_QueueReceive(&ftMsgQueue, pMsg, OSI_WAIT_FOREVER);
}

static int FT_PushMsg(uint8_t msgId, uint16_t dataLen, uint8_t *pData)
{
    uint8_t *pBuff = OSI_Malloc(dataLen);
    if (pBuff == NULL) {
        return -1;
    }

    OSI_Memcpy(pBuff, pData, dataLen);

    FT_Msg_t ftMsg = {
        .pData      = pBuff,
        .dataLen    = dataLen,
        .msgId      = msgId,
    };

    OSI_QueueSend(&ftMsgQueue, &ftMsg, 0);

    return 0;
}


static void FT_SetState(FT_State_t state)
{
    unsigned long key = __lock_irq();

    ftState = state;

    __unlock_irq((unsigned long)key);
}

static FT_State_t FT_GetState(void)
{
    FT_State_t retState;

    unsigned long key = __lock_irq();

    retState = ftState;

    __unlock_irq((unsigned long)key);

    return retState;
}

static void FT_SetMode(FT_Mode_t mode)
{
    unsigned long key = __lock_irq();

    ftMode = mode;

    __unlock_irq((unsigned long)key);
}

static FT_Mode_t FT_GetMode(void)
{
    FT_Mode_t retMode;

    unsigned long key = __lock_irq();

    retMode = ftMode;

    __unlock_irq((unsigned long)key);

    return retMode;
}


static FT_Mode_t FT_GetNvmMode(void)
{
    FT_Mode_t retMode;

    unsigned long key = __lock_irq();

    uint32_t value = LL_NVM_GetValue(2);
    if (value & 0x2) {
        retMode = FT_MODE_REPORT;
    } else {
        retMode = FT_MODE_RESPOND;
    }

    __unlock_irq((unsigned long)key);

    return retMode;
}

static void FT_SetNvmMode(FT_Mode_t mode)
{
    unsigned long key = __lock_irq();

    uint32_t modeValue;
    if (mode == FT_MODE_RESPOND) {
        modeValue = 0;
    } else {
        modeValue = 2;
    }

    uint32_t value = LL_NVM_GetValue(2);
    value &= 0xFFFFFFFD;
    value |= modeValue;
    LL_NVM_SetValue(2, value);

    __unlock_irq((unsigned long)key);
}

#define FT_NVM_EVENT_ENTER              0xAA
#define FT_NVM_EVENT_EXIT               0x55
#define FT_NVM_EVENT_NONE               0x00
static FT_Event_t FT_GetNvmEvent(void)
{
    FT_Event_t retEvent;

    unsigned long key = __lock_irq();

    uint32_t value = LL_NVM_GetValue(2);
    uint8_t eventMagic = (value >> 8) & 0xFF;
    if (eventMagic == FT_NVM_EVENT_ENTER) {
        retEvent = FT_EVENT_ENTER;
    } else if (eventMagic == FT_NVM_EVENT_EXIT) {
        retEvent = FT_EVENT_EXIT;
    } else {
        retEvent = FT_EVENT_NONE;
    }

    __unlock_irq((unsigned long)key);

    return retEvent;
}

static void FT_SetNvmEvent(FT_Event_t event)
{
    unsigned long key = __lock_irq();

    uint32_t eventMagic = 0;
    if (event == FT_EVENT_ENTER) {
        eventMagic = FT_NVM_EVENT_ENTER << 8;
    } else if (event == FT_EVENT_EXIT) {
        eventMagic = FT_NVM_EVENT_EXIT << 8;
    } else {
        eventMagic = 0;
    }

    uint32_t value = LL_NVM_GetValue(2);
    value &= 0xFFFFFF0F;
    value |= eventMagic;
    LL_NVM_SetValue(2, value);

    __unlock_irq((unsigned long)key);
}


static void FT_ClearNvmEvent(void)
{
    unsigned long key = __lock_irq();

    LL_NVM_SetValue(2, 0);

    __unlock_irq((unsigned long)key);
}


static void FT_ResetDevice(uint32_t timeout)
{
    WDG_InitParam_t initParam = {
        .mode = WDG_MODE_RESET_SYS,
        .callback = NULL,
        .timeout = timeout,
    };
    FT_LOG_PRINT("watchdog init\n");
    HAL_Dev_t *wdg = HAL_WDG_Init(0);
    FT_LOG_PRINT("watchdog enable\n");
    HAL_WDG_Open(wdg, &initParam);
}

static int FT_StartHandler(HIF_MsgHdr_t *msg)
{
    int ret = HIF_CMD_STATUS_SUCCESS;

    /* get event
     * bit 0: 1: enter ft event, 0: exit ft event
     * bit 1: 1: reset device, 0: not  reset device
     * bit 2: 1: rreport mode, 0: response mode
     */
    uint8_t event = *((uint8_t *)(msg + 1));

    FT_Event_t ftEvent = FT_EVENT_NONE;
    if (event & 0x01) {
        ftEvent = FT_EVENT_ENTER;
    } else {
        ftEvent = FT_EVENT_EXIT;
    }

    uint8_t resetMode = event & 0x02;
    FT_Mode_t ftMode = FT_MODE_RESPOND;
    if (event & 0x04) {
        ftMode = FT_MODE_REPORT;
    }

    do {
        FT_State_t currentState = FT_GetState();

        if (ftEvent == FT_EVENT_ENTER) {
            if (currentState == FT_STATE_OPEN) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            } else if (currentState == FT_STATE_BUSY) {
                ret = HIF_CMD_STATUS_BUSY;
                break;
            }

			/* excute usr callback firstly, ensure mmw wave has stoped */
			int status = 0;
            if (ft_mode_cb) {
                status = ft_mode_cb(FT_EVENT_ENTER);
				if (status != 0) {
					ret = HIF_CMD_STATUS_SYSERR;
					break;
				}
            }

            HIF_PM_WakeupDevice();

            if (resetMode) {
                FT_SetMode(ftMode);
                FT_SetState(FT_STATE_BUSY);
                FT_SetNvmMode(ftMode);
                FT_SetNvmEvent(FT_EVENT_ENTER);

                FT_ResetDevice(100); /* 100ms */
                break;
            }



            if (ftMode == FT_MODE_REPORT) {
                status = FT_TaskInit();
                if (status != 0) {
                    ret = HIF_CMD_STATUS_SYSERR;
                    break;
                }

                status = FT_MsgQueueInit();
                if (status != 0) {
                    ret = HIF_CMD_STATUS_SYSERR;
                    break;
                }
            }

            status = FT_RspSemInit();
            if (status != 0) {
                ret = HIF_CMD_STATUS_SYSERR;
                break;
            }

            FT_SetMode(ftMode);
            FT_SetState(FT_STATE_OPEN);

        } else if (ftEvent == FT_EVENT_EXIT) {
            if (currentState == FT_STATE_CLOSE) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            } else if (currentState == FT_STATE_BUSY) {
                ret = HIF_CMD_STATUS_BUSY;
                break;
            }

            if(ftMode != FT_GetMode()) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            }

            if (resetMode) {
                FT_SetMode(ftMode);
                FT_SetState(FT_STATE_BUSY);
                FT_SetNvmMode(ftMode);
                FT_SetNvmEvent(FT_EVENT_EXIT);

                HIF_PM_WakeupDevice();

                FT_ResetDevice(100); /* 100ms */
                break;
            }

            HIF_PM_SuspendDevice();

            if (ftMode == FT_MODE_REPORT) {
                FT_TaskDeinit();
                FT_MsgQueueDeinit();
            }

            FT_RspSemDeinit();

			FactoryTest_Deinit();

            FT_SetMode(ftMode);
            FT_SetState(FT_STATE_CLOSE);

			/* excute usr callback after ft deinit finished */
			int status = 0;
			if (ft_mode_cb) {
                status = ft_mode_cb(FT_EVENT_EXIT);
				if (status != 0) {
					ret = HIF_CMD_STATUS_SYSERR;
					break;
				}
            }

        }
    } while (0);


    FT_LOG_PRINT("FT Start (%d)\n", ret);

    return HIF_MsgResp(msg, 0, ret);
}


static uint8_t * FT_MmwPerf(uint8_t *pData)
{
    int status = 0;

    status = factory_test_mmw_perf_start(pData);
    if (status != 0) {
        mmw_ctrl_stop();
        FT_LOG_PRINT("perf start fail\n");
        return NULL;
    }
    status = FT_RspSemWait(20000);  /* 20S */
    if (status != 0) {
        FT_LOG_PRINT("perf timeout\n");
        return NULL;
    }

	ft_wait_for_mmw_callback_returns();

    struct mmw_noise_est_final_report_t** noise_est_report_result = get_noise_est_report_result();

    return (uint8_t *)(*noise_est_report_result);
}


void FT_MmwPerfTask(uint8_t *pData)
{
    FT_SetState(FT_STATE_BUSY);

    uint8_t *pPerfRsp = FT_MmwPerf(pData);
    if (pPerfRsp == NULL) {
        uint8_t retStatus = HIF_CMD_STATUS_SYSERR;

        HIF_MsgReport(HIF_MSG_ID_MMW_PERF_TEST, &retStatus, 1, NULL);
    } else {
        uint16_t reportLen = sizeof(struct mmw_noise_est_final_report_t);
        uint8_t reportData[reportLen + 1];
        memcpy(&reportData[1], pPerfRsp, reportLen);

        struct mmw_noise_est_final_report_t* noise_est_report_result = (struct mmw_noise_est_final_report_t*)pPerfRsp;
        if (noise_est_report_result->fail_msk || noise_est_report_result->invalid_msk) {
            FT_LOG_PRINT("perf result fail\n");
            reportData[0] = HIF_CMD_STATUS_IO;
        } else {
            reportData[0] = HIF_CMD_STATUS_SUCCESS;
        }

        HIF_MsgReport(HIF_MSG_ID_MMW_PERF_TEST, &reportData, reportLen + 1, NULL);
    }

    ft_noise_est_deinit();

    FT_SetState(FT_STATE_OPEN);

    FT_LOG_PRINT("FT Mmw Perf Report\n");
}


#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
    const uint16_t ftPerfMsgLen = 118;     /* perf_test cmd length is 118 */
#elif CONFIG_SOC_SERIES_RS613X
    const uint16_t ftPerfMsgLen = 68;      /* perf_test cmd length is 68 */
#else
    #error "not support chip!"
#endif

static int FT_MmwPerfHandler(HIF_MsgHdr_t *msg)
{
    int status = 0;
    int ret = HIF_CMD_STATUS_SUCCESS;
    uint16_t rspLen = 0;

    FT_LOG_PRINT("FT_MmwPerfHandler\n");

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        if (ftPerfMsgLen != msg->length) {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

        uint8_t * pData = (uint8_t *)(msg + 1);

        FT_Mode_t ftMode = FT_GetMode();
        if (ftMode == FT_MODE_RESPOND) {

            FT_SetState(FT_STATE_BUSY);

            uint8_t *pPerfRsp = FT_MmwPerf(pData);
            if (pPerfRsp == NULL) {
                ret = HIF_CMD_STATUS_SYSERR;
				FT_SetState(FT_STATE_OPEN);
				break;
            } else {
                uint8_t *pRspData = HIF_Msg_AckDataPtr(msg);
                rspLen = sizeof(struct mmw_noise_est_final_report_t);
                memcpy(pRspData, pPerfRsp, rspLen);

                struct mmw_noise_est_final_report_t* noise_est_report_result = (struct mmw_noise_est_final_report_t*)pRspData;
                if (noise_est_report_result->fail_msk || noise_est_report_result->invalid_msk) {
                    FT_LOG_PRINT("perf result fail\n");
                    ret = HIF_CMD_STATUS_IO;
                }
            }

            ft_noise_est_deinit();

            FT_SetState(FT_STATE_OPEN);

        } else { /* report mode */

            FT_SetState(FT_STATE_BUSY);

            uint8_t * pData = (uint8_t *)(msg + 1);
            status = FT_PushMsg(msg->msg_id, msg->length, pData);
            if (status != 0) {
                ret = HIF_CMD_STATUS_IO;
            }
        }

    } while (0);


    FT_LOG_PRINT("FT Mmw Perf (%d)\n", ret);

    return HIF_MsgResp(msg, rspLen, ret);
}



static uint8_t * FT_MmwAngleEst(uint8_t *pData)
{
    int status = 0;
	uint8_t angle_est_work_mode = 0;

    status = factory_test_angle_est_init(pData, &angle_est_work_mode);
	 if (status != 0) {
        FT_LOG_PRINT("angle est init fail\n");
        return NULL;
    }
	status = mmw_angle_est_start(&angle_frame, angle_est_work_mode);
    if (status != 0) {
        mmw_ctrl_stop();
        FT_LOG_PRINT("angle est start fail\n");
        return NULL;
    }

    status = FT_RspSemWait(20000);  /* 20S */
    if (status != 0) {
        FT_LOG_PRINT("angle est timeout status:%d\n", status);
        return NULL;
    }

	ft_wait_for_mmw_callback_returns();

	if (angle_est_work_mode == FT_ANGELE_CALIB_MODE) {
		angle_est_work_mode = FT_ANGELE_CALCULATE_MODE;
		status = mmw_angle_est_start(&angle_frame, angle_est_work_mode);
		status = FT_RspSemWait(20000);  /* 20S */
		if (status != 0) {
			FT_LOG_PRINT("angle est timeout status:%d\n", status);
			return NULL;
		}

		ft_wait_for_mmw_callback_returns();
	}

    struct mmw_angle_est_report_t** angle_est_report = get_radar_angle_est_report();

    return (uint8_t *)(*angle_est_report);
}



void FT_MmwAngleEstTask(uint8_t *pData)
{
    FT_SetState(FT_STATE_BUSY);

    uint8_t *pPerfRsp = FT_MmwAngleEst(pData);
    if (pPerfRsp == NULL) {
        uint8_t retStatus = HIF_CMD_STATUS_SYSERR;
        g_ft_mmw_calib_status = HIF_ERRCODE_IO_ERROR;
        HIF_MsgReport(HIF_MSG_ID_MMW_PERF_TEST, &retStatus, 1, NULL);
    } else {
        uint16_t reportLen = sizeof(struct mmw_angle_est_report_t);
        uint8_t reportData[reportLen + 1];
        memcpy(&reportData[1], pPerfRsp, reportLen);
        g_ft_mmw_calib_status = HIF_ERRCODE_SUCCESS;

        struct mmw_angle_est_report_t* angle_est_report = (struct mmw_angle_est_report_t*)pPerfRsp;
        if (angle_est_report->fail_msk) {   /* angle est fail */
            FT_LOG_PRINT("angle est result fail\n");
            reportData[0] = HIF_CMD_STATUS_IO;
            g_ft_mmw_calib_status = HIF_ERRCODE_IO_ERROR;
        } else {
            reportData[0] = HIF_CMD_STATUS_SUCCESS;
            g_ft_mmw_calib_status = HIF_ERRCODE_SUCCESS;
        }



        HIF_MsgReport(HIF_MSG_ID_MMW_PERF_TEST, &reportData, reportLen + 1, NULL);
    }

    mmw_radar_angle_est_deinit();

    FT_SetState(FT_STATE_OPEN);

    FT_LOG_PRINT("FT Mmw Angle Est Report\n");
}



static int FT_MmwAngleEstHandler(HIF_MsgHdr_t *msg)
{
    int status = 0;
    int ret = HIF_CMD_STATUS_SUCCESS;
    uint16_t rspLen = 0;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        if (10 != msg->length) {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

        uint8_t * pData = (uint8_t *)(msg + 1);

        FT_Mode_t ftMode = FT_GetMode();
        if (ftMode == FT_MODE_RESPOND) {

            FT_SetState(FT_STATE_BUSY);

            uint8_t *pPerfRsp = FT_MmwAngleEst(pData);
            if (pPerfRsp == NULL) {
                ret = HIF_CMD_STATUS_SYSERR;
                g_ft_mmw_calib_status = HIF_ERRCODE_IO_ERROR;
            } else {
                uint8_t *pRspData = HIF_Msg_AckDataPtr(msg);
                rspLen = sizeof(struct mmw_angle_est_report_t);
                memcpy(pRspData, pPerfRsp, rspLen);
                struct mmw_angle_est_report_t* angle_est_report = (struct mmw_angle_est_report_t*)pPerfRsp;
                if (angle_est_report->fail_msk) {   /* angle est fail */
                    FT_LOG_PRINT("angle est result fail\n");
                    ret = HIF_CMD_STATUS_IO;
                    g_ft_mmw_calib_status = HIF_ERRCODE_IO_ERROR;
                } else {
                    ret = HIF_CMD_STATUS_SUCCESS;
                    g_ft_mmw_calib_status = HIF_ERRCODE_SUCCESS;
                }
            }

            mmw_radar_angle_est_deinit();

            FT_SetState(FT_STATE_OPEN);
        } else { /* report mode */

            FT_SetState(FT_STATE_BUSY);

            uint8_t * pData = (uint8_t *)(msg + 1);
            status = FT_PushMsg(msg->msg_id, msg->length, pData);
            if (status != 0) {
                ret = HIF_CMD_STATUS_IO;
            }
        }

    } while (0);


    FT_LOG_PRINT("FT Mmw angle est (%d)\n", ret);

    return HIF_MsgResp(msg, rspLen, ret);
}



#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
    const uint16_t calib_data_save_len = CONFIG_MAX_MIMO_ANT * 4;
#elif CONFIG_SOC_SERIES_RS613X
    const uint16_t calib_data_save_len = (CONFIG_MAX_MIMO_ANT + 2) * 4;
#else
	#error "not support chip!"
#endif

static int FT_SaveCalibHandler(HIF_MsgHdr_t *msg)
{
    int ret = HIF_CMD_STATUS_SUCCESS;
    uint16_t rspLen = 0;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }


        if (msg->length) {
            if ((calib_data_save_len + 1) != msg->length) {
                ret = HIF_CMD_STATUS_PARAM;
                break;
            }

            uint8_t *subid = (uint8_t *)(msg + 1);
            uint8_t *param = (uint8_t *)(subid + 1);

            if(*subid != 0x01) {
                ret = HIF_CMD_STATUS_UNSUPPORT;
                break;
            }

            FT_SetState(FT_STATE_BUSY);

            int ret = mmw_angle_save_calib_data_custom(param);
            if (ret == 0) {
                ret = HIF_CMD_STATUS_SUCCESS;
                set_ft_angle_calculate_data(FT_CAL_DATA_FLASH);
            } else {            /* save error*/
                ret = HIF_CMD_STATUS_SYSERR;
            }

            FT_SetState(FT_STATE_OPEN);

        } else {
            FT_SetState(FT_STATE_BUSY);

            if (g_ft_mmw_calib_status == HIF_ERRCODE_NOT_READY) {
                ret = HIF_CMD_STATUS_IO;
                LOG_PRINT("not calib\n");
            } else {
                if(g_ft_mmw_calib_status == HIF_CMD_STATUS_SUCCESS) {
                    if (mmw_angle_save_calib_data()) {
                        ret = HIF_CMD_STATUS_SYSERR;
                    }
                } else {
                    ret = HIF_CMD_STATUS_STATE;
                }
                g_ft_mmw_calib_status = HIF_ERRCODE_NOT_READY;
            }

            FT_SetState(FT_STATE_OPEN);
        }
    } while (0);


    FT_LOG_PRINT("FT get calib (%d)\n", ret);

    return HIF_MsgResp(msg, rspLen, ret);
}





#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
    const uint16_t calib_data_len = CONFIG_MAX_MIMO_ANT * 4;
#elif CONFIG_SOC_SERIES_RS613X
    const uint16_t calib_data_len = HWINFO_SIZE_NVM_MMW_CALIB;
#else
	#error "not support chip!"
#endif

static int FT_GetCalibHandler(HIF_MsgHdr_t *msg)
{
    int ret = HIF_CMD_STATUS_SUCCESS;
    uint16_t rspLen = 0;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        FT_SetState(FT_STATE_BUSY);

        uint8_t *ack_data = HIF_Msg_AckDataPtr(msg);
        uint8_t calib_data[calib_data_len];
        ret = mmw_angle_get_calib_data((uint32_t *)&calib_data);
        if (ret == 0) {
            memcpy(ack_data, &calib_data, sizeof(calib_data));
            rspLen = calib_data_len;
        } else {
            ret = HIF_CMD_STATUS_IO;
        }

        FT_SetState(FT_STATE_OPEN);
    } while (0);


    FT_LOG_PRINT("FT get calib (%d)\n", ret);

    return HIF_MsgResp(msg, rspLen, ret);
}



static int FT_VoltageHandler(HIF_MsgHdr_t *msg)
{
    int ret = HIF_CMD_STATUS_SUCCESS;
    uint16_t rspLen = 0;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        if (8 != msg->length) {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

        struct dcdc_vol_t *vol = (struct dcdc_vol_t *)(msg + 1);

        uint8_t *ack_data = HIF_Msg_AckDataPtr(msg);

        FT_SetState(FT_STATE_BUSY);

        uint32_t adc_val = ft_ctrl_adc_read_mv();

        if (adc_val > vol->min && adc_val < vol->max) {
            ret = HIF_CMD_STATUS_SUCCESS;
        } else {
            ret = HIF_CMD_STATUS_IO;
        }

        rspLen = sizeof(adc_val);
        memcpy(ack_data, &adc_val, rspLen);

        FT_SetState(FT_STATE_OPEN);

        FT_LOG_PRINT("%d\n", adc_val);
    } while (0);


    FT_LOG_PRINT("FT Voltage (%d)\n", ret);

    return HIF_MsgResp(msg, rspLen, ret);
}



static int TF_IoHandler(HIF_MsgHdr_t *msg)
{
    int ret = HIF_CMD_STATUS_SUCCESS;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }


        FT_SetState(FT_STATE_BUSY);

        struct io_ctrl_t *io_ctrl = (struct io_ctrl_t *)(msg + 1);
        struct io_output_ctrl_t *io_output_ctrl = (struct io_output_ctrl_t *)(msg + 1);
        struct io_input_ctrl_t *io_input_ctrl = (struct io_input_ctrl_t *)(msg + 1);

        GPIO_Pin_t input_pin, output_pin;
        HAL_Dev_t *input_gpioDev, *output_gpioDev;

        /* input and output mode */
        if (io_ctrl->type == 0) {
            if(msg->length == 3) {
                io_ctrl->input_mode = 0;
            } else if(msg->length != sizeof(struct io_input_ctrl_t)) {
                ret = HIF_CMD_STATUS_PARAM;
                break;
            }

            uint8_t read_value;
            /* init input GPIO */
            input_gpioDev = ft_GPIO_PIN_init(io_ctrl->input, FT_GPIO_DIR_INPUT, io_ctrl->input_mode, &input_pin);
            if (input_gpioDev ==  NULL) {
                ret = HIF_CMD_STATUS_IO;
                break;
            }

            output_gpioDev = ft_GPIO_PIN_init(io_ctrl->output, FT_GPIO_DIR_OUTPUT, 0, &output_pin);
            if (output_gpioDev == NULL) {
                ret = HIF_CMD_STATUS_IO;
                break;
            }

            for (int output_value = 0; output_value < 2; output_value++) {
                /* take turns to writing output pin low/high level */
                if (HAL_GPIO_WritePin(output_gpioDev, output_pin, output_value) != 0) {
                    ret = HIF_CMD_STATUS_STATE;
                    break;
                }

                msleep(10);

                read_value = HAL_GPIO_ReadPin(input_gpioDev, input_pin);
                if (read_value != output_value) {   /* if read value is not equal to write value, break loop */
                    ret = HIF_CMD_STATUS_STATE;
                    break;
                }
                msleep(10);
            }
        /* output mode */
        } else if (io_ctrl->type == 1) {
            if(msg->length != sizeof(struct io_output_ctrl_t)) {
                ret = HIF_CMD_STATUS_PARAM;
                break;
            }
            /* init output GPIO */
            output_gpioDev = ft_GPIO_PIN_init(io_output_ctrl->output, FT_GPIO_DIR_OUTPUT, 0, &output_pin);
            if (output_gpioDev == NULL) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            }

            if (HAL_GPIO_WritePin(output_gpioDev, output_pin, io_output_ctrl->value) != 0) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            }
        /* input mode */
        } else if (io_ctrl->type == 2) {
            if(msg->length == 3) {
                io_input_ctrl->input_mode = 0;
            } else if(msg->length != sizeof(struct io_input_ctrl_t)) {
                ret = HIF_CMD_STATUS_PARAM;
                break;
            }
            /* init input GPIO */
            input_gpioDev = ft_GPIO_PIN_init(io_input_ctrl->input, FT_GPIO_DIR_INPUT, io_input_ctrl->input_mode, &input_pin);
            if (input_gpioDev == NULL) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            }

            uint8_t value = io_input_ctrl->value;
            uint8_t read_value;
            read_value = HAL_GPIO_ReadPin(input_gpioDev, input_pin);
            if (read_value != value) {
                ret = HIF_CMD_STATUS_IO;
                break;
            }
        } else {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

    } while(0);

    FT_SetState(FT_STATE_OPEN);

    return HIF_MsgResp(msg, 0, ret);
}

static uint8_t s_2Hz_mode_flag = 0;	 // 1--> enter 2Hz typical mode.
static int FT_PowerHandler(HIF_MsgHdr_t *msg)
{
    HAL_Status_t status = HAL_STATUS_OK;
    uint8_t ret = HIF_CMD_STATUS_SUCCESS;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } 

        uint8_t *ppwr_ctrl = (uint8_t *)(msg + 1);
        uint8_t test_mode = ppwr_ctrl[0];   // 0 --> standby mode; 1--> 2Hz typical mode.

        /* standby */
        if (test_mode == FT_POWER_MODE_STANDBY) {
			
			if (currentState == FT_STATE_BUSY) {
				ret = HIF_CMD_STATUS_STATE;
				break;
			}
			
			FT_SetState(FT_STATE_BUSY);
            uint8_t wake_mode = ppwr_ctrl[1];
            if (wake_mode == FT_POWER_WAKEUP_MODE_IO) { /* wakeup io */
                uint8_t wakeup_io_pin =  ppwr_ctrl[2];

                if (HAL_STATUS_OK != ft_power_io_wakeup(wakeup_io_pin)) {
                    ret = HIF_CMD_STATUS_IO;
                    FT_SetState(FT_STATE_OPEN);
                    break;
                }
                HIF_PM_SuspendDevice();
            } else if (wake_mode == FT_POWER_WAKEUP_MODE_TIMEROUT) { /* timerout */
                uint32_t timerout_ms = ppwr_ctrl[2] * 1000;

                ft_power_timer_wakeup(timerout_ms);
                HIF_PM_SuspendDevice();
            } else {
                ret = HIF_CMD_STATUS_PARAM;
                FT_SetState(FT_STATE_OPEN);
                break;
            }
			FT_SetState(FT_STATE_OPEN);
        /* mmw 2Hz */
        } else if (test_mode == FT_POWER_MODE_MMW_2HZ) {
            uint8_t enter_flag = ppwr_ctrl[1];
            if (enter_flag == FT_POWER_MMW_2HZ_ENTER && currentState == FT_STATE_OPEN) {
                status = ft_power_mode_2hz_frame_config();
                if (status) {
                    ret = HIF_CMD_STATUS_IO;
                    FT_SetState(FT_STATE_OPEN);
                    break;
                }

                mmw_ctrl_callback_cfg(&ft_2hz_mode_frame_cb, MMW_DATA_TYPE_1DFFT, NULL);
                mmw_ctrl_start();
#if (CONFIG_MMW_LOOP_TASK_MONITOR)
                mmw_loop_task_time_stop();
#endif
                HIF_PM_SuspendDevice();
				FT_SetState(FT_STATE_BUSY);
				s_2Hz_mode_flag = 1;
            } else if (enter_flag == FT_POWER_MMW_2HZ_QUIT && currentState == FT_STATE_BUSY) {
				if (s_2Hz_mode_flag == 1) {
					mmw_ctrl_stop();
					HIF_PM_WakeupDevice();
					FT_SetState(FT_STATE_OPEN);
					s_2Hz_mode_flag = 0;
				} else {
					ret = HIF_CMD_STATUS_IO;
					break;
				}
               
            } else {
                ret = HIF_CMD_STATUS_PARAM;
                break;
            }
        }

    } while(0);
    return HIF_MsgResp(msg, 0, ret);
}


static uint8_t s_trigger_mode_flag = 0;		// 1--> enter trigger mode.
static int FT_TriggerFuncHandler(HIF_MsgHdr_t *msg)
{
    uint8_t ret = HIF_CMD_STATUS_SUCCESS;
    int status = 0;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        }

        uint8_t test_mode = *((uint8_t *)(msg + 1));    // 1 --> enter; 0 --> quit;

        if (test_mode == FT_TRIGGER_MODE_ENTER && currentState == FT_STATE_OPEN) {
            FT_SetState(FT_STATE_BUSY);
            status = factory_test_trigger_process();
            if(status != 0){
                ret = HIF_CMD_STATUS_IO;
				 FT_SetState(FT_STATE_OPEN);
                break;
            }
			s_trigger_mode_flag = 1;
        } else if (test_mode == FT_TRIGGER_MODE_QUIT && currentState == FT_STATE_BUSY && s_trigger_mode_flag == 1) {
            ft_trigger_deinit(ft_trigger_wkio_Dev);
            FT_SetState(FT_STATE_OPEN);
			s_trigger_mode_flag = 0;
        } else {
            ret = HIF_CMD_STATUS_STATE;
            break;
        }
    } while(0);

    return HIF_MsgResp(msg, 0, ret);
}



static int FT_GetIqDataHandler(HIF_MsgHdr_t *msg)
{

    int ret = HIF_CMD_STATUS_SUCCESS;
    uint16_t rspLen = 0;
    uint8_t *ack_data = HIF_Msg_AckDataPtr(msg);
    uint8_t iq_data[CONFIG_MAX_MIMO_ANT * 4] = {0x00};

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        if (g_ft_mmw_calib_status == HIF_ERRCODE_NOT_READY) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        }

        if (g_ft_mmw_calib_status != HIF_CMD_STATUS_SUCCESS) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        }
        FT_SetState(FT_STATE_BUSY);

        mmw_angle_calib_iq_data_get(iq_data, sizeof(iq_data));
        rspLen = sizeof(iq_data);
        memcpy(ack_data, &iq_data, rspLen);

        FT_SetState(FT_STATE_OPEN);
    } while(0);

    return HIF_MsgResp(msg, rspLen, ret);
}



static int FT_RtxPkdetTestHandler(HIF_MsgHdr_t *msg)
{
    float *power_result = 0;
    struct mmw_pkdet_test_result_t *pkdet_test_result = 0;
    int ret = HIF_CMD_STATUS_SUCCESS;
    uint16_t rspLen = 0;

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        struct mmw_pkdet_test_recive_param_t *pkdet_param = (struct mmw_pkdet_test_recive_param_t *)(msg + 1);

        if (pkdet_param->start_freq > pkdet_param->end_freq
            || pkdet_param->freq_num != (pkdet_param->end_freq - pkdet_param->start_freq) / pkdet_param->freq_step + 1
            || pkdet_param->end_freq > 66000
            || pkdet_param->start_freq < 57000) {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

        uint32_t start_freq_Hz = pkdet_param->start_freq * 1000;
        uint32_t end_freq_Hz = pkdet_param->end_freq * 1000;
        uint32_t step_freq_Hz = pkdet_param->freq_step * 1000;
        uint32_t result_len_byte = sizeof(*pkdet_test_result) * pkdet_param->freq_num;

        mmw_process_mem_alloc((void **)&power_result, sizeof(*power_result) * pkdet_param->freq_num);
        if (power_result == NULL) {
            ret = HIF_CMD_STATUS_IO;
            break;
        }
        mmw_process_mem_alloc((void **)&pkdet_test_result, result_len_byte);
        if (pkdet_test_result == NULL) {
            ret = HIF_CMD_STATUS_IO;
            break;
        }

        FT_SetState(FT_STATE_BUSY);

        ft_get_tx_pa_power(start_freq_Hz, end_freq_Hz, step_freq_Hz, power_result);

        /* convert result to q12 */
        for (int idx = 0; idx < pkdet_param->freq_num; idx++) {
            pkdet_test_result[idx].freq = pkdet_param->start_freq + idx * pkdet_param->freq_step;
            pkdet_test_result[idx].power = (uint16_t)(power_result[idx] * 4096.f);
        }

        /* check result is pass, criteria is not supported yet */

        /* response result */
        uint8_t *ack_data = HIF_Msg_AckDataPtr(msg);
        *ack_data = pkdet_param->freq_num;
        memcpy((ack_data + 1), (uint8_t *)pkdet_test_result, result_len_byte);
        rspLen = result_len_byte + 1;

        FT_SetState(FT_STATE_OPEN);
    } while (0);

    if(power_result) {
        mmw_process_mem_free((void **)&power_result);
    }

    if(pkdet_test_result) {
        mmw_process_mem_free((void **)&pkdet_test_result);
    }

    return HIF_MsgResp(msg, rspLen , ret);
}



#if CONFIG_SOC_SERIES_RS624X || CONFIG_SOC_SERIES_RS724X
    const uint16_t ft_perf_msg_len = 123;     /* perf_test cmd length is 123 */
#elif CONFIG_SOC_SERIES_RS613X
    const uint16_t ft_perf_msg_len = 73;      /* perf_test cmd length is 73 */
#else
    #error "not support chip!"
#endif

static void FT_MmwMaxDiffDetectTask(uint8_t *pData)
{
    uint16_t rptLen = 1;
    uint8_t *report_data = NULL;
    uint8_t ret = HIF_CMD_STATUS_SUCCESS;

    do {
        mmw_process_mem_alloc((void **)&report_data, 10*1024);
        if(report_data == NULL) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else {
            report_data[0] = HIF_CMD_STATUS_SUCCESS;
        }

        /* get receive HIF msg */
        uint16_t noise_est_mode = *((uint8_t *)pData) | (*((uint8_t *)pData + 1)) << 8;
        uint8_t noise_est_mode_v1 = noise_est_mode & 0x01;
        uint8_t noise_est_mode_v2 = (noise_est_mode & 0x02) >> 1;
        //uint8_t noise_est_mode_clutter_remove = noise_est_mode & 0x04; // not supported yet

        if (factory_test_mmw_max_diff_detect_start((uint8_t*)pData) != 0) {
            report_data[0] = HIF_CMD_STATUS_STATE;
            mmw_ctrl_stop();
            break;
        }

        if (0 != FT_RspSemWait(20000)) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        }

        if (noise_est_mode_v1) {
            struct mmw_noise_est_final_report_t** noise_est_report_result = get_noise_est_report_result();
            if ((*noise_est_report_result)->fail_msk || (*noise_est_report_result)->invalid_msk) { /* noise est fail */
                report_data[0] = HIF_CMD_STATUS_IO;
            }
            rptLen = sizeof(struct mmw_noise_est_final_report_t) + 1;
            memcpy(&report_data[1], (uint8_t*)(*noise_est_report_result), sizeof(struct mmw_noise_est_final_report_t));

            ft_noise_est_deinit();
        } else if (noise_est_mode_v2) {
            g_ft_max_diff_value_detect_finish_flag = 1;

            struct mmw_max_diff_detect_report_t** max_diff_detect_report = get_max_diff_detect_report_param();
            uint32_t noise_data_len_byte = sizeof((*max_diff_detect_report)->noise_est_report);
            uint32_t diff_value_len_byte = sizeof((*max_diff_detect_report)->diff_value_report);
            rptLen = noise_data_len_byte + diff_value_len_byte + 1;
            if ((*max_diff_detect_report)->noise_est_report.fail_msk
                || (*max_diff_detect_report)->noise_est_report.invalid_msk
                || g_max_diff_detect_pass_flag) { /* noise est fail */
                report_data[0] = HIF_CMD_STATUS_IO;
            }

            memcpy(&report_data[1], &((*max_diff_detect_report)->noise_est_report), noise_data_len_byte);
            memcpy(&report_data[1 +  noise_data_len_byte], &((*max_diff_detect_report)->diff_value_report), diff_value_len_byte);

            ft_max_diff_detect_deinit();
        }
    } while(0);

    if (report_data == NULL) {
        HIF_MsgReport(HIF_MSG_ID_MAX_DIFF_DETECT, &ret, rptLen, NULL);
    } else {
        HIF_MsgReport(HIF_MSG_ID_MAX_DIFF_DETECT, report_data, rptLen, NULL);
        mmw_process_mem_free((void **)&report_data);
    }

    FT_SetState(FT_STATE_OPEN);
}


static int FT_MaxDiffDetectHandler(HIF_MsgHdr_t *msg)
{
    uint8_t ret = HIF_CMD_STATUS_SUCCESS;
    uint8_t noise_est_mode_v1 = 0;
    uint8_t noise_est_mode_v2 = 0;
    uint8_t get_diff_value_flag = 0;
    uint16_t rspLen = 0;

    FT_Mode_t ftMode = FT_GetMode();

    do {
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        uint16_t ft_get_max_diff_value_msg_len = 3;
        /* get receive HIF msg */
        uint16_t noise_est_mode = *((uint8_t*)msg + 4) | (*((uint8_t*)msg + 5) <<8);
        get_diff_value_flag = *((uint8_t*)msg + 6);
        noise_est_mode_v1 = noise_est_mode & 0x01;
        noise_est_mode_v2 = (noise_est_mode & 0x02) >> 1;

        if (noise_est_mode_v1 && noise_est_mode_v2) {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

        if (!ft_cmd_param_len_check(msg->length, ft_perf_msg_len)
            && (!ft_cmd_param_len_check(msg->length, ft_get_max_diff_value_msg_len) || !get_diff_value_flag)) {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

        memcpy(g_ft_cmd_param_buffer, (uint8_t*)msg + 4, msg->length);

        if (get_diff_value_flag) {
            if (!g_ft_max_diff_value_detect_finish_flag) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            }

            if (ftMode == FT_MODE_RESPOND) {
                ret = HIF_CMD_STATUS_STATE;
                break;
            }

            FT_SetState(FT_STATE_BUSY);

            uint8_t * pData = (uint8_t *)(msg + 1);
            if (0 != FT_PushMsg(msg->msg_id, msg->length, pData)) {
                ret = HIF_CMD_STATUS_IO;
                break;
            }
        } else {
            if (ftMode == FT_MODE_RESPOND) {
                FT_SetState(FT_STATE_BUSY);

                if (factory_test_mmw_max_diff_detect_start(g_ft_cmd_param_buffer) != 0) {
                    mmw_ctrl_stop();
                    ret = HIF_CMD_STATUS_STATE;
                    FT_SetState(FT_STATE_OPEN);
                    break;
                }

                if (0 != FT_RspSemWait(20000)) {
                    ret = HIF_CMD_STATUS_STATE;
                    FT_SetState(FT_STATE_OPEN);
                    break;
                }


                /* after semphore release, get result of noise estimation, and respond hif cmd */
                if (noise_est_mode_v1) {
                    struct mmw_noise_est_final_report_t** noise_est_report_result = get_noise_est_report_result();
                    uint8_t *ack_data = HIF_Msg_AckDataPtr(msg);
                    memcpy(ack_data, (uint8_t*)noise_est_report_result, sizeof(struct mmw_noise_est_final_report_t));
                    if ((*noise_est_report_result)->fail_msk || (*noise_est_report_result)->invalid_msk) { /* noise est fail */
                        ret = HIF_CMD_STATUS_IO;
                    }

                    rspLen = sizeof(struct mmw_noise_est_final_report_t);
                    ft_noise_est_deinit();

                } else if (noise_est_mode_v2) {
                    g_ft_max_diff_value_detect_finish_flag = 1;
                    uint8_t *ack_data = HIF_Msg_AckDataPtr(msg);
                    struct mmw_max_diff_detect_report_t** max_diff_detect_report = get_max_diff_detect_report_param();
                    uint32_t noise_data_len_byte = sizeof((*max_diff_detect_report)->noise_est_report);
                    uint32_t diff_value_len_byte = sizeof((*max_diff_detect_report)->diff_value_report);
                    uint32_t all_len_byte = noise_data_len_byte + diff_value_len_byte;

                    memcpy(ack_data, &((*max_diff_detect_report)->noise_est_report), noise_data_len_byte);
                    memcpy(ack_data + noise_data_len_byte, &((*max_diff_detect_report)->diff_value_report), diff_value_len_byte);
                    if ((*max_diff_detect_report)->noise_est_report.fail_msk
                        || (*max_diff_detect_report)->noise_est_report.invalid_msk
                        || g_max_diff_detect_pass_flag) { /* noise est fail */
                        ret = HIF_CMD_STATUS_IO;
                    }
                    rspLen = all_len_byte;
                    ft_max_diff_detect_deinit();
                }
                FT_SetState(FT_STATE_OPEN);
            }else if (ftMode == FT_MODE_REPORT) {
                FT_SetState(FT_STATE_BUSY);
                uint8_t * pData = (uint8_t *)(msg + 1);
                if (0 != FT_PushMsg(msg->msg_id, msg->length, pData)) {
                    ret = HIF_CMD_STATUS_IO;
                    break;
                }
            }
        }
    } while(0);

    return HIF_MsgResp(msg, rspLen, ret);
}



static void FT_FreqBinDetectTask(uint8_t *pData)
{
    /* get result of noise estimation */
    uint8_t *report_data = NULL;
    uint16_t rptLen = 1;
    uint8_t ret = HIF_CMD_STATUS_SUCCESS;

    do {
        mmw_process_mem_alloc((void **)&report_data, 4*1024);
        if(report_data == NULL) {
            ret = HIF_CMD_STATUS_STATE;
        } else {
            report_data[0] = HIF_CMD_STATUS_SUCCESS;
        }

        if(0 != factory_test_freq_bin_detect_start((uint8_t *)pData)) {
            report_data[0] = HIF_CMD_STATUS_PARAM;
            mmw_ctrl_stop();
            break;
        }

        if (0 != FT_RspSemWait(20000)) {
            report_data[0] = HIF_CMD_STATUS_STATE;
            break;
        }

        struct mmw_freq_bin_detect_report_t* freq_bin_report = get_freq_bin_report_param();
        rptLen = 1 + 2 + sizeof(*freq_bin_report->typical_value) * freq_bin_report->range_detect_len * CONFIG_MAX_MIMO_ANT;
        memcpy(report_data + 1, freq_bin_report, 2);     /* report param: range_detect_len(2Byte) */
        memcpy(((uint8_t *)report_data + 2 + 1), (uint8_t *)freq_bin_report->typical_value, rptLen - 2);
    } while (0);

    if(report_data == NULL) {
        HIF_MsgReport(HIF_MSG_ID_EACH_FREQ_BIN_DETECT, &ret, rptLen, NULL);
    } else {
        HIF_MsgReport(HIF_MSG_ID_EACH_FREQ_BIN_DETECT, report_data, rptLen, NULL);
        mmw_process_mem_free((void **)&report_data);
    }

    ft_freq_bin_detect_deinit();
    FT_SetState(FT_STATE_OPEN);
}


static int FT_FreqBinDetectHandler(HIF_MsgHdr_t *msg)
{
    int ret = HIF_CMD_STATUS_SUCCESS;

    do{
        FT_State_t currentState = FT_GetState();
        if (currentState == FT_STATE_CLOSE) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        } else if (currentState == FT_STATE_BUSY) {
            ret = HIF_CMD_STATUS_BUSY;
            break;
        }

        uint16_t ft_freq_bin_msg_len = 20;        /* freq_bin detect cmd length is 20 */
        if (!ft_cmd_param_len_check(msg->length, ft_freq_bin_msg_len)) {
            ret = HIF_CMD_STATUS_PARAM;
            break;
        }

        FT_Mode_t ftMode = FT_GetMode();
        if (ftMode == FT_MODE_RESPOND) {
            ret = HIF_CMD_STATUS_STATE;
            break;
        }

        uint8_t * pData = (uint8_t *)(msg + 1);
        if (FT_PushMsg(msg->msg_id, msg->length, pData)) {
            ret = HIF_CMD_STATUS_IO;
            break;
        }

        FT_SetState(FT_STATE_BUSY);
    }while(0);

    return HIF_MsgResp(msg, 0, ret);
}



static void FT_Task(void *arg)
{
    FT_Msg_t ftMsg;

    while (1) {
        FT_PopMsg(&ftMsg);

        FT_LOG_PRINT("FT task: msg(%d), data(%d)\n", ftMsg.msgId, ftMsg.dataLen);

        switch (ftMsg.msgId) {
        case HIF_MSG_ID_MMW_PERF_TEST:
            FT_MmwPerfTask(ftMsg.pData);
            break;

        case HIF_MSG_ID_MMW_CALIB_TEST:
            FT_MmwAngleEstTask(ftMsg.pData);
            break;

        case HIF_MSG_ID_MAX_DIFF_DETECT:
            FT_MmwMaxDiffDetectTask(ftMsg.pData);
            break;

        case HIF_MSG_ID_EACH_FREQ_BIN_DETECT:
            FT_FreqBinDetectTask(ftMsg.pData);
            break;

        default:
            break;
        }

        OSI_Free(ftMsg.pData);
    }
}


int FactoryTest_Init(FT_Callback userCallback)
{
    int retSttaus = 0;
    int status = 0;
    int hifStatus = HIF_ERRCODE_SUCCESS;
    uint8_t reportStatusEna = 0;

    do {
        /* 1>. Enter/Exit Factory Test for Version 2.0 FT_MODE_REPORT
         */
        /* 1.1. Get Factory Test mode and state
         */
        FT_Mode_t ftMode = FT_GetNvmMode();
        FT_Event_t ftEvent = FT_GetNvmEvent();
        FT_ClearNvmEvent();
        FT_SetState(FT_STATE_CLOSE);

        FT_LOG_PRINT("Factory Test mode(%d) event(%d)\n", ftMode, ftEvent);

        if (ftMode == FT_MODE_REPORT) {
            /* enable report status */
            reportStatusEna = 1;

            /* disable inikt status report */
            uint32_t reportInitStatus = 0;
            HIF_ExtControl(HIF_SET_REPORT_INIT_STATUS, &reportInitStatus, 4);

            FT_SetMode(FT_MODE_REPORT);

            if (ftEvent == FT_EVENT_ENTER) {
                HIF_PM_WakeupDevice();

                FT_SetState(FT_STATE_OPEN);

                status = FT_TaskInit();
                if (status != 0) {
                    hifStatus = HIF_CMD_STATUS_SYSERR;
                    break;
                }

                status = FT_RspSemInit();
                if (status != 0) {
                    hifStatus = HIF_CMD_STATUS_SYSERR;
                    break;
                }

                status = FT_MsgQueueInit();
                if (status != 0) {
                    hifStatus = HIF_CMD_STATUS_SYSERR;
                    break;
                }

                hifStatus = HIF_CMD_STATUS_SUCCESS;
                retSttaus = FACTORY_TEST_MODE_VALUE_ENTER;
            } else if (ftEvent == FT_EVENT_EXIT) {

                FT_SetState(FT_STATE_CLOSE);

                hifStatus = HIF_CMD_STATUS_SUCCESS;
                retSttaus = FACTORY_TEST_MODE_VALUE_QUIT;

            } else {
                hifStatus = HIF_ERRCODE_INVALID_PARAM;
            }
        }

        /* 2>. Save callback
         */
        ft_mode_cb = userCallback;


        /* 3> . Messgae Register
         */
        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_FACTORY_MODE, FT_StartHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_MMW_PERF_TEST, FT_MmwPerfHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_MMW_CALIB_TEST, FT_MmwAngleEstHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_PARAM_SAVE, FT_SaveCalibHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_PARAM_GET, FT_GetCalibHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_VOLTAGE_TEST, FT_VoltageHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_IO_TEST, TF_IoHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_PWR_CONSUME_TEST, FT_PowerHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_MMW_FUN_TEST, FT_TriggerFuncHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_MMW_CALIB_IQ_DATA_GET, FT_GetIqDataHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_RTX_PEDET_TEST, FT_RtxPkdetTestHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_MAX_DIFF_DETECT, FT_MaxDiffDetectHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }

        hifStatus = HIF_MsgHdl_Regist(HIF_MSG_ID_EACH_FREQ_BIN_DETECT, FT_FreqBinDetectHandler);
        if (hifStatus != HIF_ERRCODE_SUCCESS) {
            break;
        }
    } while (0);

    if (reportStatusEna) {
        FT_LOG_PRINT("Report status(%d)\n", hifStatus);
        hifStatus = HIF_MsgReport(HIF_MSG_ID_FACTORY_MODE, &hifStatus, 1, NULL);
    }

    if (hifStatus != HIF_ERRCODE_SUCCESS) {
        return -1;
    }

    FT_LOG_PRINT("Factory Test Init (%d)\n", hifStatus);

    return retSttaus;
}

void FactoryTest_Deinit(void) {

	/* free max_diff_value_result buffer */
	struct mmw_max_diff_detect_result_t **g_diff_detect_result = get_diff_detect_result();
	if (*g_diff_detect_result != NULL) {
		mmw_process_mem_free((void **)g_diff_detect_result);
		*g_diff_detect_result = NULL;
	}

	complex16_cube** ft_angle_calib_data = get_ft_calib_data();
	if ((*ft_angle_calib_data) != NULL) {
		mmw_process_mem_free((void **)ft_angle_calib_data);
		*ft_angle_calib_data = NULL;
	}

	complex16_cube** ft_calib_iq_data = get_ft_calib_ant_iq_data();
	if ((*ft_calib_iq_data) != NULL) {
		mmw_process_mem_free((void **)ft_calib_iq_data);
		*ft_calib_iq_data = NULL;
	}

}

void ft_wait_for_mmw_callback_returns(void)
{
	int mmw_state = 0;
	int to = 0;
	do {
		/* get mmw state, 4: runing */
		mmw_state = can_mmw_configured();
		if ((mmw_state != 4) && (mmw_event_process_completed())) {
			/* alreay stop */
			break;
		} else {
			/* prevent freezing, max 200ms */
			if (to >= 200) {
				break;
			}
			OSI_MSleep(1);
		}
		to++;
	} while (1);
}
