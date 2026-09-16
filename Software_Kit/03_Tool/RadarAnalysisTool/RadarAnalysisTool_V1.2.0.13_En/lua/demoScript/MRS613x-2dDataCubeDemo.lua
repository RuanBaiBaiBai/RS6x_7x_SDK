ParamCfgPageBtn()  -- 切换到参数配置页面

StopRangeSpeedDetectBtn()

DisplayRadarWaveformConfigWarning(false)

SetReportDataType("DataCube")

SetCulttersElimination("disable")
SetDopplerFFT("Checked")
-- GetDefaultConfiguration()

local mimoMode = "1T3R"
local startFrequency = "58"
local maxDetectRange = "20.48"
local rangeResolution = "8"
local maxDetectVelocity = "3.2"
local velocityResolution = "20"

local frameStructure = "2DFFT"
local acc = "1"

local framePeriod = 200
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

local minFramePeriod = GetMinFramePeriod()
if minFramePeriod <= 200
then
    framePeriod = 200
else
    framePeriod = minFramePeriod
end
SetFramePeriod(tostring(framePeriod))

DisplayRadarWaveformConfigWarning(true)

Sleep(1500)

StartRangeSpeedDetectBtn()  -- 开始测量

Sleep(500)

ImageDisplayPageBtn()  --切换到Demo演示页面

SetRangeSpecDisplayMode("staticRemoval")
Sleep(framePeriod + 1500)
SetStaticRemovalNum("8")

prints("\n=================================================\n")
prints("2D DataCube Demo Startup Completed.\n")
prints("=================================================\n\n")