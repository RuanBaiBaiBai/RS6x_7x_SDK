/**
 **************************************************************************************************
 * @file    hal_store.h
 * @brief   HAL store define.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef _HAL_STORE_H_
#define _HAL_STORE_H_

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "../host_port.h"


/* Exported defines.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
#if (CFG_HOST_PORT_STORE_EN == 1)
/**
 * @brief Open a image.
 *
 * @note Get a image by path, the function will return hdl and len
 *
 * @param hdl Image handle, image handle type is void*.
 *
 * @param path Image path, like "/tmp/Tracking_spi_60m_mrs6240_p2512".
 *
 * @param len Image length, typical unit: byte.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_store_open(void **hdl, void *path, uint32_t *len);

/**
 * @brief Close a image.
 *
 * @param hdl Image handle.
 */
void host_store_close(void *hdl);

/**
 * @brief Read image.
 *
 * @param hdl Image handle.
 *
 * @param offset The offset address within the image, such as 0 indicating the
 *               starting address of the image.
 *
 * @param pbuff The buffer to read image.
 *
 * @param size The buffer size.
 *
 * @retval HostDriver error code, HOST_ERRCODE_SUCCESS on success.
 */
int host_store_read(void *hdl, uint32_t offset, void *pbuff, uint32_t size);
#endif  /* CFG_HOST_PORT_STORE_EN == 1 */


#ifdef __cplusplus
}
#endif

#endif /* _HAL_STORE_H_ */

