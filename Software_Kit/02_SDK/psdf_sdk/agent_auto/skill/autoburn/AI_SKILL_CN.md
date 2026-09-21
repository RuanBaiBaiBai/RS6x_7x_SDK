---
name: autoburn
description: "AgentAuto 中文 Agent 使用规则：环境发现、prepare/build、进入烧录模式、烧录/复位、串口监控、HIF、解析与安装流程。"
lang: zh-CN
---

# AgentAuto Agent 使用规则（中文）

> 语言：中文
> 语种标记：`lang: zh-CN`

## 目标

`AgentAuto` 是公开命令名，`autoburn` 是 Skill 包名。本规则要求 Agent 不要硬编码 SDK 路径、工程路径、COM 口或波特率，而是在修改工程或操作硬件之前先发现并确认。

最简单流程由 `README_CN.md` 开头定义：把 `agent_auto` 放入 SDK 的工程目录，由 Agent 读取中文 README 和本规则，在用户确认参数后自动完成编译、烧录、复位和监控。

## 执行前必须确认参数

开始 `prepare`、`build`、`burn` 或 `workflow` 前，需要和用户二次确认：

- 工程文件夹路径：例如 `D:\psic_sdk3\project\hello_word`。
- CDK 工程目录：`project.cdkproj` 所在文件夹，例如 `...\cdk`、`...\cdk_6130_1812_cpus`、`...\cdk_6240_cpuf`。
- 烧录/控制台/Shell 监控使用的本机可识别 `COMx` 端口，以及实际使用的控制台串口。
- HIF 流程使用的 PC SPI 设备端口/接口；HIF 进 upgrade、HIF 采集和 HIF 转存默认不使用 `COMx`。
- 波特率：烧录默认 `921600`，Shell/LOG 监控默认 `115200`，HIF 以工程 PHY 配置为准。
- 首次下载是否需要用户手动进入烧录模式。
- 是否在烧录后自动复位并启动固件。
- 是否执行监控，以及监控端口、时长、`--hex` 和输出文件。

用户不知道具体值时，先执行 `AgentAuto discover`、`doctor` 和 `ports`，报告真实结果后再向用户确认。缺少或存在歧义的参数只列出缺失项，不要猜测。

## 自动演示顺序

README 批准的自动流程按以下顺序执行：

1. 读取 `README_CN.md` 和本 `AI_SKILL_CN.md`。
2. 发现并确认 SDK 根目录、工程文件夹路径、CDK 工程目录和 COM 口。
3. 执行 `prepare`/`build`，确认 `BUILD_OK=1`。
4. 当前固件支持自动进入烧录模式时，使用 `upgrade --mode shell`、`upgrade --mode hif` 或 `workflow --enter-mode shell/hif`。
5. 第一次下载固件时，如果无法自动进入或自动进入失败，停下来请用户手动进入烧录模式，然后继续。
6. 烧录并复位/启动固件。
7. 监控控制台或 LOG 串口，确认固件正常运行。

README 开头的自动演示请求视为已预先批准；除非缺少必要参数或要打开监控串口，不要对每个步骤重复询问。

## 环境发现

```powershell
AgentAuto discover --json
AgentAuto doctor
AgentAuto ports
```

检查项：

- `SDK_ROOT` 必须同时包含 `project/` 和 `platform/`。
- `CDK_MAKE` 必须指向 `cdk-make.exe`，不能使用 MINGW 的 `make`、`mingw32-make` 或 `gcc`。
- `DOWNLOAD_DLL` 必须指向存在的 `DownloadLib.dll`。
- `PROJECT_PATHS` 必须包含用户要编译的工程文件夹。
- `PORTS` 必须包含用户确认的 COM 口。

`discover --json` 会输出 `project_paths`，优先使用完整工程路径。

## 烧录文件复制策略

默认关闭文件复制。`AgentAuto burn` 默认直接使用 SDK 内的
`DownloadLib.dll` 和固件原始路径烧录；正常烧录时不要复制文件，也不要替用户
选择复制路径。

