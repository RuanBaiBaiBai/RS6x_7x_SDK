/**
 **************************************************************************************************
 * @file    host_driver.c
 * @brief   Implementation of host driver APIs.
 * @attention
 *          Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */
#include "host_driver.h"
#include "host_mmw_api.h"  //get 0x00-0x0F macro

#define HOST_DEVICE_SYNC_PERIOD_MS             10
#define HOST_DEVICE_SYNC_CNT_LIMIT             30
#define HOST_DEVICE_BURN_BUFF_SIZE             512

#define HOST_DEVICE_STATE_NONE                 0
#define HOST_DEVICE_STATE_REGIST               1
#define HOST_DEVICE_STATE_OPEN                 2
typedef struct {
	DEV_HANDLE nextDev;
	LLC_HANDLE llc_hdl;
	HIF_HANDLE hif_hdl;
	uint8_t    valid_mark;
	uint8_t    burn_mode;
	uint8_t    burn_proto;
	uint8_t    dev_state;

    DevParam   devParam_download;
    DevParam   devParam_function;
} DevLocal_t;

struct {
	host_os_sem_t oper_sem;
	DevLocal_t   *pDevs;
	uint32_t      devs_num;
} g_host_drv;

struct {
    host_os_sem_t oper_sem;
    DevLocal_t   *pDevs;
    uint32_t      devs_num;
    uint8_t       bus_param;
    uint32_t      bus_speed;
} g_host_suspend_drv;

bool Host_DevHdl_IsValid(DEV_HANDLE DevHdl)
{
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	return ((DEV_HANDLE)local != NULL &&local->valid_mark == 'V');
}

static DevLocal_t* Host_Driver_GetDev(void)
{
	DevLocal_t *local = g_host_drv.pDevs;
	if ((DEV_HANDLE)local != DEV_HANDLE_INVALID) {
		g_host_drv.pDevs = (DevLocal_t *)local->nextDev;
	}
	return local;
}

static void Host_Driver_PutDev(DevLocal_t *local)
{
	host_os_sem_take(g_host_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);
	local->nextDev = (DEV_HANDLE)g_host_drv.pDevs;
	g_host_drv.pDevs = local;
	g_host_drv.devs_num++;
	host_os_sem_give(g_host_drv.oper_sem);
}

static DevLocal_t* Host_Driver_FindNextDev(DevLocal_t *local)
{
    if ((DEV_HANDLE)local == DEV_HANDLE_INVALID) {
        return g_host_drv.pDevs;
    }

    host_os_sem_take(g_host_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);
    DevLocal_t* parent = g_host_drv.pDevs;
    while ((DEV_HANDLE)parent != DEV_HANDLE_INVALID) {
        if (parent == (DEV_HANDLE)local) {
            break;
        }
        parent = (DevLocal_t *)parent->nextDev;
    }
    host_os_sem_give(g_host_drv.oper_sem);

    if ((DEV_HANDLE)parent != DEV_HANDLE_INVALID) {
        return parent->nextDev;
    } else {
        return (DevLocal_t *)DEV_HANDLE_INVALID;
    }
}

static void Host_Driver_RemoveDev(DevLocal_t *local)
{
	if (host_os_sem_is_valid(g_host_drv.oper_sem)) {
		host_os_sem_take(g_host_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);

		DevLocal_t *parent = g_host_drv.pDevs;
		if (parent == local) {
			g_host_drv.pDevs = (DevLocal_t *)local->nextDev;
			g_host_drv.devs_num--;
		} else {
			while ((DEV_HANDLE)parent != DEV_HANDLE_INVALID) {
				if (parent->nextDev == (DEV_HANDLE)local) {
					parent->nextDev = local->nextDev;
					g_host_drv.devs_num--;
					break;
				} 
				parent = (DevLocal_t *)parent->nextDev;
			}
		}
		host_os_sem_give(g_host_drv.oper_sem);
	}
}

static DevLocal_t* Host_Driver_GetSuspendDev(void)
{
    DevLocal_t *local = g_host_suspend_drv.pDevs;
    if ((DEV_HANDLE)local != DEV_HANDLE_INVALID) {
        g_host_suspend_drv.pDevs = (DevLocal_t *)local->nextDev;
    }
    return local;
}

static void Host_Driver_PutSuspendDev(DevLocal_t *local)
{
    host_os_sem_take(g_host_suspend_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);
    local->nextDev = (DEV_HANDLE)g_host_suspend_drv.pDevs;
    g_host_suspend_drv.pDevs = local;
    g_host_suspend_drv.devs_num++;
    host_os_sem_give(g_host_suspend_drv.oper_sem);
}

