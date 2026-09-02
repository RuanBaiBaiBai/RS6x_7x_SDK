/**
 **************************************************************************************************
 * @file    hif_type.h
 * @brief   Definition of HIF protocol data type.
 * @attention
 *          Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.*/
#ifndef __HIF_TYPE_H__
#define __HIF_TYPE_H__

/* Includes */
#include "../utils/host_std_includes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types*/
typedef struct {
    /*! @brief  0=reserve, 1=to device, 2=to host, 3=local msg
        this type is useful for HIF message bridging. */
    uint32_t    type    : 2;

    /*! @brief  bit0: 0=no ACK, 1=need ACK.(1-request，0-ack or indication)
     *          bit1: 0=no encrypt, 1=encrypt.
     *          bit2: 0=no check code, 1=with check code.
     *          bit3: 0=no more, 1=more data.
     *          bit4: 0=no extent header, 1=with extent header.
     *          bit5: 0=checksum, 1=mac32.
     */
    uint32_t    flag    : 6;

    /*! @brief  Message ID */
    uint32_t    msg_id  : 8;

    /*! @brief  Payload length, unit=byte */
    uint32_t    length  : 12;

    /*! @brief  Sequence number and Fragment flag*/
    uint32_t    seq     : 3;
    uint32_t    frag    : 1;
} hif_msg_hdr_t;

#define HIF_MSG_TYPE_MASK                       0x3 /**< Message type mask */
#define HIF_MSG_TYPE_RESERVED                   0   /**< Reserved type */
#define HIF_MSG_TYPE_TO_DEVICE                  1   /**< Message to Device */
#define HIF_MSG_TYPE_TO_HOST                    2   /**< Message to Host */
#define HIF_MSG_TYPE_LOCAL                      3   /**< Message of local data */

#define HIF_MSG_FLAG_MASK                       0x3F     /**< Message flag mask */
#define HIF_MSG_FLAG_REQ_BIT                    (1U<<0)  /**< Req need a ACK to the msg      */
#define HIF_MSG_FLAG_ENCRYPT_BIT                (1U<<1)  /**< Message is encrypted           */
#define HIF_MSG_FLAG_CHECK_BIT                  (1U<<2)  /**< Message with check code        */
#define HIF_MSG_FLAG_MORE_DATA_BIT              (1U<<3)  /**< More data to transmit          */
#define HIF_MSG_FLAG_EXTEND_BIT                 (1U<<4)  /**< Message with a extend header   */
#define HIF_MSG_FLAG_MAC32_BIT                  (1U<<5)  /**< 1: check code is mac32; 0: checksum */

#define HIF_MSG_LENGTH_MASK                     0xFFF   /**< Message flag mask */
#define HIF_MSG_SEQ_MASK                        0x7     /**< mask of sequence number */
#define HIF_MSG_FRAG_MASK                       0x1     /**< mask of fragment flag */
#define HIF_MSG_FRAG_INVALID                    0       /**< 0: the message is finished; 1: no more fragment. */


/*! @brief    For future use */
typedef struct {
    uint32_t    reserved;
} hif_msg_ext_hdr_t;

typedef struct {
    uint8_t magic;      /**< magic for determine the UART header. */
    uint8_t checksum;   /**< checksum for MMW_MSG_HEADER */
} hif_phy_hdr_t;

#define HIF_PROTO_HEAD_LEN         (2 + 4) //(sizeof(hif_phy_hdr_t) + sizeof(hif_msg_hdr_t))
#define HIF_PROTO_TAIL_LEN         (4) //check32
#define HIF_PROTO_HEAD_TAL_LEN     (HIF_PROTO_HEAD_LEN + HIF_PROTO_TAIL_LEN)

/* Data format defined in payload */
typedef struct {
    uint8_t    status;
    uint8_t    payload[0];
} hif_msg_ack_t;

typedef struct {
	uint32_t total_len;
	uint32_t offset     : 24;
	uint32_t flow_seq   : 6;
	uint32_t reserved   : 2;
} fragment_hdr_t;

typedef struct {
	uint32_t blank_len;
	uint32_t blank_off  : 24;
	uint32_t flow_seq   : 6;
	uint32_t reserved   : 2;
} fragment_ack_t;

/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/*! @brief  General Ack Data */
#define HIF_CMD_STATUS_SUCCESS                  0   /**< Cmd Success */
#define HIF_CMD_STATUS_UNSUPPORT                1   /**< Unsupport Cmd */
#define HIF_CMD_STATUS_VERSION                  2   /**< Version not match */
#define HIF_CMD_STATUS_TOOLONG                  3   /**< Message too long */
#define HIF_CMD_STATUS_CHECK                    4   /**< Head or Data check failed */
#define HIF_CMD_STATUS_PARAM                    5   /**< Invalid Param */
#define HIF_CMD_STATUS_REPLAY                   6   /**< Replay message */
#define HIF_CMD_STATUS_AUTH                     7   /**< Auth failed */
#define HIF_CMD_STATUS_LATE                     8   /**< Result need report later */
#define HIF_CMD_STATUS_BUSY                     9   /**< Device is busy now */
#define HIF_CMD_STATUS_STATE                    10  /**< Invalid Device State to process Cmd */
#define HIF_CMD_STATUS_SYSERR                   11  /**< System error in Cmd Handler */
#define HIF_CMD_STATUS_IO                       12  /**< Genenal error */