只有用户明确允许复制，并确认本地复制路径后，才可以使用 `--copy-dir` 执行：

```powershell
AgentAuto burn --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600 --copy-dir D:\path\to\local\staging
```

如果检测到 `DownloadLib.dll` 或固件镜像位于 UNC/网络路径，或用户电脑出现
DownloadLib 无法从网络位置加载的报错，必须在烧录前停止，先询问用户是否允许
复制以及复制到哪个本地路径，再使用 `--copy-dir` 烧录。不得自动复制，也不得
自行指定复制路径。

## 常用命令

```powershell
AgentAuto prepare --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto build --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto workflow --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600
AgentAuto burn --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600
AgentAuto monitor --port %COMPORT% --baud 115200 --duration 10
```

参数说明：

- `--project`：工程目录名或工程文件夹绝对路径。
- `--config`：`project.cdkproj` 所在目录，或 CDK 编译配置名。
- `--port`：DownloadLib 烧录串口。
- `--console-port`：与烧录串口不同时的控制台串口。
- `--baud`：DownloadLib 波特率，默认 `921600`。
- `--shell-baud`：Shell/HIF 自动进入烧录模式使用的串口波特率。
- `--enter-mode shell`：通过 shell `upgrade` 自动进入烧录模式。
- `--enter-mode hif`：通过 HIF 唤醒和 reboot 自动进入烧录模式。
- `--dry-run`：只校验流程，不修改工程文件，也不打开串口。

工程下只有一个 CDK 目录时，`--config` 可以省略；有多个 CDK 目录时，先按用户描述的板级匹配目录名，无法唯一匹配时再请用户确认 `--config`。

## COM 与 SPI 通道职责

所有流程先明确通道归属；换机器、换开发板或换工具目录都不能改变这条规则：

1. 使用本机可识别到的 `COMx`：
   - 烧录固件（仅 UART 模式）。
   - 使用 Shell 进入 upgrade 烧录模式。
   - 使用规定的雷达复位流程：`0x7E` 同步 -> 写 `0x40009070=0x00000000` -> `0x13` BROM reset。
   - 串口监控（Shell/LOG/控制台）。
2. 使用 PC 的 SPI 设备端口/接口（例如 CH347T SPI、`HifMsgDataCollectionLib` SPI API）：
   - 使用 HIF 模式进入 upgrade 烧录模式。
   - 获取 HIF 设备数据。
   - 将采集到的 HIF 数据转存为文档/表格文件。
   不得把这些 HIF 步骤错误使用为 `COMx` 串口。
3. 若工程为 HIF SPI 模式，HIF 进烧录/采集统一走 SPI：
   - HIF wake 示例帧：`55 FF 55 FF`
   - HIF upgrade 示例帧：`A5 30 15 04 01 10 02 E8 FB FE EF`
   - HIF 采集调用 SPI DLL/API，例如 `HifMsgDataCollectionApi.OpenSpiDevice` / `StartCollectingData`。
4. 换环境后必须重新发现：
   - `COMx` 列表用于烧录/Shell 进 upgrade/`0x13` 复位/串口监控；
   - SPI 设备列表用于 HIF 进 upgrade/HIF 采集/HIF 转存；
   不允许因为换环境就把两类通道混用。

## 下载模式 BROM 指令

板子已进入下载模式后，如果需要单独发送 BROM 指令并读取返回值，使用
`brom-send`。`--dry-run` 不打开串口，只验证并打印待发送帧。

写 SRAM，指令 ID 为 `0x20`：

```powershell
AgentAuto brom-send --port %COMPORT% --baud 921600 --cmd 0x20 --mode sram-write --addr 0x20000000 --len 4 --data "12 34 56 78"
```

雷达复位必须按下述顺序执行，缺少任何一步都不得判定复位成功：