static DevLocal_t* Host_Driver_FindNextSuspendDev(DevLocal_t *local)
{
    if ((DEV_HANDLE)local == DEV_HANDLE_INVALID) {
        return g_host_suspend_drv.pDevs;
    }

    host_os_sem_take(g_host_suspend_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);
    DevLocal_t* parent = g_host_suspend_drv.pDevs;
    while ((DEV_HANDLE)parent != DEV_HANDLE_INVALID) {
        if (parent == (DEV_HANDLE)local) {
            break;
        }
        parent = (DevLocal_t *)parent->nextDev;
    }
    host_os_sem_give(g_host_suspend_drv.oper_sem);

    if ((DEV_HANDLE)parent != DEV_HANDLE_INVALID) {
        return parent->nextDev;
    } else {
        return (DevLocal_t *)DEV_HANDLE_INVALID;
    }
}

static void Host_Driver_RemoveSuspendDev(DevLocal_t *local)
{
    if (host_os_sem_is_valid(g_host_suspend_drv.oper_sem)) {
        host_os_sem_take(g_host_suspend_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);

        DevLocal_t *parent = g_host_suspend_drv.pDevs;
        if (parent == local) {
            g_host_suspend_drv.pDevs = (DevLocal_t *)local->nextDev;
            g_host_suspend_drv.devs_num--;
        } else {
            while ((DEV_HANDLE)parent != DEV_HANDLE_INVALID) {
                if (parent->nextDev == (DEV_HANDLE)local) {
                    parent->nextDev = local->nextDev;
                    g_host_suspend_drv.devs_num--;
                    break;
                } 
                parent = (DevLocal_t *)parent->nextDev;
            }
        }
        host_os_sem_give(g_host_suspend_drv.oper_sem);
    }
}

int	Host_Driver_Init(void)
{
	int ret = HOST_ERRCODE_SUCCESS;
	if (!host_os_sem_is_valid(g_host_drv.oper_sem)) {
		g_host_drv.pDevs = (DevLocal_t *)DEV_HANDLE_INVALID;
        g_host_drv.devs_num = 0;
		g_host_drv.oper_sem = host_os_sem_create(1, 1);
		if (!host_os_sem_is_valid(g_host_drv.oper_sem)) {
			HOST_LOG_ERR("%s:oper_sem Invalid!\n", __func__);
			return HOST_ERRCODE_NO_BUFFER;
		}
        if (!host_os_sem_is_valid(g_host_suspend_drv.oper_sem)) {
            g_host_suspend_drv.pDevs = (DevLocal_t *)DEV_HANDLE_INVALID;
            g_host_suspend_drv.devs_num = 0;
            g_host_suspend_drv.oper_sem = host_os_sem_create(1, 1);
            if (!host_os_sem_is_valid(g_host_suspend_drv.oper_sem)) {
                host_os_sem_delete(g_host_drv.oper_sem);
                g_host_drv.oper_sem = NULL;
                HOST_LOG_ERR("%s:suspend oper_sem Invalid!\n", __func__);
                return HOST_ERRCODE_NO_BUFFER;
            }
        }
		ret = LLC_Init();
		if (ret) {
            host_os_sem_delete(g_host_suspend_drv.oper_sem);
            g_host_suspend_drv.oper_sem = NULL;
			host_os_sem_delete(g_host_drv.oper_sem);
            g_host_drv.oper_sem = NULL;
			HOST_LOG_ERR("%s:LLC Init Failed(%d)!\n", __func__, ret);
		}
		HOST_LOG_DBG("%s: End!\n", __func__);
	}
	return ret;
}

