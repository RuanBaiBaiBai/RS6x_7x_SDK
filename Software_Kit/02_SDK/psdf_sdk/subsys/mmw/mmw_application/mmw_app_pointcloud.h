#ifndef __MMW_APP_POINTCLOUD_H__
#define __MMW_APP_POINTCLOUD_H__

#include "mmw_alg_pointcloud_typedef.h"

#define readw(addr)             (*((volatile unsigned long *)(addr)))

/**
 * @brief Configure default frame and bb for point cloud process.
 *        This function should be called BEFORE @ref mmw_point_cloud_init()
 * */
extern int8_t mmw_point_cloud_frame_config(void);

/**
 * @brief Configure default baseband for point cloud process.
 *        This function should be called BEFORE @ref mmw_point_cloud_init()
 * @return MMWave error code (defined MMW_ERR_CODE_*),  Zero on success.
 * */
extern int mmw_point_cloud_bb_config(void);

/**
 * @brief This function allocate memories and BB configs before startup.
 *        This function should be called AFTER frame has been configed
 **/
extern int mmw_point_cloud_init(void);

/**
 * @brief Run point cloud process in every callback.
 * @return PointCloudBuffer_t* pointer allocated in mmw_point_cloud_process(), which contains point cloud results.
 *         This pointer should be free outside mmw_point_cloud_process() function after use.
 * */
extern PointCloudBuffer_t* mmw_point_cloud_process(void);

/**
 * @brief Free all memories allocated in point cloud process.
 * */
PointCloudBuffer_t* mmw_point_cloud_process_sw_cfar(void);
void mmw_point_cloud_deinit(void);

/**
  * @brief Transform radar coordinate system to user coordinate system
  *        User cooridnate is basedon dev-board, different dev-board has different transform method.
  *        This function only support MRS6130_P1812/MRS6130_P1806/MRS6240_P2510 mounted on official dev board
  *        Attention: coordinate transform should be done on cartisian coordinate system but polar coodinate system.
  * @param x: x in radar cartisian coodinate system
  * @param y: y in radar cartisian coodinate system
  * @param z: z in radar cartisian coodinate system
  * @param ptr_cartisian_coord: Transformed cartisian coodinate, x,y,z
  *
  */
void mmw_point_cloud_trans_radar_coord_to_user_coord(float x, float y, float z, float *ptr_cartisian_coord);

/* this function cloud trans float type varible to db(q4),formula:out = 10 * log10(lin_data) * 2^4 */
extern int16_t fast_db_trans_f32(float lin_data);

/**
 * @brief This function implements cfar detection process. firstly, read hardware cfar results, and when the buffer of hardware
 * 		  cfar is full, execute software cfar post process.
 * @param ptr_hw_cfar_buff: pointer to hw cfar result
 * @param ptr_sw_cfar_buff: pointer to sw cfar result
 * @param ptr_cfar_path_buff: the starting addresses of hw and sw cfar reult
 * @param ptr_cfar_num_buff: hw and sw cfar num
 * @param max_cfar_num: the maximum number of cfar points for calculation
 * @return total_cfar_num: the sum number of cfar points
 */
uint32_t mmw_cfar_detection_process(CFAR_Result** ptr_hw_cfar_buff, CFAR_Result** ptr_sw_cfar_buff, CFAR_Result** ptr_cfar_path_buff, uint32_t* ptr_cfar_num_buff, uint16_t max_cfar_num);

#endif/*__MMW_APP_POINTCLOUD_H__*/
