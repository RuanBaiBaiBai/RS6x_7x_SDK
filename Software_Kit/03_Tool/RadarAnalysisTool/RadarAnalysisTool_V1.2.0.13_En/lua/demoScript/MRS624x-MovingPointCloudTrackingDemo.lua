ParamCfgPageBtn()  -- 切换到参数配置页面

StopRangeSpeedDetectBtn()

DisplayRadarWaveformConfigWarning(false)

SetReportDataType("PointCloud")

SetCulttersElimination("disable")
SetDopplerFFT("Checked")
-- GetDefaultConfiguration()

local mimoMode = "2T4R"
local startFrequency = "58"
local maxDetectRange = "20.48"
local rangeResolution = "8"
local maxDetectVelocity = "3.2"
local velocityResolution = "20"

local frameStructure = "2DFFT"
local acc = "1"

local framePeriod = "50"
local frameNum = "0"

SetMultiAntennaMode(mimoMode)
SetStartFrequency(startFrequency)
SetMaximumMeasureDistance(maxDetectRange)
SetRangeResolution(rangeResolution)
SetMaximumMeasureVelocity(maxDetectVelocity)
SetVelocityResolution(velocityResolution)

SetFrameStructure(frameStructure)
SetCumulativeNum(acc)

SetFrameNum(frameNum)
SetFramePeriod(framePeriod)

SetMovingPointCloudSNR("12")
SetCFARAlgorithm("SNR Threshold"); -- CFAR 算法
SetMovingPointCloudStaticObjectRemoveState(true)

SetPresencePointCloudFrameExtractionRate("4")

-- Is2DPointCloudCoordAreaFollow3D(true)

-- local sRadarMountType = GetCurrentRadarMountType() -- 获取雷达安装方式
-- if sRadarMountType == "顶装" or sRadarMountType == "TopMount"
-- then
SetPointCloudCoordinateType("Cartesian")
-- else -- 侧装
--     SetPointCloudCoordinateType("Polar")
-- end

DisplayRadarWaveformConfigWarning(true)

Sleep(1500)

StartRangeSpeedDetectBtn()  -- 开始测量

Sleep(500)

ImageDisplayPageBtn()  --切换到Demo演示页面

Display3DMicroMotionPointCloud(false)
Display2DMicroMotionPointCloud("XY", false)
Display2DMicroMotionPointCloud("YZ", false)
Display2DMicroMotionPointCloud("XZ", false)

Display3DMovingPointCloud(true)
Display2DMovingPointCloud("XY", true)
Display2DMovingPointCloud("YZ", true)
Display2DMovingPointCloud("XZ", true)

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
prints("Moving PointCloud Tracking Demo Startup Completed.\n")
prints("=================================================\n\n")