void Host_Driver_Deinit(void)
{
	if (host_os_sem_is_valid(g_host_drv.oper_sem)) {
		host_os_sem_take(g_host_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);
		HOST_LOG_DBG("%s: DevNum %u!\n", __func__, g_host_drv.devs_num);

		/* Close all devices */
		DevLocal_t *local = Host_Driver_GetDev();
		while (local != NULL) {
			local->valid_mark = 0;
			if (local->llc_hdl != NULL) {
				if (local->dev_state == HOST_DEVICE_STATE_OPEN) {
					LLC_Device_Close(local->llc_hdl);
				}
				LLC_Device_UnRegist(local->llc_hdl);
				local->llc_hdl = NULL;
			}
			if (local->hif_hdl != NULL) {
				Mmw_Report_Handle_FreeAll((DEV_HANDLE)local);
				HIF_Entity_DeInit(local->hif_hdl);
				local->hif_hdl = NULL;
			}
			host_os_free(local);
			local = Host_Driver_GetDev();
		}
		g_host_drv.devs_num = 0;

        if (host_os_sem_is_valid(g_host_suspend_drv.oper_sem)) {
            host_os_sem_take(g_host_suspend_drv.oper_sem, HOST_OS_TIMEOUT_FOREVER);
            HOST_LOG_DBG("%s: suspend DevNum %u!\n", __func__, g_host_suspend_drv.devs_num);

            /* Close all devices */
            local = Host_Driver_GetSuspendDev();
            while (local != NULL) {
                local->valid_mark = 0;
                if (local->llc_hdl != NULL) {
                    if (local->dev_state == HOST_DEVICE_STATE_OPEN) {
                        LLC_Device_Close(local->llc_hdl);
                    }
                    LLC_Device_UnRegist(local->llc_hdl);
                    local->llc_hdl = NULL;
                }
                if (local->hif_hdl != NULL) {
                    Mmw_Report_Handle_FreeAll((DEV_HANDLE)local);
                    HIF_Entity_DeInit(local->hif_hdl);
                    local->hif_hdl = NULL;
                }
                host_os_free(local);
                local = Host_Driver_GetSuspendDev();
            }
            g_host_suspend_drv.devs_num = 0;
            host_os_sem_give(g_host_suspend_drv.oper_sem);
            host_os_sem_delete(g_host_suspend_drv.oper_sem);
            g_host_suspend_drv.oper_sem = NULL;
        }

		/* LLC Deinit */
		LLC_Deinit();
		host_os_sem_give(g_host_drv.oper_sem);
		host_os_sem_delete(g_host_drv.oper_sem);
        g_host_drv.oper_sem = NULL;
	}
}

int Host_Device_Regist(DEV_HANDLE *pDevHdl_out, DevHw_t *hw_cfg, HifCfg_t *hif_cfg)
{
	LLC_HANDLE llc_hdl = NULL;
	HIF_HANDLE hif_hdl = NULL;
	DevLocal_t *local;
	int ret = HOST_ERRCODE_INVALID_PARAM;
	if (pDevHdl_out == NULL || hw_cfg == NULL) {
		return ret;
	}
	if (!host_os_sem_is_valid(g_host_drv.oper_sem)) {
		HOST_LOG_ERR("%s: Host Driver is not Init yet!\n", __func__);
		return HOST_ERRCODE_NOT_READY;
	}
	if (Host_DevHdl_IsValid(*pDevHdl_out)) {
		return HOST_ERRCODE_STATE;
	}

	local = host_os_malloc(sizeof(DevLocal_t));
	if (local != NULL) {
		/* 1. Init LLC Handle */
		llc_hdl = LLC_Device_Regist(hw_cfg);
		if (llc_hdl == NULL) {
			HOST_LOG_ERR("%s:LLC_Device_Regist Failed!\n", __func__);
			goto open_failed;
		}

		/* 2. Init HIF Handle */
		if (hw_cfg->upload_type == LLC_UPLOAD_TYPE_PASSIVE) {
			hif_cfg->poll_enable = true;
		}
		hif_hdl = HIF_Entity_Init(hif_cfg, llc_hdl);
		if (hif_hdl == NULL) {
			HOST_LOG_ERR("%s:HIF_Entity_Init Failed!\n", __func__);
			goto open_failed;
		}

		/* 5. Update device data and return */
		local->llc_hdl  = llc_hdl;
		local->hif_hdl  = hif_hdl;
		local->valid_mark = 'V';
		local->burn_mode  = 0;
		local->burn_proto = HOST_BURN_PROTOCOL_NONE;
		local->dev_state  = HOST_DEVICE_STATE_REGIST;
        if (hw_cfg->bus_type == LLC_BUS_TYPE_I2C) {
            local->devParam_download = 0x30;
            local->devParam_function = hw_cfg->param;
        } else {
            local->devParam_download = hw_cfg->param;
            local->devParam_function = hw_cfg->param;
        }
		Host_Driver_PutDev(local);
		*pDevHdl_out = (DEV_HANDLE)local;
		HOST_LOG_DBG("%s: Dev(0x%p)!\n", __func__, local);
		return HOST_ERRCODE_SUCCESS;
	} else {
		ret = HOST_ERRCODE_NO_BUFFER;
		HOST_LOG_ERR("%s: malloc(size=%d) a Device failed!\n",
					__func__, sizeof(DevLocal_t));
	}

open_failed:
	if (hif_hdl != NULL) {
		HIF_Entity_DeInit(hif_hdl);
	}
	if (llc_hdl != NULL) {
		LLC_Device_UnRegist(llc_hdl);
	}
	if (local != NULL) {
		host_os_free(local);
	}
	return ret;
}