1. 先发送 `0x7E` 与开发板 BROM 同步，必须收到同步回复；成功标记：`BROM_SYNC_OK=1`（DownloadLib 路径同时有 `CONNECT_CODE=89`）。
2. 使用写 SRAM 指令 `0x20`，地址固定 `0x40009070`，数据固定 `0x00000000`（4 字节），必须先收到 header ACK，再收到 data ACK；成功标记：`BROM_RESET_SRAM_ADDR=0x40009070`、`BROM_RESET_SRAM_DATA=00 00 00 00`、`BROM_SRAM_WRITE_OK=1`。
3. 随后才发送指令 ID `0x13` 的 BROM reset，必须收到匹配 `50 53 49 43 02 03 64 66 00 00 00 00` 的 ACK；成功标记：`BROM_RESET_ACK_MATCH=1`、`BROM_RESET_OK=1`。

一体化的复位命令如下；`brom-reset` 会在脚本内部按 1 -> 2 -> 3 顺序执行，并强制 `--cmd 0x13`：

```powershell
AgentAuto brom-send --port %COMPORT% --baud 921600 --cmd 0x13 --mode brom-reset
```

不发 `0x7E` 同步、不写 `0x40009070=0x00000000`、只发 `0x13` 的流程已废弃，禁止作为雷达复位成功依据。复位帧
`50 53 49 43 01 02 4D 66 05 00 00 00 13 01 00 00 00`，默认校验
`50 53 49 43 02 03 64 66 00 00 00 00`；可通过 `--expected-ack` 覆盖最终 `0x13` 的期望回复。

`brom-send` 通过 `DownloadLib.BromSendData` 发送，并接管下载线程的 ACK
回调来返回 `BROM_RX_HEX`；有数据段时同时返回 `BROM_RX_PAYLOAD_HEX`。执行真实
发送前必须确认板子已进入下载模式，并确认指令地址、长度和数据不会破坏正在调试
的目标。

## 手动步骤

环境发现通过后：

```powershell
AgentAuto prepare --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto build --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto burn --project %PROJECT_PATH% --config %CDK_DIR% --port %COMPORT% --baud 921600
```

烧录默认开启回读校验。只有用户明确选择关闭时才传 `--no-read-back-check`；开启且成功时输出
`READ_BACK_CHECK=enabled` 和 `READ_BACK_CHECK_OK=1`，比对失败会作为 DownloadLib 回读错误返回，不能当作烧录成功。

首次烧录建议先加入 `--dry-run`。烧录完成后如果出现 `AUTO_RESET_OK=1`，说明固件已复位/启动；需要确认运行状态时再执行监控：

```powershell
AgentAuto monitor --port %COMPORT% --baud 115200 --duration 10
```

监控前必须再次确认串口、波特率、时长和输出参数。不要在用户未确认时自动打开串口。

## 宏规则

`CONFIG_SHELL` 与 `CONFIG_SYS_SHELL` 必须绑定开启：`prepare` 会一起补齐或保持，绝不会只开启其中一个。

- 本规则优先于 README 自动演示流程中关于 `prepare` 自动开启 Shell/HIF 宏的描述。
- 默认禁止为集成 `Shell` 或 `HIF` 功能而编辑工程宏文件；未得到用户明确要求时，不写入 `CONFIG_SHELL`、`CONFIG_SYS_SHELL`、`CONFIG_HIF`、HIF PHY 等宏，不添加对应源码、include 路径或链接配置。
- 工程中的“额外新增功能”包括但不限于：`CONFIG_SHELL_*`/Shell 命令、`CONFIG_SYS_SHELL`、`CONFIG_HIF_*`/HIF PHY、MMW/HIF 源码模块、CDK include/linker 配置等。
- 只有用户明确告知“需要在工程中添加 Shell/HIF 功能”时，才允许对 `prj_config.h`、`.cdkproj` 或相关工程文件进行这类新增。
- 选择 `upgrade --mode shell` 或 `upgrade --mode hif` 只是选择进入烧录模式的方式，不代表用户授权给工程新增功能宏。
- 若当前工程/固件没有对应功能，停下来询问用户，不得用 `prepare` 擅自补齐 Shell/HIF 集成。
- 在交互式操作中，修改 `prj_config.h` 前先询问用户一次。
- 如果用户选择不开启 shell，继续 `prepare`/`build`，但不开启 shell 宏对。
- HIF 自动进入还需要确认 `CONFIG_HIF` 和 HIF PHY 串口配置。

