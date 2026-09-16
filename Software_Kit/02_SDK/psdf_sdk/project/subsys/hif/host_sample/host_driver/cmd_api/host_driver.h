/**
 **************************************************************************************************
 * @file    host_driver.h
 * @brief   APIs of host driver for using PSIC devices.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.*/
#ifndef _HOST_DRIVER_H_
#define _HOST_DRIVER_H_

/* Includes.*/
#include "../host_config.h"
#include "../protocol/utils/host_types.h"
#include "../protocol/llc/host_llc.h"
#include "../protocol/hif/host_hif.h"
#include "../protocol/burn/host_burn.h"

#include "../porting/host_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */

typedef void*  DEV_HANDLE;
#define DEV_HANDLE_INVALID           ((DEV_HANDLE)(NULL))

typedef struct {
	int         dev_state;
	DevHw_t    *hw_cfg;     /* host_llc.h */
	HifCfg_t   *hif_cfg;    /* host_hif.h */
	HifState_t *hif_state;  /* host_hif.h */
	HifCount_t *hif_cnt;    /* host_hif.h */
} DevState_t;

/* Exported Macro Function.
 * ------------------------------------------------------------------------------------------------
 */

#define HOST_DEVHW_DUMP(dev_hw, tag) \
    do {                                                                            \
        DevHw_t *___cfg___ = (dev_hw);                                              \
        HOST_LOG_PRINT("\n------------------------------\n");                       \
        HOST_LOG_PRINT(tag ":\n");                                                  \
        if (___cfg___ == NULL) {                                                    \
            HOST_LOG_PRINT("dev_hw is NULL\n");                                     \
            break;                                                                  \
        } else {                                                                    \
            HOST_LOG_PRINT("%p\n", ___cfg___);                                      \
        }                                                                           \
        HOST_LOG_PRINT("bus_speed             %u\n", ___cfg___->bus_speed);         \
        HOST_LOG_PRINT("bus_type              %d\n", ___cfg___->bus_type);          \
        HOST_LOG_PRINT("bus_id                %d\n", ___cfg___->bus_id);            \
        HOST_LOG_PRINT("bus_param             %d\n", ___cfg___->bus_param);         \
        HOST_LOG_PRINT("bus_event_method      %d\n", ___cfg___->bus_event_method);  \
        HOST_LOG_PRINT("upd_io                %d\n", ___cfg___->upd_io);            \
        HOST_LOG_PRINT("rst_io                %d\n", ___cfg___->rst_io);            \
        HOST_LOG_PRINT("notify_io             %d\n", ___cfg___->notify_io);         \
        HOST_LOG_PRINT("notify_type           %d\n", ___cfg___->notify_type);       \
        HOST_LOG_PRINT("upload_type           %d\n", ___cfg___->upload_type);       \
        HOST_LOG_PRINT("param                 %u\n", ___cfg___->param);             \
        HOST_LOG_PRINT("\n------------------------------\n");                       \
    } while(0)

/* Exported API functions.
 * ------------------------------------------------------------------------------------------------
 */
/**
 * @brief Initialize HostDriver.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_Driver_Init(void);

/**
 * @brief De-Initialize HostDriver.
 */
void Host_Driver_Deinit(void);

/**
 * @brief Regist a device.
 *
 * @param pDevHdl_out Pointer of device handle.
 *
 * @param hw_cfg Hardware param of device, you can use DevHw_Set_DefaultParam to config this param.
 *
 * @param hif_cfg Configuration of HIF.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_Device_Regist(DEV_HANDLE *pDevHdl_out, DevHw_t *hw_cfg, HifCfg_t *hif_cfg);

/**
 * @brief Un-Regist a device.
 *
 * @param DevHdl Device handle.
 */
void Host_Device_Unregist(DEV_HANDLE DevHdl);

/**
 * @brief Open a device.
 *
 * @param DevHdl Device handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_Device_Open(DEV_HANDLE DevHdl);

/**
 * @brief Close a device.
 *
 * @param DevHdl Device handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_Device_Close(DEV_HANDLE DevHdl);

/**
 * @brief Get state of device.
 *
 * @param DevHdl Device handle.
 *
 * @param pState_out Pointer used to recv state.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_Device_GetState(DEV_HANDLE DevHdl, DevState_t *pState_out);

/**
 * @brief Reboot a device by rst io.
 *
 * @param DevHdl Device handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_Device_Reboot(DEV_HANDLE DevHdl);

/**
 * @brief Disable device sleep.
 *
 * @param DevHdl Device handle.
 *
 * @param disable
 *            true  -> disable sleep
 *            false -> enable sleep
 */
int Host_Device_Disable_Sleep(DEV_HANDLE DevHdl, bool disable);

/**
 * @brief Let a device enter burn mode.
 *
 * @param DevHdl Device handle.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_BurnMode_Enter(DEV_HANDLE DevHdl);

/**
 * @brief Let a device exit burn mode.
 *
 * @note This function will reboor the device.
 *
 * @param DevHdl Device handle.
 */
void Host_BurnMode_Exit(DEV_HANDLE DevHdl);

/**
 * @brief Burn image for a device.
 *
 * @param DevHdl Device handle.
 *
 * @param path The path of image.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int  Host_Burn_Image(DEV_HANDLE DevHdl, void *path);

/**
 * @brief Get HIF handle in a device.
 *
 * @param DevHdl Device handle.
 *
 * @retval HIF handle.
 */
HIF_HANDLE Host_HIF_Handle_Get(DEV_HANDLE DevHdl);

/**
 * @brief Send command and recv responce for a device.
 *
 * @param DevHdl Device handle.
 *
 * @param msg_id The msg id in the command.
 *
 * @param param The payload in the command.
 *
 * @param param_len The length of param.
 *
 * @param resp_buff Used to recv reponse's payload.
 *
 * @param buffer_len The length of resp_buff.
 *
 * @param data_len The response's payload length, if buffer_len < payload_len,
 *                 the data_len will be set to payload_len, but just copy buffer_len into resp_buff.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int Host_General_Command(DEV_HANDLE DevHdl, uint32_t msg_id, void *param,
					uint32_t param_len, void *resp_buff, uint32_t buffer_len, uint32_t *data_len);

/**
 * @brief Get the version of device image.
 *
 * @param DevHdl Device handle.
 *
 * @param user_version 1 -> get custom version, 0 -> get sdk version.
 */
int HostCmd_Version_Get(DEV_HANDLE DevHdl, uint32_t user_version);

/**
 * @brief Reboot a device by HIF command.
 *
 * @param DevHdl Device handle.
 *
 * @param reboot 1 -> reboot, 2 -> enter burn mode.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int HostCmd_Device_Reboot(DEV_HANDLE DevHdl, uint32_t reboot);


//int         HostCmd_***_Set(DEV_HANDLE DevHdl, ***);
//int         HostCmd_***_Get(DEV_HANDLE DevHdl, ***);

#ifdef __cplusplus
}
#endif

#endif /* _HOST_DRIVER_H_ */