void Host_Device_Unregist(DEV_HANDLE DevHdl)
{
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	if (Host_DevHdl_IsValid(DevHdl)) {
		Host_Driver_RemoveDev(local);
		if (local->dev_state == HOST_DEVICE_STATE_OPEN) {
			LLC_Device_Close(local->llc_hdl);
		}
		Mmw_Report_Handle_FreeAll(DevHdl);
		HIF_Entity_DeInit(local->hif_hdl);
		LLC_Device_UnRegist(local->llc_hdl);
		local->llc_hdl = NULL;
		local->hif_hdl = NULL;
		local->valid_mark = 0;
		host_os_free(local);
		HOST_LOG_DBG("%s: Dev(0x%p)!\n", __func__, DevHdl);
	} else {
		HOST_LOG_ERR("%s: Bad Dev Handle(0x%p)!\n", __func__, DevHdl);
	}
}

int Host_Device_Open(DEV_HANDLE DevHdl)
{
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	if (Host_DevHdl_IsValid(DevHdl)) {
		DevHw_t *hw_cfg;
		/* 1. Open LLC Handle and PowerUp Device*/
		int ret = LLC_Device_Open(local->llc_hdl);
		if (ret != HOST_ERRCODE_SUCCESS) {
			HOST_LOG_ERR("%s: LLC_Device_Open Failed(%d)!\n", __func__, ret);
			return ret;
		}
        do {  // io
            ret = LLC_IO_Enable_Rst(local->llc_hdl);
    		if (ret != HOST_ERRCODE_SUCCESS) {
    			HOST_LOG_ERR("%s: Device Enable RST IO Failed(%d)!\n", __func__, ret);
    			break;
    		}
    		ret = LLC_IO_Set_Rst(local->llc_hdl, 1);
    		if (ret != HOST_ERRCODE_SUCCESS) {
    			HOST_LOG_ERR("%s: Device PowerUp Failed(%d)!\n", __func__, ret);
    			break;
    		}
        } while (0);
        if (ret != HOST_ERRCODE_SUCCESS) {
    		LLC_Device_Close(local->llc_hdl);
            return ret;
        }

        /* 2. Enable LLC Handle Notify IO IRQ */
		hw_cfg = LLC_Device_Get_DevHwCfg(local->llc_hdl);
        if (NULL != hw_cfg && hw_cfg->upload_type == LLC_UPLOAD_TYPE_PASSIVE) {
            ret = LLC_IO_Notify_Irq_Control(local->llc_hdl, true);
            if (ret != HOST_ERRCODE_SUCCESS) {
                LLC_IO_Set_Rst(local->llc_hdl, 0);
                LLC_Device_Close(local->llc_hdl);
                HOST_LOG_ERR("%s: LLC_IO_Notify_Irq_Control Failed(%d)!\n", __func__, ret);
               return ret;
            }
        }

		HIF_Config_Confirm(local->hif_hdl);
		local->dev_state  = HOST_DEVICE_STATE_OPEN;
		HOST_LOG_DBG("%s: Dev(0x%p)!\n", __func__, DevHdl);
	} else {
		HOST_LOG_ERR("%s: Bad Dev Handle(0x%p)!\n", __func__, DevHdl);
		return HOST_ERRCODE_INVALID_HANDLE;
	}
	return HOST_ERRCODE_SUCCESS;
}

int Host_Device_Close(DEV_HANDLE DevHdl)
{
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	if (Host_DevHdl_IsValid(DevHdl)) {
		DevHw_t *hw_cfg = LLC_Device_Get_DevHwCfg(local->llc_hdl);
		if (NULL != hw_cfg && hw_cfg->upload_type == LLC_UPLOAD_TYPE_PASSIVE) {
			LLC_IO_Notify_Irq_Control(local->llc_hdl, false);
		}
		LLC_IO_Set_Rst(local->llc_hdl, 0);
		LLC_Device_Close(local->llc_hdl);
		local->dev_state  = HOST_DEVICE_STATE_REGIST;
		HIF_Entity_DevReset(local->hif_hdl);
		HOST_LOG_DBG("%s: Dev(0x%p)!\n", __func__, DevHdl);
	} else {
		HOST_LOG_ERR("%s: Bad Dev Handle(0x%p)!\n", __func__, DevHdl);
		return HOST_ERRCODE_INVALID_HANDLE;
	}
	return HOST_ERRCODE_SUCCESS;
}