## 板级与 CDK 规则

- 工程用 `cdk` 目录存放 `project.cdkproj` 时，板级通过该 `project.cdkproj` 中的 `CONFIG_BOARD_*` 编译宏选择。合法宏名以 `platform/boards/board_config.h` 为准。
- 用户描述板级后，解析出唯一的 `CONFIG_BOARD_*` 宏，写入 `project.cdkproj` 的 `BuildConfig/Compiler` 和 `BuildConfig/Asm` 的 `Define`，不要写入 `prj_config.h`。AgentAuto 会把 `app/inc/prj_config.h` 中旧板级宏注释掉，避免重复定义；`CONFIG_SHELL`、`CONFIG_SYS_SHELL`、`CONFIG_HIF` 等功能宏仍写入 `prj_config.h`。
- 工程存在多个按板级区分的 `cdk*` 目录时，把用户板级匹配到唯一目录，并直接以该目录作为 `--config` 编译，不修改共享 `prj_config.h`。
- 无法唯一匹配宏或 CDK 目录、多个目录同板级、板级超出工程支持范围时，列出候选并请用户确认。

## 配置文件与优先级

`AgentAuto.conf` 的 `[default]` 保存本机默认值：

```ini
[default]
sdk = D:\psic_sdk3
cdk = C:\C-Sky\CDK\cdk-make.exe
dll = D:\psic_sdk3\agent_auto\assets\DownloadLib.dll
project = D:\psic_sdk3\project\hello_word
config = D:\psic_sdk3\project\hello_word\cdk
port = COM109
console-port = COM109
```

生效顺序：

1. 命令行参数。
2. `AgentAuto.conf`，按 `[default]`、基础模块段、子命令段合并。
3. 环境变量。
4. 当前目录、SDK 上级目录、标准 CDK 安装路径和随包资源自动发现。

常用环境变量：

| 变量 | 用途 |
| --- | --- |
| `AGENT_AUTO_SDK_ROOT` | SDK 根目录 |
| `AGENT_AUTO_CDK` | `cdk-make.exe` 绝对路径 |
| `CDK_MAKE` | `AGENT_AUTO_CDK` 的兼容别名 |
| `AGENT_AUTO_DLL` | `DownloadLib.dll` 绝对路径 |
| `AGENT_AUTO_PYTHON` | 启动脚本使用的 `python.exe` |
| `AGENT_AUTO_CONFIG` | 替代配置文件路径 |
| `AGENT_AUTO_SKILLS` | Skill 安装目标目录 |

## 路径解析

| 占位符 | 含义 |
| --- | --- |
| `%SDK_ROOT%` | SDK 根目录 |
| `%PROJECT_PATH%` | 工程文件夹绝对路径 |
| `%CDK_DIR%` | `project.cdkproj` 所在目录 |
| `%COMPORT%` | 烧录 COM 口 |
| `%CONSOLE_COMPORT%` | 控制台串口 |
| `%HIF_COMPORT%` | HIF 串口（仅用户明确要求 HIF UART 并确认使用 COMx 时） |
| `%IMAGE%` | 固件镜像路径 |

工程解析：

1. `--project` 为绝对路径时直接使用该文件夹。
2. 当前目录位于 `project/<name>` 时使用当前目录。
3. 只有一个工程时自动选择。
4. 多个工程时列出 `PROJECT_PATHS` 并请用户确认。

CDK 解析：

1. `--config` 指定的 `project.cdkproj` 所在目录。
2. 工程下 `cdk` 目录。
3. 工程下唯一的 `cdk*` 目录。
4. 存在多个 `cdk*` 目录时，先按用户描述的板级匹配目录名；无法唯一匹配时，请用户提供 `--config`。

固件镜像解析：

1. `--image`。
2. `%PROJECT_PATH%/%CDK_DIR%/output/*.img` 中最新的镜像。

## 编译规则