#define HIF_PHY_HEAD_LEN                        (sizeof(hif_phy_hdr_t))
#define HIF_MSG_HEAD_LEN                        (sizeof(hif_msg_hdr_t))
#define HIF_EXT_HEAD_LEN                        (sizeof(hif_msg_ext_hdr_t))
#define HIF_HEAD_LEN                            (HIF_PHY_HEAD_LEN + HIF_MSG_HEAD_LEN)
#define HIF_CHKEC_LEN                           (4)
#define HIF_TAIL_LEN                            (HIF_CHKEC_LEN + 4)  /* 4 for ack status + align */
#define HIF_HEAD_SPACE                          (HIF_HEAD_LEN + HIF_EXT_HEAD_LEN)
#define HIF_MAX_SPACE                           (HIF_HEAD_SPACE + HIF_TAIL_LEN)

#define HIF_PHY_SYNC_MAGIC                      0x7E
#define HIF_PHY_WKP_MAGIC                       0x55
#define HIF_PHY_ACK_MAGIC                       0x79
#define HIF_PHY_MSG_MAGIC                       0xA5

#define HIF_PROTO_VERSION                       0x03


/**
 * @defgroup HIF_Message_ID
 * @brief    Message Identification for communication with Host or remote Client.
 *           0x00~0xFE
 * @{
 */
/* 0x00 ~ 0x0F Common CMDs */
#define HIF_MSG_ID_VERSION                      0x00  /**< HIF and Software Version, such as ROM, Firmware version */
#define HIF_MSG_ID_CHIPINFO                     0x01  /**< Device/Chip information, such as ProductID, ChipID, SN */
#define HIF_MSG_ID_HIF_PHY                      0x02  /**< Physical information of HIF, such as UART/TWI/SPI/Wireless */
#define HIF_MSG_ID_RESERVED0                    0x03  /**< Reserved */

#define HIF_MSG_ID_REBOOT                       0x04  /**< Reboot ctrl of device, HW/SW reboot, upgrade, shutdown */
#define HIF_MSG_ID_PWR_CTRL                     0x05  /**< Power Ctrl of device, such as sleep, standby, hibernaiton */
#define HIF_MSG_ID_HW_CFG                       0x06  /**< Hardware config of device, such as clock, voltage, pin */
#define HIF_MSG_ID_START_CTRL                   0x07  /**< Device Work Start/Stop */
#define HIF_MSG_ID_RESERVED1                    0x08  /**< Reserved */

#define HIF_MSG_ID_HIF_PHY_CFG                  0x09  /**< Config of Physical interface, such as UART/TWI/SPI/Wireless */
#define HIF_MSG_ID_HIF_MODE_CFG                 0x0A  /**< Config of HIF mode. */
#define HIF_MSG_ID_HOST_STATE                   0x0B  /**< Inform the device of host's state, such as Sleep/Active. */
#define HIF_MSG_ID_HOST_POLL_ACK                0x0C  /**< Inform the device of host ready to recieve to device data */
#define HIF_MSG_ID_RESERVED2                    0x0D  /**< Reserved */

#define HIF_MSG_ID_DOWNLOAD_CTRL                0x0E  /**< Firmware download control */
#define HIF_MSG_ID_DOWNLOAD_DATA                0x0F  /**< Firmware download data packets */

/* 0x10 ~ 0x1D Reserved for storage CMDs, such as EEPROM/Flash/Efuse etc. */
#define HIF_MSG_ID_STORAGE_INFO                 0x10  /**< Get info of storage supported */
#define HIF_MSG_ID_STORAGE_CTRL                 0x11  /**< Control of storage, such as mount or unmount */
#define HIF_MSG_ID_EEPROM_WRITE                 0x12  /**< Write operation of EEPROM */
#define HIF_MSG_ID_EEPROM_READ                  0x13  /**< Read operation of EEPROM */
#define HIF_MSG_ID_FLASH_ID                     0x14  /**< Get ID of Flash */
#define HIF_MSG_ID_FLASH_ERASE                  0x15  /**< Erase operation of Flash */
#define HIF_MSG_ID_FLASH_WRITE                  0x16  /**< Write operation of Flash */
#define HIF_MSG_ID_FLASH_READ                   0x17  /**< Read operation of Flash */
#define HIF_MSG_ID_EFUSE_CFG                    0x18  /**< Config Parameter to Efuse */
#define HIF_MSG_ID_EFUSE_GET                    0x19  /**< Get Parameter from Efuse */