int Host_Device_GetState(DEV_HANDLE DevHdl, DevState_t *pState_out)
{
#if (HOST_DEVICE_STATE_INFO)
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	if (pState_out != NULL && Host_DevHdl_IsValid(DevHdl)) {
		pState_out->dev_state = local->dev_state;
		pState_out->hw_cfg    = (DevHw_t *)LLC_Device_Get_DevHwCfg(local->llc_hdl);
		pState_out->hif_cfg   = HIF_Config_Get(local->hif_hdl);
		pState_out->hif_state = HIF_State_Get(local->hif_hdl);
		pState_out->hif_cnt   = HIF_Counters_Get(local->hif_hdl);
		return HOST_ERRCODE_SUCCESS;
	} else {
		return HOST_ERRCODE_INVALID_PARAM;
	}
#else
	return HOST_ERRCODE_UNSUPPORT;
#endif
}

int Host_Device_Reboot(DEV_HANDLE DevHdl)
{
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	if (Host_DevHdl_IsValid(DevHdl)) {
		LLC_IO_Set_Rst(local->llc_hdl, 0);
		host_os_delayms(20); //20ms
		LLC_IO_Set_Rst(local->llc_hdl, 1);
		HOST_LOG_DBG("%s: Dev(0x%p)!\n", __func__, DevHdl);
		return HOST_ERRCODE_SUCCESS;
	}
	return HOST_ERRCODE_INVALID_PARAM;
}

int Host_Device_Disable_Sleep(DEV_HANDLE DevHdl, bool disable)
{
    DevLocal_t *local = (DevLocal_t *)DevHdl;
    if (Host_DevHdl_IsValid(DevHdl)) {
        return HIF_Entity_DevSleep_Disable(local->hif_hdl, disable);
    }
    return HOST_ERRCODE_INVALID_PARAM;
}

