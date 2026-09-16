ParamCfgPageBtn()  -- 切换到参数配置页面

StopRangeSpeedDetectBtn()

DisplayRadarWaveformConfigWarning(false)

SetReportDataType("PointCloud")

SetCulttersElimination("disable")
SetDopplerFFT("Checked")

-- GetDefaultConfiguration()

local mimoMode = "2T4R"
local startFrequency = "58"
local maxDetectRange = "10.24"
local rangeResolution = "4"
local maxDetectVelocity = "0.96"
local velocityResolution = "6"

local frameStructure = "2DFFT"
local acc = "1"

local framePeriod = "50"
local frameNum = "0"

-- 参数配置 - 雷达参数配置
SetMultiAntennaMode(mimoMode) -- 多天线模式
SetStartFrequency(startFrequency) -- 起始频率
SetMaximumMeasureDistance(maxDetectRange) -- 最大探测距离
SetRangeResolution(rangeResolution) -- 距离分辨率
SetMaximumMeasureVelocity(maxDetectVelocity) -- 最大探测速度
SetVelocityResolution(velocityResolution) -- 速度分辨率

-- 参数配置 - 帧结构配置
SetFrameStructure(frameStructure) -- 帧结构
SetCumulativeNum(acc) -- Interval Chirp N
SetFrameNum(frameNum) -- Frame 个数
SetFramePeriod(framePeriod) -- Frame 周期

-- Is2DPointCloudCoordAreaFollow3D(true)

-- local sRadarMountType = GetCurrentRadarMountType() -- 获取雷达安装方式

-- if sRadarMountType == "顶装" or sRadarMountType == "TopMount"
-- then
    SetPointCloudCoordinateType("Cartesian")
-- else -- 侧装
--     SetPointCloudCoordinateType("Polar")
-- end

-- 参数配置 - 点云上报数据配置 - 移动点云配置
SetMovingPointCloudSNR("17") -- SNR
SetCFARAlgorithm("SNR Threshold"); -- CFAR 算法
SetMovingPointCloudStaticObjectRemoveState(true) -- 静态目标移除

SetPresencePointCloudFrameExtractionRate("4")

DisplayRadarWaveformConfigWarning(true)

Sleep(1500)

StartRangeSpeedDetectBtn()  -- 开始测量

Sleep(500)

ImageDisplayPageBtn()  --切换到Demo演示页面

Display3DMovingPointCloud(true)
Display2DMovingPointCloud("XY", true)
Display2DMovingPointCloud("YZ", true)
Display2DMovingPointCloud("XZ", true)

Display3DMicroMotionPointCloud(true)
Display2DMicroMotionPointCloud("XY", true)
Display2DMicroMotionPointCloud("YZ", true)
Display2DMicroMotionPointCloud("XZ", true)

-- if sRadarMountType == "顶装" or sRadarMountType == "TopMount"
-- then
--     Set3DPointCloudCoordinateParam("X", -4, 4)
--     Set3DPointCloudCoordinateParam("Y", -4, 4)
--     Set3DPointCloudCoordinateParam("Z", -3, 0)
-- else -- 侧装
--     Set3DPointCloudCoordinateParam("Z", -2, 1)
-- 
--     Reset3DPointCloudCoordinateParam("X")
--     Reset3DPointCloudCoordinateParam("Y")
-- end
-- 
-- Apply3DPointCloudCoordinateParam("X")
-- Apply3DPointCloudCoordinateParam("Y")
-- Apply3DPointCloudCoordinateParam("Z")
-- 
-- Is2DPointCloudCoordAreaFollow3D(true)

prints("\n=================================================\n")
prints("Pose PointCloud Demo Startup Completed.\n")
prints("=================================================\n\n")