/* 0x1A ~ 0x1D OTA */
#define HIF_MSG_ID_OTA_GET_INF                  0x1A  /**< OTA Get Image Information */
#define HIF_MSG_ID_OTA_WR_IMG                   0x1B  /**< OTA Write Image */
#define HIF_MSG_ID_OTA_REQ_UPG                  0x1C  /**< OTA Request upgrade */

/* 0x1E ~ 0x1F for Debug purpose */
#define HIF_MSG_ID_DEBUG_CFG                    0x1E  /**< Debug config to device */
#define HIF_MSG_ID_DEBUG_GET                    0x1F  /**< Get debug data from device */

/* 0x20 ~ 0x2F undefined
 * These CMDs can be used for system special purpose on different devices */

/* 0x30 ~ 0x3F Reserved for factory test purpose */
#define HIF_MSG_ID_FACTORY_MODE                 0x30  /**< factory mode enable/disable, ack return test version */
#define HIF_MSG_ID_VOLTAGE_TEST                 0x31  /**< system voltage test */
#define HIF_MSG_ID_IO_TEST                      0x32  /**< system I/O test */
#define HIF_MSG_ID_MMW_PERF_TEST                0x33  /**< MMW performance test */
#define HIF_MSG_ID_MMW_CALIB_TEST               0x34  /**< MMW calibration test */
#define HIF_MSG_ID_MMW_FUN_TEST                 0x35  /**< MMW function test */
#define HIF_MSG_ID_PWR_CONSUME_TEST             0x36  /**< Power consume in different scenes */
#define HIF_MSG_ID_PARAM_SAVE                   0x37  /**< Parameter Save in factory test */

/* 0x38 ~ 0x3F unused */

/* 0x40 ~ 0x4F Reserved for Radar Hardware Config */
#define HIF_MSG_ID_READ_CFG                     0x42  /**< Get MMW Radar Hardware Config */

/* 0x50 ~ 0x5F Reserved for Radar Detection Parameters Config */

/* 0x60 ~ 0x6F Reserved for Application Config */


/* 0x70 ~ 0x7F Reserved for MPW Config */
/* 0x80 ~ 0x8F undefined */
/* 0x90 ~ 0x9F undefined */


/**************************** Report Message Definintion *********************/
#define HIF_MSG_ID_REPORT_START                 0xA0
/* 0xA0 ~ 0xAF undefined */
/* 0xB0 ~ 0xBF undefined */
/* 0xC0 ~ 0xCF Reserved for Radar Report Data */
#define HIF_MSG_ID_ADC_DATA                     0xC0
#define HIF_MSG_ID_CUBE_DATA                    0xC1
#define HIF_MSG_ID_FFT_DATA                     0xC2
#define HIF_MSG_ID_OBJECTS                      0xC3  /**< pointcloud or objects */
#define HIF_MSG_ID_RESERVED4                    0xC4
#define HIF_MSG_ID_RESERVED5                    0xC5
#define HIF_MSG_ID_MOTION_DATA                  0xC6  /**< approach data report   */
#define HIF_MSG_ID_MOTION_SENSOR_PLOT_DATA      0xC7
#define HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA0   0xC8  /**< MotionSensor target data for low power application */
#define HIF_MSG_ID_MRS2_RANGE_DATA              0xC9
#define HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA1   0xCA
#define HIF_MSG_ID_MOTION_SENSOR_LP_TAR_DATA2   0xCB
#define HIF_MSG_ID_HEARTBEAT_DATA               0xFE

/* 0xC8 ~ 0xCF unused */

/* 0xD0 ~ 0xDF undefined */
/* 0xE0 ~ 0xEF undefined */

/* 0xF0 ~ 0xFE Reserved for system/debug report */
#define HIF_MSG_ID_DBG_PRINT                    0xF0  /**< Debug Strings */
#define HIF_MSG_ID_DBG_CP_INFO                  0xF1  /**< Compress format of debug information */
#define HIF_MSG_ID_PERF_INFO                    0xF2  /**< Performance report of modules */
#define HIF_MSG_ID_DBG_PWR_CONSUME              0xF3  /**< Power Consume report of modules */
#define HIF_MSG_ID_SYSTEM_INFO                  0xF4  /**< System state periodic report */
/* 0xF5 ~ 0xFE undefined */

#define HIF_MSG_ID_STARTUP                      0xFF  /**< Device StartUp Message ID */
/** @}*/ /* MMW_Message_ID */

/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */


#ifdef __cplusplus
}
#endif


#endif /* __HIF_TYPE_H__ */