int Host_BurnMode_Enter(DEV_HANDLE DevHdl)
{
    int status = HOST_ERRCODE_SUCCESS;
    bool is_dev_reopened = false;
	DevLocal_t *local = (DevLocal_t *)DevHdl;
    DevHw_t *hw_cfg = NULL;
	if (Host_DevHdl_IsValid(DevHdl)) {
        hw_cfg = LLC_Device_Get_DevHwCfg(local->llc_hdl);
        if (hw_cfg == NULL) {
            HOST_LOG_ERR("%s: get llc device hw cfg fail\n", __func__);
            return HOST_ERRCODE_IO_ERROR;
        }

        /* Close other devices on bus */
        g_host_suspend_drv.bus_param = hw_cfg->bus_param;
        g_host_suspend_drv.bus_speed = hw_cfg->bus_speed;
        do {
            DevLocal_t *temp = Host_Driver_FindNextDev((DevLocal_t *)DEV_HANDLE_INVALID);
            DevLocal_t *temp_next = DEV_HANDLE_INVALID;
            while ((DEV_HANDLE)temp != DEV_HANDLE_INVALID) {
                temp_next = Host_Driver_FindNextDev(temp);
                DevHw_t *temp_hw_cfg = LLC_Device_Get_DevHwCfg(temp->llc_hdl);
                if (temp_hw_cfg == NULL) {
                    HOST_LOG_ERR("%s: get Dev(0x%p) hw cfg fail!\n", __func__, temp);
                    goto recovery_suspend_drv;
                }
                if ((temp_hw_cfg->bus_type == hw_cfg->bus_type)
                    && (temp_hw_cfg->bus_id == hw_cfg->bus_id)
                    && (temp_hw_cfg->bus_param == hw_cfg->bus_param)) {
                    /* Remove */
                    Host_Driver_RemoveDev(temp);
                    /* Close Device */
                    status = Host_Device_Close((DEV_HANDLE)temp);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        /* Put back */
                        Host_Driver_PutDev(temp);
                        HOST_LOG_ERR("%s: close device fail(%d), use app api\n", __func__, status);
                        goto recovery_suspend_drv;
                    }
                    /* Suspend */
                    Host_Driver_PutSuspendDev(temp);
                }
                temp = temp_next;
            }
        } while (0);

        LLC_IO_Enable_Upd(local->llc_hdl);
        LLC_IO_Enable_Rst(local->llc_hdl);
		LLC_IO_Set_Upd(local->llc_hdl, 1);
		LLC_IO_Set_Rst(local->llc_hdl, 0);
		host_os_delayms(20);
		LLC_IO_Set_Rst(local->llc_hdl, 1);
		host_os_delayms(50);
		LLC_IO_Set_Upd(local->llc_hdl, 0);
		LLC_IO_Set_Rst(local->llc_hdl, 0);
		host_os_delayms(20);
		LLC_IO_Set_Rst(local->llc_hdl, 1);
		host_os_delayms(50);
		LLC_IO_Set_Upd(local->llc_hdl, 1);
        LLC_IO_Disable_Upd(local->llc_hdl);
        if (hw_cfg->bus_type == LLC_BUS_TYPE_SPI) {
            hw_cfg = LLC_Device_Get_DevHwCfg(local->llc_hdl);
            status = LLC_Device_Set_Bus_Param(local->llc_hdl, LLC_BUS_PARAM_SPI);
            if (status != HOST_ERRCODE_SUCCESS) {
                HOST_LOG_ERR("%s: set bus param fail(%d)\n", __func__, status);
                goto recovery_suspend_drv;
            }
            status = LLC_Device_Set_Bus_Speed(local->llc_hdl, CFG_HOST_DRIVER_SPI_SPEED_IN_DOWNLOAD);
            if (status != HOST_ERRCODE_SUCCESS) {
                HOST_LOG_ERR("%s: set bus speed to %u fail(%d)\n", __func__, CFG_HOST_DRIVER_SPI_SPEED_IN_DOWNLOAD, status);
                goto recovery_suspend_drv;
            }
        } else if (hw_cfg->bus_type == LLC_BUS_TYPE_I2C) {
            status = LLC_Device_Set_Device_Param(local->llc_hdl, local->devParam_download);
            if (status != HOST_ERRCODE_SUCCESS) {
                HOST_LOG_ERR("%s: set device param fail(%d)\n", __func__, status);
                goto recovery_suspend_drv;
            }
        }
        status = LLC_Device_Open(local->llc_hdl);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("%s: open device fail(%d), use llc api\n", __func__, status);
            goto recovery_suspend_drv;
        }
        is_dev_reopened = true;

		/*TODO: halt other devices while one device burning image */
		//if (local->dev_cfg.hwcfg.bus_type == LLC_BUS_TYPE_SPI) {
		//}

        /* Burn init */
        status = HOST_BURN_Init();
        if (status != HOST_ERRCODE_SUCCESS) {
			HOST_LOG_ERR("%s: Burn Init Failed(%d)!\n", __func__, status);
            goto recovery_suspend_drv;
        }

		/* Burn Proto Init */
		status = HOST_BURN_Protocol_Init(local->llc_hdl);
		if (status != HOST_ERRCODE_SUCCESS) {
            HOST_BURN_DeInit();
			HOST_LOG_ERR("%s: Burn Proto Init Failed(%d)!\n", __func__, status);
			goto recovery_suspend_drv;
		}

		/* Device sync */
		uint8_t proto_type;
		status = HOST_BURN_Sync(200, &proto_type);
		if (status != HOST_ERRCODE_SUCCESS) {
            HOST_BURN_Protocol_DeInit();
            HOST_BURN_DeInit();
			HOST_LOG_ERR("%s: Device Sync Failed(%d)!\n", __func__, status);
			goto recovery_suspend_drv;
		}
		local->burn_mode  = 1;
		local->burn_proto = proto_type;
		return HOST_ERRCODE_SUCCESS;
	}
	return HOST_ERRCODE_INVALID_PARAM;

