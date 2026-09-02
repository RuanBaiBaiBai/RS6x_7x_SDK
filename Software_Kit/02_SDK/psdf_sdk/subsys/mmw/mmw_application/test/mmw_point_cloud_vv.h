#ifndef MMW_POINT_CLOUD_VV_TOOLS_H
#define MMW_POINT_CLOUD_VV_TOOLS_H

#include <stdint.h>
#include "mmw_ctrl.h"
#include "mmw_type.h"

void mmw_point_cloud_run_vv(const CFAR_Result* ptr_cfar_result_sw, const CFAR_Result* ptr_cfar_result_hw, const uint32_t *ptr_cfar_num);

void mmw_point_cloud_vv_upload_mcu_result(PointCloudBuffer_t *ptr_point_cloud_buffer);

#endif