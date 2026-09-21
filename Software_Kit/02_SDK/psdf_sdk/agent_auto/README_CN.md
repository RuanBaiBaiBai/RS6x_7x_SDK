# AgentAuto 用户手册（中文）

> 语言：中文
> 语种标记：`lang: zh-CN`

## 介绍

AgentAuto 是面向 Possumic SDK 开发板的统一自动化工具包，用于工程编译、固件发布、串口调试和雷达 HIF 数据分析。工具包通过统一入口组织不同功能模块，并提供配套 Python 工具和 Agent Skill，不需要依赖固定的 SDK 路径、工程路径、串口或波特率假设。

### 支持功能

- 环境辅助：自动发现 SDK、工程、CDK、DownloadLib 和可用串口，并对环境进行检查与诊断。

- 工程准备：按板级启用或维护 Shell/HIF 相关编译配置，并刷新 CDK 工程源码列表。

- 工程编译：调用系统 CDK 工具链编译指定工程，可按需排除或恢复暂不参与编译的源码。

- 发布流程：支持分步操作或一键完成“准备、编译、进入设备下载/升级模式、烧录、复位/启动、监控”的完整流程。

- 固件烧录：通过 DownloadLib 写入固件，可按需开启回读校验、烧录后复位/启动，并处理首次下载需要手动进入下载模式的情况。

- 串口调试：支持配置、发送 Shell 文本或原始 HIF 字节，并监控控制台、LOG 或 HIF 串口输出。

- 下载模式调试：可在设备处于下载模式时发送 BROM 指令，并获取 ACK 或返回数据。

- HIF 数据下发：支持唤醒 HIF、发送原始字节、进入升级/下载模式，以及解析 HIF 十六进制日志、RX_HEX 日志和原始二进制捕获。

- HIF 数据解析：能够扫描 HIF 帧、校验帧头和校验和，重组分片 HIF 消息以及分片 DataCube 数据。

- DataCube数据处理：可从 UART/COM 或 CH347T SPI 会话中采集 HIF 数据，提取并导出 C1/C2 DataCube 报告为原始文件、IQ 文件、NumPy 数组及会话清单；也支持离线解析已有捕获。

- 点云数据处理：可将点云上报转换为运动/存在点云清单和 CSV 数据，用于进一步分析。

- 可视化与分析：支持 DataCube 距离谱、点云总览/分帧图、2D-FFT 实时热图，以及离线 2D-FFT/HIF 数据分析和演示。

- 数据表格导出：可将 HIF 采集会话导出为调试工具风格的一维帧信息、运动点云和存在点云表。

## 使用功能介绍

### Agent 使用方式

1. 将 `agent_auto` 放入当前 SDK 文件夹，并把 SDK 目录设置为 Agent 聊天工作空间。
2. 向 Agent 描述要编译或发布的工程、板级、烧录端口、控制台/HIF 端口及波特率；不清楚时，可先使用环境发现、环境检查和串口枚举辅助确认。
3. Agent 会先校验环境和参数，再按用户批准的方式完成准备、编译、进入下载模式、烧录、复位/启动和监控。

#### Agent 使用模板

```text
请使用 AgentAuto 处理工程 <工程路径>，板级为 <板级>，CDK 工程目录为 <CDK 目录>，
烧录串口为 <COM 口>。使用 <Shell 或 HIF> 方式进入烧录模式，并检查对应的编译宏配置。
如需修改 project.cdkproj、prj_config.h、源码或编译配置，请先说明改动内容和影响，
取得确认后再执行。确认后完成编译、进入烧录模式、烧录、复位/启动和监控，并报告日志与结果。
```

#### Agent 使用例程

```text
请使用 AgentAuto 处理工程 RS6x_7x_mmWave_sdk_V2.1.2\Software_Kit\02_SDK\
psdf_sdk\project\mmwave\mmwave_application\Posture，板级为 6240，
CDK 工程目录为 RS6x_7x_mmWave_sdk_V2.1.2\Software_Kit\02_SDK\psdf_sdk\
project\mmwave\mmwave_application\Posture\cdk_6240_cpuf，
烧录串口为 COM109。使用 HIF 方式进入烧录模式，并检查对应的编译宏配置。
如需修改 project.cdkproj、prj_config.h、源码或编译配置，请先说明改动内容和影响，
取得确认后再执行。确认后完成编译、进入烧录模式、烧录、复位/启动和监控，并报告日志与结果。
```

# 发布目录

```text
agent_auto/
  README_CN.md
  README_EN.md
  ReleaseNote_CN.md
  ReleaseNote_EN.md
  AgentAuto.conf
  AgentAuto.cmd
  agentcore.cmd
  install.cmd
  assets/
    DownloadLib.dll
  modules/
    core/cli.py
    workflow/module.py        一键发布流程（发现、准备、编译、烧录、复位、监控）
    upgrade/module.py         进入下载/升级模式（Shell 或 HIF）
    burn/module.py            固件烧录与回读校验
    hif/module.py             HIF 数据发送、升级进入、日志解析
    monitor/module.py         控制台/LOG/HIF 串口监控
    datarecord/               原始 UART/HIF 数据记录
    dataparse/                HIF、DataCube、点云解析
  runtime/
    agentauto_engine.py
  skill/autoburn/
    AI_SKILL_CN.md
    AI_SKILL_EN.md
  tools/
    agentauto.py              离线 2D-FFT/HIF 数据分析
    hif_2dfft_live.py         2D-FFT 实时热图
    hif_1dfft_live.py         1D-FFT 实时处理
    hif_pointcloud_sample.py  点云 HIF 样例处理
```