recovery_suspend_drv:
    // Close
    if (is_dev_reopened) {
        status = Host_Device_Close(DevHdl);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("%s: close Dev(0x%p) fail(%d) when recovery suspend drv\n", __func__, DevHdl, status);
        }
    }
    // Recovery
    DevLocal_t *temp = Host_Driver_FindNextSuspendDev((DevLocal_t *)DEV_HANDLE_INVALID);
    if ((DEV_HANDLE)temp != DEV_HANDLE_INVALID) {
        status = LLC_Device_Set_Bus_Param(local->llc_hdl, g_host_suspend_drv.bus_param);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("%s: recovery bus param fail(%d) when recovery suspend drv\n", __func__, status);
        }
        status = LLC_Device_Set_Bus_Speed(local->llc_hdl, g_host_suspend_drv.bus_speed);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("%s: recovery bus speed fail(%d) when recovery suspend drv\n", __func__, status);
        }
    }
    while ((DEV_HANDLE)temp != DEV_HANDLE_INVALID) {
        // Remove
        Host_Driver_RemoveSuspendDev(temp);
        // Open
        status = Host_Device_Open((DEV_HANDLE)temp);
        if (status != HOST_ERRCODE_SUCCESS) {
            HOST_LOG_ERR("%s: open Dev(0x%p) fail(%d) when recovery suspend drv\n", __func__, temp, status);
        }
        // Put
        Host_Driver_PutDev(temp);
        temp = Host_Driver_FindNextSuspendDev((DevLocal_t *)DEV_HANDLE_INVALID);
    }

    // reset device
    LLC_IO_Set_Rst(local->llc_hdl, 0);
    host_os_delayms(20); //20ms
    LLC_IO_Set_Rst(local->llc_hdl, 1);

    return (status != HOST_ERRCODE_SUCCESS) ? status : HOST_ERRCODE_IO_ERROR;
}

void Host_BurnMode_Exit(DEV_HANDLE DevHdl)
{
    int status = HOST_ERRCODE_SUCCESS;
	DevLocal_t *local = (DevLocal_t *)DevHdl;
    DevHw_t *hw_cfg = NULL;
	if (Host_DevHdl_IsValid(DevHdl) && local->burn_mode) {
		/*TODO: restore other devices while exit burn mode */
		//if (local->dev_cfg.hwcfg.bus_type == LLC_BUS_TYPE_SPI) {
		//}
        HOST_BURN_Protocol_DeInit();
        HOST_BURN_DeInit();
		local->burn_mode = 0;
        local->burn_proto = HOST_BURN_PROTOCOL_NONE;

        LLC_Device_Close(local->llc_hdl);
        LLC_IO_Enable_Upd(local->llc_hdl);
        LLC_IO_Enable_Rst(local->llc_hdl);
		LLC_IO_Set_Upd(local->llc_hdl, 1);
		LLC_IO_Set_Rst(local->llc_hdl, 0);
		host_os_delayms(20);
		LLC_IO_Set_Rst(local->llc_hdl, 1);
        LLC_IO_Disable_Upd(local->llc_hdl);
        hw_cfg = LLC_Device_Get_DevHwCfg(local->llc_hdl);
        if (hw_cfg != NULL) {
            DevLocal_t *temp = Host_Driver_FindNextSuspendDev((DevLocal_t *)DEV_HANDLE_INVALID);
            if ((DEV_HANDLE)temp != DEV_HANDLE_INVALID) {
                // Recovery
                if (hw_cfg->bus_type == LLC_BUS_TYPE_SPI) {
                    status = LLC_Device_Set_Bus_Param(local->llc_hdl, g_host_suspend_drv.bus_param);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("%s: recovery bus param fail(%d)\n", __func__, status);
                    }
                    status = LLC_Device_Set_Bus_Speed(local->llc_hdl, g_host_suspend_drv.bus_speed);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("%s: recovery bus speed fail(%d)\n", __func__, status);
                    }
                } else if (hw_cfg->bus_type == LLC_BUS_TYPE_UART) {
                    // nothing to do
                } else if (hw_cfg->bus_type == LLC_BUS_TYPE_I2C) {
                    status = LLC_Device_Set_Device_Param(local->llc_hdl, local->devParam_function);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("%s: recovery bus param fail(%d)\n", __func__, status);
                    }
                    status = LLC_Device_Set_Bus_Speed(local->llc_hdl, g_host_suspend_drv.bus_speed);
                } else {
                    HOST_LOG_ERR("%s: Dev(0x%p) bus type=%d error!\n", __func__, DevHdl, hw_cfg->bus_type);
                    return ;
                }
                // Open
                while ((DEV_HANDLE)temp != DEV_HANDLE_INVALID) {
                    // Remove
                    Host_Driver_RemoveSuspendDev(temp);
                    // Open
                    int status = Host_Device_Open((DEV_HANDLE)temp);
                    if (status != HOST_ERRCODE_SUCCESS) {
                        HOST_LOG_ERR("%s: open Dev(0x%p) fail(%d)\n", __func__, temp, status);
                    }
                    // Put
                    Host_Driver_PutDev(temp);
                    temp = Host_Driver_FindNextSuspendDev((DevLocal_t *)DEV_HANDLE_INVALID);
                }
            } else {
                HOST_LOG_ERR("%s: suspend dev not have device!\n", __func__);
                return ;
            }
        } else {
            HOST_LOG_ERR("%s: get Dev(0x%p) hw cfg fail\n", __func__, DevHdl);
            return ;
        }
	}
}