- `.cdkproj` 是编译工程主文件；`.cdkws` 只用于 CDK workspace。
- 编译时使用系统 CDK 工具链，例如 `cdk-make.exe /w project.cdkws /p project.cdkproj /c <config> /d build /v`。
- 显式通过 `/p` 指定当前 `.cdkproj`。
- 不要使用 MINGW 的 `gcc`、`make` 或 `mingw32-make`。

## 从 hello_word 新建工程

```powershell
Copy-Item "%SDK_ROOT%\project\hello_word" "%SDK_ROOT%\project\hello_word_mmw_hif_demo" -Recurse
```

更新 `cdk/project.cdkws` 和 `cdk/project.cdkproj` 中的工程标识，然后：

```powershell
AgentAuto prepare --project "%SDK_ROOT%\project\hello_word_mmw_hif_demo" --config "%SDK_ROOT%\project\hello_word_mmw_hif_demo\cdk"
AgentAuto build --project "%SDK_ROOT%\project\hello_word_mmw_hif_demo" --config "%SDK_ROOT%\project\hello_word_mmw_hif_demo\cdk"
```

## 源码刷新与排除

```powershell
AgentAuto refresh-sources --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto exclude --project %PROJECT_PATH% --config %CDK_DIR% --source app\src\bad_source.c
AgentAuto excluded --project %PROJECT_PATH% --config %CDK_DIR%
AgentAuto unexclude --project %PROJECT_PATH% --config %CDK_DIR% --source app\src\bad_source.c
```

排除列表保存在所选 CDK 目录的 `autoburn_exclude.txt`，被排除的文件仍保留在磁盘。

## 常见问题

| 问题 | 处理 |
| --- | --- |
| 新源码未编译 | 执行 `refresh-sources` 后重新编译 |
| MMW/HIF 符号未定义 | 执行 `prepare` 添加宏、源码和链接配置 |
| 找不到 CDK 或误用 MINGW | 执行 `doctor`，确认 `CDK_MAKE=` 指向 `cdk-make.exe` |
| 多个 CDK 目录 | 使用 `--config` 明确 `project.cdkproj` 所在目录 |
| 无法自动进入烧录模式 | 请用户手动进入烧录模式，再执行 `burn` |
| 烧录后无输出 | 确认监控串口和波特率，重新烧录匹配工程的镜像 |
| DownloadLib 网络位置/加载报错 | 先询问用户是否允许复制并确认本地路径，再用 `burn --copy-dir <path>` 执行 |

## 红线

- 不编造 SDK 路径、工程路径、COM 口、波特率、时长或日志标记。
- 未确认必要参数前不打开串口、不修改工程文件、不执行烧录。
- `CONFIG_SHELL` 与 `CONFIG_SYS_SHELL` 必须成对开启或关闭。
- 用户未明确要求新增 Shell/HIF 功能时，不得使用 `prepare` 或其他工具向工程写入 `CONFIG_SHELL`/`CONFIG_SYS_SHELL`/`CONFIG_HIF` 等宏或添加对应源码/链接配置。
- COM/SPI 通道职责固定：烧录、Shell 进 upgrade、`0x13` 复位、串口监控使用 `COMx`；HIF 进 upgrade、HIF 采集、HIF 转存使用 PC 的 SPI 设备端口，不得混用。
- 雷达复位流程固定为：先 `0x7E` 同步并收到回复，再 `0x20` 写 `0x40009070=0x00000000` 并收到 ACK，最后发送 `0x13` BROM reset 并收到匹配 ACK。
- 默认关闭烧录文件复制；不得自动复制，也不得在未得到用户确认时自行选择复制路径。
- 第一次下载固件时，如果开发板固件不能自动进入烧录模式，必须先请用户手动进入，再执行 `burn`。
- 只使用 CDK 工具链，把 `.cdkproj` 作为编译目标，不使用 MINGW。
- `prepare`、`build` 和 `refresh-sources` 会修改工程文件，仅在用户要求或流程需要时执行。
- 编译或烧录失败后，不继续复用旧 `.img`。
- 对外命令名保持 `AgentAuto`，不新增 `autoburn` 作为公开命令。