int Host_Burn_Image(DEV_HANDLE DevHdl, void *path)
{
	int ret = HOST_ERRCODE_NOT_READY;
    void *img = NULL;
    uint32_t img_len = 0;

    /* 1. open image file */
    ret = HOST_BURN_Open_Image(&img, path, &img_len);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    }
    /* 2. download image file */
    ret = HOST_BURN_Download_Image(img, img_len, HOST_BURN_DEVICE_STORE_FLASH);
    if (ret != HOST_ERRCODE_SUCCESS) {
        HOST_BURN_Close_Image(&img);
        return ret;
    }
    /* 3. close image file */
    HOST_BURN_Close_Image(&img);

    return ret;
}

HIF_HANDLE Host_HIF_Handle_Get(DEV_HANDLE DevHdl)
{
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	if (Host_DevHdl_IsValid(DevHdl)) {
		return local->hif_hdl;
	} else {
		return NULL;
	}
}

int Host_General_Command(DEV_HANDLE DevHdl, uint32_t msg_id, void *param,
					uint32_t param_len, void *resp_buff, uint32_t buffer_len, uint32_t *data_len)
{
	DevLocal_t *local = (DevLocal_t *)DevHdl;
	if (Host_DevHdl_IsValid(DevHdl)) {
		return HIF_Cmd_Request(local->hif_hdl, msg_id, param, param_len, resp_buff, buffer_len, data_len);
	} else {
		HOST_LOG_ERR("%s: Bad Dev Handle(0x%p)!\n", __func__, DevHdl);
		return HOST_ERRCODE_INVALID_PARAM;
	}
}

int HostCmd_Version_Get(DEV_HANDLE DevHdl, uint32_t user_version)
{
    int ret = 0;
    if (user_version == 1) {
        /* customer version */
        uint32_t msg_id = HOST_HIF_MSG_ID_GET_VERSION;
        uint32_t param_len = 1;
        uint32_t buffer_len = 9;
        uint8_t param[1];
        uint8_t resp_buff[9];
        param[0] = 0x40;

        ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
        if (ret != HOST_ERRCODE_SUCCESS) {
            return ret;
        } else {
            HOST_LOG_INF("[customer] major:%d minor:%d revision:%d\nbuild_num:%08x\n", 
                resp_buff[1], resp_buff[2], (resp_buff[3] + (resp_buff[4] << 8)), 
                (resp_buff[5] + (resp_buff[6] << 8) + (resp_buff[7] << 16) + (resp_buff[8] << 24)));
            return resp_buff[0];
        }
    } else if (user_version == 0) {
        /* code version */
        uint32_t msg_id = HOST_HIF_MSG_ID_GET_VERSION;
        uint32_t param_len = 1;
        uint32_t buffer_len = 13;
        uint8_t param[1];
        uint8_t resp_buff[13];
        param[0] = 0x02;

        ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
        if (ret != HOST_ERRCODE_SUCCESS) {
            return ret;
        } else {
            HOST_LOG_INF("[app_code] ir_type:%d ir_minor:%d\nim_minor:%d  im_project:%d im_number:%d\niv_major:%d iv_minor:%d iv_revision:%d\n", 
                resp_buff[1], (resp_buff[2] + (resp_buff[3] << 8)), (resp_buff[4] + (resp_buff[5] << 8)), 
                resp_buff[6], (resp_buff[7] + (resp_buff[8] << 8)),
                resp_buff[9], resp_buff[10], (resp_buff[11] + (resp_buff[12] << 8)));
            return resp_buff[0];
        }

    } else {
        return HOST_ERRCODE_UNSUPPORT;
    }

    return ret;
}

int HostCmd_Device_Reboot(DEV_HANDLE DevHdl, uint32_t reboot)
{
    int ret = 0;
    uint32_t msg_id = HOST_HIF_MSG_ID_HOST_REBOOT;
    uint32_t param_len = 1;
    uint32_t buffer_len = 1;
    uint8_t param[1];
    uint8_t resp_buff[1];
    param[0] = reboot;

    ret = Host_General_Command(DevHdl, msg_id, param, param_len, resp_buff, buffer_len, NULL);
    if (ret != HOST_ERRCODE_SUCCESS) {
        return ret;
    } else {
        return resp_buff[0];
    }
}

/*
 **************************************************************************************************
 * (C) COPYRIGHT POSSUMIC Technology
 * END OF FILE
 */
